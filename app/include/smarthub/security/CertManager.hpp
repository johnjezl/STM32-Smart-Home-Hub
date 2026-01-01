/**
 * SmartHub Certificate Manager
 *
 * Manages TLS certificates for HTTPS.
 * Supports self-signed certificate generation and renewal.
 */
#pragma once

#include <string>
#include <ctime>
#include <optional>

namespace smarthub::security {

/**
 * Certificate information
 */
struct CertInfo {
    std::string subject;
    std::string issuer;
    time_t notBefore = 0;
    time_t notAfter = 0;
    std::string serialNumber;
    bool isValid = false;
};

/**
 * TLS Certificate Manager
 *
 * Handles generation, loading, and renewal of TLS certificates.
 */
class CertManager {
public:
    /**
     * Constructor
     * @param certDir Directory to store certificates
     */
    explicit CertManager(const std::string& certDir);

    ~CertManager() = default;

    // Non-copyable
    CertManager(const CertManager&) = delete;
    CertManager& operator=(const CertManager&) = delete;

    /**
     * Initialize certificate manager
     * Checks for existing certificates or generates new ones
     * @return true if certificates are ready for use
     */
    bool initialize();

    /**
     * Initialize with specific hostname and IP
     * @param hostname Server hostname (e.g., "smarthub.local")
     * @param ipAddress Server IP address (optional)
     * @return true if certificates are ready for use
     */
    bool initialize(const std::string& hostname, const std::string& ipAddress = "");

    /**
     * Get CA certificate path
     */
    std::string caCertPath() const;

    /**
     * Get server certificate path
     */
    std::string serverCertPath() const;

    /**
     * Get server private key path
     */
    std::string serverKeyPath() const;

    /**
     * Check if certificates are valid
     */
    bool isValid() const;

    /**
     * Check if certificates exist
     */
    bool certificatesExist() const;

    /**
     * Get certificate expiration date
     */
    time_t expirationDate() const;

    /**
     * Get days until certificate expires
     */
    int daysUntilExpiry() const;

    /**
     * Get detailed certificate information
     */
    std::optional<CertInfo> getCertInfo() const;

    /**
     * Regenerate certificates
     * @param hostname Server hostname
     * @param ipAddress Server IP address (optional)
     * @param validityDays Certificate validity period
     * @return true if generation succeeded
     */
    bool regenerate(const std::string& hostname,
                    const std::string& ipAddress = "",
                    int validityDays = 365);

    /**
     * Check if certificates need renewal (within 30 days of expiry)
     */
    bool needsRenewal() const;

    /**
     * Get certificate directory
     */
    const std::string& certDir() const { return m_certDir; }

private:
    /**
     * Load certificate information from file
     */
    bool loadCertInfo();

    /**
     * Generate certificates using openssl
     */
    bool generateCertificates(const std::string& hostname,
                               const std::string& ipAddress,
                               int validityDays);

    /**
     * Parse X.509 certificate to extract info
     */
    bool parseCertificate(const std::string& certPath, CertInfo& info);

    std::string m_certDir;
    CertInfo m_certInfo;
    bool m_initialized = false;
};

} // namespace smarthub::security
