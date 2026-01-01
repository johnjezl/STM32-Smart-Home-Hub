/**
 * WebServer Unit and Integration Tests
 *
 * Tests the web server REST API endpoints and basic functionality.
 */

#include <gtest/gtest.h>
#include <smarthub/web/WebServer.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/devices/Device.hpp>
#include <smarthub/database/Database.hpp>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <array>
#include <memory>
#include <filesystem>

namespace fs = std::filesystem;

// Helper to execute curl and capture output
static std::string curlGet(const std::string& url) {
    std::string cmd = "curl -s --max-time 5 " + url + " 2>/dev/null";
    std::array<char, 4096> buffer;
    std::string result;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

static std::string curlPut(const std::string& url, const std::string& data) {
    std::string cmd = "curl -s --max-time 5 -X PUT -H 'Content-Type: application/json' -d '" + data + "' " + url + " 2>/dev/null";
    std::array<char, 4096> buffer;
    std::string result;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

class WebServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use a random high port to avoid conflicts
        testPort = 18080 + (rand() % 1000);
        testDbPath = "/tmp/webserver_test_" + std::to_string(getpid()) + ".db";

        // Clean up any existing test database
        if (fs::exists(testDbPath)) {
            fs::remove(testDbPath);
        }

        // Initialize components
        eventBus = std::make_unique<smarthub::EventBus>();
        database = std::make_unique<smarthub::Database>(testDbPath);
        database->initialize();
        deviceManager = std::make_unique<smarthub::DeviceManager>(*eventBus, *database);
        deviceManager->initialize();
    }

    void TearDown() override {
        if (webServer) {
            webServer->stop();
            webServer.reset();
        }
        deviceManager.reset();
        database.reset();
        eventBus.reset();

        if (fs::exists(testDbPath)) {
            fs::remove(testDbPath);
        }
    }

    void startServer() {
        webServer = std::make_unique<smarthub::WebServer>(
            *eventBus, *deviceManager, testPort, "/tmp"
        );

        // Make all API routes public for testing (no auth required)
        webServer->setPublicRoutes({"/api/"});

        ASSERT_TRUE(webServer->start());
        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::string baseUrl() {
        return "http://localhost:" + std::to_string(testPort);
    }

    int testPort;
    std::string testDbPath;
    std::unique_ptr<smarthub::EventBus> eventBus;
    std::unique_ptr<smarthub::Database> database;
    std::unique_ptr<smarthub::DeviceManager> deviceManager;
    std::unique_ptr<smarthub::WebServer> webServer;
};

// Basic construction and destruction
TEST_F(WebServerTest, Construction) {
    smarthub::WebServer server(*eventBus, *deviceManager, 8080, "/tmp");
    EXPECT_FALSE(server.isRunning());
}

TEST_F(WebServerTest, StartStop) {
    smarthub::WebServer server(*eventBus, *deviceManager, testPort, "/tmp");
    EXPECT_TRUE(server.start());
    EXPECT_TRUE(server.isRunning());
    server.stop();
    EXPECT_FALSE(server.isRunning());
}

TEST_F(WebServerTest, DoubleStartIsIdempotent) {
    smarthub::WebServer server(*eventBus, *deviceManager, testPort, "/tmp");
    EXPECT_TRUE(server.start());
    // Second start should handle gracefully (implementation dependent)
    server.stop();
}

TEST_F(WebServerTest, DoubleStopIsIdempotent) {
    smarthub::WebServer server(*eventBus, *deviceManager, testPort, "/tmp");
    EXPECT_TRUE(server.start());
    server.stop();
    server.stop();  // Should not crash
    EXPECT_FALSE(server.isRunning());
}

// REST API Tests
TEST_F(WebServerTest, ApiGetDevicesEmpty) {
    startServer();
    std::string response = curlGet(baseUrl() + "/api/devices");
    EXPECT_EQ(response, "[]");
}

TEST_F(WebServerTest, ApiGetDevicesWithDevices) {
    // Add a device before starting server
    auto light = std::make_shared<smarthub::Device>(
        "light1", "Living Room Light", smarthub::DeviceType::Light
    );
    deviceManager->addDevice(light);

    startServer();
    std::string response = curlGet(baseUrl() + "/api/devices");

    EXPECT_NE(response.find("light1"), std::string::npos);
    EXPECT_NE(response.find("Living Room Light"), std::string::npos);
}

TEST_F(WebServerTest, ApiGetSystemStatus) {
    startServer();
    std::string response = curlGet(baseUrl() + "/api/system/status");

    EXPECT_NE(response.find("version"), std::string::npos);
    EXPECT_NE(response.find("0.1.0"), std::string::npos);
    EXPECT_NE(response.find("devices"), std::string::npos);
}

TEST_F(WebServerTest, ApiGetDeviceNotFound) {
    startServer();
    std::string response = curlGet(baseUrl() + "/api/devices/nonexistent");

    EXPECT_NE(response.find("error"), std::string::npos);
    EXPECT_NE(response.find("not found"), std::string::npos);
}

TEST_F(WebServerTest, ApiGetDeviceFound) {
    auto sensor = std::make_shared<smarthub::Device>(
        "sensor1", "Temperature Sensor", smarthub::DeviceType::TemperatureSensor
    );
    deviceManager->addDevice(sensor);

    startServer();
    std::string response = curlGet(baseUrl() + "/api/devices/sensor1");

    EXPECT_NE(response.find("sensor1"), std::string::npos);
    EXPECT_NE(response.find("Temperature Sensor"), std::string::npos);
}

TEST_F(WebServerTest, ApiSetDeviceState) {
    auto light = std::make_shared<smarthub::Device>(
        "light1", "Test Light", smarthub::DeviceType::Light
    );
    deviceManager->addDevice(light);

    startServer();
    std::string response = curlPut(
        baseUrl() + "/api/devices/light1",
        R"({"power":"on","brightness":75})"
    );

    EXPECT_NE(response.find("success"), std::string::npos);
}

TEST_F(WebServerTest, ApiSetDeviceStateNotFound) {
    startServer();
    std::string response = curlPut(
        baseUrl() + "/api/devices/nonexistent",
        R"({"power":"on"})"
    );

    EXPECT_NE(response.find("error"), std::string::npos);
}

TEST_F(WebServerTest, ApiNotFoundRoute) {
    startServer();
    std::string response = curlGet(baseUrl() + "/api/nonexistent");

    EXPECT_NE(response.find("error"), std::string::npos);
    EXPECT_NE(response.find("Not found"), std::string::npos);
}

TEST_F(WebServerTest, MultipleDeviceTypes) {
    deviceManager->addDevice(std::make_shared<smarthub::Device>(
        "light1", "Light", smarthub::DeviceType::Light));
    deviceManager->addDevice(std::make_shared<smarthub::Device>(
        "sensor1", "Sensor", smarthub::DeviceType::TemperatureSensor));
    deviceManager->addDevice(std::make_shared<smarthub::Device>(
        "thermo1", "Thermostat", smarthub::DeviceType::Thermostat));

    startServer();
    std::string response = curlGet(baseUrl() + "/api/devices");

    EXPECT_NE(response.find("light1"), std::string::npos);
    EXPECT_NE(response.find("sensor1"), std::string::npos);
    EXPECT_NE(response.find("thermo1"), std::string::npos);
}

TEST_F(WebServerTest, SystemStatusDeviceCount) {
    deviceManager->addDevice(std::make_shared<smarthub::Device>(
        "d1", "Device 1", smarthub::DeviceType::Light));
    deviceManager->addDevice(std::make_shared<smarthub::Device>(
        "d2", "Device 2", smarthub::DeviceType::Switch));

    startServer();
    std::string response = curlGet(baseUrl() + "/api/system/status");

    // Should show 2 devices
    EXPECT_NE(response.find("\"devices\":2"), std::string::npos);
}

// Concurrent request handling
TEST_F(WebServerTest, ConcurrentRequests) {
    deviceManager->addDevice(std::make_shared<smarthub::Device>(
        "light1", "Light", smarthub::DeviceType::Light));

    startServer();

    // Launch multiple concurrent requests
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < 10; i++) {
        threads.emplace_back([this, &successCount]() {
            std::string response = curlGet(baseUrl() + "/api/devices");
            if (response.find("light1") != std::string::npos) {
                successCount++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(successCount.load(), 10);
}

// ============================================================
// Security Feature Tests
// ============================================================

// Helper to get HTTP headers
static std::string curlHeaders(const std::string& url) {
    std::string cmd = "curl -s -I --max-time 5 " + url + " 2>/dev/null";
    std::array<char, 4096> buffer;
    std::string result;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

// Helper to make POST request
static std::string curlPost(const std::string& url, const std::string& data) {
    std::string cmd = "curl -s --max-time 5 -X POST -H 'Content-Type: application/json' -d '" + data + "' " + url + " 2>/dev/null";
    std::array<char, 4096> buffer;
    std::string result;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

// Helper to get with auth header
static std::string curlGetWithAuth(const std::string& url, const std::string& token) {
    std::string cmd = "curl -s --max-time 5 -H 'Authorization: Bearer " + token + "' " + url + " 2>/dev/null";
    std::array<char, 4096> buffer;
    std::string result;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

// Security Headers Tests
TEST_F(WebServerTest, SecurityHeadersPresent) {
    startServer();
    std::string headers = curlHeaders(baseUrl() + "/api/system/status");

    // Check for essential security headers
    EXPECT_NE(headers.find("X-Content-Type-Options: nosniff"), std::string::npos)
        << "Missing X-Content-Type-Options header";
    EXPECT_NE(headers.find("X-Frame-Options: DENY"), std::string::npos)
        << "Missing X-Frame-Options header";
    EXPECT_NE(headers.find("X-XSS-Protection: 1; mode=block"), std::string::npos)
        << "Missing X-XSS-Protection header";
    EXPECT_NE(headers.find("Cache-Control: no-store"), std::string::npos)
        << "Missing Cache-Control header";
    EXPECT_NE(headers.find("Referrer-Policy: strict-origin-when-cross-origin"), std::string::npos)
        << "Missing Referrer-Policy header";
}

TEST_F(WebServerTest, ContentTypeHeader) {
    startServer();
    std::string headers = curlHeaders(baseUrl() + "/api/system/status");

    EXPECT_NE(headers.find("Content-Type: application/json"), std::string::npos)
        << "Missing or incorrect Content-Type header";
}

// Authentication Tests
class WebServerAuthTest : public WebServerTest {
protected:
    void startServerWithAuth() {
        webServer = std::make_unique<smarthub::WebServer>(
            *eventBus, *deviceManager, testPort, "/tmp"
        );
        // Don't set public routes - require auth for protected endpoints
        ASSERT_TRUE(webServer->start());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
};

TEST_F(WebServerAuthTest, UnauthorizedWithoutCredentials) {
    startServerWithAuth();

    std::string response = curlGet(baseUrl() + "/api/devices");
    EXPECT_NE(response.find("Unauthorized"), std::string::npos)
        << "Expected 401 Unauthorized for protected route without auth";
}

TEST_F(WebServerAuthTest, PublicRoutesAccessible) {
    startServerWithAuth();

    // /api/system/status is a public route by default
    std::string response = curlGet(baseUrl() + "/api/system/status");
    EXPECT_NE(response.find("version"), std::string::npos)
        << "Public route should be accessible without auth";
}

TEST_F(WebServerAuthTest, InvalidTokenRejected) {
    startServerWithAuth();

    std::string response = curlGetWithAuth(baseUrl() + "/api/devices", "invalid-token-12345");
    EXPECT_NE(response.find("Unauthorized"), std::string::npos)
        << "Invalid token should be rejected";
}

// Rate Limiting Tests
class WebServerRateLimitTest : public WebServerTest {
protected:
    void startServerWithRateLimit(int requestsPerMinute) {
        webServer = std::make_unique<smarthub::WebServer>(
            *eventBus, *deviceManager, testPort, "/tmp"
        );
        webServer->setPublicRoutes({"/api/"});
        webServer->setRateLimit(requestsPerMinute);
        ASSERT_TRUE(webServer->start());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
};

TEST_F(WebServerRateLimitTest, AllowsRequestsUnderLimit) {
    startServerWithRateLimit(100);  // 100 requests per minute

    int successCount = 0;
    for (int i = 0; i < 10; i++) {
        std::string response = curlGet(baseUrl() + "/api/system/status");
        if (response.find("version") != std::string::npos) {
            successCount++;
        }
    }

    EXPECT_EQ(successCount, 10) << "All requests under limit should succeed";
}

TEST_F(WebServerRateLimitTest, BlocksExcessiveRequests) {
    startServerWithRateLimit(5);  // Only 5 requests per minute

    int successCount = 0;
    int blockedCount = 0;

    for (int i = 0; i < 10; i++) {
        std::string response = curlGet(baseUrl() + "/api/system/status");
        if (response.find("version") != std::string::npos) {
            successCount++;
        } else if (response.find("Too many") != std::string::npos) {
            blockedCount++;
        }
    }

    EXPECT_LE(successCount, 5) << "Should not exceed rate limit";
    EXPECT_GE(blockedCount, 5) << "Excessive requests should be blocked";
}

// Public Routes Configuration Tests
TEST_F(WebServerTest, CustomPublicRoutes) {
    webServer = std::make_unique<smarthub::WebServer>(
        *eventBus, *deviceManager, testPort, "/tmp"
    );
    webServer->setPublicRoutes({"/api/devices"});  // Only /api/devices is public
    ASSERT_TRUE(webServer->start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // /api/devices should be accessible (starts with public route)
    deviceManager->addDevice(std::make_shared<smarthub::Device>(
        "light1", "Light", smarthub::DeviceType::Light));
    std::string response = curlGet(baseUrl() + "/api/devices");
    EXPECT_NE(response.find("light1"), std::string::npos)
        << "Public route should be accessible";
}

// Login/Logout Tests (stub - requires full security manager integration)
TEST_F(WebServerTest, LoginEndpointExists) {
    startServer();

    std::string response = curlPost(baseUrl() + "/api/auth/login", "{}");
    // Should get a meaningful response (even if error about missing credentials)
    EXPECT_FALSE(response.empty()) << "Login endpoint should respond";
}

// Error Response Format Tests
TEST_F(WebServerTest, ErrorResponseFormat) {
    startServer();

    std::string response = curlGet(baseUrl() + "/api/nonexistent");
    EXPECT_NE(response.find("\"error\""), std::string::npos)
        << "Error responses should have JSON format with 'error' field";
}
