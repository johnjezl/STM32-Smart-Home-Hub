/**
 * SmartHub Device Group Implementation
 */

#include <smarthub/devices/DeviceGroup.hpp>
#include <algorithm>

namespace smarthub {

DeviceGroup::DeviceGroup(const std::string& id, const std::string& name)
    : m_id(id)
    , m_name(name)
{
}

void DeviceGroup::addDevice(const std::string& deviceId) {
    if (!hasDevice(deviceId)) {
        m_deviceIds.push_back(deviceId);
    }
}

void DeviceGroup::removeDevice(const std::string& deviceId) {
    auto it = std::find(m_deviceIds.begin(), m_deviceIds.end(), deviceId);
    if (it != m_deviceIds.end()) {
        m_deviceIds.erase(it);
    }
}

bool DeviceGroup::hasDevice(const std::string& deviceId) const {
    return std::find(m_deviceIds.begin(), m_deviceIds.end(), deviceId) != m_deviceIds.end();
}

void DeviceGroup::clearDevices() {
    m_deviceIds.clear();
}

nlohmann::json DeviceGroup::toJson() const {
    return {
        {"id", m_id},
        {"name", m_name},
        {"icon", m_icon},
        {"sort_order", m_sortOrder},
        {"devices", m_deviceIds}
    };
}

DeviceGroup DeviceGroup::fromJson(const nlohmann::json& json) {
    std::string id = json.value("id", "");
    std::string name = json.value("name", "");

    DeviceGroup group(id, name);
    group.setIcon(json.value("icon", "group"));
    group.setSortOrder(json.value("sort_order", 0));

    if (json.contains("devices") && json["devices"].is_array()) {
        for (const auto& deviceId : json["devices"]) {
            if (deviceId.is_string()) {
                group.addDevice(deviceId.get<std::string>());
            }
        }
    }

    return group;
}

} // namespace smarthub
