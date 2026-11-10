# Phase 6: M4 Bare-Metal Firmware - Detailed TODO

## Overview
**Status**: Alpha - RPMsg works through `/dev/ttyRPMSG0`; sensors don't yet (#18)

**Correction:** IPCC isn't blocked by TrustZone. The M4 reaches it on this board with the current boot chain, and Linux and the M4 use RPMsg normally through `/dev/ttyRPMSG0`. The firmware just never rang it. See [m4-firmware.md](m4-firmware.md). What this page says about polling is history.
**Objective**: Implement real-time sensor acquisition and time-critical functions on the Cortex-M4 core.
**Prerequisites**: Phase 2 complete (core application with RPMsg)

---

## Completion Status

| Component | Status | Notes |
|-----------|--------|-------|
| Project structure | ✅ | CMakeLists.txt, directories created |
| Linker script | ✅ | Vector table in RETRAM, code in MCUSRAM |
| Startup code | ✅ | Vector table, Reset_Handler, C++ support |
| STM32MP1 registers | ✅ | GPIO, I2C, IPCC, RCC, NVIC, SysTick |
| Clock driver | ✅ | SysTick, delayMs/Us, peripheral enables |
| GPIO driver | ✅ | Mode, pull, speed, alternate function |
| I2C driver | ✅ | Master mode, probe, read, write |
| OpenAMP resource table | ✅ | VirtIO RPMsg, trace buffer |
| RPMsg interface | ✅ | VirtIO vring protocol, name service announcement |
| SHT31 driver | ✅ | Temperature/humidity with CRC |
| Sensor manager | ✅ | Periodic polling, RPMsg reporting |
| Main application | ✅ | Command handling, sensor polling |
| Compilation test | ✅ | 3.5KB binary, ARM hard-float ABI |
| Host-side unit tests | ✅ | 115 tests in 5 suites |
| Hardware: Firmware load | ✅ | remoteproc loads and runs firmware |
| Hardware: Resource table | ✅ | Linux detects and registers VirtIO device |
| Hardware: VirtIO vring | ✅ | M4 writes to TX used ring correctly (idx=2) |
| Hardware: IPCC notification | ✅ | `/dev/ttyRPMSG0` appears |
| Hardware: Communication | ✅ | Both ways through `/dev/ttyRPMSG0` |

---

## 6.1 Project Structure

```
m4-firmware/
├── CMakeLists.txt
├── linker/
│   └── stm32mp15xx_m4.ld
├── include/smarthub_m4/
│   ├── stm32mp1xx.h           # Register definitions
│   ├── drivers/
│   │   ├── clock.hpp
│   │   ├── gpio.hpp
│   │   └── i2c.hpp
│   ├── rpmsg/
│   │   └── rpmsg.hpp
│   └── sensors/
│       ├── sensor_manager.hpp
│       └── sht31.hpp
└── src/
    ├── main.cpp
    ├── system/
    │   ├── startup.cpp
    │   └── clock.cpp
    ├── drivers/
    │   ├── gpio.cpp
    │   └── i2c.cpp
    ├── rpmsg/
    │   ├── resource_table.c
    │   └── rpmsg.cpp
    └── sensors/
        ├── sensor_manager.cpp
        └── sht31.cpp
```

---

## 6.2 Memory Layout

The linker script and device tree define the following memory regions:

| Region | Address | Size | Purpose |
|--------|---------|------|---------|
| MCUSRAM | 0x10000000 | 256K | M4 code and data |
| RETRAM | 0x00000000 | 64K | Vector table (aliased to 0x38000000) |
| vdev0vring0 | 0x10040000 | 4K | RPMsg TX vring (M4→A7) |
| vdev0vring1 | 0x10041000 | 4K | RPMsg RX vring (A7→M4) |
| vdev0buffer | 0x10042000 | 16K | Shared message buffers |
| mcu-rsc-table | 0x10048000 | 32K | Resource table location |

---

## 6.3 OpenAMP / RPMsg Protocol

### Message Types
```cpp
enum class MsgType : uint8_t {
    // A7 -> M4 Commands
    Ping = 0x01,
    GetSensorData = 0x10,
    SetInterval = 0x11,
    GetStatus = 0x20,
    GpioSet = 0x30,
    GpioGet = 0x31,

    // M4 -> A7 Responses
    Pong = 0x81,
    SensorData = 0x90,
    Status = 0xA0,
    GpioState = 0xB1,

    // M4 -> A7 events, sent unasked
    SensorUpdate = 0xC0,
    GpioChange = 0xC1,
    Error = 0xE0,
};
```

### Sensor Data Format
```cpp
struct SensorDataPayload {   // 16 bytes, after an 8-byte header
    uint8_t sensorId;
    uint8_t sensorType;      // 1=Temp, 2=Humidity
    uint16_t reserved;
    int32_t value;           // Fixed point: value / scale
    int32_t scale;
    uint32_t timestamp;
};
```

`shared/include/smarthub_protocol/rpmsg_protocol.hpp`, which both cores build against, is the reference. See [m4-firmware.md](m4-firmware.md#rpmsg-protocol).

---

## 6.4 Build and Deploy

### Build
```bash
cd m4-firmware
mkdir -p build && cd build
cmake ..
make
```

Output files:
- `smarthub-m4.elf` - ELF executable for debugging
- `smarthub-m4.bin` - Raw binary for loading
- `smarthub-m4.hex` - Intel HEX format
- `smarthub-m4.map` - Linker map file

### Deploy to Board
```bash
# Copy firmware to board
scp build/smarthub-m4.elf root@board:/lib/firmware/

# Stop M4, load new firmware, restart
ssh root@board << 'EOF'
echo stop > /sys/class/remoteproc/remoteproc0/state
echo smarthub-m4.elf > /sys/class/remoteproc/remoteproc0/firmware
echo start > /sys/class/remoteproc/remoteproc0/state
EOF
```

### Monitor Output
```bash
# View M4 trace buffer
cat /sys/kernel/debug/remoteproc/remoteproc0/trace0

# Check remoteproc status
cat /sys/class/remoteproc/remoteproc0/state
```

---

## 6.5 Hardware Testing Results

**Board**: STM32MP157F-DK2 at <board-ip>

### Completed Tests

| Test | Command | Result |
|------|---------|--------|
| Firmware loads | `echo start > .../state` | ✅ "running" |
| Resource table | Check dmesg | ✅ VirtIO registered |
| RPMsg virtio | `ls /sys/bus/rpmsg/devices/` | ✅ virtio0.rpmsg_ctrl.0.0 |
| RPMsg endpoint | ioctl RPMSG_CREATE_EPT | ✅ /dev/rpmsg0 created |

### Pending Tests (VirtIO Refinement Needed)

| Test | Status | Notes |
|------|--------|-------|
| ttyRPMSG auto-create | ⚠️ | M4 needs to send NS announcement via VirtIO |
| Ping/Pong | ☐ | Requires working VirtIO vring |
| Sensor poll | ☐ | Requires working VirtIO vring |
| GPIO toggle | ☐ | Requires working VirtIO vring |

### Technical Notes

The M4 firmware loads and runs successfully. VirtIO vring handling is now correct
(TX used ring shows idx=2 with proper NS announcement and status message).

#### The IPCC Fault

These notes used to say touching IPCC was a bus fault because of TrustZone, and listed ways around that: SP_MIN instead of OP-TEE, changing OP-TEE, and polling the vrings from Linux. None are needed. The firmware had stopped touching IPCC because of that fault, so nothing told Linux about its messages. The fault doesn't happen now, with or without the M4's own IPCC clock enable, and what caused it then was never established.

#### References

- [ST ETZPC Configuration](https://wiki.st.com/stm32mpu/wiki/ETZPC_device_tree_configuration)
- [TF-A STM32MP1 Documentation](https://trustedfirmware-a.readthedocs.io/en/v2.12/plat/st/stm32mp1.html)
- [ST Security Overview](https://wiki.st.com/stm32mpu/wiki/Security_overview)

---

## 6.7 Linux-Side VirtIO Polling (Alpha Workaround)

No longer needed, as the M4 can notify Linux (see the correction above). This polled the
VirtIO vring from userspace to receive messages from the M4.

### Memory Map

The VirtIO vrings are at fixed addresses defined in the device tree:

| Region | Address | Size | Purpose |
|--------|---------|------|---------|
| vdev0vring0 | 0x10040000 | 4K | TX vring (M4→A7) - poll this |
| vdev0vring1 | 0x10041000 | 4K | RX vring (A7→M4) |
| vdev0buffer | 0x10042000 | 16K | Shared message buffers |

### VirtIO Vring Structure

```c
struct vring {
    uint16_t flags;      // offset 0x00
    uint16_t idx;        // offset 0x02 - number of entries added
    // ... descriptors and available ring ...
    // At offset 0xA0 (for 16 descriptors):
    struct {
        uint16_t flags;  // offset 0xA0
        uint16_t idx;    // offset 0xA2 - used ring index (poll this)
        struct {
            uint32_t id;
            uint32_t len;
        } ring[];
    } used;
};
```

### Polling Tool

The `tools/rpmsg_poll` directory contains a userspace polling daemon:

```bash
# Build with Buildroot toolchain
cd tools/rpmsg_poll
make buildroot

# Deploy to board
scp -O rpmsg_poll root@<board-ip>:/root/

# On the board - dump current vring state
./rpmsg_poll --dump

# On the board - poll for new messages
./rpmsg_poll --interval 100  # Poll every 100ms

# Options:
#   -d, --dump       Dump vring state and exit
#   -v, --verbose    Verbose output
#   -1, --once       Read once and exit
#   -i, --interval   Poll interval in ms (default: 100)
```

**Tested successfully**: The tool correctly reads the used ring and displays
the Name Service announcement and status messages from the M4.

### Integration with SmartHub App

The main SmartHub application can either:
1. Run rpmsg_poll as a subprocess and read from its stdout
2. Link against librpmsg_poll and call polling functions directly
3. Use the M4 communication module which wraps the polling

---

## 6.6 Future Enhancements (Deferred)

- ⏸️ BME280 barometric pressure sensor driver
- ⏸️ UART driver for additional sensors
- ⏸️ ADC driver for analog sensors
- ⏸️ PWM output for LED dimming
- ⏸️ Watchdog timer implementation
- ⏸️ Low-power sleep modes
