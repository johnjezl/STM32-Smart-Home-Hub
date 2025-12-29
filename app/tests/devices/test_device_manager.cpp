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
