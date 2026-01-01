/**
 * SmartHub User Manager
 *
 * Manages user accounts with secure password hashing.
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
 * User account information
 */
struct User {
    int id = 0;
    std::string username;
    std::string passwordHash;
    std::string role;  // "admin" or "user"
    uint64_t createdAt = 0;
    uint64_t lastLogin = 0;
    bool enabled = true;
};

/**
 * User Manager
 *
 * Handles user creation, authentication, and management.
 * Uses PBKDF2-SHA256 for password hashing.
 */
class UserManager {
public:
    /**
     * Constructor
     * @param db Database for user storage
     */
    explicit UserManager(Database& db);

    ~UserManager() = default;

    // Non-copyable
    UserManager(const UserManager&) = delete;
    UserManager& operator=(const UserManager&) = delete;

    /**
     * Initialize user tables in database
     * @return true if successful
     */
    bool initialize();

    /**
     * Create a new user
     * @param username Unique username
     * @param password Plaintext password (will be hashed)
     * @param role User role ("admin" or "user")
     * @return true if user created successfully
     */
    bool createUser(const std::string& username,
                    const std::string& password,
                    const std::string& role = "user");

    /**
     * Delete a user
     * @param username Username to delete
     * @return true if deleted
     */
    bool deleteUser(const std::string& username);

    /**
     * Change user's password
     * @param username Username
     * @param newPassword New password (will be hashed)
     * @return true if password changed
     */
    bool changePassword(const std::string& username,
                        const std::string& newPassword);

    /**
     * Get user by username
     * @param username Username to look up
     * @return User if found
     */
    std::optional<User> getUser(const std::string& username);

    /**
     * Get user by ID
     * @param id User ID
     * @return User if found
     */
    std::optional<User> getUserById(int id);

    /**
     * Get all users
     * @return List of users
     */
    std::vector<User> getAllUsers();

    /**
     * Authenticate user with password
     * @param username Username
     * @param password Plaintext password
     * @return true if authentication successful
     */
    bool authenticate(const std::string& username,
                      const std::string& password);

    /**
     * Update last login timestamp
     * @param username Username
     */
    void updateLastLogin(const std::string& username);

    /**
     * Enable or disable a user
     * @param username Username
     * @param enabled Enable status
     * @return true if updated
     */
    bool setUserEnabled(const std::string& username, bool enabled);

    /**
     * Change user's role
     * @param username Username
     * @param newRole New role ("admin" or "user")
     * @return true if role changed
     */
    bool changeRole(const std::string& username, const std::string& newRole);

    /**
     * Check if an admin user exists
     * Used to determine if first-run setup is needed
     * @return true if at least one admin exists
     */
    bool hasAdminUser() const;

    /**
     * Count total users
     * @return Number of users
     */
    int userCount() const;

    /**
     * Validate password strength
     * @param password Password to validate
     * @return Error message if invalid, empty if valid
     */
    static std::string validatePassword(const std::string& password);

    /**
     * Validate username
     * @param username Username to validate
     * @return Error message if invalid, empty if valid
     */
    static std::string validateUsername(const std::string& username);

private:
    /**
     * Hash password with PBKDF2-SHA256
     * @param password Plaintext password
     * @param salt Salt (generated if empty)
     * @return Hash in format: salt$iterations$hash
     */
    std::string hashPassword(const std::string& password,
                              const std::string& salt = "");

    /**
     * Generate random salt
     * @param length Salt length in bytes
     * @return Base64-encoded salt
     */
    std::string generateSalt(size_t length = 16);

    /**
     * Constant-time string comparison
     */
    bool secureCompare(const std::string& a, const std::string& b);

    Database& m_db;

    // PBKDF2 parameters
    static constexpr int PBKDF2_ITERATIONS = 100000;
    static constexpr int HASH_LENGTH = 32;  // 256 bits
};

} // namespace smarthub::security
