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
#include <mutex>
#include <vector>
#include <cstdint>

// Forward declaration
struct mg_mgr;
struct mg_connection;
struct mg_http_message;

namespace smarthub {

class EventBus;
class DeviceManager;

namespace security {
class SessionManager;
class ApiTokenManager;
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
     * Set security managers for authentication
     */
    void setSecurityManagers(security::SessionManager* sessionMgr,
                             security::ApiTokenManager* tokenMgr);

    /**
     * Configure rate limiting
     * @param requestsPerMinute Max requests per minute per IP (0 = disabled)
     */
    void setRateLimit(int requestsPerMinute);

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
    bool isPublicRoute(const std::string& uri) const;
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
    void apiLogout(struct mg_connection* c, const AuthInfo& auth);
    void apiGetCurrentUser(struct mg_connection* c, const AuthInfo& auth);

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
    int m_rateLimitPerMinute = 0;
    std::vector<std::string> m_publicRoutes;

    // Rate limiting state
    std::unordered_map<std::string, RateLimitEntry> m_rateLimits;
    std::mutex m_rateLimitMutex;

    struct mg_mgr* m_mgr = nullptr;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
};

} // namespace smarthub
