/**
 * SmartHub Setup Manager
 *
 * Handles first-run setup and system initialization.
 */
#pragma once

#include <string>
#include <functional>

namespace smarthub {
class Database;
}

namespace smarthub::security {

class UserManager;
class CertManager;
class CredentialStore;

/**
 * Setup status
 */
struct SetupStatus {
    bool isSetupComplete = false;
    bool hasAdminUser = false;
    bool hasCertificates = false;
    bool hasCredentialStore = false;
    std::string message;
};

/**
 * First-run setup configuration
 */
struct SetupConfig {
    std::string adminUsername;
    std::string adminPassword;
    std::string hostname;
    std::string ipAddress;
    std::string credentialPassphrase;
};

/**
 * Setup Manager
 *
 * Manages first-run setup and system initialization checks.
 */
class SetupManager {
public:
    /**
     * Constructor
     * @param db Database for setup state
     * @param userMgr User manager for admin account
     * @param certMgr Certificate manager for TLS
     * @param credStore Credential store for secrets
     */
    SetupManager(Database& db,
                 UserManager& userMgr,
                 CertManager& certMgr,
                 CredentialStore& credStore);

    ~SetupManager() = default;

    // Non-copyable
    SetupManager(const SetupManager&) = delete;
    SetupManager& operator=(const SetupManager&) = delete;

    /**
     * Initialize setup manager
     * Creates setup state table if needed.
     * @return true if successful
     */
    bool initialize();

    /**
     * Check if first-run setup is needed
     * @return true if setup is required
     */
    bool isSetupRequired();

    /**
     * Get detailed setup status
     * @return Setup status information
     */
    SetupStatus getSetupStatus();

    /**
     * Perform first-run setup
     * Creates admin user, generates certificates, sets up credential store.
     * @param config Setup configuration
     * @return true if setup completed successfully
     */
    bool performSetup(const SetupConfig& config);

    /**
     * Validate setup configuration
     * @param config Configuration to validate
     * @return Error message, or empty if valid
     */
    std::string validateConfig(const SetupConfig& config);

    /**
     * Mark setup as complete
     * Called after successful setup.
     */
    void markSetupComplete();

    /**
     * Reset setup state
     * WARNING: This does NOT delete data, just resets the setup flag.
     */
    void resetSetupState();

    /**
     * Get default hostname
     * @return Suggested hostname based on system
     */
    static std::string getDefaultHostname();

    /**
     * Get local IP address
     * @return Primary local IP address
     */
    static std::string getLocalIpAddress();

private:
    bool loadSetupState();
    bool saveSetupState();

    Database& m_db;
    UserManager& m_userMgr;
    CertManager& m_certMgr;
    CredentialStore& m_credStore;

    bool m_setupComplete = false;
};

} // namespace smarthub::security
