# SmartHub Security Architecture

This document describes the security features implemented in SmartHub for protecting user data and securing network communications.

## Overview

SmartHub implements multiple layers of security appropriate for a local network IoT hub:

- **Transport Security**: TLS 1.2+ for HTTPS connections
- **Authentication**: Session-based web authentication and API tokens
- **Authorization**: Role-based access control (admin/user)
- **Password Security**: PBKDF2-SHA256 password hashing
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
- PBKDF2-SHA256 with 100,000 iterations
- Unique random salt per password
- Constant-time comparison

**Password rules:** 8 to 128 characters, with an uppercase letter, a lowercase letter and a digit.

**User Roles:**

| Action | `admin` | `user` |
|--------|---------|--------|
| View devices, rooms and automations | Yes | Yes |
| Control a device (`PUT /api/devices/{id}`) | Yes | Yes |
| Write a sensor reading or other reported state | No | No |
| Add, edit or remove devices and rooms | Yes | No |
| Zigbee pairing | Yes | No |
| Add, edit, enable or remove automations | Yes | No |

The server enforces these rules: any request that changes something needs `admin` unless it is listed as open to `user`, so new routes start out admin-only. The dashboard also hides the controls a user can't use.

### Session Management (`SessionManager`)

Manages web authentication sessions.

```cpp
#include <smarthub/security/SessionManager.hpp>

SessionManager sessionMgr(60);  // Expires after 60 idle minutes

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
- Sessions are held in memory, so restarting the hub logs everyone out
- Each authenticated request restarts the idle timeout. An open dashboard polls, so it counts as activity and stays logged in, which suits a wall-mounted tablet
- Disabling or removing an account, or changing its role, takes effect on that user's next request
- At most 5 sessions per user; a new login ends the least recently used
- Logout ends only that browser's session

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

### First-Run Setup

Web authentication is always required, including on the local network. A new hub has no accounts and no default password, so the dashboard opens on a screen that creates the administrator (`POST /api/setup`).

Setup closes for good as soon as any account exists, even a disabled one. Until then, **anyone who can reach the hub on the network can claim it**, so create the administrator as soon as the hub is on the network. The hub logs a warning at startup while no account exists.

Setup and login only accept a request addressed to an IP address, a name with no dots, or a name ending in `.local`, `.localdomain`, `.lan`, `.home`, `.internal` or `.home.arpa`. This stops a web page on another site from using your browser to reach the hub, by pointing its own domain at it (DNS rebinding). To use another name, such as `hub.fritz.box`, add it to `allowed_hosts` in the configuration.

`SetupManager` is a separate class for a fuller setup that also generates certificates and initializes the credential store. The application doesn't use it yet.

### Managing Accounts

The dashboard's **Account** tab has everything here, and the [API](api.md#account-endpoints) has the same.

- **Everyone** can change their own password and manage their own API tokens.
  - Changing your password needs the current one. A wrong one counts as a failed login. It ends your other sessions and revokes your API tokens, as whoever knew the old password may have made some.
  - A token is shown once, when it's made. It acts as you, with your role. Tokens can't be made or revoked with a token, only when signed in, so one that leaks can't be used to make more. You can have up to 20.
- **Administrators** can also add accounts, change roles, disable and enable accounts, set someone's password and remove accounts.
  - There's always at least one enabled administrator: the last one can't be demoted, disabled or removed.
  - Setting someone's password ends their sessions and revokes their tokens. Passwords can only be set when signed in, not with a token.
  - Disabling an account ends its sessions. Its tokens are refused while it's disabled and work again if it's enabled.
  - Removing an account revokes its tokens and ends its sessions.

An open dashboard checks your role with each refresh, so a role change shows within a few seconds.

### Recovering a Lost Administrator Password

This needs a shell on the hub, which is what keeps it safe. Run:

```bash
smarthub --recover-admin <username>
```

It asks for a new password twice, or reads one line from stdin if that isn't a terminal. The account becomes an enabled administrator with that password, and is created if it doesn't exist. Use `-c` first if the hub uses a configuration file other than `/etc/smarthub/config.yaml`.

The running hub doesn't need restarting, as it reads accounts on every request.

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

Requests without valid credentials get `401`. Requests from a user without the required role get `403`.

**Public Routes** (no auth required):
- `/api/auth/login` and `/api/auth/logout`
- `/api/system/status`
- `/api/setup` and `/api/setup/status` (setup refuses to run once an account exists)
- Static files (non-API routes)

A session or API token acts as its owner, whose role and enabled state are checked on every request.

The cookie is marked `Secure` only on a TLS connection, because a browser would discard it on plain HTTP.

### Rate Limiting

Two limits apply per client IP:

| Limit | Default | Config key | When exceeded |
|-------|---------|------------|---------------|
| All requests | 600 per minute | `rate_limit_per_minute` | `429` |
| Failed logins | 10 per 5 minutes | `login_max_failures`, `login_failure_window_seconds` | `429` with `Retry-After` |

An open dashboard makes 48 requests a minute, whatever the number of devices. The login limit is what stops password guessing: once it is reached, logins from that IP are refused without checking the password, even a correct one, until the window ends. A successful login doesn't reset the count. Setting either limit to 0 disables it.

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
  -d '{"username":"admin","password":"Your-Password-1"}'
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
curl -X POST -d '' https://smarthub.local/api/auth/logout \
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
  rate_limit_per_minute: 600
  login_max_failures: 10
  login_failure_window_seconds: 300
  allowed_hosts: []
```

## Security Best Practices

1. **Create the administrator** as soon as the hub is on the network
2. **Use strong passwords** (12+ characters, mixed case, numbers, symbols)
3. **Rotate API tokens** periodically
4. **Monitor logs** for failed authentication attempts
5. **Keep firmware updated** for security patches
6. **Use HTTPS** for all connections (enabled by default)

## Threat Model

SmartHub is designed for **local network deployment**, but doesn't treat that network as trusted: every API request needs authentication. It protects against:

- Eavesdropping (TLS encryption)
- Password theft (secure hashing)
- Session hijacking (HttpOnly, Secure cookies)
- Password guessing (failed login limit)
- XSS/Clickjacking (security headers)

**TLS:** the hub creates a self-signed certificate on first boot if the certificate doesn't exist. If one exists but can't be used, the web server doesn't start, rather than falling back to plain HTTP, and the error is in the log. Automations and the touchscreen keep working. Setting both paths to empty in the config is the only way to run without TLS.

The self-signed certificate names `smarthub.local` and `localhost`, so a browser reaching the hub by IP address still warns after it's trusted. It runs until 2099, which Apple devices don't accept as trusted even if it's installed by hand, so they keep showing a warning.

**Not in scope**:
- Network-level attacks (use firewall)
- Physical device access
- Supply chain attacks

## Testing

Security tests are located in:
- `app/tests/security/test_security.cpp` - Unit tests (90 tests)
- `app/tests/web/test_webserver.cpp` - Web server integration tests
- `app/tests/web/test_web_auth.cpp` - Login, roles and rate limits, end to end against the real `Application`

Run tests:
```bash
cd app/build
./tests/test_security
./tests/test_webserver
./tests/test_web_auth
```
