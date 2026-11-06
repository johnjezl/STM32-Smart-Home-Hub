/**
 * SmartHub Device Type Registry Implementation
 */

#include <smarthub/devices/DeviceTypeRegistry.hpp>
#include <smarthub/core/Logger.hpp>
#include <smarthub/devices/Device.hpp>
#include <smarthub/devices/types/SwitchDevice.hpp>
#include <smarthub/devices/types/DimmerDevice.hpp>
#include <smarthub/devices/types/ColorLightDevice.hpp>
#include <smarthub/devices/types/TemperatureSensor.hpp>
#include <smarthub/devices/types/MotionSensor.hpp>

namespace smarthub {

DeviceTypeRegistry& DeviceTypeRegistry::instance() {
    static DeviceTypeRegistry instance;
    return instance;
}

DeviceTypeRegistry::DeviceTypeRegistry() {
    registerBuiltinTypes();
}

void DeviceTypeRegistry::registerBuiltinTypes() {
    // Switch device
    registerType(DeviceType::Switch, "switch",
        [](const std::string& id, const std::string& name,
           const std::string& protocol, const std::string& address,
           const nlohmann::json& /* config */) -> DevicePtr {
            return std::make_shared<SwitchDevice>(id, name, protocol, address);
        });

    // Light (basic on/off) - uses Switch implementation
    registerType(DeviceType::Light, "light",
        [](const std::string& id, const std::string& name,
           const std::string& protocol, const std::string& address,
           const nlohmann::json& /* config */) -> DevicePtr {
            return std::make_shared<SwitchDevice>(id, name, protocol, address, DeviceType::Light);
        });

    // Dimmer device
    registerType(DeviceType::Dimmer, "dimmer",
        [](const std::string& id, const std::string& name,
           const std::string& protocol, const std::string& address,
           const nlohmann::json& /* config */) -> DevicePtr {
            return std::make_shared<DimmerDevice>(id, name, protocol, address);
        });

    // Color light device
    registerType(DeviceType::ColorLight, "color_light",
        [](const std::string& id, const std::string& name,
           const std::string& protocol, const std::string& address,
           const nlohmann::json& /* config */) -> DevicePtr {
            return std::make_shared<ColorLightDevice>(id, name, protocol, address);
        });

    // Temperature sensor
    registerType(DeviceType::TemperatureSensor, "temperature_sensor",
        [](const std::string& id, const std::string& name,
           const std::string& protocol, const std::string& address,
           const nlohmann::json& config) -> DevicePtr {
            bool hasHumidity = config.value("has_humidity", false);
            bool hasBattery = config.value("has_battery", false);
            return std::make_shared<TemperatureSensor>(
                id, name, protocol, address, hasHumidity, hasBattery);
        });

    // Motion sensor
    registerType(DeviceType::MotionSensor, "motion_sensor",
        [](const std::string& id, const std::string& name,
           const std::string& protocol, const std::string& address,
           const nlohmann::json& config) -> DevicePtr {
            bool hasIlluminance = config.value("has_illuminance", false);
            bool hasBattery = config.value("has_battery", false);
            return std::make_shared<MotionSensor>(
                id, name, protocol, address, hasIlluminance, hasBattery);
        });

    // Multi-sensor (temperature + humidity)
    registerType(DeviceType::MultiSensor, "multi_sensor",
        [](const std::string& id, const std::string& name,
           const std::string& protocol, const std::string& address,
           const nlohmann::json& config) -> DevicePtr {
            bool hasBattery = config.value("has_battery", false);
            return std::make_shared<TemperatureSensor>(
                id, name, protocol, address, true, hasBattery);
        });

    // Outlet - uses Switch implementation
    registerType(DeviceType::Outlet, "outlet",
        [](const std::string& id, const std::string& name,
           const std::string& protocol, const std::string& address,
           const nlohmann::json& /* config */) -> DevicePtr {
            return std::make_shared<SwitchDevice>(id, name, protocol, address, DeviceType::Outlet);
        });

    // Every other type has no class of its own yet, but has to keep its type through a save and
    // load. Otherwise it comes back as Unknown, and the next save makes that permanent.
    for (int value = 0; value <= static_cast<int>(kLastDeviceType); ++value) {
        const auto type = static_cast<DeviceType>(value);
        if (hasType(type)) {
            continue;
        }
        registerType(type, deviceTypeToString(type),
            [type](const std::string& id, const std::string& name,
                   const std::string& protocol, const std::string& address,
                   const nlohmann::json& /* config */) -> DevicePtr {
                return std::make_shared<Device>(id, name, type, protocol, address);
            });
    }
}

void DeviceTypeRegistry::registerType(DeviceType type, const std::string& typeName,
                                      DeviceCreator creator) {
    TypeInfo info;
    info.name = typeName;
    info.creator = std::move(creator);
    m_types[type] = std::move(info);
    m_nameToType[typeName] = type;
}

void DeviceTypeRegistry::unregisterType(DeviceType type) {
    auto it = m_types.find(type);
    if (it != m_types.end()) {
        m_nameToType.erase(it->second.name);
        m_types.erase(it);
    }
}

bool DeviceTypeRegistry::hasType(DeviceType type) const {
    return m_types.find(type) != m_types.end();
}

bool DeviceTypeRegistry::hasType(const std::string& typeName) const {
    return m_nameToType.find(typeName) != m_nameToType.end();
}

DevicePtr DeviceTypeRegistry::create(DeviceType type,
                                     const std::string& id,
                                     const std::string& name,
                                     const std::string& protocol,
                                     const std::string& protocolAddress,
                                     const nlohmann::json& config) {
    auto it = m_types.find(type);
    if (it == m_types.end()) {
        // Fall back to generic device
        return std::make_shared<Device>(id, name, type, protocol, protocolAddress);
    }

    DevicePtr device = it->second.creator(id, name, protocol, protocolAddress, config);
    // Creators only read what they need, and the rest of the config has to survive too
    if (device && config.is_object() && !config.empty()) {
        device->setConfig(config);
    }
    return device;
}

DevicePtr DeviceTypeRegistry::createFromTypeName(const std::string& typeName,
                                                 const std::string& id,
                                                 const std::string& name,
                                                 const std::string& protocol,
                                                 const std::string& protocolAddress,
                                                 const nlohmann::json& config) {
    auto it = m_nameToType.find(typeName);
    if (it == m_nameToType.end()) {
        // Fall back to generic device
        LOG_WARN("Device %s has unknown type '%s'", id.c_str(), typeName.c_str());
        return std::make_shared<Device>(id, name, DeviceType::Unknown, protocol, protocolAddress);
    }
    return create(it->second, id, name, protocol, protocolAddress, config);
}

std::string DeviceTypeRegistry::getTypeName(DeviceType type) const {
    auto it = m_types.find(type);
    if (it != m_types.end()) {
        return it->second.name;
    }
    return "unknown";
}

DeviceType DeviceTypeRegistry::getTypeFromName(const std::string& name) const {
    auto it = m_nameToType.find(name);
    if (it != m_nameToType.end()) {
        return it->second;
    }
    return DeviceType::Unknown;
}

std::vector<std::string> DeviceTypeRegistry::registeredTypes() const {
    std::vector<std::string> names;
    names.reserve(m_types.size());
    for (const auto& [type, info] : m_types) {
        names.push_back(info.name);
    }
    return names;
}

} // namespace smarthub
