# M4 Firmware Documentation

## Overview

The SmartHub M4 firmware runs on the Cortex-M4 coprocessor of the STM32MP157 SoC. It handles real-time sensor acquisition and provides a communication interface to the Linux A7 core via OpenAMP/RPMsg.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Linux (Cortex-A7)                    │
│  ┌─────────────┐    ┌──────────────┐    ┌───────────┐  │
│  │  SmartHub   │◄──►│   RPMsg      │◄──►│ remoteproc│  │
│  │    App      │    │  /dev/ttyRPMSG   │    │  driver   │  │
│  └─────────────┘    └──────────────┘    └─────┬─────┘  │
└────────────────────────────────────────────────┼───────┘
                                                 │ IPCC
┌────────────────────────────────────────────────┼───────┐
│                    M4 Firmware                 │       │
│  ┌─────────────┐    ┌──────────────┐    ┌─────┴─────┐  │
│  │   Sensor    │───►│    RPMsg     │───►│   IPCC    │  │
│  │   Manager   │    │  Interface   │    │  Handler  │  │
│  └──────┬──────┘    └──────────────┘    └───────────┘  │
│         │                                              │
│  ┌──────┴──────┐                                       │
│  │   SHT31     │ ◄──► I2C Bus                         │
│  │   Driver    │                                       │
│  └─────────────┘                                       │
└────────────────────────────────────────────────────────┘
```

## Building

### Prerequisites

```bash
# Install ARM bare-metal toolchain
sudo apt-get install gcc-arm-none-eabi
```

### Build Commands

```bash
cd m4-firmware
mkdir -p build && cd build
cmake ..
make
```

### Build Outputs

| File | Description |
|------|-------------|
| smarthub-m4.elf | ELF executable with debug symbols |
| smarthub-m4.bin | Raw binary (3.3 KB) |
| smarthub-m4.hex | Intel HEX format |
| smarthub-m4.map | Linker map for symbol analysis |

## Memory Map

| Region | Address | Size | Contents |
|--------|---------|------|----------|
| RETRAM | 0x00000000 | 64K | Vector table (M4 boot address) |
| MCUSRAM | 0x10000000 | 256K | Code, data, BSS |
| vdev0vring0 | 0x10040000 | 4K | VirtIO TX vring (M4→A7) |
| vdev0vring1 | 0x10041000 | 4K | VirtIO RX vring (A7→M4) |
| vdev0buffer | 0x10042000 | 16K | Shared message buffers |
| rsc-table | 0x10048000 | 4K | OpenAMP resource table |
| trace0 | 0x10049000 | 4K | Debug trace buffer |

### VirtIO Vring Layout (TX at 0x10040000)

```
Offset    Size    Contents
0x000     0x80    Descriptor table (8 entries × 16 bytes)
0x080     0x14    Available ring (flags, idx, ring[8], used_event)
0x0A0     0x48    Used ring (flags, idx, ring[8] × 8 bytes)
```

## RPMsg Protocol

### Message Format

All messages use this header:
```cpp
struct MsgHeader {
    MsgType type;    // 1 byte: message type
    uint8_t len;     // 1 byte: payload length
    uint16_t seq;    // 2 bytes: sequence number
};
```

### Message Types

| Type | Code | Direction | Description |
|------|------|-----------|-------------|
| Ping | 0x01 | A7→M4 | Heartbeat request |
| Pong | 0x81 | M4→A7 | Heartbeat response |
| GetSensorData | 0x10 | A7→M4 | Request sensor reading |
| SensorData | 0x90 | M4→A7 | Sensor data response |
| SetInterval | 0x11 | A7→M4 | Set polling interval |
| GetStatus | 0x20 | A7→M4 | Request M4 status |
| Status | 0xA0 | M4→A7 | Status response |
| GpioSet | 0x30 | A7→M4 | Set GPIO output |
| GpioGet | 0x31 | A7→M4 | Read GPIO state |
| GpioState | 0xB0 | M4→A7 | GPIO state response |

### Sensor Data Payload

```cpp
struct SensorPayload {
    uint8_t sensorId;      // Sensor identifier
    uint8_t sensorType;    // 1=Temperature, 2=Humidity
    int16_t valueX100;     // Value multiplied by 100
    uint32_t timestamp;    // Milliseconds since boot
};
```

Example: Temperature of 23.45°C is encoded as `valueX100 = 2345`.

## Drivers

### Clock (clock.hpp)

```cpp
namespace smarthub::m4 {
    class Clock {
    public:
        static void init();              // Initialize SysTick
        static uint32_t getTicks();      // Get ms since init
        static void delayMs(uint32_t ms);
        static void delayUs(uint32_t us);
        static void enableGPIO(GPIO_Port port);
        static void enableI2C(I2C_Instance instance);
    };
}
```

### GPIO (gpio.hpp)

```cpp
namespace smarthub::m4 {
    class GPIO {
    public:
        static void setMode(GPIO_TypeDef* port, uint8_t pin, uint8_t mode);
        static void setOutputType(GPIO_TypeDef* port, uint8_t pin, uint8_t type);
        static void setPull(GPIO_TypeDef* port, uint8_t pin, uint8_t pull);
        static void setSpeed(GPIO_TypeDef* port, uint8_t pin, uint8_t speed);
        static void setAF(GPIO_TypeDef* port, uint8_t pin, uint8_t af);
        static void set(GPIO_TypeDef* port, uint8_t pin);
        static void reset(GPIO_TypeDef* port, uint8_t pin);
        static void toggle(GPIO_TypeDef* port, uint8_t pin);
        static bool read(GPIO_TypeDef* port, uint8_t pin);
    };
}
```

### I2C (i2c.hpp)

```cpp
namespace smarthub::m4 {
    class I2C {
    public:
        bool init(I2C_TypeDef* i2c, uint32_t speed = 400000);
        bool probe(uint8_t addr);
        bool write(uint8_t addr, const uint8_t* data, size_t len);
        bool read(uint8_t addr, uint8_t* data, size_t len);
        bool writeReg(uint8_t addr, uint8_t reg, uint8_t value);
        bool readReg(uint8_t addr, uint8_t reg, uint8_t* value);
        bool readRegs(uint8_t addr, uint8_t reg, uint8_t* data, size_t len);
    };
}
```

### SHT31 Sensor (sht31.hpp)

```cpp
namespace smarthub::m4 {
    class SHT31 {
    public:
        SHT31(I2C& i2c, uint8_t addr = 0x44);
        bool init();
        bool measure();
        float temperature() const;  // °C
        float humidity() const;     // %RH
        bool isValid() const;
    };
}
```

## Deployment

### Copy Firmware to Board

```bash
scp build/smarthub-m4.elf root@192.168.1.100:/lib/firmware/
```

### Load and Start Firmware

```bash
# On the board (via SSH)
echo stop > /sys/class/remoteproc/remoteproc0/state
echo smarthub-m4.elf > /sys/class/remoteproc/remoteproc0/firmware
echo start > /sys/class/remoteproc/remoteproc0/state
```

### Verify Firmware Running

```bash
cat /sys/class/remoteproc/remoteproc0/state
# Expected: running

# Check for RPMsg device
ls /dev/ttyRPMSG*
```

### View Debug Output

```bash
cat /sys/kernel/debug/remoteproc/remoteproc0/trace0
```

## Testing

### Unit Tests (Host-Side)

The M4 firmware includes 115 unit tests that run on x86 with mocked hardware:

```bash
cd m4-firmware/tests
mkdir -p build && cd build
cmake ..
make
ctest --output-on-failure
```

Test suites:
- **MessageTypesTest** (25 tests): Message structure, enum values, wire format
- **RpmsgProtocolTest** (20 tests): Message building, parsing, roundtrip
- **SHT31CalculationsTest** (28 tests): CRC-8, temperature/humidity conversions
- **I2CTransactionsTest** (21 tests): Mock I2C probe, read, write operations
- **SensorManagerTest** (21 tests): Polling logic, timing, sensor presence

### Ping Test

Send a ping message from the A7 side and verify pong response:

```cpp
// On A7 Linux application
int fd = open("/dev/ttyRPMSG0", O_RDWR);
uint8_t ping[] = {0x01, 0x00, 0x00, 0x00};  // Ping message
write(fd, ping, 4);

uint8_t response[64];
int n = read(fd, response, sizeof(response));
// response[0] should be 0x81 (Pong)
```

### Sensor Data Test

The M4 automatically sends sensor data every `SENSOR_POLL_MS` (default 1000ms):

```cpp
// On A7 Linux application
while (true) {
    uint8_t buf[64];
    int n = read(fd, buf, sizeof(buf));
    if (n > 0 && buf[0] == 0x90) {
        // Parse SensorPayload from buf+4
        int16_t temp = (buf[6] << 8) | buf[7];
        printf("Temperature: %.2f°C\n", temp / 100.0f);
    }
}
```

## Troubleshooting

### Firmware Won't Start

1. Check remoteproc state: `cat /sys/class/remoteproc/remoteproc0/state`
2. View kernel messages: `dmesg | grep -i rproc`
3. Verify firmware exists: `ls -la /lib/firmware/smarthub-m4.elf`

### M4 Not Executing (Trace Buffer Empty)

**Symptom**: remoteproc shows "running" but trace buffer at 0x10049000 is zeros.

**Root Cause**: Vector table not at M4 boot address (0x00000000).

**Solution**: Ensure linker script places `.isr_vector` in RETRAM:
```ld
.isr_vector :
{
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
} >RETRAM
```

On STM32MP1, the M4 always fetches its reset vector from address 0x00000000, which is aliased to RETRAM (0x38000000 from A7's view).

### No RPMsg Device (ttyRPMSG Not Created)

**Symptom**: M4 firmware runs, trace shows messages sent, but no `/dev/ttyRPMSG*` device.

**Diagnosis**:
1. Check trace buffer: `cat /sys/kernel/debug/remoteproc/remoteproc0/trace0`
2. Check vring used idx: `devmem 0x100400A0 32` (should show non-zero if M4 wrote messages)
3. Check dmesg for IPCC errors: `dmesg | grep -i ipcc`

**Root Cause**: TrustZone/ETZPC blocks M4 access to IPCC (Inter-Processor Communication Controller).

**Evidence**:
- M4 trace stops at "IPCC test" when attempting IPCC access
- TX used ring shows idx > 0 (messages written correctly)
- No notification reaches Linux

**Solution**: Modify TF-A device tree to allow M4 IPCC access. See `docs/PHASE_6_TODO.md` for details.

### IPCC Bus Fault

**Symptom**: M4 crashes when accessing IPCC at 0x4C001000 or HSEM at 0x4C000000.

**Root Cause**: TF-A configures ETZPC to restrict M4 access to APB5 peripherals.

**Workaround**: Skip IPCC notification (messages won't reach Linux):
```cpp
void Rpmsg::notifyHost() {
    // IPCC access blocked by TrustZone
    trace("NOTIFY:skip");
}
```

**Permanent Fix**: Rebuild TF-A with IPCC MCU isolation enabled.

### Verifying VirtIO Vring

Check if M4 has written to the TX used ring:
```bash
# TX used ring at 0x100400A0
# Format: flags (16-bit), idx (16-bit)
devmem 0x100400A0 32
# 0x00020000 means idx=2 (2 messages sent)

# Check descriptor 0 length
devmem 0x100400A8 32
# 0x00000038 = 56 bytes (name service announcement)
```

### I2C Not Working

1. Verify I2C pins are correctly muxed in device tree
2. Check sensor power supply
3. Use I2C probe to scan for devices
4. Note: I2C requires peripheral clocks enabled via RCC

## Source Files

| File | Description |
|------|-------------|
| src/main.cpp | Main application loop |
| src/system/startup.cpp | Vector table, Reset_Handler |
| src/system/clock.cpp | SysTick, delays |
| src/drivers/gpio.cpp | GPIO configuration |
| src/drivers/i2c.cpp | I2C master driver |
| src/rpmsg/resource_table.c | OpenAMP resource table |
| src/rpmsg/rpmsg.cpp | RPMsg message handling |
| src/sensors/sht31.cpp | SHT31 sensor driver |
| src/sensors/sensor_manager.cpp | Sensor polling manager |
