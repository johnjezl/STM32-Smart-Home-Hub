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
#include <deque>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>

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

    using CoordinatorFactory = std::function<std::unique_ptr<ZigbeeCoordinator>()>;

    /** Create coordinators with this instead of opening the configured port (for testing). */
    ZigbeeHandler(EventBus& eventBus, const nlohmann::json& config,
                  CoordinatorFactory coordinatorFactory);
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
    std::string lastError() const override;

    bool supportsDiscovery() const override { return true; }
    void startDiscovery() override;
    void stopDiscovery() override;
    bool isDiscovering() const override { return m_discovering; }

    bool sendCommand(const std::string& deviceAddress, const std::string& command,
                     const nlohmann::json& params) override;

    void setDeviceDiscoveredCallback(DeviceDiscoveredCallback cb) override;
    void setDeviceStateCallback(DeviceStateCallback cb) override;
    void setDeviceAvailabilityCallback(DeviceAvailabilityCallback cb) override;

    /**
     * Set callback for devices discovered during pairing (not auto-added)
     * This is called instead of DeviceDiscoveredCallback when in discovery mode
     */
    void setPendingDeviceCallback(DeviceDiscoveredCallback cb);

    /**
     * Get pending device discovered during pairing (if any)
     */
    DevicePtr getPendingDevice() const;

    /**
     * Clear pending device
     */
    void clearPendingDevice();

    nlohmann::json getStatus() const override;
    std::vector<std::string> getKnownDeviceAddresses() const override;
    void restoreDevice(const IDevice& device) override;
    void forgetDevice(const std::string& deviceId) override;

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
    void startPairing(uint16_t nwkAddr, uint64_t ieeeAddr);
    void stopPairing();
    void fail(const std::string& message);
    uint64_t pairDevice(uint64_t ieeeAddr);  // Returns the attempt it last worked on
    void finishPairing(uint64_t ieeeAddr);
    void reportPairingFailure(uint64_t ieeeAddr, const std::string& message);
    bool pairingStopped();

    enum class PairingWait { Ready, TimedOut, Restarted, Stopped };
    PairingWait waitForPairing(uint64_t ieeeAddr, uint64_t attempt, int timeoutMs,
                               const std::function<bool()>& ready);
    void onDeviceLeft(uint64_t ieeeAddr);
    void onAttributeReport(uint16_t nwkAddr, const ZclAttributeValue& attr);
    void onCommandReceived(uint16_t nwkAddr, uint8_t endpoint, uint16_t cluster,
                           uint8_t command, const std::vector<uint8_t>& payload);

    // Device creation
    DevicePtr createDeviceFromInfo(const ZigbeeDeviceInfo& info);
    DeviceType inferDeviceType(const ZigbeeDeviceInfo& info);
    DeviceType resolveDeviceType(const ZigbeeDeviceInfo& info);
    void setupReporting(uint16_t nwkAddr, uint8_t endpoint, DeviceType type);

    // Command handling
    bool handleOnOffCommand(uint16_t nwkAddr, uint8_t endpoint, const std::string& command);
    bool handleBrightnessCommand(uint16_t nwkAddr, uint8_t endpoint, const nlohmann::json& params);
    bool handleColorCommand(uint16_t nwkAddr, uint8_t endpoint, const nlohmann::json& params);

    // Address conversion
    std::string ieeeToDeviceId(uint64_t ieeeAddr) const;
    std::optional<uint64_t> deviceIdToIeee(const std::string& deviceId) const;
    std::optional<std::string> savedDeviceId(uint64_t ieeeAddr) const;
    static std::optional<uint64_t> parseIeeeAddress(std::string text);
    void lookUpNetworkAddress(uint64_t ieeeAddr);  // Caller keeps the coordinator from going away
    void onNetworkAddressFound(uint16_t nwkAddr, uint64_t ieeeAddr, bool changed);

    // Dependencies
    EventBus& m_eventBus;
    nlohmann::json m_config;

    // Coordinator
    CoordinatorFactory m_coordinatorFactory;
    std::unique_ptr<ZigbeeCoordinator> m_coordinator;

    // Device database
    ZigbeeDeviceDatabase m_deviceDb;

    // State
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_discovering{false};
    std::string m_lastError;
    mutable std::mutex m_errorMutex;
    std::shared_mutex m_coordinatorMutex;

    // Pairing, one background thread per joining device
    struct PairingTask {
        std::thread thread;
        uint16_t nwkAddr = 0;
        uint64_t attempt = 0;      // Goes up when the device announces again
        bool finished = false;
    };
    std::map<uint64_t, PairingTask> m_pairingTasks;
    std::mutex m_pairingMutex;
    std::condition_variable m_pairingCv;
    bool m_stopPairing = false;
    int m_pairingTimeoutMs = 30000;
    int m_pairingEndpointTimeoutMs = 5000;

    // Callbacks
    DeviceDiscoveredCallback m_discoveredCb;
    DeviceDiscoveredCallback m_pendingDeviceCb;
    DeviceStateCallback m_stateCb;
    DeviceAvailabilityCallback m_availabilityCb;
    std::mutex m_callbackMutex;                  // Guards the callbacks above, except pending
    std::recursive_mutex m_pendingCallbackMutex;  // Guards m_pendingDeviceCb, and held to call it

    void notifyState(const std::string& deviceId, const std::string& property,
                     const nlohmann::json& value);
    void notifyAvailability(const std::string& deviceId, DeviceAvailability availability);

    // Pending device during discovery (not auto-added)
    DevicePtr m_pendingDevice;

    // Device mappings
    std::map<uint64_t, std::string> m_ieeeToDeviceId;
    std::map<std::string, uint64_t> m_deviceIdToIeee;
    std::map<uint64_t, uint8_t> m_deviceEndpoints;  // Primary endpoint per device
    // Devices the hub keeps, IEEE to device id. Announcing again doesn't make one of these new, and
    // this outlives a leave, because the hub still has the device when it rejoins.
    std::map<uint64_t, std::string> m_savedDevices;
    std::deque<uint64_t> m_addressChecks;  // Saved addresses still to confirm
    std::chrono::steady_clock::time_point m_lastAddressCheck{};
    std::map<uint64_t, std::chrono::steady_clock::time_point> m_lastAddressLookup;
    int m_addressLookupIntervalMs = 30000;
    mutable std::mutex m_deviceMutex;
};

} // namespace smarthub::zigbee
