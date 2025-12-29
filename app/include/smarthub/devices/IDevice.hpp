/**
 * SmartHub Device Interface
 *
 * Abstract interface for all devices in the smart home system.
 * Provides a common contract for device identity, capabilities, state, and serialization.
 */
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

namespace smarthub {

/**
 * Device capabilities - features a device can support
 */
enum class DeviceCapability {
    OnOff,              // Can be turned on/off
    Brightness,         // Supports brightness control (0-100%)
    ColorTemperature,   // Supports color temperature (Kelvin)
    ColorRGB,           // Supports RGB color control
    ColorHSV,           // Supports HSV color control
    Temperature,        // Reports temperature readings
    Humidity,           // Reports humidity readings
    Pressure,           // Reports barometric pressure
    Motion,             // Detects motion
    Contact,            // Detects open/close state
    Battery,            // Reports battery level
    Power,              // Reports power consumption (W)
    Energy,             // Reports energy usage (kWh)
    Voltage,            // Reports voltage (V)
    Current,            // Reports current (A)
    Occupancy,          // Detects room occupancy
    Illuminance,        // Reports light level (lux)
    Smoke,              // Smoke detection
    CarbonMonoxide,     // CO detection
    Water,              // Water/leak detection
    Lock,               // Lock/unlock capability
    Position,           // Position/percentage (blinds, covers)
    Tilt,               // Tilt angle
    Speed,              // Fan speed level
    Mode                // Operating mode selection
};

/**
 * Device type enumeration - categorizes devices
 */
enum class DeviceType {
    Unknown,
    Switch,             // Simple on/off switch
    Light,              // Basic light (on/off only)
    Dimmer,             // Dimmable light
    ColorLight,         // Color-capable light
    Outlet,             // Smart outlet/plug
    Thermostat,         // HVAC thermostat
    TemperatureSensor,  // Temperature sensor
    HumiditySensor,     // Humidity sensor
    MultiSensor,        // Multi-function sensor
    MotionSensor,       // Motion/occupancy sensor
    ContactSensor,      // Door/window sensor
    PowerMeter,         // Power monitoring device
    SmokeSensor,        // Smoke/fire detector
    WaterSensor,        // Water/leak sensor
    Fan,                // Smart fan
    Blinds,             // Window blinds/shades
    Lock,               // Smart lock
    Doorbell,           // Video doorbell
    Camera,             // Security camera
    Speaker,            // Smart speaker
    Custom              // Custom/generic device
};

/**
 * Device availability state
 */
enum class DeviceAvailability {
    Online,             // Device is reachable
    Offline,            // Device is not reachable
    Unknown             // State unknown (e.g., just added)
};

/**
 * Abstract device interface
 */
class IDevice {
public:
    virtual ~IDevice() = default;

    // Identity
    virtual std::string id() const = 0;
    virtual std::string name() const = 0;
    virtual void setName(const std::string& name) = 0;
    virtual DeviceType type() const = 0;
    virtual std::string typeString() const = 0;

    // Protocol information
    virtual std::string protocol() const = 0;
    virtual std::string protocolAddress() const = 0;

    // Location
    virtual std::string room() const = 0;
    virtual void setRoom(const std::string& room) = 0;

    // Capabilities
    virtual std::vector<DeviceCapability> capabilities() const = 0;
    virtual bool hasCapability(DeviceCapability cap) const = 0;

    // State management
    virtual nlohmann::json getState() const = 0;
    virtual bool setState(const std::string& property, const nlohmann::json& value) = 0;
    virtual nlohmann::json getProperty(const std::string& property) const = 0;

    // Availability
    virtual DeviceAvailability availability() const = 0;
    virtual bool isAvailable() const = 0;
    virtual uint64_t lastSeen() const = 0;
    virtual void updateLastSeen() = 0;

    // Configuration
    virtual nlohmann::json getConfig() const = 0;
    virtual void setConfig(const nlohmann::json& config) = 0;

    // Serialization
    virtual nlohmann::json toJson() const = 0;
};

// Shared pointer type alias
using DevicePtr = std::shared_ptr<IDevice>;

/**
 * Convert DeviceType to string
 */
inline std::string deviceTypeToString(DeviceType type) {
    switch (type) {
        case DeviceType::Switch:            return "switch";
        case DeviceType::Light:             return "light";
        case DeviceType::Dimmer:            return "dimmer";
        case DeviceType::ColorLight:        return "color_light";
        case DeviceType::Outlet:            return "outlet";
        case DeviceType::Thermostat:        return "thermostat";
        case DeviceType::TemperatureSensor: return "temperature_sensor";
        case DeviceType::HumiditySensor:    return "humidity_sensor";
        case DeviceType::MultiSensor:       return "multi_sensor";
        case DeviceType::MotionSensor:      return "motion_sensor";
        case DeviceType::ContactSensor:     return "contact_sensor";
        case DeviceType::PowerMeter:        return "power_meter";
        case DeviceType::SmokeSensor:       return "smoke_sensor";
        case DeviceType::WaterSensor:       return "water_sensor";
        case DeviceType::Fan:               return "fan";
        case DeviceType::Blinds:            return "blinds";
        case DeviceType::Lock:              return "lock";
        case DeviceType::Doorbell:          return "doorbell";
        case DeviceType::Camera:            return "camera";
        case DeviceType::Speaker:           return "speaker";
        case DeviceType::Custom:            return "custom";
        default:                            return "unknown";
    }
}

/**
 * Convert string to DeviceType
 */
inline DeviceType stringToDeviceType(const std::string& str) {
    if (str == "switch")             return DeviceType::Switch;
    if (str == "light")              return DeviceType::Light;
    if (str == "dimmer")             return DeviceType::Dimmer;
    if (str == "color_light")        return DeviceType::ColorLight;
    if (str == "outlet")             return DeviceType::Outlet;
    if (str == "thermostat")         return DeviceType::Thermostat;
    if (str == "temperature_sensor") return DeviceType::TemperatureSensor;
    if (str == "humidity_sensor")    return DeviceType::HumiditySensor;
    if (str == "multi_sensor")       return DeviceType::MultiSensor;
    if (str == "motion_sensor")      return DeviceType::MotionSensor;
    if (str == "contact_sensor")     return DeviceType::ContactSensor;
    if (str == "power_meter")        return DeviceType::PowerMeter;
    if (str == "smoke_sensor")       return DeviceType::SmokeSensor;
    if (str == "water_sensor")       return DeviceType::WaterSensor;
    if (str == "fan")                return DeviceType::Fan;
    if (str == "blinds")             return DeviceType::Blinds;
    if (str == "lock")               return DeviceType::Lock;
    if (str == "doorbell")           return DeviceType::Doorbell;
    if (str == "camera")             return DeviceType::Camera;
    if (str == "speaker")            return DeviceType::Speaker;
    if (str == "custom")             return DeviceType::Custom;
    return DeviceType::Unknown;
}

/**
 * Convert DeviceCapability to string
 */
inline std::string capabilityToString(DeviceCapability cap) {
    switch (cap) {
        case DeviceCapability::OnOff:            return "on_off";
        case DeviceCapability::Brightness:       return "brightness";
        case DeviceCapability::ColorTemperature: return "color_temperature";
        case DeviceCapability::ColorRGB:         return "color_rgb";
        case DeviceCapability::ColorHSV:         return "color_hsv";
        case DeviceCapability::Temperature:      return "temperature";
        case DeviceCapability::Humidity:         return "humidity";
        case DeviceCapability::Pressure:         return "pressure";
        case DeviceCapability::Motion:           return "motion";
        case DeviceCapability::Contact:          return "contact";
        case DeviceCapability::Battery:          return "battery";
        case DeviceCapability::Power:            return "power";
        case DeviceCapability::Energy:           return "energy";
        case DeviceCapability::Voltage:          return "voltage";
        case DeviceCapability::Current:          return "current";
        case DeviceCapability::Occupancy:        return "occupancy";
        case DeviceCapability::Illuminance:      return "illuminance";
        case DeviceCapability::Smoke:            return "smoke";
        case DeviceCapability::CarbonMonoxide:   return "carbon_monoxide";
        case DeviceCapability::Water:            return "water";
        case DeviceCapability::Lock:             return "lock";
        case DeviceCapability::Position:         return "position";
        case DeviceCapability::Tilt:             return "tilt";
        case DeviceCapability::Speed:            return "speed";
        case DeviceCapability::Mode:             return "mode";
        default:                                 return "unknown";
    }
}

} // namespace smarthub
