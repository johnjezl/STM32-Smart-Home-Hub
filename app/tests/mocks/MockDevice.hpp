/**
 * Mock Device for Testing
 */
#pragma once

#include <smarthub/devices/Device.hpp>

namespace smarthub::testing {

/**
 * Mock device for unit testing
 */
class MockDevice : public Device {
public:
    MockDevice(const std::string& id = "mock-001",
               const std::string& name = "Mock Device",
               DeviceType type = DeviceType::Switch)
        : Device(id, name, type, "mock", "mock://device")
    {
        addCapability(DeviceCapability::OnOff);
        setStateInternal("on", false);
        setAvailability(DeviceAvailability::Online);
    }

    // Track calls for testing
    int setStateCalls = 0;
    std::string lastProperty;
    nlohmann::json lastValue;

protected:
    void onStateChange(const std::string& property, const nlohmann::json& value) override {
        setStateCalls++;
        lastProperty = property;
        lastValue = value;
    }
};

/**
 * Mock dimmer device for testing
 */
class MockDimmerDevice : public Device {
public:
    MockDimmerDevice(const std::string& id = "mock-dimmer-001",
                     const std::string& name = "Mock Dimmer")
        : Device(id, name, DeviceType::Dimmer, "mock", "mock://dimmer")
    {
        addCapability(DeviceCapability::OnOff);
        addCapability(DeviceCapability::Brightness);
        setStateInternal("on", false);
        setStateInternal("brightness", 0);
        setAvailability(DeviceAvailability::Online);
    }
};

/**
 * Mock sensor device for testing
 */
class MockSensorDevice : public Device {
public:
    MockSensorDevice(const std::string& id = "mock-sensor-001",
                     const std::string& name = "Mock Sensor")
        : Device(id, name, DeviceType::TemperatureSensor, "mock", "mock://sensor")
    {
        addCapability(DeviceCapability::Temperature);
        addCapability(DeviceCapability::Humidity);
        setStateInternal("temperature", 22.5);
        setStateInternal("humidity", 45.0);
        setAvailability(DeviceAvailability::Online);
    }
};

} // namespace smarthub::testing
