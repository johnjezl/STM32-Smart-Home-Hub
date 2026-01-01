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
- ✅ Purchased Zigbee 3.0 USB Dongle (Amazon B0FCYLX8FT)
  - TI CC2652P + CP2102N (USB-serial)
  - +20dBm TX power with external antenna
  - Pre-loaded with Z-Stack 3.x.0 ZNP firmware
  - See `docs/cc2652p-integration.md` for full specs

### 4.1.2 Flash Coordinator Firmware (if needed)
The USB dongle comes pre-flashed with Z-Stack 3.x.0 ZNP firmware.

If firmware update is needed:
```bash
# Install cc2538-bsl flasher
pip3 install pyserial intelhex
git clone https://github.com/JelmerT/cc2538-bsl.git

# Download latest firmware from:
# https://github.com/Koenkk/Z-Stack-firmware/tree/master/coordinator/Z-Stack_3.x.0/bin

# Put device in bootloader mode (hold BOOT button while plugging in)
python3 cc2538-bsl.py -p /dev/ttyUSB0 -evw CC2652P_coordinator_YYYYMMDD.hex
```

### 4.1.3 Connect to STM32MP157F-DK2

**USB Dongle Approach (Current Hardware):**
- ☐ Enable cp210x kernel driver (CONFIG_USB_SERIAL_CP210X=m)
  - Config fragment: `buildroot/board/smarthub/linux-usb-serial.config`
- ☐ Plug USB dongle into STM32MP157F-DK2 USB-A port
- ☐ Verify device appears as `/dev/ttyUSB0`

**Alternative: Bare CC2652P Module via UART:**
If using a bare module instead of USB dongle:
```
CC2652P      STM32MP1 (GPIO Header)
-------      ----------------------
VCC    -->   3.3V
GND    -->   GND
TX     -->   UART7_RX (PA8)
RX     -->   UART7_TX (PA15)
```
Enable UART7 in device tree if using this approach.

### 4.1.4 Verify Serial Communication
```bash
# Check USB detection
dmesg | grep cp210x
# Expected: "cp210x converter now attached to ttyUSB0"

# List device
ls -la /dev/ttyUSB0

# Test communication (send SYS_PING)
# SmartHub app will do this automatically on startup
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

### 4.7.2 Hardware Integration Tests ✅ COMPLETE
- ✅ Coordinator initialization with real hardware
- ✅ Device pairing via permit join
- ✅ Device state reading
- ✅ Command sending (on/off/toggle)
- ✅ Attribute reporting (sensor data)
- ☐ Device removal (not tested)
- ✅ Network persistence across restarts

### 4.7.3 Test Devices
**Tested:**
- ✅ ThirdReality Smart Outlet (On/Off cluster)
- ✅ ThirdReality Tilt Sensor (IAS Zone cluster)

**Not Yet Tested:**
- ☐ IKEA TRADFRI bulb (on/off, dimming, color temp)
- ☐ Aqara temperature/humidity sensor
- ☐ Sonoff SNZB-02 temperature sensor

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
| CC2652P flashed with Z-Stack | ✅ | Pre-flashed USB dongle |
| Kernel cp210x driver enabled | ✅ | linux-usb-serial.config |
| USB serial communication | ✅ | Verified with ZNP test |
| Coordinator starts network | ✅ | Network forms successfully |
| Permit join enables pairing | ✅ | Devices pair within seconds |
| Light bulb pairs and controls | ✅ | ThirdReality outlet tested |
| Sensor pairs and reports | ✅ | ThirdReality tilt sensor (IAS Zone) |
| State persists across restarts | ✅ | Devices respond after coordinator restart |
| UI shows device status | ⏸️ | Deferred to Phase 7 |
| Command latency <200ms | ✅ | Avg 13ms, Max 37ms |

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
