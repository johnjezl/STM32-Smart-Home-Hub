/**
 * SmartHub API Token Manager
 *
 * Manages long-lived API tokens for programmatic access.
 */
#pragma once

#include <string>
#include <optional>
#include <vector>
#include <cstdint>

namespace smarthub {
class Database;
}

namespace smarthub::security {

/**
 * API Token information
 */
struct ApiToken {
    int id = 0;
    std::string token;       // Only shown on creation
    std::string tokenHash;   // Stored in database
    std::string tokenPrefix; // First 8 chars for identification
    int userId = 0;
    std::string name;        // User-friendly name
    uint64_t createdAt = 0;
    uint64_t lastUsed = 0;
    uint64_t expiresAt = 0;  // 0 = never expires
    bool enabled = true;
};

/**
 * API Token Manager
 *
 * Handles creation, validation, and revocation of API tokens.
 * Tokens are stored hashed (like passwords) for security.
 */
class ApiTokenManager {
public:
    /**
     * Constructor
     * @param db Database for token storage
     */
    explicit ApiTokenManager(Database& db);

    ~ApiTokenManager() = default;

    // Non-copyable
    ApiTokenManager(const ApiTokenManager&) = delete;
    ApiTokenManager& operator=(const ApiTokenManager&) = delete;

    /**
     * Initialize token tables in database
     * @return true if successful
     */
    bool initialize();

    /**
     * Create a new API token
     * @param userId User ID who owns the token
     * @param name User-friendly name for the token
     * @param expiryDays Days until expiration (0 = never)
     * @return Token (only shown once!) or empty on failure
     */
    std::string createToken(int userId,
                             const std::string& name,
                             int expiryDays = 0);

    /**
     * Validate an API token
     * @param token Token string
     * @return Token info if valid (token field will be empty)
     */
    std::optional<ApiToken> validateToken(const std::string& token);

    /**
     * Revoke a token by ID
     * @param tokenId Token ID
     * @return true if revoked
     */
    bool revokeToken(int tokenId);

    /**
     * Revoke a token by its value
     * @param token Token string
     * @return true if revoked
     */
    bool revokeTokenByValue(const std::string& token);

    /**
     * Revoke all tokens for a user
     * @param userId User ID
     * @return Number of tokens revoked
     */
    int revokeUserTokens(int userId);

    /**
     * Get all tokens for a user (tokens hidden)
     * @param userId User ID
     * @return List of tokens
     */
    std::vector<ApiToken> getUserTokens(int userId);

    /**
     * Update last used timestamp
     * @param tokenId Token ID
     */
    void updateLastUsed(int tokenId);

    /**
     * Set token enabled status
     * @param tokenId Token ID
     * @param enabled Enable status
     * @return true if updated
     */
    bool setTokenEnabled(int tokenId, bool enabled);

    /**
     * Count tokens for a user
     * @param userId User ID
     * @return Number of tokens
     */
    int countUserTokens(int userId);

    /**
     * Cleanup expired tokens
     * Should be called periodically
     * @return Number of tokens cleaned up
     */
    int cleanupExpired();

private:
    /**
     * Generate secure random token
     * @return Token string (40 chars)
     */
    std::string generateToken();

    /**
     * Hash a token for storage
     */
    std::string hashToken(const std::string& token);

    /**
     * Get token prefix for display
     */
    static std::string getTokenPrefix(const std::string& token);

    Database& m_db;

    // Token length (32 bytes = 64 hex chars)
    static constexpr int TOKEN_BYTES = 32;
};

} // namespace smarthub::security
