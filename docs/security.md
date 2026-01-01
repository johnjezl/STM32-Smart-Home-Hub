# SmartHub Security Architecture

This document describes the security features implemented in SmartHub for protecting user data and securing network communications.

## Overview

SmartHub implements multiple layers of security appropriate for a local network IoT hub:

- **Transport Security**: TLS 1.2+ for HTTPS connections
- **Authentication**: Session-based web authentication and API tokens
- **Authorization**: Role-based access control (admin/user)
- **Password Security**: Argon2id password hashing
- **Credential Protection**: AES-256-GCM encrypted credential storage
- **Request Protection**: Rate limiting and security headers

## Components

### TLS Certificate Management (`CertManager`)

Manages TLS certificates for HTTPS connections.

```cpp
#include <smarthub/security/CertManager.hpp>

CertManager certMgr("/etc/smarthub/certs");
certMgr.initialize();

// Check certificate validity
if (certMgr.daysUntilExpiry() < 30) {
    certMgr.regenerate("smarthub.local", "192.168.1.100");
}
```

**Features:**
- Self-signed certificate generation
- CA and server certificate management
- Automatic expiration checking
- Certificate regeneration with custom hostname/IP

**File Locations:**
- CA Certificate: `/etc/smarthub/certs/ca.crt`
- Server Certificate: `/etc/smarthub/certs/server.crt`
- Server Key: `/etc/smarthub/certs/server.key`

### User Management (`UserManager`)

Manages user accounts with secure password storage.

```cpp
#include <smarthub/security/UserManager.hpp>

UserManager userMgr(database);
userMgr.initialize();

// Create admin user
userMgr.createUser("admin", "SecureP@ssw0rd!", "admin");

// Authenticate
if (userMgr.authenticate("admin", "SecureP@ssw0rd!")) {
    // Success
}
```

**Password Security:**
- Argon2id hashing (memory-hard, GPU-resistant)
- Unique random salt per password (16 bytes)
- Configurable iterations (default: 3)
- Memory cost: 64 MB
- Parallelism: 4 threads

**User Roles:**
- `admin`: Full system access
- `user`: Device control, limited configuration

### Session Management (`SessionManager`)

Manages web authentication sessions.

```cpp
#include <smarthub/security/SessionManager.hpp>

SessionManager sessionMgr(database, 60);  // 60-minute sessions

// Create session after login
std::string token = sessionMgr.createSession(userId, "admin", "admin");

// Validate session
auto session = sessionMgr.validateSession(token);
if (session) {
    // Session valid, access session->username, session->role
}

// Logout
sessionMgr.destroySession(token);
```

**Features:**
- Cryptographically secure token generation (32 bytes, hex-encoded)
- Configurable session timeout
- Automatic expired session cleanup
- User session enumeration and bulk invalidation

### API Token Authentication (`ApiTokenManager`)

Long-lived tokens for programmatic API access.

```cpp
#include <smarthub/security/ApiTokenManager.hpp>

ApiTokenManager tokenMgr(database);
tokenMgr.initialize();

// Create API token (0 = never expires)
std::string token = tokenMgr.createToken(userId, "Home Assistant Integration", 365);

// Validate token
auto apiToken = tokenMgr.validateToken(token);
if (apiToken) {
    // Valid, access apiToken->userId, apiToken->name
}

// Revoke token
tokenMgr.revokeToken(token);
```

**Features:**
- SHA-256 token hashing (only hash stored in database)
- Optional expiration (days or never)
- Token naming for identification
- Per-user token limits
- Bulk token revocation

### Credential Storage (`CredentialStore`)

Encrypted storage for sensitive configuration data.

```cpp
#include <smarthub/security/CredentialStore.hpp>

CredentialStore store("/var/lib/smarthub/credentials");
store.initialize("master-passphrase");

// Store sensitive data
store.store("mqtt_password", "secret123");
store.store("wifi_psk", "WifiPassword!");

// Retrieve
auto password = store.get("mqtt_password");
if (password) {
    // Use *password
}
```

**Encryption:**
- AES-256-GCM authenticated encryption
- PBKDF2-SHA256 key derivation (100,000 iterations)
- Unique IV per encryption
- Authentication tag verification

### First-Run Setup (`SetupManager`)

Manages initial system configuration.

```cpp
#include <smarthub/security/SetupManager.hpp>

SetupManager setup(database, userMgr, credStore, certMgr);
setup.initialize();

if (!setup.isSetupComplete()) {
    SetupConfig config;
    config.adminUsername = "admin";
    config.adminPassword = "SecurePassword123!";
    config.hostname = "smarthub.local";

    setup.performSetup(config);
}
```

## Web Server Security

### Security Headers

All responses include security headers:

| Header | Value | Purpose |
|--------|-------|---------|
| `X-Content-Type-Options` | `nosniff` | Prevent MIME sniffing |
| `X-Frame-Options` | `DENY` | Prevent clickjacking |
| `X-XSS-Protection` | `1; mode=block` | XSS filter |
| `Referrer-Policy` | `strict-origin-when-cross-origin` | Control referrer leakage |
| `Cache-Control` | `no-store` | Prevent sensitive data caching |
| `Strict-Transport-Security` | `max-age=31536000; includeSubDomains` | Enforce HTTPS (when TLS enabled) |

### Authentication Middleware

Protected routes require authentication via:

1. **Session Cookie**: `session=<token>` (for web UI)
2. **Bearer Token**: `Authorization: Bearer <token>` (for API clients)

**Public Routes** (no auth required):
- `/api/auth/login`
- `/api/system/status`
- Static files (non-API routes)

### Rate Limiting

Per-IP rate limiting protects against brute force attacks:

```cpp
webServer.setRateLimit(60);  // 60 requests per minute per IP
```

When exceeded, returns `429 Too Many Requests`.

### HTTP to HTTPS Redirect

When TLS is configured, HTTP requests are redirected:

```
HTTP/1.1 301 Moved Permanently
Location: https://smarthub.local/...
```

## API Authentication

### Login

```bash
curl -X POST https://smarthub.local/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"password"}'
```

Response sets session cookie:
```
Set-Cookie: session=<token>; HttpOnly; SameSite=Strict; Secure; Path=/
```

### API Token Usage

```bash
curl https://smarthub.local/api/devices \
  -H "Authorization: Bearer <api-token>"
```

### Logout

```bash
curl -X POST https://smarthub.local/api/auth/logout \
  -H "Cookie: session=<token>"
```

## Database Schema

### Users Table
```sql
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    username TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    role TEXT DEFAULT 'user',
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL
);
```

### Sessions Table
```sql
CREATE TABLE sessions (
    id INTEGER PRIMARY KEY,
    user_id INTEGER NOT NULL,
    token_hash TEXT UNIQUE NOT NULL,
    username TEXT NOT NULL,
    role TEXT NOT NULL,
    created_at INTEGER NOT NULL,
    expires_at INTEGER NOT NULL,
    ip_address TEXT,
    FOREIGN KEY (user_id) REFERENCES users(id)
);
```

### API Tokens Table
```sql
CREATE TABLE api_tokens (
    id INTEGER PRIMARY KEY,
    user_id INTEGER NOT NULL,
    token_hash TEXT UNIQUE NOT NULL,
    name TEXT NOT NULL,
    created_at INTEGER NOT NULL,
    expires_at INTEGER,
    last_used_at INTEGER,
    FOREIGN KEY (user_id) REFERENCES users(id)
);
```

## Configuration

### config.yaml

```yaml
web:
  port: 443
  tls:
    cert: /etc/smarthub/certs/server.crt
    key: /etc/smarthub/certs/server.key

security:
  session_timeout_minutes: 60
  rate_limit_per_minute: 60
  max_api_tokens_per_user: 10
```

## Security Best Practices

1. **Change default passwords** immediately after first-run setup
2. **Use strong passwords** (12+ characters, mixed case, numbers, symbols)
3. **Rotate API tokens** periodically
4. **Monitor logs** for failed authentication attempts
5. **Keep firmware updated** for security patches
6. **Use HTTPS** for all connections (enabled by default)

## Threat Model

SmartHub is designed for **local network deployment**. It protects against:

- Eavesdropping (TLS encryption)
- Password theft (secure hashing)
- Session hijacking (HttpOnly, Secure cookies)
- Brute force attacks (rate limiting)
- XSS/Clickjacking (security headers)

**Not in scope** (local trusted network assumed):
- Network-level attacks (use firewall)
- Physical device access
- Supply chain attacks

## Testing

Security tests are located in:
- `app/tests/security/test_security.cpp` - Unit tests (90 tests)
- `app/tests/web/test_webserver.cpp` - Integration tests (25 tests)

Run tests:
```bash
cd app/build
./tests/test_security
./tests/test_webserver
```
