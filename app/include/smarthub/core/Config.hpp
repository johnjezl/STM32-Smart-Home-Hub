/**
 * SmartHub Configuration
 *
 * Loads and provides access to application configuration
 * from YAML files.
 */
#pragma once

#include <string>
#include <map>
#include <vector>
#include <optional>

namespace smarthub {

/**
 * Application configuration manager
 */
class Config {
public:
    Config() = default;
    ~Config() = default;

    /**
     * Load configuration from YAML file
     * @param path Path to config file
     * @return true if loaded successfully
     */
    bool load(const std::string& path);

    /**
     * Save configuration to YAML file
     * @param path Path to config file
     * @return true if saved successfully
     */
    bool save(const std::string& path) const;

    /**
     * Check if configuration is loaded
     */
    bool isLoaded() const { return m_loaded; }

    // Database settings
    std::string databasePath() const { return m_databasePath; }

    // MQTT settings
    std::string mqttBroker() const { return m_mqttBroker; }
    int mqttPort() const { return m_mqttPort; }
    std::string mqttClientId() const { return m_mqttClientId; }
    std::string mqttUsername() const { return m_mqttUsername; }
    std::string mqttPassword() const { return m_mqttPassword; }

    // Web server settings
    int webPort() const { return m_webPort; }
    int webHttpPort() const { return m_webHttpPort; }
    std::string webRoot() const { return m_webRoot; }
    std::string tlsCertPath() const { return m_tlsCertPath; }
    std::string tlsKeyPath() const { return m_tlsKeyPath; }

    // Display settings
    std::string displayDevice() const { return m_displayDevice; }
    std::string touchDevice() const { return m_touchDevice; }
    int displayBrightness() const { return m_displayBrightness; }
    int screenTimeout() const { return m_screenTimeout; }

    // RPMsg settings
    std::string rpmsgDevice() const { return m_rpmsgDevice; }

    // Logging settings
    std::string logLevel() const { return m_logLevel; }
    std::string logFile() const { return m_logFile; }

    // Generic getters for custom settings
    std::optional<std::string> getString(const std::string& key) const;
    std::optional<int> getInt(const std::string& key) const;
    std::optional<bool> getBool(const std::string& key) const;

    // Setters
    void setDatabasePath(const std::string& path) { m_databasePath = path; }
    void setMqttBroker(const std::string& broker) { m_mqttBroker = broker; }
    void setMqttPort(int port) { m_mqttPort = port; }

private:
    void setDefaults();

    bool m_loaded = false;

    // Database
    std::string m_databasePath = "/var/lib/smarthub/smarthub.db";

    // MQTT
    std::string m_mqttBroker = "127.0.0.1";
    int m_mqttPort = 1883;
    std::string m_mqttClientId = "smarthub";
    std::string m_mqttUsername;
    std::string m_mqttPassword;

    // Web server
    int m_webPort = 443;
    int m_webHttpPort = 80;
    std::string m_webRoot = "/opt/smarthub/www";
    std::string m_tlsCertPath = "/etc/smarthub/cert.pem";
    std::string m_tlsKeyPath = "/etc/smarthub/key.pem";

    // Display
    std::string m_displayDevice = "/dev/fb0";
    std::string m_touchDevice = "/dev/input/event0";
    int m_displayBrightness = 100;
    int m_screenTimeout = 60;

    // RPMsg
    std::string m_rpmsgDevice = "/dev/ttyRPMSG0";

    // Logging
    std::string m_logLevel = "info";
    std::string m_logFile = "/var/log/smarthub.log";

    // Custom settings storage
    std::map<std::string, std::string> m_customSettings;
};

} // namespace smarthub
