/**
 * Integration Tests
 *
 * Tests that verify multiple components work together correctly.
 */

#include <gtest/gtest.h>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/core/Config.hpp>
#include <smarthub/database/Database.hpp>
#include <smarthub/devices/Device.hpp>
#include <smarthub/devices/DeviceManager.hpp>
#include <filesystem>
#include <fstream>
#include <memory>

namespace fs = std::filesystem;

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up test files
        if (fs::exists(testDbPath)) fs::remove(testDbPath);
        if (fs::exists(testConfigPath)) fs::remove(testConfigPath);

        // Create test config
        std::ofstream configFile(testConfigPath);
        configFile << R"(
database:
  path: )" << testDbPath << R"(

mqtt:
  broker: 127.0.0.1
  port: 1883

web:
  port: 8080

logging:
  level: debug
)";
        configFile.close();
    }

    void TearDown() override {
        if (fs::exists(testDbPath)) fs::remove(testDbPath);
        if (fs::exists(testConfigPath)) fs::remove(testConfigPath);
    }

    const std::string testDbPath = "/tmp/smarthub_integration_test.db";
    const std::string testConfigPath = "/tmp/smarthub_integration_config.yaml";
};

TEST_F(IntegrationTest, ConfigLoadAndDatabaseInit) {
    smarthub::Config config;
    ASSERT_TRUE(config.load(testConfigPath));

    smarthub::Database db(config.databasePath());
    ASSERT_TRUE(db.initialize());
    EXPECT_TRUE(db.isOpen());
}

TEST_F(IntegrationTest, DeviceManagerWithDatabase) {
    smarthub::EventBus eventBus;
    smarthub::Database db(testDbPath);
    db.initialize();

    smarthub::DeviceManager manager(eventBus, db);
    manager.initialize();

    // Add devices
    auto light = std::make_shared<smarthub::Device>(
        "light1", "Living Room Light", smarthub::DeviceType::Light
    );
    manager.addDevice(light);

    EXPECT_EQ(manager.deviceCount(), 1u);
    EXPECT_TRUE(manager.saveAllDevices());
}

TEST_F(IntegrationTest, DevicePersistenceToDatabase) {
    smarthub::Database db(testDbPath);
    db.initialize();

    // Create and store device manually in database (protocol is required)
    db.execute(
        "INSERT INTO devices (id, name, type, protocol, room) VALUES "
        "('sensor1', 'Temperature Sensor', 'sensor', 'local', 'Living Room')"
    );

    // Retrieve from database using prepared statement
    auto stmt = db.prepare("SELECT name, type, room FROM devices WHERE id = 'sensor1'");
    ASSERT_NE(stmt, nullptr);
    ASSERT_TRUE(stmt->step());

    EXPECT_EQ(stmt->getString(0), "Temperature Sensor");
    EXPECT_EQ(stmt->getString(1), "sensor");
    EXPECT_EQ(stmt->getString(2), "Living Room");
}

TEST_F(IntegrationTest, EventBusAsyncProcessing) {
    smarthub::EventBus eventBus;

    std::atomic<int> syncCount{0};
    std::atomic<int> asyncCount{0};

    eventBus.subscribe("TestEvent", [&](const smarthub::Event& /*e*/) {
        syncCount++;
    });

    eventBus.subscribe("AsyncTestEvent", [&](const smarthub::Event& /*e*/) {
        asyncCount++;
    });

    // Custom test events
    class TestEvent : public smarthub::Event {
    public:
        std::string type() const override { return "TestEvent"; }
    };

    class AsyncTestEvent : public smarthub::Event {
    public:
        std::string type() const override { return "AsyncTestEvent"; }
    };

    // Sync publish
    TestEvent syncEvent;
    eventBus.publish(syncEvent);
    EXPECT_EQ(syncCount.load(), 1);

    // Async publish
    eventBus.publishAsync(std::make_unique<AsyncTestEvent>());
    EXPECT_EQ(asyncCount.load(), 0);  // Not processed yet

    eventBus.processQueue();
    EXPECT_EQ(asyncCount.load(), 1);  // Now processed
}

TEST_F(IntegrationTest, MultipleDeviceTypesCoexist) {
    smarthub::EventBus eventBus;
    smarthub::Database db(testDbPath);
    db.initialize();

    smarthub::DeviceManager manager(eventBus, db);
    manager.initialize();

    // Add various device types
    manager.addDevice(std::make_shared<smarthub::Device>(
        "light1", "Light", smarthub::DeviceType::Light
    ));
    manager.addDevice(std::make_shared<smarthub::Device>(
        "sensor1", "Sensor", smarthub::DeviceType::Sensor
    ));
    manager.addDevice(std::make_shared<smarthub::Device>(
        "thermo1", "Thermostat", smarthub::DeviceType::Thermostat
    ));
    manager.addDevice(std::make_shared<smarthub::Device>(
        "lock1", "Lock", smarthub::DeviceType::Lock
    ));

    EXPECT_EQ(manager.deviceCount(), 4u);
    EXPECT_EQ(manager.getDevicesByType(static_cast<int>(smarthub::DeviceType::Light)).size(), 1u);
    EXPECT_EQ(manager.getDevicesByType(static_cast<int>(smarthub::DeviceType::Sensor)).size(), 1u);
    EXPECT_EQ(manager.getDevicesByType(static_cast<int>(smarthub::DeviceType::Thermostat)).size(), 1u);
    EXPECT_EQ(manager.getDevicesByType(static_cast<int>(smarthub::DeviceType::Lock)).size(), 1u);
}

TEST_F(IntegrationTest, SensorHistoryLogging) {
    smarthub::Database db(testDbPath);
    db.initialize();

    // First create a device (required by foreign key constraint)
    ASSERT_TRUE(db.execute(
        "INSERT INTO devices (id, name, type, protocol) VALUES "
        "('sensor1', 'Temperature Sensor', 'sensor', 'local')"
    ));

    // Insert sensor readings into sensor_history table
    int64_t timestamp = 1700000000;  // Fixed timestamp for test
    for (int i = 0; i < 10; i++) {
        auto stmt = db.prepare("INSERT INTO sensor_history (device_id, property, value, timestamp) VALUES (?, ?, ?, ?)");
        ASSERT_NE(stmt, nullptr);
        stmt->bind(1, "sensor1")
            .bind(2, "temperature")
            .bind(3, 20.0 + i * 0.5)
            .bind(4, static_cast<int64_t>(timestamp + i));
        EXPECT_TRUE(stmt->execute());
    }

    // Query readings
    auto stmt = db.prepare("SELECT value FROM sensor_history WHERE device_id = 'sensor1' AND property = 'temperature'");
    ASSERT_NE(stmt, nullptr);

    int count = 0;
    double sum = 0.0;
    while (stmt->step()) {
        count++;
        sum += stmt->getDouble(0);
    }

    EXPECT_EQ(count, 10);
    EXPECT_NEAR(sum / count, 22.25, 0.01);  // Average temperature
}

TEST_F(IntegrationTest, ConfigToComponentWiring) {
    // Load config
    smarthub::Config config;
    ASSERT_TRUE(config.load(testConfigPath));

    // Use config values to initialize components
    smarthub::Database db(config.databasePath());
    ASSERT_TRUE(db.initialize());

    // Verify config was applied correctly
    EXPECT_EQ(config.mqttBroker(), "127.0.0.1");
    EXPECT_EQ(config.mqttPort(), 1883);
    EXPECT_EQ(config.webPort(), 8080);
}

TEST_F(IntegrationTest, DeviceStateCallback) {
    smarthub::Device device("light1", "Light", smarthub::DeviceType::Light);

    std::vector<std::pair<std::string, std::any>> stateChanges;

    device.setStateCallback([&](const std::string& property, const std::any& value) {
        stateChanges.emplace_back(property, value);
    });

    device.setState("power", std::string("on"));
    device.setState("brightness", 75);
    device.setState("power", std::string("off"));

    EXPECT_EQ(stateChanges.size(), 3u);
    EXPECT_EQ(stateChanges[0].first, "power");
    EXPECT_EQ(stateChanges[1].first, "brightness");
    EXPECT_EQ(stateChanges[2].first, "power");
}

TEST_F(IntegrationTest, EventBusDeviceStateEvents) {
    smarthub::EventBus eventBus;

    std::vector<std::string> deviceIds;

    eventBus.subscribe("device.state", [&](const smarthub::Event& e) {
        auto& stateEvent = static_cast<const smarthub::DeviceStateEvent&>(e);
        deviceIds.push_back(stateEvent.deviceId);
    });

    // Publish device state event
    smarthub::DeviceStateEvent event;
    event.deviceId = "light1";
    event.property = "power";
    event.value = std::string("on");
    eventBus.publish(event);

    EXPECT_EQ(deviceIds.size(), 1u);
    EXPECT_EQ(deviceIds[0], "light1");
}
