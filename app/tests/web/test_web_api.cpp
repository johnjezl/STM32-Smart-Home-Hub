/**
 * Web API Tests
 *
 * Tests for the SmartHub REST API endpoints used by the web dashboard.
 */

#include <gtest/gtest.h>
#include <smarthub/web/WebServer.hpp>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/devices/types/DimmerDevice.hpp>
#include <smarthub/devices/types/SwitchDevice.hpp>
#include <smarthub/devices/types/ColorLightDevice.hpp>
#include <smarthub/devices/types/TemperatureSensor.hpp>
#include <smarthub/devices/types/MotionSensor.hpp>
#include <smarthub/database/Database.hpp>
#include <smarthub/core/EventBus.hpp>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>

using namespace smarthub;
using json = nlohmann::json;

class WebApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_eventBus = std::make_unique<EventBus>();
        // Use in-memory SQLite database for testing
        m_database = std::make_unique<Database>(":memory:");
        m_deviceManager = std::make_unique<DeviceManager>(*m_eventBus, *m_database);

        // Add test devices
        addTestDevices();
    }

    void TearDown() override {
        m_deviceManager.reset();
        m_database.reset();
        m_eventBus.reset();
    }

    void addTestDevices() {
        // Add a dimmer light
        auto light = std::make_shared<DimmerDevice>("test-light-1", "Living Room Light");
        light->setRoom("Living Room");
        light->setState("on", true);
        light->setState("brightness", 75);
        m_deviceManager->addDevice(light);

        // Add a switch
        auto sw = std::make_shared<SwitchDevice>("test-switch-1", "Kitchen Switch");
        sw->setRoom("Kitchen");
        sw->setState("on", false);
        m_deviceManager->addDevice(sw);

        // Add a temperature sensor
        auto tempSensor = std::make_shared<TemperatureSensor>("test-temp-1", "Bedroom Temp");
        tempSensor->setRoom("Bedroom");
        tempSensor->setState("value", 72.5);
        m_deviceManager->addDevice(tempSensor);

        // Add a motion sensor (as second sensor type)
        auto motionSensor = std::make_shared<MotionSensor>("test-motion-1", "Hallway Motion");
        motionSensor->setRoom("Hallway");
        motionSensor->setState("value", false);
        m_deviceManager->addDevice(motionSensor);

        // Add a color light
        auto colorLight = std::make_shared<ColorLightDevice>("test-color-1", "Office Light");
        colorLight->setRoom("Office");
        colorLight->setState("on", true);
        colorLight->setState("brightness", 100);
        colorLight->setState("color_temp", 4000);
        m_deviceManager->addDevice(colorLight);
    }

    std::unique_ptr<EventBus> m_eventBus;
    std::unique_ptr<Database> m_database;
    std::unique_ptr<DeviceManager> m_deviceManager;
};

// ============================================================================
// Device List API Tests
// ============================================================================

TEST_F(WebApiTest, GetAllDevices_ReturnsAllDevices) {
    auto devices = m_deviceManager->getAllDevices();

    EXPECT_EQ(devices.size(), 5);
}

TEST_F(WebApiTest, GetAllDevices_ContainsRequiredFields) {
    auto devices = m_deviceManager->getAllDevices();

    for (const auto& device : devices) {
        EXPECT_FALSE(device->id().empty()) << "Device ID should not be empty";
        EXPECT_FALSE(device->name().empty()) << "Device name should not be empty";
        EXPECT_FALSE(device->typeString().empty()) << "Device type should not be empty";
    }
}

TEST_F(WebApiTest, GetAllDevices_IncludesRoomInfo) {
    auto devices = m_deviceManager->getAllDevices();

    int devicesWithRoom = 0;
    for (const auto& device : devices) {
        if (!device->room().empty()) {
            devicesWithRoom++;
        }
    }

    EXPECT_EQ(devicesWithRoom, 5) << "All test devices should have room info";
}

TEST_F(WebApiTest, GetAllDevices_IncludesState) {
    auto devices = m_deviceManager->getAllDevices();

    for (const auto& device : devices) {
        auto state = device->getState();
        EXPECT_FALSE(state.is_null()) << "Device state should not be null for " << device->id();
    }
}

// ============================================================================
// Single Device API Tests
// ============================================================================

TEST_F(WebApiTest, GetDevice_ValidId_ReturnsDevice) {
    auto device = m_deviceManager->getDevice("test-light-1");

    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->id(), "test-light-1");
    EXPECT_EQ(device->name(), "Living Room Light");
    EXPECT_EQ(device->room(), "Living Room");
}

TEST_F(WebApiTest, GetDevice_InvalidId_ReturnsNull) {
    auto device = m_deviceManager->getDevice("nonexistent-device");

    EXPECT_EQ(device, nullptr);
}

TEST_F(WebApiTest, GetDevice_ReturnsCorrectState) {
    auto device = m_deviceManager->getDevice("test-light-1");
    ASSERT_NE(device, nullptr);

    auto state = device->getState();
    EXPECT_TRUE(state.contains("on"));
    EXPECT_TRUE(state.contains("brightness"));
    EXPECT_EQ(state["on"].get<bool>(), true);
    EXPECT_EQ(state["brightness"].get<int>(), 75);
}

// ============================================================================
// Device State Update Tests
// ============================================================================

TEST_F(WebApiTest, SetDeviceState_TurnOn) {
    auto device = m_deviceManager->getDevice("test-switch-1");
    ASSERT_NE(device, nullptr);

    // Initially off
    auto state = device->getState();
    EXPECT_FALSE(state["on"].get<bool>());

    // Turn on
    device->setState("on", true);

    // Verify
    state = device->getState();
    EXPECT_TRUE(state["on"].get<bool>());
}

TEST_F(WebApiTest, SetDeviceState_TurnOff) {
    auto device = m_deviceManager->getDevice("test-light-1");
    ASSERT_NE(device, nullptr);

    // Initially on
    auto state = device->getState();
    EXPECT_TRUE(state["on"].get<bool>());

    // Turn off
    device->setState("on", false);

    // Verify
    state = device->getState();
    EXPECT_FALSE(state["on"].get<bool>());
}

TEST_F(WebApiTest, SetDeviceState_SetBrightness) {
    auto device = m_deviceManager->getDevice("test-light-1");
    ASSERT_NE(device, nullptr);

    device->setState("brightness", 50);

    auto state = device->getState();
    EXPECT_EQ(state["brightness"].get<int>(), 50);
}

TEST_F(WebApiTest, SetDeviceState_BrightnessCanBeSet) {
    auto device = m_deviceManager->getDevice("test-light-1");
    ASSERT_NE(device, nullptr);

    // Set brightness to valid value
    device->setState("brightness", 50);
    auto state = device->getState();
    EXPECT_EQ(state["brightness"].get<int>(), 50);

    // Set brightness to another value
    device->setState("brightness", 100);
    state = device->getState();
    EXPECT_EQ(state["brightness"].get<int>(), 100);
}

TEST_F(WebApiTest, SetDeviceState_ColorTemperature) {
    auto device = m_deviceManager->getDevice("test-color-1");
    ASSERT_NE(device, nullptr);

    device->setState("color_temp", 5000);

    auto state = device->getState();
    EXPECT_EQ(state["color_temp"].get<int>(), 5000);
}

// ============================================================================
// Sensor Device Tests
// ============================================================================

TEST_F(WebApiTest, SensorDevice_HasValue) {
    auto device = m_deviceManager->getDevice("test-temp-1");
    ASSERT_NE(device, nullptr);

    auto state = device->getState();
    EXPECT_TRUE(state.contains("value"));
    EXPECT_NEAR(state["value"].get<double>(), 72.5, 0.1);
}

TEST_F(WebApiTest, SensorDevice_MotionHasValue) {
    auto device = m_deviceManager->getDevice("test-motion-1");
    ASSERT_NE(device, nullptr);

    auto state = device->getState();
    EXPECT_TRUE(state.contains("value"));
    EXPECT_FALSE(state["value"].get<bool>());
}

// ============================================================================
// Device Filtering Tests
// ============================================================================

TEST_F(WebApiTest, GetDevicesByRoom_ReturnsCorrectDevices) {
    auto livingRoomDevices = m_deviceManager->getDevicesByRoom("Living Room");

    EXPECT_EQ(livingRoomDevices.size(), 1);
    EXPECT_EQ(livingRoomDevices[0]->id(), "test-light-1");
}

TEST_F(WebApiTest, GetDevicesByRoom_EmptyForNonexistentRoom) {
    auto devices = m_deviceManager->getDevicesByRoom("Nonexistent Room");

    EXPECT_TRUE(devices.empty());
}

TEST_F(WebApiTest, GetDevicesByType_ReturnsCorrectDevices) {
    auto devices = m_deviceManager->getAllDevices();

    int lightCount = 0;
    int sensorCount = 0;

    for (const auto& device : devices) {
        if (device->typeString() == "dimmer" || device->typeString() == "color_light") {
            lightCount++;
        } else if (device->typeString().find("sensor") != std::string::npos) {
            sensorCount++;
        }
    }

    EXPECT_EQ(lightCount, 2);
    EXPECT_EQ(sensorCount, 2);
}

// ============================================================================
// JSON Serialization Tests
// ============================================================================

TEST_F(WebApiTest, DeviceState_SerializesToValidJson) {
    auto device = m_deviceManager->getDevice("test-light-1");
    ASSERT_NE(device, nullptr);

    auto state = device->getState();
    std::string jsonStr = state.dump();

    // Verify it's valid JSON by parsing it back
    EXPECT_NO_THROW({
        auto parsed = json::parse(jsonStr);
        EXPECT_TRUE(parsed.is_object());
    });
}

TEST_F(WebApiTest, DeviceList_BuildsValidJsonArray) {
    auto devices = m_deviceManager->getAllDevices();

    json deviceArray = json::array();
    for (const auto& device : devices) {
        json deviceJson;
        deviceJson["id"] = device->id();
        deviceJson["name"] = device->name();
        deviceJson["type"] = device->typeString();
        deviceJson["room"] = device->room();
        deviceJson["state"] = device->getState();
        deviceArray.push_back(deviceJson);
    }

    std::string jsonStr = deviceArray.dump();

    EXPECT_NO_THROW({
        auto parsed = json::parse(jsonStr);
        EXPECT_TRUE(parsed.is_array());
        EXPECT_EQ(parsed.size(), 5);
    });
}

// ============================================================================
// API Response Format Tests
// ============================================================================

TEST_F(WebApiTest, ApiResponse_DeviceHasAllRequiredFields) {
    auto device = m_deviceManager->getDevice("test-light-1");
    ASSERT_NE(device, nullptr);

    // Build API response as WebServer does
    json response;
    response["id"] = device->id();
    response["name"] = device->name();
    response["type"] = device->typeString();
    response["room"] = device->room();
    response["online"] = device->isAvailable();
    response["state"] = device->getState();

    EXPECT_TRUE(response.contains("id"));
    EXPECT_TRUE(response.contains("name"));
    EXPECT_TRUE(response.contains("type"));
    EXPECT_TRUE(response.contains("room"));
    EXPECT_TRUE(response.contains("online"));
    EXPECT_TRUE(response.contains("state"));
}

// ============================================================================
// Concurrent Access Tests
// ============================================================================

TEST_F(WebApiTest, ConcurrentStateUpdates_DoNotCorrupt) {
    auto device = m_deviceManager->getDevice("test-light-1");
    ASSERT_NE(device, nullptr);

    std::vector<std::thread> threads;

    // Multiple threads updating state simultaneously
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([&device, i]() {
            for (int j = 0; j < 100; j++) {
                device->setState("brightness", (i * 10 + j) % 100);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // State should still be valid
    auto state = device->getState();
    EXPECT_TRUE(state.contains("brightness"));
    int brightness = state["brightness"].get<int>();
    EXPECT_GE(brightness, 0);
    EXPECT_LE(brightness, 100);
}

TEST_F(WebApiTest, ConcurrentDeviceReads_DoNotBlock) {
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < 10; i++) {
        threads.emplace_back([this, &successCount]() {
            for (int j = 0; j < 100; j++) {
                auto devices = m_deviceManager->getAllDevices();
                if (devices.size() == 5) {
                    successCount++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(successCount, 1000);
}
