# Phase 4: Zigbee Integration - Detailed TODO

## Overview
**Duration**: 2-3 weeks
**Objective**: Enable control and monitoring of Zigbee devices via CC2652P coordinator.
**Prerequisites**: Phase 3 complete (device framework)

---

## Software Implementation Status

### Completed (Without Hardware)

| Component | Status | Files |
|-----------|--------|-------|
| ZNP Frame Parser | ✅ | `include/smarthub/protocols/zigbee/ZnpFrame.hpp`, `src/protocols/zigbee/ZnpFrame.cpp` |
| ZCL Constants | ✅ | `include/smarthub/protocols/zigbee/ZclConstants.hpp` |
| ZNP Transport | ✅ | `include/smarthub/protocols/zigbee/ZnpTransport.hpp`, `src/protocols/zigbee/ZnpTransport.cpp` |
| Zigbee Coordinator | ✅ | `include/smarthub/protocols/zigbee/ZigbeeCoordinator.hpp`, `src/protocols/zigbee/ZigbeeCoordinator.cpp` |
| Zigbee Handler | ✅ | `include/smarthub/protocols/zigbee/ZigbeeHandler.hpp`, `src/protocols/zigbee/ZigbeeHandler.cpp` |
| Device Database | ✅ | `include/smarthub/protocols/zigbee/ZigbeeDeviceDatabase.hpp`, `src/protocols/zigbee/ZigbeeDeviceDatabase.cpp` |
| Comprehensive Tests | ✅ | `tests/protocols/test_zigbee.cpp` (42 test cases) |

---

## 4.1 Hardware Setup

### 4.1.1 Acquire CC2652P Module
- ☐ Purchase Zigbee coordinator module:
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
- ☐ Identify available UART on GPIO header
- ☐ Connection (3.3V logic levels!):
  ```
  CC2652P      STM32MP1 (GPIO Header)
  -------      ----------------------
  VCC    -->   3.3V
  GND    -->   GND
  TX     -->   UART7_RX (PA8, pin XX)
  RX     -->   UART7_TX (PA15, pin XX)
  ```
- ☐ Enable UART in device tree if needed
- ☐ Verify serial communication:
```bash
# On the board
stty -F /dev/ttySTM1 115200 raw -echo
cat /dev/ttySTM1 &
echo -ne '\xfe\x00\x00\x00\xff' > /dev/ttySTM1  # Z-Stack reset command
```

---

## 4.2 Z-Stack ZNP Protocol Implementation ✅

### 4.2.1 ZNP Protocol Overview
```
Z-Stack ZNP (Zigbee Network Processor) Protocol:

Frame Format:
+------+--------+------+--------+---------+-----+
| SOF  | Length | Cmd0 | Cmd1   | Payload | FCS |
| 0xFE | 1 byte | 1    | 1      | N bytes | 1   |
+------+--------+------+--------+---------+-----+

Cmd0: Type (bits 7-5) | Subsystem (bits 4-0)
  - POLL: 0x00
  - SREQ: 0x20 (Synchronous Request)
  - AREQ: 0x40 (Asynchronous Request/Indication)
  - SRSP: 0x60 (Synchronous Response)

Subsystems:
  - SYS: 0x01
  - AF: 0x04
  - ZDO: 0x05
  - UTIL: 0x07
  - APP_CNF: 0x0F
```

### 4.2.2 ZNP Frame Classes ✅
**Files**: `ZnpFrame.hpp`, `ZnpFrame.cpp`

Features implemented:
- Frame construction with type, subsystem, command
- Payload building (appendByte, appendWord, appendDWord, appendQWord)
- Frame serialization with FCS calculation
- Frame parsing with validation
- Frame finding in byte stream
- Debug toString() method

### 4.2.3 ZNP Serial Transport ✅
**Files**: `ZnpTransport.hpp`, `ZnpTransport.cpp`

Features implemented:
- ISerialPort interface for dependency injection (testability)
- PosixSerialPort implementation (8N1, no flow control)
- Reader thread for async frame reception
- Synchronous request/response with timeout
- Async indication callback registration

---

## 4.3 Zigbee Coordinator Controller ✅

**Files**: `ZigbeeCoordinator.hpp`, `ZigbeeCoordinator.cpp`

Features implemented:
- Network initialization and startup
- Permit join control
- Device tracking (network address, IEEE address, endpoints, clusters)
- ZCL attribute reading/writing
- ZCL command sending
- Callbacks for device announce, device leave, attribute reports, commands
- Convenience methods: setOnOff, setLevel, setColorTemp, setHueSat
- Configure reporting for automatic state updates

---

## 4.4 Zigbee Cluster Library (ZCL) Support ✅

**File**: `ZclConstants.hpp`

Clusters supported:
- Basic (0x0000): Manufacturer, model identification
- Power Configuration (0x0001): Battery voltage/percent
- On/Off (0x0006): On, off, toggle commands
- Level Control (0x0008): Brightness control
- Color Control (0x0300): Hue, saturation, color temperature
- Temperature Measurement (0x0402): Temperature readings
- Relative Humidity (0x0405): Humidity readings
- Occupancy Sensing (0x0406): Motion detection
- IAS Zone (0x0500): Security sensors
- Electrical Measurement (0x0B04): Power monitoring
- Metering (0x0702): Energy measurement

---

## 4.5 Zigbee Protocol Handler ✅

**Files**: `ZigbeeHandler.hpp`, `ZigbeeHandler.cpp`

Implements IProtocolHandler interface:
- `initialize()`: Start coordinator and network
- `shutdown()`: Clean shutdown
- `startDiscovery()`: Enable permit join
- `stopDiscovery()`: Disable permit join
- `sendCommand()`: Route commands to devices
- `getStatus()`: Return protocol status JSON
- `getKnownDeviceAddresses()`: List paired devices

Device type inference from clusters:
- On/Off + Level + Color → ColorLight
- On/Off + Level → Dimmer
- On/Off only → Switch
- Temperature → TemperatureSensor
- IAS Zone / Occupancy → MotionSensor

### 4.5.1 Device Database ✅

**Files**: `ZigbeeDeviceDatabase.hpp`, `ZigbeeDeviceDatabase.cpp`

Features:
- Load device definitions from JSON
- Lookup by manufacturer/model (case-insensitive)
- Device type mapping
- Quirks support for device-specific workarounds

---

## 4.6 Pairing Mode UI

### 4.6.1 Pairing Screen
- ⏸️ Deferred to Phase 6 (UI implementation phase)

---

## 4.7 Testing

### 4.7.1 Unit Tests ✅ (42 test cases)

| Test Suite | Tests | Status |
|------------|-------|--------|
| ZnpFrameTest | 9 | ✅ |
| ZnpTransportTest | 4 | ✅ |
| ZclConstantsTest | 4 | ✅ |
| ZigbeeDeviceDatabaseTest | 8 | ✅ |
| ZigbeeCoordinatorTest | 3 | ✅ |
| ZigbeeHandlerTest | 3 | ✅ |
| ZclAttributeValueTest | 6 | ✅ |
| ZigbeeDeviceInfoTest | 2 | ✅ |
| ZigbeeIntegrationTest | 2 | ✅ |
| ZigbeeProtocolTest | 1 | ✅ |

Mock serial port enables testing without hardware:
- `MockSerialPort` class in test file
- Simulates open, close, read, write operations
- Queue-based read data injection

### 4.7.2 Hardware Integration Tests (Pending CC2652P)
- ☐ Coordinator initialization with real hardware
- ☐ Device pairing via permit join
- ☐ Device state reading
- ☐ Command sending (on/off/dim)
- ☐ Attribute reporting (sensor data)
- ☐ Device removal
- ☐ Network persistence across restarts

### 4.7.3 Test Devices (Pending)
- ☐ IKEA TRADFRI bulb (on/off, dimming, color temp)
- ☐ Aqara temperature/humidity sensor
- ☐ Aqara door/window sensor
- ☐ Sonoff SNZB-02 temperature sensor
- ☐ Smart plug/outlet

---

## 4.8 Validation Checklist

| Item | Status | Notes |
|------|--------|-------|
| ZNP frame parsing works | ✅ | 9 tests passing |
| ZNP transport abstraction | ✅ | ISerialPort interface for testability |
| ZCL constants defined | ✅ | All major clusters |
| Zigbee coordinator logic | ✅ | Network and device management |
| IProtocolHandler implemented | ✅ | Full interface compliance |
| Device database works | ✅ | JSON loading, lookup, quirks |
| Comprehensive unit tests | ✅ | 42 test cases |
| CC2652P flashed with Z-Stack | ☐ | Waiting for hardware |
| UART communication verified | ☐ | Waiting for hardware |
| Coordinator starts network | ☐ | Waiting for hardware |
| Permit join enables pairing | ☐ | Waiting for hardware |
| Light bulb pairs and controls | ☐ | Waiting for hardware |
| Sensor pairs and reports | ☐ | Waiting for hardware |
| State persists across restarts | ☐ | Waiting for hardware |
| UI shows device status | ⏸️ | Deferred to Phase 6 |
| Command latency <200ms | ☐ | Waiting for hardware |

---

## Time Estimates

| Task | Estimated | Actual | Status |
|------|-----------|--------|--------|
| 4.2 ZNP Protocol | 8-12 hours | ~6 hours | ✅ COMPLETE |
| 4.3 Coordinator Controller | 8-10 hours | ~6 hours | ✅ COMPLETE |
| 4.4 ZCL Support | 6-8 hours | ~3 hours | ✅ COMPLETE |
| 4.5 Protocol Handler | 8-10 hours | ~4 hours | ✅ COMPLETE |
| 4.7 Unit Testing | 4-6 hours | ~2 hours | ✅ COMPLETE |
| 4.1 Hardware Setup | 2-4 hours | - | ☐ PENDING |
| 4.6 Pairing UI | 4-6 hours | - | ⏸️ DEFERRED |
| 4.7-4.8 Hardware Testing | 6-8 hours | - | ☐ PENDING |
| **Software Total** | **34-46 hours** | **~21 hours** | ✅ COMPLETE |
| **Hardware Total** | **8-12 hours** | - | ☐ PENDING |

---

## Files Created

### Headers (6 files)
- `include/smarthub/protocols/zigbee/ZnpFrame.hpp`
- `include/smarthub/protocols/zigbee/ZclConstants.hpp`
- `include/smarthub/protocols/zigbee/ZnpTransport.hpp`
- `include/smarthub/protocols/zigbee/ZigbeeCoordinator.hpp`
- `include/smarthub/protocols/zigbee/ZigbeeHandler.hpp`
- `include/smarthub/protocols/zigbee/ZigbeeDeviceDatabase.hpp`

### Sources (5 files)
- `src/protocols/zigbee/ZnpFrame.cpp`
- `src/protocols/zigbee/ZnpTransport.cpp`
- `src/protocols/zigbee/ZigbeeCoordinator.cpp`
- `src/protocols/zigbee/ZigbeeHandler.cpp`
- `src/protocols/zigbee/ZigbeeDeviceDatabase.cpp`

### Tests (1 file)
- `tests/protocols/test_zigbee.cpp` (42 test cases)

### Modified
- `app/CMakeLists.txt` - Added Zigbee sources
- `app/tests/CMakeLists.txt` - Added Zigbee tests
