# SmartHub Protocol Handler Framework

This document describes the protocol handler framework implemented in Phase 3.

## Overview

The protocol handler framework provides an abstraction layer for device communication protocols. It allows SmartHub to support multiple protocols (MQTT, Zigbee, HTTP, etc.) through a unified interface.

## Architecture

```
DeviceManager
    |
    +-- ProtocolFactory (creates handlers)
    |
    +-- IProtocolHandler implementations
           |
           +-- MqttProtocolHandler (Phase 4)
           +-- ZigbeeProtocolHandler (future)
           +-- HttpProtocolHandler (future)
           +-- MockProtocolHandler (testing)
```

## IProtocolHandler Interface

All protocol handlers implement the `IProtocolHandler` interface:

```cpp
#include <smarthub/protocols/IProtocolHandler.hpp>

class IProtocolHandler {
public:
    // Identification
    virtual std::string name() const = 0;
    virtual std::string version() const = 0;
    virtual std::string description() const = 0;

    // Lifecycle
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual void poll() = 0;

    // State
    virtual ProtocolState state() const = 0;
    virtual bool isConnected() const = 0;
    virtual std::string lastError() const = 0;

    // Discovery
    virtual bool supportsDiscovery() const = 0;
    virtual void startDiscovery() = 0;
    virtual void stopDiscovery() = 0;
    virtual bool isDiscovering() const = 0;

    // Commands
    virtual bool sendCommand(const std::string& address,
                             const std::string& command,
                             const nlohmann::json& params) = 0;

    // Callbacks
    virtual void setDeviceDiscoveredCallback(DeviceDiscoveredCallback cb) = 0;
    virtual void setDeviceStateCallback(DeviceStateCallback cb) = 0;
    virtual void setDeviceAvailabilityCallback(DeviceAvailabilityCallback cb) = 0;

    // Status
    virtual nlohmann::json getStatus() const = 0;
    virtual std::vector<std::string> getKnownDeviceAddresses() const = 0;
};
```

## Protocol States

```cpp
enum class ProtocolState {
    Disconnected,  // Not connected
    Connecting,    // Connection in progress
    Connected,     // Connected and operational
    Reconnecting,  // Lost connection, attempting reconnect
    Error          // Error state
};
```

## Callbacks

Protocol handlers use callbacks to notify the DeviceManager of events:

### DeviceDiscoveredCallback

Called when a new device is discovered:

```cpp
using DeviceDiscoveredCallback = std::function<void(DevicePtr)>;

handler->setDeviceDiscoveredCallback([](DevicePtr device) {
    std::cout << "Discovered: " << device->name() << std::endl;
});
```

### DeviceStateCallback

Called when a device's state changes:

```cpp
using DeviceStateCallback = std::function<void(
    const std::string& deviceId,
    const std::string& property,
    const nlohmann::json& value
)>;

handler->setDeviceStateCallback([](const std::string& id,
                                    const std::string& prop,
                                    const nlohmann::json& val) {
    std::cout << id << "." << prop << " = " << val << std::endl;
});
```

### DeviceAvailabilityCallback

Called when a device's availability changes:

```cpp
using DeviceAvailabilityCallback = std::function<void(
    const std::string& deviceId,
    DeviceAvailability availability
)>;

handler->setDeviceAvailabilityCallback([](const std::string& id,
                                           DeviceAvailability avail) {
    std::cout << id << " is now " << (avail == DeviceAvailability::Online ? "online" : "offline") << std::endl;
});
```

## ProtocolFactory

The `ProtocolFactory` singleton creates protocol handler instances:

```cpp
#include <smarthub/protocols/ProtocolFactory.hpp>

auto& factory = ProtocolFactory::instance();

// Check available protocols
std::vector<std::string> protocols = factory.availableProtocols();

// Check if protocol exists
bool hasMqtt = factory.hasProtocol("mqtt");

// Get protocol info
auto info = factory.getProtocolInfo("mqtt");
// info.name, info.version, info.description

// Create protocol handler
EventBus eventBus;
nlohmann::json config = {{"broker", "localhost"}, {"port", 1883}};
auto handler = factory.create("mqtt", eventBus, config);
```

## Registering Protocol Handlers

### Using the Registration Macro

```cpp
#include <smarthub/protocols/ProtocolFactory.hpp>

class MyProtocolHandler : public IProtocolHandler {
public:
    MyProtocolHandler(EventBus& eventBus, const nlohmann::json& config);
    // ... implementation
};

// Auto-register at static initialization
REGISTER_PROTOCOL("my_protocol", MyProtocolHandler, "1.0.0", "My custom protocol")
```

### Manual Registration

```cpp
ProtocolFactory::instance().registerProtocol(
    "my_protocol",
    [](EventBus& eb, const nlohmann::json& cfg) -> ProtocolHandlerPtr {
        return std::make_shared<MyProtocolHandler>(eb, cfg);
    },
    {"my_protocol", "1.0.0", "My custom protocol"}
);
```

## Implementing a Protocol Handler

Example skeleton for a custom protocol handler:

```cpp
#include <smarthub/protocols/IProtocolHandler.hpp>
#include <smarthub/core/EventBus.hpp>

class CustomProtocolHandler : public IProtocolHandler {
public:
    CustomProtocolHandler(EventBus& eventBus, const nlohmann::json& config)
        : m_eventBus(eventBus)
        , m_config(config)
    {
    }

    std::string name() const override { return "custom"; }
    std::string version() const override { return "1.0.0"; }
    std::string description() const override { return "Custom protocol"; }

    bool initialize() override {
        // Connect to external service
        // Return true on success
        m_state = ProtocolState::Connected;
        return true;
    }

    void shutdown() override {
        // Disconnect from external service
        m_state = ProtocolState::Disconnected;
    }

    void poll() override {
        // Check for incoming messages
        // Process them and invoke callbacks
    }

    ProtocolState state() const override { return m_state; }
    bool isConnected() const override { return m_state == ProtocolState::Connected; }
    std::string lastError() const override { return m_lastError; }

    bool supportsDiscovery() const override { return true; }

    void startDiscovery() override {
        m_discovering = true;
        // Start discovery process
    }

    void stopDiscovery() override {
        m_discovering = false;
    }

    bool isDiscovering() const override { return m_discovering; }

    bool sendCommand(const std::string& address,
                     const std::string& command,
                     const nlohmann::json& params) override {
        // Send command to device
        // Return true on success
        return true;
    }

    void setDeviceDiscoveredCallback(DeviceDiscoveredCallback cb) override {
        m_discoveredCallback = cb;
    }

    void setDeviceStateCallback(DeviceStateCallback cb) override {
        m_stateCallback = cb;
    }

    void setDeviceAvailabilityCallback(DeviceAvailabilityCallback cb) override {
        m_availabilityCallback = cb;
    }

    nlohmann::json getStatus() const override {
        return {
            {"connected", isConnected()},
            {"discovering", m_discovering}
        };
    }

    std::vector<std::string> getKnownDeviceAddresses() const override {
        return m_knownAddresses;
    }

protected:
    // Call this when a device is discovered
    void onDeviceDiscovered(DevicePtr device) {
        if (m_discoveredCallback) {
            m_discoveredCallback(device);
        }
    }

    // Call this when device state changes
    void onStateChange(const std::string& id, const std::string& prop,
                       const nlohmann::json& val) {
        if (m_stateCallback) {
            m_stateCallback(id, prop, val);
        }
    }

    // Call this when device availability changes
    void onAvailabilityChange(const std::string& id, DeviceAvailability avail) {
        if (m_availabilityCallback) {
            m_availabilityCallback(id, avail);
        }
    }

private:
    EventBus& m_eventBus;
    nlohmann::json m_config;
    ProtocolState m_state = ProtocolState::Disconnected;
    std::string m_lastError;
    bool m_discovering = false;
    std::vector<std::string> m_knownAddresses;

    DeviceDiscoveredCallback m_discoveredCallback;
    DeviceStateCallback m_stateCallback;
    DeviceAvailabilityCallback m_availabilityCallback;
};

// Register the protocol
REGISTER_PROTOCOL("custom", CustomProtocolHandler, "1.0.0", "Custom protocol handler")
```

## DeviceManager Integration

The DeviceManager uses protocol handlers:

```cpp
#include <smarthub/devices/DeviceManager.hpp>

DeviceManager manager(eventBus, database);
manager.initialize();

// Load a protocol
nlohmann::json mqttConfig = {{"broker", "localhost"}, {"port", 1883}};
manager.loadProtocol("mqtt", mqttConfig);

// Get loaded protocols
std::vector<std::string> loaded = manager.loadedProtocols();

// Start discovery on all protocols
manager.startDiscovery();

// Or on a specific protocol
manager.startDiscovery("mqtt");

// Poll all protocols (call in main loop)
manager.poll();

// Set device state (routes through appropriate protocol)
manager.setDeviceState("light1", "on", true);

// Unload protocol
manager.unloadProtocol("mqtt");
```

## Testing with MockProtocolHandler

For unit testing, use the MockProtocolHandler:

```cpp
#include "mocks/MockProtocolHandler.hpp"

MockProtocolHandler handler(eventBus, {});

// Initialize
handler.initialize();
EXPECT_TRUE(handler.isConnected());

// Simulate device discovery
auto device = std::make_shared<Device>("dev1", "Device 1", DeviceType::Switch);
handler.simulateDeviceDiscovered(device);

// Simulate state change
handler.simulateStateChange("dev1", "on", true);

// Simulate availability change
handler.simulateAvailabilityChange("dev1", DeviceAvailability::Offline);

// Verify commands were sent
handler.sendCommand("addr", "set", {{"on", true}});
EXPECT_EQ(handler.lastCommand, "set");
EXPECT_EQ(handler.commandCount, 1);
```

## Protocol Configuration

Each protocol can define its own configuration schema. Example for MQTT:

```json
{
    "broker": "localhost",
    "port": 1883,
    "username": "user",
    "password": "secret",
    "client_id": "smarthub",
    "topics": {
        "discovery": "homeassistant/+/+/config",
        "state": "zigbee2mqtt/+"
    }
}
```

## Error Handling

Protocol handlers should:

1. Set `m_lastError` with descriptive error messages
2. Transition to `ProtocolState::Error` on critical failures
3. Attempt automatic reconnection when appropriate
4. Log errors using the Logger

```cpp
bool initialize() override {
    if (!connect()) {
        m_lastError = "Failed to connect to broker";
        m_state = ProtocolState::Error;
        LOG_ERROR("Protocol initialization failed: %s", m_lastError.c_str());
        return false;
    }
    return true;
}
```

## See Also

- [Device Abstraction Layer](devices.md) - Device types and management
- [MQTT Protocol](mqtt.md) - MQTT-specific documentation
- [Architecture](architecture.md) - System architecture
- [Testing](testing.md) - Testing guide
