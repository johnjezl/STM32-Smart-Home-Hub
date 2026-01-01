/**
 * SmartHub Session Manager
 *
 * Manages user sessions for web authentication.
 */
#pragma once

#include <string>
#include <optional>
#include <map>
#include <mutex>
#include <vector>
#include <cstdint>

namespace smarthub::security {

/**
 * Session information
 */
struct Session {
    std::string token;
    std::string username;
    std::string role;
    int userId = 0;
    uint64_t createdAt = 0;
    uint64_t expiresAt = 0;
    uint64_t lastActivity = 0;
    std::string ipAddress;
    std::string userAgent;
};

/**
 * Session Manager
 *
 * Handles session creation, validation, and cleanup.
 * Sessions are stored in memory and expire after a configurable timeout.
 */
class SessionManager {
public:
    /**
     * Constructor
     * @param sessionTimeoutMinutes Session expiration time in minutes (default 60)
     * @param maxSessionsPerUser Maximum concurrent sessions per user (default 5)
     */
    explicit SessionManager(int sessionTimeoutMinutes = 60,
                            int maxSessionsPerUser = 5);

    ~SessionManager() = default;

    // Non-copyable
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    /**
     * Create a new session after successful login
     * @param userId User ID
     * @param username Username
     * @param role User role
     * @param ipAddress Client IP address
     * @param userAgent Client user agent string
     * @return Session token
     */
    std::string createSession(int userId,
                               const std::string& username,
                               const std::string& role,
                               const std::string& ipAddress = "",
                               const std::string& userAgent = "");

    /**
     * Validate session token
     * @param token Session token
     * @return Session if valid
     */
    std::optional<Session> validateSession(const std::string& token);

    /**
     * Update session activity (extends expiration)
     * @param token Session token
     * @return true if session was updated
     */
    bool touchSession(const std::string& token);

    /**
     * Destroy session (logout)
     * @param token Session token
     */
    void destroySession(const std::string& token);

    /**
     * Destroy all sessions for a user
     * @param username Username
     */
    void destroyUserSessions(const std::string& username);

    /**
     * Get all sessions for a user
     * @param username Username
     * @return List of sessions (tokens hidden)
     */
    std::vector<Session> getUserSessions(const std::string& username);

    /**
     * Cleanup expired sessions
     * Should be called periodically
     */
    void cleanup();

    /**
     * Get number of active sessions
     */
    size_t sessionCount() const;

    /**
     * Get session timeout in minutes
     */
    int getTimeoutMinutes() const { return m_timeoutMinutes; }

    /**
     * Set session timeout in minutes
     */
    void setTimeoutMinutes(int minutes) { m_timeoutMinutes = minutes; }

private:
    /**
     * Generate cryptographically secure session token
     */
    std::string generateToken();

    /**
     * Remove oldest sessions if user exceeds limit
     */
    void enforceMaxSessions(const std::string& username);

    std::map<std::string, Session> m_sessions;
    mutable std::mutex m_mutex;
    int m_timeoutMinutes;
    int m_maxSessionsPerUser;
};

} // namespace smarthub::security
