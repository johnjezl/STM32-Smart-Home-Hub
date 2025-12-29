/**
 * Zigbee Device Database Implementation
 */

#include <smarthub/protocols/zigbee/ZigbeeDeviceDatabase.hpp>
#include <smarthub/core/Logger.hpp>
#include <fstream>
#include <algorithm>

namespace smarthub::zigbee {

bool ZigbeeDeviceDatabase::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open device database: {}", path);
        return false;
    }

    try {
        nlohmann::json json;
        file >> json;
        return loadFromJson(json);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse device database: {}", e.what());
        return false;
    }
}

bool ZigbeeDeviceDatabase::loadFromJson(const nlohmann::json& json) {
    m_devices.clear();
    m_index.clear();

    if (!json.contains("devices") || !json["devices"].is_array()) {
        LOG_ERROR("Device database missing 'devices' array");
        return false;
    }

    for (const auto& entry : json["devices"]) {
        ZigbeeDeviceEntry device;

        // Required fields
        if (!entry.contains("manufacturer") || !entry.contains("model")) {
            LOG_WARN("Skipping device entry missing manufacturer/model");
            continue;
        }

        device.manufacturer = entry["manufacturer"].get<std::string>();
        device.model = entry["model"].get<std::string>();

        // Optional fields
        if (entry.contains("displayName")) {
            device.displayName = entry["displayName"].get<std::string>();
        } else {
            device.displayName = device.manufacturer + " " + device.model;
        }

        if (entry.contains("deviceType")) {
            device.deviceType = parseDeviceType(entry["deviceType"].get<std::string>());
        }

        if (entry.contains("clusters") && entry["clusters"].is_array()) {
            for (const auto& cluster : entry["clusters"]) {
                if (cluster.is_number()) {
                    device.supportedClusters.push_back(cluster.get<uint16_t>());
                }
            }
        }

        if (entry.contains("quirks")) {
            device.quirks = entry["quirks"];
        }

        addDevice(device);
    }

    m_loaded = true;
    LOG_INFO("Loaded {} Zigbee device definitions", m_devices.size());

    return true;
}

std::optional<ZigbeeDeviceEntry> ZigbeeDeviceDatabase::lookup(
    const std::string& manufacturer, const std::string& model) const {

    std::string key = makeKey(manufacturer, model);
    auto it = m_index.find(key);
    if (it != m_index.end()) {
        return m_devices[it->second];
    }

    // Try partial match on manufacturer
    for (const auto& device : m_devices) {
        // Check if manufacturer contains the search term (case insensitive)
        std::string devMfr = device.manufacturer;
        std::string searchMfr = manufacturer;
        std::transform(devMfr.begin(), devMfr.end(), devMfr.begin(), ::tolower);
        std::transform(searchMfr.begin(), searchMfr.end(), searchMfr.begin(), ::tolower);

        if (devMfr.find(searchMfr) != std::string::npos ||
            searchMfr.find(devMfr) != std::string::npos) {
            // Manufacturer matches, check model
            std::string devModel = device.model;
            std::string searchModel = model;
            std::transform(devModel.begin(), devModel.end(), devModel.begin(), ::tolower);
            std::transform(searchModel.begin(), searchModel.end(), searchModel.begin(), ::tolower);

            if (devModel == searchModel ||
                devModel.find(searchModel) != std::string::npos ||
                searchModel.find(devModel) != std::string::npos) {
                return device;
            }
        }
    }

    return std::nullopt;
}

void ZigbeeDeviceDatabase::addDevice(const ZigbeeDeviceEntry& entry) {
    std::string key = makeKey(entry.manufacturer, entry.model);

    // Check for duplicate
    if (m_index.find(key) != m_index.end()) {
        LOG_WARN("Duplicate device entry: {} {}", entry.manufacturer, entry.model);
        return;
    }

    m_index[key] = m_devices.size();
    m_devices.push_back(entry);
}

std::string ZigbeeDeviceDatabase::makeKey(const std::string& manufacturer,
                                           const std::string& model) const {
    // Normalize to lowercase for consistent lookup
    std::string key = manufacturer + ":" + model;
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    return key;
}

DeviceType ZigbeeDeviceDatabase::parseDeviceType(const std::string& typeStr) const {
    std::string lower = typeStr;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "switch" || lower == "outlet" || lower == "plug") {
        return DeviceType::Switch;
    } else if (lower == "dimmer" || lower == "dimmable") {
        return DeviceType::Dimmer;
    } else if (lower == "color_light" || lower == "colorlight" || lower == "rgbw") {
        return DeviceType::ColorLight;
    } else if (lower == "temperature" || lower == "temperature_sensor" || lower == "temp") {
        return DeviceType::TemperatureSensor;
    } else if (lower == "motion" || lower == "motion_sensor" || lower == "occupancy") {
        return DeviceType::MotionSensor;
    }

    return DeviceType::Unknown;
}

} // namespace smarthub::zigbee
