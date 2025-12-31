/**
 * SmartHub WiFi Protocol Handler
 *
 * Unified protocol handler for WiFi-based smart devices.
 * Supports:
 * - MQTT devices (Tasmota, ESPHome, generic MQTT discovery)
 * - HTTP devices (Shelly Gen1/Gen2)
 * - Tuya local protocol
 */
#pragma once

#include <smarthub/protocols/IProtocolHandler.hpp>
#include <smarthub/protocols/wifi/MqttDiscovery.hpp>
#include <smarthub/protocols/wifi/ShellyDevice.hpp>
#include <smarthub/protocols/wifi/TuyaDevice.hpp>
#include <smarthub/protocols/http/HttpClient.hpp>
#include <smarthub/protocols/mqtt/MqttClient.hpp>

#include <memory>
#include <map>
#include <mutex>

namespace smarthub::wifi {

/**
 * WiFi device entry tracking
 */
struct WifiDeviceEntry {
    std::string id;
    std::string type;  // "mqtt", "shelly", "tuya"
    DevicePtr device;
    bool available = false;

    // For MQTT devices
    MqttDiscoveryConfig mqttConfig;

    // For Shelly devices
    std::unique_ptr<ShellyDevice> shellyDevice;

    // For Tuya devices
    std::unique_ptr<TuyaDevice> tuyaDevice;
};

/**
 * WiFi Protocol Handler
 *
 * Implements IProtocolHandler to provide unified handling of
 * WiFi-based smart devices through MQTT, HTTP, and Tuya protocols.
 */
class WifiHandler : public IProtocolHandler {
public:
    WifiHandler(EventBus& eventBus, const nlohmann::json& config);
    ~WifiHandler() override;

    // IProtocolHandler interface
    std::string name() const override { return "wifi"; }
    std::string version() const override { return "1.0.0"; }
    std::string description() const override {
        return "WiFi device handler (MQTT/Tasmota/ESPHome/Shelly/Tuya)";
    }

    bool initialize() override;
    void shutdown() override;
    void poll() override;

    ProtocolState state() const override;
    bool isConnected() const override { return m_connected; }
    std::string lastError() const override { return m_lastError; }

    bool supportsDiscovery() const override { return true; }
    void startDiscovery() override;
    void stopDiscovery() override;
    bool isDiscovering() const override { return m_discovering; }

    bool sendCommand(const std::string& deviceAddress,
                     const std::string& command,
                     const nlohmann::json& params) override;

    void setDeviceDiscoveredCallback(DeviceDiscoveredCallback cb) override;
    void setDeviceStateCallback(DeviceStateCallback cb) override;
    void setDeviceAvailabilityCallback(DeviceAvailabilityCallback cb) override;

    nlohmann::json getStatus() const override;
    std::vector<std::string> getKnownDeviceAddresses() const override;

    // Additional methods

    /**
     * Manually add a Shelly device by IP address
     */
    bool addShellyDevice(const std::string& ipAddress);

    /**
     * Manually add a Tuya device
     */
    bool addTuyaDevice(const TuyaDeviceConfig& config);

    /**
     * Get a device by ID
     */
    DevicePtr getDevice(const std::string& id) const;

private:
    // MQTT handling
    void setupMqtt();
    void onMqttMessage(const std::string& topic, const std::string& payload);
    void onMqttDiscovery(const MqttDiscoveryConfig& config);
    DevicePtr createMqttDevice(const MqttDiscoveryConfig& config);

    // Shelly handling
    void pollShellyDevices();

    // State updates
    void onDeviceStateUpdate(const std::string& deviceId,
                              const std::string& property,
                              const nlohmann::json& value);
    void onDeviceAvailabilityUpdate(const std::string& deviceId, bool available);

    // Device management
    void addDevice(const std::string& id, WifiDeviceEntry entry);
    void removeDevice(const std::string& id);

    EventBus& m_eventBus;
    nlohmann::json m_config;

    // MQTT
    std::unique_ptr<MqttClient> m_mqtt;
    MqttDiscoveryManager m_mqttDiscovery;
    std::string m_mqttBroker;
    int m_mqttPort = 1883;

    // HTTP
    std::unique_ptr<http::HttpClient> m_http;
    std::unique_ptr<ShellyDiscovery> m_shellyDiscovery;

    // Devices
    std::map<std::string, WifiDeviceEntry> m_devices;
    mutable std::mutex m_deviceMutex;

    // State
    bool m_connected = false;
    bool m_discovering = false;
    std::string m_lastError;
    ProtocolState m_state = ProtocolState::Disconnected;

    // Callbacks
    DeviceDiscoveredCallback m_discoveredCallback;
    DeviceStateCallback m_stateCallback;
    DeviceAvailabilityCallback m_availabilityCallback;

    // Polling
    std::chrono::steady_clock::time_point m_lastPoll;
    static constexpr int POLL_INTERVAL_MS = 30000;  // 30 seconds
};

} // namespace smarthub::wifi
