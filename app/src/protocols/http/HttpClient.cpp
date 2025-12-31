/**
 * SmartHub HTTP Client Implementation
 */

#include <smarthub/protocols/http/HttpClient.hpp>
#include <smarthub/core/Logger.hpp>

#if __has_include("mongoose.h")
#include "mongoose.h"
#define HAVE_MONGOOSE 1
#else
#define HAVE_MONGOOSE 0
#endif

#include <chrono>

namespace smarthub::http {

#if HAVE_MONGOOSE

struct HttpClient::RequestContext {
    bool done = false;
    bool error = false;
    HttpResponse response;
    std::condition_variable cv;
    std::mutex mutex;
};

HttpClient::HttpClient() = default;
HttpClient::~HttpClient() = default;

void HttpClient::eventHandler(struct mg_connection* c, int ev, void* ev_data) {
    auto* ctx = static_cast<RequestContext*>(c->fn_data);

    if (ev == MG_EV_CONNECT) {
        // Connection established or failed
        if (mg_url_is_ssl(static_cast<const char*>(c->data))) {
            // SSL not fully handled here - would need TLS config
        }
    } else if (ev == MG_EV_HTTP_MSG) {
        // Got full HTTP response
        auto* hm = static_cast<struct mg_http_message*>(ev_data);

        {
            std::lock_guard<std::mutex> lock(ctx->mutex);
            ctx->response.statusCode = mg_http_status(hm);
            ctx->response.body = std::string(hm->body.buf, hm->body.len);

            // Parse headers
            for (int i = 0; i < MG_MAX_HTTP_HEADERS && hm->headers[i].name.buf != nullptr; i++) {
                std::string name(hm->headers[i].name.buf, hm->headers[i].name.len);
                std::string value(hm->headers[i].value.buf, hm->headers[i].value.len);
                ctx->response.headers[name] = value;
            }

            ctx->done = true;
        }
        ctx->cv.notify_one();

        // Close connection
        c->is_draining = 1;
    } else if (ev == MG_EV_ERROR) {
        LOG_ERROR("HTTP client error: %s", static_cast<const char*>(ev_data));
        {
            std::lock_guard<std::mutex> lock(ctx->mutex);
            ctx->error = true;
            ctx->done = true;
        }
        ctx->cv.notify_one();
    } else if (ev == MG_EV_CLOSE) {
        // Connection closed
        {
            std::lock_guard<std::mutex> lock(ctx->mutex);
            if (!ctx->done) {
                ctx->error = true;
            }
            ctx->done = true;
        }
        ctx->cv.notify_one();
    }
}

std::optional<HttpResponse> HttpClient::request(const std::string& method,
                                                  const std::string& url,
                                                  const std::string& body,
                                                  const HttpRequestOptions& options) {
    std::lock_guard<std::mutex> lock(m_mutex);

    RequestContext ctx;
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);

    // Build headers string
    std::string headerStr;
    for (const auto& [key, value] : options.headers) {
        headerStr += key + ": " + value + "\r\n";
    }
    if (!body.empty() && options.headers.find("Content-Type") == options.headers.end()) {
        headerStr += "Content-Type: " + options.contentType + "\r\n";
    }

    // Create connection
    struct mg_connection* c = mg_http_connect(&mgr, url.c_str(), eventHandler, &ctx);
    if (!c) {
        LOG_ERROR("Failed to create HTTP connection to %s", url.c_str());
        mg_mgr_free(&mgr);
        return std::nullopt;
    }

    // Store URL for SSL check
    c->data = const_cast<char*>(url.c_str());

    // Send request once connected
    struct mg_str host = mg_url_host(url.c_str());

    if (body.empty()) {
        mg_printf(c,
            "%s %s HTTP/1.1\r\n"
            "Host: %.*s\r\n"
            "Connection: close\r\n"
            "%s"
            "\r\n",
            method.c_str(),
            mg_url_uri(url.c_str()),
            static_cast<int>(host.len), host.buf,
            headerStr.c_str());
    } else {
        mg_printf(c,
            "%s %s HTTP/1.1\r\n"
            "Host: %.*s\r\n"
            "Connection: close\r\n"
            "Content-Length: %zu\r\n"
            "%s"
            "\r\n"
            "%s",
            method.c_str(),
            mg_url_uri(url.c_str()),
            static_cast<int>(host.len), host.buf,
            body.size(),
            headerStr.c_str(),
            body.c_str());
    }

    // Poll until done or timeout
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(options.timeoutMs);

    while (!ctx.done) {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();

        if (remaining <= 0) {
            LOG_WARN("HTTP request to %s timed out", url.c_str());
            mg_mgr_free(&mgr);
            return std::nullopt;
        }

        mg_mgr_poll(&mgr, std::min<long>(remaining, 100));
    }

    mg_mgr_free(&mgr);

    if (ctx.error) {
        return std::nullopt;
    }

    return ctx.response;
}

std::optional<HttpResponse> HttpClient::get(const std::string& url,
                                             const HttpRequestOptions& options) {
    return request("GET", url, "", options);
}

std::optional<HttpResponse> HttpClient::post(const std::string& url,
                                              const std::string& body,
                                              const HttpRequestOptions& options) {
    return request("POST", url, body, options);
}

std::optional<HttpResponse> HttpClient::put(const std::string& url,
                                             const std::string& body,
                                             const HttpRequestOptions& options) {
    return request("PUT", url, body, options);
}

std::optional<nlohmann::json> HttpClient::getJson(const std::string& url, int timeoutMs) {
    HttpRequestOptions opts;
    opts.timeoutMs = timeoutMs;

    auto response = get(url, opts);
    if (response && response->ok()) {
        try {
            return nlohmann::json::parse(response->body);
        } catch (const std::exception& e) {
            LOG_WARN("Failed to parse JSON from %s: %s", url.c_str(), e.what());
        }
    }
    return std::nullopt;
}

std::optional<nlohmann::json> HttpClient::postJson(const std::string& url,
                                                    const nlohmann::json& body,
                                                    int timeoutMs) {
    HttpRequestOptions opts;
    opts.timeoutMs = timeoutMs;
    opts.contentType = "application/json";

    auto response = post(url, body.dump(), opts);
    if (response && response->ok()) {
        try {
            return nlohmann::json::parse(response->body);
        } catch (const std::exception& e) {
            LOG_WARN("Failed to parse JSON response from %s: %s", url.c_str(), e.what());
        }
    }
    return std::nullopt;
}

#else // !HAVE_MONGOOSE

// Stub implementation when Mongoose is not available
HttpClient::HttpClient() {
    LOG_WARN("HTTP client not compiled in - Mongoose not available");
}
HttpClient::~HttpClient() = default;

std::optional<HttpResponse> HttpClient::get(const std::string&, const HttpRequestOptions&) {
    return std::nullopt;
}
std::optional<HttpResponse> HttpClient::post(const std::string&, const std::string&,
                                              const HttpRequestOptions&) {
    return std::nullopt;
}
std::optional<HttpResponse> HttpClient::put(const std::string&, const std::string&,
                                             const HttpRequestOptions&) {
    return std::nullopt;
}
std::optional<nlohmann::json> HttpClient::getJson(const std::string&, int) {
    return std::nullopt;
}
std::optional<nlohmann::json> HttpClient::postJson(const std::string&, const nlohmann::json&, int) {
    return std::nullopt;
}
std::optional<HttpResponse> HttpClient::request(const std::string&, const std::string&,
                                                 const std::string&, const HttpRequestOptions&) {
    return std::nullopt;
}

#endif // HAVE_MONGOOSE

} // namespace smarthub::http
