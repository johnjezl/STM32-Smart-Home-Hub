/**
 * SmartHub Device Implementation
 */

#include <smarthub/devices/Device.hpp>
#include <nlohmann/json.hpp>

namespace smarthub {

Device::Device(const std::string& id, const std::string& name, DeviceType type)
    : m_id(id)
    , m_name(name)
    , m_type(type)
{
}

std::any Device::getState(const std::string& property) const {
    auto it = m_state.find(property);
    if (it != m_state.end()) {
        return it->second;
    }
    return std::any{};
}

bool Device::setState(const std::string& property, const std::any& value) {
    m_state[property] = value;
    notifyStateChange(property, value);
    return true;
}

std::map<std::string, std::any> Device::getAllState() const {
    return m_state;
}

void Device::notifyStateChange(const std::string& property, const std::any& value) {
    if (m_stateCallback) {
        m_stateCallback(property, value);
    }
}

nlohmann::json Device::toJson() const {
    nlohmann::json j;
    j["id"] = m_id;
    j["name"] = m_name;
    j["type"] = typeToString(m_type);
    j["protocol"] = protocolToString(m_protocol);
    j["room"] = m_room;
    j["online"] = m_online;
    return j;
}

void Device::fromJson(const nlohmann::json& j) {
    if (j.contains("name")) {
        m_name = j["name"].get<std::string>();
    }
    if (j.contains("type")) {
        m_type = stringToType(j["type"].get<std::string>());
    }
    if (j.contains("protocol")) {
        m_protocol = stringToProtocol(j["protocol"].get<std::string>());
    }
    if (j.contains("room")) {
        m_room = j["room"].get<std::string>();
    }
    if (j.contains("online")) {
        m_online = j["online"].get<bool>();
    }
}

std::string Device::typeToString(DeviceType type) {
    switch (type) {
        case DeviceType::Light:      return "light";
        case DeviceType::Switch:     return "switch";
        case DeviceType::Sensor:     return "sensor";
        case DeviceType::Thermostat: return "thermostat";
        case DeviceType::Lock:       return "lock";
        case DeviceType::Camera:     return "camera";
        case DeviceType::Speaker:    return "speaker";
        case DeviceType::Custom:     return "custom";
        default:                     return "unknown";
    }
}

DeviceType Device::stringToType(const std::string& str) {
    if (str == "light")      return DeviceType::Light;
    if (str == "switch")     return DeviceType::Switch;
    if (str == "sensor")     return DeviceType::Sensor;
    if (str == "thermostat") return DeviceType::Thermostat;
    if (str == "lock")       return DeviceType::Lock;
    if (str == "camera")     return DeviceType::Camera;
    if (str == "speaker")    return DeviceType::Speaker;
    if (str == "custom")     return DeviceType::Custom;
    return DeviceType::Unknown;
}

std::string Device::protocolToString(DeviceProtocol protocol) {
    switch (protocol) {
        case DeviceProtocol::Local:   return "local";
        case DeviceProtocol::MQTT:    return "mqtt";
        case DeviceProtocol::Zigbee:  return "zigbee";
        case DeviceProtocol::ZWave:   return "zwave";
        case DeviceProtocol::WiFi:    return "wifi";
        case DeviceProtocol::Virtual: return "virtual";
        default:                      return "unknown";
    }
}

DeviceProtocol Device::stringToProtocol(const std::string& str) {
    if (str == "local")   return DeviceProtocol::Local;
    if (str == "mqtt")    return DeviceProtocol::MQTT;
    if (str == "zigbee")  return DeviceProtocol::Zigbee;
    if (str == "zwave")   return DeviceProtocol::ZWave;
    if (str == "wifi")    return DeviceProtocol::WiFi;
    if (str == "virtual") return DeviceProtocol::Virtual;
    return DeviceProtocol::Local;
}

} // namespace smarthub
