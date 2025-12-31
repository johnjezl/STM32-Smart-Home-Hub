/**
 * SmartHub MQTT Discovery Implementation
 */

#include <smarthub/protocols/wifi/MqttDiscovery.hpp>
#include <smarthub/core/Logger.hpp>
#include <algorithm>
#include <sstream>

namespace smarthub::wifi {

namespace {

// Helper to split string by delimiter
std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        result.push_back(item);
    }
    return result;
}

// Helper to detect source from JSON
MqttDiscoveryConfig::Source detectSource(const nlohmann::json& j) {
    // Check for Tasmota signatures
    if (j.contains("device") && j["device"].contains("sw")) {
        std::string sw = j["device"]["sw"].get<std::string>();
        if (sw.find("Tasmota") != std::string::npos) {
            return MqttDiscoveryConfig::Source::Tasmota;
        }
    }

    // Check for tasmota topic pattern
    if (j.contains("state_topic")) {
        std::string topic = j["state_topic"].get<std::string>();
        if (topic.find("tele/") != std::string::npos ||
            topic.find("stat/") != std::string::npos) {
            return MqttDiscoveryConfig::Source::Tasmota;
        }
    }

    // Check for ESPHome signatures
    if (j.contains("device") && j["device"].contains("sw_version")) {
        std::string sw = j["device"]["sw_version"].get<std::string>();
        if (sw.find("esphome") != std::string::npos ||
            sw.find("ESPHome") != std::string::npos) {
            return MqttDiscoveryConfig::Source::ESPHome;
        }
    }

    // Check for Zigbee2MQTT
    if (j.contains("device") && j["device"].contains("manufacturer")) {
        std::string mfr = j["device"]["manufacturer"].get<std::string>();
        if (j.contains("via_device")) {
            std::string via = j["via_device"].get<std::string>();
            if (via.find("zigbee2mqtt") != std::string::npos) {
                return MqttDiscoveryConfig::Source::Zigbee2MQTT;
            }
        }
    }

    return MqttDiscoveryConfig::Source::Unknown;
}

} // anonymous namespace

std::optional<MqttDiscoveryConfig> MqttDiscoveryConfig::parse(
    const std::string& topic,
    const std::string& payload) {

    // Parse topic: <prefix>/<component>/<node_id>/<object_id>/config
    // or: <prefix>/<component>/<node_id>/config (short form)
    auto parts = split(topic, '/');
    if (parts.size() < 4 || parts.back() != "config") {
        return std::nullopt;
    }

    // Parse JSON payload
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(payload);
    } catch (const std::exception& e) {
        LOG_WARN("Failed to parse discovery JSON: %s", e.what());
        return std::nullopt;
    }

    // Empty payload means device removal
    if (j.empty() || (j.is_object() && j.size() == 0)) {
        return std::nullopt;
    }

    MqttDiscoveryConfig config;
    config.raw = j;
    config.component = parts[1];

    // Parse node_id and object_id from topic
    if (parts.size() == 5) {
        config.nodeId = parts[2];
        config.objectId = parts[3];
    } else if (parts.size() == 4) {
        config.nodeId = parts[2];
        config.objectId = "";
    }

    // Required: unique_id or generate from topic
    if (j.contains("unique_id")) {
        config.uniqueId = j["unique_id"].get<std::string>();
    } else if (j.contains("uniq_id")) {
        config.uniqueId = j["uniq_id"].get<std::string>();
    } else {
        config.uniqueId = config.nodeId + "_" + config.objectId;
    }

    // Name
    if (j.contains("name")) {
        config.name = j["name"].get<std::string>();
    } else {
        config.name = config.uniqueId;
    }

    // Topics
    auto getStr = [&j](const std::string& key) -> std::string {
        if (j.contains(key) && j[key].is_string()) {
            return j[key].get<std::string>();
        }
        return "";
    };

    config.stateTopic = getStr("state_topic");
    if (config.stateTopic.empty()) config.stateTopic = getStr("stat_t");

    config.commandTopic = getStr("command_topic");
    if (config.commandTopic.empty()) config.commandTopic = getStr("cmd_t");

    config.availabilityTopic = getStr("availability_topic");
    if (config.availabilityTopic.empty()) config.availabilityTopic = getStr("avty_t");

    // Check for availability array
    if (j.contains("availability") && j["availability"].is_array()) {
        for (const auto& av : j["availability"]) {
            if (av.contains("topic")) {
                config.availabilityTopic = av["topic"].get<std::string>();
                break;
            }
        }
    }

    // Payloads
    if (j.contains("payload_on")) config.payloadOn = j["payload_on"].get<std::string>();
    if (j.contains("pl_on")) config.payloadOn = j["pl_on"].get<std::string>();

    if (j.contains("payload_off")) config.payloadOff = j["payload_off"].get<std::string>();
    if (j.contains("pl_off")) config.payloadOff = j["pl_off"].get<std::string>();

    if (j.contains("payload_available")) config.payloadAvailable = j["payload_available"].get<std::string>();
    if (j.contains("pl_avail")) config.payloadAvailable = j["pl_avail"].get<std::string>();

    if (j.contains("payload_not_available")) config.payloadNotAvailable = j["payload_not_available"].get<std::string>();
    if (j.contains("pl_not_avail")) config.payloadNotAvailable = j["pl_not_avail"].get<std::string>();

    // Value templates
    config.valueTemplate = getStr("value_template");
    if (config.valueTemplate.empty()) config.valueTemplate = getStr("val_tpl");

    config.stateValueTemplate = getStr("state_value_template");

    config.unitOfMeasurement = getStr("unit_of_measurement");
    if (config.unitOfMeasurement.empty()) config.unitOfMeasurement = getStr("unit_of_meas");

    // Light-specific fields
    config.brightnessCommandTopic = getStr("brightness_command_topic");
    if (config.brightnessCommandTopic.empty()) config.brightnessCommandTopic = getStr("bri_cmd_t");

    config.brightnessStateTopic = getStr("brightness_state_topic");
    if (config.brightnessStateTopic.empty()) config.brightnessStateTopic = getStr("bri_stat_t");

    config.colorTempCommandTopic = getStr("color_temp_command_topic");
    if (config.colorTempCommandTopic.empty()) config.colorTempCommandTopic = getStr("clr_temp_cmd_t");

    config.colorTempStateTopic = getStr("color_temp_state_topic");
    if (config.colorTempStateTopic.empty()) config.colorTempStateTopic = getStr("clr_temp_stat_t");

    config.rgbCommandTopic = getStr("rgb_command_topic");
    if (config.rgbCommandTopic.empty()) config.rgbCommandTopic = getStr("rgb_cmd_t");

    config.rgbStateTopic = getStr("rgb_state_topic");
    if (config.rgbStateTopic.empty()) config.rgbStateTopic = getStr("rgb_stat_t");

    if (j.contains("brightness_scale")) config.brightnessScale = j["brightness_scale"].get<int>();
    if (j.contains("bri_scl")) config.brightnessScale = j["bri_scl"].get<int>();

    if (j.contains("min_mireds")) config.minMireds = j["min_mireds"].get<int>();
    if (j.contains("min_mirs")) config.minMireds = j["min_mirs"].get<int>();

    if (j.contains("max_mireds")) config.maxMireds = j["max_mireds"].get<int>();
    if (j.contains("max_mirs")) config.maxMireds = j["max_mirs"].get<int>();

    // Device info
    if (j.contains("device") && j["device"].is_object()) {
        const auto& dev = j["device"];

        auto getDevStr = [&dev](const std::string& key) -> std::string {
            if (dev.contains(key) && dev[key].is_string()) {
                return dev[key].get<std::string>();
            }
            return "";
        };

        config.device.identifiers = getDevStr("identifiers");
        if (config.device.identifiers.empty() && dev.contains("identifiers") && dev["identifiers"].is_array()) {
            if (!dev["identifiers"].empty()) {
                config.device.identifiers = dev["identifiers"][0].get<std::string>();
            }
        }
        if (config.device.identifiers.empty()) config.device.identifiers = getDevStr("ids");

        config.device.manufacturer = getDevStr("manufacturer");
        if (config.device.manufacturer.empty()) config.device.manufacturer = getDevStr("mf");

        config.device.model = getDevStr("model");
        if (config.device.model.empty()) config.device.model = getDevStr("mdl");

        config.device.name = getDevStr("name");
        config.device.swVersion = getDevStr("sw_version");
        if (config.device.swVersion.empty()) config.device.swVersion = getDevStr("sw");
    }

    // Detect source
    config.source = detectSource(j);

    return config;
}

MqttDeviceClass MqttDiscoveryConfig::deviceClass() const {
    if (component == "switch") return MqttDeviceClass::Switch;
    if (component == "light") return MqttDeviceClass::Light;
    if (component == "sensor") return MqttDeviceClass::Sensor;
    if (component == "binary_sensor") return MqttDeviceClass::BinarySensor;
    if (component == "climate") return MqttDeviceClass::Climate;
    if (component == "cover") return MqttDeviceClass::Cover;
    if (component == "fan") return MqttDeviceClass::Fan;
    if (component == "lock") return MqttDeviceClass::Lock;
    if (component == "button") return MqttDeviceClass::Button;
    if (component == "number") return MqttDeviceClass::Number;
    if (component == "select") return MqttDeviceClass::Select;
    if (component == "text") return MqttDeviceClass::Text;
    return MqttDeviceClass::Unknown;
}

// MqttDiscoveryManager implementation

bool MqttDiscoveryManager::isDiscoveryTopic(const std::string& topic) const {
    return topic.find(m_discoveryPrefix + "/") == 0 &&
           topic.rfind("/config") == topic.size() - 7;
}

bool MqttDiscoveryManager::isStateTopic(const std::string& topic) const {
    return m_stateTopicMap.find(topic) != m_stateTopicMap.end();
}

bool MqttDiscoveryManager::isAvailabilityTopic(const std::string& topic) const {
    return m_availabilityTopicMap.find(topic) != m_availabilityTopicMap.end();
}

void MqttDiscoveryManager::processMessage(const std::string& topic, const std::string& payload) {
    if (isDiscoveryTopic(topic)) {
        handleDiscovery(topic, payload);
    } else if (isStateTopic(topic)) {
        handleState(topic, payload);
    } else if (isAvailabilityTopic(topic)) {
        handleAvailability(topic, payload);
    }
}

void MqttDiscoveryManager::handleDiscovery(const std::string& topic, const std::string& payload) {
    auto config = MqttDiscoveryConfig::parse(topic, payload);
    if (!config) {
        // Empty payload means device removal
        if (payload.empty() || payload == "{}") {
            // Try to extract uniqueId from topic to remove device
            auto parts = split(topic, '/');
            if (parts.size() >= 4) {
                std::string possibleId = parts[2];
                if (parts.size() >= 5) {
                    possibleId = parts[2] + "_" + parts[3];
                }
                removeDevice(possibleId);
            }
        }
        return;
    }

    LOG_INFO("MQTT Discovery: %s (%s) [%s]",
             config->name.c_str(),
             config->uniqueId.c_str(),
             config->component.c_str());

    // Store device config
    m_devices[config->uniqueId] = *config;

    // Register topic mappings for state updates
    if (!config->stateTopic.empty()) {
        m_stateTopicMap[config->stateTopic] = config->uniqueId;
    }
    if (!config->brightnessStateTopic.empty()) {
        m_stateTopicMap[config->brightnessStateTopic] = config->uniqueId;
    }
    if (!config->colorTempStateTopic.empty()) {
        m_stateTopicMap[config->colorTempStateTopic] = config->uniqueId;
    }
    if (!config->rgbStateTopic.empty()) {
        m_stateTopicMap[config->rgbStateTopic] = config->uniqueId;
    }

    // Register availability topic
    if (!config->availabilityTopic.empty()) {
        m_availabilityTopicMap[config->availabilityTopic] = config->uniqueId;
    }

    // Notify callback
    if (m_discoveryCallback) {
        m_discoveryCallback(*config);
    }
}

void MqttDiscoveryManager::handleState(const std::string& topic, const std::string& payload) {
    auto it = m_stateTopicMap.find(topic);
    if (it == m_stateTopicMap.end()) {
        return;
    }

    const std::string& uniqueId = it->second;
    auto deviceIt = m_devices.find(uniqueId);
    if (deviceIt == m_devices.end()) {
        return;
    }

    const auto& config = deviceIt->second;

    // Determine which property this is
    std::string property = "state";
    nlohmann::json value;

    // Try to parse as JSON first
    try {
        value = nlohmann::json::parse(payload);
    } catch (...) {
        // Not JSON, use as string
        value = payload;
    }

    // Determine property based on topic
    if (topic == config.stateTopic) {
        property = "state";
        // Convert ON/OFF to boolean if applicable
        if (payload == config.payloadOn) {
            value = true;
        } else if (payload == config.payloadOff) {
            value = false;
        }
    } else if (topic == config.brightnessStateTopic) {
        property = "brightness";
    } else if (topic == config.colorTempStateTopic) {
        property = "colorTemp";
    } else if (topic == config.rgbStateTopic) {
        property = "rgb";
    }

    LOG_DEBUG("MQTT state update: %s.%s = %s",
              uniqueId.c_str(), property.c_str(), payload.c_str());

    if (m_stateCallback) {
        m_stateCallback(uniqueId, property, value);
    }
}

void MqttDiscoveryManager::handleAvailability(const std::string& topic, const std::string& payload) {
    auto it = m_availabilityTopicMap.find(topic);
    if (it == m_availabilityTopicMap.end()) {
        return;
    }

    const std::string& uniqueId = it->second;
    auto deviceIt = m_devices.find(uniqueId);
    if (deviceIt == m_devices.end()) {
        return;
    }

    const auto& config = deviceIt->second;
    bool available = (payload == config.payloadAvailable);

    LOG_DEBUG("MQTT availability: %s = %s", uniqueId.c_str(), available ? "online" : "offline");

    if (m_availabilityCallback) {
        m_availabilityCallback(uniqueId, available);
    }
}

std::optional<MqttDiscoveryConfig> MqttDiscoveryManager::getDevice(const std::string& uniqueId) const {
    auto it = m_devices.find(uniqueId);
    if (it != m_devices.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<std::string> MqttDiscoveryManager::getDiscoveredDeviceIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_devices.size());
    for (const auto& [id, _] : m_devices) {
        ids.push_back(id);
    }
    return ids;
}

void MqttDiscoveryManager::removeDevice(const std::string& uniqueId) {
    auto it = m_devices.find(uniqueId);
    if (it == m_devices.end()) {
        return;
    }

    const auto& config = it->second;

    // Remove topic mappings
    if (!config.stateTopic.empty()) {
        m_stateTopicMap.erase(config.stateTopic);
    }
    if (!config.brightnessStateTopic.empty()) {
        m_stateTopicMap.erase(config.brightnessStateTopic);
    }
    if (!config.colorTempStateTopic.empty()) {
        m_stateTopicMap.erase(config.colorTempStateTopic);
    }
    if (!config.rgbStateTopic.empty()) {
        m_stateTopicMap.erase(config.rgbStateTopic);
    }
    if (!config.availabilityTopic.empty()) {
        m_availabilityTopicMap.erase(config.availabilityTopic);
    }

    m_devices.erase(it);
    LOG_INFO("MQTT device removed: %s", uniqueId.c_str());
}

void MqttDiscoveryManager::clear() {
    m_devices.clear();
    m_stateTopicMap.clear();
    m_availabilityTopicMap.clear();
}

} // namespace smarthub::wifi
