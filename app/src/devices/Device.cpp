/**
 * SmartHub Device Implementation
 */

#include <smarthub/devices/Device.hpp>
#include <algorithm>
#include <chrono>

namespace smarthub {

Device::Device(const std::string& id,
               const std::string& name,
               DeviceType type,
               const std::string& protocol,
               const std::string& protocolAddress)
    : m_id(id)
    , m_name(name)
    , m_type(type)
    , m_protocol(protocol)
    , m_protocolAddress(protocolAddress)
{
    updateLastSeen();
}

std::string Device::name() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_name;
}

void Device::setName(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_name = name;
}

std::string Device::typeString() const {
    return deviceTypeToString(m_type);
}

std::string Device::room() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_room;
}

void Device::setRoom(const std::string& room) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_room = room;
}

std::vector<DeviceCapability> Device::capabilities() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_capabilities;
}

bool Device::hasCapability(DeviceCapability cap) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::find(m_capabilities.begin(), m_capabilities.end(), cap) != m_capabilities.end();
}

nlohmann::json Device::getState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    nlohmann::json state;
    for (const auto& [key, value] : m_state) {
        state[key] = value;
    }
    return state;
}

bool Device::setState(const std::string& property, const nlohmann::json& value) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state[property] = value;
    }

    // Callback without lock to avoid deadlocks
    onStateChange(property, value);
    notifyStateChange(property, value);

    return true;
}

nlohmann::json Device::getProperty(const std::string& property) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_state.find(property);
    if (it != m_state.end()) {
        return it->second;
    }
    return nullptr;
}

DeviceAvailability Device::availability() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_availability;
}

void Device::updateLastSeen() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastSeen = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
}

nlohmann::json Device::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void Device::setConfig(const nlohmann::json& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

nlohmann::json Device::toJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    nlohmann::json j;
    j["id"] = m_id;
    j["name"] = m_name;
    j["type"] = deviceTypeToString(m_type);
    j["protocol"] = m_protocol;
    j["protocol_address"] = m_protocolAddress;
    j["room"] = m_room;

    // Capabilities
    nlohmann::json caps = nlohmann::json::array();
    for (const auto& cap : m_capabilities) {
        caps.push_back(capabilityToString(cap));
    }
    j["capabilities"] = caps;

    // State
    nlohmann::json state;
    for (const auto& [key, value] : m_state) {
        state[key] = value;
    }
    j["state"] = state;

    // Availability
    switch (m_availability) {
        case DeviceAvailability::Online:  j["availability"] = "online"; break;
        case DeviceAvailability::Offline: j["availability"] = "offline"; break;
        default:                          j["availability"] = "unknown"; break;
    }

    j["last_seen"] = m_lastSeen;
    j["config"] = m_config;

    return j;
}

void Device::setStateCallback(StateCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stateCallback = std::move(callback);
}

void Device::setAvailability(DeviceAvailability availability) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_availability = availability;
    if (availability == DeviceAvailability::Online) {
        m_lastSeen = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
    }
}

std::shared_ptr<Device> Device::fromJson(const nlohmann::json& json) {
    std::string id = json.value("id", "");
    std::string name = json.value("name", "");
    DeviceType type = stringToDeviceType(json.value("type", "unknown"));
    std::string protocol = json.value("protocol", "local");
    std::string protocolAddress = json.value("protocol_address", "");

    auto device = std::make_shared<Device>(id, name, type, protocol, protocolAddress);

    if (json.contains("room")) {
        device->setRoom(json["room"].get<std::string>());
    }

    if (json.contains("config")) {
        device->setConfig(json["config"]);
    }

    if (json.contains("state") && json["state"].is_object()) {
        for (auto& [key, value] : json["state"].items()) {
            device->setStateInternal(key, value);
        }
    }

    if (json.contains("availability")) {
        std::string avail = json["availability"].get<std::string>();
        if (avail == "online") {
            device->setAvailability(DeviceAvailability::Online);
        } else if (avail == "offline") {
            device->setAvailability(DeviceAvailability::Offline);
        }
    }

    return device;
}

void Device::onStateChange(const std::string& /* property */, const nlohmann::json& /* value */) {
    // Default implementation does nothing
    // Derived classes can override for custom behavior
}

void Device::addCapability(DeviceCapability cap) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (std::find(m_capabilities.begin(), m_capabilities.end(), cap) == m_capabilities.end()) {
        m_capabilities.push_back(cap);
    }
}

void Device::setStateInternal(const std::string& property, const nlohmann::json& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state[property] = value;
}

void Device::notifyStateChange(const std::string& property, const nlohmann::json& value) {
    StateCallback callback;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        callback = m_stateCallback;
    }
    if (callback) {
        callback(property, value);
    }
}

} // namespace smarthub
