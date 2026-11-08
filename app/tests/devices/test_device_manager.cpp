/**
 * DeviceManager Unit Tests - Phase 3
 */

#include <gtest/gtest.h>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/devices/Device.hpp>
#include <smarthub/devices/types/SwitchDevice.hpp>
#include <smarthub/devices/DeviceTypeRegistry.hpp>
#include <smarthub/devices/types/MotionSensor.hpp>
#include <smarthub/devices/types/TemperatureSensor.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/database/Database.hpp>
#include "../mocks/MockProtocolHandler.hpp"
#include <ctime>
#include <filesystem>
#include <memory>
#include <thread>
#include <vector>

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

// Joins a thread during shutdown that calls back into DeviceManager, as a
// protocol's background threads do.
class JoinsCallbackThreadOnShutdown : public smarthub::testing::MockProtocolHandler {
public:
    using smarthub::testing::MockProtocolHandler::MockProtocolHandler;

    void shutdown() override {
        std::thread callback([this] {
            simulateDeviceDiscovered(std::make_shared<Device>(
                "late", "Late Device", DeviceType::Light));
        });
        callback.join();
        MockProtocolHandler::shutdown();
    }
};

// Joins a thread during startup that calls back into DeviceManager, as a protocol
// does if startup fails while a device is joining.
class JoinsCallbackThreadOnInitialize : public smarthub::testing::MockProtocolHandler {
public:
    using smarthub::testing::MockProtocolHandler::MockProtocolHandler;

    bool initialize() override {
        std::thread callback([this] {
            simulateDeviceDiscovered(std::make_shared<Device>(
                "early", "Early Device", DeviceType::Light));
        });
        callback.join();
        return false;
    }
};

TEST_F(DeviceManagerTest, FailedLoadDoesNotDeadlockWithProtocolCallbacks) {
    ProtocolFactory::instance().registerProtocol("joins_on_initialize",
        [](EventBus& bus, const nlohmann::json& config) -> ProtocolHandlerPtr {
            return std::make_shared<JoinsCallbackThreadOnInitialize>(bus, config);
        });

    DeviceManager manager(eventBus, *database);
    manager.initialize();

    EXPECT_FALSE(manager.loadProtocol("joins_on_initialize", nlohmann::json::object()));
    EXPECT_TRUE(manager.loadedProtocols().empty());
    ProtocolFactory::instance().unregisterProtocol("joins_on_initialize");
}

TEST_F(DeviceManagerTest, ShutdownDoesNotDeadlockWithProtocolCallbacks) {
    ProtocolFactory::instance().registerProtocol("joins_on_shutdown",
        [](EventBus& bus, const nlohmann::json& config) -> ProtocolHandlerPtr {
            return std::make_shared<JoinsCallbackThreadOnShutdown>(bus, config);
        });

    DeviceManager manager(eventBus, *database);
    manager.initialize();
    ASSERT_TRUE(manager.loadProtocol("joins_on_shutdown", nlohmann::json::object()));

    manager.shutdown();

    EXPECT_EQ(manager.deviceCount(), 0u);
    ProtocolFactory::instance().unregisterProtocol("joins_on_shutdown");
}

TEST_F(DeviceManagerTest, UnloadDoesNotDeadlockWithProtocolCallbacks) {
    ProtocolFactory::instance().registerProtocol("joins_on_shutdown",
        [](EventBus& bus, const nlohmann::json& config) -> ProtocolHandlerPtr {
            return std::make_shared<JoinsCallbackThreadOnShutdown>(bus, config);
        });

    DeviceManager manager(eventBus, *database);
    manager.initialize();
    ASSERT_TRUE(manager.loadProtocol("joins_on_shutdown", nlohmann::json::object()));

    manager.unloadProtocol("joins_on_shutdown");

    EXPECT_TRUE(manager.loadedProtocols().empty());
    EXPECT_EQ(manager.deviceCount(), 1u);  // The callback's device still arrived
    ProtocolFactory::instance().unregisterProtocol("joins_on_shutdown");
}

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

// ---------- Device types through a save and load

namespace {

std::vector<DeviceType> everyDeviceType() {
    std::vector<DeviceType> types;
    for (int value = 0; value <= static_cast<int>(smarthub::kLastDeviceType); ++value) {
        types.push_back(static_cast<DeviceType>(value));
    }
    return types;
}

}  // namespace

TEST_F(DeviceManagerTest, EveryDeviceTypeIsRegistered) {
    auto& registry = DeviceTypeRegistry::instance();
    for (const DeviceType type : everyDeviceType()) {
        const std::string name = smarthub::deviceTypeToString(type);
        SCOPED_TRACE(name);
        EXPECT_TRUE(registry.hasType(type));
        EXPECT_TRUE(registry.hasType(name));
        EXPECT_EQ(smarthub::stringToDeviceType(name), type);
        EXPECT_EQ(registry.getTypeName(type), name);
    }
}

TEST_F(DeviceManagerTest, EveryDeviceTypeSurvivesASaveAndLoad) {
    const nlohmann::json config = {{"custom_setting", 42}};
    {
        DeviceManager manager(eventBus, *database);
        ASSERT_TRUE(manager.initialize());
        for (const DeviceType type : everyDeviceType()) {
            const std::string name = smarthub::deviceTypeToString(type);
            auto device = DeviceTypeRegistry::instance().createFromTypeName(
                name, "device_" + name, name, "local", "", config);
            ASSERT_NE(device, nullptr);
            ASSERT_TRUE(manager.addDevice(device));
        }
        manager.shutdown();
    }

    // Twice, because a type lost on the first load would be saved that way
    for (int load = 0; load < 2; ++load) {
        DeviceManager manager(eventBus, *database);
        ASSERT_TRUE(manager.initialize());
        for (const DeviceType type : everyDeviceType()) {
            const std::string name = smarthub::deviceTypeToString(type);
            SCOPED_TRACE(name);
            auto device = manager.getDevice("device_" + name);
            ASSERT_NE(device, nullptr);
            EXPECT_EQ(device->type(), type);
            EXPECT_EQ(device->getConfig().value("custom_setting", 0), 42);
        }
        ASSERT_TRUE(manager.saveAllDevices());
        manager.shutdown();
    }
}

TEST_F(DeviceManagerTest, SensorOptionsSurviveASaveAndLoad) {
    {
        DeviceManager manager(eventBus, *database);
        ASSERT_TRUE(manager.initialize());
        ASSERT_TRUE(manager.addDevice(std::make_shared<TemperatureSensor>(
            "temp_humidity", "Climate", "local", "", true, true)));
        ASSERT_TRUE(manager.addDevice(std::make_shared<TemperatureSensor>(
            "temp_only", "Temperature", "local", "", false, false)));
        ASSERT_TRUE(manager.addDevice(std::make_shared<MotionSensor>(
            "motion_light", "Hall", "local", "", true, true)));
        ASSERT_TRUE(manager.addDevice(std::make_shared<MotionSensor>(
            "motion_only", "Porch", "local", "", false, false)));
        manager.shutdown();
    }

    DeviceManager manager(eventBus, *database);
    ASSERT_TRUE(manager.initialize());

    auto climate = manager.getDevice("temp_humidity");
    ASSERT_NE(climate, nullptr);
    EXPECT_EQ(climate->type(), DeviceType::MultiSensor);
    EXPECT_TRUE(climate->hasCapability(DeviceCapability::Temperature));
    EXPECT_TRUE(climate->hasCapability(DeviceCapability::Humidity));
    EXPECT_TRUE(climate->hasCapability(DeviceCapability::Battery));

    auto temperature = manager.getDevice("temp_only");
    ASSERT_NE(temperature, nullptr);
    EXPECT_EQ(temperature->type(), DeviceType::TemperatureSensor);
    EXPECT_FALSE(temperature->hasCapability(DeviceCapability::Humidity));
    EXPECT_FALSE(temperature->hasCapability(DeviceCapability::Battery));

    // Motion stays motion. As a multi sensor it would come back as a temperature sensor.
    auto motionLight = manager.getDevice("motion_light");
    ASSERT_NE(motionLight, nullptr);
    EXPECT_EQ(motionLight->type(), DeviceType::MotionSensor);
    EXPECT_TRUE(motionLight->hasCapability(DeviceCapability::Motion));
    EXPECT_TRUE(motionLight->hasCapability(DeviceCapability::Illuminance));
    EXPECT_TRUE(motionLight->hasCapability(DeviceCapability::Battery));

    auto motion = manager.getDevice("motion_only");
    ASSERT_NE(motion, nullptr);
    EXPECT_FALSE(motion->hasCapability(DeviceCapability::Illuminance));
    manager.shutdown();
}

TEST_F(DeviceManagerTest, LightAndOutletStayThoseTypesAndCanBeControlled) {
    {
        DeviceManager manager(eventBus, *database);
        ASSERT_TRUE(manager.initialize());
        auto& registry = DeviceTypeRegistry::instance();
        ASSERT_TRUE(manager.addDevice(registry.createFromTypeName("light", "lamp", "Lamp")));
        ASSERT_TRUE(manager.addDevice(registry.createFromTypeName("outlet", "plug", "Plug")));
        manager.shutdown();
    }

    DeviceManager manager(eventBus, *database);
    ASSERT_TRUE(manager.initialize());
    EXPECT_EQ(manager.getDevice("lamp")->type(), DeviceType::Light);
    EXPECT_EQ(manager.getDevice("plug")->type(), DeviceType::Outlet);
    EXPECT_TRUE(manager.getDevice("lamp")->hasCapability(DeviceCapability::OnOff));
    EXPECT_TRUE(manager.setDeviceState("plug", "on", true));
    manager.shutdown();
}

TEST_F(DeviceManagerTest, ReplacingASensorsConfigKeepsItsOptions) {
    TemperatureSensor sensor("s", "Sensor", "local", "", true, false);
    sensor.setConfig({{"custom_setting", 1}});

    const nlohmann::json config = sensor.getConfig();
    EXPECT_EQ(config.value("custom_setting", 0), 1);
    EXPECT_EQ(config.value("has_humidity", false), true);
    EXPECT_EQ(config.value("has_battery", true), false);
}

TEST_F(DeviceManagerTest, SensorsCanBeCreatedWithTheDefaultConfig) {
    auto& registry = DeviceTypeRegistry::instance();
    for (const char* type : {"temperature_sensor", "motion_sensor", "multi_sensor"}) {
        SCOPED_TRACE(type);
        EXPECT_NE(registry.create(smarthub::stringToDeviceType(type), "a", "A"), nullptr);
        EXPECT_NE(registry.createFromTypeName(type, "b", "B"), nullptr);
        EXPECT_NE(registry.createFromTypeName(type, "c", "C", "local", "", nullptr), nullptr);
    }
}

TEST_F(DeviceManagerTest, DevicesAlwaysHaveAConfigObject) {
    SwitchDevice device("s", "Switch");
    EXPECT_EQ(device.getConfig().value("manufacturer", "none"), "none");
}

namespace {

void addRow(Database& database, const std::string& id, const std::string& type,
            const std::string& config, const std::vector<std::string>& properties) {
    auto device = database.prepare(
        "INSERT INTO devices (id, name, type, protocol, protocol_address, room, config) "
        "VALUES (?, ?, ?, 'local', '', '', ?)");
    device->bind(1, id);
    device->bind(2, id);
    device->bind(3, type);
    device->bind(4, config);
    device->execute();
    for (const std::string& property : properties) {
        auto state = database.prepare(
            "INSERT INTO device_state (device_id, property, value, updated_at) VALUES (?, ?, '1', 0)");
        state->bind(1, id);
        state->bind(2, property);
        state->execute();
    }
}

}  // namespace

TEST_F(DeviceManagerTest, UnrecognizedTypeAndConfigSurviveASave) {
    addRow(*database, "future", "door_sensor", R"({"x":1})", {});
    {
        DeviceManager manager(eventBus, *database);
        ASSERT_TRUE(manager.initialize());
        EXPECT_EQ(manager.getDevice("future")->type(), DeviceType::Unknown);
        ASSERT_TRUE(manager.saveAllDevices());
        manager.shutdown();
    }

    auto stmt = database->prepare("SELECT type, config FROM devices WHERE id = 'future'");
    ASSERT_TRUE(stmt->step());
    EXPECT_EQ(stmt->getString(0), "door_sensor");
    EXPECT_EQ(nlohmann::json::parse(stmt->getString(1)).value("x", 0), 1);
}

TEST_F(DeviceManagerTest, OlderRowsGetTheirSensorOptionsFromSavedState) {
    addRow(*database, "old_climate", "multi_sensor", "{}", {"temperature", "humidity", "battery"});
    addRow(*database, "old_temp", "temperature_sensor", "{}", {"temperature"});
    addRow(*database, "old_quiet", "multi_sensor", "{}", {});
    addRow(*database, "old_motion", "multi_sensor", "{}", {"motion", "illuminance"});

    DeviceManager manager(eventBus, *database);
    ASSERT_TRUE(manager.initialize());

    EXPECT_EQ(manager.getDevice("old_climate")->type(), DeviceType::MultiSensor);
    EXPECT_TRUE(manager.getDevice("old_climate")->hasCapability(DeviceCapability::Battery));
    EXPECT_FALSE(manager.getDevice("old_temp")->hasCapability(DeviceCapability::Humidity));
    EXPECT_EQ(manager.getDevice("old_quiet")->type(), DeviceType::MultiSensor);
    EXPECT_EQ(manager.getDevice("old_motion")->type(), DeviceType::MotionSensor);
    EXPECT_TRUE(manager.getDevice("old_motion")->hasCapability(DeviceCapability::Illuminance));
    manager.shutdown();
}

TEST_F(DeviceManagerTest, OneBadRowDoesNotLoseTheOthers) {
    addRow(*database, "a_bad", "temperature_sensor", R"({"has_humidity":"yes","has_battery":false})", {});
    addRow(*database, "b_good", "switch", "{}", {});

    DeviceManager manager(eventBus, *database);
    ASSERT_TRUE(manager.initialize());

    EXPECT_EQ(manager.getDevice("a_bad"), nullptr);
    EXPECT_NE(manager.getDevice("b_good"), nullptr);
    manager.shutdown();
}

// ---------- State, history and rooms surviving saves

TEST_F(DeviceManagerTest, FlushedStateSurvivesAnUncleanStop) {
    auto manager = std::make_unique<DeviceManager>(eventBus, *database);
    ASSERT_TRUE(manager->initialize());
    ASSERT_TRUE(manager->addDevice(std::make_shared<SwitchDevice>("lamp", "Lamp")));
    ASSERT_TRUE(manager->setDeviceState("lamp", "on", true));
    ASSERT_TRUE(manager->flushStateChanges());

    // Read it back without any shutdown saving, as after a power cut
    DeviceManager restarted(eventBus, *database);
    ASSERT_TRUE(restarted.initialize());
    EXPECT_EQ(restarted.getDevice("lamp")->getProperty("on"), true);
    restarted.shutdown();

    manager->shutdown();
}

TEST_F(DeviceManagerTest, UnflushedStateWaitsForTheNextWrite) {
    DeviceManager manager(eventBus, *database);
    ASSERT_TRUE(manager.initialize());
    ASSERT_TRUE(manager.addDevice(std::make_shared<SwitchDevice>("lamp", "Lamp")));

    const auto savedOn = [&] {
        auto stmt = database->prepare(
            "SELECT value FROM device_state WHERE device_id = 'lamp' AND property = 'on'");
        stmt->step();
        return stmt->getString(0);
    };
    ASSERT_TRUE(manager.setDeviceState("lamp", "on", true));
    EXPECT_EQ(savedOn(), "false");
    ASSERT_TRUE(manager.flushStateChanges());
    EXPECT_EQ(savedOn(), "true");
    manager.shutdown();
}

TEST_F(DeviceManagerTest, FlushingAfterARemovalKeepsTheOtherDevices) {
    DeviceManager manager(eventBus, *database);
    ASSERT_TRUE(manager.initialize());
    ASSERT_TRUE(manager.addDevice(std::make_shared<SwitchDevice>("gone", "Gone")));
    ASSERT_TRUE(manager.addDevice(std::make_shared<SwitchDevice>("kept", "Kept")));
    ASSERT_TRUE(manager.setDeviceState("gone", "on", true));
    ASSERT_TRUE(manager.setDeviceState("kept", "on", true));
    ASSERT_TRUE(manager.removeDevice("gone"));

    EXPECT_TRUE(manager.flushStateChanges());

    auto stmt = database->prepare(
        "SELECT value FROM device_state WHERE device_id = 'kept' AND property = 'on'");
    ASSERT_TRUE(stmt->step());
    EXPECT_EQ(stmt->getString(0), "true");
    manager.shutdown();
}

TEST_F(DeviceManagerTest, SavingADeviceKeepsItsHistoryAndCreationTime) {
    DeviceManager manager(eventBus, *database);
    ASSERT_TRUE(manager.initialize());
    auto device = std::make_shared<SwitchDevice>("lamp", "Lamp");
    ASSERT_TRUE(manager.addDevice(device));
    ASSERT_TRUE(database->execute(
        "INSERT INTO sensor_history (device_id, property, value, timestamp) "
        "VALUES ('lamp', 'power', 5, 1000)"));
    ASSERT_TRUE(database->execute("UPDATE devices SET created_at = 1234 WHERE id = 'lamp'"));

    device->setName("Renamed");
    ASSERT_TRUE(manager.saveDevice(device));
    ASSERT_TRUE(manager.saveAllDevices());

    auto history = database->prepare("SELECT COUNT(*) FROM sensor_history WHERE device_id = 'lamp'");
    ASSERT_TRUE(history->step());
    EXPECT_EQ(history->getInt(0), 1);

    auto row = database->prepare("SELECT name, created_at FROM devices WHERE id = 'lamp'");
    ASSERT_TRUE(row->step());
    EXPECT_EQ(row->getString(0), "Renamed");
    EXPECT_EQ(row->getInt(1), 1234);
    manager.shutdown();
}

TEST_F(DeviceManagerTest, DeletingARoomSticksThroughASave) {
    {
        DeviceManager manager(eventBus, *database);
        ASSERT_TRUE(manager.initialize());
        ASSERT_TRUE(manager.addRoom("kitchen_test", "Kitchen Test"));
        auto device = std::make_shared<SwitchDevice>("lamp", "Lamp");
        device->setRoom("kitchen_test");
        ASSERT_TRUE(manager.addDevice(device));

        ASSERT_TRUE(manager.deleteRoom("kitchen_test"));
        EXPECT_EQ(manager.getDevice("lamp")->room(), "");
        ASSERT_TRUE(manager.saveAllDevices());
        manager.shutdown();
    }

    DeviceManager manager(eventBus, *database);
    ASSERT_TRUE(manager.initialize());
    EXPECT_EQ(manager.getDevice("lamp")->room(), "");
    manager.shutdown();
}

TEST_F(DeviceManagerTest, RoomChangesReportFailures) {
    DeviceManager manager(eventBus, *database);
    ASSERT_TRUE(manager.initialize());

    ASSERT_TRUE(manager.addRoom("den_test", "Den"));
    EXPECT_FALSE(manager.addRoom("den_test", "Den again"));

    EXPECT_TRUE(manager.updateRoom("den_test", "Study"));
    EXPECT_FALSE(manager.updateRoom("no_such_room", "Nothing"));

    EXPECT_TRUE(manager.deleteRoom("den_test"));
    EXPECT_FALSE(manager.deleteRoom("den_test"));
    manager.shutdown();
}

TEST_F(DeviceManagerTest, DeletingARoomThatDoesNotExistLeavesDevicesAlone) {
    DeviceManager manager(eventBus, *database);
    ASSERT_TRUE(manager.initialize());
    auto device = std::make_shared<SwitchDevice>("lamp", "Lamp");
    device->setRoom("no_such_room");
    ASSERT_TRUE(manager.addDevice(device));

    EXPECT_FALSE(manager.deleteRoom("no_such_room"));
    EXPECT_EQ(manager.getDevice("lamp")->room(), "no_such_room");
    manager.shutdown();
}

TEST_F(DeviceManagerTest, FailedStateWritesAreTriedAgain) {
    DeviceManager manager(eventBus, *database);
    ASSERT_TRUE(manager.initialize());
    ASSERT_TRUE(manager.addDevice(std::make_shared<SwitchDevice>("lamp", "Lamp")));
    ASSERT_TRUE(manager.setDeviceState("lamp", "on", true));

    // Stands in for a write failing: the state row can't be written until this is gone
    ASSERT_TRUE(database->execute(
        "CREATE TRIGGER block_state BEFORE UPDATE ON device_state "
        "BEGIN SELECT RAISE(ABORT, 'blocked'); END"));
    EXPECT_FALSE(manager.flushStateChanges());

    ASSERT_TRUE(database->execute("DROP TRIGGER block_state"));
    EXPECT_TRUE(manager.flushStateChanges());

    auto stmt = database->prepare(
        "SELECT value FROM device_state WHERE device_id = 'lamp' AND property = 'on'");
    ASSERT_TRUE(stmt->step());
    EXPECT_EQ(stmt->getString(0), "true");
    manager.shutdown();
}

namespace {

// Reports a state change from its own shutdown, as a protocol's last report might
class ReportsOnShutdown : public smarthub::testing::MockProtocolHandler {
public:
    using smarthub::testing::MockProtocolHandler::MockProtocolHandler;

    void shutdown() override {
        simulateStateChange("lamp", "on", true);
        MockProtocolHandler::shutdown();
    }
};

}  // namespace

TEST_F(DeviceManagerTest, ReportArrivingDuringShutdownIsSaved) {
    ProtocolFactory::instance().registerProtocol("reports_on_shutdown",
        [](EventBus& bus, const nlohmann::json& config) -> ProtocolHandlerPtr {
            return std::make_shared<ReportsOnShutdown>(bus, config);
        });

    {
        DeviceManager manager(eventBus, *database);
        ASSERT_TRUE(manager.initialize());
        ASSERT_TRUE(manager.addDevice(std::make_shared<SwitchDevice>("lamp", "Lamp")));
        ASSERT_TRUE(manager.loadProtocol("reports_on_shutdown", nlohmann::json::object()));
        manager.shutdown();
    }
    ProtocolFactory::instance().unregisterProtocol("reports_on_shutdown");

    auto stmt = database->prepare(
        "SELECT value FROM device_state WHERE device_id = 'lamp' AND property = 'on'");
    ASSERT_TRUE(stmt->step());
    EXPECT_EQ(stmt->getString(0), "true");
}

TEST_F(DeviceManagerTest, StateWithInvalidTextDoesNotThrow) {
    ProtocolFactory::instance().registerProtocol("plain_mock",
        [](EventBus& bus, const nlohmann::json& config) -> ProtocolHandlerPtr {
            return std::make_shared<smarthub::testing::MockProtocolHandler>(bus, config);
        });

    DeviceManager manager(eventBus, *database);
    ASSERT_TRUE(manager.initialize());
    ASSERT_TRUE(manager.addDevice(std::make_shared<SwitchDevice>("lamp", "Lamp")));
    ASSERT_TRUE(manager.loadProtocol("plain_mock", nlohmann::json::object()));
    auto* mock = dynamic_cast<smarthub::testing::MockProtocolHandler*>(manager.getProtocol("plain_mock"));
    ASSERT_NE(mock, nullptr);

    EXPECT_NO_THROW(mock->simulateStateChange("lamp", "note", std::string("bad\xFF")));
    EXPECT_TRUE(manager.flushStateChanges());

    manager.shutdown();
    ProtocolFactory::instance().unregisterProtocol("plain_mock");
}

TEST(DeviceManagerRooms, SavedRoomNamesBecomeIds) {
    // The dashboard used to save a room's name and the LCD its id (#56)
    const std::string path = "/tmp/test_rooms_" + std::to_string(getpid()) + ".db";
    std::remove(path.c_str());
    {
        EventBus eventBus;
        Database database(path);
        ASSERT_TRUE(database.initialize());
        {
            DeviceManager manager(eventBus, database);
            ASSERT_TRUE(manager.initialize());
            ASSERT_TRUE(manager.addRoom("living-room", "Living Room"));
            manager.shutdown();
        }
        for (const auto& [id, room] : std::vector<std::pair<std::string, std::string>>{
                 {"lcd", "living-room"}, {"web", "Living Room"}, {"gone", "Old Shed"}, {"none", ""}}) {
            auto stmt = database.prepare(
                "INSERT INTO devices (id, name, type, protocol, protocol_address, room, config) "
                "VALUES (?, ?, 'switch', 'local', '', ?, '{}')");
            ASSERT_TRUE(stmt && stmt->isValid());
            stmt->bind(1, id);
            stmt->bind(2, id);
            stmt->bind(3, room);
            ASSERT_TRUE(stmt->execute());
        }

        DeviceManager manager(eventBus, database);
        ASSERT_TRUE(manager.initialize());
        EXPECT_EQ(manager.getDevice("lcd")->room(), "living-room");
        EXPECT_EQ(manager.getDevice("web")->room(), "living-room");
        EXPECT_EQ(manager.getDevice("gone")->room(), "Old Shed")
            << "left alone, as clearing it would lose every room if the rooms couldn't be read";
        EXPECT_EQ(manager.getDevice("none")->room(), "");
        EXPECT_EQ(manager.getDevicesByRoom("living-room").size(), 2u);
        EXPECT_TRUE(manager.getDevicesByRoom("Living Room").empty()) << "only ids match now";

        // And in the database, not just in memory
        auto stmt = database.prepare("SELECT room FROM devices WHERE id = 'web'");
        ASSERT_TRUE(stmt && stmt->step());
        EXPECT_EQ(stmt->getString(0), "living-room");
        manager.shutdown();
    }
    std::remove(path.c_str());
}

TEST(DeviceManagerRooms, ARoomGivenByNameIsStoredById) {
    const std::string path = "/tmp/test_rooms_name_" + std::to_string(getpid()) + ".db";
    std::remove(path.c_str());
    {
        EventBus eventBus;
        Database database(path);
        ASSERT_TRUE(database.initialize());
        DeviceManager manager(eventBus, database);
        ASSERT_TRUE(manager.initialize());
        ASSERT_TRUE(manager.addRoom("den", "Den"));

        auto device = std::make_shared<SwitchDevice>("lamp", "Lamp");
        device->setRoom("Den");
        ASSERT_TRUE(manager.addDevice(device));
        EXPECT_EQ(device->room(), "den");

        EXPECT_EQ(manager.roomIdFor("den"), "den");
        EXPECT_EQ(manager.roomIdFor("Den"), "den");
        EXPECT_EQ(manager.roomIdFor(""), "");
        EXPECT_FALSE(manager.roomIdFor("Attic"));

        // A name two rooms have is a guess, so it's refused
        ASSERT_TRUE(manager.addRoom("den-2", "Den"));
        EXPECT_FALSE(manager.roomIdFor("Den"));
        EXPECT_EQ(manager.roomIdFor("den-2"), "den-2");
        manager.shutdown();
    }
    std::remove(path.c_str());
}

TEST(DeviceManagerRooms, RenamingARoomKeepsItsDevices) {
    const std::string path = "/tmp/test_rooms_rename_" + std::to_string(getpid()) + ".db";
    std::remove(path.c_str());
    {
        EventBus eventBus;
        Database database(path);
        ASSERT_TRUE(database.initialize());
        DeviceManager manager(eventBus, database);
        ASSERT_TRUE(manager.initialize());
        ASSERT_TRUE(manager.addRoom("den", "Den"));
        auto device = std::make_shared<SwitchDevice>("lamp", "Lamp");
        device->setRoom("Den");
        ASSERT_TRUE(manager.addDevice(device));

        ASSERT_TRUE(manager.updateRoom("den", "Study"));
        EXPECT_EQ(manager.getDevicesByRoom("den").size(), 1u);
        EXPECT_EQ(manager.roomIdFor("Study"), "den");
        manager.shutdown();
    }
    std::remove(path.c_str());
}

namespace {

class RecordingProtocol : public smarthub::testing::MockProtocolHandler {
public:
    using smarthub::testing::MockProtocolHandler::MockProtocolHandler;

    void restoreDevice(const IDevice& device) override {
        std::lock_guard<std::mutex> lock(mutex);
        restored.push_back(device.id());
    }
    void forgetDevice(const std::string& deviceId) override {
        std::lock_guard<std::mutex> lock(mutex);
        forgotten.push_back(deviceId);
    }
    std::mutex mutex;
    std::vector<std::string> restored;
    std::vector<std::string> forgotten;
};

} // namespace

TEST(DeviceManagerRestore, ProtocolLearnsItsDevicesSoTheyWorkAfterARestart) {
    const std::string path = "/tmp/test_restore_" + std::to_string(getpid()) + ".db";
    std::remove(path.c_str());
    EventBus protocolBus;
    auto protocol = std::make_shared<RecordingProtocol>(protocolBus);
    ProtocolFactory::instance().registerProtocol("recording",
        [protocol](EventBus&, const nlohmann::json&) -> ProtocolHandlerPtr { return protocol; });

    {
        EventBus eventBus;
        Database database(path);
        ASSERT_TRUE(database.initialize());
        DeviceManager manager(eventBus, database);
        ASSERT_TRUE(manager.initialize());

        auto device = std::make_shared<SwitchDevice>("paired", "Paired", "recording");
        device->setState("networkAddress", 4660);
        device->setState("endpoint", 11);
        ASSERT_TRUE(manager.addDevice(device));
        ASSERT_TRUE(manager.addDevice(std::make_shared<SwitchDevice>("other", "Other", "local")));
        manager.shutdown();
    }
    {
        EventBus eventBus;
        Database database(path);
        ASSERT_TRUE(database.initialize());
        DeviceManager manager(eventBus, database);
        ASSERT_TRUE(manager.initialize());

        auto device = manager.getDevice("paired");
        ASSERT_NE(device, nullptr);
        EXPECT_EQ(device->getProperty("networkAddress"), 4660) << "what the handler rebuilds from";
        EXPECT_EQ(device->getProperty("endpoint"), 11);

        ASSERT_TRUE(manager.loadProtocol("recording", nlohmann::json::object()));
        // Before it starts, and again once it can reach them
        EXPECT_EQ(protocol->restored, (std::vector<std::string>{"paired", "paired"}));

        protocol->restored.clear();
        ASSERT_TRUE(manager.addDevice(std::make_shared<SwitchDevice>("typed", "Typed", "recording")));
        EXPECT_EQ(protocol->restored, std::vector<std::string>{"typed"}) << "added after the protocol loaded";

        ASSERT_TRUE(manager.removeDevice("typed"));
        EXPECT_EQ(protocol->forgotten, std::vector<std::string>{"typed"});
        manager.shutdown();
    }
    ProtocolFactory::instance().unregisterProtocol("recording");
    std::remove(path.c_str());
}

// ---- Sensor history ----

class SensorHistoryTest : public DeviceManagerTest {
protected:
    void SetUp() override {
        DeviceManagerTest::SetUp();
        ProtocolFactory::instance().registerProtocol("history_mock",
            [this](EventBus& bus, const nlohmann::json& config) -> ProtocolHandlerPtr {
                protocol = std::make_shared<smarthub::testing::MockProtocolHandler>(bus, config);
                return protocol;
            });
        manager = std::make_unique<DeviceManager>(eventBus, *database);
        manager->initialize();
        ASSERT_TRUE(manager->loadProtocol("history_mock", nlohmann::json::object()));
        ASSERT_TRUE(manager->addDevice(std::make_shared<TemperatureSensor>("t", "T", "history_mock")));
    }
    void TearDown() override {
        manager->shutdown();
        manager.reset();
        ProtocolFactory::instance().unregisterProtocol("history_mock");
        DeviceManagerTest::TearDown();
    }
    std::vector<SensorSample> history(const std::string& property = "temperature") {
        return manager->sensorHistory("t", property, std::time(nullptr) - 3600, 60);
    }
    int rowCount() {
        auto stmt = database->prepare("SELECT COUNT(*) FROM sensor_history");
        stmt->step();
        return stmt->getInt(0);
    }
    void insertRow(int64_t timestamp, double value) {
        auto stmt = database->prepare(
            "INSERT INTO sensor_history (device_id, property, value, timestamp) VALUES ('t', 'temperature', ?, ?)");
        stmt->bind(1, value);
        stmt->bind(2, timestamp);
        ASSERT_TRUE(stmt->execute());
    }
    std::shared_ptr<smarthub::testing::MockProtocolHandler> protocol;
    std::unique_ptr<DeviceManager> manager;
};

TEST_F(SensorHistoryTest, ReportIsRecorded) {
    protocol->simulateStateChange("t", "temperature", 21.5);
    ASSERT_TRUE(manager->flushSensorSamples());

    auto samples = history();
    ASSERT_EQ(samples.size(), 1u);
    EXPECT_DOUBLE_EQ(samples[0].value, 21.5);
    EXPECT_NEAR(samples[0].timestamp, std::time(nullptr), 5);
}

TEST_F(SensorHistoryTest, FrequentReportsAreLimitedForTheSdCard) {
    for (int i = 0; i < 10; i++) {
        protocol->simulateStateChange("t", "temperature", 20.0 + i);
    }
    ASSERT_TRUE(manager->flushSensorSamples());
    EXPECT_EQ(rowCount(), 1);
}

TEST_F(SensorHistoryTest, EachPropertyHasItsOwnLimit) {
    protocol->simulateStateChange("t", "temperature", 20.0);
    protocol->simulateStateChange("t", "humidity", 50.0);
    ASSERT_TRUE(manager->flushSensorSamples());
    EXPECT_EQ(history("temperature").size(), 1u);
    EXPECT_EQ(history("humidity").size(), 1u);
}

TEST_F(SensorHistoryTest, OtherStateIsNotRecorded) {
    protocol->simulateStateChange("t", "networkAddress", 4660);
    protocol->simulateStateChange("t", "battery", 90);
    protocol->simulateStateChange("t", "illuminance", 300);
    protocol->simulateStateChange("t", "temperature", "hot");
    protocol->simulateStateChange("t", "temperature", -327.68);  // A sensor's "no reading" value
    protocol->simulateStateChange("t", "humidity", 655.35);
    ASSERT_TRUE(manager->flushSensorSamples());
    EXPECT_EQ(rowCount(), 0);
}

TEST_F(SensorHistoryTest, FlushWithNothingNewWritesNothing) {
    ASSERT_TRUE(manager->flushSensorSamples());
    EXPECT_EQ(rowCount(), 0);
}

TEST_F(SensorHistoryTest, ReadingFromARemovedDeviceDoesNotLoseOthers) {
    ASSERT_TRUE(manager->addDevice(std::make_shared<TemperatureSensor>("u", "U", "history_mock")));
    protocol->simulateStateChange("t", "temperature", 21.0);
    protocol->simulateStateChange("u", "temperature", 22.0);
    ASSERT_TRUE(manager->removeDevice("u"));

    ASSERT_TRUE(manager->flushSensorSamples());

    EXPECT_EQ(rowCount(), 1);
    EXPECT_EQ(history().size(), 1u);
}

TEST_F(SensorHistoryTest, RemovingADeviceRemovesItsHistory) {
    protocol->simulateStateChange("t", "temperature", 21.0);
    ASSERT_TRUE(manager->flushSensorSamples());
    ASSERT_EQ(rowCount(), 1);

    ASSERT_TRUE(manager->removeDevice("t"));

    EXPECT_EQ(rowCount(), 0);
}

TEST_F(SensorHistoryTest, OldReadingsArePruned) {
    const int64_t now = std::time(nullptr);
    insertRow(now - DeviceManager::kHistoryRetentionSeconds - 100, 1.0);
    insertRow(now - DeviceManager::kHistoryRetentionSeconds + 100, 2.0);

    ASSERT_TRUE(manager->flushSensorSamples());

    EXPECT_EQ(rowCount(), 1);
}

TEST_F(SensorHistoryTest, QueryIsOldestFirstAndWithinTheRange) {
    const int64_t now = std::time(nullptr);
    insertRow(now - 7200, 1.0);
    insertRow(now - 600, 3.0);
    insertRow(now - 1800, 2.0);

    auto samples = history();

    ASSERT_EQ(samples.size(), 2u);
    EXPECT_DOUBLE_EQ(samples[0].value, 2.0);
    EXPECT_DOUBLE_EQ(samples[1].value, 3.0);
}

TEST_F(SensorHistoryTest, QueryLeavesOutRowsDatedInTheFuture) {
    insertRow(std::time(nullptr) + 3600, 1.0);  // The clock was ahead when this was recorded
    EXPECT_TRUE(history().empty());
}

TEST_F(SensorHistoryTest, SavingStateDoesNotSaveReadings) {
    protocol->simulateStateChange("t", "temperature", 21.5);
    ASSERT_TRUE(manager->flushStateChanges());
    EXPECT_EQ(rowCount(), 0) << "they're saved together less often, to limit SD card writes";
}

TEST_F(SensorHistoryTest, QueryAveragesIntoTheNumberOfPointsAskedFor) {
    const int64_t now = std::time(nullptr);
    for (int i = 0; i < 120; i++) {
        insertRow(now - 3600 + i * 30, i < 60 ? 10.0 : 20.0);
    }

    // Slots are aligned to the clock, so a range can touch one more than asked for, and with very
    // few it can fit in one. With four, the hour always spans several.
    auto samples = manager->sensorHistory("t", "temperature", now - 3600, 4);

    ASSERT_LE(samples.size(), 5u);
    ASSERT_GE(samples.size(), 2u);
    EXPECT_LT(samples.front().value, samples.back().value);
    for (const auto& sample : samples) {
        EXPECT_GE(sample.value, 10.0);
        EXPECT_LE(sample.value, 20.0);
    }
}

TEST_F(SensorHistoryTest, HistorySurvivesARestart) {
    protocol->simulateStateChange("t", "temperature", 21.5);
    manager->shutdown();
    manager = std::make_unique<DeviceManager>(eventBus, *database);
    manager->initialize();
    EXPECT_EQ(history().size(), 1u) << "shutting down saves what's waiting";
}
