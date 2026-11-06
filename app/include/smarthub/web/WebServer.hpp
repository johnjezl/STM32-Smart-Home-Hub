/**
 * SmartHub Web Server
 *
 * HTTPS web server for REST API and web UI.
 * Uses Mongoose as the underlying HTTP library.
 */
#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <set>
#include <vector>
#include <cstdint>

#include <smarthub/security/UserManager.hpp>

// Forward declaration
struct mg_mgr;
struct mg_connection;
struct mg_http_message;

namespace smarthub {

class EventBus;
class DeviceManager;
class AutomationManager;

namespace zigbee {
class ZigbeeHandler;
}

namespace security {
class SessionManager;
class ApiTokenManager;
class UserManager;
struct User;
}

/**
 * Authenticated user info from session or API token
 */
struct AuthInfo {
    bool authenticated = false;
    int userId = 0;
    std::string username;
    std::string role;
    bool isApiToken = false;
};

/**
 * Rate limit entry per IP
 */
struct RateLimitEntry {
    uint64_t windowStart = 0;
    int requestCount = 0;
};

/**
 * Web server with REST API
 */
class WebServer {
public:
    WebServer(EventBus& eventBus, DeviceManager& deviceManager,
              int port, const std::string& webRoot);
    ~WebServer();

    // Non-copyable
    WebServer(const WebServer&) = delete;
    WebServer& operator=(const WebServer&) = delete;

    /**
     * Start the web server
     */
    bool start();

    /**
     * Stop the web server
     */
    void stop();

    /**
     * Check if server is running
     */
    bool isRunning() const { return m_running; }

    /**
     * Set TLS certificate paths
     */
    void setTlsCert(const std::string& certPath, const std::string& keyPath);

    /**
     * Enable/disable HTTP to HTTPS redirect
     */
    void setHttpRedirect(bool enable, int httpPort = 80);

    /**
     * Set security managers for authentication.
     * Without them, login fails and every protected route answers 401.
     */
    void setSecurityManagers(security::SessionManager* sessionMgr,
                             security::ApiTokenManager* tokenMgr,
                             security::UserManager* userMgr);

    /**
     * Set automation manager for automation API
     */
    void setAutomationManager(AutomationManager* automationMgr);

    /**
     * Set Zigbee handler for pairing API
     */
    void setZigbeeHandler(zigbee::ZigbeeHandler* zigbeeHandler);

    /**
     * Configure rate limiting
     * @param requestsPerMinute Max requests per minute per IP (0 = disabled)
     */
    void setRateLimit(int requestsPerMinute);

    /**
     * Configure the limit on failed logins from one client IP
     * @param maxFailures Failures allowed per window (0 = disabled)
     * @param windowSeconds Window length; logins are refused until it ends
     */
    void setLoginThrottle(int maxFailures, int windowSeconds);

    /**
     * Add hostnames that login and first-run setup accept, beyond IP addresses
     * and local names. Only for names you control: see isAllowedHostHeader.
     */
    void setAllowedHosts(const std::vector<std::string>& hosts);

    /**
     * List of routes that don't require authentication
     */
    void setPublicRoutes(const std::vector<std::string>& routes);

private:
    static void eventHandler(struct mg_connection* c, int ev, void* ev_data);
    static void httpRedirectHandler(struct mg_connection* c, int ev, void* ev_data);

    void handleHttpRequest(struct mg_connection* c, struct mg_http_message* hm);
    void handleWebSocket(struct mg_connection* c, void* ev_data);

    // Authentication & security
    AuthInfo checkAuth(struct mg_http_message* hm);
    bool checkRateLimit(const std::string& ip);
    int loginRetryAfterSeconds(const std::string& ip);
    void recordLoginFailure(const std::string& ip);
    bool isPublicRoute(const std::string& uri) const;
    static bool requiresAdmin(const std::string& method, const std::string& uri);
    void startSession(struct mg_connection* c, const security::User& user, int status);
    std::string getClientIp(struct mg_connection* c) const;
    void addSecurityHeaders(std::string& headers);

    // API handlers
    void apiGetDevices(struct mg_connection* c);
    void apiGetDevice(struct mg_connection* c, const std::string& id);
    void apiSetDeviceState(struct mg_connection* c, const std::string& id,
                           struct mg_http_message* hm);
    void apiGetSensorHistory(struct mg_connection* c, const std::string& id,
                             struct mg_http_message* hm);
    void apiGetSystemStatus(struct mg_connection* c);

    // Auth API handlers
    void apiLogin(struct mg_connection* c, struct mg_http_message* hm);
    void apiLogout(struct mg_connection* c, struct mg_http_message* hm);
    void apiGetCurrentUser(struct mg_connection* c, const AuthInfo& auth);
    void apiGetSetupStatus(struct mg_connection* c);
    void apiSetup(struct mg_connection* c, struct mg_http_message* hm);

    // Room API handlers
    void apiGetRooms(struct mg_connection* c);
    void apiCreateRoom(struct mg_connection* c, struct mg_http_message* hm);
    void apiUpdateRoom(struct mg_connection* c, const std::string& id,
                       struct mg_http_message* hm);
    void apiDeleteRoom(struct mg_connection* c, const std::string& id);

    // Device CRUD API handlers
    void apiCreateDevice(struct mg_connection* c, struct mg_http_message* hm);
    void apiUpdateDeviceSettings(struct mg_connection* c, const std::string& id,
                                  struct mg_http_message* hm);
    void apiDeleteDevice(struct mg_connection* c, const std::string& id);

    // Zigbee pairing API handlers
    void apiZigbeePermitJoin(struct mg_connection* c, struct mg_http_message* hm);
    void apiZigbeeStopPermitJoin(struct mg_connection* c);
    void apiGetPendingDevices(struct mg_connection* c);
    void apiAddZigbeeDevice(struct mg_connection* c, struct mg_http_message* hm);

    // Automation API handlers
    void apiGetAutomations(struct mg_connection* c);
    void apiCreateAutomation(struct mg_connection* c, struct mg_http_message* hm);
    void apiGetAutomation(struct mg_connection* c, const std::string& id);
    void apiUpdateAutomation(struct mg_connection* c, const std::string& id,
                             struct mg_http_message* hm);
    void apiDeleteAutomation(struct mg_connection* c, const std::string& id);
    void apiSetAutomationEnabled(struct mg_connection* c, const std::string& id,
                                  struct mg_http_message* hm);

    void sendJson(struct mg_connection* c, int status, const std::string& json);
    void sendJsonWithHeaders(struct mg_connection* c, int status,
                             const std::string& json, const std::string& extraHeaders = "");
    void sendError(struct mg_connection* c, int status, const std::string& message);

    EventBus& m_eventBus;
    DeviceManager& m_deviceManager;
    int m_port;
    std::string m_webRoot;
    std::string m_certPath;
    std::string m_keyPath;
    bool m_httpRedirect = false;
    int m_httpPort = 80;

    // Security
    security::SessionManager* m_sessionMgr = nullptr;
    security::ApiTokenManager* m_tokenMgr = nullptr;
    security::UserManager* m_userMgr = nullptr;
    int m_rateLimitPerMinute = 0;
    int m_loginMaxFailures = 0;
    int m_loginFailureWindowSeconds = 0;
    std::vector<std::string> m_allowedHosts;
    std::vector<std::string> m_publicRoutes;

    // Additional managers
    AutomationManager* m_automationMgr = nullptr;
    zigbee::ZigbeeHandler* m_zigbeeHandler = nullptr;

    // Rate limiting state
    std::unordered_map<std::string, RateLimitEntry> m_rateLimits;
    std::unordered_map<std::string, RateLimitEntry> m_loginFailures;
    std::mutex m_rateLimitMutex;

    struct mg_mgr* m_mgr = nullptr;
    std::thread m_thread;

    // Hashing a password takes over a second on the board, so it happens on its own thread, and
    // the connection gets its answer when that's done.
    struct PasswordJob {
        unsigned long connectionId = 0;
        bool isSetup = false;
        std::string username;
        std::string password;
        std::string clientIp;
        bool closeAfterReply = false;
    };
    struct PasswordResult {
        unsigned long connectionId = 0;
        std::string clientIp;
        bool isLogin = false;
        std::optional<security::User> user;
        int status = 0;  // For a response without a session
        bool closeAfterReply = false;
        std::string error;
    };
    void queuePasswordJob(struct mg_connection* c, PasswordJob job);
    void passwordWorkerLoop();
    PasswordResult runPasswordJob(const PasswordJob& job);
    void sendPasswordResults();

    std::thread m_passwordWorker;
    std::mutex m_passwordMutex;
    std::condition_variable m_passwordWake;
    std::deque<PasswordJob> m_passwordJobs;
    std::set<std::string> m_passwordJobsByIp;  // Queued, or waiting for their failure to be counted
    std::vector<PasswordResult> m_passwordResults;
    bool m_passwordWorkerStopping = false;
    std::atomic<bool> m_running{false};
};

} // namespace smarthub
