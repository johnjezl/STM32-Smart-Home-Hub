# Phase 4: Zigbee Integration - Detailed TODO

## Overview
**Duration**: 2-3 weeks  
**Objective**: Enable control and monitoring of Zigbee devices via CC2652P coordinator.  
**Prerequisites**: Phase 3 complete (device framework)

---

## 4.1 Hardware Setup

### 4.1.1 Acquire CC2652P Module
- [ ] Purchase Zigbee coordinator module:
  - **Recommended**: SONOFF Zigbee 3.0 USB Dongle Plus (CC2652P)
  - **Alternative**: Electrolama zig-a-zig-ah! (zzh!)
  - **Alternative**: cod.m CC2652P2 module

### 4.1.2 Flash Coordinator Firmware
```bash
# Install cc2538-bsl flasher
pip3 install pyserial intelhex

git clone https://github.com/JelmerT/cc2538-bsl.git
cd cc2538-bsl

# Download Z-Stack coordinator firmware
# Get from: https://github.com/Koenkk/Z-Stack-firmware/tree/master/coordinator/Z-Stack_3.x.0/bin
wget https://github.com/Koenkk/Z-Stack-firmware/raw/master/coordinator/Z-Stack_3.x.0/bin/CC2652P_coordinator_20230507.zip
unzip CC2652P_coordinator_20230507.zip

# Put device in bootloader mode (hold boot button while plugging in)
# Flash firmware
python3 cc2538-bsl.py -p /dev/ttyUSB0 -evw CC2652P_coordinator_20230507.hex
```

### 4.1.3 Connect to STM32MP157F-DK2
- [ ] Identify available UART on GPIO header
- [ ] Connection (3.3V logic levels!):
  ```
  CC2652P      STM32MP1 (GPIO Header)
  -------      ----------------------
  VCC    -->   3.3V
  GND    -->   GND
  TX     -->   UART7_RX (PA8, pin XX)
  RX     -->   UART7_TX (PA15, pin XX)
  ```
- [ ] Enable UART in device tree if needed
- [ ] Verify serial communication:
```bash
# On the board
stty -F /dev/ttySTM1 115200 raw -echo
cat /dev/ttySTM1 &
echo -ne '\xfe\x00\x00\x00\xff' > /dev/ttySTM1  # Z-Stack reset command
```

---

## 4.2 Z-Stack ZNP Protocol Implementation

### 4.2.1 ZNP Protocol Overview
```
Z-Stack ZNP (Zigbee Network Processor) Protocol:

Frame Format:
+------+--------+------+--------+---------+-----+
| SOF  | Length | Cmd0 | Cmd1   | Payload | FCS |
| 0xFE | 1 byte | 1    | 1      | N bytes | 1   |
+------+--------+------+--------+---------+-----+

Cmd0: Command Type
  - 0x20-0x2F: SREQ (Synchronous Request)
  - 0x40-0x4F: AREQ (Asynchronous Request)  
  - 0x60-0x6F: SRSP (Synchronous Response)

Cmd1: Command ID (subsystem + command)
  - SYS: 0x01
  - AF: 0x04
  - ZDO: 0x05
  - SAPI: 0x06
  - UTIL: 0x07
  - APP_CNF: 0x0F
```

### 4.2.2 ZNP Frame Classes
```cpp
// app/include/smarthub/protocols/zigbee/ZnpFrame.hpp
#pragma once

#include <cstdint>
#include <vector>
#include <optional>

namespace smarthub::zigbee {

// Command types
enum class ZnpType : uint8_t {
    POLL = 0x00,
    SREQ = 0x20,
    AREQ = 0x40,
    SRSP = 0x60
};

// Subsystems
enum class ZnpSubsystem : uint8_t {
    RPC_ERROR = 0x00,
    SYS = 0x01,
    MAC = 0x02,
    NWK = 0x03,
    AF = 0x04,
    ZDO = 0x05,
    SAPI = 0x06,
    UTIL = 0x07,
    DEBUG = 0x08,
    APP = 0x09,
    APP_CNF = 0x0F,
    GREENPOWER = 0x15
};

// Common commands
namespace cmd {
    // SYS commands
    constexpr uint8_t SYS_RESET_REQ = 0x00;
    constexpr uint8_t SYS_PING = 0x01;
    constexpr uint8_t SYS_VERSION = 0x02;
    constexpr uint8_t SYS_OSAL_NV_READ = 0x08;
    constexpr uint8_t SYS_OSAL_NV_WRITE = 0x09;
    
    // AF commands
    constexpr uint8_t AF_REGISTER = 0x00;
    constexpr uint8_t AF_DATA_REQUEST = 0x01;
    constexpr uint8_t AF_INCOMING_MSG = 0x81;
    
    // ZDO commands
    constexpr uint8_t ZDO_STARTUP_FROM_APP = 0x40;
    constexpr uint8_t ZDO_STATE_CHANGE_IND = 0xC0;
    constexpr uint8_t ZDO_END_DEVICE_ANNCE_IND = 0xC1;
    constexpr uint8_t ZDO_LEAVE_IND = 0xC9;
    constexpr uint8_t ZDO_PERMIT_JOIN_IND = 0xCB;
    
    // UTIL commands
    constexpr uint8_t UTIL_GET_DEVICE_INFO = 0x00;
    constexpr uint8_t UTIL_LED_CONTROL = 0x0E;
    
    // APP_CNF commands
    constexpr uint8_t APP_CNF_BDB_START_COMMISSIONING = 0x05;
}

class ZnpFrame {
public:
    static constexpr uint8_t SOF = 0xFE;
    
    ZnpFrame() = default;
    ZnpFrame(ZnpType type, ZnpSubsystem subsystem, uint8_t command);
    ZnpFrame(ZnpType type, ZnpSubsystem subsystem, uint8_t command, 
             const std::vector<uint8_t>& payload);
    
    // Build frame
    ZnpFrame& setPayload(const std::vector<uint8_t>& payload);
    ZnpFrame& appendByte(uint8_t b);
    ZnpFrame& appendWord(uint16_t w);  // Little-endian
    ZnpFrame& appendDWord(uint32_t d);
    ZnpFrame& appendBytes(const uint8_t* data, size_t len);
    
    // Serialize to bytes
    std::vector<uint8_t> serialize() const;
    
    // Parse from bytes
    static std::optional<ZnpFrame> parse(const uint8_t* data, size_t len);
    
    // Accessors
    ZnpType type() const { return m_type; }
    ZnpSubsystem subsystem() const { return m_subsystem; }
    uint8_t command() const { return m_command; }
    const std::vector<uint8_t>& payload() const { return m_payload; }
    
    // Helpers
    uint8_t cmd0() const;
    uint8_t cmd1() const;
    bool isResponse() const { return m_type == ZnpType::SRSP; }
    bool isIndication() const { return m_type == ZnpType::AREQ; }
    
private:
    static uint8_t calculateFCS(const uint8_t* data, size_t len);
    
    ZnpType m_type = ZnpType::SREQ;
    ZnpSubsystem m_subsystem = ZnpSubsystem::SYS;
    uint8_t m_command = 0;
    std::vector<uint8_t> m_payload;
};

} // namespace smarthub::zigbee
```

### 4.2.3 ZNP Serial Transport
```cpp
// app/include/smarthub/protocols/zigbee/ZnpTransport.hpp
#pragma once

#include "ZnpFrame.hpp"
#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

namespace smarthub::zigbee {

using FrameCallback = std::function<void(const ZnpFrame&)>;

class ZnpTransport {
public:
    explicit ZnpTransport(const std::string& port, int baud = 115200);
    ~ZnpTransport();
    
    bool open();
    void close();
    bool isOpen() const { return m_fd >= 0; }
    
    // Send frame and wait for response (synchronous)
    std::optional<ZnpFrame> request(const ZnpFrame& frame, int timeoutMs = 5000);
    
    // Send frame without waiting (asynchronous)
    bool send(const ZnpFrame& frame);
    
    // Set callback for async indications (AREQ)
    void setIndicationCallback(FrameCallback callback);
    
private:
    void readerThread();
    void processReceivedData();
    
    std::string m_port;
    int m_baud;
    int m_fd = -1;
    
    std::thread m_readerThread;
    std::atomic<bool> m_running{false};
    
    // For synchronous request/response
    std::mutex m_responseMutex;
    std::condition_variable m_responseCv;
    std::optional<ZnpFrame> m_pendingResponse;
    uint8_t m_expectedCmd0 = 0;
    uint8_t m_expectedCmd1 = 0;
    
    // Receive buffer
    std::vector<uint8_t> m_rxBuffer;
    std::mutex m_rxMutex;
    
    FrameCallback m_indicationCallback;
};

} // namespace smarthub::zigbee
```

---

## 4.3 Zigbee Coordinator Controller

### 4.3.1 Coordinator Class
```cpp
// app/include/smarthub/protocols/zigbee/ZigbeeCoordinator.hpp
#pragma once

#include "ZnpTransport.hpp"
#include <map>
#include <functional>

namespace smarthub::zigbee {

struct DeviceInfo {
    uint16_t networkAddress;
    uint64_t ieeeAddress;
    uint8_t deviceType;  // 0=Coordinator, 1=Router, 2=EndDevice
    std::string manufacturer;
    std::string model;
    uint8_t endpoints[32];
    uint8_t endpointCount;
};

struct ClusterAttribute {
    uint16_t id;
    uint8_t dataType;
    std::vector<uint8_t> value;
};

class ZigbeeCoordinator {
public:
    using DeviceJoinedCallback = std::function<void(const DeviceInfo&)>;
    using DeviceLeftCallback = std::function<void(uint64_t ieeeAddr)>;
    using AttributeReportCallback = std::function<void(uint16_t nwkAddr, uint8_t endpoint,
                                                        uint16_t cluster, const ClusterAttribute&)>;
    
    explicit ZigbeeCoordinator(const std::string& port);
    ~ZigbeeCoordinator();
    
    // Initialization
    bool initialize();
    void shutdown();
    
    // Network operations
    bool startNetwork();
    bool permitJoin(uint8_t duration = 60);  // 0=disable, 255=permanent
    bool getNetworkInfo();
    
    // Device operations
    bool readAttribute(uint16_t nwkAddr, uint8_t endpoint, uint16_t cluster, uint16_t attrId);
    bool writeAttribute(uint16_t nwkAddr, uint8_t endpoint, uint16_t cluster,
                        uint16_t attrId, uint8_t dataType, const std::vector<uint8_t>& value);
    bool sendCommand(uint16_t nwkAddr, uint8_t endpoint, uint16_t cluster,
                     uint8_t command, const std::vector<uint8_t>& payload = {});
    
    // Callbacks
    void setDeviceJoinedCallback(DeviceJoinedCallback cb) { m_deviceJoinedCb = cb; }
    void setDeviceLeftCallback(DeviceLeftCallback cb) { m_deviceLeftCb = cb; }
    void setAttributeReportCallback(AttributeReportCallback cb) { m_attrReportCb = cb; }
    
    // Status
    bool isNetworkUp() const { return m_networkUp; }
    uint16_t panId() const { return m_panId; }
    uint8_t channel() const { return m_channel; }
    uint64_t ieeeAddress() const { return m_ieeeAddr; }
    
private:
    void handleIndication(const ZnpFrame& frame);
    void handleDeviceAnnounce(const ZnpFrame& frame);
    void handleIncomingMessage(const ZnpFrame& frame);
    void handleStateChange(const ZnpFrame& frame);
    
    void requestDeviceInfo(uint16_t nwkAddr);
    void configureReporting(uint16_t nwkAddr, uint8_t endpoint, uint16_t cluster);
    
    ZnpTransport m_transport;
    
    bool m_networkUp = false;
    uint16_t m_panId = 0;
    uint8_t m_channel = 0;
    uint64_t m_ieeeAddr = 0;
    
    std::map<uint64_t, DeviceInfo> m_devices;
    
    DeviceJoinedCallback m_deviceJoinedCb;
    DeviceLeftCallback m_deviceLeftCb;
    AttributeReportCallback m_attrReportCb;
};

} // namespace smarthub::zigbee
```

---

## 4.4 Zigbee Cluster Library (ZCL) Support

### 4.4.1 ZCL Constants
```cpp
// app/include/smarthub/protocols/zigbee/ZclConstants.hpp
#pragma once

#include <cstdint>

namespace smarthub::zigbee::zcl {

// Cluster IDs
namespace cluster {
    constexpr uint16_t BASIC = 0x0000;
    constexpr uint16_t POWER_CONFIG = 0x0001;
    constexpr uint16_t IDENTIFY = 0x0003;
    constexpr uint16_t GROUPS = 0x0004;
    constexpr uint16_t SCENES = 0x0005;
    constexpr uint16_t ON_OFF = 0x0006;
    constexpr uint16_t LEVEL_CONTROL = 0x0008;
    constexpr uint16_t COLOR_CONTROL = 0x0300;
    constexpr uint16_t ILLUMINANCE = 0x0400;
    constexpr uint16_t TEMPERATURE = 0x0402;
    constexpr uint16_t HUMIDITY = 0x0405;
    constexpr uint16_t OCCUPANCY = 0x0406;
    constexpr uint16_t IAS_ZONE = 0x0500;
    constexpr uint16_t ELECTRICAL = 0x0B04;
    constexpr uint16_t METERING = 0x0702;
}

// Basic cluster attributes
namespace attr::basic {
    constexpr uint16_t ZCL_VERSION = 0x0000;
    constexpr uint16_t APP_VERSION = 0x0001;
    constexpr uint16_t MANUFACTURER = 0x0004;
    constexpr uint16_t MODEL = 0x0005;
    constexpr uint16_t SW_BUILD = 0x4000;
}

// Power configuration attributes
namespace attr::power {
    constexpr uint16_t BATTERY_VOLTAGE = 0x0020;
    constexpr uint16_t BATTERY_PERCENT = 0x0021;
}

// On/Off cluster attributes
namespace attr::onoff {
    constexpr uint16_t ON_OFF = 0x0000;
}

// Level control attributes
namespace attr::level {
    constexpr uint16_t CURRENT_LEVEL = 0x0000;
}

// Color control attributes
namespace attr::color {
    constexpr uint16_t CURRENT_HUE = 0x0000;
    constexpr uint16_t CURRENT_SAT = 0x0001;
    constexpr uint16_t COLOR_TEMP = 0x0007;
    constexpr uint16_t COLOR_MODE = 0x0008;
}

// Temperature measurement
namespace attr::temperature {
    constexpr uint16_t MEASURED_VALUE = 0x0000;  // In 0.01°C
}

// Humidity measurement
namespace attr::humidity {
    constexpr uint16_t MEASURED_VALUE = 0x0000;  // In 0.01%
}

// On/Off commands
namespace cmd::onoff {
    constexpr uint8_t OFF = 0x00;
    constexpr uint8_t ON = 0x01;
    constexpr uint8_t TOGGLE = 0x02;
}

// Level control commands
namespace cmd::level {
    constexpr uint8_t MOVE_TO_LEVEL = 0x00;
    constexpr uint8_t MOVE = 0x01;
    constexpr uint8_t STEP = 0x02;
    constexpr uint8_t STOP = 0x03;
    constexpr uint8_t MOVE_TO_LEVEL_WITH_ONOFF = 0x04;
}

// Color control commands
namespace cmd::color {
    constexpr uint8_t MOVE_TO_HUE = 0x00;
    constexpr uint8_t MOVE_TO_SAT = 0x03;
    constexpr uint8_t MOVE_TO_HUE_SAT = 0x06;
    constexpr uint8_t MOVE_TO_COLOR_TEMP = 0x0A;
}

// Data types
namespace datatype {
    constexpr uint8_t BOOLEAN = 0x10;
    constexpr uint8_t UINT8 = 0x20;
    constexpr uint8_t UINT16 = 0x21;
    constexpr uint8_t UINT32 = 0x23;
    constexpr uint8_t INT8 = 0x28;
    constexpr uint8_t INT16 = 0x29;
    constexpr uint8_t ENUM8 = 0x30;
    constexpr uint8_t STRING = 0x42;
    constexpr uint8_t IEEE_ADDR = 0xF0;
}

} // namespace smarthub::zigbee::zcl
```

---

## 4.5 Zigbee Protocol Handler

### 4.5.1 Protocol Handler Implementation
```cpp
// app/include/smarthub/protocols/zigbee/ZigbeeHandler.hpp
#pragma once

#include <smarthub/protocols/IProtocolHandler.hpp>
#include "ZigbeeCoordinator.hpp"
#include "ZigbeeDeviceDatabase.hpp"

namespace smarthub::zigbee {

class ZigbeeHandler : public IProtocolHandler {
public:
    ZigbeeHandler(EventBus& eventBus, const nlohmann::json& config);
    ~ZigbeeHandler() override;
    
    // IProtocolHandler interface
    std::string name() const override { return "zigbee"; }
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
    
    void setDeviceDiscoveredCallback(DeviceDiscoveredCallback cb) override { m_discoveredCb = cb; }
    void setDeviceStateCallback(DeviceStateCallback cb) override { m_stateCb = cb; }
    
    bool isConnected() const override { return m_coordinator && m_coordinator->isNetworkUp(); }
    nlohmann::json getStatus() const override;
    
private:
    void onDeviceJoined(const DeviceInfo& info);
    void onDeviceLeft(uint64_t ieeeAddr);
    void onAttributeReport(uint16_t nwkAddr, uint8_t endpoint, uint16_t cluster,
                           const ClusterAttribute& attr);
    
    DevicePtr createDevice(const DeviceInfo& info);
    void processClusterAttribute(const std::string& deviceId, uint16_t cluster,
                                  const ClusterAttribute& attr);
    
    // Command handlers by device type
    bool handleSwitchCommand(const DeviceInfo& info, const std::string& cmd,
                              const nlohmann::json& params);
    bool handleDimmerCommand(const DeviceInfo& info, const std::string& cmd,
                              const nlohmann::json& params);
    bool handleColorLightCommand(const DeviceInfo& info, const std::string& cmd,
                                  const nlohmann::json& params);
    
    EventBus& m_eventBus;
    nlohmann::json m_config;
    
    std::unique_ptr<ZigbeeCoordinator> m_coordinator;
    ZigbeeDeviceDatabase m_deviceDb;
    
    bool m_discovering = false;
    
    DeviceDiscoveredCallback m_discoveredCb;
    DeviceStateCallback m_stateCb;
    
    // Map IEEE address to device ID
    std::map<uint64_t, std::string> m_ieeeToDeviceId;
};

// Auto-register with factory
REGISTER_PROTOCOL("zigbee", ZigbeeHandler);

} // namespace smarthub::zigbee
```

### 4.5.2 Device Database
```cpp
// app/include/smarthub/protocols/zigbee/ZigbeeDeviceDatabase.hpp
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace smarthub::zigbee {

struct DeviceDefinition {
    std::string manufacturer;
    std::string model;
    std::string description;
    DeviceType deviceType;
    std::vector<DeviceCapability> capabilities;
    nlohmann::json quirks;  // Device-specific behavior adjustments
};

class ZigbeeDeviceDatabase {
public:
    ZigbeeDeviceDatabase();
    
    // Load definitions from JSON file
    bool loadFromFile(const std::string& path);
    
    // Lookup device
    std::optional<DeviceDefinition> findDevice(const std::string& manufacturer,
                                                const std::string& model) const;
    
    // Get all known manufacturers
    std::vector<std::string> manufacturers() const;
    
private:
    std::vector<DeviceDefinition> m_definitions;
};

} // namespace smarthub::zigbee
```

### 4.5.3 Device Definition Example (JSON)
```json
// app/assets/zigbee_devices.json
{
  "devices": [
    {
      "manufacturer": "IKEA of Sweden",
      "models": ["TRADFRI bulb E26 WS opal 980lm", "TRADFRI bulb E27 WS opal 1000lm"],
      "description": "IKEA TRADFRI LED bulb (white spectrum)",
      "type": "ColorLight",
      "capabilities": ["OnOff", "Brightness", "ColorTemperature"],
      "quirks": {
        "colorTempRange": [250, 454]
      }
    },
    {
      "manufacturer": "LUMI",
      "models": ["lumi.sensor_ht", "lumi.weather"],
      "description": "Aqara Temperature & Humidity Sensor",
      "type": "TemperatureSensor",
      "capabilities": ["Temperature", "Humidity", "Battery"],
      "quirks": {
        "requiresRejoin": true
      }
    },
    {
      "manufacturer": "LUMI",
      "models": ["lumi.sensor_motion.aq2"],
      "description": "Aqara Motion Sensor",
      "type": "MotionSensor",
      "capabilities": ["Motion", "Illuminance", "Battery"],
      "quirks": {
        "occupancyTimeout": 90
      }
    },
    {
      "manufacturer": "Philips",
      "models": ["LWB010", "LWB014"],
      "description": "Philips Hue White",
      "type": "Dimmer",
      "capabilities": ["OnOff", "Brightness"]
    },
    {
      "manufacturer": "_TZ3000_",
      "models": ["TS0001", "TS0011"],
      "description": "Tuya Smart Switch",
      "type": "Switch",
      "capabilities": ["OnOff"],
      "quirks": {
        "tuyaSpecific": true
      }
    }
  ]
}
```

---

## 4.6 Pairing Mode UI

### 4.6.1 Pairing Screen
```cpp
// app/include/smarthub/ui/screens/PairingScreen.hpp
#pragma once

#include <smarthub/ui/Screen.hpp>
#include <lvgl.h>

namespace smarthub::ui {

class PairingScreen : public Screen {
public:
    PairingScreen(UIManager& uiManager);
    ~PairingScreen() override;
    
    void onCreate() override;
    void onShow() override;
    void onHide() override;
    
    // Update discovered device list
    void addDiscoveredDevice(const std::string& name, const std::string& type);
    void clearDevices();
    
private:
    void startPairing();
    void stopPairing();
    void onDeviceSelected(int index);
    
    lv_obj_t* m_container = nullptr;
    lv_obj_t* m_titleLabel = nullptr;
    lv_obj_t* m_statusLabel = nullptr;
    lv_obj_t* m_spinner = nullptr;
    lv_obj_t* m_deviceList = nullptr;
    lv_obj_t* m_startButton = nullptr;
    lv_obj_t* m_cancelButton = nullptr;
    
    bool m_pairing = false;
    std::vector<std::pair<std::string, std::string>> m_discoveredDevices;
};

} // namespace smarthub::ui
```

---

## 4.7 Testing

### 4.7.1 Test Devices
- [ ] IKEA TRADFRI bulb (on/off, dimming, color temp)
- [ ] Aqara temperature/humidity sensor
- [ ] Aqara door/window sensor
- [ ] Sonoff SNZB-02 temperature sensor
- [ ] Smart plug/outlet

### 4.7.2 Test Scenarios
- [ ] Coordinator initialization
- [ ] Device pairing via permit join
- [ ] Device state reading
- [ ] Command sending (on/off/dim)
- [ ] Attribute reporting (sensor data)
- [ ] Device removal
- [ ] Network persistence across restarts
- [ ] Multiple device mesh network

---

## 4.8 Validation Checklist

| Item | Status | Notes |
|------|--------|-------|
| CC2652P flashed with Z-Stack | ☐ | |
| UART communication verified | ☐ | |
| ZNP frame parsing works | ☐ | |
| Coordinator starts network | ☐ | |
| Permit join enables pairing | ☐ | |
| Light bulb pairs and controls | ☐ | |
| Sensor pairs and reports | ☐ | |
| State persists across restarts | ☐ | |
| UI shows device status | ☐ | |
| Command latency <200ms | ☐ | |

---

## Time Estimates

| Task | Estimated Time |
|------|----------------|
| 4.1 Hardware Setup | 2-4 hours |
| 4.2 ZNP Protocol | 8-12 hours |
| 4.3 Coordinator Controller | 8-10 hours |
| 4.4 ZCL Support | 6-8 hours |
| 4.5 Protocol Handler | 8-10 hours |
| 4.6 Pairing UI | 4-6 hours |
| 4.7-4.8 Testing & Validation | 6-8 hours |
| **Total** | **42-58 hours** |
