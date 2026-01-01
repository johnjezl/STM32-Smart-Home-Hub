/**
 * SmartHub Session Manager Implementation
 */

#include <smarthub/security/SessionManager.hpp>
#include <smarthub/core/Logger.hpp>

#include <algorithm>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

// OpenSSL for secure random
#if __has_include(<openssl/rand.h>)
#include <openssl/rand.h>
#define HAVE_OPENSSL 1
#else
#define HAVE_OPENSSL 0
#endif

namespace smarthub::security {

SessionManager::SessionManager(int sessionTimeoutMinutes, int maxSessionsPerUser)
    : m_timeoutMinutes(sessionTimeoutMinutes)
    , m_maxSessionsPerUser(maxSessionsPerUser)
{
}

std::string SessionManager::createSession(int userId,
                                           const std::string& username,
                                           const std::string& role,
                                           const std::string& ipAddress,
                                           const std::string& userAgent) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Enforce max sessions per user
    enforceMaxSessions(username);

    // Generate token
    std::string token = generateToken();

    // Get current time
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    // Calculate expiration
    auto expiresAt = timestamp + (m_timeoutMinutes * 60);

    // Create session
    Session session;
    session.token = token;
    session.userId = userId;
    session.username = username;
    session.role = role;
    session.createdAt = static_cast<uint64_t>(timestamp);
    session.expiresAt = static_cast<uint64_t>(expiresAt);
    session.lastActivity = static_cast<uint64_t>(timestamp);
    session.ipAddress = ipAddress;
    session.userAgent = userAgent;

    m_sessions[token] = session;

    LOG_DEBUG("Session created for user %s (expires in %d minutes)",
              username.c_str(), m_timeoutMinutes);

    return token;
}

std::optional<Session> SessionManager::validateSession(const std::string& token) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_sessions.find(token);
    if (it == m_sessions.end()) {
        return std::nullopt;
    }

    // Check expiration
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    if (static_cast<uint64_t>(timestamp) >= it->second.expiresAt) {
        // Session expired
        m_sessions.erase(it);
        return std::nullopt;
    }

    return it->second;
}

bool SessionManager::touchSession(const std::string& token) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_sessions.find(token);
    if (it == m_sessions.end()) {
        return false;
    }

    // Update last activity and extend expiration
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    it->second.lastActivity = static_cast<uint64_t>(timestamp);
    it->second.expiresAt = static_cast<uint64_t>(timestamp + (m_timeoutMinutes * 60));

    return true;
}

void SessionManager::destroySession(const std::string& token) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_sessions.find(token);
    if (it != m_sessions.end()) {
        LOG_DEBUG("Session destroyed for user %s", it->second.username.c_str());
        m_sessions.erase(it);
    }
}

void SessionManager::destroyUserSessions(const std::string& username) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto it = m_sessions.begin(); it != m_sessions.end(); ) {
        if (it->second.username == username) {
            it = m_sessions.erase(it);
        } else {
            ++it;
        }
    }

    LOG_DEBUG("All sessions destroyed for user %s", username.c_str());
}

std::vector<Session> SessionManager::getUserSessions(const std::string& username) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<Session> result;
    for (const auto& [token, session] : m_sessions) {
        if (session.username == username) {
            // Copy session but hide token
            Session s = session;
            s.token = "***";
            result.push_back(s);
        }
    }

    return result;
}

void SessionManager::cleanup() {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    size_t removed = 0;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ) {
        if (static_cast<uint64_t>(timestamp) >= it->second.expiresAt) {
            it = m_sessions.erase(it);
            removed++;
        } else {
            ++it;
        }
    }

    if (removed > 0) {
        LOG_DEBUG("Cleaned up %zu expired sessions", removed);
    }
}

size_t SessionManager::sessionCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sessions.size();
}

std::string SessionManager::generateToken() {
#if HAVE_OPENSSL
    // Generate 32 bytes (256 bits) of random data
    unsigned char bytes[32];
    if (RAND_bytes(bytes, sizeof(bytes)) != 1) {
        LOG_ERROR("Failed to generate random token");
        // Fallback to less secure method
        goto fallback;
    }

    {
        // Convert to hex string
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (unsigned char byte : bytes) {
            ss << std::setw(2) << static_cast<int>(byte);
        }
        return ss.str();
    }

fallback:
#endif
    // Fallback: use random_device (less cryptographically secure)
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(16) << dis(gen);
    ss << std::setw(16) << dis(gen);
    ss << std::setw(16) << dis(gen);
    ss << std::setw(16) << dis(gen);

    return ss.str();
}

void SessionManager::enforceMaxSessions(const std::string& username) {
    // Count sessions for user
    std::vector<std::pair<uint64_t, std::string>> userSessions;  // (createdAt, token)

    for (const auto& [token, session] : m_sessions) {
        if (session.username == username) {
            userSessions.emplace_back(session.createdAt, token);
        }
    }

    // If over limit, remove oldest sessions
    if (static_cast<int>(userSessions.size()) >= m_maxSessionsPerUser) {
        // Sort by creation time (oldest first)
        std::sort(userSessions.begin(), userSessions.end());

        // Remove oldest sessions to make room
        int toRemove = static_cast<int>(userSessions.size()) - m_maxSessionsPerUser + 1;
        for (int i = 0; i < toRemove; i++) {
            m_sessions.erase(userSessions[static_cast<size_t>(i)].second);
        }

        LOG_DEBUG("Removed %d old sessions for user %s", toRemove, username.c_str());
    }
}

} // namespace smarthub::security
