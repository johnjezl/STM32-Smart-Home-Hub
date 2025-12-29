/**
 * DeviceManager Unit Tests
 */

#include <gtest/gtest.h>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/devices/Device.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/database/Database.hpp>
#include <filesystem>
#include <memory>

namespace fs = std::filesystem;

class DeviceManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up any existing test database
        if (fs::exists(testDbPath)) {
            fs::remove(testDbPath);
        }
        database = std::make_unique<smarthub::Database>(testDbPath);
        database->initialize();
    }

    void TearDown() override {
        database.reset();
        if (fs::exists(testDbPath)) {
            fs::remove(testDbPath);
        }
    }

    const std::string testDbPath = "/tmp/smarthub_devicemgr_test.db";
    smarthub::EventBus eventBus;
    std::unique_ptr<smarthub::Database> database;
};

TEST_F(DeviceManagerTest, AddDevice) {
    smarthub::DeviceManager manager(eventBus, *database);
    manager.initialize();

    auto device = std::make_shared<smarthub::Device>(
        "light1", "Living Room Light", smarthub::DeviceType::Light
    );

    EXPECT_TRUE(manager.addDevice(device));
    EXPECT_EQ(manager.deviceCount(), 1u);
}

TEST_F(DeviceManagerTest, AddDuplicateDevice) {
    smarthub::DeviceManager manager(eventBus, *database);
    manager.initialize();

    auto device1 = std::make_shared<smarthub::Device>(
        "light1", "Light 1", smarthub::DeviceType::Light
    );
    auto device2 = std::make_shared<smarthub::Device>(
        "light1", "Light 2", smarthub::DeviceType::Light
    );

    EXPECT_TRUE(manager.addDevice(device1));
    EXPECT_FALSE(manager.addDevice(device2));  // Same ID
    EXPECT_EQ(manager.deviceCount(), 1u);
}

TEST_F(DeviceManagerTest, GetDevice) {
    smarthub::DeviceManager manager(eventBus, *database);
    manager.initialize();

    auto device = std::make_shared<smarthub::Device>(
        "sensor1", "Temperature Sensor", smarthub::DeviceType::Sensor
    );
    manager.addDevice(device);

    auto retrieved = manager.getDevice("sensor1");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->id(), "sensor1");
    EXPECT_EQ(retrieved->name(), "Temperature Sensor");
}

TEST_F(DeviceManagerTest, GetNonexistentDevice) {
    smarthub::DeviceManager manager(eventBus, *database);
    manager.initialize();

    auto device = manager.getDevice("nonexistent");
    EXPECT_EQ(device, nullptr);
}

TEST_F(DeviceManagerTest, RemoveDevice) {
    smarthub::DeviceManager manager(eventBus, *database);
    manager.initialize();

    auto device = std::make_shared<smarthub::Device>(
        "light1", "Light", smarthub::DeviceType::Light
    );
    manager.addDevice(device);
    EXPECT_EQ(manager.deviceCount(), 1u);

    EXPECT_TRUE(manager.removeDevice("light1"));
    EXPECT_EQ(manager.deviceCount(), 0u);
    EXPECT_EQ(manager.getDevice("light1"), nullptr);
}

TEST_F(DeviceManagerTest, RemoveNonexistentDevice) {
    smarthub::DeviceManager manager(eventBus, *database);
    manager.initialize();

    EXPECT_FALSE(manager.removeDevice("nonexistent"));
}

TEST_F(DeviceManagerTest, GetAllDevices) {
    smarthub::DeviceManager manager(eventBus, *database);
    manager.initialize();

    manager.addDevice(std::make_shared<smarthub::Device>(
        "light1", "Light 1", smarthub::DeviceType::Light
    ));
    manager.addDevice(std::make_shared<smarthub::Device>(
        "light2", "Light 2", smarthub::DeviceType::Light
    ));
    manager.addDevice(std::make_shared<smarthub::Device>(
        "sensor1", "Sensor", smarthub::DeviceType::Sensor
    ));

    auto devices = manager.getAllDevices();
    EXPECT_EQ(devices.size(), 3u);
}

TEST_F(DeviceManagerTest, GetDevicesByType) {
    smarthub::DeviceManager manager(eventBus, *database);
    manager.initialize();

    manager.addDevice(std::make_shared<smarthub::Device>(
        "light1", "Light 1", smarthub::DeviceType::Light
    ));
    manager.addDevice(std::make_shared<smarthub::Device>(
        "light2", "Light 2", smarthub::DeviceType::Light
    ));
    manager.addDevice(std::make_shared<smarthub::Device>(
        "sensor1", "Sensor", smarthub::DeviceType::Sensor
    ));

    auto lights = manager.getDevicesByType(static_cast<int>(smarthub::DeviceType::Light));
    EXPECT_EQ(lights.size(), 2u);

    auto sensors = manager.getDevicesByType(static_cast<int>(smarthub::DeviceType::Sensor));
    EXPECT_EQ(sensors.size(), 1u);

    auto locks = manager.getDevicesByType(static_cast<int>(smarthub::DeviceType::Lock));
    EXPECT_EQ(locks.size(), 0u);
}

TEST_F(DeviceManagerTest, GetDevicesByRoom) {
    smarthub::DeviceManager manager(eventBus, *database);
    manager.initialize();

    auto light1 = std::make_shared<smarthub::Device>(
        "light1", "Light 1", smarthub::DeviceType::Light
    );
    light1->setRoom("Living Room");
    manager.addDevice(light1);

    auto light2 = std::make_shared<smarthub::Device>(
        "light2", "Light 2", smarthub::DeviceType::Light
    );
    light2->setRoom("Bedroom");
    manager.addDevice(light2);

    auto sensor = std::make_shared<smarthub::Device>(
        "sensor1", "Sensor", smarthub::DeviceType::Sensor
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

TEST_F(DeviceManagerTest, DeviceCount) {
    smarthub::DeviceManager manager(eventBus, *database);
    manager.initialize();

    EXPECT_EQ(manager.deviceCount(), 0u);

    manager.addDevice(std::make_shared<smarthub::Device>(
        "d1", "Device 1", smarthub::DeviceType::Light
    ));
    EXPECT_EQ(manager.deviceCount(), 1u);

    manager.addDevice(std::make_shared<smarthub::Device>(
        "d2", "Device 2", smarthub::DeviceType::Light
    ));
    EXPECT_EQ(manager.deviceCount(), 2u);

    manager.removeDevice("d1");
    EXPECT_EQ(manager.deviceCount(), 1u);
}

TEST_F(DeviceManagerTest, SaveAllDevices) {
    smarthub::DeviceManager manager(eventBus, *database);
    manager.initialize();

    manager.addDevice(std::make_shared<smarthub::Device>(
        "light1", "Light 1", smarthub::DeviceType::Light
    ));
    manager.addDevice(std::make_shared<smarthub::Device>(
        "sensor1", "Sensor 1", smarthub::DeviceType::Sensor
    ));

    EXPECT_TRUE(manager.saveAllDevices());
}

TEST_F(DeviceManagerTest, Shutdown) {
    smarthub::DeviceManager manager(eventBus, *database);
    manager.initialize();

    manager.addDevice(std::make_shared<smarthub::Device>(
        "light1", "Light", smarthub::DeviceType::Light
    ));

    // Shutdown should not throw
    EXPECT_NO_THROW(manager.shutdown());
}
