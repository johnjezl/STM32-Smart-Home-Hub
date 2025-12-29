/**
 * Zigbee Device Database
 *
 * Loads device definitions from JSON for mapping manufacturer/model
 * to device types and display names.
 */
#pragma once

#include <smarthub/devices/IDevice.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <map>
#include <optional>
#include <vector>

namespace smarthub::zigbee {

/**
 * Device definition entry
 */
struct ZigbeeDeviceEntry {
    std::string manufacturer;
    std::string model;
    std::string displayName;
    DeviceType deviceType = DeviceType::Unknown;

    // Optional overrides
    std::vector<uint16_t> supportedClusters;
    nlohmann::json quirks;  // Device-specific workarounds
};

/**
 * Database for looking up Zigbee device definitions
 */
class ZigbeeDeviceDatabase {
public:
    ZigbeeDeviceDatabase() = default;

    /**
     * Load device definitions from JSON file
     * @param path Path to zigbee_devices.json
     * @return true if loaded successfully
     */
    bool load(const std::string& path);

    /**
     * Load from JSON object directly
     */
    bool loadFromJson(const nlohmann::json& json);

    /**
     * Look up device by manufacturer and model
     * @return Device entry if found
     */
    std::optional<ZigbeeDeviceEntry> lookup(const std::string& manufacturer,
                                             const std::string& model) const;

    /**
     * Get all known devices
     */
    const std::vector<ZigbeeDeviceEntry>& devices() const { return m_devices; }

    /**
     * Get device count
     */
    size_t size() const { return m_devices.size(); }

    /**
     * Check if database is loaded
     */
    bool isLoaded() const { return m_loaded; }

    /**
     * Add a device entry programmatically
     */
    void addDevice(const ZigbeeDeviceEntry& entry);

private:
    std::vector<ZigbeeDeviceEntry> m_devices;

    // Index for fast lookup: "manufacturer:model" -> index
    std::map<std::string, size_t> m_index;

    bool m_loaded = false;

    std::string makeKey(const std::string& manufacturer, const std::string& model) const;
    DeviceType parseDeviceType(const std::string& typeStr) const;
};

} // namespace smarthub::zigbee
