/**
 * SmartHub Device Manager Implementation
 */

#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/devices/Device.hpp>
#include <smarthub/devices/DeviceTypeRegistry.hpp>
#include <smarthub/protocols/ProtocolFactory.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/database/Database.hpp>
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

    loadDevicesFromDatabase();
    setupEventHandlers();

    LOG_INFO("Device manager initialized with %zu devices", m_devices.size());
    return true;
}

void DeviceManager::shutdown() {
    LOG_INFO("Shutting down device manager");

    // Stop discovery
    stopDiscovery();

    // Save device states
    saveAllDevices();

    // Shutdown all protocols
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [name, protocol] : m_protocols) {
        LOG_DEBUG("Shutting down protocol: %s", name.c_str());
        protocol->shutdown();
    }
    m_protocols.clear();
    m_devices.clear();
}

void DeviceManager::poll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [name, protocol] : m_protocols) {
        protocol->poll();
    }
}

bool DeviceManager::loadProtocol(const std::string& name, const nlohmann::json& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_protocols.find(name) != m_protocols.end()) {
        LOG_WARN("Protocol %s already loaded", name.c_str());
        return false;
    }

    auto protocol = ProtocolFactory::instance().create(name, m_eventBus, config);
    if (!protocol) {
        LOG_ERROR("Failed to create protocol: %s", name.c_str());
        return false;
    }

    // Set up callbacks
    protocol->setDeviceDiscoveredCallback(
        [this](DevicePtr device) {
            onDeviceDiscovered(device);
        });

    protocol->setDeviceStateCallback(
        [this](const std::string& id, const std::string& prop, const nlohmann::json& val) {
            onDeviceStateChanged(id, prop, val);
        });

    protocol->setDeviceAvailabilityCallback(
        [this](const std::string& id, DeviceAvailability avail) {
            onDeviceAvailabilityChanged(id, avail);
        });

    if (!protocol->initialize()) {
        LOG_ERROR("Failed to initialize protocol: %s", name.c_str());
        return false;
    }

    m_protocols[name] = protocol;
    LOG_INFO("Loaded protocol: %s v%s", name.c_str(), protocol->version().c_str());

    return true;
}

void DeviceManager::unloadProtocol(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_protocols.find(name);
    if (it != m_protocols.end()) {
        it->second->shutdown();
        m_protocols.erase(it);
        LOG_INFO("Unloaded protocol: %s", name.c_str());
    }
}

std::vector<std::string> DeviceManager::loadedProtocols() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> names;
    names.reserve(m_protocols.size());
    for (const auto& [name, protocol] : m_protocols) {
        names.push_back(name);
    }
    return names;
}

IProtocolHandler* DeviceManager::getProtocol(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_protocols.find(name);
    if (it != m_protocols.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool DeviceManager::addDevice(DevicePtr device) {
    if (!device) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_devices.count(device->id()) > 0) {
        LOG_WARN("Device %s already exists", device->id().c_str());
        return false;
    }

    m_devices[device->id()] = device;
    LOG_INFO("Added device: %s (%s)", device->name().c_str(), device->id().c_str());

    // Publish device added event
    DeviceStateEvent event;
    event.deviceId = device->id();
    event.property = "_added";
    event.value = device->toJson();
    m_eventBus.publish(event);

    return true;
}

bool DeviceManager::removeDevice(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_devices.find(id);
    if (it == m_devices.end()) {
        return false;
    }

    LOG_INFO("Removing device: %s", id.c_str());

    // Publish device removed event
    DeviceStateEvent event;
    event.deviceId = id;
    event.property = "_removed";
    m_eventBus.publish(event);

    m_devices.erase(it);
    return true;
}

DevicePtr DeviceManager::getDevice(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_devices.find(id);
    if (it != m_devices.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<DevicePtr> DeviceManager::getAllDevices() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<DevicePtr> result;
    result.reserve(m_devices.size());

    for (const auto& [id, device] : m_devices) {
        result.push_back(device);
    }

    return result;
}

std::vector<DevicePtr> DeviceManager::getDevicesByRoom(const std::string& room) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<DevicePtr> result;

    for (const auto& [id, device] : m_devices) {
        if (device->room() == room) {
            result.push_back(device);
        }
    }

    return result;
}

std::vector<DevicePtr> DeviceManager::getDevicesByType(DeviceType type) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<DevicePtr> result;

    for (const auto& [id, device] : m_devices) {
        if (device->type() == type) {
            result.push_back(device);
        }
    }

    return result;
}

std::vector<DevicePtr> DeviceManager::getDevicesByProtocol(const std::string& protocol) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<DevicePtr> result;

    for (const auto& [id, device] : m_devices) {
        if (device->protocol() == protocol) {
            result.push_back(device);
        }
    }

    return result;
}

size_t DeviceManager::deviceCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_devices.size();
}

void DeviceManager::startDiscovery(const std::string& protocol) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_discovering = true;

    if (protocol.empty()) {
        // Start discovery on all protocols
        for (auto& [name, handler] : m_protocols) {
            if (handler->supportsDiscovery()) {
                LOG_INFO("Starting discovery on protocol: %s", name.c_str());
                handler->startDiscovery();
            }
        }
    } else {
        // Start discovery on specific protocol
        auto it = m_protocols.find(protocol);
        if (it != m_protocols.end() && it->second->supportsDiscovery()) {
            LOG_INFO("Starting discovery on protocol: %s", protocol.c_str());
            it->second->startDiscovery();
        }
    }
}

void DeviceManager::stopDiscovery() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [name, handler] : m_protocols) {
        if (handler->isDiscovering()) {
            LOG_INFO("Stopping discovery on protocol: %s", name.c_str());
            handler->stopDiscovery();
        }
    }

    m_discovering = false;
}

bool DeviceManager::isDiscovering() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_discovering;
}

bool DeviceManager::setDeviceState(const std::string& deviceId, const std::string& property,
                                   const nlohmann::json& value) {
    DevicePtr device;
    ProtocolHandlerPtr protocol;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto devIt = m_devices.find(deviceId);
        if (devIt == m_devices.end()) {
            LOG_WARN("Device not found: %s", deviceId.c_str());
            return false;
        }
        device = devIt->second;

        auto protoIt = m_protocols.find(device->protocol());
        if (protoIt != m_protocols.end()) {
            protocol = protoIt->second;
        }
    }

    if (protocol) {
        // Send command through protocol handler
        nlohmann::json params;
        params[property] = value;
        return protocol->sendCommand(device->protocolAddress(), "set", params);
    } else {
        // Direct state update (for local/virtual devices)
        return device->setState(property, value);
    }
}

bool DeviceManager::saveAllDevices() {
    std::lock_guard<std::mutex> lock(m_mutex);

    LOG_DEBUG("Saving %zu devices to database", m_devices.size());

    for (const auto& [id, device] : m_devices) {
        try {
            // Save device info
            auto stmt = m_database.prepare(
                "INSERT OR REPLACE INTO devices "
                "(id, name, type, protocol, protocol_address, room, config) "
                "VALUES (?, ?, ?, ?, ?, ?, ?)");

            if (stmt && stmt->isValid()) {
                stmt->bind(1, device->id());
                stmt->bind(2, device->name());
                stmt->bind(3, device->typeString());
                stmt->bind(4, device->protocol());
                stmt->bind(5, device->protocolAddress());
                stmt->bind(6, device->room());
                stmt->bind(7, device->getConfig().dump());
                stmt->execute();
            }

            // Save current state
            nlohmann::json state = device->getState();
            for (auto& [prop, val] : state.items()) {
                auto stateStmt = m_database.prepare(
                    "INSERT OR REPLACE INTO device_state "
                    "(device_id, property, value, updated_at) "
                    "VALUES (?, ?, ?, strftime('%s', 'now'))");

                if (stateStmt && stateStmt->isValid()) {
                    stateStmt->bind(1, device->id());
                    stateStmt->bind(2, prop);
                    stateStmt->bind(3, val.dump());
                    stateStmt->execute();
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to save device %s: %s", id.c_str(), e.what());
        }
    }

    return true;
}

void DeviceManager::loadDevicesFromDatabase() {
    LOG_INFO("Loading devices from database");

    try {
        auto stmt = m_database.prepare(
            "SELECT id, name, type, protocol, protocol_address, room, config FROM devices");

        if (!stmt || !stmt->isValid()) {
            LOG_WARN("Failed to prepare device query");
            return;
        }

        while (stmt->step()) {
            std::string id = stmt->getString(0);
            std::string name = stmt->getString(1);
            std::string typeName = stmt->getString(2);
            std::string protocol = stmt->getString(3);
            std::string address = stmt->getString(4);
            std::string room = stmt->getString(5);
            std::string configStr = stmt->isNull(6) ? "{}" : stmt->getString(6);

            nlohmann::json config;
            try {
                config = nlohmann::json::parse(configStr);
            } catch (...) {
                config = nlohmann::json::object();
            }

            // Create device using registry
            auto device = DeviceTypeRegistry::instance().createFromTypeName(
                typeName, id, name, protocol, address, config);

            if (device) {
                device->setRoom(room);
                m_devices[id] = device;

                LOG_DEBUG("Loaded device: %s (%s)", name.c_str(), id.c_str());
            }
        }

        // Load device states
        for (auto& [id, device] : m_devices) {
            auto stateStmt = m_database.prepare(
                "SELECT property, value FROM device_state WHERE device_id = ?");

            if (stateStmt && stateStmt->isValid()) {
                stateStmt->bind(1, id);

                while (stateStmt->step()) {
                    std::string property = stateStmt->getString(0);
                    std::string valueStr = stateStmt->isNull(1) ? "null" : stateStmt->getString(1);

                    try {
                        nlohmann::json value = nlohmann::json::parse(valueStr);
                        device->setState(property, value);
                    } catch (...) {
                        // Ignore parse errors
                    }
                }
            }
        }

        LOG_INFO("Loaded %zu devices from database", m_devices.size());

    } catch (const std::exception& e) {
        LOG_WARN("Failed to load devices from database: %s", e.what());
    }
}

void DeviceManager::setupEventHandlers() {
    // Subscribe to MQTT messages for device updates
    m_eventBus.subscribe("mqtt.message", [this](const Event& event) {
        (void)event;
        // Protocol handlers will call onDeviceStateChanged directly
    });

    // Subscribe to sensor data from M4
    m_eventBus.subscribe("sensor.data", [this](const Event& event) {
        (void)event;
        // RPMsg handler will call onDeviceStateChanged directly
    });
}

void DeviceManager::onDeviceDiscovered(DevicePtr device) {
    LOG_INFO("Device discovered: %s (%s)", device->name().c_str(), device->id().c_str());
    addDevice(device);
}

void DeviceManager::onDeviceStateChanged(const std::string& deviceId,
                                          const std::string& property,
                                          const nlohmann::json& value) {
    DevicePtr device;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_devices.find(deviceId);
        if (it != m_devices.end()) {
            device = it->second;
        }
    }

    if (device) {
        device->setState(property, value);
        device->updateLastSeen();

        // Publish state change event
        DeviceStateEvent event;
        event.deviceId = deviceId;
        event.property = property;
        event.value = value;
        m_eventBus.publish(event);
    }
}

void DeviceManager::onDeviceAvailabilityChanged(const std::string& deviceId,
                                                 DeviceAvailability availability) {
    DevicePtr device;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_devices.find(deviceId);
        if (it != m_devices.end()) {
            device = it->second;
        }
    }

    if (device) {
        // Cast to Device to access setAvailability
        auto* devPtr = dynamic_cast<Device*>(device.get());
        if (devPtr) {
            devPtr->setAvailability(availability);
        }

        // Publish availability change event
        DeviceStateEvent event;
        event.deviceId = deviceId;
        event.property = "_availability";
        event.value = (availability == DeviceAvailability::Online) ? "online" : "offline";
        m_eventBus.publish(event);

        LOG_INFO("Device %s is now %s", deviceId.c_str(),
                 (availability == DeviceAvailability::Online) ? "online" : "offline");
    }
}

} // namespace smarthub
