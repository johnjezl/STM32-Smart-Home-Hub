/**
 * Zigbee Protocol Handler
 *
 * Implements IProtocolHandler for Zigbee devices via CC2652P coordinator.
 * Bridges between the SmartHub device model and Zigbee/ZCL protocols.
 */
#pragma once

#include <smarthub/protocols/IProtocolHandler.hpp>
#include "ZigbeeCoordinator.hpp"
#include "ZigbeeDeviceDatabase.hpp"
#include <map>
#include <memory>

namespace smarthub::zigbee {

/**
 * Zigbee Protocol Handler
 */
class ZigbeeHandler : public IProtocolHandler {
public:
    /**
     * Construct handler with event bus and configuration
     * @param eventBus Event bus for publishing events
     * @param config Configuration JSON containing "port", "baudRate", etc.
     */
    ZigbeeHandler(EventBus& eventBus, const nlohmann::json& config);
    ~ZigbeeHandler() override;

    // Non-copyable
    ZigbeeHandler(const ZigbeeHandler&) = delete;
    ZigbeeHandler& operator=(const ZigbeeHandler&) = delete;

    // ========================================================================
    // IProtocolHandler interface
    // ========================================================================

    std::string name() const override { return "zigbee"; }
    std::string version() const override { return "1.0.0"; }
    std::string description() const override { return "Zigbee protocol handler via CC2652P"; }

    bool initialize() override;
    void shutdown() override;
    void poll() override;

    ProtocolState state() const override;
    bool isConnected() const override;
    std::string lastError() const override { return m_lastError; }

    bool supportsDiscovery() const override { return true; }
    void startDiscovery() override;
    void stopDiscovery() override;
    bool isDiscovering() const override { return m_discovering; }

    bool sendCommand(const std::string& deviceAddress, const std::string& command,
                     const nlohmann::json& params) override;

    void setDeviceDiscoveredCallback(DeviceDiscoveredCallback cb) override;
    void setDeviceStateCallback(DeviceStateCallback cb) override;
    void setDeviceAvailabilityCallback(DeviceAvailabilityCallback cb) override;

    nlohmann::json getStatus() const override;
    std::vector<std::string> getKnownDeviceAddresses() const override;

    // ========================================================================
    // Zigbee-specific methods
    // ========================================================================

    /**
     * Get the underlying coordinator (for advanced operations)
     */
    ZigbeeCoordinator* coordinator() { return m_coordinator.get(); }

    /**
     * Load device definitions from file
     */
    bool loadDeviceDatabase(const std::string& path);

private:
    // Device discovery and management
    void onDeviceAnnounced(uint16_t nwkAddr, uint64_t ieeeAddr);
    void onDeviceLeft(uint64_t ieeeAddr);
    void onAttributeReport(uint16_t nwkAddr, const ZclAttributeValue& attr);
    void onCommandReceived(uint16_t nwkAddr, uint8_t endpoint, uint16_t cluster,
                           uint8_t command, const std::vector<uint8_t>& payload);

    // Device creation
    DevicePtr createDeviceFromInfo(const ZigbeeDeviceInfo& info);
    DeviceType inferDeviceType(const ZigbeeDeviceInfo& info);
    void setupReporting(uint16_t nwkAddr, uint8_t endpoint, DeviceType type);

    // Command handling
    bool handleOnOffCommand(uint16_t nwkAddr, uint8_t endpoint, const nlohmann::json& params);
    bool handleBrightnessCommand(uint16_t nwkAddr, uint8_t endpoint, const nlohmann::json& params);
    bool handleColorCommand(uint16_t nwkAddr, uint8_t endpoint, const nlohmann::json& params);

    // Address conversion
    std::string ieeeToDeviceId(uint64_t ieeeAddr) const;
    std::optional<uint64_t> deviceIdToIeee(const std::string& deviceId) const;

    // Dependencies
    EventBus& m_eventBus;
    nlohmann::json m_config;

    // Coordinator
    std::unique_ptr<ZigbeeCoordinator> m_coordinator;

    // Device database
    ZigbeeDeviceDatabase m_deviceDb;

    // State
    bool m_initialized = false;
    bool m_discovering = false;
    std::string m_lastError;

    // Callbacks
    DeviceDiscoveredCallback m_discoveredCb;
    DeviceStateCallback m_stateCb;
    DeviceAvailabilityCallback m_availabilityCb;

    // Device mappings
    std::map<uint64_t, std::string> m_ieeeToDeviceId;
    std::map<std::string, uint64_t> m_deviceIdToIeee;
    std::map<uint64_t, uint8_t> m_deviceEndpoints;  // Primary endpoint per device
    mutable std::mutex m_deviceMutex;
};

} // namespace smarthub::zigbee
