/**
 * SmartHub Device Manager Implementation
 */

#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/devices/Device.hpp>
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
    loadDevicesFromDatabase();
    setupEventHandlers();

    LOG_INFO("Device manager initialized with %zu devices", m_devices.size());
    return true;
}

void DeviceManager::shutdown() {
    saveAllDevices();
    std::lock_guard<std::mutex> lock(m_mutex);
    m_devices.clear();
}

bool DeviceManager::addDevice(std::shared_ptr<Device> device) {
    if (!device) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_devices.count(device->id()) > 0) {
        LOG_WARN("Device %s already exists", device->id().c_str());
        return false;
    }

    // Set up state change callback
    device->setStateCallback([this, id = device->id()](const std::string& property, const std::any& value) {
        DeviceStateEvent event;
        event.deviceId = id;
        event.property = property;
        event.value = value;
        m_eventBus.publish(event);
    });

    m_devices[device->id()] = device;
    LOG_INFO("Added device: %s (%s)", device->name().c_str(), device->id().c_str());

    return true;
}

bool DeviceManager::removeDevice(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_devices.find(id);
    if (it == m_devices.end()) {
        return false;
    }

    LOG_INFO("Removing device: %s", id.c_str());
    m_devices.erase(it);
    return true;
}

std::shared_ptr<Device> DeviceManager::getDevice(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_devices.find(id);
    if (it != m_devices.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<Device>> DeviceManager::getAllDevices() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::shared_ptr<Device>> result;
    result.reserve(m_devices.size());

    for (const auto& [id, device] : m_devices) {
        result.push_back(device);
    }

    return result;
}

std::vector<std::shared_ptr<Device>> DeviceManager::getDevicesByRoom(const std::string& room) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::shared_ptr<Device>> result;

    for (const auto& [id, device] : m_devices) {
        if (device->room() == room) {
            result.push_back(device);
        }
    }

    return result;
}

std::vector<std::shared_ptr<Device>> DeviceManager::getDevicesByType(int type) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::shared_ptr<Device>> result;

    for (const auto& [id, device] : m_devices) {
        if (static_cast<int>(device->type()) == type) {
            result.push_back(device);
        }
    }

    return result;
}

size_t DeviceManager::deviceCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_devices.size();
}

bool DeviceManager::saveAllDevices() {
    // TODO: Implement database persistence
    LOG_DEBUG("Saving devices to database");
    return true;
}

void DeviceManager::loadDevicesFromDatabase() {
    // TODO: Load devices from database
    LOG_DEBUG("Loading devices from database");
}

void DeviceManager::setupEventHandlers() {
    // Subscribe to MQTT messages for device updates
    m_eventBus.subscribe("mqtt.message", [this](const Event& event) {
        // Handle MQTT device updates
        (void)event;
    });

    // Subscribe to sensor data from M4
    m_eventBus.subscribe("sensor.data", [this](const Event& event) {
        // Handle sensor data
        (void)event;
    });
}

} // namespace smarthub
