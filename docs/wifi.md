# SmartHub WiFi Protocol Handler

This document describes the WiFi protocol handler implemented in Phase 5, which provides unified support for WiFi-based smart devices.

## Overview

The WiFi protocol handler integrates multiple WiFi device protocols:

- **MQTT Discovery** - Home Assistant MQTT Discovery format (Tasmota, ESPHome)
- **Shelly Devices** - Gen1 REST API and Gen2 JSON-RPC
- **Tuya Local** - Direct TCP communication with Tuya/Smart Life devices

## Architecture

```
WifiHandler (IProtocolHandler)
    |
    +-- MqttClient (MQTT broker connection)
    |       |
    |       +-- MqttDiscoveryManager
    |               - Tasmota device handling
    |               - ESPHome device handling
    |               - Generic MQTT Discovery
    |
    +-- HttpClient (HTTP requests)
    |       |
    |       +-- ShellyDiscovery (device probing)
    |       +-- ShellyGen1Device (REST API)
    |       +-- ShellyGen2Device (JSON-RPC)
    |
    +-- TuyaDevice (TCP connections)
            - Protocol versions 3.1, 3.3, 3.4, 3.5
            - AES encryption
```

## Configuration

```json
{
    "mqtt": {
        "broker": "192.168.1.100",
        "port": 1883,
        "username": "smarthub",
        "password": "secret",
        "clientId": "smarthub_main"
    }
}
```

## MQTT Discovery Protocol

### Topic Format

The MQTT Discovery protocol uses Home Assistant's format:

```
<discovery_prefix>/<component>/<node_id>/<object_id>/config
```

Example topics:
- `homeassistant/switch/kitchen_plug/config`
- `homeassistant/light/living_room/bulb1/config`
- `homeassistant/sensor/outdoor/temperature/config`

### Discovery Payload

```json
{
    "name": "Kitchen Plug",
    "unique_id": "kitchen_plug_001",
    "state_topic": "stat/kitchen_plug/POWER",
    "command_topic": "cmnd/kitchen_plug/POWER",
    "availability_topic": "tele/kitchen_plug/LWT",
    "payload_on": "ON",
    "payload_off": "OFF",
    "payload_available": "Online",
    "payload_not_available": "Offline",
    "device": {
        "identifiers": ["kitchen_plug_001"],
        "name": "Kitchen Plug",
        "manufacturer": "Tasmota",
        "model": "Sonoff S31"
    }
}
```

### Supported Device Classes

| Component | Device Type Created |
|-----------|---------------------|
| `switch` | SwitchDevice |
| `light` | DimmerDevice or ColorLightDevice |
| `sensor` | TemperatureSensor |
| `binary_sensor` | MotionSensor |

### Usage

```cpp
#include <smarthub/protocols/wifi/MqttDiscovery.hpp>

MqttDiscoveryManager discovery;

discovery.setDiscoveryCallback([](const MqttDiscoveryConfig& config) {
    std::cout << "Discovered: " << config.name << std::endl;
});

discovery.setStateCallback([](const std::string& id, const std::string& prop,
                               const nlohmann::json& value) {
    std::cout << id << "." << prop << " = " << value << std::endl;
});

// Process incoming MQTT message
discovery.processMessage("homeassistant/switch/test/config", payload);
```

## Tasmota Devices

Tasmota devices are automatically discovered via MQTT Discovery.

### Topic Patterns

| Topic | Description |
|-------|-------------|
| `cmnd/<device>/POWER` | Command (ON/OFF/TOGGLE) |
| `stat/<device>/POWER` | State response |
| `stat/<device>/RESULT` | Command result |
| `tele/<device>/STATE` | Periodic telemetry |
| `tele/<device>/LWT` | Last Will (online/offline) |

### Source Detection

Tasmota devices are identified by:
- `device.manufacturer` containing "Tasmota"
- Topic patterns starting with `cmnd/`, `stat/`, `tele/`
- Discovery payload structure

## ESPHome Devices

ESPHome devices also use Home Assistant MQTT Discovery.

### Source Detection

ESPHome devices are identified by:
- `device.manufacturer` containing "ESPHome"
- Discovery topic patterns

## Shelly Devices

### Gen1 Devices (REST API)

Shelly Gen1 devices use a simple HTTP REST API.

**Endpoints:**
| Endpoint | Description |
|----------|-------------|
| `GET /shelly` | Device info |
| `GET /settings` | Device settings |
| `GET /status` | Full status |
| `GET /relay/0?turn=on` | Turn relay on |
| `GET /relay/0?turn=off` | Turn relay off |
| `GET /relay/0?turn=toggle` | Toggle relay |
| `GET /light/0?brightness=75` | Set brightness |

**Usage:**
```cpp
#include <smarthub/protocols/wifi/ShellyDevice.hpp>

ShellyDeviceInfo info;
info.ipAddress = "192.168.1.50";
info.type = "SHSW-1";
info.gen2 = false;

HttpClient http;
auto device = createShellyDevice(info, http);

auto* gen1 = dynamic_cast<ShellyGen1Device*>(device.get());
gen1->turnOn(0);
gen1->setBrightness(0, 75);
```

### Gen2 Devices (JSON-RPC)

Shelly Gen2 (Plus/Pro) devices use JSON-RPC over HTTP.

**RPC Methods:**
| Method | Description |
|--------|-------------|
| `Shelly.GetStatus` | Full device status |
| `Shelly.GetConfig` | Device configuration |
| `Switch.Set` | Control switch |
| `Light.Set` | Control light |

**RPC Request Format:**
```json
{
    "id": 1,
    "method": "Switch.Set",
    "params": {
        "id": 0,
        "on": true
    }
}
```

**Usage:**
```cpp
auto* gen2 = dynamic_cast<ShellyGen2Device*>(device.get());
gen2->turnOn(0);
gen2->setBrightness(0, 75);
```

### Device Discovery

```cpp
#include <smarthub/protocols/wifi/ShellyDevice.hpp>

HttpClient http;
ShellyDiscovery discovery(http);

discovery.setCallback([](const ShellyDeviceInfo& info) {
    std::cout << "Found Shelly: " << info.name
              << " (" << info.type << ") at " << info.ipAddress << std::endl;
});

// Probe a specific IP
auto info = discovery.probeDevice("192.168.1.50");
if (info) {
    // Device found
}
```

## Tuya Local Protocol

The Tuya local protocol enables direct control of Tuya/Smart Life devices without cloud dependency.

### Requirements

Each Tuya device requires:
- **Device ID** - 20-character identifier
- **Local Key** - 16-byte AES encryption key
- **IP Address** - Device's local network address
- **Protocol Version** - 3.1, 3.3, 3.4, or 3.5

### Obtaining Credentials

Local keys can be obtained through:
1. Tuya IoT Platform Developer Account
2. Tuya-specific extraction tools (tinytuya, tuya-cli)

### Protocol Versions

| Version | Encryption | Notes |
|---------|------------|-------|
| 3.1 | AES-128-ECB | Legacy devices |
| 3.3 | AES-128-ECB | Most common |
| 3.4 | AES-128-GCM | Session negotiation |
| 3.5 | AES-128-GCM | Latest protocol |

### Message Format

```
+--------+--------+--------+--------+--------+--------+
| Prefix | SeqNo  | Cmd    | Length | Data   | Suffix |
| 4 bytes| 4 bytes| 4 bytes| 4 bytes| N bytes| 8 bytes|
+--------+--------+--------+--------+--------+--------+

Prefix: 0x000055AA
Suffix: 0x0000AA55 + CRC32
```

### Commands

| Command | Value | Description |
|---------|-------|-------------|
| `CONTROL` | 0x07 | Set data points |
| `STATUS` | 0x08 | Status response |
| `HEART_BEAT` | 0x09 | Keep-alive ping |
| `DP_QUERY` | 0x0A | Query data points |

### Data Points (DPs)

Tuya devices use numbered data points:

| DP ID | Typical Use |
|-------|-------------|
| 1 | Power on/off (bool) |
| 2 | Brightness (0-1000) |
| 3 | Color temperature |
| 4 | Color mode |
| 5 | HSV color |

### Usage

```cpp
#include <smarthub/protocols/wifi/TuyaDevice.hpp>

TuyaDeviceConfig config;
config.deviceId = "bf1234567890abcdef12";
config.localKey = "0123456789abcdef";
config.ipAddress = "192.168.1.60";
config.version = "3.3";
config.name = "Smart Plug";

auto device = std::make_unique<TuyaDevice>(
    "tuya_" + config.deviceId,
    config.name,
    config
);

device->setStateCallback([](const std::string& id,
                             const std::map<uint8_t, TuyaDataPoint>& dps) {
    for (const auto& [dpId, dp] : dps) {
        std::cout << "DP " << (int)dpId << " = " << dp.value << std::endl;
    }
});

if (device->connect()) {
    device->setDataPoint(1, true);   // Turn on
    device->setDataPoint(2, 500);    // 50% brightness
    device->queryStatus();
}
```

## WifiHandler Integration

The WifiHandler combines all WiFi protocols into a unified IProtocolHandler:

```cpp
#include <smarthub/protocols/wifi/WifiHandler.hpp>

EventBus eventBus;
nlohmann::json config = {
    {"mqtt", {
        {"broker", "192.168.1.100"},
        {"port", 1883}
    }}
};

WifiHandler handler(eventBus, config);
handler.initialize();

// Set discovery callback
handler.setDeviceDiscoveredCallback([](DevicePtr device) {
    std::cout << "Discovered: " << device->name() << std::endl;
});

// Set state callback
handler.setDeviceStateCallback([](const std::string& id,
                                   const std::string& prop,
                                   const nlohmann::json& value) {
    std::cout << id << "." << prop << " = " << value << std::endl;
});

// Start discovery (MQTT auto-discovery)
handler.startDiscovery();

// Add devices manually
handler.addShellyDevice("192.168.1.50");

TuyaDeviceConfig tuyaConfig;
tuyaConfig.deviceId = "...";
tuyaConfig.localKey = "...";
tuyaConfig.ipAddress = "192.168.1.60";
handler.addTuyaDevice(tuyaConfig);

// Send commands
handler.sendCommand("kitchen_plug", "on", {});
handler.sendCommand("living_room_dimmer", "brightness", {{"brightness", 75}});

// Poll (call in main loop)
handler.poll();

// Status
auto status = handler.getStatus();
std::cout << "Devices: " << status["deviceCount"] << std::endl;
```

## HTTP Client

The HTTP client is used internally for Shelly device communication:

```cpp
#include <smarthub/protocols/http/HttpClient.hpp>

HttpClient client;

// GET request
auto response = client.get("http://192.168.1.50/status");
if (response && response->ok()) {
    auto json = response->json();
}

// POST request
auto result = client.post("http://192.168.1.50/rpc",
                          R"({"id":1,"method":"Shelly.GetStatus"})");

// JSON convenience methods
auto json = client.getJson("http://192.168.1.50/shelly");
auto result = client.postJson("http://192.168.1.50/rpc",
                               {{"id", 1}, {"method", "Shelly.GetStatus"}});
```

## Testing

WiFi protocol tests are in `app/tests/protocols/test_wifi.cpp`:

```bash
cd app/build
./tests/test_wifi
```

Test suites:
- `HttpResponseTest` - HTTP response parsing
- `MqttDiscoveryTest` - MQTT Discovery protocol
- `ShellyDeviceTest` - Shelly device handling
- `TuyaCryptoTest` - Tuya encryption
- `TuyaMessageTest` - Tuya message framing
- `WifiHandlerTest` - Handler integration
- `WifiIntegrationTest` - End-to-end flows

## See Also

- [Protocol Handler Framework](protocols.md) - IProtocolHandler interface
- [MQTT Protocol](mqtt.md) - MQTT client details
- [Device Abstraction Layer](devices.md) - Device types
- [Testing](testing.md) - Test documentation
