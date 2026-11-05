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
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>

namespace smarthub::zigbee {

// ============================================================================
// Construction / Destruction
// ============================================================================

namespace {

// Devices hold brightness as 0-100 and color temperature in kelvin. ZCL uses a
// level of 0-254 and mireds.
constexpr int MAX_ZCL_LEVEL = 254;

uint8_t percentToLevel(double percent) {
    return static_cast<uint8_t>(std::lround(std::clamp(percent, 0.0, 100.0) * MAX_ZCL_LEVEL / 100.0));
}

int levelToPercent(uint8_t level) {
    return static_cast<int>(std::lround(std::min<int>(level, MAX_ZCL_LEVEL) * 100.0 / MAX_ZCL_LEVEL));
}

uint16_t kelvinToMireds(double kelvin) {
    return static_cast<uint16_t>(std::clamp<long>(std::lround(1000000.0 / kelvin), 1, 65279));
}

int miredsToKelvin(uint16_t mireds) {
    return static_cast<int>(std::lround(1000000.0 / mireds));
}

bool isApplicationEndpoint(uint8_t endpoint) {
    return endpoint != 0 && endpoint != GREEN_POWER_ENDPOINT;
}

/** The clusters that identify a device type's working endpoint, most specific first. */
std::vector<uint16_t> clustersFor(DeviceType type) {
    switch (type) {
        case DeviceType::ColorLight:
            return {zcl::cluster::COLOR_CONTROL, zcl::cluster::LEVEL_CONTROL, zcl::cluster::ON_OFF};
        case DeviceType::Dimmer:
            return {zcl::cluster::LEVEL_CONTROL, zcl::cluster::ON_OFF};
        case DeviceType::TemperatureSensor:
            return {zcl::cluster::TEMPERATURE_MEASUREMENT, zcl::cluster::RELATIVE_HUMIDITY};
        case DeviceType::HumiditySensor:
            return {zcl::cluster::RELATIVE_HUMIDITY};
        case DeviceType::MotionSensor:
            return {zcl::cluster::IAS_ZONE, zcl::cluster::OCCUPANCY_SENSING};
        case DeviceType::ContactSensor:
            return {zcl::cluster::IAS_ZONE};
        default:
            return {zcl::cluster::ON_OFF};
    }
}

/** The endpoint that does the work for this device type, or else the first application one. */
std::optional<uint8_t> primaryEndpoint(const ZigbeeDeviceInfo& info, DeviceType type) {
    for (uint16_t cluster : clustersFor(type)) {
        for (uint8_t endpoint : info.endpoints) {
            auto clusters = info.inClusters.find(endpoint);
            if (isApplicationEndpoint(endpoint) && clusters != info.inClusters.end() &&
                std::find(clusters->second.begin(), clusters->second.end(), cluster) !=
                    clusters->second.end()) {
                return endpoint;
            }
        }
    }
    for (uint8_t endpoint : info.endpoints) {
        if (isApplicationEndpoint(endpoint)) return endpoint;
    }
    return std::nullopt;
}

/** Reads a time setting in milliseconds, using the default if it's missing or out of range. */
int timeoutSetting(const nlohmann::json& config, const char* key, int defaultValue) {
    constexpr int MIN_MS = 100;
    constexpr int MAX_MS = 600000;

    auto it = config.find(key);
    if (it == config.end()) return defaultValue;
    if (!it->is_number_integer() || it->get<int64_t>() < MIN_MS || it->get<int64_t>() > MAX_MS) {
        LOG_WARN("Ignoring invalid Zigbee setting %s (must be %d-%d ms); using %d",
                 key, MIN_MS, MAX_MS, defaultValue);
        return defaultValue;
    }
    return it->get<int>();
}

} // namespace

ZigbeeHandler::ZigbeeHandler(EventBus& eventBus, const nlohmann::json& config)
    : m_eventBus(eventBus)
    , m_config(config)
{
}

ZigbeeHandler::ZigbeeHandler(EventBus& eventBus, const nlohmann::json& config,
                             CoordinatorFactory coordinatorFactory)
    : ZigbeeHandler(eventBus, config)
{
    m_coordinatorFactory = std::move(coordinatorFactory);
}

void ZigbeeHandler::fail(const std::string& message) {
    LOG_ERROR("%s", message.c_str());
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_lastError = message;
}

std::string ZigbeeHandler::lastError() const {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    return m_lastError;
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
    m_pairingTimeoutMs = timeoutSetting(m_config, "pairingTimeoutMs", 30000);
    m_pairingEndpointTimeoutMs = timeoutSetting(m_config, "pairingEndpointTimeoutMs", 5000);

    {
        std::lock_guard<std::mutex> lock(m_pairingMutex);
        m_stopPairing = false;
    }

    LOG_INFO("Initializing Zigbee handler on {} at {} baud", port, baudRate);

    // Create coordinator
    m_coordinator = m_coordinatorFactory
        ? m_coordinatorFactory()
        : std::make_unique<ZigbeeCoordinator>(port, baudRate);

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
        fail("Failed to initialize Zigbee coordinator");
        stopPairing();
        std::unique_lock<std::shared_mutex> lock(m_coordinatorMutex);
        m_coordinator.reset();
        return false;
    }

    // Start network
    if (!m_coordinator->startNetwork()) {
        fail("Failed to start Zigbee network");
        stopPairing();
        std::unique_lock<std::shared_mutex> lock(m_coordinatorMutex);
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

    // Before the coordinator goes away, since these threads use it
    stopPairing();

    if (m_discovering) {
        stopDiscovery();
    }

    m_initialized = false;
    if (m_coordinator) {
        // This waits for callbacks, which can be sending commands, so hold no lock yet.
        // Requests still in progress fail once the transport closes.
        m_coordinator->shutdown();

        std::unique_lock<std::shared_mutex> lock(m_coordinatorMutex);
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
        fail("Cannot start discovery: handler not initialized");
        return;
    }

    LOG_INFO("Starting Zigbee device discovery (permit join)");

    // Enable permit join for 60 seconds
    if (m_coordinator->permitJoin(60)) {
        m_discovering = true;
    } else {
        fail("Failed to enable permit join");
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
    // Any thread can get here, so keep the coordinator from going away during shutdown
    std::shared_lock<std::shared_mutex> coordinatorLock(m_coordinatorMutex);

    if (!m_initialized || !m_coordinator) {
        fail("Cannot send command: handler not initialized");
        return false;
    }

    // Look up IEEE address from device ID
    auto ieeeOpt = deviceIdToIeee(deviceAddress);
    if (!ieeeOpt) {
        fail("Unknown device address: " + deviceAddress);
        return false;
    }

    uint64_t ieeeAddr = *ieeeOpt;

    // Get device info from coordinator
    auto deviceInfo = m_coordinator->getDevice(ieeeAddr);
    if (!deviceInfo) {
        fail("Device " + deviceAddress + " not found in coordinator");
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
        return handleOnOffCommand(deviceInfo->networkAddress, endpoint, command);
    } else if (command == "brightness" || command == "level") {
        return handleBrightnessCommand(deviceInfo->networkAddress, endpoint, params);
    } else if (command == "color" || command == "colorTemp" || command == "hue" || command == "saturation") {
        return handleColorCommand(deviceInfo->networkAddress, endpoint, params);
    }

    fail("Unknown command: " + command);
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
    status["initialized"] = m_initialized.load();
    status["connected"] = isConnected();
    status["discovering"] = m_discovering.load();

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

    startPairing(nwkAddr, ieeeAddr);
}

void ZigbeeHandler::startPairing(uint16_t nwkAddr, uint64_t ieeeAddr) {
    std::lock_guard<std::mutex> lock(m_pairingMutex);
    if (m_stopPairing) {
        return;
    }

    // Clear out finished threads
    for (auto it = m_pairingTasks.begin(); it != m_pairingTasks.end();) {
        if (it->second.finished) {
            it->second.thread.join();
            it = m_pairingTasks.erase(it);
        } else {
            ++it;
        }
    }

    // Announcing again during pairing starts it over, in case the device has a new
    // address or missed a request.
    auto existing = m_pairingTasks.find(ieeeAddr);
    if (existing != m_pairingTasks.end()) {
        existing->second.nwkAddr = nwkAddr;
        existing->second.attempt++;
        m_pairingCv.notify_all();
        return;
    }

    // IMPORTANT: Don't block here! This runs on the ZNP reader thread, and blocking
    // would prevent reception of the Active Endpoints and Simple Descriptor responses.
    PairingTask& task = m_pairingTasks[ieeeAddr];
    task.nwkAddr = nwkAddr;
    task.thread = std::thread([this, ieeeAddr]() {
        for (;;) {
            uint64_t attempt = pairDevice(ieeeAddr);

            // An announce that arrived as pairing ended needs another pass.
            std::lock_guard<std::mutex> taskLock(m_pairingMutex);
            auto current = m_pairingTasks.find(ieeeAddr);
            if (m_stopPairing || current == m_pairingTasks.end()) {
                return;
            }
            if (current->second.attempt == attempt) {
                current->second.finished = true;
                return;
            }
        }
    });
}

void ZigbeeHandler::stopPairing() {
    std::map<uint64_t, PairingTask> tasks;
    {
        std::lock_guard<std::mutex> lock(m_pairingMutex);
        m_stopPairing = true;
        tasks.swap(m_pairingTasks);
    }
    m_pairingCv.notify_all();

    for (auto& [ieeeAddr, task] : tasks) {
        task.thread.join();
    }
}

bool ZigbeeHandler::pairingStopped() {
    std::lock_guard<std::mutex> lock(m_pairingMutex);
    return m_stopPairing;
}

ZigbeeHandler::PairingWait ZigbeeHandler::waitForPairing(uint64_t ieeeAddr, uint64_t attempt,
                                                         int timeoutMs,
                                                         const std::function<bool()>& ready) {
    constexpr int POLL_MS = 100;

    for (int elapsedMs = 0;; elapsedMs += POLL_MS) {
        if (ready()) {
            return PairingWait::Ready;
        }
        if (elapsedMs >= timeoutMs) {
            return PairingWait::TimedOut;
        }

        std::unique_lock<std::mutex> lock(m_pairingMutex);
        auto interrupted = [&] {
            auto task = m_pairingTasks.find(ieeeAddr);
            return m_stopPairing || task == m_pairingTasks.end() ||
                   task->second.attempt != attempt;
        };
        m_pairingCv.wait_for(lock, std::chrono::milliseconds(POLL_MS), interrupted);
        if (interrupted()) {
            auto task = m_pairingTasks.find(ieeeAddr);
            bool restarted = !m_stopPairing && task != m_pairingTasks.end();
            return restarted ? PairingWait::Restarted : PairingWait::Stopped;
        }
    }
}

uint64_t ZigbeeHandler::pairDevice(uint64_t ieeeAddr) {
    // Ask about one endpoint at a time and wait for its answer. A refusal doesn't say
    // which endpoint it's for, and some devices don't cope with many requests at once.
    uint64_t attempt = 0;
    int mayStillRefuse = 0;  // Requests left unanswered, whose refusal could still arrive
    for (;;) {
        uint16_t nwkAddr;
        {
            std::lock_guard<std::mutex> lock(m_pairingMutex);
            auto task = m_pairingTasks.find(ieeeAddr);
            if (m_stopPairing || task == m_pairingTasks.end()) {
                return attempt;
            }
            nwkAddr = task->second.nwkAddr;
            attempt = task->second.attempt;
        }

        // Which endpoints does it have?
        m_coordinator->requestDeviceInfo(nwkAddr);
        std::vector<uint8_t> endpoints;
        PairingWait result = waitForPairing(ieeeAddr, attempt, m_pairingTimeoutMs, [&] {
            auto info = m_coordinator->getDevice(ieeeAddr);
            if (!info) return true;  // It left
            endpoints = info->endpoints;
            return !endpoints.empty();
        });
        if (result == PairingWait::Stopped) return attempt;
        if (result == PairingWait::Restarted) continue;
        if (result == PairingWait::TimedOut) {
            reportPairingFailure(ieeeAddr, "Device did not report its endpoints. Try re-pairing.");
            return attempt;
        }

        // What does each one do?
        bool restart = false;
        for (uint8_t endpoint : endpoints) {
            if (!isApplicationEndpoint(endpoint)) continue;

            auto before = m_coordinator->getDevice(ieeeAddr);
            if (!before) return attempt;
            if (before->inClusters.count(endpoint) > 0) continue;

            m_coordinator->requestSimpleDescriptor(nwkAddr, endpoint);
            result = waitForPairing(ieeeAddr, attempt, m_pairingEndpointTimeoutMs, [&] {
                auto info = m_coordinator->getDevice(ieeeAddr);
                return !info ||  // It left
                       info->inClusters.count(endpoint) > 0 ||
                       info->failedDescriptors > before->failedDescriptors + mayStillRefuse;
            });
            if (result == PairingWait::Stopped) return attempt;
            if (result == PairingWait::Restarted) {
                ++mayStillRefuse;
                restart = true;
                break;
            }
            if (result == PairingWait::TimedOut) {
                LOG_WARN("Endpoint %d of 0x%04X didn't answer; continuing without it", endpoint, nwkAddr);
                ++mayStillRefuse;
            }
        }
        if (!restart) {
            break;
        }
    }

    finishPairing(ieeeAddr);
    return attempt;
}

void ZigbeeHandler::reportPairingFailure(uint64_t ieeeAddr, const std::string& message) {
    LOG_ERROR("Pairing 0x%016llX failed: %s", (unsigned long long)ieeeAddr, message.c_str());
    if (m_discovering) {
        DeviceStateEvent errorEvent;
        errorEvent.deviceId = ieeeToDeviceId(ieeeAddr);
        errorEvent.property = "_pairing_error";
        errorEvent.value = message;
        m_eventBus.publish(errorEvent);
    }
}

void ZigbeeHandler::finishPairing(uint64_t ieeeAddr) {
    auto deviceInfo = m_coordinator->getDevice(ieeeAddr);
    if (!deviceInfo) {
        return;  // It left
    }

    if (deviceInfo->inClusters.empty()) {
        reportPairingFailure(ieeeAddr,
            "Device did not respond with cluster information. Try re-pairing.");
        return;
    }

    // Create SmartHub device from Zigbee device info
    auto device = createDeviceFromInfo(*deviceInfo);
    if (!device) {
        LOG_ERROR("Failed to create device for 0x%016llX", (unsigned long long)ieeeAddr);
        return;
    }

    DeviceType type = resolveDeviceType(*deviceInfo);
    std::optional<uint8_t> endpoint = primaryEndpoint(*deviceInfo, type);

    // Store mapping
    std::string deviceId = device->id();
    {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        m_ieeeToDeviceId[ieeeAddr] = deviceId;
        m_deviceIdToIeee[deviceId] = ieeeAddr;
        if (endpoint) {
            m_deviceEndpoints[ieeeAddr] = *endpoint;
        }
    }

    // Set up attribute reporting for the device type
    if (endpoint) {
        setupReporting(deviceInfo->networkAddress, *endpoint, type);
    }

    if (pairingStopped()) {
        return;
    }

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
                    m_stateCb(deviceId, "brightness", levelToPercent(attr.asUint8()));
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
                if (m_stateCb && attr.asUint16() > 0) {
                    m_stateCb(deviceId, "colorTemp", miredsToKelvin(attr.asUint16()));
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

DeviceType ZigbeeHandler::resolveDeviceType(const ZigbeeDeviceInfo& info) {
    // The device database knows better than the clusters, if it has an answer
    auto dbEntry = m_deviceDb.lookup(info.manufacturer, info.model);
    if (dbEntry && dbEntry->deviceType != DeviceType::Unknown) {
        return dbEntry->deviceType;
    }
    return inferDeviceType(info);
}

DevicePtr ZigbeeHandler::createDeviceFromInfo(const ZigbeeDeviceInfo& info) {
    // Generate device ID from IEEE address
    std::string deviceId = ieeeToDeviceId(info.ieeeAddress);

    DeviceType type = resolveDeviceType(info);

    // Look up in device database for better info
    auto dbEntry = m_deviceDb.lookup(info.manufacturer, info.model);

    std::string name;
    if (dbEntry) {
        name = dbEntry->displayName;
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
            // Each request can block on an unresponsive radio, so check for shutdown between them
            if (pairingStopped()) return;
            m_coordinator->configureReporting(nwkAddr, endpoint,
                zcl::cluster::ON_OFF, zcl::attr::onoff::ON_OFF,
                zcl::datatype::BOOLEAN, 1, 300);

            if (type == DeviceType::Dimmer || type == DeviceType::ColorLight) {
                // Report level changes
                if (pairingStopped()) return;
                m_coordinator->configureReporting(nwkAddr, endpoint,
                    zcl::cluster::LEVEL_CONTROL, zcl::attr::level::CURRENT_LEVEL,
                    zcl::datatype::UINT8, 1, 300, {0x01}); // 1% change
            }

            if (type == DeviceType::ColorLight) {
                // Report color temp changes
                if (pairingStopped()) return;
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
                                        const std::string& command) {
    if (command == "toggle") {
        return m_coordinator->sendCommand(nwkAddr, endpoint,
            zcl::cluster::ON_OFF, zcl::cmd::onoff::TOGGLE);
    }

    return m_coordinator->setOnOff(nwkAddr, endpoint, command == "on");
}

bool ZigbeeHandler::handleBrightnessCommand(uint16_t nwkAddr, uint8_t endpoint,
                                             const nlohmann::json& params) {
    if (!params.contains("brightness") && !params.contains("level")) {
        fail("Brightness command missing brightness/level parameter");
        return false;
    }

    if (params.contains("brightness") && !params["brightness"].is_number()) {
        fail("Brightness must be a number");
        return false;
    }

    // "brightness" is the percentage devices use everywhere else; "level" is raw ZCL
    uint8_t level = params.contains("brightness")
        ? percentToLevel(params["brightness"].get<double>())
        : static_cast<uint8_t>(params.value("level", 254));
    uint16_t transitionTime = params.value("transitionTime", 10); // 1 second default

    return m_coordinator->setLevel(nwkAddr, endpoint, level, transitionTime);
}

bool ZigbeeHandler::handleColorCommand(uint16_t nwkAddr, uint8_t endpoint,
                                        const nlohmann::json& params) {
    uint16_t transitionTime = params.value("transitionTime", 10);

    if (params.contains("colorTemp")) {
        if (!params["colorTemp"].is_number() || params["colorTemp"].get<double>() <= 0) {
            fail("Color temperature must be a positive number of kelvin");
            return false;
        }
        return m_coordinator->setColorTemp(nwkAddr, endpoint,
            kelvinToMireds(params["colorTemp"].get<double>()), transitionTime);
    }

    if (params.contains("hue") || params.contains("saturation")) {
        uint8_t hue = params.value("hue", 0);
        uint8_t sat = params.value("saturation", 254);
        return m_coordinator->setHueSat(nwkAddr, endpoint, hue, sat, transitionTime);
    }

    fail("Color command missing color parameters");
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
