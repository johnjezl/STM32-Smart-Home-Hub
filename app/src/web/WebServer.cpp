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
#include <smarthub/devices/DeviceTypeRegistry.hpp>
#include <smarthub/automation/AutomationManager.hpp>
#include <smarthub/automation/Automation.hpp>
#include <smarthub/protocols/zigbee/ZigbeeHandler.hpp>
#include <smarthub/security/SessionManager.hpp>
#include <smarthub/security/ApiTokenManager.hpp>
#include <smarthub/core/Logger.hpp>
#include <nlohmann/json.hpp>

#include <chrono>
#include <algorithm>
#include <fstream>
#include <sstream>

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

// Static TLS options and cert data - must persist for lifetime of connections
static struct mg_tls_opts s_tls_opts = {};
static std::string s_certData;
static std::string s_keyData;

// Helper to read file contents
static std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return "";
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

bool WebServer::start() {
    m_mgr = new struct mg_mgr;
    mg_mgr_init(m_mgr);

    char addr[128];

    // Check if TLS is configured
    bool useTls = !m_certPath.empty() && !m_keyPath.empty();

    if (useTls) {
        // Read certificate and key file contents
        s_certData = readFile(m_certPath);
        s_keyData = readFile(m_keyPath);

        if (s_certData.empty() || s_keyData.empty()) {
            LOG_ERROR("Failed to read TLS cert/key files");
            useTls = false;
        } else {
            std::snprintf(addr, sizeof(addr), "https://0.0.0.0:%d", m_port);
            s_tls_opts.cert = mg_str_n(s_certData.c_str(), s_certData.size());
            s_tls_opts.key = mg_str_n(s_keyData.c_str(), s_keyData.size());
            LOG_INFO("TLS enabled with cert: %s (%zu bytes)", m_certPath.c_str(), s_certData.size());
        }
    }

    if (!useTls) {
        std::snprintf(addr, sizeof(addr), "http://0.0.0.0:%d", m_port);
        LOG_WARN("TLS not configured - running HTTP only (insecure)");
    }

    struct mg_connection* c = mg_http_listen(m_mgr, addr, eventHandler,
                                              useTls ? &s_tls_opts : nullptr);
    if (!c) {
        LOG_ERROR("Failed to start web server on %s", addr);
        delete m_mgr;
        m_mgr = nullptr;
        return false;
    }

    // Start HTTP redirect listener if enabled
    if (useTls && m_httpRedirect && m_httpPort != m_port) {
        char httpAddr[64];
        std::snprintf(httpAddr, sizeof(httpAddr), "http://0.0.0.0:%d", m_httpPort);
        struct mg_connection* httpC = mg_http_listen(m_mgr, httpAddr,
                                                      httpRedirectHandler, this);
        if (httpC) {
            LOG_INFO("HTTP redirect enabled on port %d", m_httpPort);
        } else {
            LOG_WARN("Failed to start HTTP redirect listener on port %d", m_httpPort);
        }
    }

    // Additional HTTP listener on port 4321 for development/testing
    {
        char devAddr[64];
        std::snprintf(devAddr, sizeof(devAddr), "http://0.0.0.0:4321");
        struct mg_connection* devC = mg_http_listen(m_mgr, devAddr, eventHandler, nullptr);
        if (devC) {
            LOG_INFO("Additional HTTP listener on port 4321");
        } else {
            LOG_WARN("Failed to start listener on port 4321");
        }
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

void WebServer::setSecurityManagers(security::SessionManager* sessionMgr,
                                     security::ApiTokenManager* tokenMgr) {
    m_sessionMgr = sessionMgr;
    m_tokenMgr = tokenMgr;
}

void WebServer::setRateLimit(int requestsPerMinute) {
    m_rateLimitPerMinute = requestsPerMinute;
}

void WebServer::setAutomationManager(AutomationManager* automationMgr) {
    m_automationMgr = automationMgr;
}

void WebServer::setZigbeeHandler(zigbee::ZigbeeHandler* zigbeeHandler) {
    m_zigbeeHandler = zigbeeHandler;
}

void WebServer::setPublicRoutes(const std::vector<std::string>& routes) {
    m_publicRoutes = routes;
}

AuthInfo WebServer::checkAuth(struct mg_http_message* hm) {
    AuthInfo auth;

    // Check Authorization header for Bearer token (API token)
    struct mg_str* authHeader = mg_http_get_header(hm, "Authorization");
    if (authHeader && authHeader->len > 7) {
        std::string headerVal(authHeader->buf, authHeader->len);
        if (headerVal.rfind("Bearer ", 0) == 0) {
            std::string token = headerVal.substr(7);
            if (m_tokenMgr) {
                auto apiToken = m_tokenMgr->validateToken(token);
                if (apiToken) {
                    auth.authenticated = true;
                    auth.userId = apiToken->userId;
                    auth.isApiToken = true;
                    // Note: API tokens don't carry username/role - would need user lookup
                    return auth;
                }
            }
        }
    }

    // Check for session cookie
    struct mg_str* cookieHeader = mg_http_get_header(hm, "Cookie");
    if (cookieHeader && m_sessionMgr) {
        std::string cookies(cookieHeader->buf, cookieHeader->len);

        // Find session cookie
        std::string sessionToken;
        size_t pos = cookies.find("session=");
        if (pos != std::string::npos) {
            pos += 8;  // length of "session="
            size_t end = cookies.find(';', pos);
            if (end == std::string::npos) end = cookies.length();
            sessionToken = cookies.substr(pos, end - pos);
        }

        if (!sessionToken.empty()) {
            auto session = m_sessionMgr->validateSession(sessionToken);
            if (session) {
                auth.authenticated = true;
                auth.userId = session->userId;
                auth.username = session->username;
                auth.role = session->role;
                return auth;
            }
        }
    }

    return auth;
}

bool WebServer::checkRateLimit(const std::string& ip) {
    if (m_rateLimitPerMinute <= 0) {
        return true;  // Rate limiting disabled
    }

    auto now = std::chrono::steady_clock::now();
    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::lock_guard<std::mutex> lock(m_rateLimitMutex);

    auto& entry = m_rateLimits[ip];

    // Reset window if expired (1 minute)
    if (nowMs - entry.windowStart >= 60000) {
        entry.windowStart = nowMs;
        entry.requestCount = 0;
    }

    entry.requestCount++;

    if (entry.requestCount > m_rateLimitPerMinute) {
        LOG_WARN("Rate limit exceeded for IP: %s (%d requests)",
                 ip.c_str(), entry.requestCount);
        return false;
    }

    return true;
}

bool WebServer::isPublicRoute(const std::string& uri) const {
    // Always allow static files
    if (uri.rfind("/api/", 0) != 0) {
        return true;
    }

    // Check explicit public routes
    for (const auto& route : m_publicRoutes) {
        if (uri == route || uri.rfind(route, 0) == 0) {
            return true;
        }
    }

    // Default public routes
    if (uri == "/api/auth/login" || uri == "/api/system/status") {
        return true;
    }

    return false;
}

std::string WebServer::getClientIp(struct mg_connection* c) const {
    char buf[64] = {0};
    mg_snprintf(buf, sizeof(buf), "%M", mg_print_ip, &c->rem);
    return std::string(buf);
}

void WebServer::addSecurityHeaders(std::string& headers) {
    headers += "X-Content-Type-Options: nosniff\r\n";
    headers += "X-Frame-Options: DENY\r\n";
    headers += "X-XSS-Protection: 1; mode=block\r\n";
    headers += "Referrer-Policy: strict-origin-when-cross-origin\r\n";
    headers += "Cache-Control: no-store\r\n";
    if (!m_certPath.empty()) {
        headers += "Strict-Transport-Security: max-age=31536000; includeSubDomains\r\n";
    }
}

void WebServer::eventHandler(struct mg_connection* c, int ev, void* ev_data) {
    if (!s_instance) return;

    if (ev == MG_EV_ACCEPT && c->fn_data != nullptr) {
        // Initialize TLS on new connection if TLS opts were provided
        struct mg_tls_opts* opts = static_cast<struct mg_tls_opts*>(c->fn_data);
        mg_tls_init(c, opts);
    } else if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message* hm = static_cast<struct mg_http_message*>(ev_data);
        s_instance->handleHttpRequest(c, hm);
    }
}

void WebServer::httpRedirectHandler(struct mg_connection* c, int ev, void* ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message* hm = static_cast<struct mg_http_message*>(ev_data);

        // Get Host header
        struct mg_str* host = mg_http_get_header(hm, "Host");
        std::string hostStr = host ? std::string(host->buf, host->len) : "smarthub.local";

        // Remove port if present
        size_t colonPos = hostStr.find(':');
        if (colonPos != std::string::npos) {
            hostStr = hostStr.substr(0, colonPos);
        }

        // Build redirect URL
        std::string uri(hm->uri.buf, hm->uri.len);
        std::string location = "https://" + hostStr;

        // Add port if not default 443
        if (s_instance && s_instance->m_port != 443) {
            location += ":" + std::to_string(s_instance->m_port);
        }
        location += uri;

        // Send redirect
        mg_http_reply(c, 301,
                      ("Location: " + location + "\r\n"
                       "Content-Type: text/plain\r\n").c_str(),
                      "Redirecting to HTTPS...");
    }
}

void WebServer::handleHttpRequest(struct mg_connection* c, struct mg_http_message* hm) {
    std::string uri(hm->uri.buf, hm->uri.len);
    std::string clientIp = getClientIp(c);

    // Rate limiting check
    if (!checkRateLimit(clientIp)) {
        sendError(c, 429, "Too many requests");
        return;
    }

    // API routes
    if (uri.rfind("/api/", 0) == 0) {
        // Check authentication for protected routes
        AuthInfo auth;
        if (!isPublicRoute(uri)) {
            auth = checkAuth(hm);
            if (!auth.authenticated) {
                sendError(c, 401, "Unauthorized");
                return;
            }
        }

        // Route handling
        if (uri == "/api/devices" && mg_strcmp(hm->method, mg_str("GET")) == 0) {
            apiGetDevices(c);
        } else if (uri == "/api/devices" && mg_strcmp(hm->method, mg_str("POST")) == 0) {
            apiCreateDevice(c, hm);
        } else if (uri == "/api/system/status") {
            apiGetSystemStatus(c);
        } else if (uri == "/api/auth/login" && mg_strcmp(hm->method, mg_str("POST")) == 0) {
            apiLogin(c, hm);
        } else if (uri == "/api/auth/logout" && mg_strcmp(hm->method, mg_str("POST")) == 0) {
            auth = checkAuth(hm);  // Get auth info even for logout
            apiLogout(c, auth);
        } else if (uri == "/api/auth/me" && mg_strcmp(hm->method, mg_str("GET")) == 0) {
            apiGetCurrentUser(c, auth);
        }
        // Room API
        else if (uri == "/api/rooms" && mg_strcmp(hm->method, mg_str("GET")) == 0) {
            apiGetRooms(c);
        } else if (uri == "/api/rooms" && mg_strcmp(hm->method, mg_str("POST")) == 0) {
            apiCreateRoom(c, hm);
        } else if (uri.rfind("/api/rooms/", 0) == 0) {
            std::string id = uri.substr(11);  // Remove "/api/rooms/"
            if (mg_strcmp(hm->method, mg_str("PUT")) == 0) {
                apiUpdateRoom(c, id, hm);
            } else if (mg_strcmp(hm->method, mg_str("DELETE")) == 0) {
                apiDeleteRoom(c, id);
            } else {
                sendError(c, 405, "Method not allowed");
            }
        }
        // Device API
        else if (uri.rfind("/api/devices/", 0) == 0) {
            std::string remainder = uri.substr(13);  // Remove "/api/devices/"
            size_t slashPos = remainder.find('/');
            if (slashPos == std::string::npos) {
                // /api/devices/{id}
                std::string id = remainder;
                if (mg_strcmp(hm->method, mg_str("GET")) == 0) {
                    apiGetDevice(c, id);
                } else if (mg_strcmp(hm->method, mg_str("PUT")) == 0) {
                    apiSetDeviceState(c, id, hm);
                } else if (mg_strcmp(hm->method, mg_str("DELETE")) == 0) {
                    apiDeleteDevice(c, id);
                } else {
                    sendError(c, 405, "Method not allowed");
                }
            } else {
                // /api/devices/{id}/settings
                std::string id = remainder.substr(0, slashPos);
                std::string action = remainder.substr(slashPos + 1);
                if (action == "settings" && mg_strcmp(hm->method, mg_str("PUT")) == 0) {
                    apiUpdateDeviceSettings(c, id, hm);
                } else {
                    sendError(c, 404, "Not found");
                }
            }
        }
        // Zigbee pairing API
        else if (uri == "/api/zigbee/permit-join" && mg_strcmp(hm->method, mg_str("POST")) == 0) {
            apiZigbeePermitJoin(c, hm);
        } else if (uri == "/api/zigbee/permit-join" && mg_strcmp(hm->method, mg_str("DELETE")) == 0) {
            apiZigbeeStopPermitJoin(c);
        } else if (uri == "/api/zigbee/pending-devices" && mg_strcmp(hm->method, mg_str("GET")) == 0) {
            apiGetPendingDevices(c);
        } else if (uri == "/api/zigbee/devices" && mg_strcmp(hm->method, mg_str("POST")) == 0) {
            apiAddZigbeeDevice(c, hm);
        }
        // Automation API
        else if (uri == "/api/automations" && mg_strcmp(hm->method, mg_str("GET")) == 0) {
            apiGetAutomations(c);
        } else if (uri == "/api/automations" && mg_strcmp(hm->method, mg_str("POST")) == 0) {
            apiCreateAutomation(c, hm);
        } else if (uri.rfind("/api/automations/", 0) == 0) {
            std::string remainder = uri.substr(17);  // Remove "/api/automations/"
            size_t slashPos = remainder.find('/');
            if (slashPos == std::string::npos) {
                // /api/automations/{id}
                std::string id = remainder;
                if (mg_strcmp(hm->method, mg_str("GET")) == 0) {
                    apiGetAutomation(c, id);
                } else if (mg_strcmp(hm->method, mg_str("PUT")) == 0) {
                    apiUpdateAutomation(c, id, hm);
                } else if (mg_strcmp(hm->method, mg_str("DELETE")) == 0) {
                    apiDeleteAutomation(c, id);
                } else {
                    sendError(c, 405, "Method not allowed");
                }
            } else {
                // /api/automations/{id}/enable
                std::string id = remainder.substr(0, slashPos);
                std::string action = remainder.substr(slashPos + 1);
                if (action == "enable" && mg_strcmp(hm->method, mg_str("PUT")) == 0) {
                    apiSetAutomationEnabled(c, id, hm);
                } else {
                    sendError(c, 404, "Not found");
                }
            }
        }
        else {
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
        json += "\"room\":\"" + device->room() + "\",";
        json += "\"online\":" + std::string(device->isAvailable() ? "true" : "false") + ",";
        json += "\"state\":" + device->getState().dump();
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
    json += "\"online\":" + std::string(device->isAvailable() ? "true" : "false") + ",";
    json += "\"state\":" + device->getState().dump();
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
    std::string body(hm->body.buf, hm->body.len);
    LOG_DEBUG("Set device state: %s = %s", id.c_str(), body.c_str());

    try {
        auto stateJson = nlohmann::json::parse(body);

        // Apply each state property
        for (auto& [key, value] : stateJson.items()) {
            device->setState(key, value);
        }

        // Return updated state
        std::string json = "{\"success\":true,\"state\":" + device->getState().dump() + "}";
        sendJson(c, 200, json);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse device state: %s", e.what());
        sendError(c, 400, "Invalid JSON");
    }
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

void WebServer::apiLogin(struct mg_connection* c, struct mg_http_message* hm) {
    // Parse JSON body - simplified parsing
    std::string body(hm->body.buf, hm->body.len);

    // Extract username and password (basic JSON parsing)
    std::string username, password;

    size_t usernamePos = body.find("\"username\"");
    if (usernamePos != std::string::npos) {
        size_t colonPos = body.find(':', usernamePos);
        size_t quoteStart = body.find('"', colonPos);
        size_t quoteEnd = body.find('"', quoteStart + 1);
        if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
            username = body.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        }
    }

    size_t passwordPos = body.find("\"password\"");
    if (passwordPos != std::string::npos) {
        size_t colonPos = body.find(':', passwordPos);
        size_t quoteStart = body.find('"', colonPos);
        size_t quoteEnd = body.find('"', quoteStart + 1);
        if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
            password = body.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        }
    }

    if (username.empty() || password.empty()) {
        sendError(c, 400, "Username and password required");
        return;
    }

    // Note: Would need UserManager here for actual authentication
    // For now, just create a session if SessionManager is available
    if (!m_sessionMgr) {
        sendError(c, 500, "Authentication not configured");
        return;
    }

    // TODO: Validate password with UserManager
    // For now, this is a stub - in production, integrate with UserManager
    LOG_WARN("Login attempt for user: %s (password validation not implemented)", username.c_str());

    // Create session (userId=1 is placeholder)
    std::string token = m_sessionMgr->createSession(1, username, "user");

    if (token.empty()) {
        sendError(c, 500, "Failed to create session");
        return;
    }

    // Set session cookie
    std::string cookieHeader = "Set-Cookie: session=" + token +
                                "; HttpOnly; SameSite=Strict; Path=/";
    if (!m_certPath.empty()) {
        cookieHeader += "; Secure";
    }
    cookieHeader += "\r\n";

    std::string json = "{\"success\":true,\"username\":\"" + username + "\"}";
    sendJsonWithHeaders(c, 200, json, cookieHeader);
}

void WebServer::apiLogout(struct mg_connection* c, const AuthInfo& auth) {
    if (m_sessionMgr && auth.authenticated && !auth.isApiToken) {
        // Get session token from cookie to destroy
        // The token was already validated in checkAuth
        m_sessionMgr->destroyUserSessions(auth.username);
    }

    // Clear session cookie
    std::string cookieHeader = "Set-Cookie: session=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0\r\n";

    std::string json = "{\"success\":true}";
    sendJsonWithHeaders(c, 200, json, cookieHeader);
}

void WebServer::apiGetCurrentUser(struct mg_connection* c, const AuthInfo& auth) {
    if (!auth.authenticated) {
        sendError(c, 401, "Not authenticated");
        return;
    }

    std::string json = "{";
    json += "\"userId\":" + std::to_string(auth.userId) + ",";
    json += "\"username\":\"" + auth.username + "\",";
    json += "\"role\":\"" + auth.role + "\",";
    json += "\"isApiToken\":" + std::string(auth.isApiToken ? "true" : "false");
    json += "}";

    sendJson(c, 200, json);
}

// =============================================================================
// Room API Handlers
// =============================================================================

void WebServer::apiGetRooms(struct mg_connection* c) {
    auto rooms = m_deviceManager.getAllRooms();

    std::string json = "[";
    bool first = true;
    for (const auto& [id, name] : rooms) {
        if (!first) json += ",";
        first = false;

        // Count devices in this room (devices store room name, not id)
        auto devices = m_deviceManager.getDevicesByRoom(name);
        int activeCount = 0;
        for (const auto& dev : devices) {
            auto state = dev->getState();
            if (state.contains("on") && state["on"].get<bool>()) {
                activeCount++;
            }
        }

        json += "{";
        json += "\"id\":\"" + id + "\",";
        json += "\"name\":\"" + name + "\",";
        json += "\"deviceCount\":" + std::to_string(devices.size()) + ",";
        json += "\"activeDevices\":" + std::to_string(activeCount);
        json += "}";
    }
    json += "]";

    sendJson(c, 200, json);
}

void WebServer::apiCreateRoom(struct mg_connection* c, struct mg_http_message* hm) {
    std::string body(hm->body.buf, hm->body.len);

    try {
        auto data = nlohmann::json::parse(body);
        std::string name = data.value("name", "");

        if (name.empty()) {
            sendError(c, 400, "name is required");
            return;
        }

        // Generate ID from name if not provided
        std::string id = data.value("id", "");
        if (id.empty()) {
            // Convert name to lowercase with hyphens
            id = name;
            std::transform(id.begin(), id.end(), id.begin(), ::tolower);
            std::replace(id.begin(), id.end(), ' ', '-');
            // Remove any non-alphanumeric chars except hyphen
            id.erase(std::remove_if(id.begin(), id.end(), [](char c) {
                return !std::isalnum(c) && c != '-';
            }), id.end());
        }

        if (m_deviceManager.addRoom(id, name)) {
            std::string json = "{\"success\":true,\"id\":\"" + id + "\",\"name\":\"" + name + "\"}";
            sendJson(c, 201, json);
        } else {
            sendError(c, 409, "Room already exists");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse room create request: %s", e.what());
        sendError(c, 400, "Invalid JSON");
    }
}

void WebServer::apiUpdateRoom(struct mg_connection* c, const std::string& id,
                               struct mg_http_message* hm) {
    std::string body(hm->body.buf, hm->body.len);

    try {
        auto data = nlohmann::json::parse(body);
        std::string name = data.value("name", "");

        if (name.empty()) {
            sendError(c, 400, "name is required");
            return;
        }

        if (m_deviceManager.updateRoom(id, name)) {
            std::string json = "{\"success\":true,\"id\":\"" + id + "\",\"name\":\"" + name + "\"}";
            sendJson(c, 200, json);
        } else {
            sendError(c, 404, "Room not found");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse room update request: %s", e.what());
        sendError(c, 400, "Invalid JSON");
    }
}

void WebServer::apiDeleteRoom(struct mg_connection* c, const std::string& id) {
    if (m_deviceManager.deleteRoom(id)) {
        sendJson(c, 200, "{\"success\":true}");
    } else {
        sendError(c, 404, "Room not found");
    }
}

// =============================================================================
// Device CRUD API Handlers
// =============================================================================

void WebServer::apiCreateDevice(struct mg_connection* c, struct mg_http_message* hm) {
    std::string body(hm->body.buf, hm->body.len);

    try {
        auto data = nlohmann::json::parse(body);

        std::string id = data.value("id", "");
        std::string name = data.value("name", "");
        std::string type = data.value("type", "");
        std::string protocol = data.value("protocol", "local");
        std::string protocolAddress = data.value("protocolAddress", "");
        std::string room = data.value("room", "");
        nlohmann::json config = data.value("config", nlohmann::json::object());

        if (name.empty() || type.empty()) {
            sendError(c, 400, "name and type are required");
            return;
        }

        // Generate ID from name if not provided
        if (id.empty()) {
            id = "device-";
            std::string nameSlug = name;
            std::transform(nameSlug.begin(), nameSlug.end(), nameSlug.begin(), ::tolower);
            std::replace(nameSlug.begin(), nameSlug.end(), ' ', '-');
            nameSlug.erase(std::remove_if(nameSlug.begin(), nameSlug.end(), [](char c) {
                return !std::isalnum(c) && c != '-';
            }), nameSlug.end());
            id += nameSlug;
        }

        // Check if device already exists
        if (m_deviceManager.getDevice(id)) {
            sendError(c, 409, "Device already exists");
            return;
        }

        // Create device using registry
        auto& registry = DeviceTypeRegistry::instance();
        auto device = registry.createFromTypeName(type, id, name, protocol, protocolAddress, config);

        if (!device) {
            sendError(c, 400, "Invalid device type: " + type);
            return;
        }

        device->setRoom(room);

        if (m_deviceManager.addDevice(device)) {
            std::string json = "{\"success\":true,\"id\":\"" + id + "\"}";
            sendJson(c, 201, json);
        } else {
            sendError(c, 500, "Failed to add device");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse device create request: %s", e.what());
        sendError(c, 400, "Invalid JSON");
    }
}

void WebServer::apiUpdateDeviceSettings(struct mg_connection* c, const std::string& id,
                                         struct mg_http_message* hm) {
    auto device = m_deviceManager.getDevice(id);
    if (!device) {
        sendError(c, 404, "Device not found");
        return;
    }

    std::string body(hm->body.buf, hm->body.len);

    try {
        auto data = nlohmann::json::parse(body);

        // Update name if provided
        if (data.contains("name")) {
            device->setName(data["name"].get<std::string>());
        }

        // Update room if provided
        if (data.contains("room")) {
            device->setRoom(data["room"].get<std::string>());
        }

        // Save device to persist changes
        m_deviceManager.saveDevice(device);

        std::string json = "{\"success\":true}";
        sendJson(c, 200, json);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse device settings update: %s", e.what());
        sendError(c, 400, "Invalid JSON");
    }
}

void WebServer::apiDeleteDevice(struct mg_connection* c, const std::string& id) {
    if (m_deviceManager.removeDevice(id)) {
        sendJson(c, 200, "{\"success\":true}");
    } else {
        sendError(c, 404, "Device not found");
    }
}

// =============================================================================
// Zigbee Pairing API Handlers
// =============================================================================

void WebServer::apiZigbeePermitJoin(struct mg_connection* c, struct mg_http_message* hm) {
    if (!m_zigbeeHandler) {
        sendError(c, 503, "Zigbee not available");
        return;
    }

    std::string body(hm->body.buf, hm->body.len);
    int duration = 60;  // Default 60 seconds

    try {
        if (!body.empty()) {
            auto data = nlohmann::json::parse(body);
            duration = data.value("duration", 60);
        }
    } catch (...) {
        // Use default duration
    }

    m_zigbeeHandler->startDiscovery();

    std::string json = "{\"success\":true,\"duration\":" + std::to_string(duration) + "}";
    sendJson(c, 200, json);
}

void WebServer::apiZigbeeStopPermitJoin(struct mg_connection* c) {
    if (!m_zigbeeHandler) {
        sendError(c, 503, "Zigbee not available");
        return;
    }

    m_zigbeeHandler->stopDiscovery();
    sendJson(c, 200, "{\"success\":true}");
}

void WebServer::apiGetPendingDevices(struct mg_connection* c) {
    if (!m_zigbeeHandler) {
        sendError(c, 503, "Zigbee not available");
        return;
    }

    // Get pending device if any
    auto pendingDevice = m_zigbeeHandler->getPendingDevice();

    std::string json = "[";
    if (pendingDevice) {
        json += "{";
        json += "\"ieeeAddress\":\"" + pendingDevice->protocolAddress() + "\",";
        json += "\"manufacturer\":\"" + pendingDevice->getConfig().value("manufacturer", "") + "\",";
        json += "\"model\":\"" + pendingDevice->getConfig().value("model", "") + "\",";
        json += "\"type\":\"" + pendingDevice->typeString() + "\"";
        json += "}";
    }
    json += "]";

    sendJson(c, 200, json);
}

void WebServer::apiAddZigbeeDevice(struct mg_connection* c, struct mg_http_message* hm) {
    if (!m_zigbeeHandler) {
        sendError(c, 503, "Zigbee not available");
        return;
    }

    std::string body(hm->body.buf, hm->body.len);

    try {
        auto data = nlohmann::json::parse(body);

        std::string ieeeAddress = data.value("ieeeAddress", "");
        std::string name = data.value("name", "");
        std::string room = data.value("room", "");

        if (ieeeAddress.empty()) {
            sendError(c, 400, "ieeeAddress is required");
            return;
        }

        // Check if there's a pending device with this address
        auto pendingDevice = m_zigbeeHandler->getPendingDevice();
        if (!pendingDevice || pendingDevice->protocolAddress() != ieeeAddress) {
            sendError(c, 404, "No pending device with that address");
            return;
        }

        // Update device name and room
        if (!name.empty()) {
            pendingDevice->setName(name);
        }
        if (!room.empty()) {
            pendingDevice->setRoom(room);
        }

        // Add to device manager
        if (m_deviceManager.addDevice(pendingDevice)) {
            // Clear pending device
            m_zigbeeHandler->clearPendingDevice();

            std::string json = "{\"success\":true,\"id\":\"" + pendingDevice->id() + "\"}";
            sendJson(c, 201, json);
        } else {
            sendError(c, 500, "Failed to add device");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse Zigbee device add request: %s", e.what());
        sendError(c, 400, "Invalid JSON");
    }
}

// =============================================================================
// Automation API Handlers
// =============================================================================

void WebServer::apiGetAutomations(struct mg_connection* c) {
    if (!m_automationMgr) {
        sendError(c, 503, "Automations not available");
        return;
    }

    auto automations = m_automationMgr->getAllAutomations();

    std::string json = "[";
    bool first = true;
    for (const auto& automation : automations) {
        if (!first) json += ",";
        first = false;

        json += automation->toJson().dump();
    }
    json += "]";

    sendJson(c, 200, json);
}

void WebServer::apiCreateAutomation(struct mg_connection* c, struct mg_http_message* hm) {
    if (!m_automationMgr) {
        sendError(c, 503, "Automations not available");
        return;
    }

    std::string body(hm->body.buf, hm->body.len);

    try {
        auto data = nlohmann::json::parse(body);

        // Validate required fields
        if (!data.contains("name") || data["name"].get<std::string>().empty()) {
            sendError(c, 400, "name is required");
            return;
        }

        Automation automation = Automation::fromJson(data);

        // Generate ID if not provided
        if (automation.id.empty()) {
            automation.id = m_automationMgr->generateId();
        }

        if (m_automationMgr->addAutomation(automation)) {
            std::string json = "{\"success\":true,\"id\":\"" + automation.id + "\"}";
            sendJson(c, 201, json);
        } else {
            sendError(c, 500, "Failed to create automation");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse automation create request: %s", e.what());
        sendError(c, 400, std::string("Invalid JSON: ") + e.what());
    }
}

void WebServer::apiGetAutomation(struct mg_connection* c, const std::string& id) {
    if (!m_automationMgr) {
        sendError(c, 503, "Automations not available");
        return;
    }

    auto automation = m_automationMgr->getAutomation(id);
    if (!automation) {
        sendError(c, 404, "Automation not found");
        return;
    }

    sendJson(c, 200, automation->toJson().dump());
}

void WebServer::apiUpdateAutomation(struct mg_connection* c, const std::string& id,
                                     struct mg_http_message* hm) {
    if (!m_automationMgr) {
        sendError(c, 503, "Automations not available");
        return;
    }

    // Check if automation exists
    auto existing = m_automationMgr->getAutomation(id);
    if (!existing) {
        sendError(c, 404, "Automation not found");
        return;
    }

    std::string body(hm->body.buf, hm->body.len);

    try {
        auto data = nlohmann::json::parse(body);
        Automation automation = Automation::fromJson(data);
        automation.id = id;  // Ensure ID matches URL

        if (m_automationMgr->updateAutomation(automation)) {
            sendJson(c, 200, "{\"success\":true}");
        } else {
            sendError(c, 500, "Failed to update automation");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse automation update request: %s", e.what());
        sendError(c, 400, std::string("Invalid JSON: ") + e.what());
    }
}

void WebServer::apiDeleteAutomation(struct mg_connection* c, const std::string& id) {
    if (!m_automationMgr) {
        sendError(c, 503, "Automations not available");
        return;
    }

    if (m_automationMgr->deleteAutomation(id)) {
        sendJson(c, 200, "{\"success\":true}");
    } else {
        sendError(c, 404, "Automation not found");
    }
}

void WebServer::apiSetAutomationEnabled(struct mg_connection* c, const std::string& id,
                                         struct mg_http_message* hm) {
    if (!m_automationMgr) {
        sendError(c, 503, "Automations not available");
        return;
    }

    std::string body(hm->body.buf, hm->body.len);

    try {
        auto data = nlohmann::json::parse(body);
        bool enabled = data.value("enabled", true);

        if (m_automationMgr->setEnabled(id, enabled)) {
            sendJson(c, 200, "{\"success\":true,\"enabled\":" +
                     std::string(enabled ? "true" : "false") + "}");
        } else {
            sendError(c, 404, "Automation not found");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse automation enable request: %s", e.what());
        sendError(c, 400, "Invalid JSON");
    }
}

void WebServer::sendJson(struct mg_connection* c, int status, const std::string& json) {
    sendJsonWithHeaders(c, status, json, "");
}

void WebServer::sendJsonWithHeaders(struct mg_connection* c, int status,
                                     const std::string& json, const std::string& extraHeaders) {
    std::string headers = "Content-Type: application/json\r\n";
    addSecurityHeaders(headers);
    headers += extraHeaders;

    mg_http_reply(c, status, headers.c_str(), "%s", json.c_str());
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
void WebServer::setSecurityManagers(security::SessionManager*, security::ApiTokenManager*) {}
void WebServer::setAutomationManager(AutomationManager*) {}
void WebServer::setZigbeeHandler(zigbee::ZigbeeHandler*) {}
void WebServer::setRateLimit(int) {}
void WebServer::setPublicRoutes(const std::vector<std::string>&) {}
AuthInfo WebServer::checkAuth(struct mg_http_message*) { return {}; }
bool WebServer::checkRateLimit(const std::string&) { return true; }
bool WebServer::isPublicRoute(const std::string&) const { return true; }
std::string WebServer::getClientIp(struct mg_connection*) const { return ""; }
void WebServer::addSecurityHeaders(std::string&) {}
void WebServer::eventHandler(struct mg_connection*, int, void*) {}
void WebServer::httpRedirectHandler(struct mg_connection*, int, void*) {}
void WebServer::handleHttpRequest(struct mg_connection*, struct mg_http_message*) {}
void WebServer::handleWebSocket(struct mg_connection*, void*) {}
void WebServer::apiGetDevices(struct mg_connection*) {}
void WebServer::apiGetDevice(struct mg_connection*, const std::string&) {}
void WebServer::apiSetDeviceState(struct mg_connection*, const std::string&, struct mg_http_message*) {}
void WebServer::apiGetSensorHistory(struct mg_connection*, const std::string&, struct mg_http_message*) {}
void WebServer::apiGetSystemStatus(struct mg_connection*) {}
void WebServer::apiLogin(struct mg_connection*, struct mg_http_message*) {}
void WebServer::apiLogout(struct mg_connection*, const AuthInfo&) {}
void WebServer::apiGetCurrentUser(struct mg_connection*, const AuthInfo&) {}
void WebServer::apiGetRooms(struct mg_connection*) {}
void WebServer::apiCreateRoom(struct mg_connection*, struct mg_http_message*) {}
void WebServer::apiUpdateRoom(struct mg_connection*, const std::string&, struct mg_http_message*) {}
void WebServer::apiDeleteRoom(struct mg_connection*, const std::string&) {}
void WebServer::apiCreateDevice(struct mg_connection*, struct mg_http_message*) {}
void WebServer::apiUpdateDeviceSettings(struct mg_connection*, const std::string&, struct mg_http_message*) {}
void WebServer::apiDeleteDevice(struct mg_connection*, const std::string&) {}
void WebServer::apiZigbeePermitJoin(struct mg_connection*, struct mg_http_message*) {}
void WebServer::apiZigbeeStopPermitJoin(struct mg_connection*) {}
void WebServer::apiGetPendingDevices(struct mg_connection*) {}
void WebServer::apiAddZigbeeDevice(struct mg_connection*, struct mg_http_message*) {}
void WebServer::apiGetAutomations(struct mg_connection*) {}
void WebServer::apiCreateAutomation(struct mg_connection*, struct mg_http_message*) {}
void WebServer::apiGetAutomation(struct mg_connection*, const std::string&) {}
void WebServer::apiUpdateAutomation(struct mg_connection*, const std::string&, struct mg_http_message*) {}
void WebServer::apiDeleteAutomation(struct mg_connection*, const std::string&) {}
void WebServer::apiSetAutomationEnabled(struct mg_connection*, const std::string&, struct mg_http_message*) {}
void WebServer::sendJson(struct mg_connection*, int, const std::string&) {}
void WebServer::sendJsonWithHeaders(struct mg_connection*, int, const std::string&, const std::string&) {}
void WebServer::sendError(struct mg_connection*, int, const std::string&) {}

#endif // HAVE_MONGOOSE

} // namespace smarthub
