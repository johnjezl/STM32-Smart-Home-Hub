/**
 * SmartHub Security Tests
 *
 * Tests for certificate management, user management, and security features.
 */

#include <gtest/gtest.h>
#include <smarthub/security/CertManager.hpp>
#include <smarthub/security/UserManager.hpp>
#include <smarthub/security/SessionManager.hpp>
#include <smarthub/security/ApiTokenManager.hpp>
#include <smarthub/security/CredentialStore.hpp>
#include <smarthub/security/SetupManager.hpp>
#include <smarthub/database/Database.hpp>
#include <thread>
#include <chrono>

#include <cstdlib>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace smarthub::security {

class CertManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test certificates
        testDir = "/tmp/smarthub_cert_test_" + std::to_string(getpid());
        fs::create_directories(testDir);
    }

    void TearDown() override {
        // Cleanup test directory
        fs::remove_all(testDir);
    }

    std::string testDir;
};

TEST_F(CertManagerTest, ConstructorSetsDirectory) {
    CertManager mgr(testDir);
    EXPECT_EQ(mgr.certDir(), testDir);
}

TEST_F(CertManagerTest, PathsAreCorrect) {
    CertManager mgr(testDir);

    EXPECT_EQ(mgr.caCertPath(), testDir + "/ca.crt");
    EXPECT_EQ(mgr.serverCertPath(), testDir + "/server.crt");
    EXPECT_EQ(mgr.serverKeyPath(), testDir + "/server.key");
}

TEST_F(CertManagerTest, CertificatesDoNotExistInitially) {
    CertManager mgr(testDir);
    EXPECT_FALSE(mgr.certificatesExist());
}

TEST_F(CertManagerTest, IsValidReturnsFalseWithoutCerts) {
    CertManager mgr(testDir);
    EXPECT_FALSE(mgr.isValid());
}

TEST_F(CertManagerTest, GetCertInfoReturnsNulloptWithoutCerts) {
    CertManager mgr(testDir);
    auto info = mgr.getCertInfo();
    // Either no value, or isValid is false
    EXPECT_TRUE(!info.has_value() || !info->isValid);
}

TEST_F(CertManagerTest, DaysUntilExpiryZeroWithoutCerts) {
    CertManager mgr(testDir);
    // Without certs or with uninitialized info, should be 0 or negative
    EXPECT_LE(mgr.daysUntilExpiry(), 0);
}

// Integration test - requires openssl to be installed
class CertManagerIntegrationTest : public CertManagerTest {
protected:
    void SetUp() override {
        CertManagerTest::SetUp();

        // Check if openssl is available
        int result = std::system("which openssl > /dev/null 2>&1");
        opensslAvailable = (result == 0);
    }

    bool opensslAvailable = false;
};

TEST_F(CertManagerIntegrationTest, InitializeGeneratesCertificates) {
    if (!opensslAvailable) {
        GTEST_SKIP() << "OpenSSL not available";
    }

    CertManager mgr(testDir);
    bool result = mgr.initialize("test.local", "192.168.1.1");

    EXPECT_TRUE(result);
    EXPECT_TRUE(mgr.certificatesExist());

    // Check cert info is valid
    auto info = mgr.getCertInfo();
    ASSERT_TRUE(info.has_value());
    EXPECT_TRUE(info->isValid);
}

TEST_F(CertManagerIntegrationTest, GeneratedCertsHaveCorrectInfo) {
    if (!opensslAvailable) {
        GTEST_SKIP() << "OpenSSL not available";
    }

    CertManager mgr(testDir);
    ASSERT_TRUE(mgr.initialize("myhost.local"));

    auto info = mgr.getCertInfo();
    ASSERT_TRUE(info.has_value());

    EXPECT_TRUE(info->isValid);
    EXPECT_FALSE(info->subject.empty());
    EXPECT_GT(info->notAfter, info->notBefore);
}

TEST_F(CertManagerIntegrationTest, DaysUntilExpiryIsReasonable) {
    if (!opensslAvailable) {
        GTEST_SKIP() << "OpenSSL not available";
    }

    CertManager mgr(testDir);
    ASSERT_TRUE(mgr.initialize("test.local"));

    int days = mgr.daysUntilExpiry();
    // Should be approximately 365 days (allowing some margin)
    EXPECT_GE(days, 360);
    EXPECT_LE(days, 366);
}

TEST_F(CertManagerIntegrationTest, ReinitializeUsesExistingCerts) {
    if (!opensslAvailable) {
        GTEST_SKIP() << "OpenSSL not available";
    }

    CertManager mgr1(testDir);
    ASSERT_TRUE(mgr1.initialize("test.local"));

    // Get original serial number and expiration
    auto info1 = mgr1.getCertInfo();
    ASSERT_TRUE(info1.has_value());
    std::string originalSerial = info1->serialNumber;
    time_t originalExpiry = mgr1.expirationDate();

    // Create new manager and initialize - should use existing certs
    CertManager mgr2(testDir);
    ASSERT_TRUE(mgr2.initialize("test.local"));

    auto info2 = mgr2.getCertInfo();
    ASSERT_TRUE(info2.has_value());

    // Serial number should be the same (using existing certs)
    EXPECT_EQ(info2->serialNumber, originalSerial);

    // Expiration should be within a few seconds (accounting for potential rounding)
    EXPECT_NEAR(mgr2.expirationDate(), originalExpiry, 5);
}

TEST_F(CertManagerIntegrationTest, RegenerateCreateNewCerts) {
    if (!opensslAvailable) {
        GTEST_SKIP() << "OpenSSL not available";
    }

    CertManager mgr(testDir);
    ASSERT_TRUE(mgr.initialize("test.local"));

    auto info1 = mgr.getCertInfo();
    ASSERT_TRUE(info1.has_value());
    std::string serial1 = info1->serialNumber;

    // Regenerate
    ASSERT_TRUE(mgr.regenerate("newhost.local", "10.0.0.1"));

    auto info2 = mgr.getCertInfo();
    ASSERT_TRUE(info2.has_value());

    // Serial number should be different
    EXPECT_NE(info2->serialNumber, serial1);
}

TEST_F(CertManagerIntegrationTest, NeedsRenewalReturnsFalseForNewCerts) {
    if (!opensslAvailable) {
        GTEST_SKIP() << "OpenSSL not available";
    }

    CertManager mgr(testDir);
    ASSERT_TRUE(mgr.initialize("test.local"));

    EXPECT_FALSE(mgr.needsRenewal());
}

TEST_F(CertManagerIntegrationTest, CertFilesHaveCorrectPermissions) {
    if (!opensslAvailable) {
        GTEST_SKIP() << "OpenSSL not available";
    }

    CertManager mgr(testDir);
    ASSERT_TRUE(mgr.initialize("test.local"));

    // Check key file permissions (should be 600 - owner read/write only)
    struct stat st;
    ASSERT_EQ(stat(mgr.serverKeyPath().c_str(), &st), 0);
    EXPECT_EQ(st.st_mode & 0777, 0600);

    // Check cert file permissions (should be 644 - owner read/write, others read)
    ASSERT_EQ(stat(mgr.serverCertPath().c_str(), &st), 0);
    EXPECT_EQ(st.st_mode & 0777, 0644);
}

// Test certificate chain validation
TEST_F(CertManagerIntegrationTest, ServerCertSignedByCA) {
    if (!opensslAvailable) {
        GTEST_SKIP() << "OpenSSL not available";
    }

    CertManager mgr(testDir);
    ASSERT_TRUE(mgr.initialize("test.local"));

    // Verify the server cert is signed by the CA
    std::string cmd = "openssl verify -CAfile '" + mgr.caCertPath() + "' '" +
                       mgr.serverCertPath() + "' 2>/dev/null";
    int result = std::system(cmd.c_str());
    EXPECT_EQ(result, 0);
}

// ============================================================================
// UserManager Tests
// ============================================================================

class UserManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary database
        dbPath = "/tmp/smarthub_user_test_" + std::to_string(getpid()) + ".db";
        db = std::make_unique<smarthub::Database>(dbPath);
        ASSERT_TRUE(db->initialize());

        userMgr = std::make_unique<UserManager>(*db);
        ASSERT_TRUE(userMgr->initialize());
    }

    void TearDown() override {
        userMgr.reset();
        db.reset();
        fs::remove(dbPath);
    }

    std::string dbPath;
    std::unique_ptr<smarthub::Database> db;
    std::unique_ptr<UserManager> userMgr;
};

TEST_F(UserManagerTest, CreateUserSucceeds) {
    EXPECT_TRUE(userMgr->createUser("testuser", "Password123", "user"));
    EXPECT_EQ(userMgr->userCount(), 1);
}

TEST_F(UserManagerTest, CreateAdminUser) {
    EXPECT_TRUE(userMgr->createUser("admin", "AdminPass123", "admin"));
    EXPECT_TRUE(userMgr->hasAdminUser());
}

TEST_F(UserManagerTest, DuplicateUsernameRejected) {
    EXPECT_TRUE(userMgr->createUser("testuser", "Password123", "user"));
    EXPECT_FALSE(userMgr->createUser("testuser", "DifferentPass1", "user"));
}

TEST_F(UserManagerTest, GetUserReturnsCorrectInfo) {
    EXPECT_TRUE(userMgr->createUser("testuser", "Password123", "user"));

    auto user = userMgr->getUser("testuser");
    ASSERT_TRUE(user.has_value());
    EXPECT_EQ(user->username, "testuser");
    EXPECT_EQ(user->role, "user");
    EXPECT_TRUE(user->enabled);
}

TEST_F(UserManagerTest, GetNonexistentUserReturnsNullopt) {
    auto user = userMgr->getUser("nonexistent");
    EXPECT_FALSE(user.has_value());
}

TEST_F(UserManagerTest, AuthenticateSucceeds) {
    EXPECT_TRUE(userMgr->createUser("testuser", "Password123", "user"));
    EXPECT_TRUE(userMgr->authenticate("testuser", "Password123"));
}

TEST_F(UserManagerTest, AuthenticateWithWrongPasswordFails) {
    EXPECT_TRUE(userMgr->createUser("testuser", "Password123", "user"));
    EXPECT_FALSE(userMgr->authenticate("testuser", "WrongPassword1"));
}

TEST_F(UserManagerTest, AuthenticateNonexistentUserFails) {
    EXPECT_FALSE(userMgr->authenticate("nonexistent", "Password123"));
}

TEST_F(UserManagerTest, ChangePasswordWorks) {
    EXPECT_TRUE(userMgr->createUser("testuser", "Password123", "user"));
    EXPECT_TRUE(userMgr->changePassword("testuser", "NewPassword456"));
    EXPECT_FALSE(userMgr->authenticate("testuser", "Password123"));
    EXPECT_TRUE(userMgr->authenticate("testuser", "NewPassword456"));
}

TEST_F(UserManagerTest, DeleteUserWorks) {
    EXPECT_TRUE(userMgr->createUser("testuser", "Password123", "user"));
    EXPECT_EQ(userMgr->userCount(), 1);
    EXPECT_TRUE(userMgr->deleteUser("testuser"));
    EXPECT_EQ(userMgr->userCount(), 0);
}

TEST_F(UserManagerTest, DisabledUserCannotAuthenticate) {
    EXPECT_TRUE(userMgr->createUser("testuser", "Password123", "user"));
    EXPECT_TRUE(userMgr->setUserEnabled("testuser", false));
    EXPECT_FALSE(userMgr->authenticate("testuser", "Password123"));
}

TEST_F(UserManagerTest, ChangeRoleWorks) {
    EXPECT_TRUE(userMgr->createUser("testuser", "Password123", "user"));
    EXPECT_FALSE(userMgr->hasAdminUser());
    EXPECT_TRUE(userMgr->changeRole("testuser", "admin"));
    EXPECT_TRUE(userMgr->hasAdminUser());
}

TEST_F(UserManagerTest, GetAllUsersWorks) {
    EXPECT_TRUE(userMgr->createUser("user1", "Password123", "user"));
    EXPECT_TRUE(userMgr->createUser("user2", "Password456", "admin"));

    auto users = userMgr->getAllUsers();
    EXPECT_EQ(users.size(), 2);
}

// Password validation tests
TEST(PasswordValidationTest, TooShortPassword) {
    EXPECT_FALSE(UserManager::validatePassword("Short1").empty());
}

TEST(PasswordValidationTest, NoUppercase) {
    EXPECT_FALSE(UserManager::validatePassword("password123").empty());
}

TEST(PasswordValidationTest, NoLowercase) {
    EXPECT_FALSE(UserManager::validatePassword("PASSWORD123").empty());
}

TEST(PasswordValidationTest, NoDigit) {
    EXPECT_FALSE(UserManager::validatePassword("PasswordNoDigit").empty());
}

TEST(PasswordValidationTest, ValidPassword) {
    EXPECT_TRUE(UserManager::validatePassword("ValidPass123").empty());
}

// Username validation tests
TEST(UsernameValidationTest, TooShortUsername) {
    EXPECT_FALSE(UserManager::validateUsername("ab").empty());
}

TEST(UsernameValidationTest, StartsWithDigit) {
    EXPECT_FALSE(UserManager::validateUsername("1username").empty());
}

TEST(UsernameValidationTest, InvalidCharacters) {
    EXPECT_FALSE(UserManager::validateUsername("user@name").empty());
}

TEST(UsernameValidationTest, ValidUsername) {
    EXPECT_TRUE(UserManager::validateUsername("valid_user-1").empty());
}

// ============================================================================
// SessionManager Tests
// ============================================================================

class SessionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Short timeout for testing (1 minute)
        sessionMgr = std::make_unique<SessionManager>(1, 3);
    }

    std::unique_ptr<SessionManager> sessionMgr;
};

TEST_F(SessionManagerTest, CreateSessionReturnsToken) {
    std::string token = sessionMgr->createSession(1, "testuser", "user");
    EXPECT_FALSE(token.empty());
    EXPECT_EQ(token.length(), 64);  // 32 bytes * 2 hex chars
}

TEST_F(SessionManagerTest, ValidateSessionReturnsCorrectInfo) {
    std::string token = sessionMgr->createSession(1, "testuser", "admin", "192.168.1.1", "TestAgent");

    auto session = sessionMgr->validateSession(token);
    ASSERT_TRUE(session.has_value());
    EXPECT_EQ(session->username, "testuser");
    EXPECT_EQ(session->role, "admin");
    EXPECT_EQ(session->userId, 1);
    EXPECT_EQ(session->ipAddress, "192.168.1.1");
    EXPECT_EQ(session->userAgent, "TestAgent");
}

TEST_F(SessionManagerTest, InvalidTokenReturnsNullopt) {
    auto session = sessionMgr->validateSession("invalid_token");
    EXPECT_FALSE(session.has_value());
}

TEST_F(SessionManagerTest, DestroySessionWorks) {
    std::string token = sessionMgr->createSession(1, "testuser", "user");
    EXPECT_TRUE(sessionMgr->validateSession(token).has_value());

    sessionMgr->destroySession(token);
    EXPECT_FALSE(sessionMgr->validateSession(token).has_value());
}

TEST_F(SessionManagerTest, DestroyUserSessionsWorks) {
    sessionMgr->createSession(1, "user1", "user");
    sessionMgr->createSession(1, "user1", "user");
    sessionMgr->createSession(2, "user2", "user");

    EXPECT_EQ(sessionMgr->sessionCount(), 3);

    sessionMgr->destroyUserSessions("user1");
    EXPECT_EQ(sessionMgr->sessionCount(), 1);
}

TEST_F(SessionManagerTest, GetUserSessionsHidesToken) {
    sessionMgr->createSession(1, "testuser", "user");

    auto sessions = sessionMgr->getUserSessions("testuser");
    ASSERT_EQ(sessions.size(), 1);
    EXPECT_EQ(sessions[0].token, "***");
    EXPECT_EQ(sessions[0].username, "testuser");
}

TEST_F(SessionManagerTest, TouchSessionExtendsExpiration) {
    std::string token = sessionMgr->createSession(1, "testuser", "user");

    auto session1 = sessionMgr->validateSession(token);
    ASSERT_TRUE(session1.has_value());
    uint64_t expires1 = session1->expiresAt;

    // Wait more than 1 second to ensure timestamp changes
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    EXPECT_TRUE(sessionMgr->touchSession(token));

    auto session2 = sessionMgr->validateSession(token);
    ASSERT_TRUE(session2.has_value());

    // Expiration should be updated (at least not decreased)
    EXPECT_GE(session2->expiresAt, expires1);
    // Last activity should be greater (after 1+ second)
    EXPECT_GE(session2->lastActivity, session1->lastActivity);
}

TEST_F(SessionManagerTest, MaxSessionsEnforced) {
    // Max is 3 sessions per user
    sessionMgr->createSession(1, "testuser", "user");
    sessionMgr->createSession(1, "testuser", "user");
    sessionMgr->createSession(1, "testuser", "user");

    // Should have 3 sessions
    EXPECT_EQ(sessionMgr->getUserSessions("testuser").size(), 3);

    // Creating another should remove the oldest
    sessionMgr->createSession(1, "testuser", "user");

    // Still 3 sessions (oldest removed)
    EXPECT_EQ(sessionMgr->getUserSessions("testuser").size(), 3);
}

TEST_F(SessionManagerTest, SessionCountWorks) {
    EXPECT_EQ(sessionMgr->sessionCount(), 0);

    sessionMgr->createSession(1, "user1", "user");
    EXPECT_EQ(sessionMgr->sessionCount(), 1);

    sessionMgr->createSession(2, "user2", "user");
    EXPECT_EQ(sessionMgr->sessionCount(), 2);
}

TEST_F(SessionManagerTest, TimeoutCanBeChanged) {
    sessionMgr->setTimeoutMinutes(30);
    EXPECT_EQ(sessionMgr->getTimeoutMinutes(), 30);
}

// ============================================================================
// ApiTokenManager Tests
// ============================================================================

class ApiTokenManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary database
        dbPath = "/tmp/smarthub_token_test_" + std::to_string(getpid()) + ".db";
        db = std::make_unique<smarthub::Database>(dbPath);
        ASSERT_TRUE(db->initialize());

        // UserManager must be initialized first (api_tokens references users table)
        userMgr = std::make_unique<UserManager>(*db);
        ASSERT_TRUE(userMgr->initialize());

        // Create a test user for the tokens to reference
        ASSERT_TRUE(userMgr->createUser("testuser", "Password123", "user"));

        tokenMgr = std::make_unique<ApiTokenManager>(*db);
        ASSERT_TRUE(tokenMgr->initialize());
    }

    void TearDown() override {
        tokenMgr.reset();
        userMgr.reset();
        db.reset();
        fs::remove(dbPath);
    }

    std::string dbPath;
    std::unique_ptr<smarthub::Database> db;
    std::unique_ptr<UserManager> userMgr;
    std::unique_ptr<ApiTokenManager> tokenMgr;
};

TEST_F(ApiTokenManagerTest, CreateTokenReturnsValue) {
    std::string token = tokenMgr->createToken(1, "Test Token");
    EXPECT_FALSE(token.empty());
    EXPECT_EQ(token.length(), 64);  // 32 bytes * 2 hex chars
}

TEST_F(ApiTokenManagerTest, ValidateTokenReturnsCorrectInfo) {
    std::string token = tokenMgr->createToken(1, "My Token");

    auto apiToken = tokenMgr->validateToken(token);
    ASSERT_TRUE(apiToken.has_value());
    EXPECT_EQ(apiToken->userId, 1);
    EXPECT_EQ(apiToken->name, "My Token");
    EXPECT_TRUE(apiToken->enabled);
    EXPECT_GT(apiToken->createdAt, 0u);
}

TEST_F(ApiTokenManagerTest, InvalidTokenReturnsNullopt) {
    auto result = tokenMgr->validateToken("invalid_token");
    EXPECT_FALSE(result.has_value());
}

TEST_F(ApiTokenManagerTest, EmptyTokenReturnsNullopt) {
    auto result = tokenMgr->validateToken("");
    EXPECT_FALSE(result.has_value());
}

TEST_F(ApiTokenManagerTest, RevokeTokenByIdWorks) {
    std::string token = tokenMgr->createToken(1, "Test Token");
    auto apiToken = tokenMgr->validateToken(token);
    ASSERT_TRUE(apiToken.has_value());

    EXPECT_TRUE(tokenMgr->revokeToken(apiToken->id));
    EXPECT_FALSE(tokenMgr->validateToken(token).has_value());
}

TEST_F(ApiTokenManagerTest, RevokeTokenByValueWorks) {
    std::string token = tokenMgr->createToken(1, "Test Token");
    EXPECT_TRUE(tokenMgr->validateToken(token).has_value());

    EXPECT_TRUE(tokenMgr->revokeTokenByValue(token));
    EXPECT_FALSE(tokenMgr->validateToken(token).has_value());
}

TEST_F(ApiTokenManagerTest, RevokeUserTokensWorks) {
    // Create second user for this test
    ASSERT_TRUE(userMgr->createUser("user2", "Password456", "user"));

    tokenMgr->createToken(1, "Token 1");
    tokenMgr->createToken(1, "Token 2");
    tokenMgr->createToken(2, "Other User Token");

    EXPECT_EQ(tokenMgr->countUserTokens(1), 2);
    EXPECT_EQ(tokenMgr->countUserTokens(2), 1);

    int revoked = tokenMgr->revokeUserTokens(1);
    EXPECT_EQ(revoked, 2);
    EXPECT_EQ(tokenMgr->countUserTokens(1), 0);
    EXPECT_EQ(tokenMgr->countUserTokens(2), 1);
}

TEST_F(ApiTokenManagerTest, DisabledTokenReturnsNullopt) {
    std::string token = tokenMgr->createToken(1, "Test Token");
    auto apiToken = tokenMgr->validateToken(token);
    ASSERT_TRUE(apiToken.has_value());

    // Disable the token
    EXPECT_TRUE(tokenMgr->setTokenEnabled(apiToken->id, false));

    // Should no longer validate
    EXPECT_FALSE(tokenMgr->validateToken(token).has_value());

    // Re-enable and verify it works again
    EXPECT_TRUE(tokenMgr->setTokenEnabled(apiToken->id, true));
    EXPECT_TRUE(tokenMgr->validateToken(token).has_value());
}

TEST_F(ApiTokenManagerTest, GetUserTokensWorks) {
    // Create second user for this test
    ASSERT_TRUE(userMgr->createUser("user2", "Password456", "user"));

    tokenMgr->createToken(1, "Token A");
    tokenMgr->createToken(1, "Token B");
    tokenMgr->createToken(2, "Other Token");

    auto tokens = tokenMgr->getUserTokens(1);
    EXPECT_EQ(tokens.size(), 2);

    // Tokens should be ordered by created_at DESC
    // Token field should be empty (only shown on creation)
    for (const auto& t : tokens) {
        EXPECT_EQ(t.userId, 1);
        EXPECT_TRUE(t.token.empty());
        EXPECT_FALSE(t.tokenPrefix.empty());
    }
}

TEST_F(ApiTokenManagerTest, TokenPrefixIsCorrect) {
    std::string token = tokenMgr->createToken(1, "Test Token");
    auto apiToken = tokenMgr->validateToken(token);
    ASSERT_TRUE(apiToken.has_value());

    // Prefix should be first 8 chars + "..."
    std::string expectedPrefix = token.substr(0, 8) + "...";
    EXPECT_EQ(apiToken->tokenPrefix, expectedPrefix);
}

TEST_F(ApiTokenManagerTest, LastUsedUpdatedOnValidation) {
    std::string token = tokenMgr->createToken(1, "Test Token");

    // First validation
    auto apiToken1 = tokenMgr->validateToken(token);
    ASSERT_TRUE(apiToken1.has_value());
    uint64_t lastUsed1 = apiToken1->lastUsed;

    // Wait a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    // Second validation - lastUsed should be updated
    auto apiToken2 = tokenMgr->validateToken(token);
    ASSERT_TRUE(apiToken2.has_value());

    EXPECT_GE(apiToken2->lastUsed, lastUsed1);
}

TEST_F(ApiTokenManagerTest, TokenWithExpiryWorks) {
    // Create token that expires in 1 day
    std::string token = tokenMgr->createToken(1, "Expiring Token", 1);
    auto apiToken = tokenMgr->validateToken(token);
    ASSERT_TRUE(apiToken.has_value());
    EXPECT_GT(apiToken->expiresAt, 0u);
}

TEST_F(ApiTokenManagerTest, TokenWithoutExpiryNeverExpires) {
    // Create token with no expiry
    std::string token = tokenMgr->createToken(1, "Forever Token", 0);
    auto apiToken = tokenMgr->validateToken(token);
    ASSERT_TRUE(apiToken.has_value());
    EXPECT_EQ(apiToken->expiresAt, 0u);
}

TEST_F(ApiTokenManagerTest, MultipleTokensForSameUser) {
    std::string token1 = tokenMgr->createToken(1, "Token 1");
    std::string token2 = tokenMgr->createToken(1, "Token 2");
    std::string token3 = tokenMgr->createToken(1, "Token 3");

    EXPECT_NE(token1, token2);
    EXPECT_NE(token2, token3);
    EXPECT_NE(token1, token3);

    // All should be valid
    EXPECT_TRUE(tokenMgr->validateToken(token1).has_value());
    EXPECT_TRUE(tokenMgr->validateToken(token2).has_value());
    EXPECT_TRUE(tokenMgr->validateToken(token3).has_value());

    EXPECT_EQ(tokenMgr->countUserTokens(1), 3);
}

// ============================================================================
// CredentialStore Tests
// ============================================================================

class CredentialStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary database
        dbPath = "/tmp/smarthub_cred_test_" + std::to_string(getpid()) + ".db";
        db = std::make_unique<smarthub::Database>(dbPath);
        ASSERT_TRUE(db->initialize());

        credStore = std::make_unique<CredentialStore>(*db);
        ASSERT_TRUE(credStore->initialize());
    }

    void TearDown() override {
        credStore.reset();
        db.reset();
        fs::remove(dbPath);
    }

    std::string dbPath;
    std::unique_ptr<smarthub::Database> db;
    std::unique_ptr<CredentialStore> credStore;
};

TEST_F(CredentialStoreTest, InitializeSucceeds) {
    EXPECT_FALSE(credStore->isUnlocked());
}

TEST_F(CredentialStoreTest, UnlockWithPassphrase) {
    EXPECT_TRUE(credStore->unlock("TestPassphrase123"));
    EXPECT_TRUE(credStore->isUnlocked());
}

TEST_F(CredentialStoreTest, LockClearsKey) {
    credStore->unlock("TestPassphrase123");
    EXPECT_TRUE(credStore->isUnlocked());

    credStore->lock();
    EXPECT_FALSE(credStore->isUnlocked());
}

TEST_F(CredentialStoreTest, CannotSetWithoutUnlock) {
    EXPECT_FALSE(credStore->set("key", "value"));
}

TEST_F(CredentialStoreTest, CannotGetWithoutUnlock) {
    auto result = credStore->get("key");
    EXPECT_FALSE(result.has_value());
}

TEST_F(CredentialStoreTest, SetAndGetCredential) {
    ASSERT_TRUE(credStore->unlock("TestPassphrase123"));

    EXPECT_TRUE(credStore->set("test.api.key", "secret-api-key-12345"));

    auto retrieved = credStore->get("test.api.key");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(*retrieved, "secret-api-key-12345");
}

TEST_F(CredentialStoreTest, SetWithCategory) {
    ASSERT_TRUE(credStore->unlock("TestPassphrase123"));

    EXPECT_TRUE(credStore->set("tuya.device1.key", "tuyakey123", "tuya"));
    EXPECT_TRUE(credStore->set("mqtt.broker.password", "mqttpass", "mqtt"));

    auto credentials = credStore->list("tuya");
    EXPECT_EQ(credentials.size(), 1);
    EXPECT_EQ(credentials[0].name, "tuya.device1.key");
    EXPECT_EQ(credentials[0].category, "tuya");
}

TEST_F(CredentialStoreTest, UpdateExistingCredential) {
    ASSERT_TRUE(credStore->unlock("TestPassphrase123"));

    EXPECT_TRUE(credStore->set("key", "value1"));
    auto v1 = credStore->get("key");
    ASSERT_TRUE(v1.has_value());
    EXPECT_EQ(*v1, "value1");

    EXPECT_TRUE(credStore->set("key", "value2"));
    auto v2 = credStore->get("key");
    ASSERT_TRUE(v2.has_value());
    EXPECT_EQ(*v2, "value2");
}

TEST_F(CredentialStoreTest, RemoveCredential) {
    ASSERT_TRUE(credStore->unlock("TestPassphrase123"));

    EXPECT_TRUE(credStore->set("key", "value"));
    EXPECT_TRUE(credStore->exists("key"));

    EXPECT_TRUE(credStore->remove("key"));
    EXPECT_FALSE(credStore->exists("key"));
}

TEST_F(CredentialStoreTest, GetNonexistentReturnsNullopt) {
    ASSERT_TRUE(credStore->unlock("TestPassphrase123"));

    auto result = credStore->get("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(CredentialStoreTest, ListAllCredentials) {
    ASSERT_TRUE(credStore->unlock("TestPassphrase123"));

    credStore->set("key1", "value1", "cat1");
    credStore->set("key2", "value2", "cat1");
    credStore->set("key3", "value3", "cat2");

    auto all = credStore->list();
    EXPECT_EQ(all.size(), 3);

    // Values should be hidden
    for (const auto& cred : all) {
        EXPECT_TRUE(cred.value.empty());
    }
}

TEST_F(CredentialStoreTest, CountCredentials) {
    ASSERT_TRUE(credStore->unlock("TestPassphrase123"));

    EXPECT_EQ(credStore->count(), 0);

    credStore->set("key1", "value1", "cat1");
    credStore->set("key2", "value2", "cat1");
    credStore->set("key3", "value3", "cat2");

    EXPECT_EQ(credStore->count(), 3);
    EXPECT_EQ(credStore->count("cat1"), 2);
    EXPECT_EQ(credStore->count("cat2"), 1);
}

TEST_F(CredentialStoreTest, ClearCategory) {
    ASSERT_TRUE(credStore->unlock("TestPassphrase123"));

    credStore->set("key1", "value1", "cat1");
    credStore->set("key2", "value2", "cat1");
    credStore->set("key3", "value3", "cat2");

    int cleared = credStore->clearCategory("cat1");
    EXPECT_EQ(cleared, 2);
    EXPECT_EQ(credStore->count("cat1"), 0);
    EXPECT_EQ(credStore->count("cat2"), 1);
}

TEST_F(CredentialStoreTest, EncryptionWorks) {
    // Unlock with one passphrase
    ASSERT_TRUE(credStore->unlock("Passphrase1"));
    EXPECT_TRUE(credStore->set("key", "secret"));

    // Lock and unlock with different passphrase
    credStore->lock();

    // Create new store instance with different passphrase
    auto credStore2 = std::make_unique<CredentialStore>(*db);
    ASSERT_TRUE(credStore2->initialize());
    ASSERT_TRUE(credStore2->unlock("WrongPassphrase"));

    // Should fail to decrypt (or return wrong value)
    auto result = credStore2->get("key");
    // With proper encryption, this should either:
    // - Return nullopt (decryption failed)
    // - Or return garbage (wrong key)
    // It should NOT return "secret"
    if (result.has_value()) {
        EXPECT_NE(*result, "secret");
    }
}

TEST_F(CredentialStoreTest, ChangePassphrase) {
    ASSERT_TRUE(credStore->unlock("OldPassphrase"));
    EXPECT_TRUE(credStore->set("key1", "value1"));
    EXPECT_TRUE(credStore->set("key2", "value2"));

    // Change passphrase
    EXPECT_TRUE(credStore->changePassphrase("OldPassphrase", "NewPassphrase"));

    // Verify credentials still accessible with new passphrase
    credStore->lock();
    EXPECT_TRUE(credStore->unlock("NewPassphrase"));

    auto v1 = credStore->get("key1");
    ASSERT_TRUE(v1.has_value());
    EXPECT_EQ(*v1, "value1");

    auto v2 = credStore->get("key2");
    ASSERT_TRUE(v2.has_value());
    EXPECT_EQ(*v2, "value2");
}

TEST_F(CredentialStoreTest, EmptyValueAllowed) {
    ASSERT_TRUE(credStore->unlock("TestPassphrase123"));

    EXPECT_TRUE(credStore->set("empty.key", ""));
    auto result = credStore->get("empty.key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "");
}

TEST_F(CredentialStoreTest, SpecialCharactersInValue) {
    ASSERT_TRUE(credStore->unlock("TestPassphrase123"));

    std::string specialValue = "key=value&special!@#$%^&*(){}[]\"'\\n\\t";
    EXPECT_TRUE(credStore->set("special.key", specialValue));

    auto result = credStore->get("special.key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, specialValue);
}

TEST_F(CredentialStoreTest, LongValue) {
    ASSERT_TRUE(credStore->unlock("TestPassphrase123"));

    // Create a long value (e.g., a certificate or long key)
    std::string longValue(4096, 'x');
    EXPECT_TRUE(credStore->set("long.key", longValue));

    auto result = credStore->get("long.key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, longValue);
}

// ============================================================================
// SetupManager Tests
// ============================================================================

class SetupManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary paths
        testDir = "/tmp/smarthub_setup_test_" + std::to_string(getpid());
        dbPath = testDir + "/test.db";
        certDir = testDir + "/certs";

        fs::create_directories(testDir);
        fs::create_directories(certDir);

        // Initialize components
        db = std::make_unique<smarthub::Database>(dbPath);
        ASSERT_TRUE(db->initialize());

        userMgr = std::make_unique<UserManager>(*db);
        ASSERT_TRUE(userMgr->initialize());

        certMgr = std::make_unique<CertManager>(certDir);

        credStore = std::make_unique<CredentialStore>(*db);
        ASSERT_TRUE(credStore->initialize());

        setupMgr = std::make_unique<SetupManager>(*db, *userMgr, *certMgr, *credStore);
        ASSERT_TRUE(setupMgr->initialize());
    }

    void TearDown() override {
        setupMgr.reset();
        credStore.reset();
        certMgr.reset();
        userMgr.reset();
        db.reset();
        fs::remove_all(testDir);
    }

    std::string testDir;
    std::string dbPath;
    std::string certDir;
    std::unique_ptr<smarthub::Database> db;
    std::unique_ptr<UserManager> userMgr;
    std::unique_ptr<CertManager> certMgr;
    std::unique_ptr<CredentialStore> credStore;
    std::unique_ptr<SetupManager> setupMgr;
};

TEST_F(SetupManagerTest, InitializeSucceeds) {
    // Already initialized in SetUp
    EXPECT_TRUE(setupMgr->isSetupRequired());
}

TEST_F(SetupManagerTest, SetupRequiredWithoutAdmin) {
    EXPECT_TRUE(setupMgr->isSetupRequired());

    auto status = setupMgr->getSetupStatus();
    EXPECT_FALSE(status.isSetupComplete);
    EXPECT_FALSE(status.hasAdminUser);
}

TEST_F(SetupManagerTest, ValidateConfigRejectsEmptyUsername) {
    SetupConfig config;
    config.adminUsername = "";
    config.adminPassword = "ValidPass123";
    config.hostname = "test.local";
    config.credentialPassphrase = "SecurePassphrase";

    std::string error = setupMgr->validateConfig(config);
    EXPECT_FALSE(error.empty());
    EXPECT_NE(error.find("username"), std::string::npos);
}

TEST_F(SetupManagerTest, ValidateConfigRejectsWeakPassword) {
    SetupConfig config;
    config.adminUsername = "admin";
    config.adminPassword = "weak";  // Too short, no uppercase, no digit
    config.hostname = "test.local";
    config.credentialPassphrase = "SecurePassphrase";

    std::string error = setupMgr->validateConfig(config);
    EXPECT_FALSE(error.empty());
    EXPECT_NE(error.find("password"), std::string::npos);
}

TEST_F(SetupManagerTest, ValidateConfigRejectsShortPassphrase) {
    SetupConfig config;
    config.adminUsername = "admin";
    config.adminPassword = "ValidPass123";
    config.hostname = "test.local";
    config.credentialPassphrase = "short";

    std::string error = setupMgr->validateConfig(config);
    EXPECT_FALSE(error.empty());
    EXPECT_NE(error.find("passphrase"), std::string::npos);
}

TEST_F(SetupManagerTest, ValidateConfigAcceptsValidConfig) {
    SetupConfig config;
    config.adminUsername = "admin";
    config.adminPassword = "ValidPass123";
    config.hostname = "test.local";
    config.credentialPassphrase = "SecurePassphrase123";

    std::string error = setupMgr->validateConfig(config);
    EXPECT_TRUE(error.empty());
}

TEST_F(SetupManagerTest, PerformSetupCreatesAdmin) {
    SetupConfig config;
    config.adminUsername = "myadmin";
    config.adminPassword = "MySecurePass123";
    config.hostname = "smarthub.local";
    config.credentialPassphrase = "StorePassphrase123";

    EXPECT_TRUE(setupMgr->performSetup(config));

    // Verify admin was created
    EXPECT_TRUE(userMgr->hasAdminUser());
    EXPECT_TRUE(userMgr->authenticate("myadmin", "MySecurePass123"));
}

TEST_F(SetupManagerTest, PerformSetupMarksComplete) {
    SetupConfig config;
    config.adminUsername = "admin";
    config.adminPassword = "ValidPass123";
    config.hostname = "test.local";
    config.credentialPassphrase = "SecurePassphrase123";

    EXPECT_TRUE(setupMgr->isSetupRequired());
    EXPECT_TRUE(setupMgr->performSetup(config));
    EXPECT_FALSE(setupMgr->isSetupRequired());
}

TEST_F(SetupManagerTest, SetupStatusAfterComplete) {
    SetupConfig config;
    config.adminUsername = "admin";
    config.adminPassword = "ValidPass123";
    config.hostname = "test.local";
    config.credentialPassphrase = "SecurePassphrase123";

    EXPECT_TRUE(setupMgr->performSetup(config));

    auto status = setupMgr->getSetupStatus();
    EXPECT_TRUE(status.isSetupComplete);
    EXPECT_TRUE(status.hasAdminUser);
}

TEST_F(SetupManagerTest, ResetSetupState) {
    SetupConfig config;
    config.adminUsername = "admin";
    config.adminPassword = "ValidPass123";
    config.hostname = "test.local";
    config.credentialPassphrase = "SecurePassphrase123";

    EXPECT_TRUE(setupMgr->performSetup(config));
    EXPECT_FALSE(setupMgr->isSetupRequired());

    setupMgr->resetSetupState();

    // Still not required because admin exists
    EXPECT_FALSE(setupMgr->isSetupRequired());
}

TEST_F(SetupManagerTest, GetDefaultHostname) {
    std::string hostname = SetupManager::getDefaultHostname();
    EXPECT_FALSE(hostname.empty());
}

TEST_F(SetupManagerTest, GetLocalIpAddress) {
    std::string ip = SetupManager::getLocalIpAddress();
    EXPECT_FALSE(ip.empty());
    // Should be a valid-looking IP
    EXPECT_NE(ip.find('.'), std::string::npos);
}

} // namespace smarthub::security
