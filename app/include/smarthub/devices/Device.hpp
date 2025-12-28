/**
 * SmartHub Device
 *
 * Base class for all devices in the smart home system.
 */
#pragma once

#include <string>
#include <map>
#include <any>
#include <functional>
#include <nlohmann/json.hpp>

namespace smarthub {

/**
 * Device types
 */
enum class DeviceType {
    Unknown,
    Light,
    Switch,
    Sensor,
    Thermostat,
    Lock,
    Camera,
    Speaker,
    Custom
};

/**
 * Device protocol
 */
enum class DeviceProtocol {
    Local,      // Direct hardware control
    MQTT,       // MQTT-based device
    Zigbee,     // Zigbee protocol
    ZWave,      // Z-Wave protocol
    WiFi,       // WiFi-based (Tuya, etc.)
    Virtual     // Virtual/software device
};

/**
 * Base device class
 */
class Device {
public:
    using StateCallback = std::function<void(const std::string& property, const std::any& value)>;

    Device(const std::string& id, const std::string& name, DeviceType type);
    virtual ~Device() = default;

    // Identification
    const std::string& id() const { return m_id; }
    const std::string& name() const { return m_name; }
    DeviceType type() const { return m_type; }
    DeviceProtocol protocol() const { return m_protocol; }

    // Room assignment
    const std::string& room() const { return m_room; }
    void setRoom(const std::string& room) { m_room = room; }

    // State management
    virtual std::any getState(const std::string& property) const;
    virtual bool setState(const std::string& property, const std::any& value);
    virtual std::map<std::string, std::any> getAllState() const;

    // Availability
    bool isOnline() const { return m_online; }
    void setOnline(bool online) { m_online = online; }

    // Callbacks
    void setStateCallback(StateCallback callback) { m_stateCallback = callback; }

    // Serialization
    virtual nlohmann::json toJson() const;
    virtual void fromJson(const nlohmann::json& json);

    // Type conversion helpers
    static std::string typeToString(DeviceType type);
    static DeviceType stringToType(const std::string& str);
    static std::string protocolToString(DeviceProtocol protocol);
    static DeviceProtocol stringToProtocol(const std::string& str);

protected:
    void notifyStateChange(const std::string& property, const std::any& value);

    std::string m_id;
    std::string m_name;
    DeviceType m_type;
    DeviceProtocol m_protocol = DeviceProtocol::Local;
    std::string m_room;
    bool m_online = true;
    std::map<std::string, std::any> m_state;
    StateCallback m_stateCallback;
};

} // namespace smarthub
