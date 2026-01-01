/**
 * SmartHub Setup Manager Implementation
 */

#include <smarthub/security/SetupManager.hpp>
#include <smarthub/security/UserManager.hpp>
#include <smarthub/security/CertManager.hpp>
#include <smarthub/security/CredentialStore.hpp>
#include <smarthub/database/Database.hpp>
#include <smarthub/core/Logger.hpp>

#include <fstream>
#include <sstream>
#include <array>
#include <cstdio>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>

namespace smarthub::security {

SetupManager::SetupManager(Database& db,
                           UserManager& userMgr,
                           CertManager& certMgr,
                           CredentialStore& credStore)
    : m_db(db)
    , m_userMgr(userMgr)
    , m_certMgr(certMgr)
    , m_credStore(credStore)
{
}

bool SetupManager::initialize() {
    // Create setup state table
    const char* createTableSql = R"(
        CREATE TABLE IF NOT EXISTS setup_state (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
    )";

    if (!m_db.execute(createTableSql)) {
        LOG_ERROR("Failed to create setup_state table");
        return false;
    }

    // Load existing state
    loadSetupState();

    LOG_INFO("Setup manager initialized (setup_complete=%s)",
             m_setupComplete ? "true" : "false");
    return true;
}

bool SetupManager::loadSetupState() {
    const char* selectSql = "SELECT value FROM setup_state WHERE key = 'setup_complete'";

    auto stmt = m_db.prepare(selectSql);
    if (!stmt || !stmt->isValid()) {
        return false;
    }

    if (stmt->step()) {
        std::string value = stmt->getString(0);
        m_setupComplete = (value == "true" || value == "1");
    }

    return true;
}

bool SetupManager::saveSetupState() {
    const char* upsertSql = R"(
        INSERT INTO setup_state (key, value) VALUES ('setup_complete', ?)
        ON CONFLICT(key) DO UPDATE SET value = excluded.value;
    )";

    auto stmt = m_db.prepare(upsertSql);
    if (!stmt || !stmt->isValid()) {
        return false;
    }

    stmt->bind(1, m_setupComplete ? "true" : "false");
    return stmt->execute();
}

bool SetupManager::isSetupRequired() {
    // Setup is required if:
    // 1. Setup flag is not set
    // 2. No admin user exists
    if (m_setupComplete) {
        return false;
    }

    // Check if admin user exists
    if (m_userMgr.hasAdminUser()) {
        // Admin exists but setup wasn't marked complete - mark it now
        m_setupComplete = true;
        saveSetupState();
        return false;
    }

    return true;
}

SetupStatus SetupManager::getSetupStatus() {
    SetupStatus status;

    status.hasAdminUser = m_userMgr.hasAdminUser();
    status.hasCertificates = m_certMgr.certificatesExist() && m_certMgr.isValid();
    status.hasCredentialStore = m_credStore.isUnlocked();
    status.isSetupComplete = m_setupComplete && status.hasAdminUser;

    if (!status.hasAdminUser) {
        status.message = "Admin account not configured";
    } else if (!status.hasCertificates) {
        status.message = "TLS certificates not generated";
    } else if (!status.hasCredentialStore) {
        status.message = "Credential store not initialized";
    } else {
        status.message = "System is configured";
    }

    return status;
}

std::string SetupManager::validateConfig(const SetupConfig& config) {
    // Validate admin username
    std::string usernameError = UserManager::validateUsername(config.adminUsername);
    if (!usernameError.empty()) {
        return "Admin username: " + usernameError;
    }

    // Validate admin password
    std::string passwordError = UserManager::validatePassword(config.adminPassword);
    if (!passwordError.empty()) {
        return "Admin password: " + passwordError;
    }

    // Validate hostname
    if (config.hostname.empty()) {
        return "Hostname cannot be empty";
    }
    if (config.hostname.length() > 253) {
        return "Hostname too long";
    }

    // Validate credential passphrase
    if (config.credentialPassphrase.empty()) {
        return "Credential passphrase cannot be empty";
    }
    if (config.credentialPassphrase.length() < 8) {
        return "Credential passphrase must be at least 8 characters";
    }

    return "";
}

bool SetupManager::performSetup(const SetupConfig& config) {
    LOG_INFO("Starting first-run setup...");

    // Validate configuration
    std::string error = validateConfig(config);
    if (!error.empty()) {
        LOG_ERROR("Setup validation failed: %s", error.c_str());
        return false;
    }

    // Step 1: Create admin user
    LOG_INFO("Creating admin account...");
    if (!m_userMgr.createUser(config.adminUsername, config.adminPassword, "admin")) {
        LOG_ERROR("Failed to create admin user");
        return false;
    }

    // Step 2: Generate TLS certificates
    LOG_INFO("Generating TLS certificates...");
    std::string hostname = config.hostname.empty() ? getDefaultHostname() : config.hostname;
    std::string ipAddress = config.ipAddress.empty() ? getLocalIpAddress() : config.ipAddress;

    if (!m_certMgr.initialize(hostname, ipAddress)) {
        LOG_ERROR("Failed to generate TLS certificates");
        // Continue anyway - certificates can be regenerated
    }

    // Step 3: Initialize credential store
    LOG_INFO("Initializing credential store...");
    if (!m_credStore.unlock(config.credentialPassphrase)) {
        LOG_ERROR("Failed to initialize credential store");
        // Continue anyway - can be set up later
    }

    // Mark setup complete
    markSetupComplete();

    LOG_INFO("First-run setup completed successfully");
    return true;
}

void SetupManager::markSetupComplete() {
    m_setupComplete = true;
    saveSetupState();
    LOG_INFO("Setup marked as complete");
}

void SetupManager::resetSetupState() {
    m_setupComplete = false;
    saveSetupState();
    LOG_WARN("Setup state reset");
}

std::string SetupManager::getDefaultHostname() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return std::string(hostname);
    }
    return "smarthub";
}

std::string SetupManager::getLocalIpAddress() {
    struct ifaddrs *ifaddr, *ifa;
    std::string result = "127.0.0.1";

    if (getifaddrs(&ifaddr) == -1) {
        return result;
    }

    // Find the first non-loopback IPv4 address
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;

        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *addr = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));

            // Skip loopback
            if (std::string(ip) != "127.0.0.1") {
                result = ip;
                break;
            }
        }
    }

    freeifaddrs(ifaddr);
    return result;
}

} // namespace smarthub::security
