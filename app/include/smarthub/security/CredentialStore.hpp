/**
 * SmartHub Credential Store
 *
 * Securely stores sensitive credentials like API keys, device secrets, etc.
 * Uses AES-256-GCM encryption with a master key derived from a passphrase.
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
 * Credential entry
 */
struct Credential {
    int id = 0;
    std::string name;       // Unique identifier (e.g., "tuya.device1.local_key")
    std::string value;      // The secret value (decrypted)
    std::string category;   // Category (e.g., "tuya", "mqtt", "zigbee")
    uint64_t createdAt = 0;
    uint64_t updatedAt = 0;
};

/**
 * Secure Credential Store
 *
 * Stores credentials encrypted with AES-256-GCM.
 * The encryption key is derived from a master password using PBKDF2.
 */
class CredentialStore {
public:
    /**
     * Constructor
     * @param db Database for credential storage
     */
    explicit CredentialStore(Database& db);

    ~CredentialStore() = default;

    // Non-copyable
    CredentialStore(const CredentialStore&) = delete;
    CredentialStore& operator=(const CredentialStore&) = delete;

    /**
     * Initialize credential tables
     * @return true if successful
     */
    bool initialize();

    /**
     * Set the master encryption key
     * Must be called before any get/set operations.
     * @param passphrase The master passphrase
     * @return true if key was set successfully
     */
    bool unlock(const std::string& passphrase);

    /**
     * Check if store is unlocked
     */
    bool isUnlocked() const { return m_unlocked; }

    /**
     * Lock the store (clear key from memory)
     */
    void lock();

    /**
     * Store a credential
     * @param name Unique name for the credential
     * @param value The secret value to store
     * @param category Optional category
     * @return true if stored successfully
     */
    bool set(const std::string& name,
             const std::string& value,
             const std::string& category = "");

    /**
     * Retrieve a credential
     * @param name The credential name
     * @return The decrypted value, or nullopt if not found
     */
    std::optional<std::string> get(const std::string& name);

    /**
     * Delete a credential
     * @param name The credential name
     * @return true if deleted
     */
    bool remove(const std::string& name);

    /**
     * Check if a credential exists
     * @param name The credential name
     * @return true if exists
     */
    bool exists(const std::string& name);

    /**
     * List all credentials (values hidden)
     * @param category Optional category filter
     * @return List of credential metadata
     */
    std::vector<Credential> list(const std::string& category = "");

    /**
     * Count credentials
     * @param category Optional category filter
     */
    int count(const std::string& category = "");

    /**
     * Delete all credentials in a category
     * @param category The category
     * @return Number deleted
     */
    int clearCategory(const std::string& category);

    /**
     * Change master passphrase
     * Re-encrypts all credentials with new key.
     * @param oldPassphrase Current passphrase
     * @param newPassphrase New passphrase
     * @return true if changed successfully
     */
    bool changePassphrase(const std::string& oldPassphrase,
                          const std::string& newPassphrase);

private:
    /**
     * Derive encryption key from passphrase
     */
    bool deriveKey(const std::string& passphrase);

    /**
     * Encrypt a value
     * @return Base64-encoded ciphertext with IV and tag
     */
    std::string encrypt(const std::string& plaintext);

    /**
     * Decrypt a value
     * @return Decrypted plaintext, or empty on failure
     */
    std::string decrypt(const std::string& ciphertext);

    /**
     * Get or create the salt for key derivation
     */
    std::string getSalt();

    Database& m_db;
    bool m_unlocked = false;

    // Encryption key (32 bytes for AES-256)
    std::vector<uint8_t> m_key;

    // Key derivation parameters
    static constexpr int PBKDF2_ITERATIONS = 100000;
    static constexpr int KEY_LENGTH = 32;  // AES-256
    static constexpr int SALT_LENGTH = 16;
    static constexpr int IV_LENGTH = 12;   // GCM recommended
    static constexpr int TAG_LENGTH = 16;  // GCM auth tag
};

} // namespace smarthub::security
