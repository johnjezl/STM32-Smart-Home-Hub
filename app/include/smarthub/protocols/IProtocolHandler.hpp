/**
 * SmartHub Protocol Handler Interface
 *
 * Abstract interface for protocol handlers that communicate with devices.
 */
#pragma once

#include <smarthub/devices/IDevice.hpp>
#include <functional>
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

namespace smarthub {

class EventBus;

/**
 * Callback when a device is discovered
 */
using DeviceDiscoveredCallback = std::function<void(DevicePtr)>;

/**
 * Callback when a device's state changes
 */
using DeviceStateCallback = std::function<void(
    const std::string& deviceId,
    const std::string& property,
    const nlohmann::json& value
)>;

/**
 * Callback when a device becomes available or unavailable
 */
using DeviceAvailabilityCallback = std::function<void(
    const std::string& deviceId,
    DeviceAvailability availability
)>;

/**
 * Protocol handler connection state
 */
enum class ProtocolState {
    Disconnected,
    Connecting,
    Connected,
    Error
};

/**
 * Abstract protocol handler interface
 *
 * Implement this interface to add support for new device protocols
 * (MQTT, Zigbee, Z-Wave, WiFi devices, etc.)
 */
class IProtocolHandler {
public:
    virtual ~IProtocolHandler() = default;

    // Protocol identification
    virtual std::string name() const = 0;
    virtual std::string version() const = 0;
    virtual std::string description() const = 0;

    // Lifecycle
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual void poll() = 0;  // Called from main loop

    // Connection state
    virtual ProtocolState state() const = 0;
    virtual bool isConnected() const = 0;
    virtual std::string lastError() const = 0;

    // Discovery
    virtual bool supportsDiscovery() const = 0;
    virtual void startDiscovery() = 0;
    virtual void stopDiscovery() = 0;
    virtual bool isDiscovering() const = 0;

    // Device operations
    virtual bool sendCommand(
        const std::string& deviceAddress,
        const std::string& command,
        const nlohmann::json& params
    ) = 0;

    // Callbacks
    virtual void setDeviceDiscoveredCallback(DeviceDiscoveredCallback cb) = 0;
    virtual void setDeviceStateCallback(DeviceStateCallback cb) = 0;
    virtual void setDeviceAvailabilityCallback(DeviceAvailabilityCallback cb) = 0;

    // Status and diagnostics
    virtual nlohmann::json getStatus() const = 0;
    virtual std::vector<std::string> getKnownDeviceAddresses() const = 0;
};

using ProtocolHandlerPtr = std::shared_ptr<IProtocolHandler>;

} // namespace smarthub
