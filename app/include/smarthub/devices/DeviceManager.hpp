/**
 * SmartHub Device Manager
 *
 * Manages all devices in the smart home system.
 */
#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace smarthub {

class EventBus;
class Database;
class Device;

/**
 * Device manager - central registry for all devices
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
     * Add a new device
     */
    bool addDevice(std::shared_ptr<Device> device);

    /**
     * Remove a device by ID
     */
    bool removeDevice(const std::string& id);

    /**
     * Get device by ID
     */
    std::shared_ptr<Device> getDevice(const std::string& id) const;

    /**
     * Get all devices
     */
    std::vector<std::shared_ptr<Device>> getAllDevices() const;

    /**
     * Get devices by room
     */
    std::vector<std::shared_ptr<Device>> getDevicesByRoom(const std::string& room) const;

    /**
     * Get devices by type
     */
    std::vector<std::shared_ptr<Device>> getDevicesByType(int type) const;

    /**
     * Get device count
     */
    size_t deviceCount() const;

    /**
     * Save all devices to database
     */
    bool saveAllDevices();

private:
    void loadDevicesFromDatabase();
    void setupEventHandlers();

    EventBus& m_eventBus;
    Database& m_database;
    std::unordered_map<std::string, std::shared_ptr<Device>> m_devices;
    mutable std::mutex m_mutex;
};

} // namespace smarthub
