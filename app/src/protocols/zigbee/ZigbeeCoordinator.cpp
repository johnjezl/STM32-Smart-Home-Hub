/**
 * Zigbee Coordinator Implementation
 */

#include <smarthub/protocols/zigbee/ZigbeeCoordinator.hpp>
#include <smarthub/core/Logger.hpp>
#include <chrono>
#include <cstring>

namespace smarthub::zigbee {

// ============================================================================
// ZclAttributeValue helpers
// ============================================================================

bool ZclAttributeValue::asBool() const {
    return !data.empty() && data[0] != 0;
}

int8_t ZclAttributeValue::asInt8() const {
    return data.empty() ? 0 : static_cast<int8_t>(data[0]);
}

int16_t ZclAttributeValue::asInt16() const {
    if (data.size() < 2) return 0;
    return static_cast<int16_t>(data[0] | (data[1] << 8));
}

int32_t ZclAttributeValue::asInt32() const {
    if (data.size() < 4) return 0;
    return static_cast<int32_t>(data[0] | (data[1] << 8) |
                                 (data[2] << 16) | (data[3] << 24));
}

uint8_t ZclAttributeValue::asUint8() const {
    return data.empty() ? 0 : data[0];
}

uint16_t ZclAttributeValue::asUint16() const {
    if (data.size() < 2) return 0;
    return static_cast<uint16_t>(data[0] | (data[1] << 8));
}

uint32_t ZclAttributeValue::asUint32() const {
    if (data.size() < 4) return 0;
    return static_cast<uint32_t>(data[0] | (data[1] << 8) |
                                  (data[2] << 16) | (data[3] << 24));
}

std::string ZclAttributeValue::asString() const {
    if (data.empty()) return "";
    // First byte is length for ZCL strings
    size_t len = data[0];
    if (len > data.size() - 1) len = data.size() - 1;
    return std::string(reinterpret_cast<const char*>(data.data() + 1), len);
}

// ============================================================================
// ZigbeeCoordinator Implementation
// ============================================================================

ZigbeeCoordinator::ZigbeeCoordinator(const std::string& port, int baudRate)
    : m_transport(std::make_unique<ZnpTransport>(port, baudRate))
    , m_ownsTransport(true)
{
}

ZigbeeCoordinator::ZigbeeCoordinator(std::unique_ptr<ZnpTransport> transport)
    : m_transport(std::move(transport))
    , m_ownsTransport(true)
{
}

ZigbeeCoordinator::~ZigbeeCoordinator() {
    shutdown();
}

bool ZigbeeCoordinator::initialize() {
    LOG_INFO("Initializing Zigbee coordinator...");

    // Open transport
    if (!m_transport->open()) {
        LOG_ERROR("Failed to open ZNP transport");
        return false;
    }

    // Set up indication handler
    m_transport->setIndicationCallback([this](const ZnpFrame& frame) {
        handleIndication(frame);
    });

    // Ping the coordinator
    if (!ping()) {
        LOG_ERROR("Coordinator not responding to ping");
        m_transport->close();
        return false;
    }

    // Get version info
    uint8_t transportRev, product, majorRel, minorRel, maintRel;
    if (getVersion(transportRev, product, majorRel, minorRel, maintRel)) {
        LOG_INFO("Z-Stack version: {}.{}.{} (product={}, transport={})",
                 majorRel, minorRel, maintRel, product, transportRev);
    }

    LOG_INFO("Zigbee coordinator initialized");
    return true;
}

void ZigbeeCoordinator::shutdown() {
    if (m_transport) {
        m_transport->close();
    }
    m_networkUp = false;
}

bool ZigbeeCoordinator::ping() {
    ZnpFrame req(ZnpType::SREQ, ZnpSubsystem::SYS, cmd::sys::PING);
    auto rsp = m_transport->request(req, 1000);

    return rsp.has_value();
}

bool ZigbeeCoordinator::getVersion(uint8_t& transportRev, uint8_t& product,
                                    uint8_t& majorRel, uint8_t& minorRel,
                                    uint8_t& maintRel) {
    ZnpFrame req(ZnpType::SREQ, ZnpSubsystem::SYS, cmd::sys::VERSION);
    auto rsp = m_transport->request(req);

    if (!rsp || rsp->payload().size() < 5) {
        return false;
    }

    transportRev = rsp->getByte(0);
    product = rsp->getByte(1);
    majorRel = rsp->getByte(2);
    minorRel = rsp->getByte(3);
    maintRel = rsp->getByte(4);

    return true;
}

bool ZigbeeCoordinator::reset(bool hard) {
    if (hard) {
        return m_transport->resetCoordinator();
    }

    ZnpFrame req(ZnpType::SREQ, ZnpSubsystem::SYS, cmd::sys::RESET_REQ);
    req.appendByte(0);  // Soft reset

    // Reset doesn't have an SRSP, just send it
    m_transport->send(req);

    // Wait for reset indication
    std::this_thread::sleep_for(std::chrono::seconds(2));

    return ping();
}

bool ZigbeeCoordinator::startNetwork() {
    LOG_INFO("Starting Zigbee network...");

    // Register HA endpoint
    std::vector<uint16_t> inClusters = {
        zcl::cluster::BASIC,
        zcl::cluster::ON_OFF,
        zcl::cluster::LEVEL_CONTROL,
        zcl::cluster::COLOR_CONTROL,
        zcl::cluster::TEMPERATURE_MEASUREMENT,
        zcl::cluster::RELATIVE_HUMIDITY,
        zcl::cluster::OCCUPANCY_SENSING,
        zcl::cluster::IAS_ZONE,
        zcl::cluster::ELECTRICAL_MEASUREMENT
    };

    std::vector<uint16_t> outClusters = {
        zcl::cluster::BASIC,
        zcl::cluster::ON_OFF,
        zcl::cluster::LEVEL_CONTROL,
        zcl::cluster::COLOR_CONTROL
    };

    // Profile: Home Automation (0x0104), Device: Configuration Tool
    if (!registerEndpoint(1, 0x0104, 0x0005, inClusters, outClusters)) {
        LOG_WARN("Failed to register endpoint 1");
    }

    // Also register Green Power endpoint (242)
    if (!registerEndpoint(242, 0xA1E0, 0x0061, {}, {})) {
        LOG_WARN("Failed to register Green Power endpoint");
    }

    // Start network formation
    ZnpFrame startReq(ZnpType::SREQ, ZnpSubsystem::ZDO, cmd::zdo::STARTUP_FROM_APP);
    startReq.appendWord(0);  // Start delay = 0

    auto rsp = m_transport->request(startReq);
    if (!rsp) {
        LOG_ERROR("Failed to send STARTUP_FROM_APP");
        return false;
    }

    // Response contains status (0=restored, 1=new, 2=failed)
    uint8_t status = rsp->getByte(0);
    LOG_INFO("Startup status: {}", status);

    if (status > 1) {
        LOG_ERROR("Network startup failed with status {}", status);
        return false;
    }

    // Query current device state first (in case coordinator already at target state)
    ZnpFrame stateReq(ZnpType::SREQ, ZnpSubsystem::UTIL, cmd::util::GET_DEVICE_INFO);
    auto stateRsp = m_transport->request(stateReq);
    if (stateRsp && stateRsp->payload().size() >= 13) {
        m_deviceState = static_cast<ZnpDeviceState>(stateRsp->getByte(12));
        LOG_INFO("Current device state: {}", znpDeviceStateToString(m_deviceState));
    }

    // Wait for ZB_COORD state (may already be there)
    if (!waitForState(ZnpDeviceState::ZB_COORD)) {
        LOG_ERROR("Timeout waiting for coordinator state (current: {})",
                  znpDeviceStateToString(m_deviceState));
        return false;
    }

    // Get coordinator IEEE address and network info
    ZnpFrame infoReq(ZnpType::SREQ, ZnpSubsystem::UTIL, cmd::util::GET_DEVICE_INFO);
    auto infoRsp = m_transport->request(infoReq);

    if (infoRsp && infoRsp->payload().size() >= 14) {
        // Status (1) + IEEE Addr (8) + NWK Addr (2) + Device Type (1) + Device State (1) + Assoc Count (1)
        m_ieeeAddr = infoRsp->getQWord(1);
        uint16_t nwkAddr = infoRsp->getWord(9);
        m_deviceState = static_cast<ZnpDeviceState>(infoRsp->getByte(12));

        LOG_INFO("Coordinator IEEE: {:016X}, NWK: {:04X}", m_ieeeAddr, nwkAddr);
    }

    m_networkUp = true;
    LOG_INFO("Zigbee network started successfully");

    return true;
}

bool ZigbeeCoordinator::registerEndpoint(uint8_t endpoint, uint16_t profileId,
                                          uint16_t deviceId,
                                          const std::vector<uint16_t>& inClusters,
                                          const std::vector<uint16_t>& outClusters) {
    ZnpFrame req(ZnpType::SREQ, ZnpSubsystem::AF, cmd::af::REGISTER);

    req.appendByte(endpoint);
    req.appendWord(profileId);
    req.appendWord(deviceId);
    req.appendByte(0);  // Device version
    req.appendByte(0);  // Latency requirement
    req.appendByte(static_cast<uint8_t>(inClusters.size()));

    for (uint16_t cluster : inClusters) {
        req.appendWord(cluster);
    }

    req.appendByte(static_cast<uint8_t>(outClusters.size()));

    for (uint16_t cluster : outClusters) {
        req.appendWord(cluster);
    }

    auto rsp = m_transport->request(req);
    if (!rsp) return false;

    return rsp->getByte(0) == 0;  // Status = SUCCESS
}

bool ZigbeeCoordinator::waitForState(ZnpDeviceState targetState, int timeoutMs) {
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeoutMs);

    while (std::chrono::steady_clock::now() < deadline) {
        if (m_deviceState == targetState) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false;
}

bool ZigbeeCoordinator::permitJoin(uint8_t duration) {
    LOG_INFO("Setting permit join: {} seconds", duration);

    ZnpFrame req(ZnpType::SREQ, ZnpSubsystem::ZDO, cmd::zdo::MGMT_PERMIT_JOIN_REQ);
    req.appendByte(0x0F);        // AddrMode: broadcast
    req.appendWord(0xFFFC);      // DstAddr: all routers and coordinator
    req.appendByte(duration);    // Duration
    req.appendByte(0x00);        // TC_Significance

    auto rsp = m_transport->request(req);
    if (!rsp) {
        LOG_ERROR("Permit join request failed - no response");
        return false;
    }

    uint8_t status = rsp->getByte(0);
    if (status == 0) {
        LOG_INFO("Permit join enabled successfully for {} seconds", duration);
    } else {
        LOG_ERROR("Permit join failed with status: {}", status);
    }
    return status == 0;
}

bool ZigbeeCoordinator::sendCommand(uint16_t nwkAddr, uint8_t endpoint,
                                     uint16_t cluster, uint8_t command,
                                     const std::vector<uint8_t>& payload,
                                     bool disableDefaultRsp) {
    // Build ZCL frame
    std::vector<uint8_t> zclFrame;

    // Frame control
    uint8_t frameCtrl = zcl::frame_ctrl::CLUSTER_SPECIFIC;
    if (disableDefaultRsp) {
        frameCtrl |= zcl::frame_ctrl::DISABLE_DEFAULT_RSP;
    }
    zclFrame.push_back(frameCtrl);

    // Transaction sequence
    zclFrame.push_back(nextTransactionSeq());

    // Command ID
    zclFrame.push_back(command);

    // Payload
    zclFrame.insert(zclFrame.end(), payload.begin(), payload.end());

    // Build AF_DATA_REQUEST
    ZnpFrame req(ZnpType::SREQ, ZnpSubsystem::AF, cmd::af::DATA_REQUEST);
    req.appendWord(nwkAddr);           // DstAddr
    req.appendByte(endpoint);          // DstEndpoint
    req.appendByte(1);                 // SrcEndpoint
    req.appendWord(cluster);           // ClusterID
    req.appendByte(zclFrame[1]);       // TransID (use ZCL trans seq)
    req.appendByte(0x30);              // Options: ACK + discovery route
    req.appendByte(15);                // Radius
    req.appendByte(static_cast<uint8_t>(zclFrame.size()));
    req.appendBytes(zclFrame);

    auto rsp = m_transport->request(req);
    if (!rsp) return false;

    return rsp->getByte(0) == 0;  // Status = SUCCESS
}

std::optional<ZclAttributeValue> ZigbeeCoordinator::readAttribute(
        uint16_t nwkAddr, uint8_t endpoint, uint16_t cluster, uint16_t attrId) {

    // Build ZCL Read Attributes frame
    std::vector<uint8_t> zclFrame;
    zclFrame.push_back(0x00);                    // Frame control: global, client-to-server
    zclFrame.push_back(nextTransactionSeq());    // Transaction seq
    zclFrame.push_back(zcl::global_cmd::READ_ATTRIBUTES);
    zclFrame.push_back(attrId & 0xFF);
    zclFrame.push_back((attrId >> 8) & 0xFF);

    // Send via AF
    ZnpFrame req(ZnpType::SREQ, ZnpSubsystem::AF, cmd::af::DATA_REQUEST);
    req.appendWord(nwkAddr);
    req.appendByte(endpoint);
    req.appendByte(1);
    req.appendWord(cluster);
    req.appendByte(zclFrame[1]);
    req.appendByte(0x30);
    req.appendByte(15);
    req.appendByte(static_cast<uint8_t>(zclFrame.size()));
    req.appendBytes(zclFrame);

    auto rsp = m_transport->request(req);
    if (!rsp || rsp->getByte(0) != 0) {
        return std::nullopt;
    }

    // TODO: Need to wait for AF_INCOMING_MSG with the response
    // For now, return empty - this will be improved when we add async handling
    return std::nullopt;
}

bool ZigbeeCoordinator::writeAttribute(uint16_t nwkAddr, uint8_t endpoint,
                                        uint16_t cluster, uint16_t attrId,
                                        uint8_t dataType,
                                        const std::vector<uint8_t>& value) {
    // Build ZCL Write Attributes frame
    std::vector<uint8_t> zclFrame;
    zclFrame.push_back(0x00);                    // Frame control
    zclFrame.push_back(nextTransactionSeq());
    zclFrame.push_back(zcl::global_cmd::WRITE_ATTRIBUTES);
    zclFrame.push_back(attrId & 0xFF);
    zclFrame.push_back((attrId >> 8) & 0xFF);
    zclFrame.push_back(dataType);
    zclFrame.insert(zclFrame.end(), value.begin(), value.end());

    // Send via AF
    ZnpFrame req(ZnpType::SREQ, ZnpSubsystem::AF, cmd::af::DATA_REQUEST);
    req.appendWord(nwkAddr);
    req.appendByte(endpoint);
    req.appendByte(1);
    req.appendWord(cluster);
    req.appendByte(zclFrame[1]);
    req.appendByte(0x30);
    req.appendByte(15);
    req.appendByte(static_cast<uint8_t>(zclFrame.size()));
    req.appendBytes(zclFrame);

    auto rsp = m_transport->request(req);
    return rsp && rsp->getByte(0) == 0;
}

bool ZigbeeCoordinator::configureReporting(uint16_t nwkAddr, uint8_t endpoint,
                                            uint16_t cluster, uint16_t attrId,
                                            uint8_t dataType,
                                            uint16_t minInterval, uint16_t maxInterval,
                                            const std::vector<uint8_t>& reportableChange) {
    std::vector<uint8_t> zclFrame;
    zclFrame.push_back(0x00);
    zclFrame.push_back(nextTransactionSeq());
    zclFrame.push_back(zcl::global_cmd::CONFIGURE_REPORTING);

    // Attribute reporting configuration record
    zclFrame.push_back(0x00);  // Direction: attribute is reported
    zclFrame.push_back(attrId & 0xFF);
    zclFrame.push_back((attrId >> 8) & 0xFF);
    zclFrame.push_back(dataType);
    zclFrame.push_back(minInterval & 0xFF);
    zclFrame.push_back((minInterval >> 8) & 0xFF);
    zclFrame.push_back(maxInterval & 0xFF);
    zclFrame.push_back((maxInterval >> 8) & 0xFF);

    if (!reportableChange.empty()) {
        zclFrame.insert(zclFrame.end(), reportableChange.begin(), reportableChange.end());
    }

    ZnpFrame req(ZnpType::SREQ, ZnpSubsystem::AF, cmd::af::DATA_REQUEST);
    req.appendWord(nwkAddr);
    req.appendByte(endpoint);
    req.appendByte(1);
    req.appendWord(cluster);
    req.appendByte(zclFrame[1]);
    req.appendByte(0x30);
    req.appendByte(15);
    req.appendByte(static_cast<uint8_t>(zclFrame.size()));
    req.appendBytes(zclFrame);

    auto rsp = m_transport->request(req);
    return rsp && rsp->getByte(0) == 0;
}

// ============================================================================
// Convenience methods
// ============================================================================

bool ZigbeeCoordinator::setOnOff(uint16_t nwkAddr, uint8_t endpoint, bool on) {
    return sendCommand(nwkAddr, endpoint, zcl::cluster::ON_OFF,
                       on ? zcl::cmd::onoff::ON : zcl::cmd::onoff::OFF);
}

bool ZigbeeCoordinator::setLevel(uint16_t nwkAddr, uint8_t endpoint, uint8_t level,
                                  uint16_t transitionTime) {
    std::vector<uint8_t> payload;
    payload.push_back(level);
    payload.push_back(transitionTime & 0xFF);
    payload.push_back((transitionTime >> 8) & 0xFF);

    return sendCommand(nwkAddr, endpoint, zcl::cluster::LEVEL_CONTROL,
                       zcl::cmd::level::MOVE_TO_LEVEL_ONOFF, payload);
}

bool ZigbeeCoordinator::setColorTemp(uint16_t nwkAddr, uint8_t endpoint,
                                      uint16_t colorTemp, uint16_t transitionTime) {
    std::vector<uint8_t> payload;
    payload.push_back(colorTemp & 0xFF);
    payload.push_back((colorTemp >> 8) & 0xFF);
    payload.push_back(transitionTime & 0xFF);
    payload.push_back((transitionTime >> 8) & 0xFF);

    return sendCommand(nwkAddr, endpoint, zcl::cluster::COLOR_CONTROL,
                       zcl::cmd::color::MOVE_TO_COLOR_TEMP, payload);
}

bool ZigbeeCoordinator::setHueSat(uint16_t nwkAddr, uint8_t endpoint,
                                   uint8_t hue, uint8_t sat,
                                   uint16_t transitionTime) {
    std::vector<uint8_t> payload;
    payload.push_back(hue);
    payload.push_back(sat);
    payload.push_back(transitionTime & 0xFF);
    payload.push_back((transitionTime >> 8) & 0xFF);

    return sendCommand(nwkAddr, endpoint, zcl::cluster::COLOR_CONTROL,
                       zcl::cmd::color::MOVE_TO_HUE_SAT, payload);
}

bool ZigbeeCoordinator::requestDeviceInfo(uint16_t nwkAddr) {
    // Request IEEE address
    ZnpFrame ieeeReq(ZnpType::SREQ, ZnpSubsystem::ZDO, cmd::zdo::IEEE_ADDR_REQ);
    ieeeReq.appendWord(nwkAddr);
    ieeeReq.appendByte(0);  // Request type: single device
    ieeeReq.appendByte(0);  // Start index

    m_transport->send(ieeeReq);

    // Request active endpoints
    ZnpFrame epReq(ZnpType::SREQ, ZnpSubsystem::ZDO, cmd::zdo::ACTIVE_EP_REQ);
    epReq.appendWord(nwkAddr);
    epReq.appendWord(nwkAddr);

    m_transport->send(epReq);

    return true;
}

bool ZigbeeCoordinator::leaveRequest(uint64_t ieeeAddr) {
    ZnpFrame req(ZnpType::SREQ, ZnpSubsystem::ZDO, cmd::zdo::MGMT_LEAVE_REQ);

    // Find network address
    uint16_t nwkAddr = 0xFFFF;
    {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        auto it = m_devices.find(ieeeAddr);
        if (it != m_devices.end()) {
            nwkAddr = it->second.networkAddress;
        }
    }

    req.appendWord(nwkAddr);
    req.appendQWord(ieeeAddr);
    req.appendByte(0x00);  // Remove children + rejoin flags

    auto rsp = m_transport->request(req);
    return rsp && rsp->getByte(0) == 0;
}

// ============================================================================
// Device management
// ============================================================================

std::optional<ZigbeeDeviceInfo> ZigbeeCoordinator::getDevice(uint64_t ieeeAddr) const {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    auto it = m_devices.find(ieeeAddr);
    if (it != m_devices.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<ZigbeeDeviceInfo> ZigbeeCoordinator::getDeviceByNwkAddr(uint16_t nwkAddr) const {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    auto it = m_nwkToIeee.find(nwkAddr);
    if (it != m_nwkToIeee.end()) {
        auto devIt = m_devices.find(it->second);
        if (devIt != m_devices.end()) {
            return devIt->second;
        }
    }
    return std::nullopt;
}

std::vector<ZigbeeDeviceInfo> ZigbeeCoordinator::getAllDevices() const {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    std::vector<ZigbeeDeviceInfo> result;
    result.reserve(m_devices.size());
    for (const auto& [addr, info] : m_devices) {
        result.push_back(info);
    }
    return result;
}

size_t ZigbeeCoordinator::deviceCount() const {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    return m_devices.size();
}

// ============================================================================
// Callbacks
// ============================================================================

void ZigbeeCoordinator::setDeviceJoinedCallback(DeviceJoinedCallback cb) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_deviceJoinedCb = std::move(cb);
}

void ZigbeeCoordinator::setDeviceLeftCallback(DeviceLeftCallback cb) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_deviceLeftCb = std::move(cb);
}

void ZigbeeCoordinator::setDeviceAnnouncedCallback(DeviceAnnouncedCallback cb) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_deviceAnnouncedCb = std::move(cb);
}

void ZigbeeCoordinator::setAttributeReportCallback(AttributeReportCallback cb) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_attrReportCb = std::move(cb);
}

void ZigbeeCoordinator::setCommandReceivedCallback(CommandReceivedCallback cb) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_cmdReceivedCb = std::move(cb);
}

// ============================================================================
// Indication handlers
// ============================================================================

void ZigbeeCoordinator::handleIndication(const ZnpFrame& frame) {
    LOG_INFO("Received ZNP indication: subsystem={}, cmd=0x{:02X}, len={}",
             static_cast<int>(frame.subsystem()), frame.command(), frame.payload().size());

    switch (frame.subsystem()) {
        case ZnpSubsystem::ZDO:
            switch (frame.command()) {
                case cmd::zdo::STATE_CHANGE_IND:
                    handleStateChange(frame);
                    break;
                case cmd::zdo::END_DEVICE_ANNCE_IND:
                    handleDeviceAnnounce(frame);
                    break;
                case cmd::zdo::LEAVE_IND:
                    handleDeviceLeave(frame);
                    break;
                case cmd::zdo::TC_DEV_IND:
                    handleTcDeviceInd(frame);
                    break;
                case cmd::zdo::ACTIVE_EP_RSP:
                    handleActiveEpRsp(frame);
                    break;
                case cmd::zdo::SIMPLE_DESC_RSP:
                    handleSimpleDescRsp(frame);
                    break;
                default:
                    LOG_DEBUG("Unhandled ZDO indication: 0x{:02X}", frame.command());
                    break;
            }
            break;

        case ZnpSubsystem::AF:
            if (frame.command() == cmd::af::INCOMING_MSG) {
                handleIncomingMessage(frame);
            }
            break;

        case ZnpSubsystem::SYS:
            if (frame.command() == cmd::sys::RESET_IND) {
                LOG_INFO("Coordinator reset indication received");
                m_networkUp = false;
                m_deviceState = ZnpDeviceState::HOLD;
            }
            break;

        default:
            break;
    }
}

void ZigbeeCoordinator::handleStateChange(const ZnpFrame& frame) {
    if (frame.payload().empty()) return;

    m_deviceState = static_cast<ZnpDeviceState>(frame.getByte(0));
    LOG_INFO("Device state changed: {}", znpDeviceStateToString(m_deviceState));
}

void ZigbeeCoordinator::handleDeviceAnnounce(const ZnpFrame& frame) {
    if (frame.payload().size() < 11) return;

    uint16_t srcAddr = frame.getWord(0);
    uint16_t nwkAddr = frame.getWord(2);
    uint64_t ieeeAddr = frame.getQWord(4);
    uint8_t capabilities = frame.getByte(12);

    LOG_INFO("Device announced: NWK={:04X}, IEEE={:016X}, caps=0x{:02X}",
             nwkAddr, ieeeAddr, capabilities);

    // Add/update device in database
    {
        std::lock_guard<std::mutex> lock(m_deviceMutex);

        auto& device = m_devices[ieeeAddr];
        device.ieeeAddress = ieeeAddr;
        device.networkAddress = nwkAddr;
        device.deviceType = (capabilities & 0x02) ? 1 : 2;  // FFD=Router, RFD=EndDevice
        device.lastSeen = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        device.available = true;

        m_nwkToIeee[nwkAddr] = ieeeAddr;
    }

    // Request more device info
    requestDeviceInfo(nwkAddr);

    // Notify callback
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    if (m_deviceAnnouncedCb) {
        m_deviceAnnouncedCb(nwkAddr, ieeeAddr);
    }
}

void ZigbeeCoordinator::handleDeviceLeave(const ZnpFrame& frame) {
    if (frame.payload().size() < 10) return;

    uint16_t srcAddr = frame.getWord(0);
    uint64_t ieeeAddr = frame.getQWord(2);
    uint8_t request = frame.getByte(10);
    uint8_t remove = frame.getByte(11);

    LOG_INFO("Device left: IEEE={:016X}", ieeeAddr);

    // Remove from database
    {
        std::lock_guard<std::mutex> lock(m_deviceMutex);

        auto it = m_devices.find(ieeeAddr);
        if (it != m_devices.end()) {
            m_nwkToIeee.erase(it->second.networkAddress);
            m_devices.erase(it);
        }
    }

    // Notify callback
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    if (m_deviceLeftCb) {
        m_deviceLeftCb(ieeeAddr);
    }
}

void ZigbeeCoordinator::handleTcDeviceInd(const ZnpFrame& frame) {
    // Trust Center device indication - new device joining
    if (frame.payload().size() < 10) return;

    uint16_t nwkAddr = frame.getWord(0);
    uint64_t ieeeAddr = frame.getQWord(2);
    uint16_t parentAddr = frame.getWord(10);

    LOG_INFO("New device joining: NWK={:04X}, IEEE={:016X}, parent={:04X}",
             nwkAddr, ieeeAddr, parentAddr);
}

void ZigbeeCoordinator::handleActiveEpRsp(const ZnpFrame& frame) {
    // ACTIVE_EP_RSP format:
    // SrcAddr(2) + Status(1) + NwkAddr(2) + ActiveEPCount(1) + ActiveEPList(N)
    if (frame.payload().size() < 6) return;

    uint16_t srcAddr = frame.getWord(0);
    uint8_t status = frame.getByte(2);
    uint16_t nwkAddr = frame.getWord(3);
    uint8_t epCount = frame.getByte(5);

    if (status != 0) {
        LOG_WARN("Active endpoints request failed for {:04X}: status={}", nwkAddr, status);
        return;
    }

    LOG_INFO("Active endpoints for {:04X}: {} endpoints", nwkAddr, epCount);

    // Store endpoints
    std::vector<uint8_t> endpoints;
    for (uint8_t i = 0; i < epCount && (6 + i) < frame.payload().size(); ++i) {
        uint8_t ep = frame.getByte(6 + i);
        endpoints.push_back(ep);
        LOG_DEBUG("  Endpoint: {}", ep);
    }

    // Update device with endpoints
    {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        auto it = m_nwkToIeee.find(nwkAddr);
        if (it != m_nwkToIeee.end()) {
            auto devIt = m_devices.find(it->second);
            if (devIt != m_devices.end()) {
                devIt->second.endpoints = endpoints;
            }
        }
    }

    // Request simple descriptor for each endpoint
    for (uint8_t ep : endpoints) {
        if (ep == 0) continue;  // Skip ZDO endpoint

        ZnpFrame req(ZnpType::SREQ, ZnpSubsystem::ZDO, cmd::zdo::SIMPLE_DESC_REQ);
        req.appendWord(nwkAddr);
        req.appendWord(nwkAddr);
        req.appendByte(ep);
        m_transport->send(req);
        LOG_DEBUG("Requested simple descriptor for {:04X} endpoint {}", nwkAddr, ep);
    }
}

void ZigbeeCoordinator::handleSimpleDescRsp(const ZnpFrame& frame) {
    // SIMPLE_DESC_RSP format:
    // SrcAddr(2) + Status(1) + NwkAddr(2) + Len(1) + Endpoint(1) + AppProfId(2)
    // + AppDeviceId(2) + AppDevVer(1) + NumInClusters(1) + InClusterList(N*2)
    // + NumOutClusters(1) + OutClusterList(M*2)
    if (frame.payload().size() < 14) return;

    uint16_t srcAddr = frame.getWord(0);
    uint8_t status = frame.getByte(2);
    uint16_t nwkAddr = frame.getWord(3);
    uint8_t descLen = frame.getByte(5);

    if (status != 0) {
        LOG_WARN("Simple descriptor request failed for {:04X}: status={}", nwkAddr, status);
        return;
    }

    if (descLen < 8) {
        LOG_WARN("Simple descriptor too short: {}", descLen);
        return;
    }

    uint8_t endpoint = frame.getByte(6);
    uint16_t profileId = frame.getWord(7);
    uint16_t deviceId = frame.getWord(9);
    uint8_t deviceVersion = frame.getByte(11);
    uint8_t numInClusters = frame.getByte(12);

    LOG_INFO("Simple descriptor for {:04X} ep{}: profile=0x{:04X}, device=0x{:04X}, inClusters={}",
             nwkAddr, endpoint, profileId, deviceId, numInClusters);

    // Parse input clusters
    std::vector<uint16_t> inClusters;
    size_t offset = 13;
    for (uint8_t i = 0; i < numInClusters && (offset + 1) < frame.payload().size(); ++i) {
        uint16_t cluster = frame.getWord(offset);
        inClusters.push_back(cluster);
        LOG_DEBUG("  In cluster: 0x{:04X}", cluster);
        offset += 2;
    }

    // Parse output clusters
    std::vector<uint16_t> outClusters;
    if (offset < frame.payload().size()) {
        uint8_t numOutClusters = frame.getByte(offset);
        offset++;
        for (uint8_t i = 0; i < numOutClusters && (offset + 1) < frame.payload().size(); ++i) {
            uint16_t cluster = frame.getWord(offset);
            outClusters.push_back(cluster);
            LOG_DEBUG("  Out cluster: 0x{:04X}", cluster);
            offset += 2;
        }
    }

    // Update device with cluster info
    {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        auto it = m_nwkToIeee.find(nwkAddr);
        if (it != m_nwkToIeee.end()) {
            auto devIt = m_devices.find(it->second);
            if (devIt != m_devices.end()) {
                devIt->second.inClusters[endpoint] = inClusters;
                devIt->second.outClusters[endpoint] = outClusters;
                LOG_INFO("Updated device {:016X} with clusters for endpoint {}",
                         it->second, endpoint);
            }
        }
    }
}

void ZigbeeCoordinator::handleIncomingMessage(const ZnpFrame& frame) {
    // AF_INCOMING_MSG format:
    // GroupId(2) + ClusterId(2) + SrcAddr(2) + SrcEndpoint(1) + DstEndpoint(1)
    // + WasBroadcast(1) + LinkQuality(1) + SecurityUse(1) + TimeStamp(4) + TransSeq(1) + Len(1) + Data(N)

    if (frame.payload().size() < 17) return;

    uint16_t groupId = frame.getWord(0);
    uint16_t clusterId = frame.getWord(2);
    uint16_t srcAddr = frame.getWord(4);
    uint8_t srcEndpoint = frame.getByte(6);
    uint8_t dstEndpoint = frame.getByte(7);
    uint8_t linkQuality = frame.getByte(9);
    uint8_t dataLen = frame.getByte(16);

    if (frame.payload().size() < 17 + dataLen) return;

    std::vector<uint8_t> zclData = frame.getBytes(17, dataLen);

    if (zclData.size() < 3) return;

    uint8_t frameCtrl = zclData[0];
    uint8_t transSeq = zclData[1];
    uint8_t cmdId = zclData[2];

    LOG_DEBUG("Incoming ZCL: cluster={:04X}, src={:04X}:{}, cmd=0x{:02X}",
              clusterId, srcAddr, srcEndpoint, cmdId);

    // Handle attribute reports
    if ((frameCtrl & zcl::frame_ctrl::CLUSTER_SPECIFIC) == 0 &&
        cmdId == zcl::global_cmd::REPORT_ATTRIBUTES) {

        // Parse attribute reports
        size_t offset = 3;
        while (offset + 3 <= zclData.size()) {
            uint16_t attrId = zclData[offset] | (zclData[offset + 1] << 8);
            uint8_t dataType = zclData[offset + 2];
            offset += 3;

            size_t dataSize = zcl::getDataTypeSize(dataType);
            if (dataSize == 0) {
                // Variable length - first byte is length
                if (offset < zclData.size()) {
                    dataSize = zclData[offset] + 1;
                } else {
                    break;
                }
            }

            if (offset + dataSize > zclData.size()) break;

            ZclAttributeValue attr;
            attr.clusterId = clusterId;
            attr.endpoint = srcEndpoint;
            attr.attributeId = attrId;
            attr.dataType = dataType;
            attr.data.assign(zclData.begin() + offset,
                             zclData.begin() + offset + dataSize);

            offset += dataSize;

            // Notify callback
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            if (m_attrReportCb) {
                m_attrReportCb(srcAddr, attr);
            }
        }
    }
    // Handle cluster-specific commands
    else if (frameCtrl & zcl::frame_ctrl::CLUSTER_SPECIFIC) {
        std::vector<uint8_t> cmdPayload(zclData.begin() + 3, zclData.end());

        std::lock_guard<std::mutex> lock(m_callbackMutex);
        if (m_cmdReceivedCb) {
            m_cmdReceivedCb(srcAddr, srcEndpoint, clusterId, cmdId, cmdPayload);
        }
    }
}

uint8_t ZigbeeCoordinator::nextTransactionSeq() {
    std::lock_guard<std::mutex> lock(m_seqMutex);
    return m_transSeq++;
}

} // namespace smarthub::zigbee
