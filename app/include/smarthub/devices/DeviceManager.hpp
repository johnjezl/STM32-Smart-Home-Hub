/**
 * SmartHub Device Manager
 *
 * Manages all devices and protocol handlers in the smart home system.
 */
#pragma once

#include <smarthub/devices/IDevice.hpp>
#include <smarthub/protocols/IProtocolHandler.hpp>
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <mutex>

namespace smarthub {

class EventBus;
class Database;
class Device;

/**
 * Device manager - central registry for all devices and protocols
 */
class DeviceManager {
public:
    DeviceManager(EventBus& eventBus, Database& database);
    ~DeviceManager();

    // Non-copyable
    DeviceManager(const DeviceManager&) = delete;
    DeviceManager& operator=(const DeviceManager&) = delete;

    /**
     * Initialize device manager and load devices from database
     */
    bool initialize();

    /**
     * Shutdown and cleanup
     */
    void shutdown();

    /**
     * Poll all protocol handlers (call from main loop)
     */
    void poll();

    // Protocol management

    /**
     * Load and initialize a protocol handler
     * @param name Protocol name (e.g., "mqtt", "zigbee")
     * @param config Protocol-specific configuration
     * @return true if successfully loaded
     */
    bool loadProtocol(const std::string& name, const nlohmann::json& config = {});

    /**
     * Unload a protocol handler
     */
    void unloadProtocol(const std::string& name);

    /**
     * Get list of loaded protocol names
     */
    std::vector<std::string> loadedProtocols() const;

    /**
     * Get a protocol handler by name
     */
    IProtocolHandler* getProtocol(const std::string& name);

    // Device management

    /**
     * Add a new device
     */
    bool addDevice(DevicePtr device);

    /**
     * Remove a device by ID
     */
    bool removeDevice(const std::string& id);

    /**
     * Get device by ID
     */
    DevicePtr getDevice(const std::string& id) const;

    /**
     * Get all devices
     */
    std::vector<DevicePtr> getAllDevices() const;

    /**
     * Get devices by room
     */
    std::vector<DevicePtr> getDevicesByRoom(const std::string& room) const;

    /**
     * Get devices by type
     */
    std::vector<DevicePtr> getDevicesByType(DeviceType type) const;

    /**
     * Get devices by protocol
     */
    std::vector<DevicePtr> getDevicesByProtocol(const std::string& protocol) const;

    /**
     * Get device count
     */
    size_t deviceCount() const;

    // Discovery

    /**
     * Start device discovery on specified protocol (or all protocols)
     */
    void startDiscovery(const std::string& protocol = "");

    /**
     * Stop device discovery
     */
    void stopDiscovery();

    /**
     * Check if discovery is running
     */
    bool isDiscovering() const;

    // State management

    /**
     * Set device state (routes through protocol handler)
     */
    bool setDeviceState(const std::string& deviceId, const std::string& property,
                        const nlohmann::json& value);

    // Persistence

    /**
     * Save all devices to database
     */
    bool saveAllDevices();

    /**
     * Load all devices from database
     */
    void loadDevicesFromDatabase();

private:
    void setupEventHandlers();
    void onDeviceDiscovered(DevicePtr device);
    void onDeviceStateChanged(const std::string& deviceId,
                              const std::string& property,
                              const nlohmann::json& value);
    void onDeviceAvailabilityChanged(const std::string& deviceId,
                                     DeviceAvailability availability);

    EventBus& m_eventBus;
    Database& m_database;

    std::map<std::string, ProtocolHandlerPtr> m_protocols;
    std::map<std::string, DevicePtr> m_devices;

    mutable std::mutex m_mutex;
    bool m_discovering = false;
};

} // namespace smarthub
