/**
 * SmartHub Device
 *
 * Base class implementing IDevice interface for all devices in the smart home system.
 */
#pragma once

#include <smarthub/devices/IDevice.hpp>
#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <functional>
#include <chrono>
#include <nlohmann/json.hpp>

namespace smarthub {

/**
 * Base device class implementing IDevice
 */
class Device : public IDevice {
public:
    using StateCallback = std::function<void(const std::string& property, const nlohmann::json& value)>;

    /**
     * Constructor
     * @param id Unique device identifier
     * @param name Human-readable device name
     * @param type Device type
     * @param protocol Protocol name (e.g., "mqtt", "zigbee", "mock")
     * @param protocolAddress Protocol-specific address
     */
    Device(const std::string& id,
           const std::string& name,
           DeviceType type,
           const std::string& protocol = "local",
           const std::string& protocolAddress = "");

    virtual ~Device() = default;

    // IDevice - Identity
    std::string id() const override { return m_id; }
    std::string name() const override;
    void setName(const std::string& name) override;
    DeviceType type() const override { return m_type; }
    std::string typeString() const override;

    // IDevice - Protocol
    std::string protocol() const override { return m_protocol; }
    std::string protocolAddress() const override { return m_protocolAddress; }

    // IDevice - Location
    std::string room() const override;
    void setRoom(const std::string& room) override;

    // IDevice - Capabilities
    std::vector<DeviceCapability> capabilities() const override;
    bool hasCapability(DeviceCapability cap) const override;

    // IDevice - State management
    nlohmann::json getState() const override;
    bool setState(const std::string& property, const nlohmann::json& value) override;
    nlohmann::json getProperty(const std::string& property) const override;

    // IDevice - Availability
    DeviceAvailability availability() const override;
    bool isAvailable() const override { return m_availability == DeviceAvailability::Online; }
    uint64_t lastSeen() const override { return m_lastSeen; }
    void updateLastSeen() override;

    // IDevice - Configuration
    nlohmann::json getConfig() const override;
    void setConfig(const nlohmann::json& config) override;

    // IDevice - Serialization
    nlohmann::json toJson() const override;

    // Additional methods
    void setStateCallback(StateCallback callback);
    void setAvailability(DeviceAvailability availability);

    // Factory helper for deserialization
    static std::shared_ptr<Device> fromJson(const nlohmann::json& json);

protected:
    /**
     * Called when state changes - override in derived classes for custom behavior
     */
    virtual void onStateChange(const std::string& property, const nlohmann::json& value);

    /**
     * Add a capability to this device
     */
    void addCapability(DeviceCapability cap);

    /**
     * Set state internally without triggering callback (for initialization)
     */
    void setStateInternal(const std::string& property, const nlohmann::json& value);

    /**
     * Notify callback of state change
     */
    void notifyStateChange(const std::string& property, const nlohmann::json& value);

    // Member variables
    std::string m_id;
    std::string m_name;
    DeviceType m_type;
    std::string m_protocol;
    std::string m_protocolAddress;
    std::string m_room;

    std::vector<DeviceCapability> m_capabilities;
    std::map<std::string, nlohmann::json> m_state;
    nlohmann::json m_config;

    DeviceAvailability m_availability = DeviceAvailability::Unknown;
    uint64_t m_lastSeen = 0;

    StateCallback m_stateCallback;
    mutable std::mutex m_mutex;
};

} // namespace smarthub
