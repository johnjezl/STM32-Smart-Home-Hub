/**
 * SmartHub Certificate Manager Implementation
 */

#include <smarthub/security/CertManager.hpp>
#include <smarthub/core/Logger.hpp>

#include <arpa/inet.h>
#include <cctype>
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

#if HAVE_OPENSSL

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
    // A leftover file would keep its permissions, which could expose the key
    ::unlink(temporary.c_str());
    const int fd = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, mode);
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

    // Some clients won't accept a server certificate without this, even if it's trusted by hand
    X509_EXTENSION* usage =
        X509V3_EXT_conf_nid(nullptr, nullptr, NID_ext_key_usage, "serverAuth");
    ok = ok && usage && X509_add_ext(cert.get(), usage, -1) == 1;
    X509_EXTENSION_free(usage);

    ok = ok && X509_sign(cert.get(), key.get(), EVP_sha256()) != 0;
    if (!ok) {
        LOG_ERROR("Failed to build a TLS certificate");
        return false;
    }

    // The key first, because startup treats a key on its own as an interrupted first boot and
    // tries again, but leaves a certificate alone
    return writePem(keyPath, 0600, [&](FILE* file) {
               return PEM_write_PrivateKey(file, key.get(), nullptr, nullptr, 0, nullptr, nullptr) == 1;
           }) &&
           writePem(certPath, 0644, [&](FILE* file) { return PEM_write_X509(file, cert.get()) == 1; });
}

bool CertManager::createSignedPair(const std::string& hostname, const std::string& ipAddress,
                                   int validityDays) {
    // A CA, and a server certificate it signs, so a client can trust the CA once and keep
    // trusting the server across regeneration. Built here rather than with the openssl tool, so
    // nothing from outside goes through a shell.
    PkeyPtr caKey(EVP_EC_gen("prime256v1"), EVP_PKEY_free);
    PkeyPtr serverKey(EVP_EC_gen("prime256v1"), EVP_PKEY_free);
    X509Ptr caCert(X509_new(), X509_free);
    X509Ptr serverCert(X509_new(), X509_free);
    if (!caKey || !serverKey || !caCert || !serverCert) {
        return false;
    }

    auto setSerial = [](X509* cert) {
        unsigned char bytes[16];
        if (RAND_bytes(bytes, sizeof(bytes)) != 1) {
            return false;
        }
        bytes[0] &= 0x7F;  // Serial numbers have to be positive
        BIGNUM* serial = BN_bin2bn(bytes, sizeof(bytes), nullptr);
        const bool set = serial && BN_to_ASN1_INTEGER(serial, X509_get_serialNumber(cert));
        BN_free(serial);
        return set;
    };

    auto setName = [](X509_NAME* name, const std::string& commonName) {
        return X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
                                          reinterpret_cast<const unsigned char*>("SmartHub"), -1,
                                          -1, 0) == 1 &&
               X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                          reinterpret_cast<const unsigned char*>(commonName.c_str()),
                                          -1, -1, 0) == 1;
    };

    auto addExtension = [](X509* cert, X509* issuer, int nid, const std::string& value) {
        X509V3_CTX context;
        X509V3_set_ctx(&context, issuer, cert, nullptr, nullptr, 0);
        X509_EXTENSION* extension = X509V3_EXT_conf_nid(nullptr, &context, nid, value.c_str());
        const bool added = extension && X509_add_ext(cert, extension, -1) == 1;
        X509_EXTENSION_free(extension);
        return added;
    };

    const long caSeconds = 3650L * 24 * 60 * 60;
    const long serverSeconds = static_cast<long>(validityDays) * 24 * 60 * 60;

    bool ok = setSerial(caCert.get()) && X509_set_version(caCert.get(), 2) == 1 &&
              X509_gmtime_adj(X509_getm_notBefore(caCert.get()), 0) &&
              X509_gmtime_adj(X509_getm_notAfter(caCert.get()), caSeconds) &&
              X509_set_pubkey(caCert.get(), caKey.get()) == 1 &&
              setName(X509_get_subject_name(caCert.get()), "SmartHub CA") &&
              X509_set_issuer_name(caCert.get(), X509_get_subject_name(caCert.get())) == 1 &&
              addExtension(caCert.get(), caCert.get(), NID_basic_constraints, "critical,CA:TRUE") &&
              addExtension(caCert.get(), caCert.get(), NID_key_usage, "critical,keyCertSign,cRLSign") &&
              X509_sign(caCert.get(), caKey.get(), EVP_sha256()) != 0;

    std::string altNames = "DNS:" + hostname + ",DNS:smarthub";
    if (!ipAddress.empty()) {
        altNames += ",IP:" + ipAddress;
    }

    ok = ok && setSerial(serverCert.get()) && X509_set_version(serverCert.get(), 2) == 1 &&
         X509_gmtime_adj(X509_getm_notBefore(serverCert.get()), 0) &&
         X509_gmtime_adj(X509_getm_notAfter(serverCert.get()), serverSeconds) &&
         X509_set_pubkey(serverCert.get(), serverKey.get()) == 1 &&
         setName(X509_get_subject_name(serverCert.get()), hostname) &&
         X509_set_issuer_name(serverCert.get(), X509_get_subject_name(caCert.get())) == 1 &&
         addExtension(serverCert.get(), caCert.get(), NID_basic_constraints, "CA:FALSE") &&
         addExtension(serverCert.get(), caCert.get(), NID_subject_alt_name, altNames) &&
         addExtension(serverCert.get(), caCert.get(), NID_ext_key_usage, "serverAuth") &&
         X509_sign(serverCert.get(), caKey.get(), EVP_sha256()) != 0;

    if (!ok) {
        LOG_ERROR("Failed to build TLS certificates");
        return false;
    }

    // The CA's key isn't kept: nothing else is signed with it, and a new CA comes with each
    // regeneration. The server key goes first, as startup leaves a certificate on its own alone.
    return writePem(caCertPath(), 0644,
                    [&](FILE* file) { return PEM_write_X509(file, caCert.get()) == 1; }) &&
           writePem(serverKeyPath(), 0600, [&](FILE* file) {
               return PEM_write_PrivateKey(file, serverKey.get(), nullptr, nullptr, 0, nullptr,
                                           nullptr) == 1;
           }) &&
           writePem(serverCertPath(), 0644,
                    [&](FILE* file) { return PEM_write_X509(file, serverCert.get()) == 1; });
}

#else

bool CertManager::createSignedPair(const std::string&, const std::string&, int) {
    LOG_ERROR("Built without OpenSSL, so certificates can't be generated");
    return false;
}
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
    if (!isValidHostname(hostname)) {
        LOG_ERROR("Not generating TLS certificates: invalid hostname");
        return false;
    }
    if (!ipAddress.empty() && !isValidIpAddress(ipAddress)) {
        LOG_ERROR("Not generating TLS certificates: invalid IP address");
        return false;
    }
    if (validityDays < 1) {
        LOG_ERROR("Not generating TLS certificates: validity must be at least a day");
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(m_certDir, error);
    if (error) {
        LOG_ERROR("Failed to create certificate directory: %s", m_certDir.c_str());
        return false;
    }

    if (!createSignedPair(hostname, ipAddress, validityDays)) {
        LOG_ERROR("Certificate generation failed");
        return false;
    }

    // Verify files were created
    if (!certificatesExist()) {
        LOG_ERROR("Certificate files not created");
        return false;
    }

    return true;
}

bool CertManager::isValidHostname(const std::string& hostname) {
    // Letters, digits, hyphens and dots, as in DNS. Anything else, such as a comma, could change
    // the meaning of the subjectAltName it goes into.
    if (hostname.empty() || hostname.size() > 253 || hostname.front() == '.' ||
        hostname.front() == '-') {
        return false;
    }
    for (unsigned char c : hostname) {
        if (!std::isalnum(c) && c != '-' && c != '.') {
            return false;
        }
    }
    return true;
}

bool CertManager::isValidIpAddress(const std::string& ipAddress) {
    unsigned char buffer[sizeof(struct in6_addr)];
    return inet_pton(AF_INET, ipAddress.c_str(), buffer) == 1 ||
           inet_pton(AF_INET6, ipAddress.c_str(), buffer) == 1;
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
    (void)certPath;
    (void)info;
    LOG_ERROR("Built without OpenSSL, so certificates can't be read");
    return false;
#endif
}

} // namespace smarthub::security
