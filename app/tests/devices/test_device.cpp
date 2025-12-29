/**
 * Device Unit Tests
 */

#include <gtest/gtest.h>
#include <smarthub/devices/Device.hpp>
#include <any>

class DeviceTest : public ::testing::Test {
protected:
};

TEST_F(DeviceTest, CreateDevice) {
    smarthub::Device device("light1", "Living Room Light", smarthub::DeviceType::Light);

    EXPECT_EQ(device.id(), "light1");
    EXPECT_EQ(device.name(), "Living Room Light");
    EXPECT_EQ(device.type(), smarthub::DeviceType::Light);
}

TEST_F(DeviceTest, DeviceTypes) {
    smarthub::Device light("l1", "Light", smarthub::DeviceType::Light);
    smarthub::Device sensor("s1", "Sensor", smarthub::DeviceType::Sensor);
    smarthub::Device thermostat("t1", "Thermostat", smarthub::DeviceType::Thermostat);
    smarthub::Device lock("k1", "Lock", smarthub::DeviceType::Lock);
    smarthub::Device camera("c1", "Camera", smarthub::DeviceType::Camera);

    EXPECT_EQ(light.type(), smarthub::DeviceType::Light);
    EXPECT_EQ(sensor.type(), smarthub::DeviceType::Sensor);
    EXPECT_EQ(thermostat.type(), smarthub::DeviceType::Thermostat);
    EXPECT_EQ(lock.type(), smarthub::DeviceType::Lock);
    EXPECT_EQ(camera.type(), smarthub::DeviceType::Camera);
}

TEST_F(DeviceTest, TypeToString) {
    EXPECT_EQ(smarthub::Device::typeToString(smarthub::DeviceType::Light), "light");
    EXPECT_EQ(smarthub::Device::typeToString(smarthub::DeviceType::Sensor), "sensor");
    EXPECT_EQ(smarthub::Device::typeToString(smarthub::DeviceType::Thermostat), "thermostat");
    EXPECT_EQ(smarthub::Device::typeToString(smarthub::DeviceType::Lock), "lock");
    EXPECT_EQ(smarthub::Device::typeToString(smarthub::DeviceType::Camera), "camera");
    EXPECT_EQ(smarthub::Device::typeToString(smarthub::DeviceType::Unknown), "unknown");
}

TEST_F(DeviceTest, StringToType) {
    EXPECT_EQ(smarthub::Device::stringToType("light"), smarthub::DeviceType::Light);
    EXPECT_EQ(smarthub::Device::stringToType("sensor"), smarthub::DeviceType::Sensor);
    EXPECT_EQ(smarthub::Device::stringToType("thermostat"), smarthub::DeviceType::Thermostat);
    EXPECT_EQ(smarthub::Device::stringToType("lock"), smarthub::DeviceType::Lock);
    EXPECT_EQ(smarthub::Device::stringToType("camera"), smarthub::DeviceType::Camera);
    EXPECT_EQ(smarthub::Device::stringToType("invalid"), smarthub::DeviceType::Unknown);
}

TEST_F(DeviceTest, OnlineStatus) {
    smarthub::Device device("d1", "Device", smarthub::DeviceType::Light);

    // Default should be online (per implementation)
    EXPECT_TRUE(device.isOnline());

    device.setOnline(false);
    EXPECT_FALSE(device.isOnline());

    device.setOnline(true);
    EXPECT_TRUE(device.isOnline());
}

TEST_F(DeviceTest, RoomAssignment) {
    smarthub::Device device("d1", "Device", smarthub::DeviceType::Light);

    EXPECT_TRUE(device.room().empty());

    device.setRoom("Living Room");
    EXPECT_EQ(device.room(), "Living Room");

    device.setRoom("Bedroom");
    EXPECT_EQ(device.room(), "Bedroom");
}

TEST_F(DeviceTest, State) {
    smarthub::Device device("d1", "Device", smarthub::DeviceType::Light);

    // Set various state properties
    device.setState("power", std::string("on"));
    device.setState("brightness", 75);
    device.setState("color", std::string("#FF0000"));

    // Get state
    auto power = device.getState("power");
    auto brightness = device.getState("brightness");
    auto color = device.getState("color");

    EXPECT_EQ(std::any_cast<std::string>(power), "on");
    EXPECT_EQ(std::any_cast<int>(brightness), 75);
    EXPECT_EQ(std::any_cast<std::string>(color), "#FF0000");
}

TEST_F(DeviceTest, NonexistentState) {
    smarthub::Device device("d1", "Device", smarthub::DeviceType::Light);

    // Getting non-existent state should return empty any
    auto state = device.getState("nonexistent");
    EXPECT_FALSE(state.has_value());
}

TEST_F(DeviceTest, UpdateState) {
    smarthub::Device device("d1", "Device", smarthub::DeviceType::Light);

    device.setState("power", std::string("off"));
    EXPECT_EQ(std::any_cast<std::string>(device.getState("power")), "off");

    device.setState("power", std::string("on"));
    EXPECT_EQ(std::any_cast<std::string>(device.getState("power")), "on");
}

TEST_F(DeviceTest, GetAllState) {
    smarthub::Device device("d1", "Device", smarthub::DeviceType::Light);

    device.setState("power", std::string("on"));
    device.setState("brightness", 50);

    auto allState = device.getAllState();
    EXPECT_EQ(allState.size(), 2u);
}

TEST_F(DeviceTest, ToJson) {
    smarthub::Device device("light1", "Kitchen Light", smarthub::DeviceType::Light);
    device.setRoom("Kitchen");
    device.setOnline(true);

    auto json = device.toJson();

    // Check JSON structure
    EXPECT_EQ(json["id"], "light1");
    EXPECT_EQ(json["name"], "Kitchen Light");
    EXPECT_EQ(json["room"], "Kitchen");
    EXPECT_EQ(json["online"], true);
}

TEST_F(DeviceTest, FromJson) {
    nlohmann::json json = {
        {"id", "sensor1"},
        {"name", "Temperature Sensor"},
        {"type", "sensor"},
        {"room", "Bedroom"},
        {"online", false}
    };

    smarthub::Device device("", "", smarthub::DeviceType::Unknown);
    device.fromJson(json);

    EXPECT_EQ(device.room(), "Bedroom");
    EXPECT_FALSE(device.isOnline());
}

TEST_F(DeviceTest, StateCallback) {
    smarthub::Device device("d1", "Device", smarthub::DeviceType::Light);

    bool callbackCalled = false;
    std::string changedProperty;

    device.setStateCallback([&](const std::string& property, const std::any& /*value*/) {
        callbackCalled = true;
        changedProperty = property;
    });

    device.setState("power", std::string("on"));

    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(changedProperty, "power");
}

TEST_F(DeviceTest, ProtocolDefault) {
    smarthub::Device device("d1", "Device", smarthub::DeviceType::Light);
    EXPECT_EQ(device.protocol(), smarthub::DeviceProtocol::Local);
}

TEST_F(DeviceTest, ProtocolToString) {
    EXPECT_EQ(smarthub::Device::protocolToString(smarthub::DeviceProtocol::Local), "local");
    EXPECT_EQ(smarthub::Device::protocolToString(smarthub::DeviceProtocol::MQTT), "mqtt");
    EXPECT_EQ(smarthub::Device::protocolToString(smarthub::DeviceProtocol::Zigbee), "zigbee");
}
