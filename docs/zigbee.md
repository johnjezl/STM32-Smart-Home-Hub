# SmartHub Zigbee Protocol

This document describes the Zigbee protocol implementation for SmartHub using the CC2652P coordinator and TI Z-Stack ZNP protocol.

## Overview

SmartHub supports Zigbee devices through a CC2652P-based coordinator running Z-Stack firmware. The implementation uses the ZNP (Zigbee Network Processor) protocol for communication between the STM32MP1 application processor and the CC2652P.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      SmartHub Application                     │
├───────────────────┬─────────────────────────────────────────┤
│   ZigbeeHandler   │  IProtocolHandler implementation        │
│         │         │  Device discovery, state management     │
│         ▼         │                                         │
│ ZigbeeCoordinator │  Network control, ZCL operations        │
│         │         │                                         │
│         ▼         │                                         │
│   ZnpTransport    │  Serial framing, request/response       │
│         │         │                                         │
│         ▼         │                                         │
│     ZnpFrame      │  Frame parsing/serialization            │
├─────────────────────────────────────────────────────────────┤
│                   Serial Port (UART)                         │
├─────────────────────────────────────────────────────────────┤
│              CC2652P (Z-Stack Coordinator)                   │
├─────────────────────────────────────────────────────────────┤
│                   Zigbee Mesh Network                        │
│     ┌─────────┐    ┌─────────┐    ┌─────────┐              │
│     │  Light  │    │ Sensor  │    │ Switch  │              │
│     └─────────┘    └─────────┘    └─────────┘              │
└─────────────────────────────────────────────────────────────┘
```

## Components

### ZnpFrame

Frame parser and builder for the Z-Stack ZNP protocol.

**Frame Format:**
```
+------+--------+------+------+---------+-----+
| SOF  | Length | Cmd0 | Cmd1 | Payload | FCS |
| 0xFE | 1 byte | 1    | 1    | N bytes | 1   |
+------+--------+------+------+---------+-----+
```

**Usage:**

```cpp
#include <smarthub/protocols/zigbee/ZnpFrame.hpp>

using namespace smarthub::zigbee;

// Build a SYS_PING request
ZnpFrame frame(ZnpType::SREQ, ZnpSubsystem::SYS, cmd::sys::PING);

// Build frame with payload
ZnpFrame dataReq(ZnpType::SREQ, ZnpSubsystem::AF, cmd::af::DATA_REQUEST);
dataReq.appendWord(0x1234);   // Destination address
dataReq.appendByte(0x01);     // Destination endpoint
dataReq.appendByte(0x01);     // Source endpoint
dataReq.appendWord(0x0006);   // Cluster ID (On/Off)

// Serialize for transmission
std::vector<uint8_t> bytes = frame.serialize();

// Parse received frame
auto parsed = ZnpFrame::parse(data, length);
if (parsed) {
    if (parsed->isResponse()) {
        // Handle response
    }
}
```

### ZnpTransport

Serial transport layer with async indication support.

```cpp
#include <smarthub/protocols/zigbee/ZnpTransport.hpp>

ZnpTransport transport("/dev/ttyUSB0", 115200);

if (transport.open()) {
    // Set callback for async indications
    transport.setIndicationCallback([](const ZnpFrame& frame) {
        // Handle device announcements, incoming messages, etc.
    });

    // Send synchronous request
    ZnpFrame ping(ZnpType::SREQ, ZnpSubsystem::SYS, cmd::sys::PING);
    auto response = transport.request(ping, 5000);  // 5 second timeout

    if (response) {
        // Got response
    }

    transport.close();
}
```

### ZigbeeCoordinator

High-level coordinator control and device management.

```cpp
#include <smarthub/protocols/zigbee/ZigbeeCoordinator.hpp>

ZigbeeCoordinator coordinator("/dev/ttyUSB0", 115200);

// Set callbacks
coordinator.setDeviceAnnouncedCallback([](uint16_t nwkAddr, uint64_t ieee) {
    std::cout << "Device joined: " << std::hex << ieee << std::endl;
});

coordinator.setAttributeReportCallback([](uint16_t nwkAddr, const ZclAttributeValue& attr) {
    if (attr.clusterId == zcl::cluster::TEMPERATURE_MEASUREMENT) {
        double temp = attr.asInt16() / 100.0;
        std::cout << "Temperature: " << temp << "°C" << std::endl;
    }
});

// Initialize and start network
if (coordinator.initialize() && coordinator.startNetwork()) {
    // Enable pairing for 60 seconds
    coordinator.permitJoin(60);

    // Control a light
    coordinator.setOnOff(0x1234, 1, true);  // Turn on
    coordinator.setLevel(0x1234, 1, 128);   // Set brightness to 50%
}
```

### ZigbeeHandler

IProtocolHandler implementation for DeviceManager integration.

```cpp
#include <smarthub/protocols/zigbee/ZigbeeHandler.hpp>

EventBus eventBus;
nlohmann::json config = {
    {"port", "/dev/ttyUSB0"},
    {"baudRate", 115200},
    {"deviceDatabase", "/etc/smarthub/zigbee_devices.json"}
};

ZigbeeHandler handler(eventBus, config);

handler.setDeviceDiscoveredCallback([](DevicePtr device) {
    std::cout << "Discovered: " << device->name() << std::endl;
});

handler.setDeviceStateCallback([](const std::string& id,
                                   const std::string& prop,
                                   const nlohmann::json& val) {
    std::cout << id << "." << prop << " = " << val << std::endl;
});

if (handler.initialize()) {
    // Start device discovery
    handler.startDiscovery();

    // Send command to device
    handler.sendCommand("zigbee_001122334455", "on", {{"on", true}});
}
```

## ZCL Clusters

The implementation supports the following Zigbee Cluster Library (ZCL) clusters:

### General Clusters

| Cluster | ID | Description |
|---------|-----|-------------|
| Basic | 0x0000 | Device identification |
| Power Configuration | 0x0001 | Battery status |
| Identify | 0x0003 | Device identification actions |
| Groups | 0x0004 | Group membership |
| Scenes | 0x0005 | Scene configuration |
| On/Off | 0x0006 | Binary on/off control |
| Level Control | 0x0008 | Brightness/level control |

### Lighting Clusters

| Cluster | ID | Description |
|---------|-----|-------------|
| Color Control | 0x0300 | Hue, saturation, color temp |

### Measurement Clusters

| Cluster | ID | Description |
|---------|-----|-------------|
| Illuminance | 0x0400 | Light level measurement |
| Temperature | 0x0402 | Temperature measurement |
| Relative Humidity | 0x0405 | Humidity measurement |
| Occupancy Sensing | 0x0406 | Motion detection |

### Security Clusters

| Cluster | ID | Description |
|---------|-----|-------------|
| IAS Zone | 0x0500 | Security sensors |

### Metering Clusters

| Cluster | ID | Description |
|---------|-----|-------------|
| Electrical Measurement | 0x0B04 | Power monitoring |
| Metering | 0x0702 | Energy measurement |

## Device Database

The device database maps manufacturer/model strings to device types and capabilities.

**Example zigbee_devices.json:**

```json
{
  "devices": [
    {
      "manufacturer": "IKEA of Sweden",
      "model": "TRADFRI bulb E27 WS opal 1000lm",
      "displayName": "IKEA TRADFRI Bulb",
      "deviceType": "color_light",
      "quirks": {
        "colorTempRange": [250, 454]
      }
    },
    {
      "manufacturer": "LUMI",
      "model": "lumi.weather",
      "displayName": "Aqara Temperature Sensor",
      "deviceType": "temperature_sensor",
      "quirks": {
        "requiresRejoin": true
      }
    },
    {
      "manufacturer": "Philips",
      "model": "LWB010",
      "displayName": "Philips Hue White",
      "deviceType": "dimmer"
    }
  ]
}
```

## Device Type Inference

When a device joins without a database entry, the handler infers the type from clusters:

| Clusters Present | Inferred Type |
|-----------------|---------------|
| On/Off + Level + Color | ColorLight |
| On/Off + Level | Dimmer |
| On/Off only | Switch |
| Temperature | TemperatureSensor |
| IAS Zone / Occupancy | MotionSensor |

## Commands

### Switch/Light Commands

```cpp
// Turn on
handler.sendCommand(deviceId, "on", {{"on", true}});

// Turn off
handler.sendCommand(deviceId, "off", {{"on", false}});

// Toggle
handler.sendCommand(deviceId, "toggle", {{"toggle", true}});
```

### Dimmer Commands

```cpp
// Set brightness (0-254)
handler.sendCommand(deviceId, "brightness", {
    {"brightness", 128},
    {"transitionTime", 10}  // 1 second (in 100ms units)
});
```

### Color Light Commands

```cpp
// Set color temperature (mireds)
handler.sendCommand(deviceId, "colorTemp", {
    {"colorTemp", 350},
    {"transitionTime", 10}
});

// Set hue and saturation
handler.sendCommand(deviceId, "color", {
    {"hue", 85},        // 0-254
    {"saturation", 200}, // 0-254
    {"transitionTime", 10}
});
```

## Attribute Reporting

The handler automatically configures attribute reporting for discovered devices:

| Device Type | Reported Attributes | Min/Max Interval |
|-------------|-------------------|------------------|
| Switch/Light | On/Off state | 1s / 5min |
| Dimmer | Current level | 1s / 5min, 1% change |
| Color Light | Color temp | 1s / 5min, 10 mireds change |
| Temperature | Temperature | 1min / 5min, 0.5°C change |

## Testing

The Zigbee implementation includes 42 unit tests with a mock serial port:

```cpp
// Mock serial port for testing
class MockSerialPort : public ISerialPort {
public:
    bool open(const std::string& port, int baud) override;
    ssize_t write(const uint8_t* data, size_t len) override;
    ssize_t read(uint8_t* buf, size_t max, int timeout) override;

    // Test helpers
    void queueReadData(const std::vector<uint8_t>& data);
    std::vector<uint8_t> getWrittenData();
};
```

Run tests:
```bash
cd app/build-test
cmake .. -DSMARTHUB_BUILD_TESTS=ON -DSMARTHUB_NATIVE_BUILD=ON
make test_zigbee
./tests/test_zigbee
```

## Hardware Setup

### CC2652P Module

Recommended: SONOFF Zigbee 3.0 USB Dongle Plus (CC2652P)

### Firmware

Flash Z-Stack coordinator firmware from [Koenkk/Z-Stack-firmware](https://github.com/Koenkk/Z-Stack-firmware):

```bash
# Install flasher
pip3 install pyserial intelhex
git clone https://github.com/JelmerT/cc2538-bsl.git

# Flash (hold boot button while plugging in USB)
python3 cc2538-bsl.py -p /dev/ttyUSB0 -evw CC2652P_coordinator.hex
```

### UART Connection

Connect CC2652P to STM32MP1:
```
CC2652P      STM32MP1
-------      --------
VCC    -->   3.3V
GND    -->   GND
TX     -->   UART RX
RX     -->   UART TX
```

## Configuration

Example configuration in smarthub.yaml:

```yaml
protocols:
  zigbee:
    enabled: true
    port: /dev/ttyUSB0
    baudRate: 115200
    deviceDatabase: /etc/smarthub/zigbee_devices.json
    channel: 15  # Optional, default auto
    panId: 0x1A62  # Optional, default random
```

## UI Integration Notes

### Thread Safety with LVGL

The Zigbee handler callbacks run on background threads (from the ZnpTransport read loop). When updating LVGL UI elements from these callbacks, you **must** use `lv_async_call()` to schedule the update on the main thread:

```cpp
// WRONG - will cause intermittent UI glitches
handler.setDeviceDiscoveredCallback([](DevicePtr device) {
    lv_label_set_text(statusLabel, "Device found!");  // Not thread-safe!
});

// CORRECT - defers UI update to main thread
handler.setDeviceDiscoveredCallback([this](DevicePtr device) {
    // Store data for async callback
    m_discoveredDevice = device;

    // Schedule UI update on main thread
    lv_async_call([](void* userData) {
        auto* self = static_cast<MyScreen*>(userData);
        lv_label_set_text(self->m_statusLabel, "Device found!");
    }, this);
});
```

This applies to all Zigbee callbacks including:
- `setDeviceDiscoveredCallback` - Device pairing complete
- `setDeviceStateCallback` - Device state updates
- EventBus handlers for Zigbee events

## Troubleshooting

### Device Won't Pair

1. Ensure permit join is enabled
2. Put device in pairing mode (usually hold button for 5+ seconds)
3. Check coordinator logs for device announce messages

### Commands Not Working

1. Verify device network address is correct
2. Check endpoint number (usually 1 for single-endpoint devices)
3. Verify cluster is supported by device

### Attribute Reports Not Received

1. Configure reporting after device joins
2. Some battery devices only report on state change
3. Check minimum reporting interval

## See Also

- [Protocol Handler Framework](protocols.md) - Protocol abstraction layer
- [Device Abstraction Layer](devices.md) - Device types and capabilities
- [Architecture](architecture.md) - System architecture
- [Testing](testing.md) - Testing guide
