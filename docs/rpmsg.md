# SmartHub RPMsg Client

The RPMsg client enables communication between the Cortex-A7 Linux application and the Cortex-M4 bare-metal firmware via OpenAMP/RPMsg.

## Overview

The RpmsgClient class provides:
- Bidirectional communication with M4 core
- Typed message protocol for sensors, GPIO, PWM, ADC
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
│  │ - PWM generation                             │   │
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

### Message Format

```
┌──────────┬──────────┬─────────────────────┐
│ Type (1) │ Len (1)  │ Payload (0-253)     │
└──────────┴──────────┴─────────────────────┘
```

### Message Types

| Type | Value | Direction | Description |
|------|-------|-----------|-------------|
| Ping | 0x00 | A7 → M4 | Connectivity check |
| Pong | 0x01 | M4 → A7 | Ping response |
| SensorData | 0x10 | M4 → A7 | Sensor reading |
| GpioCommand | 0x20 | A7 → M4 | Set GPIO state |
| GpioState | 0x21 | M4 → A7 | GPIO state report |
| AdcRequest | 0x30 | A7 → M4 | Request ADC reading |
| AdcResponse | 0x31 | M4 → A7 | ADC value response |
| PwmCommand | 0x40 | A7 → M4 | Set PWM duty cycle |
| Config | 0x50 | Both | Configuration data |
| Error | 0xFF | M4 → A7 | Error notification |

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
// Initialize connection to M4
if (client.initialize()) {
    // Connected and M4 responded to ping
}

// Check connection status
if (client.isConnected()) {
    // ...
}

// Shutdown
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
// Send raw data
std::vector<uint8_t> data = {0x01, 0x02, 0x03};
client.send(data);

// Send typed message
client.sendMessage(RpmsgMessageType::Config, configPayload);

// Ping M4 (verify responsiveness)
if (client.ping()) {
    // M4 is responsive
}
```

### GPIO Control

```cpp
// Set GPIO pin high
client.setGpio(13, true);   // Pin 13 = HIGH

// Set GPIO pin low
client.setGpio(13, false);  // Pin 13 = LOW
```

### PWM Control

```cpp
// Set PWM duty cycle (0-65535)
client.setPwm(0, 32768);  // Channel 0, 50% duty

// Full brightness
client.setPwm(1, 65535);  // Channel 1, 100%

// Off
client.setPwm(1, 0);      // Channel 1, 0%
```

### Sensor Data Request

```cpp
// Request sensor reading from M4
client.requestSensorData(0);  // Sensor ID 0

// Response comes via EventBus or callback
```

### Message Callback

```cpp
client.setMessageCallback([](const std::vector<uint8_t>& data) {
    auto type = static_cast<RpmsgMessageType>(data[0]);
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

SensorData message payload:

```
┌────────────┬────────────┬────────────┬────────────┐
│ SensorID   │ Value Low  │ Value Mid  │ Value High │
│ (1 byte)   │ (1 byte)   │ (1 byte)   │ (1 byte)   │
└────────────┴────────────┴────────────┴────────────┘
```

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
3. Respond to Ping messages with Pong
4. Send SensorData messages for sensor readings
5. Process GpioCommand and PwmCommand messages

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
            if (sensor.sensorId == "0") {  // Temperature sensor
                m_temperature = sensor.value / 100.0;  // Convert to Celsius
            }
        });
    }

    void requestUpdate() {
        m_rpmsg.requestSensorData(0);
    }

    double temperature() const { return m_temperature; }

private:
    RpmsgClient& m_rpmsg;
    double m_temperature = 0.0;
};
```

## Example: LED PWM Control

```cpp
class LedController {
public:
    LedController(RpmsgClient& rpmsg) : m_rpmsg(rpmsg) {}

    void setBrightness(uint8_t channel, float percent) {
        uint16_t duty = static_cast<uint16_t>(percent * 655.35f);
        m_rpmsg.setPwm(channel, duty);
    }

    void fadeIn(uint8_t channel, int durationMs) {
        for (int i = 0; i <= 100; i++) {
            setBrightness(channel, static_cast<float>(i));
            std::this_thread::sleep_for(
                std::chrono::milliseconds(durationMs / 100));
        }
    }

private:
    RpmsgClient& m_rpmsg;
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
