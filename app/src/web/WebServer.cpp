/**
 * SmartHub Web Server Implementation
 *
 * Uses Mongoose for HTTP/HTTPS serving.
 * Mongoose source must be placed in third_party/mongoose/
 */

#include <smarthub/web/WebServer.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/devices/Device.hpp>
#include <smarthub/core/Logger.hpp>

// Check if mongoose is available
#if __has_include("mongoose/mongoose.h")
#include "mongoose/mongoose.h"
#define HAVE_MONGOOSE 1
#else
#define HAVE_MONGOOSE 0
// Stub types for compilation
struct mg_mgr {};
struct mg_connection {};
struct mg_http_message {};
#endif

namespace smarthub {

#if HAVE_MONGOOSE

static WebServer* s_instance = nullptr;

WebServer::WebServer(EventBus& eventBus, DeviceManager& deviceManager,
                     int port, const std::string& webRoot)
    : m_eventBus(eventBus)
    , m_deviceManager(deviceManager)
    , m_port(port)
    , m_webRoot(webRoot)
{
    s_instance = this;
}

WebServer::~WebServer() {
    stop();
    s_instance = nullptr;
}

bool WebServer::start() {
    m_mgr = new struct mg_mgr;
    mg_mgr_init(m_mgr);

    char addr[64];
    std::snprintf(addr, sizeof(addr), "http://0.0.0.0:%d", m_port);

    struct mg_connection* c = mg_http_listen(m_mgr, addr, eventHandler, nullptr);
    if (!c) {
        LOG_ERROR("Failed to start web server on %s", addr);
        delete m_mgr;
        m_mgr = nullptr;
        return false;
    }

    m_running = true;

    // Start server thread
    m_thread = std::thread([this]() {
        while (m_running) {
            mg_mgr_poll(m_mgr, 100);
        }
    });

    LOG_INFO("Web server started on %s", addr);
    return true;
}

void WebServer::stop() {
    m_running = false;

    if (m_thread.joinable()) {
        m_thread.join();
    }

    if (m_mgr) {
        mg_mgr_free(m_mgr);
        delete m_mgr;
        m_mgr = nullptr;
    }
}

void WebServer::setTlsCert(const std::string& certPath, const std::string& keyPath) {
    m_certPath = certPath;
    m_keyPath = keyPath;
}

void WebServer::setHttpRedirect(bool enable, int httpPort) {
    m_httpRedirect = enable;
    m_httpPort = httpPort;
}

void WebServer::eventHandler(struct mg_connection* c, int ev, void* ev_data) {
    if (!s_instance) return;

    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message* hm = static_cast<struct mg_http_message*>(ev_data);
        s_instance->handleHttpRequest(c, hm);
    }
}

void WebServer::handleHttpRequest(struct mg_connection* c, struct mg_http_message* hm) {
    std::string uri(hm->uri.buf, hm->uri.len);

    // API routes
    if (uri.rfind("/api/", 0) == 0) {
        if (uri == "/api/devices" && mg_strcmp(hm->method, mg_str("GET")) == 0) {
            apiGetDevices(c);
        } else if (uri == "/api/system/status") {
            apiGetSystemStatus(c);
        } else if (uri.rfind("/api/devices/", 0) == 0) {
            std::string id = uri.substr(13);
            if (mg_strcmp(hm->method, mg_str("GET")) == 0) {
                apiGetDevice(c, id);
            } else if (mg_strcmp(hm->method, mg_str("PUT")) == 0) {
                apiSetDeviceState(c, id, hm);
            }
        } else {
            sendError(c, 404, "Not found");
        }
    }
    // Static files
    else {
        struct mg_http_serve_opts opts = {};
        opts.root_dir = m_webRoot.c_str();
        mg_http_serve_dir(c, hm, &opts);
    }
}

void WebServer::handleWebSocket(struct mg_connection* /*c*/, void* /*ev_data*/) {
    // WebSocket handling - to be implemented
}

void WebServer::apiGetDevices(struct mg_connection* c) {
    // Build JSON response
    std::string json = "[";
    auto devices = m_deviceManager.getAllDevices();
    bool first = true;

    for (const auto& device : devices) {
        if (!first) json += ",";
        first = false;

        json += "{";
        json += "\"id\":\"" + device->id() + "\",";
        json += "\"name\":\"" + device->name() + "\",";
        json += "\"type\":\"" + device->typeString() + "\",";
        json += "\"online\":" + std::string(device->isAvailable() ? "true" : "false");
        json += "}";
    }

    json += "]";
    sendJson(c, 200, json);
}

void WebServer::apiGetDevice(struct mg_connection* c, const std::string& id) {
    auto device = m_deviceManager.getDevice(id);
    if (!device) {
        sendError(c, 404, "Device not found");
        return;
    }

    std::string json = "{";
    json += "\"id\":\"" + device->id() + "\",";
    json += "\"name\":\"" + device->name() + "\",";
    json += "\"type\":\"" + device->typeString() + "\",";
    json += "\"room\":\"" + device->room() + "\",";
    json += "\"online\":" + std::string(device->isAvailable() ? "true" : "false");
    json += "}";

    sendJson(c, 200, json);
}

void WebServer::apiSetDeviceState(struct mg_connection* c, const std::string& id,
                                   struct mg_http_message* hm) {
    auto device = m_deviceManager.getDevice(id);
    if (!device) {
        sendError(c, 404, "Device not found");
        return;
    }

    // Parse JSON body and update device state
    // Simplified - real implementation would use proper JSON parsing
    std::string body(hm->body.buf, hm->body.len);
    LOG_DEBUG("Set device state: %s = %s", id.c_str(), body.c_str());

    sendJson(c, 200, "{\"success\":true}");
}

void WebServer::apiGetSensorHistory(struct mg_connection* c, const std::string& /*id*/,
                                     struct mg_http_message* /*hm*/) {
    // TODO: Query database for sensor history
    sendJson(c, 200, "[]");
}

void WebServer::apiGetSystemStatus(struct mg_connection* c) {
    std::string json = "{";
    json += "\"version\":\"0.1.0\",";
    json += "\"uptime\":" + std::to_string(0) + ",";
    json += "\"devices\":" + std::to_string(m_deviceManager.deviceCount());
    json += "}";

    sendJson(c, 200, json);
}

void WebServer::sendJson(struct mg_connection* c, int status, const std::string& json) {
    mg_http_reply(c, status, "Content-Type: application/json\r\n", "%s", json.c_str());
}

void WebServer::sendError(struct mg_connection* c, int status, const std::string& message) {
    std::string json = "{\"error\":\"" + message + "\"}";
    sendJson(c, status, json);
}

#else // !HAVE_MONGOOSE

// Stub implementation when mongoose is not available
WebServer::WebServer(EventBus& eventBus, DeviceManager& deviceManager,
                     int port, const std::string& webRoot)
    : m_eventBus(eventBus)
    , m_deviceManager(deviceManager)
    , m_port(port)
    , m_webRoot(webRoot)
{
    LOG_WARN("Web server support not compiled in (mongoose not found)");
    LOG_WARN("Download mongoose.c and mongoose.h to third_party/mongoose/");
}

WebServer::~WebServer() {}

bool WebServer::start() {
    LOG_WARN("Web server disabled - mongoose library not found");
    return true; // Return true to not block application startup
}

void WebServer::stop() {}
void WebServer::setTlsCert(const std::string&, const std::string&) {}
void WebServer::setHttpRedirect(bool, int) {}
void WebServer::eventHandler(struct mg_connection*, int, void*) {}
void WebServer::handleHttpRequest(struct mg_connection*, struct mg_http_message*) {}
void WebServer::handleWebSocket(struct mg_connection*, void*) {}
void WebServer::apiGetDevices(struct mg_connection*) {}
void WebServer::apiGetDevice(struct mg_connection*, const std::string&) {}
void WebServer::apiSetDeviceState(struct mg_connection*, const std::string&, struct mg_http_message*) {}
void WebServer::apiGetSensorHistory(struct mg_connection*, const std::string&, struct mg_http_message*) {}
void WebServer::apiGetSystemStatus(struct mg_connection*) {}
void WebServer::sendJson(struct mg_connection*, int, const std::string&) {}
void WebServer::sendError(struct mg_connection*, int, const std::string&) {}

#endif // HAVE_MONGOOSE

} // namespace smarthub
