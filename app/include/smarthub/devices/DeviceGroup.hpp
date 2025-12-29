/**
 * SmartHub Device Group
 *
 * Represents a group of devices that can be controlled together.
 */
#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace smarthub {

/**
 * DeviceGroup - represents a collection of devices that can be controlled as one
 */
class DeviceGroup {
public:
    /**
     * Constructor
     * @param id Unique group identifier
     * @param name Human-readable group name
     */
    DeviceGroup(const std::string& id, const std::string& name);

    /**
     * Get group ID
     */
    std::string id() const { return m_id; }

    /**
     * Get group name
     */
    std::string name() const { return m_name; }

    /**
     * Set group name
     */
    void setName(const std::string& name) { m_name = name; }

    /**
     * Get group icon (Material Design icon name)
     */
    std::string icon() const { return m_icon; }

    /**
     * Set group icon
     */
    void setIcon(const std::string& icon) { m_icon = icon; }

    /**
     * Get sort order for display
     */
    int sortOrder() const { return m_sortOrder; }

    /**
     * Set sort order
     */
    void setSortOrder(int order) { m_sortOrder = order; }

    /**
     * Add a device to the group
     */
    void addDevice(const std::string& deviceId);

    /**
     * Remove a device from the group
     */
    void removeDevice(const std::string& deviceId);

    /**
     * Check if a device is in the group
     */
    bool hasDevice(const std::string& deviceId) const;

    /**
     * Get all device IDs in the group
     */
    std::vector<std::string> deviceIds() const { return m_deviceIds; }

    /**
     * Clear all devices from the group
     */
    void clearDevices();

    /**
     * Get number of devices in the group
     */
    size_t deviceCount() const { return m_deviceIds.size(); }

    /**
     * Serialize to JSON
     */
    nlohmann::json toJson() const;

    /**
     * Deserialize from JSON
     */
    static DeviceGroup fromJson(const nlohmann::json& json);

private:
    std::string m_id;
    std::string m_name;
    std::string m_icon = "group";
    int m_sortOrder = 0;
    std::vector<std::string> m_deviceIds;
};

} // namespace smarthub
