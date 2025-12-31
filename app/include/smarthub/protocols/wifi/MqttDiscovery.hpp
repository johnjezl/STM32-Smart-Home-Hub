/**
 * SmartHub MQTT Discovery Protocol
 *
 * Implements Home Assistant MQTT Discovery format used by Tasmota, ESPHome,
 * and other smart devices for automatic device discovery.
 *
 * Discovery topic format: <discovery_prefix>/<component>/<node_id>/<object_id>/config
 * Default discovery prefix: homeassistant
 *
 * References:
 * - https://www.home-assistant.io/docs/mqtt/discovery/
 * - https://tasmota.github.io/docs/MQTT/
 * - https://esphome.io/components/mqtt.html
 */
#pragma once

#include <string>
#include <optional>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>

namespace smarthub::wifi {

/**
 * Device class types from Home Assistant MQTT discovery
 */
enum class MqttDeviceClass {
    Unknown,
    Switch,
    Light,
    Outlet,
    Sensor,
    BinarySensor,
    Climate,
    Cover,
    Fan,
    Lock,
    Button,
    Number,
    Select,
    Text
};

/**
 * Parsed MQTT discovery payload
 */
struct MqttDiscoveryConfig {
    // Required fields
    std::string uniqueId;
    std::string name;
    std::string component;        // switch, light, sensor, etc.

    // Topics
    std::string stateTopic;
    std::string commandTopic;
    std::string availabilityTopic;

    // Payloads
    std::string payloadOn = "ON";
    std::string payloadOff = "OFF";
    std::string payloadAvailable = "online";
    std::string payloadNotAvailable = "offline";

    // Value processing
    std::string valueTemplate;
    std::string stateValueTemplate;
    std::string unitOfMeasurement;

    // For lights
    std::string brightnessCommandTopic;
    std::string brightnessStateTopic;
    std::string colorTempCommandTopic;
    std::string colorTempStateTopic;
    std::string rgbCommandTopic;
    std::string rgbStateTopic;
    int brightnessScale = 255;
    int minMireds = 153;
    int maxMireds = 500;

    // Device information
    struct DeviceInfo {
        std::string identifiers;
        std::string manufacturer;
        std::string model;
        std::string name;
        std::string swVersion;
    } device;

    // Source identification
    enum class Source { Unknown, Tasmota, ESPHome, Zigbee2MQTT, Other };
    Source source = Source::Unknown;
    std::string nodeId;
    std::string objectId;

    // Original JSON for extension parsing
    nlohmann::json raw;

    /**
     * Parse discovery payload from topic and JSON message
     * @param topic Discovery topic (e.g., homeassistant/switch/tasmota_001/config)
     * @param payload JSON payload
     * @return Parsed config or nullopt if invalid
     */
    static std::optional<MqttDiscoveryConfig> parse(const std::string& topic,
                                                     const std::string& payload);

    /**
     * Get device class from component type
     */
    MqttDeviceClass deviceClass() const;

    /**
     * Check if this is a Tasmota device
     */
    bool isTasmota() const { return source == Source::Tasmota; }

    /**
     * Check if this is an ESPHome device
     */
    bool isESPHome() const { return source == Source::ESPHome; }
};

// Callback types
using DiscoveryCallback = std::function<void(const MqttDiscoveryConfig&)>;
using StateCallback = std::function<void(const std::string& uniqueId,
                                         const std::string& property,
                                         const nlohmann::json& value)>;
using AvailabilityCallback = std::function<void(const std::string& uniqueId, bool available)>;

/**
 * MQTT Discovery manager
 *
 * Subscribes to discovery topics and parses device configurations.
 * Handles both Tasmota and ESPHome discovery formats.
 */
class MqttDiscoveryManager {
public:
    MqttDiscoveryManager() = default;
    ~MqttDiscoveryManager() = default;

    /**
     * Set the discovery prefix (default: "homeassistant")
     */
    void setDiscoveryPrefix(const std::string& prefix) { m_discoveryPrefix = prefix; }

    /**
     * Get discovery subscription topic
     * Returns topic pattern to subscribe to (e.g., "homeassistant/#")
     */
    std::string getSubscriptionTopic() const { return m_discoveryPrefix + "/#"; }

    /**
     * Check if a topic is a discovery topic
     */
    bool isDiscoveryTopic(const std::string& topic) const;

    /**
     * Check if a topic matches a known state topic
     */
    bool isStateTopic(const std::string& topic) const;

    /**
     * Check if a topic matches a known availability topic
     */
    bool isAvailabilityTopic(const std::string& topic) const;

    /**
     * Process an incoming MQTT message
     * @param topic MQTT topic
     * @param payload Message payload
     */
    void processMessage(const std::string& topic, const std::string& payload);

    /**
     * Set callback for discovered devices
     */
    void setDiscoveryCallback(DiscoveryCallback cb) { m_discoveryCallback = std::move(cb); }

    /**
     * Set callback for state updates
     */
    void setStateCallback(StateCallback cb) { m_stateCallback = std::move(cb); }

    /**
     * Set callback for availability updates
     */
    void setAvailabilityCallback(AvailabilityCallback cb) { m_availabilityCallback = std::move(cb); }

    /**
     * Get discovered device configuration
     */
    std::optional<MqttDiscoveryConfig> getDevice(const std::string& uniqueId) const;

    /**
     * Get all discovered devices
     */
    std::vector<std::string> getDiscoveredDeviceIds() const;

    /**
     * Remove a device from tracking
     */
    void removeDevice(const std::string& uniqueId);

    /**
     * Clear all discovered devices
     */
    void clear();

private:
    void handleDiscovery(const std::string& topic, const std::string& payload);
    void handleState(const std::string& topic, const std::string& payload);
    void handleAvailability(const std::string& topic, const std::string& payload);

    std::string m_discoveryPrefix = "homeassistant";
    std::map<std::string, MqttDiscoveryConfig> m_devices;  // uniqueId -> config
    std::map<std::string, std::string> m_stateTopicMap;    // topic -> uniqueId
    std::map<std::string, std::string> m_availabilityTopicMap;

    DiscoveryCallback m_discoveryCallback;
    StateCallback m_stateCallback;
    AvailabilityCallback m_availabilityCallback;
};

} // namespace smarthub::wifi
