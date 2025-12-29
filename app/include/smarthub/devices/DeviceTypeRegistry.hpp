/**
 * SmartHub Device Type Registry
 *
 * Factory for creating device instances by type.
 */
#pragma once

#include <smarthub/devices/IDevice.hpp>
#include <functional>
#include <map>
#include <string>

namespace smarthub {

/**
 * Device type registry - factory for creating device instances
 */
class DeviceTypeRegistry {
public:
    /**
     * Device creator function type
     */
    using DeviceCreator = std::function<DevicePtr(
        const std::string& id,
        const std::string& name,
        const std::string& protocol,
        const std::string& protocolAddress,
        const nlohmann::json& config
    )>;

    /**
     * Get singleton instance
     */
    static DeviceTypeRegistry& instance();

    /**
     * Register a device type
     * @param type Device type enum value
     * @param typeName String representation of the type
     * @param creator Factory function to create instances
     */
    void registerType(DeviceType type, const std::string& typeName, DeviceCreator creator);

    /**
     * Unregister a device type
     */
    void unregisterType(DeviceType type);

    /**
     * Check if a device type is registered
     */
    bool hasType(DeviceType type) const;
    bool hasType(const std::string& typeName) const;

    /**
     * Create a device by type enum
     */
    DevicePtr create(DeviceType type,
                     const std::string& id,
                     const std::string& name,
                     const std::string& protocol = "local",
                     const std::string& protocolAddress = "",
                     const nlohmann::json& config = {});

    /**
     * Create a device by type name string
     */
    DevicePtr createFromTypeName(const std::string& typeName,
                                 const std::string& id,
                                 const std::string& name,
                                 const std::string& protocol = "local",
                                 const std::string& protocolAddress = "",
                                 const nlohmann::json& config = {});

    /**
     * Get type name from enum
     */
    std::string getTypeName(DeviceType type) const;

    /**
     * Get type enum from name
     */
    DeviceType getTypeFromName(const std::string& name) const;

    /**
     * Get all registered type names
     */
    std::vector<std::string> registeredTypes() const;

private:
    DeviceTypeRegistry();
    DeviceTypeRegistry(const DeviceTypeRegistry&) = delete;
    DeviceTypeRegistry& operator=(const DeviceTypeRegistry&) = delete;

    void registerBuiltinTypes();

    struct TypeInfo {
        std::string name;
        DeviceCreator creator;
    };

    std::map<DeviceType, TypeInfo> m_types;
    std::map<std::string, DeviceType> m_nameToType;
};

} // namespace smarthub
