# Phase 6: M4 Bare-Metal Firmware - Detailed TODO

## Overview
**Status**: Alpha - Using Linux-side VirtIO polling (IPCC blocked by TrustZone)
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
| Hardware: IPCC notification | ❌ | **BLOCKED**: TrustZone prevents M4 IPCC access |
| Hardware: Linux VirtIO polling | ✅ | rpmsg_poll tool reads messages via /dev/mem |
| Hardware: Communication | ✅ | Alpha: Polling-based M4→A7 communication working |

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
    GpioState = 0xB0,
};
```

### Sensor Data Format
```cpp
struct SensorPayload {
    uint8_t sensorId;
    uint8_t sensorType;    // 1=Temp, 2=Humidity
    int16_t valueX100;     // Fixed-point value * 100
    uint32_t timestamp;
};
```

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

**Board**: STM32MP157F-DK2 at 192.168.4.99

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

#### Root Cause: TrustZone Blocks IPCC Access

**Issue**: The M4 cannot access IPCC (Inter-Processor Communication Controller) at 0x4C001000.
Any read or write to IPCC or HSEM (0x4C000000) causes a bus fault.

**Evidence**:
- M4 trace stops at "RPMSG:IPCC test" when attempting IPCC access
- TX used ring at 0x100400A0 shows idx=2 (messages written correctly)
- No ttyRPMSG device because Linux never receives IPCC notification

**Root Cause Analysis**:

1. **IPCC is NOT in ETZPC DECPROT list** - Unlike peripherals like I2C, SPI, UART which have
   ETZPC IDs (0-86), IPCC has no ETZPC DECPROT entry. It's protected differently.

2. **Current boot chain uses OP-TEE** - The buildroot config uses:
   - `BR2_TARGET_ARM_TRUSTED_FIRMWARE_BL32_OPTEE=y`
   - `AARCH32_SP=optee`

   This means OP-TEE runs in secure world and controls security configuration.

3. **IPCC is in secure peripheral address space** - Address 0x4C001000 is in a region
   that requires secure access. The M4 runs in non-secure mode by default.

4. **ST's standard configuration blocks M4 peripheral access** - Per ST documentation:
   "In Linux deliveries, Cortex-M4 access is forbidden inside TrustZone address space"

#### Solution Options

**Option 1: Rebuild TF-A with SP_MIN instead of OP-TEE** ⏸️ *Deferred - boot failure*

Attempted but **failed**: SP_MIN causes boot loop at "BL2: Booting BL32".

Configuration tried:
```
BR2_TARGET_ARM_TRUSTED_FIRMWARE_BL32_SP_MIN=y
BR2_TARGET_ARM_TRUSTED_FIRMWARE_ADDITIONAL_VARIABLES="STM32MP_SDMMC=1 AARCH32_SP=sp_min DTB_FILE_NAME=stm32mp157f-dk2.dtb BL33_CFG=$(BINARIES_DIR)/u-boot.dtb"
```

**Failure symptoms**: Board repeatedly prints "BL2: Booting BL32" then resets.
**Likely causes**: Memory layout mismatch, DTB configuration, or SP_MIN initialization issue.
**Future investigation**: Debug with JTAG or add early UART prints to SP_MIN.

**Option 2: Configure OP-TEE for M4 IPCC access** ⏸️ *Deferred*

Requires modifying OP-TEE platform code to:
1. Configure IPCC security registers (IPCC_C2SECCFGR, IPCC_C2PRIVCFGR)
2. Grant M4 access to IPCC channels

Location: `optee_os/core/arch/arm/plat-stm32mp1/`

This is more complex but would allow keeping OP-TEE's security features.

**Option 3: Linux-side VirtIO polling** ✅ *Current alpha approach*

Instead of M4 sending IPCC notifications:
1. M4 writes messages to VirtIO vring (already working - TX used ring shows idx=2)
2. Linux A7 polls the vring for new messages using userspace tool
3. No kernel modifications needed

This approach is sufficient for alpha development and avoids TrustZone complexity.
Implementation: `tools/rpmsg_poll` - userspace VirtIO vring polling daemon.

#### References

- [ST ETZPC Configuration](https://wiki.st.com/stm32mpu/wiki/ETZPC_device_tree_configuration)
- [TF-A STM32MP1 Documentation](https://trustedfirmware-a.readthedocs.io/en/v2.12/plat/st/stm32mp1.html)
- [ST Security Overview](https://wiki.st.com/stm32mpu/wiki/Security_overview)

---

## 6.7 Linux-Side VirtIO Polling (Alpha Workaround)

Since IPCC notifications are blocked by TrustZone, we use userspace polling of the
VirtIO vring to receive messages from the M4.

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
scp -O rpmsg_poll root@192.168.4.99:/root/

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
