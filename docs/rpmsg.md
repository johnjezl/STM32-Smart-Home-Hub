# SmartHub RPMsg Client

The RPMsg client enables communication between the Cortex-A7 Linux application and the Cortex-M4 bare-metal firmware via OpenAMP/RPMsg.

## Overview

The RpmsgClient class provides:
- Bidirectional communication with M4 core
- Typed message protocol for sensors and GPIO
- EventBus integration for sensor data delivery
- Non-blocking polling for message reception

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                    Cortex-A7                        │
│  ┌─────────────┐    ┌─────────────────────────┐    │
│  │ SmartHub    │───▶│ RpmsgClient             │    │
│  │ Application │◀───│ /dev/ttyRPMSG0          │    │
│  └─────────────┘    └───────────┬─────────────┘    │
└─────────────────────────────────┼───────────────────┘
                                  │ RPMsg/OpenAMP
┌─────────────────────────────────┼───────────────────┐
│                    Cortex-M4    ▼                   │
│  ┌─────────────────────────────────────────────┐   │
│  │ Bare-metal Firmware                          │   │
│  │ - Sensor reading (ADC, I2C, SPI)            │   │
│  │ - GPIO control                               │   │
│  │ - Real-time tasks                            │   │
│  └─────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

## Configuration

RPMsg settings in `config.yaml`:

```yaml
rpmsg:
  device: /dev/ttyRPMSG0  # RPMsg TTY device
```

## Message Protocol

Both sides build against one definition, `shared/include/smarthub_protocol/rpmsg_protocol.hpp`. Each side's tests check the same bytes from `rpmsg_test_vectors.hpp`, so a change on either side that breaks the other fails its tests.

### Message Format

An 8-byte header, then the payload, little-endian:

```
type (1) | flags (1) | seq (2) | len (2) | reserved (2) | payload (len)
```

### Message Types

| Type | Value | Direction | Payload |
|------|-------|-----------|---------|
| CMD_PING | 0x01 | A7 → M4 | None |
| CMD_GET_SENSOR_DATA | 0x10 | A7 → M4 | None. Reads sensors now. |
| CMD_SET_INTERVAL | 0x11 | A7 → M4 | `IntervalPayload` |
| CMD_GET_STATUS | 0x20 | A7 → M4 | None |
| CMD_SET_GPIO | 0x30 | A7 → M4 | `GPIOPayload` |
| CMD_GET_GPIO | 0x31 | A7 → M4 | `GPIOPayload` |
| RSP_PONG | 0x81 | M4 → A7 | None |
| RSP_SENSOR_DATA | 0x90 | M4 → A7 | `SensorDataPayload` |
| RSP_STATUS | 0xA0 | M4 → A7 | `StatusPayload` |
| RSP_GPIO_STATE | 0xB1 | M4 → A7 | `GPIOPayload` |
| EVT_SENSOR_UPDATE | 0xC0 | M4 → A7 | `SensorDataPayload` |
| EVT_GPIO_CHANGE | 0xC1 | M4 → A7 | `GPIOPayload` |
| EVT_ERROR | 0xE0 | M4 → A7 | None |

## API Reference

### Construction

```cpp
#include <smarthub/rpmsg/RpmsgClient.hpp>

smarthub::EventBus eventBus;

// Default device
smarthub::RpmsgClient client(eventBus);

// Custom device path
smarthub::RpmsgClient client(eventBus, "/dev/ttyRPMSG1");
```

### Initialization

```cpp
// Opens the device and pings the M4, waiting briefly for the answer
if (client.initialize()) {
    // The M4 answered
} else if (client.isConnected()) {
    // The device is open, but nothing answered. It stays open and poll() keeps pinging in case
    // the M4 starts later, and client.m4Responded() changes if it does.
}

client.shutdown();
```

### Polling

Call from main loop to receive messages:

```cpp
while (running) {
    client.poll();  // Non-blocking
    // ... other work
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
}
```

### Sending Messages

```cpp
// Send a typed message
client.sendMessage(protocol::MsgType::CMD_GET_STATUS, {});

// Ping the M4. This only says the ping was sent: m4Responded() shows when an answer has come back,
// which poll() picks up.
client.ping();
```

### GPIO Control

```cpp
client.setGpio(1, 7, true);  // Port B, pin 7, high
```

### Sensor Data

```cpp
client.requestSensorData();         // Read sensors now
client.setSensorInterval(1000);     // Read them every second
```

Readings arrive as `SensorDataEvent`s.

### Message Callback

```cpp
// data is the whole message, header included
client.setMessageCallback([](const std::vector<uint8_t>& data) {
    const auto type = static_cast<protocol::MsgType>(data[0]);
    // Handle message based on type
});
```

## EventBus Integration

Sensor data is automatically published to EventBus:

```cpp
eventBus.subscribe("sensor.data", [](const smarthub::Event& e) {
    auto& sensor = static_cast<const smarthub::SensorDataEvent&>(e);
    std::cout << "Sensor " << sensor.sensorId
              << " = " << sensor.value << std::endl;
});
```

Raw messages are also published:

```cpp
eventBus.subscribe("rpmsg.message", [](const smarthub::Event& e) {
    auto& msg = static_cast<const smarthub::RpmsgMessageEvent&>(e);
    // Process raw message data
});
```

## Sensor Data Format

`SensorDataPayload` holds a fixed-point `value` and a `scale`. The real reading is `value / scale`, which is what `SensorDataEvent::value` holds.

## Error Handling

- `initialize()` returns `false` if device doesn't exist or M4 doesn't respond
- Operations on disconnected client return `false`
- Errors are logged via the Logger system
- Connection status updates on read errors

## Testing

Run RPMsg tests:

```bash
cd app/build && ctest -R RPMsg --verbose
```

Hardware tests only run on target device:

```bash
# On STM32MP157F-DK2 with M4 firmware loaded
./tests/test_rpmsg
```

## M4 Firmware Requirements

The M4 firmware must:
1. Initialize OpenAMP/RPMsg communication
2. Create an RPMsg endpoint
3. Answer CMD_PING with RSP_PONG
4. Send EVT_SENSOR_UPDATE messages for sensor readings
5. Process CMD_SET_GPIO, CMD_SET_INTERVAL and CMD_GET_SENSOR_DATA

See `m4-firmware/` directory for reference implementation.

## Example: Temperature Monitor

```cpp
class TemperatureMonitor {
public:
    TemperatureMonitor(RpmsgClient& rpmsg, EventBus& events)
        : m_rpmsg(rpmsg)
    {
        events.subscribe("sensor.data", [this](const Event& e) {
            auto& sensor = static_cast<const SensorDataEvent&>(e);
            if (sensor.sensorId == "0") {
                m_temperature = sensor.value;
            }
        });
    }

    void requestUpdate() {
        m_rpmsg.requestSensorData();
    }

    double temperature() const { return m_temperature; }

private:
    RpmsgClient& m_rpmsg;
    double m_temperature = 0.0;
};
```

## Troubleshooting

### Device not found

```
Could not open RPMsg device /dev/ttyRPMSG0
```

**Solutions:**
1. Check M4 firmware is loaded: `cat /sys/class/remoteproc/remoteproc0/state`
2. Start M4: `echo start > /sys/class/remoteproc/remoteproc0/state`
3. Verify device exists: `ls -la /dev/ttyRPMSG*`

### M4 not responding to ping

**Solutions:**
1. Check M4 firmware implements RPMsg endpoint correctly
2. Verify RPMsg channel name matches
3. Check for M4 firmware crashes in dmesg
