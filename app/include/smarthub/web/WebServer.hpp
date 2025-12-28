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

// Forward declaration
struct mg_mgr;
struct mg_connection;
struct mg_http_message;

namespace smarthub {

class EventBus;
class DeviceManager;

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

private:
    static void eventHandler(struct mg_connection* c, int ev, void* ev_data);

    void handleHttpRequest(struct mg_connection* c, struct mg_http_message* hm);
    void handleWebSocket(struct mg_connection* c, void* ev_data);

    // API handlers
    void apiGetDevices(struct mg_connection* c);
    void apiGetDevice(struct mg_connection* c, const std::string& id);
    void apiSetDeviceState(struct mg_connection* c, const std::string& id,
                           struct mg_http_message* hm);
    void apiGetSensorHistory(struct mg_connection* c, const std::string& id,
                             struct mg_http_message* hm);
    void apiGetSystemStatus(struct mg_connection* c);

    void sendJson(struct mg_connection* c, int status, const std::string& json);
    void sendError(struct mg_connection* c, int status, const std::string& message);

    EventBus& m_eventBus;
    DeviceManager& m_deviceManager;
    int m_port;
    std::string m_webRoot;
    std::string m_certPath;
    std::string m_keyPath;
    bool m_httpRedirect = false;
    int m_httpPort = 80;

    struct mg_mgr* m_mgr = nullptr;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
};

} // namespace smarthub
