# Phase 7: Security Hardening - Detailed TODO

## Overview
**Duration**: 2 weeks  
**Objective**: Implement security measures appropriate for a local network device.  
**Prerequisites**: Phase 2 complete, Phase 6 recommended

---

## 7.1 TLS Certificate Management

### 7.1.1 Self-Signed Certificate Generation
```bash
#!/bin/bash
# tools/generate-certs.sh

CERT_DIR="/etc/smarthub/certs"
mkdir -p $CERT_DIR

# Generate CA
openssl genrsa -out $CERT_DIR/ca.key 4096
openssl req -new -x509 -days 3650 -key $CERT_DIR/ca.key \
    -out $CERT_DIR/ca.crt \
    -subj "/C=US/ST=Home/L=Home/O=SmartHub/CN=SmartHub CA"

# Generate server certificate
openssl genrsa -out $CERT_DIR/server.key 2048
openssl req -new -key $CERT_DIR/server.key \
    -out $CERT_DIR/server.csr \
    -subj "/C=US/ST=Home/L=Home/O=SmartHub/CN=smarthub.local"

# Sign with CA
openssl x509 -req -days 365 -in $CERT_DIR/server.csr \
    -CA $CERT_DIR/ca.crt -CAkey $CERT_DIR/ca.key \
    -CAcreateserial -out $CERT_DIR/server.crt \
    -extfile <(printf "subjectAltName=DNS:smarthub.local,DNS:smarthub,IP:192.168.1.100")

# Set permissions
chmod 600 $CERT_DIR/*.key
chmod 644 $CERT_DIR/*.crt
```

### 7.1.2 Certificate Manager Class
```cpp
// app/include/smarthub/security/CertManager.hpp
#pragma once

#include <string>
#include <ctime>

namespace smarthub::security {

class CertManager {
public:
    CertManager(const std::string& certDir);
    
    bool initialize();
    
    // Certificate paths
    std::string caCertPath() const;
    std::string serverCertPath() const;
    std::string serverKeyPath() const;
    
    // Certificate info
    bool isValid() const;
    time_t expirationDate() const;
    int daysUntilExpiry() const;
    
    // Regenerate certificates
    bool regenerate(const std::string& hostname, const std::string& ip);
    
private:
    std::string m_certDir;
    time_t m_expiration = 0;
};

} // namespace smarthub::security
```

---

## 7.2 Web Authentication

### 7.2.1 User Management
```cpp
// app/include/smarthub/security/UserManager.hpp
#pragma once

#include <string>
#include <optional>

namespace smarthub::security {

struct User {
    int id;
    std::string username;
    std::string passwordHash;
    std::string role;  // "admin", "user"
    uint64_t createdAt;
};

class UserManager {
public:
    UserManager(Database& db);
    
    // User management
    bool createUser(const std::string& username, const std::string& password,
                    const std::string& role = "user");
    bool deleteUser(const std::string& username);
    bool changePassword(const std::string& username, const std::string& newPassword);
    std::optional<User> getUser(const std::string& username);
    
    // Authentication
    bool authenticate(const std::string& username, const std::string& password);
    
    // First-run setup
    bool hasAdminUser() const;
    
private:
    std::string hashPassword(const std::string& password, const std::string& salt);
    std::string generateSalt();
    
    Database& m_db;
};

} // namespace smarthub::security
```

### 7.2.2 Password Hashing (Argon2)
```cpp
// app/src/security/UserManager.cpp
#include <openssl/evp.h>
#include <openssl/rand.h>

std::string UserManager::hashPassword(const std::string& password, 
                                       const std::string& salt) {
    // Use PBKDF2 with SHA-256
    const int iterations = 100000;
    const int keyLen = 32;
    
    unsigned char hash[keyLen];
    PKCS5_PBKDF2_HMAC(password.c_str(), password.length(),
                       reinterpret_cast<const unsigned char*>(salt.c_str()),
                       salt.length(), iterations,
                       EVP_sha256(), keyLen, hash);
    
    // Encode as base64
    // ... (implementation)
    
    return salt + "$" + base64Hash;
}

bool UserManager::authenticate(const std::string& username, 
                                const std::string& password) {
    auto user = getUser(username);
    if (!user) return false;
    
    // Extract salt from stored hash
    auto pos = user->passwordHash.find('$');
    if (pos == std::string::npos) return false;
    
    std::string salt = user->passwordHash.substr(0, pos);
    std::string computedHash = hashPassword(password, salt);
    
    // Constant-time comparison
    return CRYPTO_memcmp(computedHash.c_str(), user->passwordHash.c_str(),
                          computedHash.length()) == 0;
}
```

### 7.2.3 Session Management
```cpp
// app/include/smarthub/security/SessionManager.hpp
#pragma once

#include <string>
#include <map>
#include <mutex>

namespace smarthub::security {

struct Session {
    std::string token;
    std::string username;
    std::string role;
    uint64_t createdAt;
    uint64_t expiresAt;
    std::string ipAddress;
};

class SessionManager {
public:
    SessionManager(int sessionTimeoutMinutes = 60);
    
    // Create session after successful login
    std::string createSession(const std::string& username, 
                               const std::string& role,
                               const std::string& ip);
    
    // Validate session token
    std::optional<Session> validateSession(const std::string& token);
    
    // Destroy session (logout)
    void destroySession(const std::string& token);
    
    // Cleanup expired sessions
    void cleanup();
    
private:
    std::string generateToken();
    
    std::map<std::string, Session> m_sessions;
    std::mutex m_mutex;
    int m_timeoutMinutes;
};

} // namespace smarthub::security
```

### 7.2.4 API Token Authentication
```cpp
// app/include/smarthub/security/ApiTokenManager.hpp
#pragma once

namespace smarthub::security {

struct ApiToken {
    std::string token;
    int userId;
    std::string name;
    uint64_t createdAt;
    uint64_t expiresAt;  // 0 = never
};

class ApiTokenManager {
public:
    ApiTokenManager(Database& db);
    
    // Create long-lived API token
    std::string createToken(int userId, const std::string& name,
                             int expiryDays = 0);
    
    // Validate token
    std::optional<ApiToken> validateToken(const std::string& token);
    
    // Revoke token
    bool revokeToken(const std::string& token);
    
    // List user's tokens
    std::vector<ApiToken> getUserTokens(int userId);
    
private:
    Database& m_db;
};

} // namespace smarthub::security
```

---

## 7.3 Web Server Security

### 7.3.1 Authentication Middleware
```cpp
// app/src/web/AuthMiddleware.cpp

bool WebServer::requireAuth(struct mg_connection* c, struct mg_http_message* hm) {
    // Check for session cookie
    struct mg_str* cookie = mg_http_get_header(hm, "Cookie");
    if (cookie) {
        std::string sessionToken = extractCookie(*cookie, "session");
        if (!sessionToken.empty()) {
            auto session = m_sessionManager.validateSession(sessionToken);
            if (session) {
                c->data[0] = session->role[0];  // Store role for later use
                return true;
            }
        }
    }
    
    // Check for API token
    struct mg_str* auth = mg_http_get_header(hm, "Authorization");
    if (auth) {
        std::string authStr(auth->buf, auth->len);
        if (authStr.starts_with("Bearer ")) {
            std::string token = authStr.substr(7);
            auto apiToken = m_apiTokenManager.validateToken(token);
            if (apiToken) {
                return true;
            }
        }
    }
    
    // Unauthorized
    mg_http_reply(c, 401, "WWW-Authenticate: Bearer\r\n",
                  "{\"error\":\"Unauthorized\"}");
    return false;
}
```

### 7.3.2 Rate Limiting
```cpp
// app/include/smarthub/security/RateLimiter.hpp
#pragma once

#include <map>
#include <mutex>
#include <chrono>

namespace smarthub::security {

class RateLimiter {
public:
    RateLimiter(int maxRequests, int windowSeconds);
    
    // Check if request should be allowed
    bool allow(const std::string& key);
    
    // Reset limits for key
    void reset(const std::string& key);
    
private:
    struct Window {
        std::chrono::steady_clock::time_point start;
        int count;
    };
    
    int m_maxRequests;
    int m_windowSeconds;
    std::map<std::string, Window> m_windows;
    std::mutex m_mutex;
};

} // namespace smarthub::security

// Usage in login endpoint
void WebServer::handleLogin(mg_connection* c, mg_http_message* hm) {
    std::string ip = getClientIp(c);
    
    if (!m_loginLimiter.allow(ip)) {
        mg_http_reply(c, 429, "", "{\"error\":\"Too many attempts\"}");
        return;
    }
    
    // Process login...
}
```

### 7.3.3 Security Headers
```cpp
void WebServer::addSecurityHeaders(mg_connection* c) {
    const char* headers = 
        "X-Content-Type-Options: nosniff\r\n"
        "X-Frame-Options: DENY\r\n"
        "X-XSS-Protection: 1; mode=block\r\n"
        "Content-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'\r\n"
        "Strict-Transport-Security: max-age=31536000; includeSubDomains\r\n";
    
    mg_printf(c, "%s", headers);
}
```

---

## 7.4 Secure Credential Storage

### 7.4.1 Encrypted Settings
```cpp
// app/include/smarthub/security/SecureStorage.hpp
#pragma once

namespace smarthub::security {

class SecureStorage {
public:
    SecureStorage(const std::string& keyFile);
    
    // Store encrypted value
    bool store(const std::string& key, const std::string& value);
    
    // Retrieve and decrypt value
    std::optional<std::string> retrieve(const std::string& key);
    
    // Delete value
    bool remove(const std::string& key);
    
private:
    bool loadOrGenerateKey();
    std::string encrypt(const std::string& plaintext);
    std::string decrypt(const std::string& ciphertext);
    
    std::string m_keyFile;
    std::vector<uint8_t> m_key;
};

} // namespace smarthub::security
```

### 7.4.2 WiFi Credential Protection
```cpp
// Store WiFi credentials encrypted
void WifiManager::saveCredentials(const std::string& ssid, 
                                   const std::string& password) {
    nlohmann::json creds;
    creds["ssid"] = ssid;
    creds["psk"] = password;
    
    m_secureStorage.store("wifi_credentials", creds.dump());
}

// Load and decrypt when needed
bool WifiManager::connect() {
    auto credsJson = m_secureStorage.retrieve("wifi_credentials");
    if (!credsJson) return false;
    
    auto creds = nlohmann::json::parse(*credsJson);
    return connectToNetwork(creds["ssid"], creds["psk"]);
}
```

---

## 7.5 Network Security

### 7.5.1 Firewall Rules (iptables)
```bash
#!/bin/bash
# overlay/etc/init.d/S45firewall

case "$1" in
  start)
    # Flush existing rules
    iptables -F
    iptables -X
    
    # Default policies
    iptables -P INPUT DROP
    iptables -P FORWARD DROP
    iptables -P OUTPUT ACCEPT
    
    # Allow loopback
    iptables -A INPUT -i lo -j ACCEPT
    
    # Allow established connections
    iptables -A INPUT -m state --state ESTABLISHED,RELATED -j ACCEPT
    
    # Allow HTTPS (443)
    iptables -A INPUT -p tcp --dport 443 -j ACCEPT
    
    # Allow HTTP for redirect (80)
    iptables -A INPUT -p tcp --dport 80 -j ACCEPT
    
    # Allow MQTT (1883) from local network only
    iptables -A INPUT -p tcp --dport 1883 -s 192.168.0.0/16 -j ACCEPT
    iptables -A INPUT -p tcp --dport 1883 -s 10.0.0.0/8 -j ACCEPT
    
    # Allow mDNS
    iptables -A INPUT -p udp --dport 5353 -j ACCEPT
    
    # Allow ICMP (ping)
    iptables -A INPUT -p icmp --icmp-type echo-request -j ACCEPT
    
    # Log dropped packets (optional, can fill logs)
    # iptables -A INPUT -j LOG --log-prefix "iptables: "
    
    echo "Firewall configured"
    ;;
  stop)
    iptables -F
    iptables -P INPUT ACCEPT
    echo "Firewall disabled"
    ;;
esac
```

### 7.5.2 HTTP to HTTPS Redirect
```cpp
void WebServer::handleHttpRedirect(mg_connection* c, mg_http_message* hm) {
    // Get Host header
    struct mg_str* host = mg_http_get_header(hm, "Host");
    std::string hostStr = host ? std::string(host->buf, host->len) : "smarthub.local";
    
    // Remove port if present
    auto colonPos = hostStr.find(':');
    if (colonPos != std::string::npos) {
        hostStr = hostStr.substr(0, colonPos);
    }
    
    // Build redirect URL
    std::string location = "https://" + hostStr + 
                           std::string(hm->uri.buf, hm->uri.len);
    
    mg_http_reply(c, 301, 
                  ("Location: " + location + "\r\n").c_str(),
                  "Redirecting to HTTPS...");
}
```

---

## 7.6 Secure Boot (Optional/Evaluation)

### 7.6.1 Overview
```markdown
STM32MP1 Secure Boot Chain:
1. ROM Code (immutable)
2. TF-A BL2 (signed, authenticated by ROM)
3. OP-TEE (signed, authenticated by TF-A)
4. U-Boot (signed, authenticated by TF-A)
5. Linux Kernel (signed, authenticated by U-Boot)

Requirements:
- Generate ECDSA key pair
- Sign all boot images
- Fuse public key hash to OTP (IRREVERSIBLE!)

For development: Skip secure boot, implement application-level security
For production: Full secure boot chain recommended
```

### 7.6.2 Evaluation Tasks
- ☐ Study STM32MP1 secure boot documentation
- ☐ Generate test keys (DO NOT fuse)
- ☐ Practice signing workflow
- ☐ Document production procedure

---

## 7.7 First-Run Setup Flow

### 7.7.1 Initial Setup Screen
```cpp
class SetupScreen : public Screen {
public:
    void onCreate() override {
        // Check if setup completed
        if (m_config.getBool("setup.completed", false)) {
            m_uiManager.showScreen("home");
            return;
        }
        
        // Show setup wizard
        createAdminAccountStep();
    }
    
private:
    void createAdminAccountStep();
    void configureNetworkStep();
    void finishSetup();
};
```

---

## 7.8 Validation Checklist

| Item | Status | Notes |
|------|--------|-------|
| TLS certificates generate | ✅ | CertManager with OpenSSL |
| HTTPS works | ✅ | OpenSSL TLS on ARM (MG_TLS=2) |
| HTTP redirects to HTTPS | ✅ | 301 redirect on port 80 |
| User creation works | ✅ | Argon2id password hashing |
| Login authentication works | ✅ | Session cookie flow |
| Session management works | ✅ | 18 tests pass |
| API token auth works | ✅ | Bearer token flow |
| Rate limiting works | ✅ | Per-IP rate limiting |
| Firewall rules active | ⏸️ | Deferred - system-level config |
| Credentials encrypted | ✅ | AES-256-GCM |
| First-run setup works | ✅ | Backend complete |

**Hardware Validation (STM32MP157F-DK2):**
- ✅ HTTPS connectivity verified
- ✅ Security headers confirmed
- ✅ Authentication middleware working
- ✅ 115 tests passing

---

## Time Estimates

| Task | Estimated Time |
|------|----------------|
| 7.1 TLS Certificates | 3-4 hours |
| 7.2 Authentication | 8-10 hours |
| 7.3 Web Security | 4-6 hours |
| 7.4 Secure Storage | 4-6 hours |
| 7.5 Network Security | 3-4 hours |
| 7.6 Secure Boot Eval | 4-6 hours |
| 7.7 Setup Flow | 3-4 hours |
| 7.8 Testing | 4-6 hours |
| **Total** | **33-46 hours** |
