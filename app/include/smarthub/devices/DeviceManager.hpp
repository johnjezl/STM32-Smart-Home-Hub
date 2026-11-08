/**
 * SmartHub Device Manager
 *
 * Manages all devices and protocol handlers in the smart home system.
 */
#pragma once

#include <smarthub/devices/IDevice.hpp>
#include <smarthub/protocols/IProtocolHandler.hpp>
#include <optional>
#include <string>
#include <memory>
#include <vector>
#include <utility>
#include <thread>
#include <condition_variable>
#include <map>
#include <mutex>

namespace smarthub {

class EventBus;
class Database;
class Device;

/** One recorded sensor reading */
struct SensorSample {
    int64_t timestamp;  // Unix seconds
    double value;
};

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
    bool addDevice(const DevicePtr& device);

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
    std::vector<DevicePtr> getDevicesByRoom(const std::string& roomId) const;

    /**
     * The id of the room this names, by its id or its name. An empty string for no room, and
     * nothing if it names no room. Devices store the id, so renaming a room keeps its devices.
     */
    std::optional<std::string> roomIdFor(const std::string& idOrName) const;

    // Room management

    /**
     * Get all rooms
     */
    std::vector<std::pair<std::string, std::string>> getAllRooms() const;

    /**
     * Add a new room
     * @param id Unique room identifier
     * @param name Room display name
     * @return true if added successfully
     */
    bool addRoom(const std::string& id, const std::string& name);

    /**
     * Update room name
     * @param id Room identifier
     * @param name New display name
     * @return true if updated successfully
     */
    bool updateRoom(const std::string& id, const std::string& name);

    /**
     * Delete a room
     * @param id Room identifier
     * @return true if deleted successfully
     */
    bool deleteRoom(const std::string& id);

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

    /**
     * Set callback for pending devices discovered during pairing
     * When set, discovered devices will be passed to this callback instead of auto-added.
     * The UI can then show the device info and let the user complete setup.
     * @param protocol Protocol name (e.g., "zigbee")
     * @param callback Callback to receive discovered devices (pass nullptr to clear)
     */
    void setPendingDeviceCallback(const std::string& protocol, DeviceDiscoveredCallback callback);

    /**
     * Get the pending device from a protocol (if any)
     * @param protocol Protocol name (e.g., "zigbee")
     * @return The pending device, or nullptr if none
     */
    DevicePtr getPendingDevice(const std::string& protocol);

    /**
     * Clear the pending device for a protocol
     * @param protocol Protocol name (e.g., "zigbee")
     */
    void clearPendingDevice(const std::string& protocol);

    // State management

    /**
     * Whether a client or automation may set this property to this value.
     * Other properties are reported by the device.
     */
    static bool isControllableProperty(const IDevice& device, const std::string& property,
                                       const nlohmann::json& value);

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
     * Write state changes that haven't been saved yet. Changes are saved every
     * kStateSaveIntervalSeconds, as one write, to limit wear on the SD card.
     */
    bool flushStateChanges();
    static constexpr int kStateSaveIntervalSeconds = 30;

    /**
     * Recorded readings of a property, oldest first, averaged into at most maxPoints time slots.
     * Readings from the last kHistorySaveIntervalSeconds may not be included yet.
     */
    std::vector<SensorSample> sensorHistory(const std::string& deviceId, const std::string& property,
                                            int64_t fromTimestamp, int maxPoints) const;
    /**
     * Write sensor readings that haven't been saved yet, every kHistorySaveIntervalSeconds.
     */
    bool flushSensorSamples();

    // To limit wear on the SD card: at most one reading a minute per sensor, saved together every
    // few minutes, and kept for a week, which is the longest the chart shows
    static constexpr int64_t kHistorySampleIntervalSeconds = 60;
    static constexpr int kHistorySaveIntervalSeconds = 300;
    static constexpr int64_t kHistoryRetentionSeconds = 7 * 24 * 3600;

    /**
     * Save a single device to database
     */
    bool saveDevice(const DevicePtr& device);

    /**
     * Delete a device from database
     */
    bool deleteDeviceFromDatabase(const std::string& id);

    /**
     * Load all devices from database
     */
    void loadDevicesFromDatabase();
    void restoreSensorOptions(const std::string& id, std::string& typeName, nlohmann::json& config);

private:
    ProtocolHandlerPtr protocolNamed(const std::string& name) const;
    void restoreDevices(IProtocolHandler& protocol, const std::string& name);
    void onDeviceDiscovered(const DevicePtr& device);
    void applyStateChange(const DevicePtr& device, const std::string& property,
                          const nlohmann::json& value);
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
    std::mutex m_stateChangeMutex;

    // State changes waiting to be saved: (device, property) -> value as JSON
    struct StateRow {
        std::string deviceId;
        std::string property;
        std::string value;
    };
    bool writeStateRows(const std::vector<StateRow>& rows);
    void markStateChanged(const std::string& deviceId, const std::string& property,
                          const nlohmann::json& value);
    void stateWriterLoop();
    void stopStateWriter();
    bool roomExists(const std::string& id) const;
    // Rooms saved by name, as the dashboard used to, become ids
    void migrateRoomNames();

    std::mutex m_stateWriteMutex;  // Guards the queue and the stop flag
    std::map<std::pair<std::string, std::string>, std::string> m_unsavedState;

    // Guarded by m_stateWriteMutex
    struct PendingSample {
        std::string deviceId;
        std::string property;
        double value;
        int64_t timestamp;
    };
    std::vector<PendingSample> m_unsavedSamples;
    std::map<std::pair<std::string, std::string>, int64_t> m_lastSampleTime;
    int64_t m_lastHistoryPrune = 0;  // Only the writer touches this

    void recordSensorSample(const std::string& deviceId, const std::string& property,
                            const nlohmann::json& value);
    std::thread m_stateWriter;
    std::condition_variable m_stateWriterWake;
    bool m_stateWriterStopping = false;
    bool m_discovering = false;
};

} // namespace smarthub
