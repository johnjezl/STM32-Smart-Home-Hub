/**
 * Zigbee Coordinator Controller
 *
 * High-level interface for managing the Zigbee network and devices.
 * Handles network formation, device pairing, and message routing.
 */
#pragma once

#include "ZnpTransport.hpp"
#include "ZclConstants.hpp"
#include <map>
#include <functional>
#include <memory>
#include <mutex>

namespace smarthub::zigbee {

/**
 * Information about a Zigbee device on the network
 */
struct ZigbeeDeviceInfo {
    uint16_t networkAddress = 0;      // 16-bit network address
    uint64_t ieeeAddress = 0;         // 64-bit IEEE address (MAC)
    uint8_t deviceType = 0;           // 0=Coordinator, 1=Router, 2=EndDevice
    std::string manufacturer;
    std::string model;
    std::string dateCode;
    std::vector<uint8_t> endpoints;
    uint64_t lastSeen = 0;            // Unix timestamp
    bool available = false;

    // Endpoint to cluster mappings
    std::map<uint8_t, std::vector<uint16_t>> inClusters;
    std::map<uint8_t, std::vector<uint16_t>> outClusters;
};

/**
 * Attribute value from a device
 */
struct ZclAttributeValue {
    uint16_t clusterId = 0;
    uint8_t endpoint = 0;
    uint16_t attributeId = 0;
    uint8_t dataType = 0;
    std::vector<uint8_t> data;

    // Convenience accessors
    bool asBool() const;
    int8_t asInt8() const;
    int16_t asInt16() const;
    int32_t asInt32() const;
    uint8_t asUint8() const;
    uint16_t asUint16() const;
    uint32_t asUint32() const;
    std::string asString() const;
};

/**
 * Callbacks for coordinator events
 */
using DeviceJoinedCallback = std::function<void(const ZigbeeDeviceInfo&)>;
using DeviceLeftCallback = std::function<void(uint64_t ieeeAddress)>;
using DeviceAnnouncedCallback = std::function<void(uint16_t nwkAddr, uint64_t ieeeAddr)>;
using AttributeReportCallback = std::function<void(uint16_t nwkAddr,
                                                    const ZclAttributeValue& attr)>;
using CommandReceivedCallback = std::function<void(uint16_t nwkAddr, uint8_t endpoint,
                                                    uint16_t cluster, uint8_t command,
                                                    const std::vector<uint8_t>& payload)>;

/**
 * Zigbee Coordinator Controller
 */
class ZigbeeCoordinator {
public:
    explicit ZigbeeCoordinator(const std::string& port, int baudRate = 115200);

    /**
     * Construct with custom transport (for testing)
     */
    explicit ZigbeeCoordinator(std::unique_ptr<ZnpTransport> transport);

    ~ZigbeeCoordinator();

    // Non-copyable
    ZigbeeCoordinator(const ZigbeeCoordinator&) = delete;
    ZigbeeCoordinator& operator=(const ZigbeeCoordinator&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    /**
     * Initialize the coordinator
     * Opens transport and verifies communication
     */
    bool initialize();

    /**
     * Shutdown the coordinator
     */
    void shutdown();

    /**
     * Check if coordinator is initialized and network is up
     */
    bool isNetworkUp() const { return m_networkUp; }

    // ========================================================================
    // Network Operations
    // ========================================================================

    /**
     * Start or form the Zigbee network
     */
    bool startNetwork();

    /**
     * Enable device pairing (permit join)
     * @param duration Seconds to allow joining (0=disable, 255=permanent)
     */
    bool permitJoin(uint8_t duration = 60);

    /**
     * Get coordinator version info
     */
    bool getVersion(uint8_t& transportRev, uint8_t& product,
                    uint8_t& majorRel, uint8_t& minorRel, uint8_t& maintRel);

    /**
     * Ping the coordinator
     */
    bool ping();

    /**
     * Reset the coordinator
     * @param hard True for hardware reset, false for soft reset
     */
    bool reset(bool hard = false);

    // ========================================================================
    // Device Operations
    // ========================================================================

    /**
     * Read an attribute from a device
     */
    std::optional<ZclAttributeValue> readAttribute(uint16_t nwkAddr, uint8_t endpoint,
                                                    uint16_t cluster, uint16_t attrId);

    /**
     * Write an attribute to a device
     */
    bool writeAttribute(uint16_t nwkAddr, uint8_t endpoint, uint16_t cluster,
                        uint16_t attrId, uint8_t dataType,
                        const std::vector<uint8_t>& value);

    /**
     * Send a cluster command to a device
     */
    bool sendCommand(uint16_t nwkAddr, uint8_t endpoint, uint16_t cluster,
                     uint8_t command, const std::vector<uint8_t>& payload = {},
                     bool disableDefaultRsp = true);

    /**
     * Configure attribute reporting on a device
     */
    bool configureReporting(uint16_t nwkAddr, uint8_t endpoint, uint16_t cluster,
                            uint16_t attrId, uint8_t dataType,
                            uint16_t minInterval, uint16_t maxInterval,
                            const std::vector<uint8_t>& reportableChange = {});

    /**
     * Request device info (IEEE address, node descriptor, endpoints)
     */
    bool requestDeviceInfo(uint16_t nwkAddr);

    /**
     * Leave request - ask a device to leave the network
     */
    bool leaveRequest(uint64_t ieeeAddr);

    // ========================================================================
    // Convenience Methods for Common Operations
    // ========================================================================

    /**
     * Turn a device on/off
     */
    bool setOnOff(uint16_t nwkAddr, uint8_t endpoint, bool on);

    /**
     * Set brightness level (0-254)
     */
    bool setLevel(uint16_t nwkAddr, uint8_t endpoint, uint8_t level,
                  uint16_t transitionTime = 10);

    /**
     * Set color temperature (in mireds)
     */
    bool setColorTemp(uint16_t nwkAddr, uint8_t endpoint, uint16_t colorTemp,
                      uint16_t transitionTime = 10);

    /**
     * Set hue and saturation
     */
    bool setHueSat(uint16_t nwkAddr, uint8_t endpoint, uint8_t hue, uint8_t sat,
                   uint16_t transitionTime = 10);

    // ========================================================================
    // Device Management
    // ========================================================================

    /**
     * Get known device by IEEE address
     */
    std::optional<ZigbeeDeviceInfo> getDevice(uint64_t ieeeAddr) const;

    /**
     * Get known device by network address
     */
    std::optional<ZigbeeDeviceInfo> getDeviceByNwkAddr(uint16_t nwkAddr) const;

    /**
     * Get all known devices
     */
    std::vector<ZigbeeDeviceInfo> getAllDevices() const;

    /**
     * Get device count
     */
    size_t deviceCount() const;

    // ========================================================================
    // Callbacks
    // ========================================================================

    void setDeviceJoinedCallback(DeviceJoinedCallback cb);
    void setDeviceLeftCallback(DeviceLeftCallback cb);
    void setDeviceAnnouncedCallback(DeviceAnnouncedCallback cb);
    void setAttributeReportCallback(AttributeReportCallback cb);
    void setCommandReceivedCallback(CommandReceivedCallback cb);

    // ========================================================================
    // Network Info
    // ========================================================================

    uint16_t panId() const { return m_panId; }
    uint64_t ieeeAddress() const { return m_ieeeAddr; }
    uint8_t channel() const { return m_channel; }

private:
    // ZNP indication handlers
    void handleIndication(const ZnpFrame& frame);
    void handleStateChange(const ZnpFrame& frame);
    void handleDeviceAnnounce(const ZnpFrame& frame);
    void handleDeviceLeave(const ZnpFrame& frame);
    void handleIncomingMessage(const ZnpFrame& frame);
    void handleTcDeviceInd(const ZnpFrame& frame);
    void handleActiveEpRsp(const ZnpFrame& frame);
    void handleSimpleDescRsp(const ZnpFrame& frame);

    // Internal helpers
    bool registerEndpoint(uint8_t endpoint, uint16_t profileId, uint16_t deviceId,
                          const std::vector<uint16_t>& inClusters,
                          const std::vector<uint16_t>& outClusters);
    bool waitForState(ZnpDeviceState targetState, int timeoutMs = 30000);
    void updateDeviceInfo(ZigbeeDeviceInfo& device);
    uint8_t nextTransactionSeq();

    // Transport
    std::unique_ptr<ZnpTransport> m_transport;
    bool m_ownsTransport = true;

    // Network state
    bool m_networkUp = false;
    uint16_t m_panId = 0;
    uint64_t m_ieeeAddr = 0;
    uint8_t m_channel = 0;
    ZnpDeviceState m_deviceState = ZnpDeviceState::HOLD;

    // Device database
    std::map<uint64_t, ZigbeeDeviceInfo> m_devices;
    std::map<uint16_t, uint64_t> m_nwkToIeee;  // Network addr -> IEEE addr
    mutable std::mutex m_deviceMutex;

    // Transaction sequence number
    uint8_t m_transSeq = 0;
    std::mutex m_seqMutex;

    // Callbacks
    DeviceJoinedCallback m_deviceJoinedCb;
    DeviceLeftCallback m_deviceLeftCb;
    DeviceAnnouncedCallback m_deviceAnnouncedCb;
    AttributeReportCallback m_attrReportCb;
    CommandReceivedCallback m_cmdReceivedCb;
    std::mutex m_callbackMutex;
};

} // namespace smarthub::zigbee
