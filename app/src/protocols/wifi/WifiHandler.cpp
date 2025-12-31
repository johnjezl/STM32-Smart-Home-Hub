/**
 * SmartHub WiFi Protocol Handler Implementation
 */

#include <smarthub/protocols/wifi/WifiHandler.hpp>
#include <smarthub/core/Logger.hpp>
#include <smarthub/devices/types/SwitchDevice.hpp>
#include <smarthub/devices/types/DimmerDevice.hpp>
#include <smarthub/devices/types/ColorLightDevice.hpp>
#include <smarthub/devices/types/TemperatureSensor.hpp>
#include <smarthub/devices/types/MotionSensor.hpp>

namespace smarthub::wifi {

WifiHandler::WifiHandler(EventBus& eventBus, const nlohmann::json& config)
    : m_eventBus(eventBus)
    , m_config(config)
    , m_lastPoll(std::chrono::steady_clock::now()) {

    // Parse configuration
    if (config.contains("mqtt")) {
        const auto& mqttConfig = config["mqtt"];
        m_mqttBroker = mqttConfig.value("broker", "localhost");
        m_mqttPort = mqttConfig.value("port", 1883);
    }

    // Set up MQTT discovery callbacks
    m_mqttDiscovery.setDiscoveryCallback(
        [this](const MqttDiscoveryConfig& config) {
            onMqttDiscovery(config);
        });

    m_mqttDiscovery.setStateCallback(
        [this](const std::string& id, const std::string& prop, const nlohmann::json& val) {
            onDeviceStateUpdate(id, prop, val);
        });

    m_mqttDiscovery.setAvailabilityCallback(
        [this](const std::string& id, bool available) {
            onDeviceAvailabilityUpdate(id, available);
        });
}

WifiHandler::~WifiHandler() {
    shutdown();
}

bool WifiHandler::initialize() {
    LOG_INFO("Initializing WiFi protocol handler");

    m_state = ProtocolState::Connecting;

    // Initialize HTTP client
    m_http = std::make_unique<http::HttpClient>();

    // Initialize Shelly discovery
    m_shellyDiscovery = std::make_unique<ShellyDiscovery>(*m_http);
    m_shellyDiscovery->setCallback([this](const ShellyDeviceInfo& info) {
        // Create Shelly device
        auto device = createShellyDevice(info, *m_http);
        if (device) {
            WifiDeviceEntry entry;
            entry.id = device->id();
            entry.type = "shelly";
            entry.device = nullptr;  // Stored in shellyDevice unique_ptr instead
            entry.shellyDevice = std::move(device);
            entry.available = true;

            addDevice(entry.id, std::move(entry));
        }
    });

    // Initialize MQTT if configured
    if (!m_mqttBroker.empty()) {
        setupMqtt();
    }

    m_connected = true;
    m_state = ProtocolState::Connected;

    LOG_INFO("WiFi protocol handler initialized");
    return true;
}

void WifiHandler::setupMqtt() {
    LOG_INFO("Connecting to MQTT broker %s:%d", m_mqttBroker.c_str(), m_mqttPort);

    m_mqtt = std::make_unique<MqttClient>(m_eventBus, m_mqttBroker, m_mqttPort);

    if (m_config.contains("mqtt")) {
        const auto& mqttConfig = m_config["mqtt"];
        if (mqttConfig.contains("username")) {
            m_mqtt->setCredentials(
                mqttConfig["username"].get<std::string>(),
                mqttConfig.value("password", "")
            );
        }
        if (mqttConfig.contains("clientId")) {
            m_mqtt->setClientId(mqttConfig["clientId"].get<std::string>());
        }
    }

    m_mqtt->setMessageCallback([this](const std::string& topic, const std::string& payload) {
        onMqttMessage(topic, payload);
    });

    if (m_mqtt->connect()) {
        // Subscribe to discovery topics
        std::string discoveryTopic = m_mqttDiscovery.getSubscriptionTopic();
        m_mqtt->subscribe(discoveryTopic);
        LOG_INFO("Subscribed to MQTT discovery: %s", discoveryTopic.c_str());
    } else {
        LOG_WARN("Failed to connect to MQTT broker");
        m_lastError = "Failed to connect to MQTT broker";
    }
}

void WifiHandler::shutdown() {
    LOG_INFO("Shutting down WiFi protocol handler");

    m_discovering = false;

    // Disconnect all Tuya devices
    {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        for (auto& [id, entry] : m_devices) {
            if (entry.tuyaDevice) {
                entry.tuyaDevice->disconnect();
            }
        }
    }

    // Disconnect MQTT
    if (m_mqtt) {
        m_mqtt->disconnect();
        m_mqtt.reset();
    }

    m_http.reset();
    m_shellyDiscovery.reset();

    m_connected = false;
    m_state = ProtocolState::Disconnected;
}

void WifiHandler::poll() {
    // Poll MQTT
    if (m_mqtt) {
        m_mqtt->poll();
    }

    // Periodic polling of HTTP devices
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastPoll).count();

    if (elapsed >= POLL_INTERVAL_MS) {
        m_lastPoll = now;
        pollShellyDevices();
    }
}

void WifiHandler::pollShellyDevices() {
    std::lock_guard<std::mutex> lock(m_deviceMutex);

    for (auto& [id, entry] : m_devices) {
        if (entry.type == "shelly" && entry.shellyDevice) {
            bool success = entry.shellyDevice->pollStatus();
            if (success != entry.available) {
                entry.available = success;
                onDeviceAvailabilityUpdate(id, success);
            }
        }
    }
}

void WifiHandler::onMqttMessage(const std::string& topic, const std::string& payload) {
    // Route to discovery manager
    m_mqttDiscovery.processMessage(topic, payload);
}

void WifiHandler::onMqttDiscovery(const MqttDiscoveryConfig& config) {
    LOG_INFO("MQTT device discovered: %s (%s)",
             config.name.c_str(), config.component.c_str());

    // Create device based on discovery config
    auto device = createMqttDevice(config);
    if (!device) {
        return;
    }

    WifiDeviceEntry entry;
    entry.id = config.uniqueId;
    entry.type = "mqtt";
    entry.device = device;
    entry.mqttConfig = config;
    entry.available = true;

    // Subscribe to state topics
    if (m_mqtt && !config.stateTopic.empty()) {
        m_mqtt->subscribe(config.stateTopic);
    }
    if (m_mqtt && !config.availabilityTopic.empty()) {
        m_mqtt->subscribe(config.availabilityTopic);
    }
    if (m_mqtt && !config.brightnessStateTopic.empty()) {
        m_mqtt->subscribe(config.brightnessStateTopic);
    }
    if (m_mqtt && !config.colorTempStateTopic.empty()) {
        m_mqtt->subscribe(config.colorTempStateTopic);
    }

    addDevice(config.uniqueId, std::move(entry));
}

DevicePtr WifiHandler::createMqttDevice(const MqttDiscoveryConfig& config) {
    DevicePtr device;

    switch (config.deviceClass()) {
        case MqttDeviceClass::Switch:
        case MqttDeviceClass::Outlet:
            device = std::make_shared<SwitchDevice>(
                config.uniqueId, config.name);
            break;

        case MqttDeviceClass::Light:
            if (!config.brightnessCommandTopic.empty() ||
                !config.colorTempCommandTopic.empty() ||
                !config.rgbCommandTopic.empty()) {
                // Dimmable or color light
                if (!config.rgbCommandTopic.empty() || !config.colorTempCommandTopic.empty()) {
                    device = std::make_shared<ColorLightDevice>(
                        config.uniqueId, config.name);
                } else {
                    device = std::make_shared<DimmerDevice>(
                        config.uniqueId, config.name);
                }
            } else {
                // Simple on/off light
                device = std::make_shared<SwitchDevice>(
                    config.uniqueId, config.name);
            }
            break;

        case MqttDeviceClass::Sensor:
            if (config.unitOfMeasurement == "°C" || config.unitOfMeasurement == "°F") {
                device = std::make_shared<TemperatureSensor>(
                    config.uniqueId, config.name);
            } else {
                // Generic sensor - use switch as placeholder
                device = std::make_shared<SwitchDevice>(
                    config.uniqueId, config.name);
            }
            break;

        case MqttDeviceClass::BinarySensor:
            device = std::make_shared<MotionSensor>(
                config.uniqueId, config.name);
            break;

        default:
            device = std::make_shared<SwitchDevice>(
                config.uniqueId, config.name);
            break;
    }

    if (device) {
        // Cast to Device to access setProtocol/setAddress
        auto deviceImpl = std::dynamic_pointer_cast<Device>(device);
        if (deviceImpl) {
            deviceImpl->setProtocol("mqtt");
            deviceImpl->setAddress(config.nodeId);
        }

        // Set manufacturer/model if available
        if (!config.device.manufacturer.empty()) {
            device->setState("manufacturer", config.device.manufacturer);
        }
        if (!config.device.model.empty()) {
            device->setState("model", config.device.model);
        }
    }

    return device;
}

void WifiHandler::onDeviceStateUpdate(const std::string& deviceId,
                                       const std::string& property,
                                       const nlohmann::json& value) {
    std::lock_guard<std::mutex> lock(m_deviceMutex);

    auto it = m_devices.find(deviceId);
    if (it != m_devices.end() && it->second.device) {
        it->second.device->setState(property, value);
    }

    if (m_stateCallback) {
        m_stateCallback(deviceId, property, value);
    }
}

void WifiHandler::onDeviceAvailabilityUpdate(const std::string& deviceId, bool available) {
    std::lock_guard<std::mutex> lock(m_deviceMutex);

    auto it = m_devices.find(deviceId);
    if (it != m_devices.end()) {
        it->second.available = available;
        if (it->second.device) {
            // Cast to Device to access setAvailability
            auto deviceImpl = std::dynamic_pointer_cast<Device>(it->second.device);
            if (deviceImpl) {
                deviceImpl->setAvailability(
                    available ? DeviceAvailability::Online : DeviceAvailability::Offline);
            }
        }
    }

    if (m_availabilityCallback) {
        m_availabilityCallback(deviceId,
            available ? DeviceAvailability::Online : DeviceAvailability::Offline);
    }
}

void WifiHandler::addDevice(const std::string& id, WifiDeviceEntry entry) {
    {
        std::lock_guard<std::mutex> lock(m_deviceMutex);

        // Check if device already exists
        if (m_devices.find(id) != m_devices.end()) {
            // Update existing entry
            m_devices[id] = std::move(entry);
            return;
        }

        m_devices[id] = std::move(entry);
    }

    // Notify discovery callback
    auto& addedEntry = m_devices[id];
    if (m_discoveredCallback && addedEntry.device) {
        m_discoveredCallback(addedEntry.device);
    }
}

void WifiHandler::removeDevice(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_deviceMutex);

    auto it = m_devices.find(id);
    if (it != m_devices.end()) {
        // Disconnect Tuya device if applicable
        if (it->second.tuyaDevice) {
            it->second.tuyaDevice->disconnect();
        }

        m_devices.erase(it);
    }
}

ProtocolState WifiHandler::state() const {
    return m_state;
}

void WifiHandler::startDiscovery() {
    LOG_INFO("Starting WiFi device discovery");
    m_discovering = true;

    // For now, discovery happens automatically via MQTT
    // and manually via addShellyDevice/addTuyaDevice
}

void WifiHandler::stopDiscovery() {
    m_discovering = false;
}

bool WifiHandler::sendCommand(const std::string& deviceAddress,
                               const std::string& command,
                               const nlohmann::json& params) {
    std::lock_guard<std::mutex> lock(m_deviceMutex);

    auto it = m_devices.find(deviceAddress);
    if (it == m_devices.end()) {
        LOG_WARN("Device not found: %s", deviceAddress.c_str());
        return false;
    }

    auto& entry = it->second;

    if (entry.type == "mqtt") {
        // Send MQTT command
        if (!m_mqtt) {
            return false;
        }

        std::string topic;
        std::string payload;

        if (command == "on" || command == "off") {
            topic = entry.mqttConfig.commandTopic;
            payload = (command == "on") ? entry.mqttConfig.payloadOn
                                         : entry.mqttConfig.payloadOff;
        } else if (command == "brightness") {
            topic = entry.mqttConfig.brightnessCommandTopic;
            if (params.contains("brightness")) {
                payload = std::to_string(params["brightness"].get<int>());
            }
        } else if (command == "colorTemp") {
            topic = entry.mqttConfig.colorTempCommandTopic;
            if (params.contains("colorTemp")) {
                payload = std::to_string(params["colorTemp"].get<int>());
            }
        }

        if (!topic.empty()) {
            return m_mqtt->publish(topic, payload);
        }
    } else if (entry.type == "shelly" && entry.shellyDevice) {
        // Send Shelly command
        auto* gen1 = dynamic_cast<ShellyGen1Device*>(entry.shellyDevice.get());
        auto* gen2 = dynamic_cast<ShellyGen2Device*>(entry.shellyDevice.get());

        int channel = params.value("channel", 0);

        if (command == "on") {
            return gen1 ? gen1->turnOn(channel) : (gen2 ? gen2->turnOn(channel) : false);
        } else if (command == "off") {
            return gen1 ? gen1->turnOff(channel) : (gen2 ? gen2->turnOff(channel) : false);
        } else if (command == "toggle") {
            return gen1 ? gen1->toggle(channel) : (gen2 ? gen2->toggle(channel) : false);
        } else if (command == "brightness" && params.contains("brightness")) {
            int level = params["brightness"].get<int>();
            return gen1 ? gen1->setBrightness(channel, level)
                        : (gen2 ? gen2->setBrightness(channel, level) : false);
        }
    } else if (entry.type == "tuya" && entry.tuyaDevice) {
        // Send Tuya command
        if (command == "on") {
            return entry.tuyaDevice->setDataPoint(1, true);
        } else if (command == "off") {
            return entry.tuyaDevice->setDataPoint(1, false);
        } else if (command == "brightness" && params.contains("brightness")) {
            int level = params["brightness"].get<int>() * 10;  // 0-100 to 0-1000
            return entry.tuyaDevice->setDataPoint(2, level);
        }
    }

    return false;
}

void WifiHandler::setDeviceDiscoveredCallback(DeviceDiscoveredCallback cb) {
    m_discoveredCallback = std::move(cb);
}

void WifiHandler::setDeviceStateCallback(DeviceStateCallback cb) {
    m_stateCallback = std::move(cb);
}

void WifiHandler::setDeviceAvailabilityCallback(DeviceAvailabilityCallback cb) {
    m_availabilityCallback = std::move(cb);
}

nlohmann::json WifiHandler::getStatus() const {
    std::lock_guard<std::mutex> lock(m_deviceMutex);

    nlohmann::json status;
    status["connected"] = m_connected;
    status["discovering"] = m_discovering;
    status["deviceCount"] = m_devices.size();

    if (m_mqtt) {
        status["mqtt"] = {
            {"broker", m_mqttBroker},
            {"port", m_mqttPort},
            {"connected", m_mqtt->isConnected()}
        };
    }

    // Count by type
    int mqttCount = 0, shellyCount = 0, tuyaCount = 0;
    for (const auto& [id, entry] : m_devices) {
        if (entry.type == "mqtt") mqttCount++;
        else if (entry.type == "shelly") shellyCount++;
        else if (entry.type == "tuya") tuyaCount++;
    }

    status["devices"] = {
        {"mqtt", mqttCount},
        {"shelly", shellyCount},
        {"tuya", tuyaCount}
    };

    return status;
}

std::vector<std::string> WifiHandler::getKnownDeviceAddresses() const {
    std::lock_guard<std::mutex> lock(m_deviceMutex);

    std::vector<std::string> addresses;
    addresses.reserve(m_devices.size());
    for (const auto& [id, _] : m_devices) {
        addresses.push_back(id);
    }
    return addresses;
}

bool WifiHandler::addShellyDevice(const std::string& ipAddress) {
    if (!m_http || !m_shellyDiscovery) {
        return false;
    }

    auto info = m_shellyDiscovery->probeDevice(ipAddress);
    if (!info) {
        LOG_WARN("No Shelly device found at %s", ipAddress.c_str());
        return false;
    }

    auto device = createShellyDevice(*info, *m_http);
    if (!device) {
        return false;
    }

    WifiDeviceEntry entry;
    entry.id = device->id();
    entry.type = "shelly";
    entry.device = nullptr;  // ShellyDevice doesn't inherit shared_from_this easily
    entry.shellyDevice = std::move(device);
    entry.available = true;

    // Poll initial status
    if (entry.shellyDevice) {
        entry.shellyDevice->pollStatus();
    }

    addDevice(entry.id, std::move(entry));
    return true;
}

bool WifiHandler::addTuyaDevice(const TuyaDeviceConfig& config) {
    auto device = std::make_unique<TuyaDevice>(
        "tuya_" + config.deviceId,
        config.name.empty() ? ("Tuya " + config.deviceId) : config.name,
        config);

    device->setStateCallback([this](const std::string& deviceId,
                                     const std::map<uint8_t, TuyaDataPoint>& dps) {
        // Convert DPs to state updates
        for (const auto& [dpId, dp] : dps) {
            std::string property;
            switch (dpId) {
                case 1: property = "on"; break;
                case 2: property = "brightness"; break;
                case 3: property = "colorTemp"; break;
                default: property = "dp" + std::to_string(dpId); break;
            }
            onDeviceStateUpdate(deviceId, property, dp.value);
        }
    });

    device->setConnectionCallback([this](const std::string& deviceId, bool connected) {
        onDeviceAvailabilityUpdate(deviceId, connected);
    });

    if (!device->connect()) {
        LOG_ERROR("Failed to connect to Tuya device %s", config.deviceId.c_str());
        return false;
    }

    WifiDeviceEntry entry;
    entry.id = device->id();
    entry.type = "tuya";
    entry.device = nullptr;
    entry.tuyaDevice = std::move(device);
    entry.available = true;

    addDevice(entry.id, std::move(entry));
    return true;
}

DevicePtr WifiHandler::getDevice(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_deviceMutex);

    auto it = m_devices.find(id);
    if (it != m_devices.end()) {
        return it->second.device;
    }
    return nullptr;
}

} // namespace smarthub::wifi
