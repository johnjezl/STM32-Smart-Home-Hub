# Phase 3: Device Integration Framework - Detailed TODO

## Overview
**Duration**: 2 weeks  
**Objective**: Create the modular plugin architecture for device types and protocols.  
**Prerequisites**: Phase 2 complete (core application framework)

---

## 3.1 Device Abstraction Layer

### 3.1.1 Base Device Interface
```cpp
// app/include/smarthub/devices/IDevice.hpp
#pragma once

#include <string>
#include <map>
#include <any>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

namespace smarthub {

// Device capabilities
enum class DeviceCapability {
    OnOff,
    Brightness,
    ColorTemperature,
    ColorRGB,
    Temperature,
    Humidity,
    Pressure,
    Motion,
    Contact,
    Battery,
    Power,
    Energy,
    Voltage,
    Current
};

// Device type enumeration
enum class DeviceType {
    Switch,
    Light,
    Dimmer,
    ColorLight,
    Outlet,
    Thermostat,
    TemperatureSensor,
    HumiditySensor,
    MotionSensor,
    ContactSensor,
    PowerMeter,
    Unknown
};

class IDevice {
public:
    virtual ~IDevice() = default;
    
    // Identity
    virtual std::string id() const = 0;
    virtual std::string name() const = 0;
    virtual void setName(const std::string& name) = 0;
    virtual DeviceType type() const = 0;
    virtual std::string typeString() const = 0;
    
    // Protocol info
    virtual std::string protocol() const = 0;
    virtual std::string protocolAddress() const = 0;
    
    // Capabilities
    virtual std::vector<DeviceCapability> capabilities() const = 0;
    virtual bool hasCapability(DeviceCapability cap) const = 0;
    
    // State management
    virtual nlohmann::json getState() const = 0;
    virtual bool setState(const std::string& property, const nlohmann::json& value) = 0;
    virtual nlohmann::json getProperty(const std::string& property) const = 0;
    
    // Availability
    virtual bool isAvailable() const = 0;
    virtual uint64_t lastSeen() const = 0;
    
    // Configuration
    virtual nlohmann::json getConfig() const = 0;
    virtual void setConfig(const nlohmann::json& config) = 0;
    
    // Serialization
    virtual nlohmann::json toJson() const = 0;
};

// Shared pointer type alias
using DevicePtr = std::shared_ptr<IDevice>;

} // namespace smarthub
```

### 3.1.2 Base Device Implementation
```cpp
// app/include/smarthub/devices/Device.hpp
#pragma once

#include <smarthub/devices/IDevice.hpp>
#include <mutex>

namespace smarthub {

class Device : public IDevice {
public:
    Device(const std::string& id, const std::string& name, DeviceType type,
           const std::string& protocol, const std::string& protocolAddress);
    
    // IDevice implementation
    std::string id() const override { return m_id; }
    std::string name() const override { return m_name; }
    void setName(const std::string& name) override { m_name = name; }
    DeviceType type() const override { return m_type; }
    std::string typeString() const override;
    
    std::string protocol() const override { return m_protocol; }
    std::string protocolAddress() const override { return m_protocolAddress; }
    
    std::vector<DeviceCapability> capabilities() const override { return m_capabilities; }
    bool hasCapability(DeviceCapability cap) const override;
    
    nlohmann::json getState() const override;
    bool setState(const std::string& property, const nlohmann::json& value) override;
    nlohmann::json getProperty(const std::string& property) const override;
    
    bool isAvailable() const override { return m_available; }
    uint64_t lastSeen() const override { return m_lastSeen; }
    
    nlohmann::json getConfig() const override { return m_config; }
    void setConfig(const nlohmann::json& config) override { m_config = config; }
    
    nlohmann::json toJson() const override;
    
protected:
    // Called when state changes - implement in derived classes
    virtual void onStateChange(const std::string& property, const nlohmann::json& value);
    
    void addCapability(DeviceCapability cap);
    void setAvailable(bool available);
    void updateLastSeen();
    void setStateInternal(const std::string& property, const nlohmann::json& value);
    
    std::string m_id;
    std::string m_name;
    DeviceType m_type;
    std::string m_protocol;
    std::string m_protocolAddress;
    std::string m_room;
    
    std::vector<DeviceCapability> m_capabilities;
    std::map<std::string, nlohmann::json> m_state;
    nlohmann::json m_config;
    
    bool m_available = false;
    uint64_t m_lastSeen = 0;
    
    mutable std::mutex m_mutex;
};

} // namespace smarthub
```

### 3.1.3 Specific Device Types
```cpp
// app/include/smarthub/devices/types/SwitchDevice.hpp
#pragma once

#include <smarthub/devices/Device.hpp>

namespace smarthub {

class SwitchDevice : public Device {
public:
    SwitchDevice(const std::string& id, const std::string& name,
                 const std::string& protocol, const std::string& protocolAddress);
    
    // Convenience methods
    bool isOn() const;
    void turnOn();
    void turnOff();
    void toggle();
    
protected:
    void onStateChange(const std::string& property, const nlohmann::json& value) override;
};

// app/include/smarthub/devices/types/DimmerDevice.hpp
class DimmerDevice : public Device {
public:
    DimmerDevice(const std::string& id, const std::string& name,
                 const std::string& protocol, const std::string& protocolAddress);
    
    bool isOn() const;
    int brightness() const;  // 0-100
    
    void turnOn();
    void turnOff();
    void setBrightness(int level);
    
protected:
    void onStateChange(const std::string& property, const nlohmann::json& value) override;
};

// app/include/smarthub/devices/types/ColorLightDevice.hpp
class ColorLightDevice : public DimmerDevice {
public:
    ColorLightDevice(const std::string& id, const std::string& name,
                     const std::string& protocol, const std::string& protocolAddress);
    
    // Color temperature (Kelvin, e.g., 2700-6500)
    int colorTemperature() const;
    void setColorTemperature(int kelvin);
    
    // RGB color
    struct RGB { uint8_t r, g, b; };
    RGB colorRGB() const;
    void setColorRGB(uint8_t r, uint8_t g, uint8_t b);
    
    // HSV color
    struct HSV { uint16_t h; uint8_t s, v; };
    HSV colorHSV() const;
    void setColorHSV(uint16_t h, uint8_t s, uint8_t v);
};

// app/include/smarthub/devices/types/TemperatureSensor.hpp
class TemperatureSensor : public Device {
public:
    TemperatureSensor(const std::string& id, const std::string& name,
                      const std::string& protocol, const std::string& protocolAddress);
    
    double temperature() const;  // Celsius
    double humidity() const;     // Percentage (if supported)
    int batteryLevel() const;    // Percentage (if supported)
};

// app/include/smarthub/devices/types/MotionSensor.hpp
class MotionSensor : public Device {
public:
    MotionSensor(const std::string& id, const std::string& name,
                 const std::string& protocol, const std::string& protocolAddress);
    
    bool motionDetected() const;
    uint64_t lastMotionTime() const;
    int batteryLevel() const;
};

} // namespace smarthub
```

---

## 3.2 Protocol Handler Interface

### 3.2.1 Protocol Handler Base
```cpp
// app/include/smarthub/protocols/IProtocolHandler.hpp
#pragma once

#include <smarthub/devices/IDevice.hpp>
#include <functional>
#include <string>
#include <vector>

namespace smarthub {

class EventBus;

// Callback when device is discovered
using DeviceDiscoveredCallback = std::function<void(DevicePtr)>;

// Callback when device state changes
using DeviceStateCallback = std::function<void(const std::string& deviceId, 
                                                const std::string& property,
                                                const nlohmann::json& value)>;

class IProtocolHandler {
public:
    virtual ~IProtocolHandler() = default;
    
    // Protocol identification
    virtual std::string name() const = 0;
    virtual std::string version() const = 0;
    
    // Lifecycle
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual void poll() = 0;  // Called from main loop
    
    // Discovery
    virtual bool supportsDiscovery() const = 0;
    virtual void startDiscovery() = 0;
    virtual void stopDiscovery() = 0;
    virtual bool isDiscovering() const = 0;
    
    // Device operations
    virtual bool sendCommand(const std::string& deviceAddress, 
                             const std::string& command,
                             const nlohmann::json& params) = 0;
    
    // Callbacks
    virtual void setDeviceDiscoveredCallback(DeviceDiscoveredCallback cb) = 0;
    virtual void setDeviceStateCallback(DeviceStateCallback cb) = 0;
    
    // Status
    virtual bool isConnected() const = 0;
    virtual nlohmann::json getStatus() const = 0;
};

using ProtocolHandlerPtr = std::shared_ptr<IProtocolHandler>;

} // namespace smarthub
```

### 3.2.2 Protocol Handler Factory
```cpp
// app/include/smarthub/protocols/ProtocolFactory.hpp
#pragma once

#include <smarthub/protocols/IProtocolHandler.hpp>
#include <map>
#include <functional>

namespace smarthub {

class EventBus;
class Config;

class ProtocolFactory {
public:
    using CreatorFunc = std::function<ProtocolHandlerPtr(EventBus&, const nlohmann::json&)>;
    
    static ProtocolFactory& instance();
    
    // Register protocol handler
    void registerProtocol(const std::string& name, CreatorFunc creator);
    
    // Create protocol handler
    ProtocolHandlerPtr create(const std::string& name, EventBus& eventBus, 
                               const nlohmann::json& config);
    
    // List available protocols
    std::vector<std::string> availableProtocols() const;
    
private:
    ProtocolFactory() = default;
    std::map<std::string, CreatorFunc> m_creators;
};

// Helper macro for auto-registration
#define REGISTER_PROTOCOL(name, className) \
    static bool _registered_##className = []() { \
        ProtocolFactory::instance().registerProtocol(name, \
            [](EventBus& eb, const nlohmann::json& cfg) { \
                return std::make_shared<className>(eb, cfg); \
            }); \
        return true; \
    }()

} // namespace smarthub
```

---

## 3.3 Device Manager

### 3.3.1 Device Manager Implementation
```cpp
// app/include/smarthub/devices/DeviceManager.hpp
#pragma once

#include <smarthub/devices/IDevice.hpp>
#include <smarthub/protocols/IProtocolHandler.hpp>
#include <map>
#include <vector>
#include <mutex>

namespace smarthub {

class EventBus;
class Database;

class DeviceManager {
public:
    DeviceManager(EventBus& eventBus, Database& database);
    ~DeviceManager();
    
    bool initialize();
    void shutdown();
    void poll();  // Called from main loop
    
    // Protocol management
    bool loadProtocol(const std::string& name, const nlohmann::json& config);
    void unloadProtocol(const std::string& name);
    std::vector<std::string> loadedProtocols() const;
    IProtocolHandler* getProtocol(const std::string& name);
    
    // Device management
    void addDevice(DevicePtr device);
    void removeDevice(const std::string& id);
    DevicePtr getDevice(const std::string& id) const;
    std::vector<DevicePtr> getAllDevices() const;
    std::vector<DevicePtr> getDevicesByType(DeviceType type) const;
    std::vector<DevicePtr> getDevicesByRoom(const std::string& room) const;
    std::vector<DevicePtr> getDevicesByProtocol(const std::string& protocol) const;
    
    // Discovery
    void startDiscovery(const std::string& protocol = "");
    void stopDiscovery();
    bool isDiscovering() const;
    
    // State management
    bool setDeviceState(const std::string& deviceId, const std::string& property,
                        const nlohmann::json& value);
    
    // Persistence
    void saveDevices();
    void loadDevices();
    
private:
    void onDeviceDiscovered(DevicePtr device);
    void onDeviceStateChanged(const std::string& deviceId, 
                              const std::string& property,
                              const nlohmann::json& value);
    
    EventBus& m_eventBus;
    Database& m_database;
    
    std::map<std::string, ProtocolHandlerPtr> m_protocols;
    std::map<std::string, DevicePtr> m_devices;
    
    mutable std::mutex m_mutex;
    bool m_discovering = false;
};

} // namespace smarthub
```

### 3.3.2 Device Manager Implementation
```cpp
// app/src/devices/DeviceManager.cpp
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/database/Database.hpp>
#include <smarthub/protocols/ProtocolFactory.hpp>
#include <smarthub/core/Logger.hpp>

namespace smarthub {

DeviceManager::DeviceManager(EventBus& eventBus, Database& database)
    : m_eventBus(eventBus)
    , m_database(database)
{
}

DeviceManager::~DeviceManager() {
    shutdown();
}

bool DeviceManager::initialize() {
    LOG_INFO("Initializing device manager");
    
    // Load saved devices from database
    loadDevices();
    
    return true;
}

void DeviceManager::shutdown() {
    // Save current device states
    saveDevices();
    
    // Shutdown all protocols
    for (auto& [name, protocol] : m_protocols) {
        protocol->shutdown();
    }
    m_protocols.clear();
}

void DeviceManager::poll() {
    for (auto& [name, protocol] : m_protocols) {
        protocol->poll();
    }
}

bool DeviceManager::loadProtocol(const std::string& name, const nlohmann::json& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_protocols.find(name) != m_protocols.end()) {
        LOG_WARN("Protocol {} already loaded", name);
        return false;
    }
    
    auto protocol = ProtocolFactory::instance().create(name, m_eventBus, config);
    if (!protocol) {
        LOG_ERROR("Failed to create protocol: {}", name);
        return false;
    }
    
    // Set callbacks
    protocol->setDeviceDiscoveredCallback(
        [this](DevicePtr device) { onDeviceDiscovered(device); });
    protocol->setDeviceStateCallback(
        [this](const std::string& id, const std::string& prop, const nlohmann::json& val) {
            onDeviceStateChanged(id, prop, val);
        });
    
    if (!protocol->initialize()) {
        LOG_ERROR("Failed to initialize protocol: {}", name);
        return false;
    }
    
    m_protocols[name] = protocol;
    LOG_INFO("Loaded protocol: {}", name);
    return true;
}

void DeviceManager::addDevice(DevicePtr device) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_devices[device->id()] = device;
    
    // Persist to database
    auto stmt = m_database.prepare(
        "INSERT OR REPLACE INTO devices (id, name, type, protocol, protocol_address, room, config) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)");
    
    stmt->bind(1, device->id());
    stmt->bind(2, device->name());
    stmt->bind(3, device->typeString());
    stmt->bind(4, device->protocol());
    stmt->bind(5, device->protocolAddress());
    stmt->bind(6, "");  // room
    stmt->bind(7, device->getConfig().dump());
    stmt->execute();
    
    // Publish event
    DeviceStateEvent event;
    event.deviceId = device->id();
    event.property = "_added";
    m_eventBus.publish(event);
    
    LOG_INFO("Added device: {} ({})", device->name(), device->id());
}

bool DeviceManager::setDeviceState(const std::string& deviceId, const std::string& property,
                                    const nlohmann::json& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_devices.find(deviceId);
    if (it == m_devices.end()) {
        LOG_WARN("Device not found: {}", deviceId);
        return false;
    }
    
    auto& device = it->second;
    
    // Find the protocol handler
    auto protocolIt = m_protocols.find(device->protocol());
    if (protocolIt == m_protocols.end()) {
        LOG_ERROR("Protocol not loaded: {}", device->protocol());
        return false;
    }
    
    // Build command based on property
    nlohmann::json params;
    params[property] = value;
    
    // Send command through protocol
    return protocolIt->second->sendCommand(device->protocolAddress(), "set", params);
}

void DeviceManager::onDeviceDiscovered(DevicePtr device) {
    LOG_INFO("Device discovered: {} ({})", device->name(), device->id());
    addDevice(device);
}

void DeviceManager::onDeviceStateChanged(const std::string& deviceId,
                                          const std::string& property,
                                          const nlohmann::json& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_devices.find(deviceId);
    if (it != m_devices.end()) {
        it->second->setState(property, value);
        
        // Store in database
        auto stmt = m_database.prepare(
            "INSERT OR REPLACE INTO device_state (device_id, property, value, updated_at) "
            "VALUES (?, ?, ?, strftime('%s', 'now'))");
        stmt->bind(1, deviceId);
        stmt->bind(2, property);
        stmt->bind(3, value.dump());
        stmt->execute();
        
        // Publish event
        DeviceStateEvent event;
        event.deviceId = deviceId;
        event.property = property;
        event.value = value;
        m_eventBus.publish(event);
    }
}

void DeviceManager::loadDevices() {
    LOG_INFO("Loading devices from database");
    
    m_database.query(
        "SELECT id, name, type, protocol, protocol_address, room, config FROM devices",
        [this](int cols, char** values, char** names) {
            // Create device based on type
            // This would be expanded to create appropriate device subclass
            std::string id = values[0] ? values[0] : "";
            std::string name = values[1] ? values[1] : "";
            std::string type = values[2] ? values[2] : "";
            std::string protocol = values[3] ? values[3] : "";
            std::string address = values[4] ? values[4] : "";
            
            // TODO: Create appropriate device type based on 'type' field
            // For now, create generic device
            
            return 0;
        });
}

} // namespace smarthub
```

---

## 3.4 Device Database Schema Extensions

### 3.4.1 Device Type Registry
```cpp
// app/include/smarthub/devices/DeviceTypeRegistry.hpp
#pragma once

#include <smarthub/devices/IDevice.hpp>
#include <functional>
#include <map>

namespace smarthub {

class DeviceTypeRegistry {
public:
    using DeviceCreator = std::function<DevicePtr(
        const std::string& id,
        const std::string& name,
        const std::string& protocol,
        const std::string& address,
        const nlohmann::json& config
    )>;
    
    static DeviceTypeRegistry& instance();
    
    void registerType(DeviceType type, const std::string& typeName, DeviceCreator creator);
    
    DevicePtr create(DeviceType type, const std::string& id, const std::string& name,
                     const std::string& protocol, const std::string& address,
                     const nlohmann::json& config);
    
    DevicePtr createFromTypeName(const std::string& typeName, const std::string& id,
                                  const std::string& name, const std::string& protocol,
                                  const std::string& address, const nlohmann::json& config);
    
    std::string getTypeName(DeviceType type) const;
    DeviceType getTypeFromName(const std::string& name) const;
    
private:
    DeviceTypeRegistry();
    
    struct TypeInfo {
        std::string name;
        DeviceCreator creator;
    };
    
    std::map<DeviceType, TypeInfo> m_types;
    std::map<std::string, DeviceType> m_nameToType;
};

} // namespace smarthub
```

---

## 3.5 Room and Group Management

### 3.5.1 Room Class
```cpp
// app/include/smarthub/devices/Room.hpp
#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace smarthub {

class Room {
public:
    Room(const std::string& id, const std::string& name);
    
    std::string id() const { return m_id; }
    std::string name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }
    
    std::string icon() const { return m_icon; }
    void setIcon(const std::string& icon) { m_icon = icon; }
    
    int sortOrder() const { return m_sortOrder; }
    void setSortOrder(int order) { m_sortOrder = order; }
    
    nlohmann::json toJson() const;
    static Room fromJson(const nlohmann::json& json);
    
private:
    std::string m_id;
    std::string m_name;
    std::string m_icon;
    int m_sortOrder = 0;
};

} // namespace smarthub
```

### 3.5.2 Device Group Class
```cpp
// app/include/smarthub/devices/DeviceGroup.hpp
#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace smarthub {

class DeviceGroup {
public:
    DeviceGroup(const std::string& id, const std::string& name);
    
    std::string id() const { return m_id; }
    std::string name() const { return m_name; }
    
    void addDevice(const std::string& deviceId);
    void removeDevice(const std::string& deviceId);
    std::vector<std::string> deviceIds() const { return m_deviceIds; }
    
    // Apply state to all devices in group
    nlohmann::json toJson() const;
    
private:
    std::string m_id;
    std::string m_name;
    std::vector<std::string> m_deviceIds;
};

} // namespace smarthub
```

---

## 3.6 Testing Framework

### 3.6.1 Mock Device Implementation
```cpp
// app/tests/mocks/MockDevice.hpp
#pragma once

#include <smarthub/devices/Device.hpp>

namespace smarthub::testing {

class MockDevice : public Device {
public:
    MockDevice(const std::string& id = "mock-001",
               const std::string& name = "Mock Device",
               DeviceType type = DeviceType::Switch)
        : Device(id, name, type, "mock", "mock://device")
    {
        addCapability(DeviceCapability::OnOff);
        setStateInternal("on", false);
        setAvailable(true);
    }
    
    // Track calls for testing
    int setStateCalls = 0;
    std::string lastProperty;
    nlohmann::json lastValue;
    
protected:
    void onStateChange(const std::string& property, const nlohmann::json& value) override {
        setStateCalls++;
        lastProperty = property;
        lastValue = value;
    }
};

} // namespace smarthub::testing
```

### 3.6.2 Mock Protocol Handler
```cpp
// app/tests/mocks/MockProtocolHandler.hpp
#pragma once

#include <smarthub/protocols/IProtocolHandler.hpp>

namespace smarthub::testing {

class MockProtocolHandler : public IProtocolHandler {
public:
    std::string name() const override { return "mock"; }
    std::string version() const override { return "1.0.0"; }
    
    bool initialize() override { initialized = true; return true; }
    void shutdown() override { initialized = false; }
    void poll() override { pollCount++; }
    
    bool supportsDiscovery() const override { return true; }
    void startDiscovery() override { discovering = true; }
    void stopDiscovery() override { discovering = false; }
    bool isDiscovering() const override { return discovering; }
    
    bool sendCommand(const std::string& address, const std::string& command,
                     const nlohmann::json& params) override {
        lastCommandAddress = address;
        lastCommand = command;
        lastParams = params;
        commandCount++;
        return commandResult;
    }
    
    void setDeviceDiscoveredCallback(DeviceDiscoveredCallback cb) override {
        discoveredCallback = cb;
    }
    void setDeviceStateCallback(DeviceStateCallback cb) override {
        stateCallback = cb;
    }
    
    bool isConnected() const override { return connected; }
    nlohmann::json getStatus() const override { return {}; }
    
    // Test helpers
    void simulateDeviceDiscovered(DevicePtr device) {
        if (discoveredCallback) discoveredCallback(device);
    }
    
    void simulateStateChange(const std::string& id, const std::string& prop,
                             const nlohmann::json& val) {
        if (stateCallback) stateCallback(id, prop, val);
    }
    
    // State for verification
    bool initialized = false;
    bool discovering = false;
    bool connected = true;
    bool commandResult = true;
    int pollCount = 0;
    int commandCount = 0;
    std::string lastCommandAddress;
    std::string lastCommand;
    nlohmann::json lastParams;
    
    DeviceDiscoveredCallback discoveredCallback;
    DeviceStateCallback stateCallback;
};

} // namespace smarthub::testing
```

### 3.6.3 Unit Tests
```cpp
// app/tests/unit/test_device_manager.cpp
#include <gtest/gtest.h>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/core/EventBus.hpp>
#include "mocks/MockDevice.hpp"
#include "mocks/MockProtocolHandler.hpp"

using namespace smarthub;
using namespace smarthub::testing;

class DeviceManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        eventBus = std::make_unique<EventBus>();
        // Use in-memory SQLite for testing
        database = std::make_unique<Database>(":memory:");
        database->initialize();
        manager = std::make_unique<DeviceManager>(*eventBus, *database);
    }
    
    std::unique_ptr<EventBus> eventBus;
    std::unique_ptr<Database> database;
    std::unique_ptr<DeviceManager> manager;
};

TEST_F(DeviceManagerTest, AddDevice) {
    auto device = std::make_shared<MockDevice>();
    manager->addDevice(device);
    
    EXPECT_EQ(manager->getAllDevices().size(), 1);
    EXPECT_EQ(manager->getDevice("mock-001"), device);
}

TEST_F(DeviceManagerTest, RemoveDevice) {
    auto device = std::make_shared<MockDevice>();
    manager->addDevice(device);
    manager->removeDevice("mock-001");
    
    EXPECT_EQ(manager->getAllDevices().size(), 0);
    EXPECT_EQ(manager->getDevice("mock-001"), nullptr);
}

TEST_F(DeviceManagerTest, GetDevicesByType) {
    manager->addDevice(std::make_shared<MockDevice>("d1", "Device 1", DeviceType::Switch));
    manager->addDevice(std::make_shared<MockDevice>("d2", "Device 2", DeviceType::Light));
    manager->addDevice(std::make_shared<MockDevice>("d3", "Device 3", DeviceType::Switch));
    
    auto switches = manager->getDevicesByType(DeviceType::Switch);
    EXPECT_EQ(switches.size(), 2);
}
```

---

## 3.7 Validation Checklist

Before proceeding to Phase 4, verify:

| Item | Status | Notes |
|------|--------|-------|
| IDevice interface complete | ☐ | |
| Device base class works | ☐ | |
| Device types implemented | ☐ | Switch, Dimmer, ColorLight, Sensors |
| IProtocolHandler interface complete | ☐ | |
| ProtocolFactory works | ☐ | |
| DeviceManager initializes | ☐ | |
| Device persistence works | ☐ | Save/load from DB |
| State changes propagate | ☐ | Via EventBus |
| Room management works | ☐ | |
| Unit tests pass | ☐ | |
| Mock implementations work | ☐ | |

---

## Time Estimates

| Task | Estimated Time |
|------|----------------|
| 3.1 Device Abstraction | 6-8 hours |
| 3.2 Protocol Handler Interface | 4-6 hours |
| 3.3 Device Manager | 6-8 hours |
| 3.4 Database Extensions | 2-3 hours |
| 3.5 Room/Group Management | 3-4 hours |
| 3.6 Testing Framework | 4-6 hours |
| 3.7 Validation | 2 hours |
| **Total** | **27-37 hours** |
