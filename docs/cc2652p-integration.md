# CC2652P Zigbee Coordinator Integration

**Status:** ✅ Hardware integration verified - ZNP communication working

## Hardware

**Device:** Zigbee 3.0 USB Dongle (Amazon B0FCYLX8FT)

| Specification | Value |
|---------------|-------|
| Main Chip | TI CC2652P (Zigbee SoC with PA) |
| USB-Serial | Silicon Labs CP2102N |
| TX Power | +20 dBm |
| Antenna | External SMA, detachable |
| Firmware | Z-Stack 3.x.0 ZNP (preloaded) |
| Device Capacity | Up to 128 devices |
| Baud Rate | 115200 (default) |

## Kernel Requirements

The CP2102N USB-to-serial chip requires the `cp210x` kernel driver.

**Config fragment:** `buildroot/board/smarthub/linux-usb-serial.config`
```
CONFIG_USB_SERIAL_CP210X=m
CONFIG_USB_SERIAL_GENERIC=y
```

After rebuilding the kernel, the device will appear as `/dev/ttyUSB0` when plugged in.

## Software Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    SmartHub Application                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐  │
│  │ DeviceManager│  │  EventBus  │  │  REST API       │  │
│  └──────┬──────┘  └──────┬──────┘  └────────┬────────┘  │
│         │                │                   │          │
│  ┌──────┴────────────────┴───────────────────┴───────┐  │
│  │              ZigbeeHandler (IProtocolHandler)     │  │
│  └───────────────────────┬───────────────────────────┘  │
│                          │                              │
│  ┌───────────────────────┴───────────────────────────┐  │
│  │              ZigbeeCoordinator                    │  │
│  │  - Network management                             │  │
│  │  - Device tracking                                │  │
│  │  - ZCL attribute read/write                       │  │
│  └───────────────────────┬───────────────────────────┘  │
│                          │                              │
│  ┌───────────────────────┴───────────────────────────┐  │
│  │              ZnpTransport                         │  │
│  │  - Serial port I/O                                │  │
│  │  - Frame sync/async communication                 │  │
│  └───────────────────────┬───────────────────────────┘  │
│                          │                              │
│  ┌───────────────────────┴───────────────────────────┐  │
│  │              ZnpFrame                             │  │
│  │  - MT protocol framing                            │  │
│  │  - SOF, length, FCS calculation                   │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
                           │
                           │ /dev/ttyUSB0
                           ▼
                ┌─────────────────────┐
                │   CC2652P Dongle    │
                │   (Z-Stack ZNP)     │
                └─────────────────────┘
```

## Z-Stack ZNP Protocol

### Frame Format (MT Protocol)

```
┌─────┬────────┬──────┬──────┬─────────┬─────┐
│ SOF │ Length │ Cmd0 │ Cmd1 │ Data... │ FCS │
│ FE  │ 1 byte │ 1b   │ 1b   │ 0-250   │ 1b  │
└─────┴────────┴──────┴──────┴─────────┴─────┘
```

- **SOF:** Always 0xFE
- **Length:** Payload length (0-250)
- **Cmd0:** Command type (SREQ=0x20, SRSP=0x60, AREQ=0x40)
- **Cmd1:** Subsystem + command ID
- **FCS:** XOR of Length through Data

### Key Commands

| Subsystem | Cmd0 | Description |
|-----------|------|-------------|
| SYS | 0x21 | System interface |
| AF | 0x24 | Application Framework |
| ZDO | 0x25 | ZigBee Device Object |
| SAPI | 0x26 | Simple API |
| UTIL | 0x27 | Utility |
| APP | 0x29 | Application Config |

### Initialization Sequence

1. `SYS_RESET_REQ` (0x41, 0x00) - Reset coordinator
2. Wait for `SYS_RESET_IND` (0x41, 0x80)
3. `SYS_PING` (0x21, 0x01) - Verify communication
4. `ZB_READ_CONFIGURATION` - Read network params
5. `ZB_START_REQUEST` - Start network

## Integration Steps

### 1. Hardware Setup

```bash
# Plug in CC2652P dongle
# Check detection
dmesg | tail -10
# Should show: cp210x converter now attached to ttyUSB0

# Verify device exists
ls -la /dev/ttyUSB0
```

### 2. Test Serial Communication

```bash
# Send SYS_PING and check response
# Using the smarthub app test mode or minicom
```

### 3. Initialize in SmartHub

```cpp
#include <smarthub/protocols/zigbee/ZigbeeHandler.hpp>

// In Application::initialize()
nlohmann::json zigbeeConfig = {
    {"port", "/dev/ttyUSB0"},
    {"baudRate", 115200}
};

auto zigbee = std::make_unique<ZigbeeHandler>(m_eventBus, zigbeeConfig);
if (zigbee->initialize()) {
    m_protocolHandlers["zigbee"] = std::move(zigbee);
}
```

### 4. Device Pairing

```cpp
// Start pairing mode (permit join for 60 seconds)
auto* handler = dynamic_cast<ZigbeeHandler*>(m_protocolHandlers["zigbee"].get());
handler->startDiscovery();  // Enables permit join

// Subscribe to device discovery events
m_eventBus.subscribe<DeviceDiscoveredEvent>([](const auto& e) {
    LOG_INFO("New Zigbee device: {} ({})", e.name, e.address);
});
```

## Testing Checklist

| Test | Command/Action | Expected Result |
|------|----------------|-----------------|
| USB detection | `dmesg \| grep cp210x` | "converter attached to ttyUSB0" |
| Serial open | SmartHub startup | "ZNP transport opened" |
| SYS_PING | Automatic on init | Response with capabilities |
| Network start | Automatic on init | "Zigbee network started" |
| Permit join | REST API or button | "Permit join enabled" |
| Device pair | Put device in pairing | Device discovered event |
| Device control | REST API command | Device responds |

## Troubleshooting

### Device not detected

```bash
# Check USB
lsusb | grep -i silicon
# Should show: Silicon Labs CP210x

# Check kernel module
lsmod | grep cp210x
# If empty, module not loaded

# Load manually (for testing)
modprobe cp210x
```

### Serial port permission denied

```bash
# Add user to dialout group (on dev machine)
sudo usermod -a -G dialout $USER

# Or on embedded target, device nodes should be world-readable
# Check /etc/mdev.conf for udev rules
```

### No response from coordinator

1. Check baud rate matches (115200)
2. Verify firmware is Z-Stack ZNP (not router firmware)
3. Try power cycling the dongle
4. Check serial port settings (8N1, no flow control)

### Coordinator not starting network

1. May need to perform a factory reset
2. Check if forming new network or joining existing
3. Verify channel configuration (default: 11)

## Files

| File | Description |
|------|-------------|
| `app/include/smarthub/protocols/zigbee/ZnpFrame.hpp` | MT frame handling |
| `app/include/smarthub/protocols/zigbee/ZnpTransport.hpp` | Serial transport |
| `app/include/smarthub/protocols/zigbee/ZigbeeCoordinator.hpp` | Network management |
| `app/include/smarthub/protocols/zigbee/ZigbeeHandler.hpp` | Protocol handler |
| `app/tests/protocols/test_zigbee.cpp` | 42 unit tests |
| `buildroot/board/smarthub/linux-usb-serial.config` | Kernel config |

## References

- [TI Z-Stack ZNP Interface Guide](https://software-dl.ti.com/simplelink/esd/plugins/simplelink_zigbee_sdk_plugin/1.60.01.09/exports/docs/zigbee_user_guide/html/zigbee/developing_zigbee_applications/znp_interface/znp_interface.html)
- [Zigbee2MQTT Adapter Docs](https://www.zigbee2mqtt.io/guide/adapters/zstack.html)
- [Z-Stack Firmware Repository](https://github.com/Koenkk/Z-Stack-firmware)
