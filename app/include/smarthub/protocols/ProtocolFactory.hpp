/**
 * SmartHub Protocol Factory
 *
 * Factory for creating protocol handler instances.
 */
#pragma once

#include <smarthub/protocols/IProtocolHandler.hpp>
#include <map>
#include <functional>
#include <vector>
#include <string>

namespace smarthub {

class EventBus;

/**
 * Protocol factory - creates protocol handler instances
 */
class ProtocolFactory {
public:
    /**
     * Protocol creator function type
     */
    using CreatorFunc = std::function<ProtocolHandlerPtr(EventBus&, const nlohmann::json&)>;

    /**
     * Protocol information structure
     */
    struct ProtocolInfo {
        std::string name;
        std::string version;
        std::string description;
    };

    /**
     * Get singleton instance
     */
    static ProtocolFactory& instance();

    /**
     * Register a protocol handler type
     * @param name Protocol name (e.g., "mqtt", "zigbee")
     * @param creator Factory function to create instances
     * @param info Protocol metadata
     */
    void registerProtocol(const std::string& name, CreatorFunc creator,
                          const ProtocolInfo& info = {});

    /**
     * Unregister a protocol handler type
     */
    void unregisterProtocol(const std::string& name);

    /**
     * Check if a protocol is registered
     */
    bool hasProtocol(const std::string& name) const;

    /**
     * Create a protocol handler instance
     * @param name Protocol name
     * @param eventBus EventBus for event publishing
     * @param config Protocol-specific configuration
     * @return Protocol handler instance, or nullptr if not found
     */
    ProtocolHandlerPtr create(const std::string& name, EventBus& eventBus,
                              const nlohmann::json& config = {});

    /**
     * Get list of available protocol names
     */
    std::vector<std::string> availableProtocols() const;

    /**
     * Get protocol information
     */
    ProtocolInfo getProtocolInfo(const std::string& name) const;

private:
    ProtocolFactory() = default;
    ProtocolFactory(const ProtocolFactory&) = delete;
    ProtocolFactory& operator=(const ProtocolFactory&) = delete;

    struct Registration {
        CreatorFunc creator;
        ProtocolInfo info;
    };

    std::map<std::string, Registration> m_protocols;
};

/**
 * Helper macro for auto-registration of protocol handlers
 *
 * Usage:
 *   REGISTER_PROTOCOL("mqtt", MqttProtocolHandler, "1.0.0", "MQTT protocol support")
 */
#define REGISTER_PROTOCOL(name, className, version, description) \
    namespace { \
        static bool _registered_##className = []() { \
            ProtocolFactory::instance().registerProtocol( \
                name, \
                [](EventBus& eb, const nlohmann::json& cfg) -> ProtocolHandlerPtr { \
                    return std::make_shared<className>(eb, cfg); \
                }, \
                { name, version, description } \
            ); \
            return true; \
        }(); \
    }

} // namespace smarthub
