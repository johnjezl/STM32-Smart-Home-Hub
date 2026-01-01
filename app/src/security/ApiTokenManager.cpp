/**
 * SmartHub API Token Manager Implementation
 */

#include <smarthub/security/ApiTokenManager.hpp>
#include <smarthub/database/Database.hpp>
#include <smarthub/core/Logger.hpp>

#include <chrono>
#include <sstream>
#include <iomanip>

// OpenSSL for hashing and random
#if __has_include(<openssl/sha.h>)
#include <openssl/sha.h>
#include <openssl/rand.h>
#define HAVE_OPENSSL 1
#else
#define HAVE_OPENSSL 0
#endif

namespace smarthub::security {

ApiTokenManager::ApiTokenManager(Database& db)
    : m_db(db)
{
}

bool ApiTokenManager::initialize() {
    const char* createTableSql = R"(
        CREATE TABLE IF NOT EXISTS api_tokens (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            token_hash TEXT NOT NULL,
            token_prefix TEXT NOT NULL,
            name TEXT NOT NULL,
            created_at INTEGER NOT NULL,
            last_used INTEGER DEFAULT 0,
            expires_at INTEGER DEFAULT 0,
            enabled INTEGER DEFAULT 1,
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
        );
        CREATE INDEX IF NOT EXISTS idx_api_tokens_hash ON api_tokens(token_hash);
        CREATE INDEX IF NOT EXISTS idx_api_tokens_user ON api_tokens(user_id);
    )";

    if (!m_db.execute(createTableSql)) {
        LOG_ERROR("Failed to create api_tokens table");
        return false;
    }

    LOG_INFO("API Token manager initialized");
    return true;
}

std::string ApiTokenManager::createToken(int userId,
                                          const std::string& name,
                                          int expiryDays) {
    // Generate token
    std::string token = generateToken();
    if (token.empty()) {
        LOG_ERROR("Failed to generate API token");
        return "";
    }

    // Hash token for storage
    std::string tokenHash = hashToken(token);
    std::string prefix = getTokenPrefix(token);

    // Get current time
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    // Calculate expiration
    int64_t expiresAt = 0;
    if (expiryDays > 0) {
        expiresAt = timestamp + (expiryDays * 24 * 60 * 60);
    }

    // Insert token
    const char* insertSql =
        "INSERT INTO api_tokens (user_id, token_hash, token_prefix, name, created_at, expires_at) "
        "VALUES (?, ?, ?, ?, ?, ?)";

    auto stmt = m_db.prepare(insertSql);
    if (!stmt || !stmt->isValid()) {
        LOG_ERROR("Failed to prepare token insert");
        return "";
    }

    stmt->bind(1, userId);
    stmt->bind(2, tokenHash);
    stmt->bind(3, prefix);
    stmt->bind(4, name);
    stmt->bind(5, timestamp);
    stmt->bind(6, expiresAt);

    if (!stmt->execute()) {
        LOG_ERROR("Failed to insert API token");
        return "";
    }

    if (expiryDays > 0) {
        LOG_INFO("Created API token '%s' for user %d (expires in %d days)",
                 name.c_str(), userId, expiryDays);
    } else {
        LOG_INFO("Created API token '%s' for user %d (never expires)",
                 name.c_str(), userId);
    }

    return token;
}

std::optional<ApiToken> ApiTokenManager::validateToken(const std::string& token) {
    if (token.empty()) {
        return std::nullopt;
    }

    // Hash the provided token
    std::string tokenHash = hashToken(token);

    // Look up in database
    const char* selectSql =
        "SELECT id, user_id, token_prefix, name, created_at, last_used, expires_at, enabled "
        "FROM api_tokens WHERE token_hash = ?";

    auto stmt = m_db.prepare(selectSql);
    if (!stmt || !stmt->isValid()) {
        return std::nullopt;
    }

    stmt->bind(1, tokenHash);

    if (!stmt->step()) {
        return std::nullopt;
    }

    ApiToken apiToken;
    apiToken.id = stmt->getInt(0);
    apiToken.userId = stmt->getInt(1);
    apiToken.tokenPrefix = stmt->getString(2);
    apiToken.name = stmt->getString(3);
    apiToken.createdAt = static_cast<uint64_t>(stmt->getInt64(4));
    apiToken.lastUsed = static_cast<uint64_t>(stmt->getInt64(5));
    apiToken.expiresAt = static_cast<uint64_t>(stmt->getInt64(6));
    apiToken.enabled = stmt->getInt(7) != 0;

    // Check if enabled
    if (!apiToken.enabled) {
        LOG_DEBUG("API token disabled: %s", apiToken.tokenPrefix.c_str());
        return std::nullopt;
    }

    // Check expiration
    if (apiToken.expiresAt > 0) {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();

        if (static_cast<uint64_t>(timestamp) >= apiToken.expiresAt) {
            LOG_DEBUG("API token expired: %s", apiToken.tokenPrefix.c_str());
            return std::nullopt;
        }
    }

    // Update last used
    updateLastUsed(apiToken.id);

    return apiToken;
}

bool ApiTokenManager::revokeToken(int tokenId) {
    const char* deleteSql = "DELETE FROM api_tokens WHERE id = ?";

    auto stmt = m_db.prepare(deleteSql);
    if (!stmt || !stmt->isValid()) {
        return false;
    }

    stmt->bind(1, tokenId);

    if (!stmt->execute()) {
        return false;
    }

    LOG_INFO("Revoked API token ID %d", tokenId);
    return true;
}

bool ApiTokenManager::revokeTokenByValue(const std::string& token) {
    std::string tokenHash = hashToken(token);

    const char* deleteSql = "DELETE FROM api_tokens WHERE token_hash = ?";

    auto stmt = m_db.prepare(deleteSql);
    if (!stmt || !stmt->isValid()) {
        return false;
    }

    stmt->bind(1, tokenHash);

    if (!stmt->execute()) {
        return false;
    }

    LOG_INFO("Revoked API token");
    return true;
}

int ApiTokenManager::revokeUserTokens(int userId) {
    // Count first
    int count = countUserTokens(userId);

    const char* deleteSql = "DELETE FROM api_tokens WHERE user_id = ?";

    auto stmt = m_db.prepare(deleteSql);
    if (!stmt || !stmt->isValid()) {
        return 0;
    }

    stmt->bind(1, userId);

    if (!stmt->execute()) {
        return 0;
    }

    LOG_INFO("Revoked %d API tokens for user %d", count, userId);
    return count;
}

std::vector<ApiToken> ApiTokenManager::getUserTokens(int userId) {
    std::vector<ApiToken> tokens;

    const char* selectSql =
        "SELECT id, user_id, token_prefix, name, created_at, last_used, expires_at, enabled "
        "FROM api_tokens WHERE user_id = ? ORDER BY created_at DESC";

    auto stmt = m_db.prepare(selectSql);
    if (!stmt || !stmt->isValid()) {
        return tokens;
    }

    stmt->bind(1, userId);

    while (stmt->step()) {
        ApiToken token;
        token.id = stmt->getInt(0);
        token.userId = stmt->getInt(1);
        token.tokenPrefix = stmt->getString(2);
        token.name = stmt->getString(3);
        token.createdAt = static_cast<uint64_t>(stmt->getInt64(4));
        token.lastUsed = static_cast<uint64_t>(stmt->getInt64(5));
        token.expiresAt = static_cast<uint64_t>(stmt->getInt64(6));
        token.enabled = stmt->getInt(7) != 0;
        // Token is not included - only hash is stored
        tokens.push_back(token);
    }

    return tokens;
}

void ApiTokenManager::updateLastUsed(int tokenId) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    const char* updateSql = "UPDATE api_tokens SET last_used = ? WHERE id = ?";

    auto stmt = m_db.prepare(updateSql);
    if (stmt && stmt->isValid()) {
        stmt->bind(1, timestamp);
        stmt->bind(2, tokenId);
        stmt->execute();
    }
}

bool ApiTokenManager::setTokenEnabled(int tokenId, bool enabled) {
    const char* updateSql = "UPDATE api_tokens SET enabled = ? WHERE id = ?";

    auto stmt = m_db.prepare(updateSql);
    if (!stmt || !stmt->isValid()) {
        return false;
    }

    stmt->bind(1, enabled ? 1 : 0);
    stmt->bind(2, tokenId);

    if (!stmt->execute()) {
        return false;
    }

    LOG_INFO("API token %d %s", tokenId, enabled ? "enabled" : "disabled");
    return true;
}

int ApiTokenManager::countUserTokens(int userId) {
    const char* selectSql = "SELECT COUNT(*) FROM api_tokens WHERE user_id = ?";

    auto stmt = m_db.prepare(selectSql);
    if (!stmt || !stmt->isValid()) {
        return 0;
    }

    stmt->bind(1, userId);

    if (!stmt->step()) {
        return 0;
    }

    return stmt->getInt(0);
}

int ApiTokenManager::cleanupExpired() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    // Count first
    const char* countSql =
        "SELECT COUNT(*) FROM api_tokens WHERE expires_at > 0 AND expires_at < ?";

    auto countStmt = m_db.prepare(countSql);
    if (!countStmt || !countStmt->isValid()) {
        return 0;
    }

    countStmt->bind(1, timestamp);
    int count = 0;
    if (countStmt->step()) {
        count = countStmt->getInt(0);
    }

    if (count > 0) {
        const char* deleteSql =
            "DELETE FROM api_tokens WHERE expires_at > 0 AND expires_at < ?";

        auto stmt = m_db.prepare(deleteSql);
        if (stmt && stmt->isValid()) {
            stmt->bind(1, timestamp);
            stmt->execute();
            LOG_DEBUG("Cleaned up %d expired API tokens", count);
        }
    }

    return count;
}

std::string ApiTokenManager::generateToken() {
#if HAVE_OPENSSL
    unsigned char bytes[TOKEN_BYTES];
    if (RAND_bytes(bytes, sizeof(bytes)) != 1) {
        LOG_ERROR("Failed to generate random token");
        return "";
    }

    // Convert to hex string
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char byte : bytes) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
#else
    LOG_ERROR("OpenSSL not available for token generation");
    return "";
#endif
}

std::string ApiTokenManager::hashToken(const std::string& token) {
#if HAVE_OPENSSL
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(token.c_str()),
           token.length(), hash);

    // Convert to hex string
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char byte : hash) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
#else
    // Fallback - not secure
    return token;
#endif
}

std::string ApiTokenManager::getTokenPrefix(const std::string& token) {
    if (token.length() >= 8) {
        return token.substr(0, 8) + "...";
    }
    return token;
}

} // namespace smarthub::security
