/**
 * DeviceManager Unit Tests - Phase 3
 */

#include <gtest/gtest.h>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/devices/Device.hpp>
#include <smarthub/devices/types/SwitchDevice.hpp>
#include <smarthub/devices/types/TemperatureSensor.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/database/Database.hpp>
#include <filesystem>
#include <memory>

namespace fs = std::filesystem;
using namespace smarthub;

class DeviceManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up any existing test database
        if (fs::exists(testDbPath)) {
            fs::remove(testDbPath);
        }
        database = std::make_unique<Database>(testDbPath);
        database->initialize();
    }

    void TearDown() override {
        database.reset();
        if (fs::exists(testDbPath)) {
            fs::remove(testDbPath);
        }
    }

    const std::string testDbPath = "/tmp/smarthub_devicemgr_test.db";
    EventBus eventBus;
    std::unique_ptr<Database> database;
};

TEST_F(DeviceManagerTest, AddDevice) {
    DeviceManager manager(eventBus, *database);
    manager.initialize();

    auto device = std::make_shared<Device>(
        "light1", "Living Room Light", DeviceType::Light
    );

    EXPECT_TRUE(manager.addDevice(device));
    EXPECT_EQ(manager.deviceCount(), 1u);
}

TEST_F(DeviceManagerTest, AddDuplicateDevice) {
    DeviceManager manager(eventBus, *database);
    manager.initialize();

    auto device1 = std::make_shared<Device>(
        "light1", "Light 1", DeviceType::Light
    );
    auto device2 = std::make_shared<Device>(
        "light1", "Light 2", DeviceType::Light
    );

    EXPECT_TRUE(manager.addDevice(device1));
    EXPECT_FALSE(manager.addDevice(device2));  // Same ID
    EXPECT_EQ(manager.deviceCount(), 1u);
}

TEST_F(DeviceManagerTest, GetDevice) {
    DeviceManager manager(eventBus, *database);
    manager.initialize();

    auto device = std::make_shared<TemperatureSensor>(
        "sensor1", "Temperature Sensor"
    );
    manager.addDevice(device);

    auto retrieved = manager.getDevice("sensor1");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->id(), "sensor1");
    EXPECT_EQ(retrieved->name(), "Temperature Sensor");
}

TEST_F(DeviceManagerTest, GetNonexistentDevice) {
    DeviceManager manager(eventBus, *database);
    manager.initialize();

    auto device = manager.getDevice("nonexistent");
    EXPECT_EQ(device, nullptr);
}

TEST_F(DeviceManagerTest, RemoveDevice) {
    DeviceManager manager(eventBus, *database);
    manager.initialize();

    auto device = std::make_shared<Device>(
        "light1", "Light", DeviceType::Light
    );
    manager.addDevice(device);
    EXPECT_EQ(manager.deviceCount(), 1u);

    EXPECT_TRUE(manager.removeDevice("light1"));
    EXPECT_EQ(manager.deviceCount(), 0u);
    EXPECT_EQ(manager.getDevice("light1"), nullptr);
}

TEST_F(DeviceManagerTest, RemoveNonexistentDevice) {
    DeviceManager manager(eventBus, *database);
    manager.initialize();

    EXPECT_FALSE(manager.removeDevice("nonexistent"));
}

TEST_F(DeviceManagerTest, GetAllDevices) {
    DeviceManager manager(eventBus, *database);
    manager.initialize();

    manager.addDevice(std::make_shared<Device>(
        "light1", "Light 1", DeviceType::Light
    ));
    manager.addDevice(std::make_shared<Device>(
        "light2", "Light 2", DeviceType::Light
    ));
    manager.addDevice(std::make_shared<TemperatureSensor>(
        "sensor1", "Sensor"
    ));

    auto devices = manager.getAllDevices();
    EXPECT_EQ(devices.size(), 3u);
}

TEST_F(DeviceManagerTest, GetDevicesByType) {
    DeviceManager manager(eventBus, *database);
    manager.initialize();

    manager.addDevice(std::make_shared<Device>(
        "light1", "Light 1", DeviceType::Light
    ));
    manager.addDevice(std::make_shared<Device>(
        "light2", "Light 2", DeviceType::Light
    ));
    manager.addDevice(std::make_shared<TemperatureSensor>(
        "sensor1", "Sensor"
    ));

    auto lights = manager.getDevicesByType(DeviceType::Light);
    EXPECT_EQ(lights.size(), 2u);

    auto sensors = manager.getDevicesByType(DeviceType::TemperatureSensor);
    EXPECT_EQ(sensors.size(), 1u);

    auto locks = manager.getDevicesByType(DeviceType::Lock);
    EXPECT_EQ(locks.size(), 0u);
}

TEST_F(DeviceManagerTest, GetDevicesByRoom) {
    DeviceManager manager(eventBus, *database);
    manager.initialize();

    auto light1 = std::make_shared<Device>(
        "light1", "Light 1", DeviceType::Light
    );
    light1->setRoom("Living Room");
    manager.addDevice(light1);

    auto light2 = std::make_shared<Device>(
        "light2", "Light 2", DeviceType::Light
    );
    light2->setRoom("Bedroom");
    manager.addDevice(light2);

    auto sensor = std::make_shared<TemperatureSensor>(
        "sensor1", "Sensor"
    );
    sensor->setRoom("Living Room");
    manager.addDevice(sensor);

    auto livingRoomDevices = manager.getDevicesByRoom("Living Room");
    EXPECT_EQ(livingRoomDevices.size(), 2u);

    auto bedroomDevices = manager.getDevicesByRoom("Bedroom");
    EXPECT_EQ(bedroomDevices.size(), 1u);

    auto kitchenDevices = manager.getDevicesByRoom("Kitchen");
    EXPECT_EQ(kitchenDevices.size(), 0u);
}

TEST_F(DeviceManagerTest, GetDevicesByProtocol) {
    DeviceManager manager(eventBus, *database);
    manager.initialize();

    manager.addDevice(std::make_shared<Device>(
        "mqtt1", "MQTT Device 1", DeviceType::Switch, "mqtt", "zigbee2mqtt/0x1234"
    ));
    manager.addDevice(std::make_shared<Device>(
        "mqtt2", "MQTT Device 2", DeviceType::Switch, "mqtt", "zigbee2mqtt/0x5678"
    ));
    manager.addDevice(std::make_shared<Device>(
        "local1", "Local Device", DeviceType::Switch, "local", ""
    ));

    auto mqttDevices = manager.getDevicesByProtocol("mqtt");
    EXPECT_EQ(mqttDevices.size(), 2u);

    auto localDevices = manager.getDevicesByProtocol("local");
    EXPECT_EQ(localDevices.size(), 1u);
}

TEST_F(DeviceManagerTest, DeviceCount) {
    DeviceManager manager(eventBus, *database);
    manager.initialize();

    EXPECT_EQ(manager.deviceCount(), 0u);

    manager.addDevice(std::make_shared<Device>(
        "d1", "Device 1", DeviceType::Light
    ));
    EXPECT_EQ(manager.deviceCount(), 1u);

    manager.addDevice(std::make_shared<Device>(
        "d2", "Device 2", DeviceType::Light
    ));
    EXPECT_EQ(manager.deviceCount(), 2u);

    manager.removeDevice("d1");
    EXPECT_EQ(manager.deviceCount(), 1u);
}

TEST_F(DeviceManagerTest, SaveAllDevices) {
    DeviceManager manager(eventBus, *database);
    manager.initialize();

    manager.addDevice(std::make_shared<Device>(
        "light1", "Light 1", DeviceType::Light
    ));
    manager.addDevice(std::make_shared<TemperatureSensor>(
        "sensor1", "Sensor 1"
    ));

    EXPECT_TRUE(manager.saveAllDevices());
}

TEST_F(DeviceManagerTest, Shutdown) {
    DeviceManager manager(eventBus, *database);
    manager.initialize();

    manager.addDevice(std::make_shared<Device>(
        "light1", "Light", DeviceType::Light
    ));

    // Shutdown should not throw
    EXPECT_NO_THROW(manager.shutdown());
}

TEST_F(DeviceManagerTest, Poll) {
    DeviceManager manager(eventBus, *database);
    manager.initialize();

    // Poll should not throw even with no protocols loaded
    EXPECT_NO_THROW(manager.poll());
}

TEST_F(DeviceManagerTest, LoadedProtocols) {
    DeviceManager manager(eventBus, *database);
    manager.initialize();

    // Initially no protocols loaded
    EXPECT_TRUE(manager.loadedProtocols().empty());
}

TEST_F(DeviceManagerTest, Discovery) {
    DeviceManager manager(eventBus, *database);
    manager.initialize();

    EXPECT_FALSE(manager.isDiscovering());

    // Start/stop discovery should not throw
    EXPECT_NO_THROW(manager.startDiscovery());
    EXPECT_NO_THROW(manager.stopDiscovery());
}

// ============================================================================
// Device Persistence Tests
// ============================================================================

TEST_F(DeviceManagerTest, DevicePersistsAcrossRestart) {
    // Add device with first manager instance
    {
        DeviceManager manager(eventBus, *database);
        manager.initialize();

        auto device = std::make_shared<SwitchDevice>(
            "switch1", "Test Switch", "zigbee", "0x1234"
        );
        device->setRoom("living_room");
        EXPECT_TRUE(manager.addDevice(device));
        EXPECT_EQ(manager.deviceCount(), 1u);

        manager.shutdown();
    }

    // Create new manager instance - device should be loaded from database
    {
        DeviceManager manager2(eventBus, *database);
        manager2.initialize();

        EXPECT_EQ(manager2.deviceCount(), 1u);

        auto loaded = manager2.getDevice("switch1");
        ASSERT_NE(loaded, nullptr);
        EXPECT_EQ(loaded->name(), "Test Switch");
        EXPECT_EQ(loaded->protocol(), "zigbee");
        EXPECT_EQ(loaded->protocolAddress(), "0x1234");
        EXPECT_EQ(loaded->room(), "living_room");
    }
}

TEST_F(DeviceManagerTest, DeviceRemovedFromDatabaseOnDelete) {
    // Add device
    {
        DeviceManager manager(eventBus, *database);
        manager.initialize();

        auto device = std::make_shared<SwitchDevice>(
            "switch1", "Test Switch", "local", ""
        );
        EXPECT_TRUE(manager.addDevice(device));
        EXPECT_EQ(manager.deviceCount(), 1u);

        // Remove device
        EXPECT_TRUE(manager.removeDevice("switch1"));
        EXPECT_EQ(manager.deviceCount(), 0u);

        manager.shutdown();
    }

    // New manager instance should not have the device
    {
        DeviceManager manager2(eventBus, *database);
        manager2.initialize();

        EXPECT_EQ(manager2.deviceCount(), 0u);
        EXPECT_EQ(manager2.getDevice("switch1"), nullptr);
    }
}

TEST_F(DeviceManagerTest, MultipleDevicesPersist) {
    // Add multiple devices
    {
        DeviceManager manager(eventBus, *database);
        manager.initialize();

        auto switch1 = std::make_shared<SwitchDevice>("sw1", "Switch 1", "local", "");
        auto switch2 = std::make_shared<SwitchDevice>("sw2", "Switch 2", "zigbee", "0xABCD");
        auto sensor1 = std::make_shared<TemperatureSensor>("temp1", "Temp Sensor", "zigbee", "0x5678");

        switch1->setRoom("kitchen");
        switch2->setRoom("bedroom");
        sensor1->setRoom("bedroom");

        EXPECT_TRUE(manager.addDevice(switch1));
        EXPECT_TRUE(manager.addDevice(switch2));
        EXPECT_TRUE(manager.addDevice(sensor1));
        EXPECT_EQ(manager.deviceCount(), 3u);

        manager.shutdown();
    }

    // Verify all devices loaded
    {
        DeviceManager manager2(eventBus, *database);
        manager2.initialize();

        EXPECT_EQ(manager2.deviceCount(), 3u);

        auto sw1 = manager2.getDevice("sw1");
        auto sw2 = manager2.getDevice("sw2");
        auto temp1 = manager2.getDevice("temp1");

        ASSERT_NE(sw1, nullptr);
        ASSERT_NE(sw2, nullptr);
        ASSERT_NE(temp1, nullptr);

        EXPECT_EQ(sw1->room(), "kitchen");
        EXPECT_EQ(sw2->room(), "bedroom");
        EXPECT_EQ(temp1->room(), "bedroom");

        // Check device types loaded correctly
        EXPECT_EQ(sw1->type(), DeviceType::Switch);
        EXPECT_EQ(sw2->type(), DeviceType::Switch);
        EXPECT_EQ(temp1->type(), DeviceType::TemperatureSensor);
    }
}

TEST_F(DeviceManagerTest, DeviceStatePersistedOnRestart) {
    // Add device and set state
    {
        DeviceManager manager(eventBus, *database);
        manager.initialize();

        auto device = std::make_shared<SwitchDevice>("sw1", "Switch", "local", "");
        manager.addDevice(device);
        manager.setDeviceState("sw1", "on", true);
        manager.saveAllDevices();  // Explicitly save state

        manager.shutdown();
    }

    // State should be loaded
    {
        DeviceManager manager2(eventBus, *database);
        manager2.initialize();

        auto device = manager2.getDevice("sw1");
        ASSERT_NE(device, nullptr);

        auto state = device->getState();
        EXPECT_TRUE(state.contains("on"));
        EXPECT_TRUE(state["on"].get<bool>());
    }
}
