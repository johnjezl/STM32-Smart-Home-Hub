# Phase 5: WiFi Device Integration - Detailed TODO

## Overview
**Duration**: 2 weeks  
**Objective**: Enable control of WiFi-based smart devices over local network.  
**Prerequisites**: Phase 3 complete (device framework)

---

## 5.1 MQTT Device Protocol (Tasmota/ESPHome)

### 5.1.1 MQTT Discovery Protocol
```cpp
// app/include/smarthub/protocols/mqtt/MqttDiscovery.hpp
#pragma once

#include <string>
#include <functional>
#include <nlohmann/json.hpp>

namespace smarthub::mqtt {

// Home Assistant MQTT Discovery format (used by Tasmota, ESPHome)
struct DiscoveryPayload {
    std::string uniqueId;
    std::string name;
    std::string deviceClass;      // switch, light, sensor, binary_sensor, etc.
    std::string stateTopic;
    std::string commandTopic;
    std::string availabilityTopic;
    std::string payloadOn;
    std::string payloadOff;
    std::string valueTemplate;
    nlohmann::json device;        // Device info (manufacturer, model, etc.)
    
    static std::optional<DiscoveryPayload> parse(const std::string& topic, 
                                                  const std::string& payload);
};

class MqttDiscovery {
public:
    using DeviceDiscoveredCallback = std::function<void(const DiscoveryPayload&)>;
    
    MqttDiscovery(MqttClient& client);
    
    void start();
    void stop();
    
    void setCallback(DeviceDiscoveredCallback cb) { m_callback = cb; }
    
private:
    void onMessage(const std::string& topic, const std::string& payload);
    
    MqttClient& m_client;
    DeviceDiscoveredCallback m_callback;
    bool m_running = false;
};

} // namespace smarthub::mqtt
```

### 5.1.2 Tasmota Device Handler
```cpp
// app/include/smarthub/protocols/mqtt/TasmotaDevice.hpp
#pragma once

#include <smarthub/devices/Device.hpp>
#include <string>

namespace smarthub::mqtt {

class MqttClient;

// Tasmota MQTT topics:
// cmnd/<device>/POWER    - Command
// stat/<device>/POWER    - State
// stat/<device>/RESULT   - Command result
// tele/<device>/STATE    - Periodic telemetry
// tele/<device>/SENSOR   - Sensor data
// tele/<device>/LWT      - Last Will (online/offline)

class TasmotaDevice : public Device {
public:
    TasmotaDevice(const std::string& id, const std::string& name,
                  const std::string& tasmotaTopic, MqttClient& mqtt);
    
    // Override state handling
    bool setState(const std::string& property, const nlohmann::json& value) override;
    
    // Process incoming MQTT messages
    void handleMessage(const std::string& topic, const std::string& payload);
    
    // Tasmota-specific
    std::string tasmotaTopic() const { return m_tasmotaTopic; }
    void requestStatus();
    
protected:
    void onStateChange(const std::string& property, const nlohmann::json& value) override;
    
private:
    void sendCommand(const std::string& command, const std::string& payload = "");
    void parseTelemetry(const nlohmann::json& data);
    void parseSensorData(const nlohmann::json& data);
    
    std::string m_tasmotaTopic;
    MqttClient& m_mqtt;
};

// Tasmota switch/relay
class TasmotaSwitch : public TasmotaDevice {
public:
    using TasmotaDevice::TasmotaDevice;
    
    void turnOn();
    void turnOff();
    void toggle();
};

// Tasmota dimmer
class TasmotaDimmer : public TasmotaDevice {
public:
    using TasmotaDevice::TasmotaDevice;
    
    void turnOn();
    void turnOff();
    void setBrightness(int level);  // 0-100
};

// Tasmota RGB/RGBW light
class TasmotaLight : public TasmotaDevice {
public:
    using TasmotaDevice::TasmotaDevice;
    
    void turnOn();
    void turnOff();
    void setBrightness(int level);
    void setColorRGB(uint8_t r, uint8_t g, uint8_t b);
    void setColorTemp(int kelvin);
    void setScheme(int scheme);  // 0=single, 1-4=color cycle schemes
};

} // namespace smarthub::mqtt
```

### 5.1.3 ESPHome Device Handler
```cpp
// app/include/smarthub/protocols/mqtt/ESPHomeDevice.hpp
#pragma once

#include <smarthub/devices/Device.hpp>

namespace smarthub::mqtt {

// ESPHome uses Home Assistant MQTT Discovery
// Topics follow pattern: <discovery_prefix>/<component>/<node_id>/<object_id>/config

class ESPHomeDevice : public Device {
public:
    ESPHomeDevice(const std::string& id, const std::string& name,
                  const DiscoveryPayload& config, MqttClient& mqtt);
    
    bool setState(const std::string& property, const nlohmann::json& value) override;
    void handleMessage(const std::string& topic, const std::string& payload);
    
private:
    void sendCommand(const nlohmann::json& payload);
    
    DiscoveryPayload m_config;
    MqttClient& m_mqtt;
};

} // namespace smarthub::mqtt
```

---

## 5.2 HTTP REST Device Protocol (Shelly)

### 5.2.1 Shelly Device Discovery
```cpp
// app/include/smarthub/protocols/http/ShellyDiscovery.hpp
#pragma once

#include <string>
#include <vector>
#include <functional>

namespace smarthub::http {

struct ShellyDeviceInfo {
    std::string id;
    std::string type;       // shelly1, shelly25, shellyplug, etc.
    std::string ipAddress;
    std::string macAddress;
    std::string firmware;
    bool gen2;              // Gen1 vs Gen2 API
    int numOutputs;
    bool hasPowerMetering;
};

class ShellyDiscovery {
public:
    using DiscoveryCallback = std::function<void(const ShellyDeviceInfo&)>;
    
    ShellyDiscovery();
    
    // mDNS-based discovery
    void startDiscovery();
    void stopDiscovery();
    
    // Manual addition by IP
    void probeDevice(const std::string& ipAddress);
    
    void setCallback(DiscoveryCallback cb) { m_callback = cb; }
    
private:
    void onMdnsResponse(const std::string& name, const std::string& ip);
    void queryDeviceInfo(const std::string& ip);
    
    DiscoveryCallback m_callback;
    bool m_running = false;
};

} // namespace smarthub::http
```

### 5.2.2 Shelly Gen1 API
```cpp
// app/include/smarthub/protocols/http/ShellyGen1Device.hpp
#pragma once

#include <smarthub/devices/Device.hpp>
#include <string>

namespace smarthub::http {

// Shelly Gen1 HTTP API
// GET /status - Device status
// GET /settings - Device settings
// GET /relay/0?turn=on|off|toggle - Control relay
// GET /light/0?brightness=X - Control dimmer
// GET /color/0?red=X&green=X&blue=X - Control RGB

class HttpClient;

class ShellyGen1Device : public Device {
public:
    ShellyGen1Device(const std::string& id, const std::string& name,
                     const std::string& ipAddress, const std::string& shellyType,
                     HttpClient& http);
    
    bool setState(const std::string& property, const nlohmann::json& value) override;
    
    // Poll for status updates
    void pollStatus();
    
    std::string ipAddress() const { return m_ipAddress; }
    
protected:
    void onStateChange(const std::string& property, const nlohmann::json& value) override;
    
private:
    std::string buildUrl(const std::string& path) const;
    void parseStatus(const nlohmann::json& status);
    
    std::string m_ipAddress;
    std::string m_shellyType;
    HttpClient& m_http;
};

class ShellyRelay : public ShellyGen1Device {
public:
    using ShellyGen1Device::ShellyGen1Device;
    
    void turnOn(int channel = 0);
    void turnOff(int channel = 0);
    void toggle(int channel = 0);
};

class ShellyDimmer : public ShellyGen1Device {
public:
    using ShellyGen1Device::ShellyGen1Device;
    
    void turnOn();
    void turnOff();
    void setBrightness(int level);
};

} // namespace smarthub::http
```

### 5.2.3 Shelly Gen2 API (RPC)
```cpp
// app/include/smarthub/protocols/http/ShellyGen2Device.hpp
#pragma once

#include <smarthub/devices/Device.hpp>

namespace smarthub::http {

// Shelly Gen2 uses JSON-RPC over HTTP and WebSocket
// POST /rpc with JSON-RPC 2.0 body
// WebSocket at ws://<ip>/rpc for real-time updates

class ShellyGen2Device : public Device {
public:
    ShellyGen2Device(const std::string& id, const std::string& name,
                     const std::string& ipAddress, HttpClient& http);
    
    bool setState(const std::string& property, const nlohmann::json& value) override;
    
    // Connect WebSocket for real-time updates
    bool connectWebSocket();
    void disconnectWebSocket();
    
private:
    nlohmann::json rpcCall(const std::string& method, const nlohmann::json& params = {});
    void onWebSocketMessage(const std::string& message);
    
    std::string m_ipAddress;
    HttpClient& m_http;
};

} // namespace smarthub::http
```

---

## 5.3 Tuya Local Protocol

### 5.3.1 Tuya Protocol Overview
```
Tuya devices communicate using a custom TCP protocol on port 6668.
Each device has:
- Device ID (20 chars)
- Local Key (16 bytes AES key)
- Protocol version (3.1, 3.3, 3.4, 3.5)

Message format:
+--------+--------+--------+--------+--------+--------+
| Prefix | SeqNo  | Cmd    | Length | Data   | Suffix |
| 4 bytes| 4 bytes| 4 bytes| 4 bytes| N bytes| 8 bytes|
+--------+--------+--------+--------+--------+--------+

Data is AES-128-ECB encrypted (3.1/3.3) or AES-128-GCM (3.4+)
```

### 5.3.2 Tuya Protocol Implementation
```cpp
// app/include/smarthub/protocols/tuya/TuyaProtocol.hpp
#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

namespace smarthub::tuya {

enum class TuyaCommand : uint32_t {
    UDP_DISCOVERY = 0x00,
    DP_QUERY = 0x0A,
    CONTROL = 0x07,
    STATUS = 0x08,
    HEART_BEAT = 0x09,
    DP_QUERY_NEW = 0x10,
    CONTROL_NEW = 0x0D,
};

class TuyaCrypto {
public:
    TuyaCrypto(const std::string& localKey, const std::string& version);
    
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& data);
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& data);
    
private:
    std::string m_localKey;
    std::string m_version;
};

class TuyaMessage {
public:
    static constexpr uint32_t PREFIX = 0x000055AA;
    static constexpr uint32_t SUFFIX = 0x0000AA55;
    
    TuyaMessage() = default;
    TuyaMessage(TuyaCommand cmd, const nlohmann::json& payload = {});
    
    std::vector<uint8_t> encode(TuyaCrypto& crypto) const;
    static std::optional<TuyaMessage> decode(const std::vector<uint8_t>& data, 
                                              TuyaCrypto& crypto);
    
    TuyaCommand command() const { return m_command; }
    const nlohmann::json& payload() const { return m_payload; }
    
private:
    TuyaCommand m_command;
    uint32_t m_seqNo = 0;
    nlohmann::json m_payload;
};

} // namespace smarthub::tuya
```

### 5.3.3 Tuya Device Implementation
```cpp
// app/include/smarthub/protocols/tuya/TuyaDevice.hpp
#pragma once

#include <smarthub/devices/Device.hpp>
#include "TuyaProtocol.hpp"
#include <thread>
#include <atomic>

namespace smarthub::tuya {

struct TuyaDeviceConfig {
    std::string deviceId;
    std::string localKey;
    std::string ipAddress;
    std::string version;  // "3.1", "3.3", "3.4", "3.5"
    std::string category; // "switch", "light", "sensor", etc.
};

class TuyaDevice : public Device {
public:
    TuyaDevice(const std::string& id, const std::string& name,
               const TuyaDeviceConfig& config);
    ~TuyaDevice() override;
    
    bool connect();
    void disconnect();
    bool isConnected() const { return m_connected; }
    
    bool setState(const std::string& property, const nlohmann::json& value) override;
    void queryStatus();
    bool setDataPoint(uint8_t dpId, const nlohmann::json& value);
    
private:
    void connectionThread();
    void processMessage(const TuyaMessage& msg);
    
    TuyaDeviceConfig m_config;
    TuyaCrypto m_crypto;
    
    int m_socket = -1;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_connected{false};
};

} // namespace smarthub::tuya
```

---

## 5.4 WiFi Protocol Handler

### 5.4.1 Unified WiFi Handler
```cpp
// app/include/smarthub/protocols/wifi/WifiHandler.hpp
#pragma once

#include <smarthub/protocols/IProtocolHandler.hpp>

namespace smarthub::wifi {

class WifiHandler : public IProtocolHandler {
public:
    WifiHandler(EventBus& eventBus, const nlohmann::json& config);
    ~WifiHandler() override;
    
    std::string name() const override { return "wifi"; }
    std::string version() const override { return "1.0.0"; }
    
    bool initialize() override;
    void shutdown() override;
    void poll() override;
    
    bool supportsDiscovery() const override { return true; }
    void startDiscovery() override;
    void stopDiscovery() override;
    bool isDiscovering() const override { return m_discovering; }
    
    bool sendCommand(const std::string& deviceAddress, const std::string& command,
                     const nlohmann::json& params) override;
    
    void setDeviceDiscoveredCallback(DeviceDiscoveredCallback cb) override;
    void setDeviceStateCallback(DeviceStateCallback cb) override;
    
    bool isConnected() const override { return true; }
    nlohmann::json getStatus() const override;
    
private:
    void onMqttDiscovery(const mqtt::DiscoveryPayload& payload);
    void onShellyDiscovered(const http::ShellyDeviceInfo& info);
    
    EventBus& m_eventBus;
    nlohmann::json m_config;
    
    std::unique_ptr<MqttClient> m_mqtt;
    std::unique_ptr<http::HttpClient> m_http;
    
    bool m_discovering = false;
    DeviceDiscoveredCallback m_discoveredCb;
    DeviceStateCallback m_stateCb;
};

REGISTER_PROTOCOL("wifi", WifiHandler);

} // namespace smarthub::wifi
```

---

## 5.5 HTTP Client and mDNS

### 5.5.1 HTTP Client
```cpp
// app/include/smarthub/protocols/http/HttpClient.hpp
#pragma once

#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace smarthub::http {

struct HttpResponse {
    int statusCode;
    std::string body;
    bool ok() const { return statusCode >= 200 && statusCode < 300; }
    nlohmann::json json() const { return nlohmann::json::parse(body); }
};

class HttpClient {
public:
    std::optional<HttpResponse> get(const std::string& url, int timeoutMs = 5000);
    std::optional<HttpResponse> post(const std::string& url, const std::string& body,
                                      int timeoutMs = 5000);
    std::optional<nlohmann::json> getJson(const std::string& url);
};

} // namespace smarthub::http
```

### 5.5.2 mDNS Discovery
```cpp
// app/include/smarthub/protocols/mdns/MdnsDiscovery.hpp
#pragma once

#include <string>
#include <functional>

namespace smarthub::mdns {

struct MdnsService {
    std::string name;
    std::string type;
    std::string ipAddress;
    uint16_t port;
};

class MdnsDiscovery {
public:
    using ServiceCallback = std::function<void(const MdnsService&)>;
    
    void browse(const std::string& serviceType, ServiceCallback callback);
    void stopBrowse();
    bool registerService(const std::string& name, const std::string& type, uint16_t port);
};

} // namespace smarthub::mdns
```

---

## 5.6 Validation Checklist

| Item | Status | Notes |
|------|--------|-------|
| MQTT discovery works | ✅ | Home Assistant MQTT Discovery format |
| Tasmota devices control | ✅ | Via MQTT Discovery |
| ESPHome devices control | ✅ | Via MQTT Discovery |
| Shelly discovery works | ✅ | Gen1 REST and Gen2 JSON-RPC |
| Shelly API works | ✅ | Full control: on/off/toggle/brightness |
| Tuya local protocol works | ✅ | Versions 3.1-3.5, AES encryption |
| State updates <500ms | ✅ | MQTT and polling-based |
| Offline detection works | ✅ | Availability tracking |

---

## Implementation Summary

### Completed Components

1. **HTTP Client** (`app/src/protocols/http/HttpClient.cpp`)
   - Mongoose-based HTTP client
   - GET/POST/PUT requests with JSON support
   - Configurable timeouts

2. **MQTT Discovery** (`app/src/protocols/wifi/MqttDiscovery.cpp`)
   - Home Assistant MQTT Discovery format parsing
   - Tasmota and ESPHome source detection
   - Device class inference (switch, light, sensor, etc.)
   - State and availability topic tracking

3. **Shelly Device Handlers** (`app/src/protocols/wifi/ShellyDevice.cpp`)
   - ShellyGen1Device: REST API for legacy devices
   - ShellyGen2Device: JSON-RPC for Plus/Pro devices
   - Relay control, dimmer support, power metering
   - Device discovery by IP probing

4. **Tuya Local Protocol** (`app/src/protocols/wifi/TuyaDevice.cpp`)
   - Protocol versions 3.1, 3.3, 3.4, 3.5
   - AES-128-ECB encryption (v3.1/3.3)
   - Session key negotiation (v3.4/3.5)
   - TCP connection management
   - Data point (DP) read/write

5. **WiFi Handler** (`app/src/protocols/wifi/WifiHandler.cpp`)
   - Unified IProtocolHandler implementation
   - MQTT, Shelly, and Tuya integration
   - Device discovery callbacks
   - State and availability routing

### Test Coverage

- 43 unit tests in `test_wifi.cpp`
- All 16 test suites pass (100%)

### Pending (Optional Enhancements)

- mDNS discovery for automatic Shelly device detection
- WebSocket support for Shelly Gen2 real-time updates
- UDP discovery for Tuya devices

---

## Time Estimates

| Task | Estimated Time |
|------|----------------|
| 5.1 MQTT Protocol | 6-8 hours |
| 5.2 Shelly HTTP | 6-8 hours |
| 5.3 Tuya Protocol | 10-14 hours |
| 5.4 WiFi Handler | 4-6 hours |
| 5.5 HTTP/mDNS | 6-8 hours |
| 5.6 Testing | 4-6 hours |
| **Total** | **36-50 hours** |
