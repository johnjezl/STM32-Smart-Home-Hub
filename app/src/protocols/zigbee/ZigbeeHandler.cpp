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
#include <smarthub/devices/types/ContactSensor.hpp>
#include <cinttypes>
#include <cstdint>
#include <cctype>
#include <algorithm>
#include <cmath>
#include <cstdlib>
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

using ClusterEndpoints = std::map<uint16_t, std::vector<uint8_t>>;

/** The application endpoints with each cluster. The primary one comes first, as it does the work. */
ClusterEndpoints clusterEndpointsOf(const ZigbeeDeviceInfo& info, std::optional<uint8_t> primary) {
    ClusterEndpoints endpoints;
    const auto add = [&](uint8_t endpoint) {
        auto clusters = info.inClusters.find(endpoint);
        if (!isApplicationEndpoint(endpoint) || clusters == info.inClusters.end()) return;
        for (uint16_t cluster : clusters->second) {
            auto& list = endpoints[cluster];
            if (std::find(list.begin(), list.end(), endpoint) == list.end()) {
                list.push_back(endpoint);
            }
        }
    };
    if (primary) add(*primary);
    for (uint8_t endpoint : info.endpoints) add(endpoint);
    return endpoints;
}

nlohmann::json clusterEndpointsJson(const ClusterEndpoints& endpoints) {
    nlohmann::json json = nlohmann::json::object();
    for (const auto& [cluster, list] : endpoints) {
        json[std::to_string(cluster)] = list;
    }
    return json;
}

ClusterEndpoints clusterEndpointsFromJson(const nlohmann::json& json) {
    ClusterEndpoints endpoints;
    for (const auto& [cluster, list] : json.items()) {
        char* end = nullptr;
        const unsigned long id = std::strtoul(cluster.c_str(), &end, 10);
        if (cluster.empty() || *end != '\0' || id > 0xFFFF || !list.is_array()) continue;
        std::vector<uint8_t> numbers;
        for (const auto& endpoint : list) {
            if (endpoint.is_number_integer() && endpoint.get<int>() >= 1 && endpoint.get<int>() <= 240) {
                numbers.push_back(static_cast<uint8_t>(endpoint.get<int>()));
            }
        }
        if (!numbers.empty()) {
            endpoints[static_cast<uint16_t>(id)] = std::move(numbers);
        }
    }
    return endpoints;
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
    m_addressLookupIntervalMs = timeoutSetting(m_config, "addressLookupIntervalMs", 30000);
    m_pairingReadTimeoutMs = timeoutSetting(m_config, "pairingReadTimeoutMs", 3000);

    {
        std::lock_guard<std::mutex> lock(m_pairingMutex);
        m_stopPairing = false;
    }
    {
        std::lock_guard<std::mutex> lock(m_enrollMutex);
        m_stopEnrolling = false;
        m_enrollQueue.clear();
    }

    LOG_INFO("Initializing Zigbee handler on %s at %d baud", port.c_str(), baudRate);

    // Create coordinator
    m_coordinator = m_coordinatorFactory
        ? m_coordinatorFactory()
        : std::make_unique<ZigbeeCoordinator>(port, baudRate);

    // Set up callbacks
    m_coordinator->setDeviceAnnouncedCallback(
        [this](uint16_t nwkAddr, uint64_t ieeeAddr) {
            onDeviceAnnounced(nwkAddr, ieeeAddr);
        });

    m_coordinator->setNetworkAddressCallback(
        [this](uint16_t nwkAddr, uint64_t ieeeAddr, bool changed) {
            onNetworkAddressFound(nwkAddr, ieeeAddr, changed);
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

    m_enrollThread = std::thread(&ZigbeeHandler::enrollWorker, this);

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
            LOG_WARN("Failed to load device database from %s", dbPath.c_str());
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
        m_clusterEndpoints.clear();
        m_savedDevices.clear();
        m_addressChecks.clear();
        m_lastAddressLookup.clear();
        m_zoneAlarms.clear();
        m_declinedDevices.clear();
        m_pendingDevice.reset();
    }

    m_initialized = false;
    LOG_INFO("Zigbee handler shutdown complete");
}

void ZigbeeHandler::poll() {
    // The coordinator handles async messages via its reader thread

    // A saved address may be out of date, and a command to it could reach another device. These
    // lookups are broadcasts, so they're spread out.
    constexpr auto ADDRESS_CHECK_SPACING = std::chrono::seconds(1);
    const auto now = std::chrono::steady_clock::now();
    uint64_t ieeeAddr = 0;
    {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        if (m_addressChecks.empty() || now - m_lastAddressCheck < ADDRESS_CHECK_SPACING) {
            return;
        }
        ieeeAddr = m_addressChecks.front();
        m_addressChecks.pop_front();
        m_lastAddressCheck = now;
    }

    std::shared_lock<std::shared_mutex> coordinatorLock(m_coordinatorMutex);
    if (m_coordinator) {
        lookUpNetworkAddress(ieeeAddr);
    }
}

void ZigbeeHandler::lookUpNetworkAddress(uint64_t ieeeAddr) {
    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        auto last = m_lastAddressLookup.find(ieeeAddr);
        if (last != m_lastAddressLookup.end() &&
            now - last->second < std::chrono::milliseconds(m_addressLookupIntervalMs)) {
            return;  // These are broadcasts, and a command can be retried many times
        }
        m_lastAddressLookup[ieeeAddr] = now;
    }
    m_coordinator->requestNetworkAddress(ieeeAddr);
}

void ZigbeeHandler::onNetworkAddressFound(uint16_t nwkAddr, uint64_t ieeeAddr, bool changed) {
    const auto savedId = savedDeviceId(ieeeAddr);
    if (!savedId) {
        return;
    }

    notifyAvailability(*savedId, DeviceAvailability::Online);  // The network says it's there

    bool endpointKnown = false;
    {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        auto clusters = m_clusterEndpoints.find(ieeeAddr);
        endpointKnown = m_deviceEndpoints.count(ieeeAddr) > 0 && clusters != m_clusterEndpoints.end() &&
                        !clusters->second.empty();
    }

    if (changed) {
        notifyState(*savedId, "networkAddress", static_cast<int>(nwkAddr));
    }
    // Typed in by hand or saved by an older build, so it hasn't been asked about its endpoints
    if (changed || !endpointKnown) {
        startPairing(nwkAddr, ieeeAddr);
    }
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
        // Its network address isn't known yet. Ask again, so a later try can work.
        lookUpNetworkAddress(ieeeAddr);
        fail("Device " + deviceAddress + " not found in coordinator");
        return false;
    }

    LOG_DEBUG("Sending command '%s' to %s", command.c_str(), deviceAddress.c_str());

    // Route command based on type
    const uint16_t nwkAddr = deviceInfo->networkAddress;
    if (command == "on" || command == "off" || command == "toggle") {
        return handleOnOffCommand(nwkAddr, endpointFor(ieeeAddr, zcl::cluster::ON_OFF), command);
    } else if (command == "brightness" || command == "level") {
        return handleBrightnessCommand(nwkAddr, endpointFor(ieeeAddr, zcl::cluster::LEVEL_CONTROL), params);
    } else if (command == "color" || command == "colorTemp" || command == "hue" || command == "saturation") {
        return handleColorCommand(nwkAddr, endpointFor(ieeeAddr, zcl::cluster::COLOR_CONTROL), params);
    }

    fail("Unknown command: " + command);
    return false;
}

void ZigbeeHandler::setDeviceDiscoveredCallback(DeviceDiscoveredCallback cb) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_discoveredCb = std::move(cb);
}

void ZigbeeHandler::setDeviceStateCallback(DeviceStateCallback cb) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_stateCb = std::move(cb);
}

void ZigbeeHandler::setDeviceAvailabilityCallback(DeviceAvailabilityCallback cb) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_availabilityCb = std::move(cb);
}

void ZigbeeHandler::setPendingDeviceCallback(DeviceDiscoveredCallback cb) {
    std::lock_guard<std::recursive_mutex> lock(m_pendingCallbackMutex);
    m_pendingDeviceCb = std::move(cb);
}

void ZigbeeHandler::notifyState(const std::string& deviceId, const std::string& property,
                                const nlohmann::json& value) {
    DeviceStateCallback callback;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        callback = m_stateCb;
    }
    if (callback) {
        callback(deviceId, property, value);
    }
}

void ZigbeeHandler::notifyAvailability(const std::string& deviceId,
                                       DeviceAvailability availability) {
    DeviceAvailabilityCallback callback;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        callback = m_availabilityCb;
    }
    if (callback) {
        callback(deviceId, availability);
    }
}

DevicePtr ZigbeeHandler::getPendingDevice() const {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    return m_pendingDevice;
}

void ZigbeeHandler::eraseMappingLocked(uint64_t ieeeAddr) {
    auto it = m_ieeeToDeviceId.find(ieeeAddr);
    if (it != m_ieeeToDeviceId.end()) {
        m_deviceIdToIeee.erase(it->second);
        m_ieeeToDeviceId.erase(it);
    }
    m_deviceEndpoints.erase(ieeeAddr);
    m_clusterEndpoints.erase(ieeeAddr);
    m_zoneAlarms.erase(ieeeAddr);
}

std::optional<uint8_t> ZigbeeHandler::clusterEndpoint(uint64_t ieeeAddr, uint16_t cluster) const {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    auto device = m_clusterEndpoints.find(ieeeAddr);
    if (device != m_clusterEndpoints.end()) {
        auto found = device->second.find(cluster);
        if (found != device->second.end() && !found->second.empty()) {
            return found->second.front();
        }
    }
    return std::nullopt;
}

uint8_t ZigbeeHandler::endpointFor(uint64_t ieeeAddr, uint16_t cluster) const {
    if (const auto endpoint = clusterEndpoint(ieeeAddr, cluster)) {
        return *endpoint;
    }
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    auto primary = m_deviceEndpoints.find(ieeeAddr);
    return primary != m_deviceEndpoints.end() ? primary->second : 1;
}

bool ZigbeeHandler::isOtherGang(uint64_t ieeeAddr, uint16_t cluster, uint8_t endpoint) const {
    // Only what switches something. Two temperature endpoints aren't gangs, and a sensor may well
    // send from the second. An endpoint only sending this cluster, like a button, isn't listed.
    if (cluster != zcl::cluster::ON_OFF && cluster != zcl::cluster::LEVEL_CONTROL &&
        cluster != zcl::cluster::COLOR_CONTROL) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    auto device = m_clusterEndpoints.find(ieeeAddr);
    if (device == m_clusterEndpoints.end()) {
        return false;
    }
    auto found = device->second.find(cluster);
    return found != device->second.end() && !found->second.empty() && found->second.front() != endpoint &&
           std::find(found->second.begin(), found->second.end(), endpoint) != found->second.end();
}

void ZigbeeHandler::declineLocked(uint64_t ieeeAddr) {
    if (m_savedDevices.count(ieeeAddr) == 0) {  // Otherwise it was added
        m_declinedDevices.insert(ieeeAddr);
        eraseMappingLocked(ieeeAddr);
    }
}

void ZigbeeHandler::clearPendingDevice() {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    if (m_pendingDevice) {
        if (const auto ieeeAddr = parseIeeeAddress(m_pendingDevice->id())) {
            declineLocked(*ieeeAddr);
        }
    }
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

    addresses.reserve(m_ieeeToDeviceId.size());
    for (const auto& [ieee, deviceId] : m_ieeeToDeviceId) {
        addresses.push_back(deviceId);
    }

    return addresses;
}

void ZigbeeHandler::restoreDevice(const IDevice& device) {
    const nlohmann::json state = device.getState();

    // Typed in by hand, or set when it paired, or part of the id when it paired on an older build
    std::optional<uint64_t> ieeeAddr = parseIeeeAddress(device.protocolAddress());
    if (!ieeeAddr && state.contains("ieeeAddress") && state["ieeeAddress"].is_string()) {
        ieeeAddr = parseIeeeAddress(state["ieeeAddress"].get<std::string>());
    }
    if (!ieeeAddr) {
        ieeeAddr = parseIeeeAddress(device.id());
    }
    if (!ieeeAddr) {
        LOG_WARN("Zigbee device %s has no usable address", device.id().c_str());
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        auto owner = m_savedDevices.find(*ieeeAddr);
        if (owner != m_savedDevices.end() && owner->second != device.id()) {
            LOG_WARN("Zigbee device %s has the same address as %s, so it won't work",
                     device.id().c_str(), owner->second.c_str());
            return;
        }
        m_ieeeToDeviceId[*ieeeAddr] = device.id();
        m_deviceIdToIeee[device.id()] = *ieeeAddr;
        m_savedDevices[*ieeeAddr] = device.id();
        std::optional<uint16_t> zoneType;
        if (state.contains("zoneType") && state["zoneType"].is_number_integer()) {
            zoneType = static_cast<uint16_t>(state["zoneType"].get<int>());
        }
        m_zoneAlarms[*ieeeAddr] = zoneAlarmFor(device.type(), zoneType);
        if (state.contains("endpoint") && state["endpoint"].is_number_integer()) {
            const int endpoint = state["endpoint"].get<int>();
            if (endpoint >= 1 && endpoint <= 240) {
                m_deviceEndpoints[*ieeeAddr] = static_cast<uint8_t>(endpoint);
            }
        }
        if (state.contains("clusterEndpoints") && state["clusterEndpoints"].is_object()) {
            m_clusterEndpoints[*ieeeAddr] = clusterEndpointsFromJson(state["clusterEndpoints"]);
        }
    }

    std::shared_lock<std::shared_mutex> coordinatorLock(m_coordinatorMutex);
    if (!m_coordinator) {
        return;
    }

    int nwkAddr = 0;
    if (state.contains("networkAddress") && state["networkAddress"].is_number_integer()) {
        nwkAddr = state["networkAddress"].get<int>();
    }
    if (nwkAddr > 0 && nwkAddr <= 0xFFF7 &&
        m_coordinator->restoreDevice(*ieeeAddr, static_cast<uint16_t>(nwkAddr))) {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        m_addressChecks.push_back(*ieeeAddr);
    } else if (!m_coordinator->getDevice(*ieeeAddr)) {
        lookUpNetworkAddress(*ieeeAddr);
    }
}

void ZigbeeHandler::forgetDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    auto it = m_deviceIdToIeee.find(deviceId);
    if (it == m_deviceIdToIeee.end()) {
        return;
    }
    const uint64_t ieeeAddr = it->second;
    eraseMappingLocked(ieeeAddr);
    m_savedDevices.erase(ieeeAddr);
    m_declinedDevices.erase(ieeeAddr);
    m_addressChecks.erase(std::remove(m_addressChecks.begin(), m_addressChecks.end(), ieeeAddr),
                          m_addressChecks.end());
}

std::optional<std::string> ZigbeeHandler::savedDeviceId(uint64_t ieeeAddr) const {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    auto it = m_savedDevices.find(ieeeAddr);
    if (it == m_savedDevices.end()) {
        return std::nullopt;
    }
    return it->second;
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
    LOG_INFO("Zigbee device announced: nwk=0x%04X, ieee=0x%016" PRIX64, nwkAddr, ieeeAddr);

    // Devices announce when they power up, so this one may be nothing new
    const std::optional<std::string> savedId = savedDeviceId(ieeeAddr);
    if (savedId) {
        notifyState(*savedId, "networkAddress", static_cast<int>(nwkAddr));  // It may have moved
        notifyAvailability(*savedId, DeviceAvailability::Online);  // It may have left before this
    }

    // Publish event to notify UI that a device was detected
    if (m_discovering && !savedId) {
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
        existing->second.forWizard = existing->second.forWizard || m_discovering;
        m_pairingCv.notify_all();
        return;
    }

    // IMPORTANT: Don't block here! This runs on the ZNP reader thread, and blocking
    // would prevent reception of the Active Endpoints and Simple Descriptor responses.
    PairingTask& task = m_pairingTasks[ieeeAddr];
    task.nwkAddr = nwkAddr;
    task.forWizard = m_discovering;
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
    {
        std::shared_lock<std::shared_mutex> coordinatorLock(m_coordinatorMutex);
        if (m_coordinator) {
            m_coordinator->cancelReads();
        }
    }

    for (auto& [ieeeAddr, task] : tasks) {
        task.thread.join();
    }

    {
        std::lock_guard<std::mutex> lock(m_enrollMutex);
        m_stopEnrolling = true;
    }
    m_enrollCv.notify_all();
    if (m_enrollThread.joinable()) {
        m_enrollThread.join();
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

    const bool fetched = fetchDeviceDetails(ieeeAddr);
    {
        // Announcing during those reads means they went to an old address, so start over
        std::lock_guard<std::mutex> lock(m_pairingMutex);
        auto task = m_pairingTasks.find(ieeeAddr);
        if (m_stopPairing || task == m_pairingTasks.end() || task->second.attempt != attempt) {
            return attempt;
        }
    }
    if (!fetched) {
        reportPairingFailure(ieeeAddr, "Device did not say what kind of sensor it is. Try re-pairing.");
        return attempt;
    }
    finishPairing(ieeeAddr);
    return attempt;
}

void ZigbeeHandler::reportPairingFailure(uint64_t ieeeAddr, const std::string& message) {
    LOG_ERROR("Pairing 0x%016" PRIX64 " failed: %s", ieeeAddr, message.c_str());

    // The person adding it was told this failed, so it mustn't turn up later anyway
    bool forWizard = false;
    {
        std::lock_guard<std::mutex> lock(m_pairingMutex);
        auto task = m_pairingTasks.find(ieeeAddr);
        forWizard = task != m_pairingTasks.end() && task->second.forWizard;
    }
    if (forWizard) {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        declineLocked(ieeeAddr);
    }

    if (m_discovering && !savedDeviceId(ieeeAddr)) {  // Nobody is adding a device the hub has
        DeviceStateEvent errorEvent;
        errorEvent.deviceId = ieeeToDeviceId(ieeeAddr);
        errorEvent.property = "_pairing_error";
        errorEvent.value = message;
        m_eventBus.publish(errorEvent);
    }
}

bool ZigbeeHandler::fetchDeviceDetails(uint64_t ieeeAddr) {
    const auto info = m_coordinator->getDevice(ieeeAddr);
    if (!info) {
        return true;  // It left, which the next step handles
    }

    const auto endpointWith = [&](uint16_t cluster) -> std::optional<uint8_t> {
        for (const auto& [endpoint, clusters] : info->inClusters) {
            if (std::find(clusters.begin(), clusters.end(), cluster) != clusters.end()) {
                return endpoint;
            }
        }
        return std::nullopt;
    };

    const std::optional<uint8_t> zoneEndpoint = endpointWith(zcl::cluster::IAS_ZONE);
    if (zoneEndpoint) {
        // Some devices won't report until they're enrolled. Also for a device the hub already has,
        // as one that was reset before rejoining has forgotten.
        enrollIasZone(info->networkAddress, *zoneEndpoint);
    }

    if (savedDeviceId(ieeeAddr) || pairingStopped()) {
        return true;  // What it is hasn't changed
    }

    std::string manufacturer;
    std::string model;
    if (const auto basicEndpoint = endpointWith(zcl::cluster::BASIC)) {
        const auto attributes = m_coordinator->readAttributes(
            info->networkAddress, *basicEndpoint, zcl::cluster::BASIC,
            {zcl::attr::basic::MANUFACTURER_NAME, zcl::attr::basic::MODEL_ID}, m_pairingReadTimeoutMs);
        for (const ZclAttributeValue& attr : attributes.value_or(std::vector<ZclAttributeValue>{})) {
            if (attr.attributeId == zcl::attr::basic::MANUFACTURER_NAME) {
                manufacturer = attr.asString();
            } else if (attr.attributeId == zcl::attr::basic::MODEL_ID) {
                model = attr.asString();
            }
        }
    }

    std::optional<uint16_t> zoneType;
    std::optional<uint16_t> zoneStatus;
    if (zoneEndpoint) {
        // It decides what the device is for good, so it's worth a second try, and not worth guessing
        std::optional<std::vector<ZclAttributeValue>> attributes;
        for (int attempt = 0; attempt < 2 && !attributes && !pairingStopped(); attempt++) {
            attributes = m_coordinator->readAttributes(
                info->networkAddress, *zoneEndpoint, zcl::cluster::IAS_ZONE,
                {zcl::attr::ias_zone::ZONE_TYPE, zcl::attr::ias_zone::ZONE_STATUS},
                m_pairingReadTimeoutMs);
        }
        if (!attributes) {
            return false;
        }
        for (const ZclAttributeValue& attr : *attributes) {
            if (attr.attributeId == zcl::attr::ias_zone::ZONE_TYPE) {
                zoneType = attr.asUint16();
            } else if (attr.attributeId == zcl::attr::ias_zone::ZONE_STATUS) {
                zoneStatus = attr.asUint16();
            }
        }
        if (!zoneType) {
            LOG_WARN("0x%016" PRIX64 " has no zone type; assuming a motion sensor", ieeeAddr);
        }
    }

    m_coordinator->setDeviceDetails(ieeeAddr, manufacturer, model, zoneType, zoneStatus);
    return true;
}

ZigbeeHandler::ZoneAlarm ZigbeeHandler::zoneAlarmFor(DeviceType type, std::optional<uint16_t> zoneType) {
    if (type == DeviceType::ContactSensor) {
        return ZoneAlarm::Open;
    }
    // A leak or smoke alarm isn't motion, and mustn't set off what motion sets off
    if (type == DeviceType::MotionSensor && (!zoneType || *zoneType == zcl::zone_type::MOTION_SENSOR)) {
        return ZoneAlarm::Motion;
    }
    return ZoneAlarm::Other;
}

void ZigbeeHandler::enrollWorker() {
    constexpr uint8_t kZoneId = 1;
    std::unique_lock<std::mutex> lock(m_enrollMutex);
    for (;;) {
        m_enrollCv.wait(lock, [this] { return m_stopEnrolling || !m_enrollQueue.empty(); });
        if (m_stopEnrolling) {
            return;
        }
        const auto [nwkAddr, endpoint] = m_enrollQueue.front();
        m_enrollQueue.pop_front();
        lock.unlock();
        m_coordinator->sendZoneEnrollResponse(nwkAddr, endpoint, kZoneId);
        lock.lock();
    }
}

void ZigbeeHandler::enrollIasZone(uint16_t nwkAddr, uint8_t endpoint) {
    constexpr uint8_t kZoneId = 1;  // Only meaningful to a CIE keeping a table of zones

    // Tell it where to send alarms, which is what makes it ask to enroll
    uint64_t address = m_coordinator->ieeeAddress();
    std::vector<uint8_t> value;
    value.reserve(8);
    for (int i = 0; i < 8; i++) {
        value.push_back(static_cast<uint8_t>(address >> (8 * i)));
    }
    m_coordinator->writeAttribute(nwkAddr, endpoint, zcl::cluster::IAS_ZONE,
                                  zcl::attr::ias_zone::IAS_CIE_ADDRESS, zcl::datatype::IEEE_ADDR, value);

    // Not all ask, and some only listen for this
    m_coordinator->sendZoneEnrollResponse(nwkAddr, endpoint, kZoneId);
}

void ZigbeeHandler::notifyZoneStatus(const std::string& deviceId, uint64_t ieeeAddr, uint16_t status) {
    const bool alarm = (status & 0x03) != 0;  // Either alarm bit: devices vary in which they use
    notifyState(deviceId, "alarm", alarm);
    notifyState(deviceId, "tamper", (status & 0x04) != 0);
    notifyState(deviceId, "lowBattery", (status & 0x08) != 0);

    ZoneAlarm meaning = ZoneAlarm::Other;
    {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        auto it = m_zoneAlarms.find(ieeeAddr);
        if (it != m_zoneAlarms.end()) {
            meaning = it->second;
        }
    }
    if (meaning == ZoneAlarm::Motion) {
        notifyState(deviceId, "motion", alarm);
    } else if (meaning == ZoneAlarm::Open) {
        notifyState(deviceId, "contact", !alarm);
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

    // A device the hub already has only needs its details brought up to date
    if (const auto savedId = savedDeviceId(ieeeAddr)) {
        const DeviceType savedType = resolveDeviceType(*deviceInfo);
        const std::optional<uint8_t> savedEndpoint = primaryEndpoint(*deviceInfo, savedType);
        nlohmann::json merged;
        {
            std::lock_guard<std::mutex> lock(m_deviceMutex);
            m_ieeeToDeviceId[ieeeAddr] = *savedId;
            m_deviceIdToIeee[*savedId] = ieeeAddr;
            if (savedEndpoint) {
                m_deviceEndpoints[ieeeAddr] = *savedEndpoint;
            }
            auto& known = m_clusterEndpoints[ieeeAddr];
            const ClusterEndpoints answered = clusterEndpointsOf(*deviceInfo, savedEndpoint);
            const bool complete = std::all_of(
                deviceInfo->endpoints.begin(), deviceInfo->endpoints.end(), [&](uint8_t endpoint) {
                    return !isApplicationEndpoint(endpoint) || deviceInfo->inClusters.count(endpoint) > 0;
                });
            if (complete) {
                known = answered;
            } else {
                // One didn't answer, and still has what it had, so this can't be the whole picture
                for (const auto& [cluster, endpoints] : answered) {
                    auto& list = known[cluster];
                    for (uint8_t endpoint : endpoints) {
                        if (std::find(list.begin(), list.end(), endpoint) == list.end()) {
                            list.push_back(endpoint);
                        }
                    }
                }
            }
            merged = clusterEndpointsJson(known);
        }
        if (savedEndpoint) {
            setupReporting(ieeeAddr, deviceInfo->networkAddress, savedType);
            notifyState(*savedId, "endpoint", static_cast<int>(*savedEndpoint));
            notifyState(*savedId, "clusterEndpoints", merged);
        }
        return;
    }

    // Decided from when it joined, as the wizard may have been cancelled since
    bool forWizard = false;
    {
        std::lock_guard<std::mutex> lock(m_pairingMutex);
        auto task = m_pairingTasks.find(ieeeAddr);
        forWizard = task != m_pairingTasks.end() && task->second.forWizard;
    }
    const bool discovering = m_discovering;

    {
        std::lock_guard<std::mutex> lock(m_deviceMutex);

        // Announcing again while a wizard that has stopped discovery holds it. The wizard adds this
        // one, so another device added here would make that fail. A wizard started again, though,
        // is waiting to hear about it, which carries on below.
        if (!discovering && m_pendingDevice && m_pendingDevice->id() == ieeeToDeviceId(ieeeAddr)) {
            m_pendingDevice->setState("networkAddress", static_cast<int>(deviceInfo->networkAddress));
            m_ieeeToDeviceId[ieeeAddr] = m_pendingDevice->id();  // It may have left in between
            m_deviceIdToIeee[m_pendingDevice->id()] = ieeeAddr;
            return;
        }

        if (discovering) {
            m_declinedDevices.erase(ieeeAddr);  // Being offered again
        } else if (forWizard) {
            LOG_WARN("Not adding 0x%016" PRIX64 ": the wizard it joined during has ended. "
                     "It can be offered in the next one.", ieeeAddr);
            declineLocked(ieeeAddr);
            return;
        } else if (m_declinedDevices.count(ieeeAddr) > 0) {
            LOG_INFO("Not adding 0x%016" PRIX64 ": it was declined in the wizard", ieeeAddr);
            return;
        }
    }

    // Create SmartHub device from Zigbee device info
    auto device = createDeviceFromInfo(*deviceInfo);
    if (!device) {
        LOG_ERROR("Failed to create device for 0x%016" PRIX64, ieeeAddr);
        return;
    }

    DeviceType type = resolveDeviceType(*deviceInfo);
    std::optional<uint8_t> endpoint = primaryEndpoint(*deviceInfo, type);
    const ClusterEndpoints clusters = clusterEndpointsOf(*deviceInfo, endpoint);

    if (endpoint) {
        // Commands need these after a restart
        device->setState("endpoint", static_cast<int>(*endpoint));
        device->setState("clusterEndpoints", clusterEndpointsJson(clusters));
    }
    if (deviceInfo->zoneType) {
        device->setState("zoneType", static_cast<int>(*deviceInfo->zoneType));
    }
    // The state it's in now, as a door might not move for days
    if (deviceInfo->zoneStatus) {
        const bool alarm = (*deviceInfo->zoneStatus & 0x03) != 0;
        const ZoneAlarm meaning = zoneAlarmFor(type, deviceInfo->zoneType);
        if (meaning == ZoneAlarm::Motion) {
            device->setState("motion", alarm);
        } else if (meaning == ZoneAlarm::Open) {
            device->setState("contact", !alarm);
        }
    }

    // Store mapping
    std::string deviceId = device->id();
    {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        m_ieeeToDeviceId[ieeeAddr] = deviceId;
        m_deviceIdToIeee[deviceId] = ieeeAddr;
        m_zoneAlarms[ieeeAddr] = zoneAlarmFor(type, deviceInfo->zoneType);
        m_clusterEndpoints[ieeeAddr] = clusters;
        if (endpoint) {
            m_deviceEndpoints[ieeeAddr] = *endpoint;
        }
    }

    // Set up attribute reporting for the device type
    if (endpoint) {
        setupReporting(ieeeAddr, deviceInfo->networkAddress, type);
    }

    if (pairingStopped()) {
        return;
    }

    // If in discovery mode, store as pending and notify via pending callback
    // Don't auto-add - let the UI wizard complete first
    if (discovering) {
        LOG_INFO("Device discovered during pairing - storing as pending");
        {
            std::lock_guard<std::mutex> lock(m_deviceMutex);
            // Asked again now, as setting up reporting takes time and the wizard may have been
            // cancelled. Cancelling stops discovery before it clears, so this can't miss both.
            if (!m_discovering) {
                declineLocked(ieeeAddr);
                return;
            }
            // The wizard takes one at a time. The one it was holding can be offered in the next.
            if (m_pendingDevice && m_pendingDevice->id() != deviceId) {
                if (const auto previous = parseIeeeAddress(m_pendingDevice->id())) {
                    declineLocked(*previous);
                }
            }
            m_pendingDevice = device;
        }
        // Held during the call, so clearing the callback waits for it. Recursive, and a copy, in
        // case the callback clears itself.
        std::lock_guard<std::recursive_mutex> lock(m_pendingCallbackMutex);
        const DeviceDiscoveredCallback callback = m_pendingDeviceCb;
        if (callback) {
            callback(device);
        }
    } else {
        // Not in discovery mode - auto-add the device
        DeviceDiscoveredCallback callback;
        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            callback = m_discoveredCb;
        }
        if (callback) {
            callback(device);
        }
    }
}

void ZigbeeHandler::onDeviceLeft(uint64_t ieeeAddr) {
    LOG_INFO("Zigbee device left: ieee=0x%016" PRIX64, ieeeAddr);

    std::string deviceId;
    {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        auto it = m_ieeeToDeviceId.find(ieeeAddr);
        if (it != m_ieeeToDeviceId.end()) {
            deviceId = it->second;
            // A device the hub keeps stays mapped, so it works as soon as it's back on the
            // network, even if it doesn't answer the pairing questions then
            if (m_savedDevices.count(ieeeAddr) == 0) {
                eraseMappingLocked(ieeeAddr);
            }
        }
    }

    // Notify availability callback
    if (!deviceId.empty()) {
        notifyAvailability(deviceId, DeviceAvailability::Offline);
    }
}

void ZigbeeHandler::onAttributeReport(uint16_t nwkAddr, const ZclAttributeValue& attr) {
    LOG_DEBUG("Attribute report: nwk=0x%04X, cluster=0x%04X, attr=0x%04X",
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

    // On a device with several of something, like a two-gang switch, only the first is this device
    if (isOtherGang(deviceInfo->ieeeAddress, attr.clusterId, attr.endpoint)) {
        LOG_DEBUG("Ignoring report from endpoint %d of %s", attr.endpoint, deviceId.c_str());
        return;
    }

    // Convert attribute to state update - call callback for each property
    switch (attr.clusterId) {
        case zcl::cluster::ON_OFF:
            if (attr.attributeId == zcl::attr::onoff::ON_OFF) {
                // Some door sensors report this way rather than through an IAS zone: on means open
                bool isContactSensor = false;
                {
                    std::lock_guard<std::mutex> lock(m_deviceMutex);
                    auto meaning = m_zoneAlarms.find(deviceInfo->ieeeAddress);
                    isContactSensor = meaning != m_zoneAlarms.end() && meaning->second == ZoneAlarm::Open;
                }
                if (isContactSensor) {
                    notifyState(deviceId, "contact", !attr.asBool());
                    break;
                }
                notifyState(deviceId, "on", attr.asBool());
            }
            break;

        case zcl::cluster::LEVEL_CONTROL:
            if (attr.attributeId == zcl::attr::level::CURRENT_LEVEL) {
                notifyState(deviceId, "brightness", levelToPercent(attr.asUint8()));
            }
            break;

        case zcl::cluster::COLOR_CONTROL:
            if (attr.attributeId == zcl::attr::color::CURRENT_HUE) {
                notifyState(deviceId, "hue", attr.asUint8());
            } else if (attr.attributeId == zcl::attr::color::CURRENT_SAT) {
                notifyState(deviceId, "saturation", attr.asUint8());
            } else if (attr.attributeId == zcl::attr::color::COLOR_TEMP) {
                if (attr.asUint16() > 0) {
                    notifyState(deviceId, "colorTemp", miredsToKelvin(attr.asUint16()));
                }
            }
            break;

        case zcl::cluster::TEMPERATURE_MEASUREMENT:
            if (attr.attributeId == zcl::attr::temperature::MEASURED_VALUE) {
                // Temperature in 0.01°C units. 0x8000 means the sensor has no valid reading.
                if (attr.asInt16() != INT16_MIN) {
                    notifyState(deviceId, "temperature", attr.asInt16() / 100.0);
                }
            }
            break;

        case zcl::cluster::RELATIVE_HUMIDITY:
            if (attr.attributeId == zcl::attr::humidity::MEASURED_VALUE) {
                // Humidity in 0.01% units. 0xFFFF means the sensor has no valid reading.
                if (attr.asUint16() != 0xFFFF) {
                    notifyState(deviceId, "humidity", attr.asUint16() / 100.0);
                }
            }
            break;

        case zcl::cluster::IAS_ZONE:
            if (attr.attributeId == zcl::attr::ias_zone::ZONE_STATUS) {
                notifyZoneStatus(deviceId, deviceInfo->ieeeAddress, attr.asUint16());
            }
            break;

        case zcl::cluster::OCCUPANCY_SENSING:
            if (attr.attributeId == zcl::attr::occupancy::OCCUPANCY && !attr.data.empty()) {
                notifyState(deviceId, "motion", (attr.data[0] & 0x01) != 0);
            }
            break;

        default:
            LOG_DEBUG("Unhandled cluster 0x%04X in attribute report", attr.clusterId);
            break;
    }
}

void ZigbeeHandler::onCommandReceived(uint16_t nwkAddr, uint8_t endpoint,
                                       uint16_t cluster, uint8_t command,
                                       const std::vector<uint8_t>& payload) {
    LOG_DEBUG("Command received: nwk=0x%04X, ep=%d, cluster=0x%04X, cmd=%d",
              nwkAddr, endpoint, cluster, command);

    // Find device ID
    auto deviceInfo = m_coordinator->getDeviceByNwkAddr(nwkAddr);
    if (!deviceInfo) {
        return;
    }

    // A device asks during pairing, before it has an id
    constexpr uint8_t kZoneEnrollRequest = 0x01;
    if (cluster == zcl::cluster::IAS_ZONE && command == kZoneEnrollRequest) {
        {
            std::lock_guard<std::mutex> lock(m_enrollMutex);
            m_enrollQueue.emplace_back(nwkAddr, endpoint);
        }
        m_enrollCv.notify_all();
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

    if (isOtherGang(deviceInfo->ieeeAddress, cluster, endpoint)) {
        LOG_DEBUG("Ignoring command from endpoint %d of %s", endpoint, deviceId.c_str());
        return;
    }

    // Handle specific cluster commands
    switch (cluster) {
        case zcl::cluster::ON_OFF:
            // Handle On/Off commands from switches
            if (command == zcl::cmd::onoff::OFF) {
                notifyState(deviceId, "on", false);
            } else if (command == zcl::cmd::onoff::ON) {
                notifyState(deviceId, "on", true);
            } else if (command == zcl::cmd::onoff::TOGGLE) {
                notifyState(deviceId, "toggle", true);
            }
            break;

        case zcl::cluster::IAS_ZONE:
            // Zone status change notification
            if (command == 0x00 && payload.size() >= 2) {  // Status change notification
                const uint16_t status = static_cast<uint16_t>(payload[0] | (payload[1] << 8));
                notifyZoneStatus(deviceId, deviceInfo->ieeeAddress, status);
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
    // An IAS device says what it is
    if (info.zoneType == zcl::zone_type::CONTACT_SWITCH) {
        return DeviceType::ContactSensor;
    }
    if (info.zoneType == zcl::zone_type::MOTION_SENSOR) {
        return DeviceType::MotionSensor;
    }

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

        case DeviceType::ContactSensor:
            device = std::make_shared<ContactSensor>(deviceId, name, "zigbee");
            break;

        default:
            // Create as generic switch
            LOG_WARN("Unknown device type, creating as switch: %s", name.c_str());
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

    LOG_DEBUG("Inferring device type for %016" PRIX64 ", %zu endpoints with cluster info",
              info.ieeeAddress, info.inClusters.size());

    for (const auto& [endpoint, clusters] : info.inClusters) {
        LOG_DEBUG("  Endpoint %d has %zu clusters", endpoint, clusters.size());
        for (uint16_t cluster : clusters) {
            LOG_DEBUG("    Cluster: 0x%04X", cluster);
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
                default:
                    break;
            }
        }
    }

    LOG_DEBUG("Cluster flags: OnOff=%d, Level=%d, Color=%d, Temp=%d, Humidity=%d, IAS=%d, Occupancy=%d",
              hasOnOff, hasLevel, hasColor, hasTemperature, hasHumidity, hasIasZone, hasOccupancy);

    // Determine type based on cluster combination
    if (hasColor && hasLevel) {
        return DeviceType::ColorLight;
    } else if (hasLevel && hasOnOff) {
        return DeviceType::Dimmer;
    } else if (hasOccupancy) {
        // Before temperature: motion sensors often measure it too, but not the other way around
        return DeviceType::MotionSensor;
    } else if (hasTemperature || hasHumidity) {
        // Prioritize temperature sensor for sensors with temp/humidity clusters
        return DeviceType::TemperatureSensor;
    } else if (hasIasZone) {
        return DeviceType::MotionSensor;
    } else if (hasOnOff) {
        return DeviceType::Switch;
    }

    return DeviceType::Unknown;
}

void ZigbeeHandler::setupReporting(uint64_t ieeeAddr, uint16_t nwkAddr, DeviceType type) {
    // Configure attribute reporting based on device type
    // Min interval: 1 second, Max interval: 5 minutes

    const auto report = [&](uint16_t cluster, uint16_t attribute, uint8_t dataType, uint16_t minInterval,
                            const std::vector<uint8_t>& change) {
        if (pairingStopped()) return;  // Each request can block on an unresponsive radio
        const auto endpoint = clusterEndpoint(ieeeAddr, cluster);
        if (endpoint) {  // It might not have them all
            m_coordinator->configureReporting(nwkAddr, *endpoint, cluster, attribute, dataType,
                                              minInterval, 300, change);
        }
    };

    switch (type) {
        case DeviceType::Switch:
        case DeviceType::Dimmer:
        case DeviceType::ColorLight:
            // Report on/off state changes
            report(zcl::cluster::ON_OFF, zcl::attr::onoff::ON_OFF, zcl::datatype::BOOLEAN, 1, {});

            if (type == DeviceType::Dimmer || type == DeviceType::ColorLight) {
                // Report level changes
                report(zcl::cluster::LEVEL_CONTROL, zcl::attr::level::CURRENT_LEVEL,
                       zcl::datatype::UINT8, 1, {0x01});  // 1% change
            }

            if (type == DeviceType::ColorLight) {
                // Report color temp changes
                report(zcl::cluster::COLOR_CONTROL, zcl::attr::color::COLOR_TEMP,
                       zcl::datatype::UINT16, 1, {0x0A, 0x00});  // 10 mireds change
            }
            break;

        case DeviceType::TemperatureSensor:
            // Report temperature every 5 minutes or on 0.5°C change
            report(zcl::cluster::TEMPERATURE_MEASUREMENT, zcl::attr::temperature::MEASURED_VALUE,
                   zcl::datatype::INT16, 60, {0x32, 0x00});  // 50 = 0.5°C
            break;

        case DeviceType::MotionSensor:
            // IAS Zone status arrives as notifications. An occupancy sensor needs asking to report.
            report(zcl::cluster::OCCUPANCY_SENSING, zcl::attr::occupancy::OCCUPANCY,
                   zcl::datatype::BITMAP8, 1, {});
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

std::optional<uint64_t> ZigbeeHandler::parseIeeeAddress(std::string text) {
    if (text.rfind("zigbee_", 0) == 0) {
        text.erase(0, 7);
    } else if (text.rfind("0x", 0) == 0 || text.rfind("0X", 0) == 0) {
        text.erase(0, 2);
    }
    text.erase(std::remove(text.begin(), text.end(), ':'), text.end());

    if (text.size() != 16 ||
        !std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isxdigit(c) != 0; })) {
        return std::nullopt;
    }
    return std::stoull(text, nullptr, 16);
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
