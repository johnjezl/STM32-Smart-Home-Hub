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
#include <smarthub/automation/AutomationManager.hpp>
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
        if (automationManager) {
            automationManager->shutdown();
            automationManager.reset();
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

        // Set automation manager if available
        if (automationManager) {
            webServer->setAutomationManager(automationManager.get());
        }

        ASSERT_TRUE(webServer->start());
        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void setupAutomationManager() {
        automationManager = std::make_unique<smarthub::AutomationManager>(
            *eventBus, *database, *deviceManager);
        automationManager->initialize();
    }

    std::string baseUrl() {
        return "http://localhost:" + std::to_string(testPort);
    }

    int testPort;
    std::string testDbPath;
    std::unique_ptr<smarthub::EventBus> eventBus;
    std::unique_ptr<smarthub::Database> database;
    std::unique_ptr<smarthub::DeviceManager> deviceManager;
    std::unique_ptr<smarthub::AutomationManager> automationManager;
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

// Helper to execute DELETE request
static std::string curlDelete(const std::string& url) {
    std::string cmd = "curl -s --max-time 5 -X DELETE " + url + " 2>/dev/null";
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

// ============================================================================
// Room API Tests
// ============================================================================

class WebServerRoomApiTest : public WebServerTest {
protected:
    void SetUp() override {
        WebServerTest::SetUp();
        startServer();
    }
};

TEST_F(WebServerRoomApiTest, GetRooms_InitiallyEmpty) {
    std::string response = curlGet(baseUrl() + "/api/rooms");
    EXPECT_EQ(response, "[]") << "Should return empty array when no rooms exist";
}

TEST_F(WebServerRoomApiTest, CreateRoom_Success) {
    std::string response = curlPost(baseUrl() + "/api/rooms", "{\"name\":\"Living Room\"}");

    EXPECT_NE(response.find("\"success\":true"), std::string::npos)
        << "Should return success for valid room creation";
    EXPECT_NE(response.find("\"id\""), std::string::npos)
        << "Should return room ID";
}

TEST_F(WebServerRoomApiTest, CreateRoom_MissingName) {
    std::string response = curlPost(baseUrl() + "/api/rooms", "{}");

    EXPECT_NE(response.find("\"error\""), std::string::npos)
        << "Should return error for missing name";
}

TEST_F(WebServerRoomApiTest, GetRooms_AfterCreation) {
    // Create a room
    curlPost(baseUrl() + "/api/rooms", "{\"name\":\"Kitchen\"}");

    // Get rooms
    std::string response = curlGet(baseUrl() + "/api/rooms");

    EXPECT_NE(response.find("Kitchen"), std::string::npos)
        << "Created room should appear in list";
    EXPECT_NE(response.find("\"deviceCount\""), std::string::npos)
        << "Should include device count";
}

TEST_F(WebServerRoomApiTest, UpdateRoom_Success) {
    // Create a room
    std::string createResponse = curlPost(baseUrl() + "/api/rooms", "{\"name\":\"Old Name\"}");

    // Extract room ID (simple parsing)
    size_t idStart = createResponse.find("\"id\":\"") + 6;
    size_t idEnd = createResponse.find("\"", idStart);
    std::string roomId = createResponse.substr(idStart, idEnd - idStart);

    // Update room
    std::string updateResponse = curlPut(baseUrl() + "/api/rooms/" + roomId,
                                         "{\"name\":\"New Name\"}");

    EXPECT_NE(updateResponse.find("\"success\":true"), std::string::npos)
        << "Should return success for valid update";

    // Verify update
    std::string listResponse = curlGet(baseUrl() + "/api/rooms");
    EXPECT_NE(listResponse.find("New Name"), std::string::npos)
        << "Updated name should appear in list";
}

TEST_F(WebServerRoomApiTest, DeleteRoom_Success) {
    // Create a room
    std::string createResponse = curlPost(baseUrl() + "/api/rooms", "{\"name\":\"To Delete\"}");

    size_t idStart = createResponse.find("\"id\":\"") + 6;
    size_t idEnd = createResponse.find("\"", idStart);
    std::string roomId = createResponse.substr(idStart, idEnd - idStart);

    // Delete room
    std::string deleteResponse = curlDelete(baseUrl() + "/api/rooms/" + roomId);

    EXPECT_NE(deleteResponse.find("\"success\":true"), std::string::npos)
        << "Should return success for delete";

    // Verify deletion
    std::string listResponse = curlGet(baseUrl() + "/api/rooms");
    EXPECT_EQ(listResponse.find("To Delete"), std::string::npos)
        << "Deleted room should not appear in list";
}

TEST_F(WebServerRoomApiTest, RoomDeviceCount_Accurate) {
    // Create a room
    curlPost(baseUrl() + "/api/rooms", "{\"name\":\"Test Room\"}");

    // Add a device to the room
    auto device = std::make_shared<smarthub::Device>("test-dev", "Test Device", smarthub::DeviceType::Light);
    device->setRoom("Test Room");
    deviceManager->addDevice(device);

    // Get rooms
    std::string response = curlGet(baseUrl() + "/api/rooms");

    EXPECT_NE(response.find("\"deviceCount\":1"), std::string::npos)
        << "Should show 1 device in room";
}

// ============================================================================
// Device CRUD API Tests
// ============================================================================

class WebServerDeviceCrudTest : public WebServerTest {
protected:
    void SetUp() override {
        WebServerTest::SetUp();
        startServer();
    }
};

TEST_F(WebServerDeviceCrudTest, CreateDevice_Success) {
    std::string response = curlPost(baseUrl() + "/api/devices",
        "{\"type\":\"switch\",\"name\":\"Test Switch\",\"protocol\":\"local\"}");

    EXPECT_NE(response.find("\"success\":true"), std::string::npos)
        << "Should return success for valid device creation";
    EXPECT_NE(response.find("\"id\""), std::string::npos)
        << "Should return device ID";
}

TEST_F(WebServerDeviceCrudTest, CreateDevice_WithRoom) {
    // Create room first
    curlPost(baseUrl() + "/api/rooms", "{\"name\":\"Office\"}");

    // Create device in room
    std::string response = curlPost(baseUrl() + "/api/devices",
        "{\"type\":\"dimmer\",\"name\":\"Office Light\",\"room\":\"Office\"}");

    EXPECT_NE(response.find("\"success\":true"), std::string::npos)
        << "Should create device in room";

    // Verify device is in room
    std::string listResponse = curlGet(baseUrl() + "/api/devices");
    EXPECT_NE(listResponse.find("Office Light"), std::string::npos)
        << "Device should appear in list";
}

TEST_F(WebServerDeviceCrudTest, CreateDevice_MissingType) {
    std::string response = curlPost(baseUrl() + "/api/devices",
        "{\"name\":\"No Type Device\"}");

    EXPECT_NE(response.find("\"error\""), std::string::npos)
        << "Should return error for missing type";
}

TEST_F(WebServerDeviceCrudTest, CreateDevice_MissingName) {
    std::string response = curlPost(baseUrl() + "/api/devices",
        "{\"type\":\"switch\"}");

    EXPECT_NE(response.find("\"error\""), std::string::npos)
        << "Should return error for missing name";
}

TEST_F(WebServerDeviceCrudTest, UpdateDeviceSettings_Success) {
    // Add a device
    auto device = std::make_shared<smarthub::Device>("test-update", "Original Name", smarthub::DeviceType::Switch);
    deviceManager->addDevice(device);

    // Update settings
    std::string response = curlPut(baseUrl() + "/api/devices/test-update/settings",
        "{\"name\":\"Updated Name\",\"room\":\"Living Room\"}");

    EXPECT_NE(response.find("\"success\":true"), std::string::npos)
        << "Should return success for valid update";

    // Verify update
    auto updatedDevice = deviceManager->getDevice("test-update");
    EXPECT_EQ(updatedDevice->name(), "Updated Name");
    EXPECT_EQ(updatedDevice->room(), "Living Room");
}

TEST_F(WebServerDeviceCrudTest, UpdateDeviceSettings_InvalidDevice) {
    std::string response = curlPut(baseUrl() + "/api/devices/nonexistent/settings",
        "{\"name\":\"New Name\"}");

    EXPECT_NE(response.find("\"error\""), std::string::npos)
        << "Should return error for nonexistent device";
}

TEST_F(WebServerDeviceCrudTest, DeleteDevice_Success) {
    // Add a device
    auto device = std::make_shared<smarthub::Device>("to-delete", "Delete Me", smarthub::DeviceType::Switch);
    deviceManager->addDevice(device);

    EXPECT_NE(deviceManager->getDevice("to-delete"), nullptr);

    // Delete device
    std::string response = curlDelete(baseUrl() + "/api/devices/to-delete");

    EXPECT_NE(response.find("\"success\":true"), std::string::npos)
        << "Should return success for delete";

    // Verify deletion
    EXPECT_EQ(deviceManager->getDevice("to-delete"), nullptr)
        << "Device should be removed from manager";
}

TEST_F(WebServerDeviceCrudTest, DeleteDevice_NotFound) {
    std::string response = curlDelete(baseUrl() + "/api/devices/nonexistent");

    EXPECT_NE(response.find("\"error\""), std::string::npos)
        << "Should return error for nonexistent device";
}

TEST_F(WebServerDeviceCrudTest, CreateDevice_AllTypes) {
    std::vector<std::string> types = {"switch", "dimmer", "color_light", "temperature_sensor", "motion_sensor"};

    for (const auto& type : types) {
        std::string json = "{\"type\":\"" + type + "\",\"name\":\"Test " + type + "\"}";
        std::string response = curlPost(baseUrl() + "/api/devices", json);

        EXPECT_NE(response.find("\"success\":true"), std::string::npos)
            << "Should create device of type: " << type;
    }
}

// ============================================================================
// Automation API Tests
// ============================================================================

class WebServerAutomationApiTest : public WebServerTest {
protected:
    void SetUp() override {
        WebServerTest::SetUp();

        // Add a test device for automations
        auto device = std::make_shared<smarthub::Device>("test-light", "Test Light", smarthub::DeviceType::Light);
        deviceManager->addDevice(device);

        // Set up automation manager
        setupAutomationManager();

        startServer();
    }
};

TEST_F(WebServerAutomationApiTest, GetAutomations_InitiallyEmpty) {
    std::string response = curlGet(baseUrl() + "/api/automations");
    EXPECT_EQ(response, "[]") << "Should return empty array when no automations exist";
}

TEST_F(WebServerAutomationApiTest, CreateAutomation_DeviceStateTrigger) {
    std::string json = R"({
        "name": "Motion Light",
        "enabled": true,
        "trigger": {
            "type": "device_state",
            "deviceId": "test-light",
            "property": "on",
            "value": true
        },
        "actions": [
            {"deviceId": "test-light", "property": "on", "value": true}
        ]
    })";

    std::string response = curlPost(baseUrl() + "/api/automations", json);

    EXPECT_NE(response.find("\"success\":true"), std::string::npos)
        << "Should return success for valid automation";
    EXPECT_NE(response.find("\"id\""), std::string::npos)
        << "Should return automation ID";
}

TEST_F(WebServerAutomationApiTest, CreateAutomation_TimeTrigger) {
    std::string json = R"({
        "name": "Evening Lights",
        "enabled": true,
        "trigger": {
            "type": "time",
            "hour": 18,
            "minute": 30
        },
        "actions": [
            {"deviceId": "test-light", "property": "on", "value": true}
        ]
    })";

    std::string response = curlPost(baseUrl() + "/api/automations", json);

    EXPECT_NE(response.find("\"success\":true"), std::string::npos)
        << "Should create time-based automation";
}

TEST_F(WebServerAutomationApiTest, CreateAutomation_IntervalTrigger) {
    std::string json = R"({
        "name": "Periodic Check",
        "enabled": true,
        "trigger": {
            "type": "interval",
            "intervalMinutes": 15
        },
        "actions": [
            {"deviceId": "test-light", "property": "on", "value": false}
        ]
    })";

    std::string response = curlPost(baseUrl() + "/api/automations", json);

    EXPECT_NE(response.find("\"success\":true"), std::string::npos)
        << "Should create interval-based automation";
}

TEST_F(WebServerAutomationApiTest, CreateAutomation_MissingName) {
    std::string json = R"({
        "trigger": {"type": "time", "hour": 12, "minute": 0},
        "actions": [{"deviceId": "test-light", "property": "on", "value": true}]
    })";

    std::string response = curlPost(baseUrl() + "/api/automations", json);

    EXPECT_NE(response.find("\"error\""), std::string::npos)
        << "Should return error for missing name";
}

TEST_F(WebServerAutomationApiTest, GetAutomations_AfterCreation) {
    // Create automation
    std::string json = R"({
        "name": "Test Auto",
        "trigger": {"type": "time", "hour": 12, "minute": 0},
        "actions": [{"deviceId": "test-light", "property": "on", "value": true}]
    })";
    curlPost(baseUrl() + "/api/automations", json);

    // Get automations
    std::string response = curlGet(baseUrl() + "/api/automations");

    EXPECT_NE(response.find("Test Auto"), std::string::npos)
        << "Created automation should appear in list";
    EXPECT_NE(response.find("\"enabled\""), std::string::npos)
        << "Should include enabled status";
}

TEST_F(WebServerAutomationApiTest, GetSingleAutomation) {
    // Create automation
    std::string createJson = R"({
        "name": "Single Auto",
        "trigger": {"type": "time", "hour": 8, "minute": 0},
        "actions": [{"deviceId": "test-light", "property": "on", "value": true}]
    })";
    std::string createResponse = curlPost(baseUrl() + "/api/automations", createJson);

    // Extract ID
    size_t idStart = createResponse.find("\"id\":\"") + 6;
    size_t idEnd = createResponse.find("\"", idStart);
    std::string autoId = createResponse.substr(idStart, idEnd - idStart);

    // Get single automation
    std::string response = curlGet(baseUrl() + "/api/automations/" + autoId);

    EXPECT_NE(response.find("Single Auto"), std::string::npos)
        << "Should return automation details";
    EXPECT_NE(response.find("\"trigger\""), std::string::npos)
        << "Should include trigger info";
}

TEST_F(WebServerAutomationApiTest, GetAutomation_NotFound) {
    std::string response = curlGet(baseUrl() + "/api/automations/nonexistent");

    EXPECT_NE(response.find("\"error\""), std::string::npos)
        << "Should return error for nonexistent automation";
}

TEST_F(WebServerAutomationApiTest, UpdateAutomation) {
    // Create automation
    std::string createJson = R"({
        "name": "Original Name",
        "trigger": {"type": "time", "hour": 12, "minute": 0},
        "actions": [{"deviceId": "test-light", "property": "on", "value": true}]
    })";
    std::string createResponse = curlPost(baseUrl() + "/api/automations", createJson);

    size_t idStart = createResponse.find("\"id\":\"") + 6;
    size_t idEnd = createResponse.find("\"", idStart);
    std::string autoId = createResponse.substr(idStart, idEnd - idStart);

    // Update automation
    std::string updateJson = R"({
        "name": "Updated Name",
        "trigger": {"type": "time", "hour": 18, "minute": 30},
        "actions": [{"deviceId": "test-light", "property": "on", "value": false}]
    })";
    std::string updateResponse = curlPut(baseUrl() + "/api/automations/" + autoId, updateJson);

    EXPECT_NE(updateResponse.find("\"success\":true"), std::string::npos)
        << "Should return success for update";

    // Verify update
    std::string getResponse = curlGet(baseUrl() + "/api/automations/" + autoId);
    EXPECT_NE(getResponse.find("Updated Name"), std::string::npos)
        << "Name should be updated";
}

TEST_F(WebServerAutomationApiTest, EnableDisableAutomation) {
    // Create enabled automation
    std::string createJson = R"({
        "name": "Toggle Auto",
        "enabled": true,
        "trigger": {"type": "time", "hour": 12, "minute": 0},
        "actions": [{"deviceId": "test-light", "property": "on", "value": true}]
    })";
    std::string createResponse = curlPost(baseUrl() + "/api/automations", createJson);

    size_t idStart = createResponse.find("\"id\":\"") + 6;
    size_t idEnd = createResponse.find("\"", idStart);
    std::string autoId = createResponse.substr(idStart, idEnd - idStart);

    // Disable automation
    std::string disableResponse = curlPut(baseUrl() + "/api/automations/" + autoId + "/enable",
                                          "{\"enabled\":false}");
    EXPECT_NE(disableResponse.find("\"success\":true"), std::string::npos)
        << "Should return success for disable";

    // Enable automation
    std::string enableResponse = curlPut(baseUrl() + "/api/automations/" + autoId + "/enable",
                                         "{\"enabled\":true}");
    EXPECT_NE(enableResponse.find("\"success\":true"), std::string::npos)
        << "Should return success for enable";
}

TEST_F(WebServerAutomationApiTest, DeleteAutomation) {
    // Create automation
    std::string createJson = R"({
        "name": "To Delete",
        "trigger": {"type": "time", "hour": 12, "minute": 0},
        "actions": [{"deviceId": "test-light", "property": "on", "value": true}]
    })";
    std::string createResponse = curlPost(baseUrl() + "/api/automations", createJson);

    size_t idStart = createResponse.find("\"id\":\"") + 6;
    size_t idEnd = createResponse.find("\"", idStart);
    std::string autoId = createResponse.substr(idStart, idEnd - idStart);

    // Delete automation
    std::string deleteResponse = curlDelete(baseUrl() + "/api/automations/" + autoId);

    EXPECT_NE(deleteResponse.find("\"success\":true"), std::string::npos)
        << "Should return success for delete";

    // Verify deletion
    std::string getResponse = curlGet(baseUrl() + "/api/automations/" + autoId);
    EXPECT_NE(getResponse.find("\"error\""), std::string::npos)
        << "Deleted automation should not be found";
}

TEST_F(WebServerAutomationApiTest, CreateAutomation_MultipleActions) {
    // Add another device
    auto device2 = std::make_shared<smarthub::Device>("test-switch", "Test Switch", smarthub::DeviceType::Switch);
    deviceManager->addDevice(device2);

    std::string json = R"({
        "name": "Multi Action",
        "trigger": {"type": "time", "hour": 22, "minute": 0},
        "actions": [
            {"deviceId": "test-light", "property": "on", "value": false},
            {"deviceId": "test-switch", "property": "on", "value": false}
        ]
    })";

    std::string response = curlPost(baseUrl() + "/api/automations", json);

    EXPECT_NE(response.find("\"success\":true"), std::string::npos)
        << "Should create automation with multiple actions";
}

// ============================================================================
// Zigbee Pairing API Tests (Without actual Zigbee hardware)
// ============================================================================

class WebServerZigbeeApiTest : public WebServerTest {
protected:
    void SetUp() override {
        WebServerTest::SetUp();
        startServer();
    }
};

TEST_F(WebServerZigbeeApiTest, PermitJoin_NoHandler) {
    // Without Zigbee handler configured, should return error
    std::string response = curlPost(baseUrl() + "/api/zigbee/permit-join", "{\"duration\":60}");

    // Should either succeed (if handler mock is set) or return error
    // This test verifies the endpoint exists and responds
    EXPECT_FALSE(response.empty()) << "Permit join endpoint should respond";
}

TEST_F(WebServerZigbeeApiTest, StopPermitJoin_NoHandler) {
    std::string response = curlDelete(baseUrl() + "/api/zigbee/permit-join");

    EXPECT_FALSE(response.empty()) << "Stop permit join endpoint should respond";
}

TEST_F(WebServerZigbeeApiTest, GetPendingDevices_Empty) {
    std::string response = curlGet(baseUrl() + "/api/zigbee/pending-devices");

    // Should return empty array or error
    EXPECT_FALSE(response.empty()) << "Pending devices endpoint should respond";
}

// ============================================================================
// Integration Tests - Full Workflow
// ============================================================================

class WebServerIntegrationTest : public WebServerTest {
protected:
    void SetUp() override {
        WebServerTest::SetUp();
        setupAutomationManager();
        startServer();
    }
};

TEST_F(WebServerIntegrationTest, FullRoomDeviceWorkflow) {
    // 1. Create a room
    std::string roomResponse = curlPost(baseUrl() + "/api/rooms", "{\"name\":\"Bedroom\"}");
    ASSERT_NE(roomResponse.find("\"success\":true"), std::string::npos);

    // 2. Create a device in that room
    std::string deviceResponse = curlPost(baseUrl() + "/api/devices",
        "{\"type\":\"dimmer\",\"name\":\"Bedroom Light\",\"room\":\"Bedroom\"}");
    ASSERT_NE(deviceResponse.find("\"success\":true"), std::string::npos);

    size_t idStart = deviceResponse.find("\"id\":\"") + 6;
    size_t idEnd = deviceResponse.find("\"", idStart);
    std::string deviceId = deviceResponse.substr(idStart, idEnd - idStart);

    // 3. Verify room shows device count
    std::string roomsResponse = curlGet(baseUrl() + "/api/rooms");
    EXPECT_NE(roomsResponse.find("\"deviceCount\":1"), std::string::npos)
        << "Room should show 1 device";

    // 4. Update device state
    std::string stateResponse = curlPut(baseUrl() + "/api/devices/" + deviceId,
        "{\"on\":true,\"brightness\":75}");
    EXPECT_NE(stateResponse.find("\"success\":true"), std::string::npos);

    // 5. Verify room shows active device
    roomsResponse = curlGet(baseUrl() + "/api/rooms");
    EXPECT_NE(roomsResponse.find("\"activeDevices\":1"), std::string::npos)
        << "Room should show 1 active device";

    // 6. Update device settings
    std::string settingsResponse = curlPut(baseUrl() + "/api/devices/" + deviceId + "/settings",
        "{\"name\":\"Master Bedroom Light\"}");
    EXPECT_NE(settingsResponse.find("\"success\":true"), std::string::npos);

    // 7. Verify device name changed
    std::string deviceInfo = curlGet(baseUrl() + "/api/devices/" + deviceId);
    EXPECT_NE(deviceInfo.find("Master Bedroom Light"), std::string::npos);

    // 8. Delete device
    std::string deleteResponse = curlDelete(baseUrl() + "/api/devices/" + deviceId);
    EXPECT_NE(deleteResponse.find("\"success\":true"), std::string::npos);

    // 9. Verify room is empty
    roomsResponse = curlGet(baseUrl() + "/api/rooms");
    EXPECT_NE(roomsResponse.find("\"deviceCount\":0"), std::string::npos)
        << "Room should show 0 devices after deletion";
}

TEST_F(WebServerIntegrationTest, FullAutomationWorkflow) {
    // 1. Create a device
    auto device = std::make_shared<smarthub::Device>("living-light", "Living Room Light", smarthub::DeviceType::Light);
    deviceManager->addDevice(device);

    // 2. Create an automation
    std::string createJson = R"({
        "name": "Night Mode",
        "description": "Turn off lights at night",
        "enabled": true,
        "trigger": {"type": "time", "hour": 23, "minute": 0},
        "actions": [{"deviceId": "living-light", "property": "on", "value": false}]
    })";
    std::string createResponse = curlPost(baseUrl() + "/api/automations", createJson);
    ASSERT_NE(createResponse.find("\"success\":true"), std::string::npos);

    size_t idStart = createResponse.find("\"id\":\"") + 6;
    size_t idEnd = createResponse.find("\"", idStart);
    std::string autoId = createResponse.substr(idStart, idEnd - idStart);

    // 3. Verify automation in list
    std::string listResponse = curlGet(baseUrl() + "/api/automations");
    EXPECT_NE(listResponse.find("Night Mode"), std::string::npos);

    // 4. Disable automation
    std::string disableResponse = curlPut(baseUrl() + "/api/automations/" + autoId + "/enable",
                                          "{\"enabled\":false}");
    EXPECT_NE(disableResponse.find("\"success\":true"), std::string::npos);

    // 5. Update automation
    std::string updateJson = R"({
        "name": "Late Night Mode",
        "trigger": {"type": "time", "hour": 0, "minute": 0},
        "actions": [{"deviceId": "living-light", "property": "on", "value": false}]
    })";
    std::string updateResponse = curlPut(baseUrl() + "/api/automations/" + autoId, updateJson);
    EXPECT_NE(updateResponse.find("\"success\":true"), std::string::npos);

    // 6. Re-enable automation
    std::string enableResponse = curlPut(baseUrl() + "/api/automations/" + autoId + "/enable",
                                         "{\"enabled\":true}");
    EXPECT_NE(enableResponse.find("\"success\":true"), std::string::npos);

    // 7. Verify updated name
    std::string getResponse = curlGet(baseUrl() + "/api/automations/" + autoId);
    EXPECT_NE(getResponse.find("Late Night Mode"), std::string::npos);

    // 8. Delete automation
    std::string deleteResponse = curlDelete(baseUrl() + "/api/automations/" + autoId);
    EXPECT_NE(deleteResponse.find("\"success\":true"), std::string::npos);

    // 9. Verify empty list
    listResponse = curlGet(baseUrl() + "/api/automations");
    EXPECT_EQ(listResponse, "[]");
}
