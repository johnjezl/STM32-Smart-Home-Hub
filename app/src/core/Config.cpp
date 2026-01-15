/**
 * SmartHub Config Implementation
 *
 * Uses simple INI-style parsing if yaml-cpp is not available.
 */

#include <smarthub/core/Config.hpp>
#include <smarthub/core/Logger.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>

// Try to use yaml-cpp if available
#if __has_include(<yaml-cpp/yaml.h>)
#include <yaml-cpp/yaml.h>
#define HAVE_YAML_CPP 1
#else
#define HAVE_YAML_CPP 0
#endif

namespace smarthub {

namespace {

// Trim whitespace from string
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

// Parse a simple key=value file (fallback when yaml-cpp not available)
std::map<std::string, std::string> parseSimpleConfig(const std::string& path) {
    std::map<std::string, std::string> result;
    std::ifstream file(path);

    if (!file.is_open()) {
        return result;
    }

    std::string line;
    std::string currentSection;

    while (std::getline(file, line)) {
        line = trim(line);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Section header [section]
        if (line[0] == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.length() - 2);
            continue;
        }

        // Key=value or key: value
        size_t sep = line.find('=');
        if (sep == std::string::npos) {
            sep = line.find(':');
        }

        if (sep != std::string::npos) {
            std::string key = trim(line.substr(0, sep));
            std::string value = trim(line.substr(sep + 1));

            // Remove quotes if present
            if (value.length() >= 2 &&
                ((value.front() == '"' && value.back() == '"') ||
                 (value.front() == '\'' && value.back() == '\''))) {
                value = value.substr(1, value.length() - 2);
            }

            std::string fullKey = currentSection.empty() ? key : currentSection + "." + key;
            result[fullKey] = value;
        }
    }

    return result;
}

} // anonymous namespace

bool Config::load(const std::string& path) {
    setDefaults();

#if HAVE_YAML_CPP
    LOG_INFO("Using yaml-cpp parser for config");
    try {
        YAML::Node config = YAML::LoadFile(path);

        // Database
        if (config["database"]) {
            if (config["database"]["path"]) {
                m_databasePath = config["database"]["path"].as<std::string>();
            }
        }

        // MQTT
        if (config["mqtt"]) {
            if (config["mqtt"]["broker"]) {
                m_mqttBroker = config["mqtt"]["broker"].as<std::string>();
            }
            if (config["mqtt"]["port"]) {
                m_mqttPort = config["mqtt"]["port"].as<int>();
            }
            if (config["mqtt"]["client_id"]) {
                m_mqttClientId = config["mqtt"]["client_id"].as<std::string>();
            }
            if (config["mqtt"]["username"]) {
                m_mqttUsername = config["mqtt"]["username"].as<std::string>();
            }
            if (config["mqtt"]["password"]) {
                m_mqttPassword = config["mqtt"]["password"].as<std::string>();
            }
        }

        // Web
        if (config["web"]) {
            if (config["web"]["port"]) {
                m_webPort = config["web"]["port"].as<int>();
            }
            if (config["web"]["http_port"]) {
                m_webHttpPort = config["web"]["http_port"].as<int>();
            }
            if (config["web"]["root"]) {
                m_webRoot = config["web"]["root"].as<std::string>();
            }
            if (config["web"]["tls"]) {
                if (config["web"]["tls"]["cert"]) {
                    m_tlsCertPath = config["web"]["tls"]["cert"].as<std::string>();
                }
                if (config["web"]["tls"]["key"]) {
                    m_tlsKeyPath = config["web"]["tls"]["key"].as<std::string>();
                }
            }
        }

        // Display
        if (config["display"]) {
            if (config["display"]["device"]) {
                m_displayDevice = config["display"]["device"].as<std::string>();
            }
            if (config["display"]["touch_device"]) {
                m_touchDevice = config["display"]["touch_device"].as<std::string>();
            }
            if (config["display"]["brightness"]) {
                m_displayBrightness = config["display"]["brightness"].as<int>();
            }
            if (config["display"]["screen_timeout"]) {
                m_screenTimeout = config["display"]["screen_timeout"].as<int>();
            }
        }

        // RPMsg
        if (config["rpmsg"]) {
            if (config["rpmsg"]["device"]) {
                m_rpmsgDevice = config["rpmsg"]["device"].as<std::string>();
            }
        }

        // Zigbee
        if (config["zigbee"]) {
            LOG_DEBUG("Found zigbee section in config");
            if (config["zigbee"]["enabled"]) {
                m_zigbeeEnabled = config["zigbee"]["enabled"].as<bool>();
                LOG_DEBUG("Zigbee enabled parsed as: %d", m_zigbeeEnabled);
            } else {
                LOG_DEBUG("No 'enabled' key in zigbee section");
            }
            if (config["zigbee"]["port"]) {
                m_zigbeePort = config["zigbee"]["port"].as<std::string>();
            }
            if (config["zigbee"]["baud"]) {
                m_zigbeeBaudRate = config["zigbee"]["baud"].as<int>();
            }
        } else {
            LOG_DEBUG("No zigbee section in config");
        }

        // Logging
        if (config["logging"]) {
            if (config["logging"]["level"]) {
                m_logLevel = config["logging"]["level"].as<std::string>();
            }
            if (config["logging"]["file"]) {
                m_logFile = config["logging"]["file"].as<std::string>();
            }
        }

        m_loaded = true;
        return true;

    } catch (const YAML::Exception& e) {
        LOG_ERROR("Failed to parse config file %s: %s", path.c_str(), e.what());
        return false;
    }

#else
    // Fallback: simple INI-style parsing
    LOG_INFO("Using fallback INI parser for config (yaml-cpp not available)");
    auto settings = parseSimpleConfig(path);

    if (settings.empty()) {
        LOG_WARN("Could not load config from %s, using defaults", path.c_str());
        m_loaded = true;
        return true;
    }

    // Map settings to member variables
    if (settings.count("database.path")) {
        m_databasePath = settings["database.path"];
    }
    if (settings.count("mqtt.broker")) {
        m_mqttBroker = settings["mqtt.broker"];
    }
    if (settings.count("mqtt.port")) {
        m_mqttPort = std::stoi(settings["mqtt.port"]);
    }
    if (settings.count("web.port")) {
        m_webPort = std::stoi(settings["web.port"]);
    }
    if (settings.count("web.root")) {
        m_webRoot = settings["web.root"];
    }
    if (settings.count("web.tls.cert")) {
        m_tlsCertPath = settings["web.tls.cert"];
    }
    if (settings.count("web.tls.key")) {
        m_tlsKeyPath = settings["web.tls.key"];
    }
    if (settings.count("display.device")) {
        m_displayDevice = settings["display.device"];
    }
    if (settings.count("rpmsg.device")) {
        m_rpmsgDevice = settings["rpmsg.device"];
    }
    if (settings.count("zigbee.enabled")) {
        std::string val = settings["zigbee.enabled"];
        std::transform(val.begin(), val.end(), val.begin(), ::tolower);
        m_zigbeeEnabled = (val == "true" || val == "yes" || val == "1");
    }
    if (settings.count("zigbee.port")) {
        m_zigbeePort = settings["zigbee.port"];
    }
    if (settings.count("zigbee.baud")) {
        m_zigbeeBaudRate = std::stoi(settings["zigbee.baud"]);
    }
    if (settings.count("logging.level")) {
        m_logLevel = settings["logging.level"];
    }
    if (settings.count("logging.file")) {
        m_logFile = settings["logging.file"];
    }

    m_loaded = true;
    return true;
#endif
}

bool Config::save(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open config file for writing: %s", path.c_str());
        return false;
    }

    file << "# SmartHub Configuration\n\n";

    file << "[database]\n";
    file << "path = " << m_databasePath << "\n\n";

    file << "[mqtt]\n";
    file << "broker = " << m_mqttBroker << "\n";
    file << "port = " << m_mqttPort << "\n";
    file << "client_id = " << m_mqttClientId << "\n\n";

    file << "[web]\n";
    file << "port = " << m_webPort << "\n";
    file << "http_port = " << m_webHttpPort << "\n";
    file << "root = " << m_webRoot << "\n\n";

    file << "[display]\n";
    file << "device = " << m_displayDevice << "\n";
    file << "touch_device = " << m_touchDevice << "\n";
    file << "brightness = " << m_displayBrightness << "\n";
    file << "screen_timeout = " << m_screenTimeout << "\n\n";

    file << "[rpmsg]\n";
    file << "device = " << m_rpmsgDevice << "\n\n";

    file << "[logging]\n";
    file << "level = " << m_logLevel << "\n";
    file << "file = " << m_logFile << "\n";

    return true;
}

void Config::setDefaults() {
    m_databasePath = "/var/lib/smarthub/smarthub.db";
    m_mqttBroker = "127.0.0.1";
    m_mqttPort = 1883;
    m_mqttClientId = "smarthub";
    m_mqttUsername = "";
    m_mqttPassword = "";
    m_webPort = 443;
    m_webHttpPort = 80;
    m_webRoot = "/opt/smarthub/www";
    m_tlsCertPath = "/etc/smarthub/cert.pem";
    m_tlsKeyPath = "/etc/smarthub/key.pem";
    m_displayDevice = "/dev/fb0";
    m_touchDevice = "/dev/input/event0";
    m_displayBrightness = 100;
    m_screenTimeout = 60;
    m_rpmsgDevice = "/dev/ttyRPMSG0";
    m_zigbeeEnabled = false;
    m_zigbeePort = "/dev/ttyUSB0";
    m_zigbeeBaudRate = 115200;
    m_logLevel = "info";
    m_logFile = "/var/log/smarthub.log";
}

std::optional<std::string> Config::getString(const std::string& key) const {
    auto it = m_customSettings.find(key);
    if (it != m_customSettings.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<int> Config::getInt(const std::string& key) const {
    auto str = getString(key);
    if (str) {
        try {
            return std::stoi(*str);
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<bool> Config::getBool(const std::string& key) const {
    auto str = getString(key);
    if (str) {
        std::string lower = *str;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return (lower == "true" || lower == "yes" || lower == "1");
    }
    return std::nullopt;
}

} // namespace smarthub
