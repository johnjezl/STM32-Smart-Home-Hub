/**
 * Device Unit Tests - Phase 3
 */

#include <gtest/gtest.h>
#include <smarthub/devices/Device.hpp>
#include <smarthub/devices/IDevice.hpp>
#include <smarthub/devices/types/SwitchDevice.hpp>
#include <smarthub/devices/types/DimmerDevice.hpp>
#include <smarthub/devices/types/ColorLightDevice.hpp>
#include <smarthub/devices/types/TemperatureSensor.hpp>
#include <smarthub/devices/types/MotionSensor.hpp>
#include <smarthub/devices/DeviceTypeRegistry.hpp>

using namespace smarthub;

class DeviceTest : public ::testing::Test {
protected:
};

// === Basic Device Tests ===

TEST_F(DeviceTest, CreateDevice) {
    Device device("light1", "Living Room Light", DeviceType::Light);

    EXPECT_EQ(device.id(), "light1");
    EXPECT_EQ(device.name(), "Living Room Light");
    EXPECT_EQ(device.type(), DeviceType::Light);
}

TEST_F(DeviceTest, DeviceWithProtocol) {
    Device device("dev1", "Test Device", DeviceType::Switch, "mqtt", "zigbee2mqtt/device/0x1234");

    EXPECT_EQ(device.protocol(), "mqtt");
    EXPECT_EQ(device.protocolAddress(), "zigbee2mqtt/device/0x1234");
}

TEST_F(DeviceTest, TypeToString) {
    EXPECT_EQ(deviceTypeToString(DeviceType::Light), "light");
    EXPECT_EQ(deviceTypeToString(DeviceType::Switch), "switch");
    EXPECT_EQ(deviceTypeToString(DeviceType::Dimmer), "dimmer");
    EXPECT_EQ(deviceTypeToString(DeviceType::ColorLight), "color_light");
    EXPECT_EQ(deviceTypeToString(DeviceType::TemperatureSensor), "temperature_sensor");
    EXPECT_EQ(deviceTypeToString(DeviceType::MotionSensor), "motion_sensor");
    EXPECT_EQ(deviceTypeToString(DeviceType::Unknown), "unknown");
}

TEST_F(DeviceTest, StringToType) {
    EXPECT_EQ(stringToDeviceType("light"), DeviceType::Light);
    EXPECT_EQ(stringToDeviceType("switch"), DeviceType::Switch);
    EXPECT_EQ(stringToDeviceType("dimmer"), DeviceType::Dimmer);
    EXPECT_EQ(stringToDeviceType("color_light"), DeviceType::ColorLight);
    EXPECT_EQ(stringToDeviceType("temperature_sensor"), DeviceType::TemperatureSensor);
    EXPECT_EQ(stringToDeviceType("motion_sensor"), DeviceType::MotionSensor);
    EXPECT_EQ(stringToDeviceType("invalid"), DeviceType::Unknown);
}

TEST_F(DeviceTest, AvailabilityStatus) {
    Device device("d1", "Device", DeviceType::Light);

    // Default availability is Unknown
    EXPECT_EQ(device.availability(), DeviceAvailability::Unknown);

    device.setAvailability(DeviceAvailability::Online);
    EXPECT_TRUE(device.isAvailable());
    EXPECT_EQ(device.availability(), DeviceAvailability::Online);

    device.setAvailability(DeviceAvailability::Offline);
    EXPECT_FALSE(device.isAvailable());
    EXPECT_EQ(device.availability(), DeviceAvailability::Offline);
}

TEST_F(DeviceTest, RoomAssignment) {
    Device device("d1", "Device", DeviceType::Light);

    EXPECT_TRUE(device.room().empty());

    device.setRoom("Living Room");
    EXPECT_EQ(device.room(), "Living Room");

    device.setRoom("Bedroom");
    EXPECT_EQ(device.room(), "Bedroom");
}

TEST_F(DeviceTest, StateWithJson) {
    Device device("d1", "Device", DeviceType::Light);

    // Set various state properties using JSON
    device.setState("on", true);
    device.setState("brightness", 75);
    device.setState("color", "red");

    // Get state
    EXPECT_EQ(device.getProperty("on").get<bool>(), true);
    EXPECT_EQ(device.getProperty("brightness").get<int>(), 75);
    EXPECT_EQ(device.getProperty("color").get<std::string>(), "red");
}

TEST_F(DeviceTest, GetFullState) {
    Device device("d1", "Device", DeviceType::Light);

    device.setState("on", true);
    device.setState("brightness", 50);

    auto state = device.getState();
    EXPECT_TRUE(state.contains("on"));
    EXPECT_TRUE(state.contains("brightness"));
}

TEST_F(DeviceTest, NonexistentProperty) {
    Device device("d1", "Device", DeviceType::Light);

    auto prop = device.getProperty("nonexistent");
    EXPECT_TRUE(prop.is_null());
}

TEST_F(DeviceTest, LastSeen) {
    Device device("d1", "Device", DeviceType::Light);

    uint64_t firstSeen = device.lastSeen();
    EXPECT_GT(firstSeen, 0u);

    // Update last seen
    device.updateLastSeen();
    EXPECT_GE(device.lastSeen(), firstSeen);
}

TEST_F(DeviceTest, ToJson) {
    Device device("light1", "Kitchen Light", DeviceType::Light, "mock", "mock://1");
    device.setRoom("Kitchen");
    device.setAvailability(DeviceAvailability::Online);
    device.setState("on", true);

    auto json = device.toJson();

    EXPECT_EQ(json["id"], "light1");
    EXPECT_EQ(json["name"], "Kitchen Light");
    EXPECT_EQ(json["type"], "light");
    EXPECT_EQ(json["protocol"], "mock");
    EXPECT_EQ(json["room"], "Kitchen");
    EXPECT_EQ(json["availability"], "online");
    EXPECT_TRUE(json["state"]["on"].get<bool>());
}

TEST_F(DeviceTest, FromJson) {
    nlohmann::json json = {
        {"id", "sensor1"},
        {"name", "Temperature Sensor"},
        {"type", "temperature_sensor"},
        {"protocol", "zigbee"},
        {"protocol_address", "0x1234"},
        {"room", "Bedroom"},
        {"availability", "online"},
        {"state", {{"temperature", 22.5}}}
    };

    auto device = Device::fromJson(json);

    EXPECT_EQ(device->id(), "sensor1");
    EXPECT_EQ(device->name(), "Temperature Sensor");
    EXPECT_EQ(device->room(), "Bedroom");
    EXPECT_TRUE(device->isAvailable());
}

TEST_F(DeviceTest, StateCallback) {
    Device device("d1", "Device", DeviceType::Light);

    bool callbackCalled = false;
    std::string changedProperty;

    device.setStateCallback([&](const std::string& property, const nlohmann::json& /*value*/) {
        callbackCalled = true;
        changedProperty = property;
    });

    device.setState("on", true);

    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(changedProperty, "on");
}

TEST_F(DeviceTest, Capabilities) {
    Device device("d1", "Device", DeviceType::Light);

    // Base device has no capabilities by default
    EXPECT_TRUE(device.capabilities().empty());
    EXPECT_FALSE(device.hasCapability(DeviceCapability::OnOff));
}

// === Switch Device Tests ===

TEST_F(DeviceTest, SwitchDevice) {
    SwitchDevice sw("sw1", "Test Switch", "mock", "mock://sw1");

    EXPECT_EQ(sw.type(), DeviceType::Switch);
    EXPECT_TRUE(sw.hasCapability(DeviceCapability::OnOff));
    EXPECT_FALSE(sw.isOn());

    sw.turnOn();
    EXPECT_TRUE(sw.isOn());

    sw.turnOff();
    EXPECT_FALSE(sw.isOn());

    sw.toggle();
    EXPECT_TRUE(sw.isOn());
}

// === Dimmer Device Tests ===

TEST_F(DeviceTest, DimmerDevice) {
    DimmerDevice dimmer("dim1", "Test Dimmer");

    EXPECT_EQ(dimmer.type(), DeviceType::Dimmer);
    EXPECT_TRUE(dimmer.hasCapability(DeviceCapability::OnOff));
    EXPECT_TRUE(dimmer.hasCapability(DeviceCapability::Brightness));

    EXPECT_FALSE(dimmer.isOn());
    EXPECT_EQ(dimmer.brightness(), 0);

    dimmer.setBrightness(50);
    EXPECT_TRUE(dimmer.isOn());
    EXPECT_EQ(dimmer.brightness(), 50);

    dimmer.turnOff();
    EXPECT_FALSE(dimmer.isOn());
    EXPECT_EQ(dimmer.brightness(), 0);

    // Turn on restores previous brightness
    dimmer.turnOn();
    EXPECT_TRUE(dimmer.isOn());
    EXPECT_EQ(dimmer.brightness(), 50);
}

// === Color Light Device Tests ===

TEST_F(DeviceTest, ColorLightDevice) {
    ColorLightDevice light("cl1", "Color Light");

    EXPECT_EQ(light.type(), DeviceType::ColorLight);
    EXPECT_TRUE(light.hasCapability(DeviceCapability::OnOff));
    EXPECT_TRUE(light.hasCapability(DeviceCapability::Brightness));
    EXPECT_TRUE(light.hasCapability(DeviceCapability::ColorTemperature));
    EXPECT_TRUE(light.hasCapability(DeviceCapability::ColorRGB));

    // Test color temperature
    light.setColorTemperature(4000);
    EXPECT_EQ(light.colorTemperature(), 4000);

    // Test RGB
    light.setColorRGB(255, 128, 0);
    auto rgb = light.colorRGB();
    EXPECT_EQ(rgb.r, 255);
    EXPECT_EQ(rgb.g, 128);
    EXPECT_EQ(rgb.b, 0);
}

TEST_F(DeviceTest, ColorConversion) {
    // Test HSV to RGB
    HSV hsv{0, 100, 100};  // Red
    auto rgb = ColorLightDevice::hsvToRgb(hsv);
    EXPECT_EQ(rgb.r, 255);
    EXPECT_EQ(rgb.g, 0);
    EXPECT_EQ(rgb.b, 0);

    // Test RGB to HSV
    RGB redRgb{255, 0, 0};
    auto hsvResult = ColorLightDevice::rgbToHsv(redRgb);
    EXPECT_EQ(hsvResult.h, 0);
    EXPECT_EQ(hsvResult.s, 100);
    EXPECT_EQ(hsvResult.v, 100);
}

// === Temperature Sensor Tests ===

TEST_F(DeviceTest, TemperatureSensor) {
    TemperatureSensor sensor("ts1", "Temp Sensor", "mock", "", true, true);

    EXPECT_TRUE(sensor.hasCapability(DeviceCapability::Temperature));
    EXPECT_TRUE(sensor.hasCapability(DeviceCapability::Humidity));
    EXPECT_TRUE(sensor.hasCapability(DeviceCapability::Battery));

    sensor.setTemperature(22.5);
    EXPECT_DOUBLE_EQ(sensor.temperature(), 22.5);

    sensor.setHumidity(45.0);
    EXPECT_DOUBLE_EQ(sensor.humidity(), 45.0);

    sensor.setBatteryLevel(85);
    EXPECT_EQ(sensor.batteryLevel(), 85);
}

TEST_F(DeviceTest, TemperatureSensorNoHumidity) {
    TemperatureSensor sensor("ts2", "Temp Only", "mock", "", false, false);

    EXPECT_TRUE(sensor.hasCapability(DeviceCapability::Temperature));
    EXPECT_FALSE(sensor.hasCapability(DeviceCapability::Humidity));
    EXPECT_FALSE(sensor.hasCapability(DeviceCapability::Battery));

    // Humidity not supported - returns -1
    EXPECT_EQ(sensor.humidity(), -1.0);
    EXPECT_EQ(sensor.batteryLevel(), -1);
}

// === Motion Sensor Tests ===

TEST_F(DeviceTest, MotionSensor) {
    MotionSensor sensor("ms1", "Motion Sensor", "mock", "", true, true);

    EXPECT_TRUE(sensor.hasCapability(DeviceCapability::Motion));
    EXPECT_TRUE(sensor.hasCapability(DeviceCapability::Illuminance));
    EXPECT_TRUE(sensor.hasCapability(DeviceCapability::Battery));

    EXPECT_FALSE(sensor.motionDetected());

    sensor.setMotionDetected(true);
    EXPECT_TRUE(sensor.motionDetected());
    EXPECT_GT(sensor.lastMotionTime(), 0u);

    sensor.setIlluminance(500);
    EXPECT_EQ(sensor.illuminance(), 500);
}

// === Device Type Registry Tests ===

TEST_F(DeviceTest, DeviceTypeRegistry) {
    auto& registry = DeviceTypeRegistry::instance();

    EXPECT_TRUE(registry.hasType(DeviceType::Switch));
    EXPECT_TRUE(registry.hasType(DeviceType::Dimmer));
    EXPECT_TRUE(registry.hasType(DeviceType::ColorLight));
    EXPECT_TRUE(registry.hasType("switch"));
    EXPECT_TRUE(registry.hasType("dimmer"));

    // Create device by type
    auto sw = registry.create(DeviceType::Switch, "reg-sw1", "Registry Switch");
    EXPECT_NE(sw, nullptr);
    EXPECT_EQ(sw->type(), DeviceType::Switch);

    // Create device by name
    auto dimmer = registry.createFromTypeName("dimmer", "reg-dim1", "Registry Dimmer");
    EXPECT_NE(dimmer, nullptr);
    EXPECT_EQ(dimmer->type(), DeviceType::Dimmer);
}

TEST_F(DeviceTest, DeviceTypeRegistryWithConfig) {
    auto& registry = DeviceTypeRegistry::instance();

    // Create sensor with config
    nlohmann::json config = {{"has_humidity", true}, {"has_battery", true}};
    auto sensor = registry.createFromTypeName("temperature_sensor", "cfg-ts1", "Configured Sensor",
                                               "mock", "", config);

    EXPECT_NE(sensor, nullptr);
    EXPECT_TRUE(sensor->hasCapability(DeviceCapability::Temperature));
    EXPECT_TRUE(sensor->hasCapability(DeviceCapability::Humidity));
    EXPECT_TRUE(sensor->hasCapability(DeviceCapability::Battery));
}
