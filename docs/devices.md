# SmartHub Device Abstraction Layer

This document describes the device abstraction layer implemented in Phase 3.

## Overview

The device abstraction layer provides a flexible, extensible architecture for representing smart home devices of any type. It consists of:

- **IDevice Interface** - Abstract interface for all devices
- **Device Base Class** - Common implementation with JSON state storage
- **Device Types** - Specialized classes for switches, dimmers, lights, sensors
- **DeviceTypeRegistry** - Factory for creating devices by type
- **Room/DeviceGroup** - Organizational structures

## Architecture

```
IDevice (interface)
    |
    +-- Device (base class with JSON state storage)
           |
           +-- SwitchDevice (OnOff capability)
           |
           +-- DimmerDevice (OnOff + Brightness)
           |      |
           |      +-- ColorLightDevice (+ ColorTemp + ColorRGB)
           |
           +-- TemperatureSensor (Temperature + optional Humidity/Battery)
           |
           +-- MotionSensor (Motion + optional Illuminance/Battery)
```

## Device Capabilities

Devices declare their capabilities using the `DeviceCapability` enum:

| Capability | Description | Example Devices |
|------------|-------------|-----------------|
| OnOff | Can be turned on/off | Switch, Light, Outlet |
| Brightness | Adjustable brightness (0-100) | Dimmer, Light |
| ColorTemperature | Adjustable color temp (Kelvin) | Color Light |
| ColorRGB | RGB color control | Color Light |
| Temperature | Reports temperature | Temp Sensor, Thermostat |
| Humidity | Reports humidity | Temp Sensor |
| Motion | Detects motion | Motion Sensor |
| Illuminance | Reports light level | Motion Sensor |
| Battery | Battery-powered with level reporting | Sensors |

## Device Types

### SwitchDevice

Basic on/off control device.

```cpp
#include <smarthub/devices/types/SwitchDevice.hpp>

SwitchDevice sw("switch1", "Kitchen Light", "mqtt", "zigbee/0x1234");

sw.turnOn();           // Turn on
sw.turnOff();          // Turn off
sw.toggle();           // Toggle state
bool on = sw.isOn();   // Check state
```

### DimmerDevice

Extends SwitchDevice with brightness control.

```cpp
#include <smarthub/devices/types/DimmerDevice.hpp>

DimmerDevice dimmer("dimmer1", "Living Room");

dimmer.setBrightness(75);    // Set to 75%
int level = dimmer.brightness();
dimmer.turnOff();            // Sets brightness to 0
dimmer.turnOn();             // Restores previous brightness
```

### ColorLightDevice

Full-featured color light with temperature and RGB control.

```cpp
#include <smarthub/devices/types/ColorLightDevice.hpp>

ColorLightDevice light("light1", "Bedroom Light");

// Brightness
light.setBrightness(100);

// Color temperature (2700K = warm, 6500K = cool)
light.setColorTemperature(4000);

// RGB color
light.setColorRGB(255, 128, 0);  // Orange
RGB rgb = light.colorRGB();

// HSV color
light.setColorHSV(240, 100, 100);  // Blue
HSV hsv = light.colorHSV();
```

### TemperatureSensor

Temperature sensor with optional humidity and battery.

```cpp
#include <smarthub/devices/types/TemperatureSensor.hpp>

// Create with humidity and battery support
TemperatureSensor sensor("temp1", "Office Sensor", "zigbee", "0x5678",
                          true,   // has humidity
                          true);  // has battery

// Update readings (typically from protocol handler)
sensor.setTemperature(22.5);
sensor.setHumidity(45.0);
sensor.setBatteryLevel(85);

// Read values
double temp = sensor.temperature();    // Celsius
double humidity = sensor.humidity();   // Percentage
int battery = sensor.batteryLevel();   // Percentage
```

### MotionSensor

Motion detection with optional illuminance and battery.

```cpp
#include <smarthub/devices/types/MotionSensor.hpp>

MotionSensor motion("motion1", "Hallway", "zigbee", "0xABCD",
                     true,   // has illuminance
                     true);  // has battery

motion.setMotionDetected(true);
motion.setIlluminance(500);  // Lux

bool detected = motion.motionDetected();
uint64_t lastMotion = motion.lastMotionTime();  // Unix timestamp
int lux = motion.illuminance();
```

## Device State Management

All devices use JSON for state storage:

```cpp
Device device("dev1", "Device", DeviceType::Light);

// Set state properties
device.setState("on", true);
device.setState("brightness", 75);
device.setState("color", "red");

// Get individual property
nlohmann::json on = device.getProperty("on");        // true
nlohmann::json brightness = device.getProperty("brightness");  // 75

// Get full state
nlohmann::json state = device.getState();
// {"on": true, "brightness": 75, "color": "red"}
```

### State Callbacks

Register callbacks to be notified of state changes:

```cpp
device.setStateCallback([](const std::string& property, const nlohmann::json& value) {
    std::cout << "Property " << property << " changed to " << value << std::endl;
});

device.setState("on", true);  // Triggers callback
```

## Device Serialization

Devices can be serialized to/from JSON:

```cpp
// Serialize
nlohmann::json json = device.toJson();
/*
{
    "id": "light1",
    "name": "Kitchen Light",
    "type": "light",
    "protocol": "mqtt",
    "protocol_address": "zigbee/0x1234",
    "room": "Kitchen",
    "availability": "online",
    "state": {"on": true, "brightness": 75}
}
*/

// Deserialize
auto restored = Device::fromJson(json);
```

## DeviceTypeRegistry

Factory for creating devices by type:

```cpp
auto& registry = DeviceTypeRegistry::instance();

// Create by DeviceType enum
auto device = registry.create(DeviceType::Switch, "sw1", "Switch 1");

// Create by type name string
auto sensor = registry.createFromTypeName("temperature_sensor", "ts1", "Temp Sensor");

// Create with configuration
nlohmann::json config = {{"has_humidity", true}, {"has_battery", true}};
auto configuredSensor = registry.createFromTypeName(
    "temperature_sensor", "ts2", "Configured Sensor",
    "zigbee", "0x1234", config
);
```

## Room Management

Rooms organize devices by location:

```cpp
#include <smarthub/devices/Room.hpp>

Room livingRoom("living_room", "Living Room");
livingRoom.setIcon("mdi:sofa");
livingRoom.setSortOrder(1);
livingRoom.setFloor(0);  // Ground floor

// Serialize
nlohmann::json json = livingRoom.toJson();

// Deserialize
Room restored = Room::fromJson(json);
```

## Device Groups

Groups allow controlling multiple devices together:

```cpp
#include <smarthub/devices/DeviceGroup.hpp>

DeviceGroup allLights("all_lights", "All Lights");
allLights.addDevice("light1");
allLights.addDevice("light2");
allLights.addDevice("light3");

// Get device IDs
std::vector<std::string> ids = allLights.deviceIds();

// Remove device
allLights.removeDevice("light2");
```

## Device Availability

Devices track their availability status:

```cpp
device.setAvailability(DeviceAvailability::Online);
device.setAvailability(DeviceAvailability::Offline);
device.setAvailability(DeviceAvailability::Unknown);

bool available = device.isAvailable();  // True if Online
uint64_t lastSeen = device.lastSeen();  // Unix timestamp
```

## Type Conversion

Convert between DeviceType enum and strings:

```cpp
// Enum to string
std::string typeStr = deviceTypeToString(DeviceType::TemperatureSensor);
// Returns "temperature_sensor"

// String to enum
DeviceType type = stringToDeviceType("switch");
// Returns DeviceType::Switch
```

## Adding Custom Device Types

1. Create a new class extending `Device`:

```cpp
class CustomDevice : public Device {
public:
    CustomDevice(const std::string& id, const std::string& name,
                 const std::string& protocol = "local",
                 const std::string& protocolAddress = "")
        : Device(id, name, DeviceType::Generic, protocol, protocolAddress)
    {
        addCapability(DeviceCapability::OnOff);
        // Add other capabilities
    }

    // Add custom methods
    void customOperation() {
        setState("custom_property", someValue);
    }
};
```

2. Register with DeviceTypeRegistry if factory creation is needed:

```cpp
DeviceTypeRegistry::instance().registerType(
    DeviceType::Generic,
    "custom",
    [](const std::string& id, const std::string& name,
       const std::string& protocol, const std::string& address,
       const nlohmann::json& config) {
        return std::make_shared<CustomDevice>(id, name, protocol, address);
    }
);
```

## Thread Safety

The Device base class uses mutex protection for state access. All state operations are thread-safe:

- `getState()` - Thread-safe read
- `setState()` - Thread-safe write with callback invocation
- `getProperty()` - Thread-safe read

## See Also

- [Protocol Handlers](protocols.md) - Protocol integration
- [Architecture](architecture.md) - System architecture
- [Testing](testing.md) - Testing guide
