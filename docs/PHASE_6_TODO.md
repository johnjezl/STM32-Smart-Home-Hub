# Phase 6: M4 Bare-Metal Firmware - Detailed TODO

## Overview
**Duration**: 2-3 weeks  
**Objective**: Implement real-time sensor acquisition and time-critical functions on the Cortex-M4 core.  
**Prerequisites**: Phase 2 complete (core application with RPMsg)

---

## 6.1 Project Structure

```
m4-firmware/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── system/
│   │   ├── startup.cpp
│   │   └── clock.cpp
│   ├── drivers/
│   │   ├── gpio.cpp
│   │   ├── i2c.cpp
│   │   ├── uart.cpp
│   │   └── adc.cpp
│   ├── rpmsg/
│   │   ├── rpmsg_interface.cpp
│   │   └── mailbox.cpp
│   └── sensors/
│       ├── sensor_manager.cpp
│       ├── sht31.cpp
│       └── bme280.cpp
├── include/smarthub_m4/
├── linker/stm32mp15xx_m4.ld
└── openamp/resource_table.c
```

---

## 6.2 Memory Layout and Linker Script

### 6.2.1 M4 Memory Regions
```ld
/* m4-firmware/linker/stm32mp15xx_m4.ld */
MEMORY
{
    RETRAM (xrw)   : ORIGIN = 0x00000000, LENGTH = 64K
    MCUSRAM (xrw)  : ORIGIN = 0x10000000, LENGTH = 256K
    VRING_TX (xrw) : ORIGIN = 0x10040000, LENGTH = 16K
    VRING_RX (xrw) : ORIGIN = 0x10044000, LENGTH = 16K
}

ENTRY(Reset_Handler)

SECTIONS
{
    .isr_vector : { KEEP(*(.isr_vector)) } > RETRAM
    .text : { *(.text*) *(.rodata*) } > MCUSRAM
    .data : { *(.data*) } > MCUSRAM
    .bss : { *(.bss*) *(COMMON) } > MCUSRAM
    .resource_table : { KEEP(*(.resource_table)) } > MCUSRAM
}
```

---

## 6.3 OpenAMP / RPMsg Protocol

### 6.3.1 Message Types
```cpp
// M4 <-> A7 Message Protocol
enum class MessageType : uint8_t {
    // A7 -> M4 Commands
    CMD_PING = 0x01,
    CMD_GET_SENSOR_DATA = 0x10,
    CMD_SET_SENSOR_INTERVAL = 0x11,
    CMD_GET_STATUS = 0x20,
    
    // M4 -> A7 Responses/Events
    RSP_PONG = 0x81,
    RSP_SENSOR_DATA = 0x90,
    RSP_STATUS = 0xA0,
    EVT_SENSOR_UPDATE = 0xC0,
};

struct SensorDataMsg {
    uint8_t sensorId;
    uint8_t sensorType;  // Temperature=1, Humidity=2, etc.
    int32_t value;       // Fixed-point (temp*100, humidity*100)
    uint32_t timestamp;
};
```

### 6.3.2 RPMsg Interface
```cpp
class RpmsgInterface {
public:
    bool initialize();
    void poll();
    bool send(MessageType type, const void* data, size_t len);
    bool sendSensorData(uint8_t id, uint8_t type, int32_t value);
    void setHandler(std::function<void(const void*, size_t)> handler);
};
```

---

## 6.4 Sensor Drivers

### 6.4.1 I2C Driver
```cpp
class I2C {
public:
    bool initialize(uint32_t speed = 400000);
    bool writeReg(uint8_t addr, uint8_t reg, uint8_t value);
    uint8_t readReg(uint8_t addr, uint8_t reg);
    bool readRegs(uint8_t addr, uint8_t reg, uint8_t* data, size_t len);
};
```

### 6.4.2 SHT31 Temperature/Humidity
```cpp
class SHT31 {
public:
    SHT31(I2C& i2c, uint8_t addr = 0x44);
    bool initialize();
    bool measure();
    float temperature() const;  // Celsius
    float humidity() const;     // %RH
};
```

### 6.4.3 Sensor Manager
```cpp
class SensorManager {
public:
    bool initialize();
    void poll();  // Call from main loop
    void setPollingInterval(uint32_t ms);
    void setCallback(void(*cb)(const SensorReading&));
};
```

---

## 6.5 Main Application

```cpp
int main() {
    HAL_Init();
    initClock();
    
    I2C i2c;
    i2c.initialize(400000);
    
    SensorManager sensors(i2c);
    sensors.initialize();
    sensors.setCallback([](const SensorReading& r) {
        rpmsg().sendSensorData(r.id, r.type, r.value);
    });
    
    rpmsg().initialize();
    rpmsg().setHandler(handleCommand);
    
    while (1) {
        rpmsg().poll();
        sensors.poll();
        __WFI();  // Wait for interrupt
    }
}
```

---

## 6.6 Build and Deploy

```bash
# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make

# Deploy
scp smarthub_m4.elf board:/lib/firmware/
ssh board "echo stop > /sys/class/remoteproc/remoteproc0/state"
ssh board "echo smarthub_m4.elf > /sys/class/remoteproc/remoteproc0/firmware"
ssh board "echo start > /sys/class/remoteproc/remoteproc0/state"
```

---

## 6.7 Validation Checklist

| Item | Status | Notes |
|------|--------|-------|
| Project compiles | ☐ | |
| Firmware loads via remoteproc | ☐ | |
| RPMsg channel works | ☐ | |
| Ping/pong test passes | ☐ | |
| I2C bus works | ☐ | |
| SHT31 reads correctly | ☐ | |
| Sensor data reaches A7 | ☐ | |
| Polling interval configurable | ☐ | |

---

## Time Estimates

| Task | Estimated Time |
|------|----------------|
| 6.1-6.2 Project Setup | 4-6 hours |
| 6.3 OpenAMP/RPMsg | 8-12 hours |
| 6.4 Sensor Drivers | 8-10 hours |
| 6.5-6.6 Main App/Build | 4-6 hours |
| 6.7 Testing | 4-6 hours |
| **Total** | **28-40 hours** |
