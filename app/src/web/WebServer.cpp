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
#include <smarthub/security/UserManager.hpp>
#include <smarthub/core/Logger.hpp>
#include <nlohmann/json.hpp>

#include <chrono>
#include <algorithm>
#include <cctype>
#include <optional>
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

namespace {

constexpr uint64_t kRateLimitWindowMs = 60000;
constexpr size_t kMaxTrackedClients = 1024;
constexpr const char* kUseLocalAddress = "Open the hub by its IP address or local name";

uint64_t steadyNowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

// Keeps a per-client table from growing without limit, by making room when a
// client it doesn't know arrives at a full table.
void makeRoomFor(const std::string& ip, std::unordered_map<std::string, RateLimitEntry>& entries,
                 uint64_t nowMs, uint64_t windowMs) {
    if (entries.size() < kMaxTrackedClients || entries.count(ip) > 0) {
        return;
    }
    for (auto it = entries.begin(); it != entries.end();) {
        if (nowMs - it->second.windowStart >= windowMs) {
            it = entries.erase(it);
        } else {
            ++it;
        }
    }
    if (entries.size() >= kMaxTrackedClients) {
        entries.erase(std::min_element(entries.begin(), entries.end(),
                                       [](const auto& a, const auto& b) {
                                           return a.second.windowStart < b.second.windowStart;
                                       }));
    }
}

// Lower case, without a port or the trailing dot of a fully qualified name
std::string normalizeHost(std::string host) {
    host = host.substr(0, host.find(':'));
    std::transform(host.begin(), host.end(), host.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (!host.empty() && host.back() == '.') {
        host.pop_back();
    }
    return host;
}

// A page on another site can make its domain resolve to the hub (DNS
// rebinding), and then counts as same-origin. Only names in public DNS can be
// used for that, so the routes that create a session refuse them. Other routes
// don't need this, because that page's origin has no session cookie to send.
bool isAllowedHostHeader(struct mg_http_message* hm, const std::vector<std::string>& allowedHosts) {
    static const std::vector<std::string> kLocalSuffixes = {".local", ".localdomain", ".lan", ".home",
                                                            ".internal", ".home.arpa"};

    struct mg_str* header = mg_http_get_header(hm, "Host");
    if (!header) {
        return false;
    }
    const std::string raw(header->buf, header->len);
    if (!raw.empty() && raw.front() == '[') {
        return true;  // IPv6 literal
    }
    const std::string host = normalizeHost(raw);
    if (host.empty()) {
        return false;
    }
    if (std::find(allowedHosts.begin(), allowedHosts.end(), host) != allowedHosts.end()) {
        return true;
    }

    const bool isIpv4 = host.find_first_not_of("0123456789.") == std::string::npos;
    const bool isSingleLabel = host.find('.') == std::string::npos;
    if (isIpv4 || isSingleLabel) {
        return true;
    }
    return std::any_of(kLocalSuffixes.begin(), kLocalSuffixes.end(),
                       [&host](const std::string& suffix) {
                           return host.size() > suffix.size() &&
                                  host.compare(host.size() - suffix.size(), suffix.size(),
                                               suffix) == 0;
                       });
}

AuthInfo authInfoFor(const security::User& user, bool isApiToken) {
    AuthInfo auth;
    auth.authenticated = true;
    auth.userId = user.id;
    auth.username = user.username;
    auth.role = user.role;
    auth.isApiToken = isApiToken;
    return auth;
}

std::string sessionTokenFromCookies(struct mg_http_message* hm) {
    static const std::string kName = "session=";

    struct mg_str* header = mg_http_get_header(hm, "Cookie");
    if (!header) {
        return "";
    }

    const std::string cookies(header->buf, header->len);
    size_t pos = 0;
    while (pos < cookies.size()) {
        size_t end = cookies.find(';', pos);
        if (end == std::string::npos) {
            end = cookies.size();
        }
        const size_t start = cookies.find_first_not_of(' ', pos);
        if (start != std::string::npos && start < end &&
            cookies.compare(start, kName.size(), kName) == 0) {
            return cookies.substr(start + kName.size(), end - start - kName.size());
        }
        pos = end + 1;
    }
    return "";
}

// Requiring JSON means a page on another site can't send these requests
// without a CORS preflight, which this server never approves.
bool isJsonRequest(struct mg_http_message* hm) {
    static const std::string kJson = "application/json";

    struct mg_str* header = mg_http_get_header(hm, "Content-Type");
    if (!header) {
        return false;
    }
    std::string value(header->buf, header->len);
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value.rfind(kJson, 0) == 0;
}

bool parseCredentials(struct mg_http_message* hm, std::string& username, std::string& password) {
    const auto body = nlohmann::json::parse(hm->body.buf, hm->body.buf + hm->body.len,
                                            nullptr, false);
    if (!body.is_object()) {
        return false;
    }
    const auto user = body.find("username");
    const auto pass = body.find("password");
    if (user == body.end() || pass == body.end() ||
        !user->is_string() || !pass->is_string()) {
        return false;
    }
    username = user->get<std::string>();
    password = pass->get<std::string>();
    return !username.empty() && !password.empty();
}

}  // namespace

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
                                     security::ApiTokenManager* tokenMgr,
                                     security::UserManager* userMgr) {
    m_sessionMgr = sessionMgr;
    m_tokenMgr = tokenMgr;
    m_userMgr = userMgr;
}

void WebServer::setRateLimit(int requestsPerMinute) {
    m_rateLimitPerMinute = requestsPerMinute;
}

void WebServer::setLoginThrottle(int maxFailures, int windowSeconds) {
    m_loginMaxFailures = maxFailures;
    m_loginFailureWindowSeconds = windowSeconds;
}

void WebServer::setAllowedHosts(const std::vector<std::string>& hosts) {
    m_allowedHosts.clear();
    for (const std::string& host : hosts) {
        m_allowedHosts.push_back(normalizeHost(host));
    }
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
    if (!m_userMgr) {
        return {};
    }

    // Both kinds of credential only name a user. The account is looked up each
    // time, so disabling or removing it, or changing its role, applies at once.

    // Check Authorization header for Bearer token (API token)
    struct mg_str* authHeader = mg_http_get_header(hm, "Authorization");
    if (authHeader && authHeader->len > 7 && m_tokenMgr) {
        std::string headerVal(authHeader->buf, authHeader->len);
        if (headerVal.rfind("Bearer ", 0) == 0) {
            auto apiToken = m_tokenMgr->validateToken(headerVal.substr(7));
            if (apiToken) {
                auto user = m_userMgr->getUserById(apiToken->userId);
                if (user && user->enabled) {
                    return authInfoFor(*user, true);
                }
            }
        }
    }

    // Check for session cookie
    if (m_sessionMgr) {
        const std::string sessionToken = sessionTokenFromCookies(hm);
        if (!sessionToken.empty()) {
            auto session = m_sessionMgr->validateSession(sessionToken);
            if (session) {
                auto user = m_userMgr->getUserById(session->userId);
                if (user && user->enabled) {
                    // The timeout counts idle time, not time since login
                    m_sessionMgr->touchSession(sessionToken);
                    return authInfoFor(*user, false);
                }
                // No user can also mean a database error, which shouldn't end
                // a good session: this request fails, but a reload recovers.
                // A removed user's session just expires.
                if (user) {
                    m_sessionMgr->destroySession(sessionToken);
                }
            }
        }
    }

    return {};
}

bool WebServer::checkRateLimit(const std::string& ip) {
    if (m_rateLimitPerMinute <= 0) {
        return true;  // Rate limiting disabled
    }

    const uint64_t nowMs = steadyNowMs();

    std::lock_guard<std::mutex> lock(m_rateLimitMutex);

    makeRoomFor(ip, m_rateLimits, nowMs, kRateLimitWindowMs);
    auto& entry = m_rateLimits[ip];

    // Reset window if expired (1 minute)
    if (nowMs - entry.windowStart >= kRateLimitWindowMs) {
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

int WebServer::loginRetryAfterSeconds(const std::string& ip) {
    if (m_loginMaxFailures <= 0) {
        return 0;
    }

    const uint64_t nowMs = steadyNowMs();
    const uint64_t windowMs = static_cast<uint64_t>(m_loginFailureWindowSeconds) * 1000;

    std::lock_guard<std::mutex> lock(m_rateLimitMutex);

    auto it = m_loginFailures.find(ip);
    if (it == m_loginFailures.end()) {
        return 0;
    }

    const uint64_t elapsedMs = nowMs - it->second.windowStart;
    if (elapsedMs >= windowMs) {
        m_loginFailures.erase(it);
        return 0;
    }
    if (it->second.requestCount < m_loginMaxFailures) {
        return 0;
    }
    return static_cast<int>((windowMs - elapsedMs + 999) / 1000);
}

void WebServer::recordLoginFailure(const std::string& ip) {
    if (m_loginMaxFailures <= 0) {
        return;
    }

    const uint64_t nowMs = steadyNowMs();
    const uint64_t windowMs = static_cast<uint64_t>(m_loginFailureWindowSeconds) * 1000;

    std::lock_guard<std::mutex> lock(m_rateLimitMutex);

    makeRoomFor(ip, m_loginFailures, nowMs, windowMs);
    auto& entry = m_loginFailures[ip];
    if (entry.requestCount == 0 || nowMs - entry.windowStart >= windowMs) {
        entry.windowStart = nowMs;
        entry.requestCount = 0;
    }
    entry.requestCount++;
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
    // The setup routes check for themselves that no account exists yet.
    // Logout too: it has to end the session named by the cookie even if the
    // account can't be looked up right now.
    if (uri == "/api/auth/login" || uri == "/api/auth/logout" || uri == "/api/system/status" ||
        uri == "/api/setup" || uri == "/api/setup/status") {
        return true;
    }

    return false;
}

bool WebServer::requiresAdmin(const std::string& method, const std::string& uri) {
    static const std::string kDevicePrefix = "/api/devices/";

    if (uri == "/api/zigbee/pending-devices") {
        return true;
    }
    if (method == "GET" || method == "HEAD") {
        return false;
    }
    // Controlling a device, PUT /api/devices/{id}, is open to every user.
    // Everything else that changes something, including any route added
    // later, needs an admin unless it is listed here.
    if (method == "PUT" && uri.rfind(kDevicePrefix, 0) == 0 &&
        uri.find('/', kDevicePrefix.size()) == std::string::npos) {
        return false;
    }
    return true;
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
            if (requiresAdmin(std::string(hm->method.buf, hm->method.len), uri) &&
                auth.role != "admin") {
                sendError(c, 403, "Administrator access required");
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
            apiLogout(c, hm);
        } else if (uri == "/api/auth/me" && mg_strcmp(hm->method, mg_str("GET")) == 0) {
            apiGetCurrentUser(c, auth);
        } else if (uri == "/api/setup/status" && mg_strcmp(hm->method, mg_str("GET")) == 0) {
            apiGetSetupStatus(c);
        } else if (uri == "/api/setup" && mg_strcmp(hm->method, mg_str("POST")) == 0) {
            apiSetup(c, hm);
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
        if (!stateJson.is_object()) {
            sendError(c, 400, "Expected a JSON object of properties");
            return;
        }

        for (auto& [key, value] : stateJson.items()) {
            if (!DeviceManager::isControllableProperty(*device, key, value)) {
                sendError(c, 400, "This device can't be set to that");
                return;
            }
        }

        // Apply each state property, through the device's protocol
        for (auto& [key, value] : stateJson.items()) {
            if (!m_deviceManager.setDeviceState(id, key, value)) {
                sendError(c, 502, "Could not set " + key);
                return;
            }
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

void WebServer::startSession(struct mg_connection* c, const security::User& user, int status) {
    const std::string token = m_sessionMgr->createSession(user.id, user.username, user.role,
                                                          getClientIp(c));
    if (token.empty()) {
        sendError(c, 500, "Failed to create session");
        return;
    }

    // Decided per connection: a browser drops a Secure cookie sent over plain
    // HTTP, which would leave the user unable to log in.
    std::string cookieHeader = "Set-Cookie: session=" + token +
                               "; HttpOnly; SameSite=Strict; Path=/";
    if (c->is_tls) {
        cookieHeader += "; Secure";
    }
    cookieHeader += "\r\n";

    nlohmann::json json;
    json["success"] = true;
    json["username"] = user.username;
    json["role"] = user.role;
    sendJsonWithHeaders(c, status, json.dump(), cookieHeader);
}

void WebServer::apiLogin(struct mg_connection* c, struct mg_http_message* hm) {
    if (!m_sessionMgr || !m_userMgr) {
        sendError(c, 500, "Authentication not configured");
        return;
    }

    if (!isAllowedHostHeader(hm, m_allowedHosts)) {
        sendError(c, 403, kUseLocalAddress);
        return;
    }

    // Checked before the password: hashing is slow and blocks this thread
    const std::string clientIp = getClientIp(c);
    const int retryAfter = loginRetryAfterSeconds(clientIp);
    if (retryAfter > 0) {
        sendJsonWithHeaders(c, 429, "{\"error\":\"Too many failed login attempts\"}",
                            "Retry-After: " + std::to_string(retryAfter) + "\r\n");
        return;
    }

    if (!isJsonRequest(hm)) {
        sendError(c, 415, "Content-Type must be application/json");
        return;
    }

    std::string username, password;
    if (!parseCredentials(hm, username, password)) {
        sendError(c, 400, "Username and password required");
        return;
    }

    // A malformed username can't belong to an account, so skip the lookup.
    // That also keeps control characters out of the failure log.
    std::optional<security::User> user;
    if (security::UserManager::validateUsername(username).empty() &&
        m_userMgr->authenticate(username, password)) {
        user = m_userMgr->getUser(username);
    }

    if (!user) {
        recordLoginFailure(clientIp);
        sendError(c, 401, "Invalid username or password");
        return;
    }

    startSession(c, *user, 200);
}

void WebServer::apiLogout(struct mg_connection* c, struct mg_http_message* hm) {
    // With no cookie there's nothing to end. A cross-site request also arrives
    // that way, and clearing the cookie would let any page log the user out.
    const std::string token = sessionTokenFromCookies(hm);
    std::string cookieHeader;
    if (!token.empty()) {
        // Only this browser's session; the user's other devices stay logged in
        if (m_sessionMgr) {
            m_sessionMgr->destroySession(token);
        }
        cookieHeader = "Set-Cookie: session=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0\r\n";
    }

    std::string json = "{\"success\":true}";
    sendJsonWithHeaders(c, 200, json, cookieHeader);
}

void WebServer::apiGetCurrentUser(struct mg_connection* c, const AuthInfo& auth) {
    if (!auth.authenticated) {
        sendError(c, 401, "Not authenticated");
        return;
    }

    nlohmann::json json;
    json["userId"] = auth.userId;
    json["username"] = auth.username;
    json["role"] = auth.role;
    json["isApiToken"] = auth.isApiToken;
    sendJson(c, 200, json.dump());
}

void WebServer::apiGetSetupStatus(struct mg_connection* c) {
    if (!m_userMgr) {
        sendError(c, 500, "Authentication not configured");
        return;
    }

    nlohmann::json json;
    json["setupRequired"] = !m_userMgr->hasAnyUser();
    sendJson(c, 200, json.dump());
}

void WebServer::apiSetup(struct mg_connection* c, struct mg_http_message* hm) {
    if (!m_sessionMgr || !m_userMgr) {
        sendError(c, 500, "Authentication not configured");
        return;
    }

    // Open only until the first account exists. Disabling or removing accounts
    // later must not reopen it unless none are left at all.
    if (m_userMgr->hasAnyUser()) {
        sendError(c, 409, "Setup has already been completed");
        return;
    }

    if (!isAllowedHostHeader(hm, m_allowedHosts)) {
        sendError(c, 403, kUseLocalAddress);
        return;
    }

    if (!isJsonRequest(hm)) {
        sendError(c, 415, "Content-Type must be application/json");
        return;
    }

    std::string username, password;
    if (!parseCredentials(hm, username, password)) {
        sendError(c, 400, "Username and password required");
        return;
    }

    for (const std::string& problem : {security::UserManager::validateUsername(username),
                                       security::UserManager::validatePassword(password)}) {
        if (!problem.empty()) {
            sendError(c, 400, problem);
            return;
        }
    }

    if (!m_userMgr->createUser(username, password, "admin")) {
        sendError(c, 500, "Failed to create account");
        return;
    }

    auto user = m_userMgr->getUser(username);
    if (!user) {
        sendError(c, 500, "Failed to create account");
        return;
    }

    LOG_INFO("First-run setup created administrator: %s", username.c_str());
    startSession(c, *user, 201);
}

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
        if (!registry.hasType(type)) {
            sendError(c, 400, "Invalid device type: " + type);
            return;
        }
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

        // On the request itself first, because parsing it replaces unknown values with defaults
        std::string problem = Automation::validateJson(data);
        if (!problem.empty()) {
            sendError(c, 400, problem);
            return;
        }

        Automation automation = Automation::fromJson(data);
        problem = m_automationMgr->problemWith(automation);
        if (!problem.empty()) {
            sendError(c, 400, problem);
            return;
        }

        // Generate ID if not provided
        if (automation.id.empty()) {
            automation.id = m_automationMgr->generateId();
        } else if (m_automationMgr->getAutomation(automation.id)) {
            sendError(c, 409, "An automation with that id already exists");
            return;
        }

        if (m_automationMgr->addAutomation(automation)) {
            sendJson(c, 201, nlohmann::json{{"success", true}, {"id", automation.id}}.dump());
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
        data.erase("id");  // The URL names the automation

        // On the request itself first, because parsing it replaces unknown values with defaults
        std::string problem = Automation::validateJson(data);
        if (!problem.empty()) {
            sendError(c, 400, problem);
            return;
        }

        Automation automation = Automation::fromJson(data);
        problem = m_automationMgr->problemWith(automation);
        if (!problem.empty()) {
            sendError(c, 400, problem);
            return;
        }
        automation.id = id;  // Ensure ID matches URL

        // A request that doesn't mention these leaves them as they are
        if (!data.contains("enabled")) {
            automation.enabled = existing->enabled;
        }
        if (!data.contains("description")) {
            automation.description = existing->description;
        }
        if (!data.contains("condition")) {
            automation.condition = existing->condition;
        }
        automation.lastTriggeredAt = existing->lastTriggeredAt;

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
        if (!data.is_object() || !data.contains("enabled") || !data["enabled"].is_boolean()) {
            sendError(c, 400, "enabled must be true or false");
            return;
        }
        const bool enabled = data["enabled"].get<bool>();

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
    // Messages can quote what the client sent, which might not even be valid UTF-8
    const std::string json = nlohmann::json{{"error", message}}.dump(
        -1, ' ', false, nlohmann::json::error_handler_t::replace);
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
void WebServer::setSecurityManagers(security::SessionManager*, security::ApiTokenManager*, security::UserManager*) {}
void WebServer::setLoginThrottle(int, int) {}
void WebServer::setAllowedHosts(const std::vector<std::string>&) {}
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
void WebServer::apiLogout(struct mg_connection*, struct mg_http_message*) {}
void WebServer::apiGetSetupStatus(struct mg_connection*) {}
void WebServer::apiSetup(struct mg_connection*, struct mg_http_message*) {}
void WebServer::startSession(struct mg_connection*, const security::User&, int) {}
int WebServer::loginRetryAfterSeconds(const std::string&) { return 0; }
void WebServer::recordLoginFailure(const std::string&) {}
bool WebServer::requiresAdmin(const std::string&, const std::string&) { return true; }
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
