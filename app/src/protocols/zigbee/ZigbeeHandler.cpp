/**
 * Zigbee Protocol Handler Implementation
 */

#include <smarthub/protocols/zigbee/ZigbeeHandler.hpp>
#include <smarthub/core/Logger.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/devices/types/SwitchDevice.hpp>
#include <smarthub/devices/types/DimmerDevice.hpp>
#include <smarthub/devices/types/ColorLightDevice.hpp>
#include <smarthub/devices/types/TemperatureSensor.hpp>
#include <smarthub/devices/types/MotionSensor.hpp>
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>

namespace smarthub::zigbee {

// ============================================================================
// Construction / Destruction
// ============================================================================

ZigbeeHandler::ZigbeeHandler(EventBus& eventBus, const nlohmann::json& config)
    : m_eventBus(eventBus)
    , m_config(config)
{
}

ZigbeeHandler::~ZigbeeHandler() {
    shutdown();
}

// ============================================================================
// IProtocolHandler Implementation
// ============================================================================

bool ZigbeeHandler::initialize() {
    if (m_initialized) {
        LOG_WARN("ZigbeeHandler already initialized");
        return true;
    }

    // Extract configuration
    std::string port = m_config.value("port", "/dev/ttyUSB0");
    int baudRate = m_config.value("baudRate", 115200);

    LOG_INFO("Initializing Zigbee handler on {} at {} baud", port, baudRate);

    // Create coordinator
    m_coordinator = std::make_unique<ZigbeeCoordinator>(port, baudRate);

    // Set up callbacks
    m_coordinator->setDeviceAnnouncedCallback(
        [this](uint16_t nwkAddr, uint64_t ieeeAddr) {
            onDeviceAnnounced(nwkAddr, ieeeAddr);
        });

    m_coordinator->setDeviceLeftCallback(
        [this](uint64_t ieeeAddr) {
            onDeviceLeft(ieeeAddr);
        });

    m_coordinator->setAttributeReportCallback(
        [this](uint16_t nwkAddr, const ZclAttributeValue& attr) {
            onAttributeReport(nwkAddr, attr);
        });

    m_coordinator->setCommandReceivedCallback(
        [this](uint16_t nwkAddr, uint8_t endpoint, uint16_t cluster,
               uint8_t command, const std::vector<uint8_t>& payload) {
            onCommandReceived(nwkAddr, endpoint, cluster, command, payload);
        });

    // Initialize coordinator
    if (!m_coordinator->initialize()) {
        m_lastError = "Failed to initialize Zigbee coordinator";
        LOG_ERROR("{}", m_lastError);
        m_coordinator.reset();
        return false;
    }

    // Start network
    if (!m_coordinator->startNetwork()) {
        m_lastError = "Failed to start Zigbee network";
        LOG_ERROR("{}", m_lastError);
        m_coordinator.reset();
        return false;
    }

    // Load device database if path provided
    if (m_config.contains("deviceDatabase")) {
        std::string dbPath = m_config["deviceDatabase"];
        if (!loadDeviceDatabase(dbPath)) {
            LOG_WARN("Failed to load device database from {}", dbPath);
        }
    }

    m_initialized = true;
    LOG_INFO("Zigbee handler initialized successfully");

    return true;
}

void ZigbeeHandler::shutdown() {
    if (!m_initialized) {
        return;
    }

    LOG_INFO("Shutting down Zigbee handler");

    if (m_discovering) {
        stopDiscovery();
    }

    if (m_coordinator) {
        m_coordinator->shutdown();
        m_coordinator.reset();
    }

    // Clear device mappings
    {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        m_ieeeToDeviceId.clear();
        m_deviceIdToIeee.clear();
        m_deviceEndpoints.clear();
    }

    m_initialized = false;
    LOG_INFO("Zigbee handler shutdown complete");
}

void ZigbeeHandler::poll() {
    // The coordinator handles async messages via its reader thread
    // This method can be used for periodic tasks like availability checking
}

ProtocolState ZigbeeHandler::state() const {
    if (!m_initialized) {
        return ProtocolState::Disconnected;
    }
    if (m_coordinator && m_coordinator->isNetworkUp()) {
        return ProtocolState::Connected;
    }
    return ProtocolState::Error;
}

void ZigbeeHandler::startDiscovery() {
    if (!m_initialized || !m_coordinator) {
        m_lastError = "Cannot start discovery: handler not initialized";
        LOG_ERROR("{}", m_lastError);
        return;
    }

    LOG_INFO("Starting Zigbee device discovery (permit join)");

    // Enable permit join for 60 seconds
    if (m_coordinator->permitJoin(60)) {
        m_discovering = true;
    } else {
        m_lastError = "Failed to enable permit join";
        LOG_ERROR("{}", m_lastError);
    }
}

void ZigbeeHandler::stopDiscovery() {
    if (!m_initialized || !m_coordinator) {
        return;
    }

    LOG_INFO("Stopping Zigbee device discovery");

    // Disable permit join
    m_coordinator->permitJoin(0);
    m_discovering = false;
}

bool ZigbeeHandler::sendCommand(const std::string& deviceAddress,
                                 const std::string& command,
                                 const nlohmann::json& params) {
    if (!m_initialized || !m_coordinator) {
        m_lastError = "Cannot send command: handler not initialized";
        LOG_ERROR("{}", m_lastError);
        return false;
    }

    // Look up IEEE address from device ID
    auto ieeeOpt = deviceIdToIeee(deviceAddress);
    if (!ieeeOpt) {
        m_lastError = "Unknown device address: " + deviceAddress;
        LOG_ERROR("{}", m_lastError);
        return false;
    }

    uint64_t ieeeAddr = *ieeeOpt;

    // Get device info from coordinator
    auto deviceInfo = m_coordinator->getDevice(ieeeAddr);
    if (!deviceInfo) {
        m_lastError = "Device " + deviceAddress + " not found in coordinator";
        LOG_ERROR("{}", m_lastError);
        return false;
    }

    // Get primary endpoint
    uint8_t endpoint = 1;
    {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        auto it = m_deviceEndpoints.find(ieeeAddr);
        if (it != m_deviceEndpoints.end()) {
            endpoint = it->second;
        }
    }

    LOG_DEBUG("Sending command '{}' to {} (endpoint {})", command, deviceAddress, endpoint);

    // Route command based on type
    if (command == "on" || command == "off" || command == "toggle") {
        return handleOnOffCommand(deviceInfo->networkAddress, endpoint, params);
    } else if (command == "brightness" || command == "level") {
        return handleBrightnessCommand(deviceInfo->networkAddress, endpoint, params);
    } else if (command == "color" || command == "colorTemp" || command == "hue" || command == "saturation") {
        return handleColorCommand(deviceInfo->networkAddress, endpoint, params);
    }

    m_lastError = "Unknown command: " + command;
    LOG_ERROR("{}", m_lastError);
    return false;
}

void ZigbeeHandler::setDeviceDiscoveredCallback(DeviceDiscoveredCallback cb) {
    m_discoveredCb = std::move(cb);
}

void ZigbeeHandler::setDeviceStateCallback(DeviceStateCallback cb) {
    m_stateCb = std::move(cb);
}

void ZigbeeHandler::setDeviceAvailabilityCallback(DeviceAvailabilityCallback cb) {
    m_availabilityCb = std::move(cb);
}

void ZigbeeHandler::setPendingDeviceCallback(DeviceDiscoveredCallback cb) {
    m_pendingDeviceCb = std::move(cb);
}

DevicePtr ZigbeeHandler::getPendingDevice() const {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    return m_pendingDevice;
}

void ZigbeeHandler::clearPendingDevice() {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    m_pendingDevice.reset();
}

bool ZigbeeHandler::isConnected() const {
    return m_initialized && m_coordinator && m_coordinator->isNetworkUp();
}

nlohmann::json ZigbeeHandler::getStatus() const {
    nlohmann::json status;
    status["protocol"] = "zigbee";
    status["initialized"] = m_initialized;
    status["connected"] = isConnected();
    status["discovering"] = m_discovering;

    if (m_coordinator) {
        status["panId"] = m_coordinator->panId();
        status["channel"] = m_coordinator->channel();
        status["deviceCount"] = m_coordinator->deviceCount();

        // Format IEEE address
        std::ostringstream oss;
        oss << std::hex << std::setfill('0') << std::setw(16)
            << m_coordinator->ieeeAddress();
        status["ieeeAddress"] = oss.str();
    }

    return status;
}

std::vector<std::string> ZigbeeHandler::getKnownDeviceAddresses() const {
    std::vector<std::string> addresses;
    std::lock_guard<std::mutex> lock(m_deviceMutex);

    for (const auto& [ieee, deviceId] : m_ieeeToDeviceId) {
        addresses.push_back(deviceId);
    }

    return addresses;
}

// ============================================================================
// Zigbee-specific Methods
// ============================================================================

bool ZigbeeHandler::loadDeviceDatabase(const std::string& path) {
    return m_deviceDb.load(path);
}

// ============================================================================
// Internal Callbacks
// ============================================================================

void ZigbeeHandler::onDeviceAnnounced(uint16_t nwkAddr, uint64_t ieeeAddr) {
    LOG_INFO("Zigbee device announced: nwk=0x{:04X}, ieee=0x{:016X}", nwkAddr, ieeeAddr);

    // Publish event to notify UI that a device was detected
    if (m_discovering) {
        DeviceStateEvent detectEvent;
        detectEvent.deviceId = ieeeToDeviceId(ieeeAddr);
        detectEvent.property = "_detecting";
        detectEvent.value = "Retrieving device information...";
        m_eventBus.publish(detectEvent);
    }

    // Request device info (this triggers Active Endpoints and Simple Descriptor requests)
    m_coordinator->requestDeviceInfo(nwkAddr);

    // IMPORTANT: Don't block here! This callback runs on the ZNP reader thread.
    // Blocking would prevent reception of the Active Endpoints and Simple Descriptor responses.
    // Spawn a separate thread to wait for cluster info.
    std::thread([this, nwkAddr, ieeeAddr]() {
        // Wait for cluster info to be populated (up to 30 seconds)
        std::optional<ZigbeeDeviceInfo> deviceInfo;
        for (int i = 0; i < 300; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            deviceInfo = m_coordinator->getDeviceByNwkAddr(nwkAddr);
            if (deviceInfo && !deviceInfo->inClusters.empty()) {
                LOG_INFO("Got cluster info after {} ms", (i + 1) * 100);
                break;
            }
        }

        if (!deviceInfo) {
            LOG_WARN("Device info not available for 0x{:04X}", nwkAddr);
            return;
        }

        if (deviceInfo->inClusters.empty()) {
            LOG_ERROR("No cluster info received for 0x{:04X} after 30s timeout - cannot determine device type", nwkAddr);
            // Publish error event for UI
            if (m_discovering) {
                DeviceStateEvent errorEvent;
                errorEvent.deviceId = ieeeToDeviceId(deviceInfo->ieeeAddress);
                errorEvent.property = "_pairing_error";
                errorEvent.value = "Device did not respond with cluster information. Try re-pairing.";
                m_eventBus.publish(errorEvent);
            }
            return;
        }

        // Create SmartHub device from Zigbee device info
        auto device = createDeviceFromInfo(*deviceInfo);
        if (!device) {
            LOG_ERROR("Failed to create device for 0x{:016X}", ieeeAddr);
            return;
        }

        // Store mapping
        std::string deviceId = device->id();
        {
            std::lock_guard<std::mutex> lock(m_deviceMutex);
            m_ieeeToDeviceId[ieeeAddr] = deviceId;
            m_deviceIdToIeee[deviceId] = ieeeAddr;

            // Store primary endpoint (first application endpoint)
            if (!deviceInfo->endpoints.empty()) {
                // Skip endpoint 0 (ZDO)
                for (uint8_t ep : deviceInfo->endpoints) {
                    if (ep != 0) {
                        m_deviceEndpoints[ieeeAddr] = ep;
                        break;
                    }
                }
            }
        }

        // Set up attribute reporting for the device type
        DeviceType type = inferDeviceType(*deviceInfo);
        uint8_t endpoint = m_deviceEndpoints[ieeeAddr];
        setupReporting(nwkAddr, endpoint, type);

        // If in discovery mode, store as pending and notify via pending callback
        // Don't auto-add - let the UI wizard complete first
        if (m_discovering) {
            LOG_INFO("Device discovered during pairing - storing as pending");
            {
                std::lock_guard<std::mutex> lock(m_deviceMutex);
                m_pendingDevice = device;
            }
            if (m_pendingDeviceCb) {
                m_pendingDeviceCb(device);
            }
        } else {
            // Not in discovery mode - auto-add the device
            if (m_discoveredCb) {
                m_discoveredCb(device);
            }
        }
    }).detach();
}

void ZigbeeHandler::onDeviceLeft(uint64_t ieeeAddr) {
    LOG_INFO("Zigbee device left: ieee=0x{:016X}", ieeeAddr);

    std::string deviceId;
    {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        auto it = m_ieeeToDeviceId.find(ieeeAddr);
        if (it != m_ieeeToDeviceId.end()) {
            deviceId = it->second;
            m_deviceIdToIeee.erase(deviceId);
            m_ieeeToDeviceId.erase(it);
            m_deviceEndpoints.erase(ieeeAddr);
        }
    }

    // Notify availability callback
    if (!deviceId.empty() && m_availabilityCb) {
        m_availabilityCb(deviceId, DeviceAvailability::Offline);
    }
}

void ZigbeeHandler::onAttributeReport(uint16_t nwkAddr, const ZclAttributeValue& attr) {
    LOG_DEBUG("Attribute report: nwk=0x{:04X}, cluster=0x{:04X}, attr=0x{:04X}",
              nwkAddr, attr.clusterId, attr.attributeId);

    // Find device ID
    auto deviceInfo = m_coordinator->getDeviceByNwkAddr(nwkAddr);
    if (!deviceInfo) {
        return;
    }

    std::string deviceId;
    {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        auto it = m_ieeeToDeviceId.find(deviceInfo->ieeeAddress);
        if (it != m_ieeeToDeviceId.end()) {
            deviceId = it->second;
        }
    }

    if (deviceId.empty()) {
        return;
    }

    // Convert attribute to state update - call callback for each property
    switch (attr.clusterId) {
        case zcl::cluster::ON_OFF:
            if (attr.attributeId == zcl::attr::onoff::ON_OFF) {
                if (m_stateCb) {
                    m_stateCb(deviceId, "on", attr.asBool());
                }
            }
            break;

        case zcl::cluster::LEVEL_CONTROL:
            if (attr.attributeId == zcl::attr::level::CURRENT_LEVEL) {
                if (m_stateCb) {
                    m_stateCb(deviceId, "brightness", attr.asUint8());
                }
            }
            break;

        case zcl::cluster::COLOR_CONTROL:
            if (attr.attributeId == zcl::attr::color::CURRENT_HUE) {
                if (m_stateCb) {
                    m_stateCb(deviceId, "hue", attr.asUint8());
                }
            } else if (attr.attributeId == zcl::attr::color::CURRENT_SAT) {
                if (m_stateCb) {
                    m_stateCb(deviceId, "saturation", attr.asUint8());
                }
            } else if (attr.attributeId == zcl::attr::color::COLOR_TEMP) {
                if (m_stateCb) {
                    m_stateCb(deviceId, "colorTemp", attr.asUint16());
                }
            }
            break;

        case zcl::cluster::TEMPERATURE_MEASUREMENT:
            if (attr.attributeId == zcl::attr::temperature::MEASURED_VALUE) {
                // Temperature in 0.01°C units
                if (m_stateCb) {
                    m_stateCb(deviceId, "temperature", attr.asInt16() / 100.0);
                }
            }
            break;

        case zcl::cluster::RELATIVE_HUMIDITY:
            if (attr.attributeId == zcl::attr::humidity::MEASURED_VALUE) {
                // Humidity in 0.01% units
                if (m_stateCb) {
                    m_stateCb(deviceId, "humidity", attr.asUint16() / 100.0);
                }
            }
            break;

        case zcl::cluster::IAS_ZONE:
            if (attr.attributeId == zcl::attr::ias_zone::ZONE_STATUS) {
                uint16_t status = attr.asUint16();
                if (m_stateCb) {
                    m_stateCb(deviceId, "alarm", (status & 0x01) != 0);
                    m_stateCb(deviceId, "tamper", (status & 0x04) != 0);
                    m_stateCb(deviceId, "lowBattery", (status & 0x08) != 0);
                }
            }
            break;

        default:
            LOG_DEBUG("Unhandled cluster 0x{:04X} in attribute report", attr.clusterId);
            break;
    }
}

void ZigbeeHandler::onCommandReceived(uint16_t nwkAddr, uint8_t endpoint,
                                       uint16_t cluster, uint8_t command,
                                       const std::vector<uint8_t>& payload) {
    LOG_DEBUG("Command received: nwk=0x{:04X}, ep={}, cluster=0x{:04X}, cmd={}",
              nwkAddr, endpoint, cluster, command);

    // Find device ID
    auto deviceInfo = m_coordinator->getDeviceByNwkAddr(nwkAddr);
    if (!deviceInfo) {
        return;
    }

    std::string deviceId;
    {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        auto it = m_ieeeToDeviceId.find(deviceInfo->ieeeAddress);
        if (it != m_ieeeToDeviceId.end()) {
            deviceId = it->second;
        }
    }

    if (deviceId.empty()) {
        return;
    }

    // Handle specific cluster commands
    switch (cluster) {
        case zcl::cluster::ON_OFF:
            // Handle On/Off commands from switches
            if (command == zcl::cmd::onoff::OFF) {
                if (m_stateCb) {
                    m_stateCb(deviceId, "on", false);
                }
            } else if (command == zcl::cmd::onoff::ON) {
                if (m_stateCb) {
                    m_stateCb(deviceId, "on", true);
                }
            } else if (command == zcl::cmd::onoff::TOGGLE) {
                if (m_stateCb) {
                    m_stateCb(deviceId, "toggle", true);
                }
            }
            break;

        case zcl::cluster::IAS_ZONE:
            // Zone status change notification
            if (command == 0x00 && payload.size() >= 2) {  // Status change notification
                uint16_t status = payload[0] | (payload[1] << 8);
                if (m_stateCb) {
                    m_stateCb(deviceId, "alarm", (status & 0x01) != 0);
                    m_stateCb(deviceId, "tamper", (status & 0x04) != 0);
                    m_stateCb(deviceId, "lowBattery", (status & 0x08) != 0);
                }
            }
            break;

        default:
            break;
    }
}

// ============================================================================
// Device Creation
// ============================================================================

DevicePtr ZigbeeHandler::createDeviceFromInfo(const ZigbeeDeviceInfo& info) {
    // Generate device ID from IEEE address
    std::string deviceId = ieeeToDeviceId(info.ieeeAddress);

    // Infer device type from clusters
    DeviceType type = inferDeviceType(info);

    // Look up in device database for better info
    auto dbEntry = m_deviceDb.lookup(info.manufacturer, info.model);

    std::string name;
    if (dbEntry) {
        name = dbEntry->displayName;
        if (dbEntry->deviceType != DeviceType::Unknown) {
            type = dbEntry->deviceType;
        }
    } else {
        // Generate name from manufacturer/model
        if (!info.manufacturer.empty() && !info.model.empty()) {
            name = info.manufacturer + " " + info.model;
        } else {
            name = "Zigbee Device " + deviceId.substr(deviceId.length() - 4);
        }
    }

    // Create appropriate device type
    DevicePtr device;

    switch (type) {
        case DeviceType::Switch:
            device = std::make_shared<SwitchDevice>(deviceId, name, "zigbee");
            break;

        case DeviceType::Dimmer:
            device = std::make_shared<DimmerDevice>(deviceId, name, "zigbee");
            break;

        case DeviceType::ColorLight:
            device = std::make_shared<ColorLightDevice>(deviceId, name, "zigbee");
            break;

        case DeviceType::TemperatureSensor:
            device = std::make_shared<TemperatureSensor>(deviceId, name, "zigbee");
            break;

        case DeviceType::MotionSensor:
            device = std::make_shared<MotionSensor>(deviceId, name, "zigbee");
            break;

        default:
            // Create as generic switch
            LOG_WARN("Unknown device type, creating as switch: {}", name);
            device = std::make_shared<SwitchDevice>(deviceId, name, "zigbee");
            break;
    }

    // Set device metadata through individual properties
    if (device) {
        device->setState("ieeeAddress", ieeeToDeviceId(info.ieeeAddress));
        device->setState("networkAddress", static_cast<int>(info.networkAddress));
        device->setState("manufacturer", info.manufacturer);
        device->setState("model", info.model);
    }

    return device;
}

DeviceType ZigbeeHandler::inferDeviceType(const ZigbeeDeviceInfo& info) {
    // Check all endpoints for clusters
    bool hasOnOff = false;
    bool hasLevel = false;
    bool hasColor = false;
    bool hasTemperature = false;
    bool hasHumidity = false;
    bool hasIasZone = false;
    bool hasOccupancy = false;

    LOG_DEBUG("Inferring device type for {:016X}, {} endpoints with cluster info",
              info.ieeeAddress, info.inClusters.size());

    for (const auto& [endpoint, clusters] : info.inClusters) {
        LOG_DEBUG("  Endpoint {} has {} clusters", endpoint, clusters.size());
        for (uint16_t cluster : clusters) {
            LOG_DEBUG("    Cluster: 0x{:04X}", cluster);
            switch (cluster) {
                case zcl::cluster::ON_OFF:
                    hasOnOff = true;
                    break;
                case zcl::cluster::LEVEL_CONTROL:
                    hasLevel = true;
                    break;
                case zcl::cluster::COLOR_CONTROL:
                    hasColor = true;
                    break;
                case zcl::cluster::TEMPERATURE_MEASUREMENT:
                    hasTemperature = true;
                    break;
                case zcl::cluster::RELATIVE_HUMIDITY:
                    hasHumidity = true;
                    break;
                case zcl::cluster::IAS_ZONE:
                    hasIasZone = true;
                    break;
                case zcl::cluster::OCCUPANCY_SENSING:
                    hasOccupancy = true;
                    break;
            }
        }
    }

    LOG_DEBUG("Cluster flags: OnOff={}, Level={}, Color={}, Temp={}, Humidity={}, IAS={}, Occupancy={}",
              hasOnOff, hasLevel, hasColor, hasTemperature, hasHumidity, hasIasZone, hasOccupancy);

    // Determine type based on cluster combination
    if (hasColor && hasLevel) {
        return DeviceType::ColorLight;
    } else if (hasLevel && hasOnOff) {
        return DeviceType::Dimmer;
    } else if (hasTemperature || hasHumidity) {
        // Prioritize temperature sensor for sensors with temp/humidity clusters
        return DeviceType::TemperatureSensor;
    } else if (hasIasZone || hasOccupancy) {
        return DeviceType::MotionSensor;
    } else if (hasOnOff) {
        return DeviceType::Switch;
    }

    return DeviceType::Unknown;
}

void ZigbeeHandler::setupReporting(uint16_t nwkAddr, uint8_t endpoint, DeviceType type) {
    // Configure attribute reporting based on device type
    // Min interval: 1 second, Max interval: 5 minutes

    switch (type) {
        case DeviceType::Switch:
        case DeviceType::Dimmer:
        case DeviceType::ColorLight:
            // Report on/off state changes
            m_coordinator->configureReporting(nwkAddr, endpoint,
                zcl::cluster::ON_OFF, zcl::attr::onoff::ON_OFF,
                zcl::datatype::BOOLEAN, 1, 300);

            if (type == DeviceType::Dimmer || type == DeviceType::ColorLight) {
                // Report level changes
                m_coordinator->configureReporting(nwkAddr, endpoint,
                    zcl::cluster::LEVEL_CONTROL, zcl::attr::level::CURRENT_LEVEL,
                    zcl::datatype::UINT8, 1, 300, {0x01}); // 1% change
            }

            if (type == DeviceType::ColorLight) {
                // Report color temp changes
                m_coordinator->configureReporting(nwkAddr, endpoint,
                    zcl::cluster::COLOR_CONTROL, zcl::attr::color::COLOR_TEMP,
                    zcl::datatype::UINT16, 1, 300, {0x0A, 0x00}); // 10 mireds change
            }
            break;

        case DeviceType::TemperatureSensor:
            // Report temperature every 5 minutes or on 0.5°C change
            m_coordinator->configureReporting(nwkAddr, endpoint,
                zcl::cluster::TEMPERATURE_MEASUREMENT, zcl::attr::temperature::MEASURED_VALUE,
                zcl::datatype::INT16, 60, 300, {0x32, 0x00}); // 50 = 0.5°C
            break;

        case DeviceType::MotionSensor:
            // IAS Zone status is typically sent as notifications, not reports
            break;

        default:
            break;
    }
}

// ============================================================================
// Command Handling
// ============================================================================

bool ZigbeeHandler::handleOnOffCommand(uint16_t nwkAddr, uint8_t endpoint,
                                        const nlohmann::json& params) {
    bool on = params.value("on", true);

    if (params.contains("toggle") && params["toggle"].get<bool>()) {
        // Toggle command
        return m_coordinator->sendCommand(nwkAddr, endpoint,
            zcl::cluster::ON_OFF, zcl::cmd::onoff::TOGGLE);
    }

    return m_coordinator->setOnOff(nwkAddr, endpoint, on);
}

bool ZigbeeHandler::handleBrightnessCommand(uint16_t nwkAddr, uint8_t endpoint,
                                             const nlohmann::json& params) {
    if (!params.contains("brightness") && !params.contains("level")) {
        m_lastError = "Brightness command missing brightness/level parameter";
        LOG_ERROR("{}", m_lastError);
        return false;
    }

    uint8_t level = params.value("brightness", params.value("level", 254));
    uint16_t transitionTime = params.value("transitionTime", 10); // 1 second default

    return m_coordinator->setLevel(nwkAddr, endpoint, level, transitionTime);
}

bool ZigbeeHandler::handleColorCommand(uint16_t nwkAddr, uint8_t endpoint,
                                        const nlohmann::json& params) {
    uint16_t transitionTime = params.value("transitionTime", 10);

    if (params.contains("colorTemp")) {
        uint16_t colorTemp = params["colorTemp"];
        return m_coordinator->setColorTemp(nwkAddr, endpoint, colorTemp, transitionTime);
    }

    if (params.contains("hue") || params.contains("saturation")) {
        uint8_t hue = params.value("hue", 0);
        uint8_t sat = params.value("saturation", 254);
        return m_coordinator->setHueSat(nwkAddr, endpoint, hue, sat, transitionTime);
    }

    m_lastError = "Color command missing color parameters";
    LOG_ERROR("{}", m_lastError);
    return false;
}

// ============================================================================
// Address Conversion
// ============================================================================

std::string ZigbeeHandler::ieeeToDeviceId(uint64_t ieeeAddr) const {
    std::ostringstream oss;
    oss << "zigbee_" << std::hex << std::setfill('0') << std::setw(16) << ieeeAddr;
    return oss.str();
}

std::optional<uint64_t> ZigbeeHandler::deviceIdToIeee(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    auto it = m_deviceIdToIeee.find(deviceId);
    if (it != m_deviceIdToIeee.end()) {
        return it->second;
    }
    return std::nullopt;
}

} // namespace smarthub::zigbee

// Register with protocol factory
#include <smarthub/protocols/ProtocolFactory.hpp>

namespace {
    static bool _registered_zigbee = []() {
        smarthub::ProtocolFactory::instance().registerProtocol(
            "zigbee",
            [](smarthub::EventBus& eb, const nlohmann::json& cfg) -> smarthub::ProtocolHandlerPtr {
                return std::make_shared<smarthub::zigbee::ZigbeeHandler>(eb, cfg);
            },
            { "zigbee", "1.0.0", "Zigbee protocol via CC2652P coordinator" }
        );
        return true;
    }();
}
