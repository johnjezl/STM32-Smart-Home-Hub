/**
 * SmartHub Certificate Manager Implementation
 */

#include <smarthub/security/CertManager.hpp>
#include <smarthub/core/Logger.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <vector>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <functional>
#include <memory>

// OpenSSL headers for certificate parsing
#if __has_include(<openssl/x509.h>)
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/x509v3.h>
#define HAVE_OPENSSL 1
#else
#define HAVE_OPENSSL 0
#endif

namespace smarthub::security {

CertManager::CertManager(const std::string& certDir)
    : m_certDir(certDir)
{
}

#if __has_include(<openssl/x509.h>)

namespace {

struct FileCloser {
    void operator()(FILE* file) const {
        if (file) {
            fclose(file);
        }
    }
};
using FilePtr = std::unique_ptr<FILE, FileCloser>;
using PkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using X509Ptr = std::unique_ptr<X509, decltype(&X509_free)>;

// Writes to a temporary name and renames, so a power cut can't leave half a file
bool writePem(const std::string& path, int mode, const std::function<bool(FILE*)>& write) {
    const std::string temporary = path + ".new";
    const int fd = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd < 0) {
        return false;
    }
    FilePtr file(fdopen(fd, "w"));
    if (!file) {
        ::close(fd);
        return false;
    }
    bool written = write(file.get()) && fflush(file.get()) == 0 && fsync(fd) == 0;
    file.reset();
    written = written && ::rename(temporary.c_str(), path.c_str()) == 0;
    if (!written) {
        ::unlink(temporary.c_str());
    }
    return written;
}

}  // namespace

bool CertManager::isUsablePair(const std::string& certPath, const std::string& keyPath) {
    FilePtr certFile(fopen(certPath.c_str(), "r"));
    FilePtr keyFile(fopen(keyPath.c_str(), "r"));
    if (!certFile || !keyFile) {
        return false;
    }

    X509Ptr cert(PEM_read_X509(certFile.get(), nullptr, nullptr, nullptr), X509_free);
    PkeyPtr key(PEM_read_PrivateKey(keyFile.get(), nullptr, nullptr, nullptr), EVP_PKEY_free);
    return cert && key && X509_check_private_key(cert.get(), key.get()) == 1;
}

bool CertManager::createSelfSigned(const std::string& certPath, const std::string& keyPath,
                                   const std::string& hostname) {
    std::error_code error;
    std::filesystem::create_directories(std::filesystem::path(certPath).parent_path(), error);
    std::filesystem::create_directories(std::filesystem::path(keyPath).parent_path(), error);

    PkeyPtr key(EVP_EC_gen("prime256v1"), EVP_PKEY_free);
    X509Ptr cert(X509_new(), X509_free);
    if (!key || !cert) {
        return false;
    }

    unsigned char serialBytes[16];
    if (RAND_bytes(serialBytes, sizeof(serialBytes)) != 1) {
        return false;
    }
    serialBytes[0] &= 0x7F;  // Serial numbers have to be positive
    BIGNUM* serial = BN_bin2bn(serialBytes, sizeof(serialBytes), nullptr);
    const bool serialSet = serial && BN_to_ASN1_INTEGER(serial, X509_get_serialNumber(cert.get()));
    BN_free(serial);

    // Fixed dates, because this can run before the board's clock is set: it has no battery, so
    // a date worked out from "now" could be in 2000 and already expired once the clock is right.
    // Nothing renews this certificate, and it's trusted by hand rather than by expiry.
    bool ok = serialSet && X509_set_version(cert.get(), 2) == 1 &&
              ASN1_TIME_set_string(X509_getm_notBefore(cert.get()), "20200101000000Z") == 1 &&
              ASN1_TIME_set_string(X509_getm_notAfter(cert.get()), "20991231235959Z") == 1 &&
              X509_set_pubkey(cert.get(), key.get()) == 1;

    X509_NAME* name = X509_get_subject_name(cert.get());
    ok = ok && X509_NAME_add_entry_by_txt(
                   name, "CN", MBSTRING_ASC,
                   reinterpret_cast<const unsigned char*>(hostname.c_str()), -1, -1, 0) == 1;
    ok = ok && X509_set_issuer_name(cert.get(), name) == 1;

    const std::string altNames = "DNS:" + hostname + ",DNS:localhost";
    X509_EXTENSION* extension =
        X509V3_EXT_conf_nid(nullptr, nullptr, NID_subject_alt_name, altNames.c_str());
    ok = ok && extension && X509_add_ext(cert.get(), extension, -1) == 1;
    X509_EXTENSION_free(extension);

    ok = ok && X509_sign(cert.get(), key.get(), EVP_sha256()) != 0;
    if (!ok) {
        LOG_ERROR("Failed to build a TLS certificate");
        return false;
    }

    // The key first, because a certificate without its key is what would look broken next time
    return writePem(keyPath, 0600, [&](FILE* file) {
               return PEM_write_PrivateKey(file, key.get(), nullptr, nullptr, 0, nullptr, nullptr) == 1;
           }) &&
           writePem(certPath, 0644, [&](FILE* file) { return PEM_write_X509(file, cert.get()) == 1; });
}

#else

bool CertManager::isUsablePair(const std::string&, const std::string&) { return false; }
bool CertManager::createSelfSigned(const std::string&, const std::string&, const std::string&) {
    return false;
}

#endif

bool CertManager::initialize() {
    return initialize("smarthub.local", "");
}

bool CertManager::initialize(const std::string& hostname, const std::string& ipAddress) {
    // Check if certificates already exist and are valid
    if (certificatesExist()) {
        if (loadCertInfo() && isValid()) {
            LOG_INFO("Using existing TLS certificates");
            m_initialized = true;

            // Warn if renewal needed soon
            int daysLeft = daysUntilExpiry();
            if (daysLeft <= 30) {
                LOG_WARN("TLS certificate expires in %d days", daysLeft);
            }

            return true;
        } else {
            LOG_WARN("Existing certificates invalid or expired, regenerating...");
        }
    }

    // Generate new certificates
    if (!generateCertificates(hostname, ipAddress, 365)) {
        LOG_ERROR("Failed to generate TLS certificates");
        return false;
    }

    // Load the newly generated certificate info
    if (!loadCertInfo()) {
        LOG_ERROR("Failed to load generated certificate info");
        return false;
    }

    m_initialized = true;
    LOG_INFO("TLS certificates ready");
    return true;
}

std::string CertManager::caCertPath() const {
    return m_certDir + "/ca.crt";
}

std::string CertManager::serverCertPath() const {
    return m_certDir + "/server.crt";
}

std::string CertManager::serverKeyPath() const {
    return m_certDir + "/server.key";
}

bool CertManager::isValid() const {
    if (!m_certInfo.isValid) {
        return false;
    }

    time_t now = time(nullptr);
    return now >= m_certInfo.notBefore && now <= m_certInfo.notAfter;
}

bool CertManager::certificatesExist() const {
    struct stat st;
    return stat(serverCertPath().c_str(), &st) == 0 &&
           stat(serverKeyPath().c_str(), &st) == 0;
}

time_t CertManager::expirationDate() const {
    return m_certInfo.notAfter;
}

int CertManager::daysUntilExpiry() const {
    if (!m_certInfo.isValid) {
        return 0;
    }

    time_t now = time(nullptr);
    if (now >= m_certInfo.notAfter) {
        return 0;
    }

    return static_cast<int>((m_certInfo.notAfter - now) / (24 * 60 * 60));
}

std::optional<CertInfo> CertManager::getCertInfo() const {
    if (!m_certInfo.isValid) {
        return std::nullopt;
    }
    return m_certInfo;
}

bool CertManager::regenerate(const std::string& hostname,
                              const std::string& ipAddress,
                              int validityDays) {
    LOG_INFO("Regenerating TLS certificates...");

    if (!generateCertificates(hostname, ipAddress, validityDays)) {
        LOG_ERROR("Certificate regeneration failed");
        return false;
    }

    if (!loadCertInfo()) {
        LOG_ERROR("Failed to load regenerated certificate info");
        return false;
    }

    LOG_INFO("TLS certificates regenerated successfully");
    return true;
}

bool CertManager::needsRenewal() const {
    return daysUntilExpiry() <= 30;
}

bool CertManager::loadCertInfo() {
    m_certInfo = CertInfo{};

    if (!certificatesExist()) {
        return false;
    }

    return parseCertificate(serverCertPath(), m_certInfo);
}

bool CertManager::generateCertificates(const std::string& hostname,
                                         const std::string& ipAddress,
                                         int validityDays) {
    // Create certificate directory
    std::string mkdirCmd = "mkdir -p '" + m_certDir + "'";
    if (system(mkdirCmd.c_str()) != 0) {
        LOG_ERROR("Failed to create certificate directory: %s", m_certDir.c_str());
        return false;
    }

    // Find the generate-certs.sh script
    std::vector<std::string> scriptPaths = {
        "/opt/smarthub/tools/generate-certs.sh",
        "./tools/generate-certs.sh",
        "../tools/generate-certs.sh"
    };

    std::string scriptPath;
    for (const auto& path : scriptPaths) {
        struct stat st;
        if (stat(path.c_str(), &st) == 0) {
            scriptPath = path;
            break;
        }
    }

    // Build command
    std::ostringstream cmd;

    if (!scriptPath.empty()) {
        // Use the shell script
        cmd << scriptPath << " --force"
            << " --dir '" << m_certDir << "'"
            << " --hostname '" << hostname << "'"
            << " --days " << validityDays;

        if (!ipAddress.empty()) {
            cmd << " --ip '" << ipAddress << "'";
        }
    } else {
        // Fallback: generate directly with openssl commands
        LOG_INFO("Using inline OpenSSL commands for certificate generation");

        // Create temporary config file content
        std::string sanValue = "DNS:" + hostname + ",DNS:smarthub";
        if (!ipAddress.empty()) {
            sanValue += ",IP:" + ipAddress;
        }

        // Generate CA key and cert
        cmd << "openssl genrsa -out '" << m_certDir << "/ca.key' 4096 2>/dev/null && "
            << "openssl req -new -x509 -days 3650 "
            << "-key '" << m_certDir << "/ca.key' "
            << "-out '" << m_certDir << "/ca.crt' "
            << "-subj '/C=US/ST=Home/L=Home/O=SmartHub/CN=SmartHub CA' && ";

        // Generate server key
        cmd << "openssl genrsa -out '" << m_certDir << "/server.key' 2048 2>/dev/null && ";

        // Generate server CSR
        cmd << "openssl req -new "
            << "-key '" << m_certDir << "/server.key' "
            << "-out '" << m_certDir << "/server.csr' "
            << "-subj '/C=US/ST=Home/L=Home/O=SmartHub/CN=" << hostname << "' && ";

        // Sign server cert with SAN extension
        cmd << "openssl x509 -req -days " << validityDays << " "
            << "-in '" << m_certDir << "/server.csr' "
            << "-CA '" << m_certDir << "/ca.crt' "
            << "-CAkey '" << m_certDir << "/ca.key' "
            << "-CAcreateserial "
            << "-out '" << m_certDir << "/server.crt' "
            << "-extfile <(printf 'subjectAltName=" << sanValue << "') 2>/dev/null && ";

        // Set permissions and cleanup
        cmd << "chmod 600 '" << m_certDir << "'/*.key && "
            << "chmod 644 '" << m_certDir << "'/*.crt && "
            << "rm -f '" << m_certDir << "/server.csr'";
    }

    LOG_DEBUG("Running: %s", cmd.str().c_str());

    // Execute command
    std::string fullCmd = "bash -c \"" + cmd.str() + "\"";
    int result = system(fullCmd.c_str());

    if (result != 0) {
        LOG_ERROR("Certificate generation command failed with code %d", result);
        return false;
    }

    // Verify files were created
    if (!certificatesExist()) {
        LOG_ERROR("Certificate files not created");
        return false;
    }

    return true;
}

bool CertManager::parseCertificate(const std::string& certPath, CertInfo& info) {
#if HAVE_OPENSSL
    FILE* fp = fopen(certPath.c_str(), "r");
    if (!fp) {
        LOG_ERROR("Cannot open certificate file: %s", certPath.c_str());
        return false;
    }

    X509* cert = PEM_read_X509(fp, nullptr, nullptr, nullptr);
    fclose(fp);

    if (!cert) {
        LOG_ERROR("Failed to parse certificate: %s", certPath.c_str());
        return false;
    }

    // Extract subject
    char subjectBuf[256] = {0};
    X509_NAME* subjectName = X509_get_subject_name(cert);
    if (subjectName) {
        X509_NAME_oneline(subjectName, subjectBuf, sizeof(subjectBuf));
        info.subject = subjectBuf;
    }

    // Extract issuer
    char issuerBuf[256] = {0};
    X509_NAME* issuerName = X509_get_issuer_name(cert);
    if (issuerName) {
        X509_NAME_oneline(issuerName, issuerBuf, sizeof(issuerBuf));
        info.issuer = issuerBuf;
    }

    // Extract validity dates
    const ASN1_TIME* notBefore = X509_get0_notBefore(cert);
    const ASN1_TIME* notAfter = X509_get0_notAfter(cert);

    if (notBefore) {
        struct tm tm = {};
        ASN1_TIME_to_tm(notBefore, &tm);
        // ASN1_TIME_to_tm returns UTC, use timegm for proper conversion
        info.notBefore = timegm(&tm);
    }

    if (notAfter) {
        struct tm tm = {};
        ASN1_TIME_to_tm(notAfter, &tm);
        // ASN1_TIME_to_tm returns UTC, use timegm for proper conversion
        info.notAfter = timegm(&tm);
    }

    // Extract serial number
    const ASN1_INTEGER* serial = X509_get_serialNumber(cert);
    if (serial) {
        BIGNUM* bn = ASN1_INTEGER_to_BN(serial, nullptr);
        if (bn) {
            char* hex = BN_bn2hex(bn);
            if (hex) {
                info.serialNumber = hex;
                OPENSSL_free(hex);
            }
            BN_free(bn);
        }
    }

    X509_free(cert);
    info.isValid = true;
    return true;

#else
    // Fallback: Use openssl command-line tool
    std::string cmd = "openssl x509 -in '" + certPath + "' -noout -dates 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return false;
    }

    char buffer[256];
    info.isValid = false;

    while (fgets(buffer, sizeof(buffer), pipe)) {
        std::string line(buffer);

        if (line.find("notBefore=") == 0) {
            // Parse date - simplified
            info.isValid = true;
        }
        if (line.find("notAfter=") == 0) {
            // Parse expiry date - would need proper date parsing
            // For now just mark as valid for 365 days
            info.notAfter = time(nullptr) + (365 * 24 * 60 * 60);
        }
    }

    pclose(pipe);
    return info.isValid;
#endif
}

} // namespace smarthub::security
