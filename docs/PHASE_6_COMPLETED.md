# Phase 6: M4 Bare-Metal Firmware - ALPHA COMPLETE

**Completion Date**: 2025-12-31
**Status**: Alpha (IPCC blocked by TrustZone, using Linux-side polling workaround)

---

## Summary

Phase 6 implemented bare-metal firmware for the Cortex-M4 coprocessor to handle real-time sensor acquisition. The firmware successfully loads and runs, but inter-processor communication is blocked by TrustZone security configuration. A Linux-side VirtIO polling workaround enables alpha functionality.

---

## What Works

### Firmware Infrastructure
| Component | Status | Notes |
|-----------|--------|-------|
| CMake build system | ✅ | Cross-compilation for Cortex-M4 |
| Linker script | ✅ | Vector table in RETRAM, code in MCUSRAM |
| Startup code | ✅ | C++ static initialization support |
| STM32MP1 register definitions | ✅ | GPIO, I2C, RCC, NVIC, SysTick, IPCC |

### Drivers
| Driver | Status | Notes |
|--------|--------|-------|
| Clock/SysTick | ✅ | delayMs/Us, peripheral clock enable |
| GPIO | ✅ | Mode, pull, speed, alternate function |
| I2C | ✅ | Master mode, probe, read, write |
| SHT31 sensor | ✅ | Temperature/humidity with CRC |

### OpenAMP/RPMsg
| Component | Status | Notes |
|-----------|--------|-------|
| Resource table | ✅ | Correct format, Linux detects VirtIO |
| VirtIO vring TX | ✅ | M4 writes to used ring correctly |
| Name Service announcement | ✅ | Sent via vring |
| **IPCC notification** | ❌ | **BLOCKED by TrustZone** |

### Linux-Side Workaround
| Component | Status | Notes |
|-----------|--------|-------|
| rpmsg_poll tool | ✅ | Userspace VirtIO vring polling |
| Message extraction | ✅ | Reads M4 messages from shared memory |

---

## What Doesn't Work

### IPCC Access Blocked

The M4 cannot access the Inter-Processor Communication Controller (IPCC) at 0x4C001000. Any read or write causes a bus fault.

**Root Cause**: The STM32MP1 boot chain uses OP-TEE (TrustZone), which places IPCC in the secure peripheral space. The M4 runs in non-secure mode.

**Evidence**:
- M4 trace stops at "RPMSG:IPCC test"
- TX used ring shows correct message writes (idx=2)
- Linux never receives IPCC interrupt → no ttyRPMSG device

---

## Solution Options

### Option 1: SP_MIN Instead of OP-TEE
**Status**: ⏸️ Deferred (boot failure)

Attempted configuration:
```
BR2_TARGET_ARM_TRUSTED_FIRMWARE_BL32_SP_MIN=y
AARCH32_SP=sp_min
```

Result: Boot loop at "BL2: Booting BL32"

### Option 2: Configure OP-TEE for M4 IPCC
**Status**: ⏸️ Deferred (complex)

Would require modifying OP-TEE platform code to grant M4 access to IPCC channels.

### Option 3: Linux-Side VirtIO Polling ✅
**Status**: Implemented (current workaround)

The `tools/rpmsg_poll` tool polls the VirtIO vring from userspace using `/dev/mem`:

```bash
./rpmsg_poll --dump      # Dump vring state
./rpmsg_poll --interval 100  # Poll every 100ms
```

This enables M4→A7 communication without kernel modifications.

---

## Memory Layout

| Region | Address | Size | Purpose |
|--------|---------|------|---------|
| RETRAM | 0x00000000 | 64K | Vector table |
| MCUSRAM | 0x10000000 | 256K | Code and data |
| vdev0vring0 | 0x10040000 | 4K | TX vring (M4→A7) |
| vdev0vring1 | 0x10041000 | 4K | RX vring (A7→M4) |
| vdev0buffer | 0x10042000 | 16K | Shared buffers |
| mcu-rsc-table | 0x10048000 | 32K | Resource table |

---

## Message Protocol

### Message Types
```cpp
enum class MsgType : uint8_t {
    // A7 -> M4
    Ping = 0x01,
    GetSensorData = 0x10,
    SetInterval = 0x11,
    GetStatus = 0x20,
    GpioSet = 0x30,

    // M4 -> A7
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
    int16_t valueX100;     // Fixed-point * 100
    uint32_t timestamp;
};
```

---

## Test Results

### Host-Side Unit Tests
- **115 tests** across 5 suites
- Tests GPIO, I2C, Clock, SHT31, SensorManager
- All passing

### Hardware Tests
| Test | Result | Notes |
|------|--------|-------|
| Firmware load via remoteproc | ✅ | State shows "running" |
| Resource table detection | ✅ | VirtIO device registered |
| VirtIO vring writes | ✅ | TX used ring idx=2 |
| rpmsg_poll message read | ✅ | NS announcement visible |
| IPCC access | ❌ | Bus fault |

---

## Files Created

### Firmware
```
m4-firmware/
├── CMakeLists.txt
├── linker/
│   └── stm32mp15xx_m4.ld
├── include/smarthub_m4/
│   ├── stm32mp1xx.h
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

### Polling Tool
```
tools/rpmsg_poll/
├── Makefile
├── README.md
└── rpmsg_poll.c
```

---

## Build and Deploy

### Build
```bash
cd m4-firmware
mkdir -p build && cd build
cmake ..
make
```

Output: `smarthub-m4.elf` (3.5KB)

### Deploy
```bash
scp build/smarthub-m4.elf root@board:/lib/firmware/
ssh root@board << 'EOF'
echo stop > /sys/class/remoteproc/remoteproc0/state
echo smarthub-m4.elf > /sys/class/remoteproc/remoteproc0/firmware
echo start > /sys/class/remoteproc/remoteproc0/state
EOF
```

### Monitor
```bash
# Check state
cat /sys/class/remoteproc/remoteproc0/state

# View trace buffer
cat /sys/kernel/debug/remoteproc/remoteproc0/trace0

# Poll for messages
./rpmsg_poll --interval 100
```

---

## Deferred Enhancements

| Feature | Notes |
|---------|-------|
| BME280 pressure sensor | Additional I2C sensor |
| UART driver | Serial sensor support |
| ADC driver | Analog sensors |
| PWM output | LED dimming |
| Watchdog timer | Reliability |
| Low-power modes | Battery optimization |
| IPCC fix | Requires OP-TEE modification or SP_MIN debug |

---

## References

- [ST ETZPC Configuration](https://wiki.st.com/stm32mpu/wiki/ETZPC_device_tree_configuration)
- [TF-A STM32MP1 Documentation](https://trustedfirmware-a.readthedocs.io/en/v2.12/plat/st/stm32mp1.html)
- [ST Security Overview](https://wiki.st.com/stm32mpu/wiki/Security_overview)

---

## Phase 6 Status: ⚠️ ALPHA COMPLETE

Firmware infrastructure complete. IPCC blocked by TrustZone; using Linux-side polling workaround. Full functionality requires resolving IPCC access in future work.
