/**
 * SmartHub Credential Store Implementation
 */

#include <smarthub/security/CredentialStore.hpp>
#include <smarthub/database/Database.hpp>
#include <smarthub/core/Logger.hpp>

#include <chrono>
#include <sstream>
#include <iomanip>
#include <cstring>

// OpenSSL for encryption
#if __has_include(<openssl/evp.h>)
#include <openssl/evp.h>
#include <openssl/rand.h>
#define HAVE_OPENSSL 1
#else
#define HAVE_OPENSSL 0
#endif

namespace smarthub::security {

// Base64 encoding/decoding helpers
static std::string base64Encode(const uint8_t* data, size_t len) {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((len + 2) / 3) * 4);

    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < len) n |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (i + 2 < len) n |= static_cast<uint32_t>(data[i + 2]);

        result += chars[(n >> 18) & 0x3F];
        result += chars[(n >> 12) & 0x3F];
        result += (i + 1 < len) ? chars[(n >> 6) & 0x3F] : '=';
        result += (i + 2 < len) ? chars[n & 0x3F] : '=';
    }

    return result;
}

static std::vector<uint8_t> base64Decode(const std::string& encoded) {
    static const int decodeTable[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };

    std::vector<uint8_t> result;
    result.reserve((encoded.size() * 3) / 4);

    int val = 0, bits = -8;
    for (char c : encoded) {
        if (c == '=') break;
        int d = decodeTable[static_cast<uint8_t>(c)];
        if (d < 0) continue;
        val = (val << 6) | d;
        bits += 6;
        if (bits >= 0) {
            result.push_back(static_cast<uint8_t>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }

    return result;
}

CredentialStore::CredentialStore(Database& db)
    : m_db(db)
{
}

bool CredentialStore::initialize() {
#if !HAVE_OPENSSL
    LOG_WARN("OpenSSL not available - credentials will be stored unencrypted!");
#endif

    // Create credentials table
    const char* createTableSql = R"(
        CREATE TABLE IF NOT EXISTS credentials (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            value TEXT NOT NULL,
            category TEXT DEFAULT '',
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_credentials_name ON credentials(name);
        CREATE INDEX IF NOT EXISTS idx_credentials_category ON credentials(category);
    )";

    if (!m_db.execute(createTableSql)) {
        LOG_ERROR("Failed to create credentials table");
        return false;
    }

    // Create settings table for salt
    const char* createSettingsSql = R"(
        CREATE TABLE IF NOT EXISTS credential_settings (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
    )";

    if (!m_db.execute(createSettingsSql)) {
        LOG_ERROR("Failed to create credential_settings table");
        return false;
    }

    LOG_INFO("Credential store initialized");
    return true;
}

bool CredentialStore::unlock(const std::string& passphrase) {
    if (passphrase.empty()) {
        LOG_ERROR("Passphrase cannot be empty");
        return false;
    }

    if (!deriveKey(passphrase)) {
        return false;
    }

    m_unlocked = true;
    LOG_INFO("Credential store unlocked");
    return true;
}

void CredentialStore::lock() {
    // Securely clear the key
    if (!m_key.empty()) {
        std::memset(m_key.data(), 0, m_key.size());
        m_key.clear();
    }
    m_unlocked = false;
    LOG_INFO("Credential store locked");
}

bool CredentialStore::set(const std::string& name,
                          const std::string& value,
                          const std::string& category) {
    if (!m_unlocked) {
        LOG_ERROR("Credential store is locked");
        return false;
    }

    if (name.empty()) {
        LOG_ERROR("Credential name cannot be empty");
        return false;
    }

    // Encrypt the value (even empty values are encrypted to produce ciphertext)
    std::string encrypted = encrypt(value);
    // Encryption always produces output (IV + tag at minimum for empty input)
    if (encrypted.empty()) {
        LOG_ERROR("Failed to encrypt credential");
        return false;
    }

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    // Check if exists
    if (exists(name)) {
        // Update
        const char* updateSql =
            "UPDATE credentials SET value = ?, category = ?, updated_at = ? WHERE name = ?";

        auto stmt = m_db.prepare(updateSql);
        if (!stmt || !stmt->isValid()) {
            return false;
        }

        stmt->bind(1, encrypted);
        stmt->bind(2, category);
        stmt->bind(3, timestamp);
        stmt->bind(4, name);

        return stmt->execute();
    } else {
        // Insert
        const char* insertSql =
            "INSERT INTO credentials (name, value, category, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?)";

        auto stmt = m_db.prepare(insertSql);
        if (!stmt || !stmt->isValid()) {
            return false;
        }

        stmt->bind(1, name);
        stmt->bind(2, encrypted);
        stmt->bind(3, category);
        stmt->bind(4, timestamp);
        stmt->bind(5, timestamp);

        return stmt->execute();
    }
}

std::optional<std::string> CredentialStore::get(const std::string& name) {
    if (!m_unlocked) {
        LOG_ERROR("Credential store is locked");
        return std::nullopt;
    }

    const char* selectSql = "SELECT value FROM credentials WHERE name = ?";

    auto stmt = m_db.prepare(selectSql);
    if (!stmt || !stmt->isValid()) {
        return std::nullopt;
    }

    stmt->bind(1, name);

    if (!stmt->step()) {
        return std::nullopt;
    }

    std::string encrypted = stmt->getString(0);

    // Empty encrypted value means the original was empty (shouldn't happen with proper encryption)
    if (encrypted.empty()) {
        return "";
    }

    std::string decrypted = decrypt(encrypted);

    // For encrypted data, decrypt returns empty only on decryption failure
    // (The minimum encrypted data is IV_LENGTH + TAG_LENGTH, so it will always have some data)
    // But we can't distinguish between empty input and decryption failure this way,
    // so we use a special marker: if input was truly empty, the ciphertext would still
    // be IV + TAG = 28 bytes = ~40 base64 chars. If decrypt fails, it returns empty.
    // We need to check the length of encrypted to know if this was intentional.

    // If decryption returned empty but encrypted has content > minimum (IV+TAG in base64),
    // then decryption failed
    constexpr size_t minEncryptedLen = ((IV_LENGTH + TAG_LENGTH + 2) / 3) * 4;  // ~40 chars
    if (decrypted.empty() && encrypted.length() > minEncryptedLen) {
        LOG_ERROR("Failed to decrypt credential: %s", name.c_str());
        return std::nullopt;
    }

    return decrypted;
}

bool CredentialStore::remove(const std::string& name) {
    const char* deleteSql = "DELETE FROM credentials WHERE name = ?";

    auto stmt = m_db.prepare(deleteSql);
    if (!stmt || !stmt->isValid()) {
        return false;
    }

    stmt->bind(1, name);
    return stmt->execute();
}

bool CredentialStore::exists(const std::string& name) {
    const char* selectSql = "SELECT 1 FROM credentials WHERE name = ?";

    auto stmt = m_db.prepare(selectSql);
    if (!stmt || !stmt->isValid()) {
        return false;
    }

    stmt->bind(1, name);
    return stmt->step();
}

std::vector<Credential> CredentialStore::list(const std::string& category) {
    std::vector<Credential> credentials;

    std::string sql = "SELECT id, name, category, created_at, updated_at FROM credentials";
    if (!category.empty()) {
        sql += " WHERE category = ?";
    }
    sql += " ORDER BY name";

    auto stmt = m_db.prepare(sql.c_str());
    if (!stmt || !stmt->isValid()) {
        return credentials;
    }

    if (!category.empty()) {
        stmt->bind(1, category);
    }

    while (stmt->step()) {
        Credential cred;
        cred.id = stmt->getInt(0);
        cred.name = stmt->getString(1);
        // Value is intentionally not included
        cred.category = stmt->getString(2);
        cred.createdAt = static_cast<uint64_t>(stmt->getInt64(3));
        cred.updatedAt = static_cast<uint64_t>(stmt->getInt64(4));
        credentials.push_back(cred);
    }

    return credentials;
}

int CredentialStore::count(const std::string& category) {
    std::string sql = "SELECT COUNT(*) FROM credentials";
    if (!category.empty()) {
        sql += " WHERE category = ?";
    }

    auto stmt = m_db.prepare(sql.c_str());
    if (!stmt || !stmt->isValid()) {
        return 0;
    }

    if (!category.empty()) {
        stmt->bind(1, category);
    }

    if (!stmt->step()) {
        return 0;
    }

    return stmt->getInt(0);
}

int CredentialStore::clearCategory(const std::string& category) {
    int countBefore = count(category);

    const char* deleteSql = "DELETE FROM credentials WHERE category = ?";

    auto stmt = m_db.prepare(deleteSql);
    if (!stmt || !stmt->isValid()) {
        return 0;
    }

    stmt->bind(1, category);

    if (!stmt->execute()) {
        return 0;
    }

    return countBefore;
}

bool CredentialStore::changePassphrase(const std::string& oldPassphrase,
                                        const std::string& newPassphrase) {
    // First, unlock with old passphrase
    if (!unlock(oldPassphrase)) {
        LOG_ERROR("Failed to verify old passphrase");
        return false;
    }

    // Get all credentials decrypted
    std::vector<std::pair<std::string, std::string>> allCredentials;
    auto credentials = list();

    for (const auto& cred : credentials) {
        auto value = get(cred.name);
        if (value) {
            allCredentials.emplace_back(cred.name, *value);
        }
    }

    // Generate new salt and derive new key
    // Delete old salt
    m_db.execute("DELETE FROM credential_settings WHERE key = 'salt'");

    // Lock and unlock with new passphrase (generates new salt)
    lock();
    if (!unlock(newPassphrase)) {
        LOG_ERROR("Failed to set new passphrase");
        return false;
    }

    // Re-encrypt all credentials with new key
    for (const auto& [name, value] : allCredentials) {
        // Get category
        std::string category;
        for (const auto& cred : credentials) {
            if (cred.name == name) {
                category = cred.category;
                break;
            }
        }

        if (!set(name, value, category)) {
            LOG_ERROR("Failed to re-encrypt credential: %s", name.c_str());
            return false;
        }
    }

    LOG_INFO("Credential store passphrase changed");
    return true;
}

bool CredentialStore::deriveKey(const std::string& passphrase) {
#if HAVE_OPENSSL
    std::string saltStr = getSalt();
    std::vector<uint8_t> salt = base64Decode(saltStr);

    if (salt.size() != SALT_LENGTH) {
        LOG_ERROR("Invalid salt length");
        return false;
    }

    m_key.resize(KEY_LENGTH);

    int result = PKCS5_PBKDF2_HMAC(
        passphrase.c_str(), static_cast<int>(passphrase.length()),
        salt.data(), static_cast<int>(salt.size()),
        PBKDF2_ITERATIONS,
        EVP_sha256(),
        KEY_LENGTH, m_key.data()
    );

    if (result != 1) {
        LOG_ERROR("Failed to derive encryption key");
        m_key.clear();
        return false;
    }

    return true;
#else
    // Fallback - use passphrase directly (NOT SECURE)
    m_key.assign(passphrase.begin(), passphrase.end());
    m_key.resize(KEY_LENGTH, 0);
    return true;
#endif
}

std::string CredentialStore::encrypt(const std::string& plaintext) {
#if HAVE_OPENSSL
    if (m_key.size() != KEY_LENGTH) {
        return "";
    }

    // Generate random IV
    std::vector<uint8_t> iv(IV_LENGTH);
    if (RAND_bytes(iv.data(), IV_LENGTH) != 1) {
        LOG_ERROR("Failed to generate IV");
        return "";
    }

    // Create cipher context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return "";
    }

    std::vector<uint8_t> ciphertext(plaintext.size() + 16);  // Allow for padding
    std::vector<uint8_t> tag(TAG_LENGTH);
    int len = 0, ciphertextLen = 0;

    bool success = false;
    do {
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, m_key.data(), iv.data()) != 1) {
            break;
        }

        if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
                              reinterpret_cast<const uint8_t*>(plaintext.data()),
                              static_cast<int>(plaintext.size())) != 1) {
            break;
        }
        ciphertextLen = len;

        if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
            break;
        }
        ciphertextLen += len;

        // Get the tag
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_LENGTH, tag.data()) != 1) {
            break;
        }

        success = true;
    } while (false);

    EVP_CIPHER_CTX_free(ctx);

    if (!success) {
        return "";
    }

    // Combine: IV + ciphertext + tag
    std::vector<uint8_t> combined;
    combined.reserve(IV_LENGTH + ciphertextLen + TAG_LENGTH);
    combined.insert(combined.end(), iv.begin(), iv.end());
    combined.insert(combined.end(), ciphertext.begin(), ciphertext.begin() + ciphertextLen);
    combined.insert(combined.end(), tag.begin(), tag.end());

    return base64Encode(combined.data(), combined.size());
#else
    // Fallback - no encryption (NOT SECURE)
    return base64Encode(reinterpret_cast<const uint8_t*>(plaintext.data()), plaintext.size());
#endif
}

std::string CredentialStore::decrypt(const std::string& ciphertext) {
#if HAVE_OPENSSL
    if (m_key.size() != KEY_LENGTH) {
        return "";
    }

    std::vector<uint8_t> combined = base64Decode(ciphertext);

    if (combined.size() < IV_LENGTH + TAG_LENGTH) {
        return "";
    }

    // Extract IV, ciphertext, and tag
    std::vector<uint8_t> iv(combined.begin(), combined.begin() + IV_LENGTH);
    std::vector<uint8_t> tag(combined.end() - TAG_LENGTH, combined.end());
    std::vector<uint8_t> encryptedData(combined.begin() + IV_LENGTH,
                                        combined.end() - TAG_LENGTH);

    // Create cipher context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return "";
    }

    std::vector<uint8_t> plaintext(encryptedData.size());
    int len = 0, plaintextLen = 0;

    bool success = false;
    do {
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, m_key.data(), iv.data()) != 1) {
            break;
        }

        if (EVP_DecryptUpdate(ctx, plaintext.data(), &len,
                              encryptedData.data(),
                              static_cast<int>(encryptedData.size())) != 1) {
            break;
        }
        plaintextLen = len;

        // Set the tag
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_LENGTH,
                                const_cast<uint8_t*>(tag.data())) != 1) {
            break;
        }

        // Verify and finalize
        if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
            // Authentication failed
            break;
        }
        plaintextLen += len;

        success = true;
    } while (false);

    EVP_CIPHER_CTX_free(ctx);

    if (!success) {
        return "";
    }

    return std::string(reinterpret_cast<char*>(plaintext.data()), plaintextLen);
#else
    // Fallback - no decryption
    std::vector<uint8_t> decoded = base64Decode(ciphertext);
    return std::string(decoded.begin(), decoded.end());
#endif
}

std::string CredentialStore::getSalt() {
    // Check if salt exists
    const char* selectSql = "SELECT value FROM credential_settings WHERE key = 'salt'";

    auto stmt = m_db.prepare(selectSql);
    if (stmt && stmt->isValid() && stmt->step()) {
        return stmt->getString(0);
    }

    // Generate new salt
#if HAVE_OPENSSL
    std::vector<uint8_t> salt(SALT_LENGTH);
    if (RAND_bytes(salt.data(), SALT_LENGTH) != 1) {
        LOG_ERROR("Failed to generate salt");
        return "";
    }
    std::string saltStr = base64Encode(salt.data(), salt.size());
#else
    // Fallback - weak salt
    std::string saltStr = "SmartHubDefaultSalt!";
#endif

    // Store salt
    const char* insertSql =
        "INSERT INTO credential_settings (key, value) VALUES ('salt', ?)";

    auto insertStmt = m_db.prepare(insertSql);
    if (insertStmt && insertStmt->isValid()) {
        insertStmt->bind(1, saltStr);
        insertStmt->execute();
    }

    return saltStr;
}

} // namespace smarthub::security
