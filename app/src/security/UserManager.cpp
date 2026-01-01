/**
 * SmartHub User Manager Implementation
 */

#include <smarthub/security/UserManager.hpp>
#include <smarthub/database/Database.hpp>
#include <smarthub/core/Logger.hpp>

#include <algorithm>
#include <chrono>
#include <regex>
#include <cstring>

// OpenSSL for password hashing
#if __has_include(<openssl/evp.h>)
#include <openssl/evp.h>
#include <openssl/rand.h>
#define HAVE_OPENSSL 1
#else
#define HAVE_OPENSSL 0
#endif

namespace smarthub::security {

// Base64 encoding table
static const char BASE64_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64Encode(const unsigned char* data, size_t len) {
    std::string result;
    result.reserve(((len + 2) / 3) * 4);

    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < len) n |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (i + 2 < len) n |= static_cast<uint32_t>(data[i + 2]);

        result.push_back(BASE64_CHARS[(n >> 18) & 0x3F]);
        result.push_back(BASE64_CHARS[(n >> 12) & 0x3F]);
        result.push_back(i + 1 < len ? BASE64_CHARS[(n >> 6) & 0x3F] : '=');
        result.push_back(i + 2 < len ? BASE64_CHARS[n & 0x3F] : '=');
    }

    return result;
}

static std::vector<unsigned char> base64Decode(const std::string& encoded) {
    std::vector<unsigned char> result;

    std::vector<int> decodeTable(256, -1);
    for (int i = 0; i < 64; i++) {
        decodeTable[static_cast<unsigned char>(BASE64_CHARS[i])] = i;
    }

    int val = 0;
    int bits = 0;
    for (char c : encoded) {
        if (c == '=') break;
        if (decodeTable[static_cast<unsigned char>(c)] == -1) continue;

        val = (val << 6) | decodeTable[static_cast<unsigned char>(c)];
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            result.push_back(static_cast<unsigned char>((val >> bits) & 0xFF));
        }
    }

    return result;
}

UserManager::UserManager(Database& db)
    : m_db(db)
{
}

bool UserManager::initialize() {
    // Create users table
    const char* createTableSql = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            role TEXT NOT NULL DEFAULT 'user',
            created_at INTEGER NOT NULL,
            last_login INTEGER DEFAULT 0,
            enabled INTEGER DEFAULT 1
        );
        CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
    )";

    if (!m_db.execute(createTableSql)) {
        LOG_ERROR("Failed to create users table");
        return false;
    }

    LOG_INFO("User manager initialized");
    return true;
}

bool UserManager::createUser(const std::string& username,
                              const std::string& password,
                              const std::string& role) {
    // Validate username
    std::string usernameError = validateUsername(username);
    if (!usernameError.empty()) {
        LOG_WARN("Invalid username: %s", usernameError.c_str());
        return false;
    }

    // Validate password
    std::string passwordError = validatePassword(password);
    if (!passwordError.empty()) {
        LOG_WARN("Invalid password: %s", passwordError.c_str());
        return false;
    }

    // Validate role
    if (role != "admin" && role != "user") {
        LOG_WARN("Invalid role: %s", role.c_str());
        return false;
    }

    // Check if username already exists
    if (getUser(username).has_value()) {
        LOG_WARN("Username already exists: %s", username.c_str());
        return false;
    }

    // Hash password
    std::string passwordHash = hashPassword(password);
    if (passwordHash.empty()) {
        LOG_ERROR("Failed to hash password");
        return false;
    }

    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    // Insert user
    const char* insertSql = "INSERT INTO users (username, password_hash, role, created_at) VALUES (?, ?, ?, ?)";

    auto stmt = m_db.prepare(insertSql);
    if (!stmt || !stmt->isValid()) {
        LOG_ERROR("Failed to prepare insert statement");
        return false;
    }

    stmt->bind(1, username);
    stmt->bind(2, passwordHash);
    stmt->bind(3, role);
    stmt->bind(4, static_cast<int64_t>(timestamp));

    if (!stmt->execute()) {
        LOG_ERROR("Failed to insert user");
        return false;
    }

    LOG_INFO("Created user: %s (role: %s)", username.c_str(), role.c_str());
    return true;
}

bool UserManager::deleteUser(const std::string& username) {
    const char* deleteSql = "DELETE FROM users WHERE username = ?";

    auto stmt = m_db.prepare(deleteSql);
    if (!stmt || !stmt->isValid()) {
        return false;
    }

    stmt->bind(1, username);

    if (!stmt->execute()) {
        return false;
    }

    LOG_INFO("Deleted user: %s", username.c_str());
    return true;
}

bool UserManager::changePassword(const std::string& username,
                                  const std::string& newPassword) {
    // Validate new password
    std::string passwordError = validatePassword(newPassword);
    if (!passwordError.empty()) {
        LOG_WARN("Invalid new password: %s", passwordError.c_str());
        return false;
    }

    // Hash new password
    std::string passwordHash = hashPassword(newPassword);
    if (passwordHash.empty()) {
        LOG_ERROR("Failed to hash new password");
        return false;
    }

    const char* updateSql = "UPDATE users SET password_hash = ? WHERE username = ?";

    auto stmt = m_db.prepare(updateSql);
    if (!stmt || !stmt->isValid()) {
        return false;
    }

    stmt->bind(1, passwordHash);
    stmt->bind(2, username);

    if (!stmt->execute()) {
        return false;
    }

    LOG_INFO("Changed password for user: %s", username.c_str());
    return true;
}

std::optional<User> UserManager::getUser(const std::string& username) {
    const char* selectSql =
        "SELECT id, username, password_hash, role, created_at, last_login, enabled "
        "FROM users WHERE username = ?";

    auto stmt = m_db.prepare(selectSql);
    if (!stmt || !stmt->isValid()) {
        return std::nullopt;
    }

    stmt->bind(1, username);

    if (!stmt->step()) {
        return std::nullopt;
    }

    User user;
    user.id = stmt->getInt(0);
    user.username = stmt->getString(1);
    user.passwordHash = stmt->getString(2);
    user.role = stmt->getString(3);
    user.createdAt = static_cast<uint64_t>(stmt->getInt64(4));
    user.lastLogin = static_cast<uint64_t>(stmt->getInt64(5));
    user.enabled = stmt->getInt(6) != 0;

    return user;
}

std::optional<User> UserManager::getUserById(int id) {
    const char* selectSql =
        "SELECT id, username, password_hash, role, created_at, last_login, enabled "
        "FROM users WHERE id = ?";

    auto stmt = m_db.prepare(selectSql);
    if (!stmt || !stmt->isValid()) {
        return std::nullopt;
    }

    stmt->bind(1, id);

    if (!stmt->step()) {
        return std::nullopt;
    }

    User user;
    user.id = stmt->getInt(0);
    user.username = stmt->getString(1);
    user.passwordHash = stmt->getString(2);
    user.role = stmt->getString(3);
    user.createdAt = static_cast<uint64_t>(stmt->getInt64(4));
    user.lastLogin = static_cast<uint64_t>(stmt->getInt64(5));
    user.enabled = stmt->getInt(6) != 0;

    return user;
}

std::vector<User> UserManager::getAllUsers() {
    std::vector<User> users;

    const char* selectSql =
        "SELECT id, username, password_hash, role, created_at, last_login, enabled "
        "FROM users ORDER BY id";

    auto stmt = m_db.prepare(selectSql);
    if (!stmt || !stmt->isValid()) {
        return users;
    }

    while (stmt->step()) {
        User user;
        user.id = stmt->getInt(0);
        user.username = stmt->getString(1);
        user.passwordHash = stmt->getString(2);
        user.role = stmt->getString(3);
        user.createdAt = static_cast<uint64_t>(stmt->getInt64(4));
        user.lastLogin = static_cast<uint64_t>(stmt->getInt64(5));
        user.enabled = stmt->getInt(6) != 0;
        users.push_back(user);
    }

    return users;
}

bool UserManager::authenticate(const std::string& username,
                                const std::string& password) {
    auto user = getUser(username);
    if (!user) {
        // User not found - still hash password to prevent timing attacks
        hashPassword(password);
        return false;
    }

    if (!user->enabled) {
        LOG_WARN("Authentication attempt for disabled user: %s", username.c_str());
        return false;
    }

    // Parse stored hash: salt$iterations$hash
    std::string stored = user->passwordHash;
    size_t pos1 = stored.find('$');
    size_t pos2 = stored.find('$', pos1 + 1);

    if (pos1 == std::string::npos || pos2 == std::string::npos) {
        LOG_ERROR("Invalid password hash format for user: %s", username.c_str());
        return false;
    }

    std::string salt = stored.substr(0, pos1);

    // Compute hash with same salt
    std::string computed = hashPassword(password, salt);

    // Constant-time comparison
    bool match = secureCompare(computed, stored);

    if (match) {
        updateLastLogin(username);
        LOG_DEBUG("User authenticated: %s", username.c_str());
    } else {
        LOG_WARN("Failed authentication for user: %s", username.c_str());
    }

    return match;
}

void UserManager::updateLastLogin(const std::string& username) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    const char* updateSql = "UPDATE users SET last_login = ? WHERE username = ?";

    auto stmt = m_db.prepare(updateSql);
    if (stmt && stmt->isValid()) {
        stmt->bind(1, static_cast<int64_t>(timestamp));
        stmt->bind(2, username);
        stmt->execute();
    }
}

bool UserManager::setUserEnabled(const std::string& username, bool enabled) {
    const char* updateSql = "UPDATE users SET enabled = ? WHERE username = ?";

    auto stmt = m_db.prepare(updateSql);
    if (!stmt || !stmt->isValid()) {
        return false;
    }

    stmt->bind(1, enabled ? 1 : 0);
    stmt->bind(2, username);

    if (!stmt->execute()) {
        return false;
    }

    LOG_INFO("User %s: %s", enabled ? "enabled" : "disabled", username.c_str());
    return true;
}

bool UserManager::changeRole(const std::string& username, const std::string& newRole) {
    if (newRole != "admin" && newRole != "user") {
        LOG_WARN("Invalid role: %s", newRole.c_str());
        return false;
    }

    const char* updateSql = "UPDATE users SET role = ? WHERE username = ?";

    auto stmt = m_db.prepare(updateSql);
    if (!stmt || !stmt->isValid()) {
        return false;
    }

    stmt->bind(1, newRole);
    stmt->bind(2, username);

    if (!stmt->execute()) {
        return false;
    }

    LOG_INFO("Changed role for user %s to %s", username.c_str(), newRole.c_str());
    return true;
}

bool UserManager::hasAdminUser() const {
    const char* selectSql = "SELECT COUNT(*) FROM users WHERE role = 'admin' AND enabled = 1";

    auto stmt = const_cast<Database&>(m_db).prepare(selectSql);
    if (!stmt || !stmt->isValid() || !stmt->step()) {
        return false;
    }

    return stmt->getInt(0) > 0;
}

int UserManager::userCount() const {
    const char* selectSql = "SELECT COUNT(*) FROM users";

    auto stmt = const_cast<Database&>(m_db).prepare(selectSql);
    if (!stmt || !stmt->isValid() || !stmt->step()) {
        return 0;
    }

    return stmt->getInt(0);
}

std::string UserManager::validatePassword(const std::string& password) {
    if (password.length() < 8) {
        return "Password must be at least 8 characters";
    }
    if (password.length() > 128) {
        return "Password must be at most 128 characters";
    }

    bool hasUpper = false, hasLower = false, hasDigit = false;
    for (char c : password) {
        if (std::isupper(c)) hasUpper = true;
        if (std::islower(c)) hasLower = true;
        if (std::isdigit(c)) hasDigit = true;
    }

    if (!hasUpper) {
        return "Password must contain at least one uppercase letter";
    }
    if (!hasLower) {
        return "Password must contain at least one lowercase letter";
    }
    if (!hasDigit) {
        return "Password must contain at least one digit";
    }

    return "";  // Valid
}

std::string UserManager::validateUsername(const std::string& username) {
    if (username.length() < 3) {
        return "Username must be at least 3 characters";
    }
    if (username.length() > 32) {
        return "Username must be at most 32 characters";
    }

    // Only allow alphanumeric, underscore, hyphen
    static const std::regex usernamePattern("^[a-zA-Z][a-zA-Z0-9_-]*$");
    if (!std::regex_match(username, usernamePattern)) {
        return "Username must start with a letter and contain only letters, numbers, underscore, or hyphen";
    }

    return "";  // Valid
}

std::string UserManager::hashPassword(const std::string& password,
                                       const std::string& salt) {
#if HAVE_OPENSSL
    std::string useSalt = salt;

    // Generate salt if not provided
    if (useSalt.empty()) {
        useSalt = generateSalt();
    }

    // Decode salt from base64
    std::vector<unsigned char> saltBytes = base64Decode(useSalt);
    if (saltBytes.empty()) {
        // If decoding fails, use salt as-is (shouldn't happen with generated salts)
        saltBytes.assign(useSalt.begin(), useSalt.end());
    }

    // Perform PBKDF2
    unsigned char hash[HASH_LENGTH];
    int result = PKCS5_PBKDF2_HMAC(
        password.c_str(),
        static_cast<int>(password.length()),
        saltBytes.data(),
        static_cast<int>(saltBytes.size()),
        PBKDF2_ITERATIONS,
        EVP_sha256(),
        HASH_LENGTH,
        hash
    );

    if (result != 1) {
        LOG_ERROR("PBKDF2 failed");
        return "";
    }

    // Encode hash as base64
    std::string hashBase64 = base64Encode(hash, HASH_LENGTH);

    // Return format: salt$iterations$hash
    return useSalt + "$" + std::to_string(PBKDF2_ITERATIONS) + "$" + hashBase64;

#else
    // Fallback: simple SHA-256 (not recommended for production)
    LOG_WARN("OpenSSL not available - using weak password hashing");
    return salt + "$1$" + password;  // Insecure fallback
#endif
}

std::string UserManager::generateSalt(size_t length) {
#if HAVE_OPENSSL
    std::vector<unsigned char> bytes(length);
    if (RAND_bytes(bytes.data(), static_cast<int>(length)) != 1) {
        LOG_ERROR("Failed to generate random salt");
        return "";
    }
    return base64Encode(bytes.data(), length);
#else
    // Fallback: use random() (not cryptographically secure)
    std::string result;
    for (size_t i = 0; i < length; i++) {
        result += static_cast<char>('a' + (random() % 26));
    }
    return result;
#endif
}

bool UserManager::secureCompare(const std::string& a, const std::string& b) {
    if (a.length() != b.length()) {
        return false;
    }

    volatile unsigned char result = 0;
    for (size_t i = 0; i < a.length(); i++) {
        result |= static_cast<unsigned char>(a[i]) ^
                  static_cast<unsigned char>(b[i]);
    }

    return result == 0;
}

} // namespace smarthub::security
