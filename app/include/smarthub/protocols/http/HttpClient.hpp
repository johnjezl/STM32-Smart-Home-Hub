/**
 * SmartHub HTTP Client
 *
 * Simple HTTP client for REST API communication with WiFi devices.
 * Uses Mongoose for HTTP client functionality.
 */
#pragma once

#include <string>
#include <optional>
#include <functional>
#include <map>
#include <mutex>
#include <condition_variable>
#include <nlohmann/json.hpp>

namespace smarthub::http {

/**
 * HTTP response structure
 */
struct HttpResponse {
    int statusCode = 0;
    std::string body;
    std::map<std::string, std::string> headers;

    bool ok() const { return statusCode >= 200 && statusCode < 300; }

    nlohmann::json json() const {
        if (body.empty()) return nlohmann::json{};
        try {
            return nlohmann::json::parse(body);
        } catch (...) {
            return nlohmann::json{};
        }
    }
};

/**
 * HTTP request options
 */
struct HttpRequestOptions {
    int timeoutMs = 5000;
    std::map<std::string, std::string> headers;
    std::string contentType = "application/json";
};

/**
 * Interface for HTTP client (for dependency injection in tests)
 */
class IHttpClient {
public:
    virtual ~IHttpClient() = default;

    virtual std::optional<HttpResponse> get(const std::string& url,
                                             const HttpRequestOptions& options = {}) = 0;
    virtual std::optional<HttpResponse> post(const std::string& url,
                                              const std::string& body,
                                              const HttpRequestOptions& options = {}) = 0;
    virtual std::optional<nlohmann::json> getJson(const std::string& url,
                                                   int timeoutMs = 5000) = 0;
};

/**
 * HTTP Client for device communication
 *
 * Thread-safe HTTP client that uses Mongoose's event-driven model
 * but provides a synchronous interface for simplicity.
 */
class HttpClient : public IHttpClient {
public:
    HttpClient();
    ~HttpClient() override;

    // Non-copyable
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    /**
     * Perform HTTP GET request
     * @param url Full URL including http:// or https://
     * @param options Request options (timeout, headers)
     * @return Response or nullopt on error
     */
    std::optional<HttpResponse> get(const std::string& url,
                                     const HttpRequestOptions& options = {}) override;

    /**
     * Perform HTTP POST request
     * @param url Full URL
     * @param body Request body
     * @param options Request options
     * @return Response or nullopt on error
     */
    std::optional<HttpResponse> post(const std::string& url,
                                      const std::string& body,
                                      const HttpRequestOptions& options = {}) override;

    /**
     * Perform HTTP PUT request
     */
    std::optional<HttpResponse> put(const std::string& url,
                                     const std::string& body,
                                     const HttpRequestOptions& options = {});

    /**
     * GET request returning JSON
     */
    std::optional<nlohmann::json> getJson(const std::string& url,
                                           int timeoutMs = 5000) override;

    /**
     * POST request with JSON body, returning JSON
     */
    std::optional<nlohmann::json> postJson(const std::string& url,
                                            const nlohmann::json& body,
                                            int timeoutMs = 5000);

private:
    std::optional<HttpResponse> request(const std::string& method,
                                         const std::string& url,
                                         const std::string& body,
                                         const HttpRequestOptions& options);

    struct RequestContext;
    static void eventHandler(struct mg_connection* c, int ev, void* ev_data);

    std::mutex m_mutex;
};

} // namespace smarthub::http
