# SmartHub MQTT Client

The MQTT client enables communication with external devices and services via the MQTT protocol. It uses libmosquitto as the underlying library.

## Overview

The MqttClient class provides:
- Connection to MQTT brokers
- Topic subscription with QoS support
- Message publishing with retain flag
- Automatic reconnection handling
- EventBus integration for message delivery

## Configuration

MQTT settings in `config.yaml`:

```yaml
mqtt:
  broker: 127.0.0.1    # Broker hostname/IP
  port: 1883           # Broker port (default: 1883)
  client_id: smarthub  # Client identifier
  username: ""         # Optional username
  password: ""         # Optional password
```

## API Reference

### Construction

```cpp
#include <smarthub/protocols/mqtt/MqttClient.hpp>

smarthub::EventBus eventBus;
smarthub::MqttClient client(eventBus, "127.0.0.1", 1883);
```

### Configuration Methods

```cpp
// Set client ID (before connect)
client.setClientId("my_smarthub");

// Set authentication credentials (before connect)
client.setCredentials("username", "password");

// Set message callback (in addition to EventBus)
client.setMessageCallback([](const std::string& topic, const std::string& payload) {
    // Handle message
});
```

### Connection

```cpp
// Connect to broker
if (client.connect()) {
    // Connected successfully
}

// Check connection status
if (client.isConnected()) {
    // ...
}

// Disconnect
client.disconnect();
```

### Subscribe/Unsubscribe

```cpp
// Subscribe with default QoS (AtLeastOnce)
client.subscribe("smarthub/devices/#");

// Subscribe with specific QoS
client.subscribe("sensors/temperature", smarthub::MqttQoS::ExactlyOnce);

// Unsubscribe
client.unsubscribe("smarthub/devices/#");
```

### Publishing

```cpp
// Publish with default QoS, no retain
client.publish("smarthub/status", "online");

// Publish with QoS and retain flag
client.publish("smarthub/status", "online",
               smarthub::MqttQoS::AtLeastOnce, true);
```

### QoS Levels

| Level | Enum Value | Description |
|-------|------------|-------------|
| 0 | `MqttQoS::AtMostOnce` | Fire and forget |
| 1 | `MqttQoS::AtLeastOnce` | Acknowledged delivery |
| 2 | `MqttQoS::ExactlyOnce` | Assured delivery |

## EventBus Integration

Messages are automatically published to the EventBus as `MqttMessageEvent`:

```cpp
eventBus.subscribe("mqtt.message", [](const smarthub::Event& e) {
    auto& msg = static_cast<const smarthub::MqttMessageEvent&>(e);
    std::cout << "Topic: " << msg.topic << std::endl;
    std::cout << "Payload: " << msg.payload << std::endl;
});
```

## Topic Conventions

SmartHub uses these MQTT topic patterns:

| Topic Pattern | Direction | Description |
|---------------|-----------|-------------|
| `smarthub/status` | Publish | Hub online status |
| `smarthub/devices/{id}/state` | Publish | Device state updates |
| `smarthub/devices/{id}/set` | Subscribe | Device control commands |
| `smarthub/sensors/{id}` | Publish | Sensor readings |
| `homeassistant/+/+/config` | Subscribe | HA discovery (future) |

## Error Handling

The client handles errors gracefully:
- Connection failures return `false` from `connect()`
- Operations on disconnected client return `false`
- Network errors trigger automatic reconnection attempts
- Errors are logged via the Logger system

## Thread Safety

- The MqttClient uses mosquitto's threaded loop
- Callbacks may be invoked from the mosquitto thread
- Use appropriate synchronization when accessing shared state

## Testing

Run MQTT tests:

```bash
cd app/build && ctest -R MQTT --verbose
```

For live broker tests, set environment variable:

```bash
MQTT_BROKER_TEST=1 ./tests/test_mqtt
```

## Dependencies

- libmosquitto (Mosquitto client library)
- Mosquitto broker (for runtime operation)

Install on Ubuntu:
```bash
sudo apt-get install libmosquitto-dev mosquitto
```

## Example: Device State Publisher

```cpp
void publishDeviceState(MqttClient& mqtt,
                        const std::string& deviceId,
                        const std::string& state) {
    std::string topic = "smarthub/devices/" + deviceId + "/state";
    mqtt.publish(topic, state, MqttQoS::AtLeastOnce, true);
}
```

## Example: Command Subscriber

```cpp
void setupCommandHandler(MqttClient& mqtt, DeviceManager& devices) {
    mqtt.setMessageCallback([&](const std::string& topic, const std::string& payload) {
        // Parse topic: smarthub/devices/{id}/set
        if (topic.find("smarthub/devices/") == 0 &&
            topic.find("/set") != std::string::npos) {
            // Extract device ID and apply command
            auto id = extractDeviceId(topic);
            auto device = devices.getDevice(id);
            if (device) {
                device->applyCommand(payload);
            }
        }
    });

    mqtt.subscribe("smarthub/devices/+/set");
}
```
