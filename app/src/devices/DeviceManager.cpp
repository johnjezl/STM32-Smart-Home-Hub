/**
 * SmartHub Device Manager Implementation
 */

#include <chrono>
#include <optional>
#include <set>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/devices/Device.hpp>
#include <smarthub/devices/DeviceTypeRegistry.hpp>
#include <smarthub/protocols/ProtocolFactory.hpp>
#include <smarthub/protocols/zigbee/ZigbeeHandler.hpp>
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

    {
        std::lock_guard<std::mutex> lock(m_stateWriteMutex);
        m_stateWriterStopping = false;
    }
    if (!m_stateWriter.joinable()) {
        m_stateWriter = std::thread(&DeviceManager::stateWriterLoop, this);
    }

    LOG_INFO("Device manager initialized with %zu devices", m_devices.size());
    return true;
}

void DeviceManager::shutdown() {
    LOG_INFO("Shutting down device manager");

    // Stop discovery
    stopDiscovery();

    // Everything's about to be saved, so it doesn't need the regular writes any more
    stopStateWriter();

    // Shutdown all protocols, unlocked: they join threads that may be waiting to
    // call back into this class, which takes m_mutex.
    decltype(m_protocols) protocols;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        protocols.swap(m_protocols);
    }
    for (auto& [name, protocol] : protocols) {
        LOG_DEBUG("Shutting down protocol: %s", name.c_str());
        protocol->shutdown();
    }

    // After the protocols have stopped, so this includes the last reports
    saveAllDevices();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_devices.clear();
}

void DeviceManager::poll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [name, protocol] : m_protocols) {
        protocol->poll();
    }
}

bool DeviceManager::loadProtocol(const std::string& name, const nlohmann::json& config) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_protocols.find(name) != m_protocols.end()) {
            LOG_WARN("Protocol %s already loaded", name.c_str());
            return false;
        }
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

    // Unlocked, for the same reason as in shutdown(): initialize can join threads
    // that are waiting to call back into this class.
    if (!protocol->initialize()) {
        LOG_ERROR("Failed to initialize protocol: %s", name.c_str());
        return false;
    }

    bool inserted = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        inserted = m_protocols.emplace(name, protocol).second;
    }
    if (!inserted) {
        LOG_WARN("Protocol %s already loaded", name.c_str());
        protocol->shutdown();
        return false;
    }

    LOG_INFO("Loaded protocol: %s v%s", name.c_str(), protocol->version().c_str());

    return true;
}

void DeviceManager::unloadProtocol(const std::string& name) {
    ProtocolHandlerPtr protocol;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_protocols.find(name);
        if (it == m_protocols.end()) {
            return;
        }
        protocol = it->second;
        m_protocols.erase(it);
    }

    // Unlocked, for the same reason as in shutdown()
    protocol->shutdown();
    LOG_INFO("Unloaded protocol: %s", name.c_str());
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
        LOG_ERROR("addDevice called with null device");
        return false;
    }

    const std::string deviceId = device->id();
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_devices.count(deviceId) > 0) {
            LOG_WARN("Device %s already exists", deviceId.c_str());
            return false;
        }

        m_devices[deviceId] = device;
        LOG_INFO("Added device: %s (%s)", device->name().c_str(), deviceId.c_str());

        saveDevice(device);
    }

    // Unlocked, because handlers can call back into this class
    try {
        DeviceStateEvent event;
        event.deviceId = deviceId;
        event.property = "_added";
        event.value = device->toJson();
        m_eventBus.publish(event);
    } catch (const std::exception& e) {
        LOG_ERROR("Exception publishing device event: %s", e.what());
    } catch (...) {
        LOG_ERROR("Unknown exception publishing device event");
    }

    return true;
}

bool DeviceManager::removeDevice(const std::string& id) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_devices.find(id);
        if (it == m_devices.end()) {
            return false;
        }

        LOG_INFO("Removing device: %s", id.c_str());

        {
            std::lock_guard<std::mutex> stateLock(m_stateWriteMutex);
            for (auto entry = m_unsavedState.begin(); entry != m_unsavedState.end();) {
                entry = entry->first.first == id ? m_unsavedState.erase(entry) : std::next(entry);
            }
        }

        deleteDeviceFromDatabase(id);
        m_devices.erase(it);
    }

    // Unlocked, because handlers can call back into this class
    try {
        DeviceStateEvent event;
        event.deviceId = id;
        event.property = "_removed";
        m_eventBus.publish(event);
    } catch (const std::exception& e) {
        LOG_ERROR("Exception publishing device event: %s", e.what());
    } catch (...) {
        LOG_ERROR("Unknown exception publishing device event");
    }

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
    // Copied so the handlers run unlocked: they wait on the radio, and that shouldn't hold up
    // every other use of this class.
    std::vector<std::pair<std::string, ProtocolHandlerPtr>> handlers;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_discovering = true;

        for (const auto& [name, handler] : m_protocols) {
            if ((protocol.empty() || name == protocol) && handler->supportsDiscovery()) {
                handlers.emplace_back(name, handler);
            }
        }
    }

    for (const auto& [name, handler] : handlers) {
        LOG_INFO("Starting discovery on protocol: %s", name.c_str());
        handler->startDiscovery();
    }
}

void DeviceManager::stopDiscovery() {
    std::vector<std::pair<std::string, ProtocolHandlerPtr>> handlers;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& [name, handler] : m_protocols) {
            handlers.emplace_back(name, handler);
        }
    }

    for (const auto& [name, handler] : handlers) {
        if (handler->isDiscovering()) {
            LOG_INFO("Stopping discovery on protocol: %s", name.c_str());
            handler->stopDiscovery();
        }
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_discovering = false;
}

bool DeviceManager::isDiscovering() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_discovering;
}

void DeviceManager::setPendingDeviceCallback(const std::string& protocol, DeviceDiscoveredCallback callback) {
    ProtocolHandlerPtr handler;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_protocols.find(protocol);
        if (it == m_protocols.end()) {
            LOG_WARN("Protocol %s not loaded", protocol.c_str());
            return;
        }
        handler = it->second;
    }

    // For Zigbee protocol, use the pending device callback mechanism. Unlocked, because this
    // waits for a call in progress, which could be calling back into this class.
    if (protocol == "zigbee") {
        auto* zigbee = dynamic_cast<zigbee::ZigbeeHandler*>(handler.get());
        if (zigbee) {
            zigbee->setPendingDeviceCallback(std::move(callback));
            LOG_DEBUG("Set pending device callback for Zigbee");
        }
    }
    // Other protocols can be added here in the future
}

DevicePtr DeviceManager::getPendingDevice(const std::string& protocol) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_protocols.find(protocol);
    if (it == m_protocols.end()) {
        return nullptr;
    }

    if (protocol == "zigbee") {
        auto* zigbee = dynamic_cast<zigbee::ZigbeeHandler*>(it->second.get());
        if (zigbee) {
            return zigbee->getPendingDevice();
        }
    }

    return nullptr;
}

void DeviceManager::clearPendingDevice(const std::string& protocol) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_protocols.find(protocol);
    if (it == m_protocols.end()) {
        return;
    }

    if (protocol == "zigbee") {
        auto* zigbee = dynamic_cast<zigbee::ZigbeeHandler*>(it->second.get());
        if (zigbee) {
            zigbee->clearPendingDevice();
            LOG_DEBUG("Cleared pending device for Zigbee");
        }
    }
}

namespace {

/** Whether a value has the right type for a property. Other properties can hold anything. */
bool isValidStateValue(const std::string& property, const nlohmann::json& value) {
    if (property == "on") return value.is_boolean();
    if (property == "brightness" || property == "colorTemp") return value.is_number();
    return true;
}

/** The protocol command that changes a property, if there is one. */
std::optional<std::string> commandForProperty(const std::string& property,
                                              const nlohmann::json& value) {
    if (property == "on") return value.get<bool>() ? "on" : "off";
    if (property == "brightness" || property == "colorTemp") return property;
    return std::nullopt;
}

/** Whether a device can have this property changed. Call for properties that have a command. */
bool canControl(const DevicePtr& device, const std::string& property) {
    if (property == "on") return device->hasCapability(DeviceCapability::OnOff);
    if (property == "brightness") return device->hasCapability(DeviceCapability::Brightness);
    return device->hasCapability(DeviceCapability::ColorTemperature);
}

} // namespace

// What the API can change on a device. Anything else on a device's state is
// reported by the device: a client writing it could fake a sensor reading, set
// off automations, or store a string the dashboard then has to display.
bool DeviceManager::isControllableProperty(const IDevice& device, const std::string& property,
                            const nlohmann::json& value) {
    struct Control {
        const char* property;
        DeviceCapability capability;
        double min;
        double max;
    };
    static const Control kNumeric[] = {
        {"brightness", DeviceCapability::Brightness, 0, 100},
        {"colorTemp", DeviceCapability::ColorTemperature, 1000, 10000},  // kelvin
        {"color_h", DeviceCapability::ColorHSV, 0, 360},
        {"color_s", DeviceCapability::ColorHSV, 0, 100},
        {"color_v", DeviceCapability::ColorHSV, 0, 100},
        {"color_r", DeviceCapability::ColorRGB, 0, 255},
        {"color_g", DeviceCapability::ColorRGB, 0, 255},
        {"color_b", DeviceCapability::ColorRGB, 0, 255},
    };

    if (property == "on") {
        return value.is_boolean() && device.hasCapability(DeviceCapability::OnOff);
    }
    if (!value.is_number()) {
        return false;
    }
    for (const Control& control : kNumeric) {
        if (property == control.property) {
            const double number = value.get<double>();
            return number >= control.min && number <= control.max &&
                   device.hasCapability(control.capability);
        }
    }
    return false;
}

bool DeviceManager::setDeviceState(const std::string& deviceId, const std::string& property,
                                   const nlohmann::json& value) {
    if (!isValidStateValue(property, value)) {
        LOG_WARN("Invalid value for %s.%s: %s", deviceId.c_str(), property.c_str(),
                 value.dump().c_str());
        return false;
    }

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

    // Unlocked: this can wait on the radio.
    if (protocol) {
        auto command = commandForProperty(property, value);
        if (!command) {
            LOG_WARN("%s can't change %s", device->protocol().c_str(), property.c_str());
            return false;
        }
        if (!canControl(device, property)) {
            LOG_WARN("%s doesn't support %s", deviceId.c_str(), property.c_str());
            return false;
        }

        nlohmann::json params;
        params[property] = value;
        if (!protocol->sendCommand(deviceId, *command, params)) {
            LOG_WARN("Failed to set %s.%s", deviceId.c_str(), property.c_str());
            return false;
        }
    } else if (device->protocol() != "local") {
        // Reporting success here would show a change that never reached the device
        LOG_WARN("Can't set %s.%s: protocol %s isn't loaded", deviceId.c_str(),
                 property.c_str(), device->protocol().c_str());
        return false;
    }

    applyStateChange(device, property, value);

    // Setting a level also switches a light on or off
    if (property == "brightness" && device->hasCapability(DeviceCapability::OnOff)) {
        applyStateChange(device, "on", value.get<double>() > 0);
    }
    return true;
}

void DeviceManager::applyStateChange(const DevicePtr& device, const std::string& property,
                                     const nlohmann::json& value) {
    // Only publish real changes. A device's report confirming a command isn't news, and an
    // automation shouldn't keep re-triggering itself.
    // Together, because commands and device reports can arrive on different threads
    nlohmann::json previous;
    {
        std::lock_guard<std::mutex> lock(m_stateChangeMutex);
        previous = device->getProperty(property);
        device->setState(property, value);
    }
    if (previous != value) {
        markStateChanged(device->id(), property, value);

        DeviceStateEvent event;
        event.deviceId = device->id();
        event.property = property;
        event.value = value;
        event.previousValue = previous;
        m_eventBus.publish(event);
    }
}

bool DeviceManager::saveAllDevices() {
    std::lock_guard<std::mutex> lock(m_mutex);

    LOG_DEBUG("Saving %zu devices to database", m_devices.size());

    bool saved = true;
    for (const auto& [id, device] : m_devices) {
        saved = saveDevice(device) && saved;
    }
    return saved;
}

bool DeviceManager::saveDevice(const DevicePtr& device) {
    if (!device) {
        return false;
    }

    try {
        // An upsert, because INSERT OR REPLACE deletes the row first, and that would delete the
        // device's state and history with it, and reset when it was created.
        auto stmt = m_database.prepare(
            "INSERT INTO devices (id, name, type, protocol, protocol_address, room, config) "
            "VALUES (?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(id) DO UPDATE SET "
            "name = excluded.name, type = excluded.type, protocol = excluded.protocol, "
            "protocol_address = excluded.protocol_address, room = excluded.room, "
            "config = excluded.config, updated_at = strftime('%s', 'now')");

        if (!stmt || !stmt->isValid()) {
            return false;
        }
        stmt->bind(1, device->id());
        stmt->bind(2, device->name());
        stmt->bind(3, device->typeString());
        stmt->bind(4, device->protocol());
        stmt->bind(5, device->protocolAddress());
        stmt->bind(6, device->room());
        // Ensure config is valid JSON object
        const auto config = device->getConfig();
        constexpr auto kReplaceBadText = nlohmann::json::error_handler_t::replace;
        stmt->bind(7, config.is_object() ? config.dump(-1, ' ', false, kReplaceBadText) : "{}");
        if (!stmt->execute()) {
            LOG_ERROR("Failed to save device %s", device->id().c_str());
            return false;
        }

        // Save current state
        std::vector<StateRow> rows;
        const nlohmann::json state = device->getState();
        for (const auto& [property, value] : state.items()) {
            rows.push_back({device->id(), property, value.dump(-1, ' ', false, kReplaceBadText)});
        }
        if (!writeStateRows(rows)) {
            return false;
        }

        LOG_DEBUG("Saved device to database: %s", device->id().c_str());
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to save device %s: %s", device->id().c_str(), e.what());
        return false;
    }
}

bool DeviceManager::writeStateRows(const std::vector<StateRow>& rows) {
    if (rows.empty()) {
        return true;
    }

    // One statement per batch, so each batch is one write to the SD card
    std::string sql = "INSERT INTO device_state (device_id, property, value, updated_at) VALUES ";
    for (size_t i = 0; i < rows.size(); ++i) {
        sql += i == 0 ? "(?, ?, ?, strftime('%s', 'now'))" : ", (?, ?, ?, strftime('%s', 'now'))";
    }
    sql += " ON CONFLICT(device_id, property) DO UPDATE SET "
           "value = excluded.value, updated_at = excluded.updated_at";

    auto stmt = m_database.prepare(sql);
    if (!stmt || !stmt->isValid()) {
        return false;
    }
    int parameter = 1;
    for (const StateRow& row : rows) {
        stmt->bind(parameter++, row.deviceId);
        stmt->bind(parameter++, row.property);
        stmt->bind(parameter++, row.value);
    }
    return stmt->execute();
}

void DeviceManager::markStateChanged(const std::string& deviceId, const std::string& property,
                                     const nlohmann::json& value) {
    // Devices can report text that isn't valid UTF-8, which a plain dump throws on
    std::string text = value.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    std::lock_guard<std::mutex> lock(m_stateWriteMutex);
    m_unsavedState[{deviceId, property}] = std::move(text);
}

bool DeviceManager::flushStateChanges() {
    std::map<std::pair<std::string, std::string>, std::string> unsaved;
    {
        std::lock_guard<std::mutex> lock(m_stateWriteMutex);
        unsaved.swap(m_unsavedState);
    }

    // SQLite allows a limited number of parameters in one statement, three to a row here
    constexpr size_t kRowsPerBatch = 500;
    bool saved = true;
    std::vector<StateRow> batch;
    const auto writeBatch = [&] {
        if (writeStateRows(batch)) {
            batch.clear();
            return;
        }
        // Probably a device removed since it changed, which shouldn't lose the others
        for (const StateRow& row : batch) {
            if (!writeStateRows({row})) {
                bool exists = false;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    exists = m_devices.count(row.deviceId) > 0;
                }
                if (exists) {
                    LOG_ERROR("Failed to save %s.%s", row.deviceId.c_str(), row.property.c_str());
                    saved = false;
                    // Try again next time, unless there's a newer value by then
                    std::lock_guard<std::mutex> lock(m_stateWriteMutex);
                    m_unsavedState.emplace(std::make_pair(row.deviceId, row.property), row.value);
                }
            }
        }
        batch.clear();
    };

    for (const auto& [key, value] : unsaved) {
        batch.push_back({key.first, key.second, value});
        if (batch.size() == kRowsPerBatch) {
            writeBatch();
        }
    }
    writeBatch();
    return saved;
}

void DeviceManager::stateWriterLoop() {
    std::unique_lock<std::mutex> lock(m_stateWriteMutex);
    while (!m_stateWriterStopping) {
        m_stateWriterWake.wait_for(lock, std::chrono::seconds(kStateSaveIntervalSeconds),
                                   [this] { return m_stateWriterStopping; });
        if (m_stateWriterStopping) {
            break;
        }
        lock.unlock();
        try {
            flushStateChanges();
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to save device state: %s", e.what());
        }
        lock.lock();
    }
}

void DeviceManager::stopStateWriter() {
    {
        std::lock_guard<std::mutex> lock(m_stateWriteMutex);
        m_stateWriterStopping = true;
    }
    m_stateWriterWake.notify_all();
    if (m_stateWriter.joinable()) {
        m_stateWriter.join();
    }
}

bool DeviceManager::deleteDeviceFromDatabase(const std::string& id) {
    try {
        // Delete device state
        auto stateStmt = m_database.prepare(
            "DELETE FROM device_state WHERE device_id = ?");
        if (stateStmt && stateStmt->isValid()) {
            stateStmt->bind(1, id);
            stateStmt->execute();
        }

        // Delete device
        auto stmt = m_database.prepare(
            "DELETE FROM devices WHERE id = ?");
        if (stmt && stmt->isValid()) {
            stmt->bind(1, id);
            stmt->execute();
        }

        LOG_DEBUG("Deleted device from database: %s", id.c_str());
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to delete device %s from database: %s", id.c_str(), e.what());
        return false;
    }
}

void DeviceManager::restoreSensorOptions(const std::string& id, std::string& typeName,
                                         nlohmann::json& config) {
    const bool sensor = typeName == "temperature_sensor" || typeName == "multi_sensor" ||
                        typeName == "motion_sensor";
    if (!sensor || config.contains("has_battery")) {
        return;
    }

    // Saved before sensors recorded their options, but what they've reported shows which they have
    std::set<std::string> properties;
    auto stmt = m_database.prepare("SELECT property FROM device_state WHERE device_id = ?");
    if (stmt && stmt->isValid()) {
        stmt->bind(1, id);
        while (stmt->step()) {
            properties.insert(stmt->getString(0));
        }
    }

    // A motion sensor with illuminance used to be saved as a multi sensor
    if (typeName == "multi_sensor" && properties.count("motion") > 0) {
        typeName = "motion_sensor";
    }

    config["has_battery"] = properties.count("battery") > 0;
    if (typeName == "motion_sensor") {
        config["has_illuminance"] = properties.count("illuminance") > 0;
    } else {
        config["has_humidity"] = typeName == "multi_sensor" || properties.count("humidity") > 0;
    }
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
                // Ensure config is an object, not null or other type
                if (!config.is_object()) {
                    config = nlohmann::json::object();
                }
            } catch (...) {
                config = nlohmann::json::object();
            }

            // One bad row shouldn't lose the others
            try {
                restoreSensorOptions(id, typeName, config);

                // Create device using registry
                auto device = DeviceTypeRegistry::instance().createFromTypeName(
                    typeName, id, name, protocol, address, config);

                if (device) {
                    device->setRoom(room);
                    m_devices[id] = device;

                    LOG_DEBUG("Loaded device: %s (%s)", name.c_str(), id.c_str());
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to load device %s: %s", id.c_str(), e.what());
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

// Room management

std::vector<std::pair<std::string, std::string>> DeviceManager::getAllRooms() const {
    std::vector<std::pair<std::string, std::string>> rooms;

    try {
        auto stmt = m_database.prepare(
            "SELECT id, name FROM rooms ORDER BY sort_order, name");

        if (stmt && stmt->isValid()) {
            while (stmt->step()) {
                std::string id = stmt->getString(0);
                std::string name = stmt->getString(1);
                rooms.emplace_back(id, name);
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get rooms: %s", e.what());
    }

    return rooms;
}

bool DeviceManager::addRoom(const std::string& id, const std::string& name) {
    try {
        auto stmt = m_database.prepare(
            "INSERT INTO rooms (id, name) VALUES (?, ?)");

        if (stmt && stmt->isValid()) {
            stmt->bind(1, id);
            stmt->bind(2, name);
            if (stmt->execute()) {
                LOG_INFO("Added room: %s (%s)", name.c_str(), id.c_str());
                return true;
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to add room %s: %s", id.c_str(), e.what());
    }

    return false;
}

bool DeviceManager::roomExists(const std::string& id) {
    auto stmt = m_database.prepare("SELECT 1 FROM rooms WHERE id = ?");
    if (!stmt || !stmt->isValid()) {
        return false;
    }
    stmt->bind(1, id);
    return stmt->step();
}

bool DeviceManager::updateRoom(const std::string& id, const std::string& name) {
    try {
        if (!roomExists(id)) {
            return false;
        }

        auto stmt = m_database.prepare("UPDATE rooms SET name = ? WHERE id = ?");
        if (stmt && stmt->isValid()) {
            stmt->bind(1, name);
            stmt->bind(2, id);
            if (stmt->execute()) {
                LOG_INFO("Updated room: %s -> %s", id.c_str(), name.c_str());
                return true;
            }
        }
        LOG_ERROR("Failed to update room %s", id.c_str());
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to update room %s: %s", id.c_str(), e.what());
    }

    return false;
}

bool DeviceManager::deleteRoom(const std::string& id) {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!roomExists(id)) {
            return false;
        }

        // The room first, so a failure can't leave devices unassigned from a room that's still there
        auto stmt = m_database.prepare("DELETE FROM rooms WHERE id = ?");
        if (!stmt || !stmt->isValid()) {
            return false;
        }
        stmt->bind(1, id);
        if (!stmt->execute()) {
            LOG_ERROR("Failed to delete room %s", id.c_str());
            return false;
        }

        auto updateStmt = m_database.prepare("UPDATE devices SET room = '' WHERE room = ?");
        if (updateStmt && updateStmt->isValid()) {
            updateStmt->bind(1, id);
            if (!updateStmt->execute()) {
                LOG_ERROR("Failed to clear room %s from saved devices", id.c_str());
            }
        }

        // And in memory, or the next save would put them back. This also corrects the database
        // on that save if the update above failed.
        for (const auto& [deviceId, device] : m_devices) {
            if (device->room() == id) {
                device->setRoom("");
            }
        }

        LOG_INFO("Deleted room: %s", id.c_str());
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to delete room %s: %s", id.c_str(), e.what());
    }

    return false;
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
        device->updateLastSeen();
        applyStateChange(device, property, value);
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
