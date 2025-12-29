/**
 * Z-Stack ZNP (Zigbee Network Processor) Frame
 *
 * Implements the TI Z-Stack ZNP protocol frame format for communication
 * with CC2652P and similar Zigbee coordinators.
 *
 * Frame Format:
 * +------+--------+------+------+---------+-----+
 * | SOF  | Length | Cmd0 | Cmd1 | Payload | FCS |
 * | 0xFE | 1 byte | 1    | 1    | N bytes | 1   |
 * +------+--------+------+------+---------+-----+
 *
 * Cmd0 encodes: Type (bits 7-5) | Subsystem (bits 4-0)
 * Cmd1 is the command ID within the subsystem
 */
#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include <string>

namespace smarthub::zigbee {

/**
 * ZNP message types (encoded in bits 7-5 of Cmd0)
 */
enum class ZnpType : uint8_t {
    POLL = 0x00,  // Poll request
    SREQ = 0x20,  // Synchronous request
    AREQ = 0x40,  // Asynchronous request (indication)
    SRSP = 0x60   // Synchronous response
};

/**
 * ZNP subsystems (encoded in bits 4-0 of Cmd0)
 */
enum class ZnpSubsystem : uint8_t {
    RPC_ERROR   = 0x00,
    SYS         = 0x01,
    MAC         = 0x02,
    NWK         = 0x03,
    AF          = 0x04,
    ZDO         = 0x05,
    SAPI        = 0x06,
    UTIL        = 0x07,
    DEBUG       = 0x08,
    APP         = 0x09,
    APP_CNF     = 0x0F,
    GREENPOWER  = 0x15
};

/**
 * Common ZNP commands organized by subsystem
 */
namespace cmd {

// SYS subsystem commands
namespace sys {
    constexpr uint8_t RESET_REQ         = 0x00;
    constexpr uint8_t PING              = 0x01;
    constexpr uint8_t VERSION           = 0x02;
    constexpr uint8_t SET_EXTADDR       = 0x03;
    constexpr uint8_t GET_EXTADDR       = 0x04;
    constexpr uint8_t OSAL_NV_READ      = 0x08;
    constexpr uint8_t OSAL_NV_WRITE     = 0x09;
    constexpr uint8_t OSAL_NV_INIT      = 0x07;
    constexpr uint8_t OSAL_NV_DELETE    = 0x12;
    constexpr uint8_t OSAL_NV_LENGTH    = 0x13;
    constexpr uint8_t RESET_IND         = 0x80;  // AREQ
}

// AF (Application Framework) subsystem commands
namespace af {
    constexpr uint8_t REGISTER          = 0x00;
    constexpr uint8_t DATA_REQUEST      = 0x01;
    constexpr uint8_t DATA_REQUEST_EXT  = 0x02;
    constexpr uint8_t DATA_CONFIRM      = 0x80;  // AREQ
    constexpr uint8_t INCOMING_MSG      = 0x81;  // AREQ
    constexpr uint8_t INCOMING_MSG_EXT  = 0x82;  // AREQ
}

// ZDO (Zigbee Device Object) subsystem commands
namespace zdo {
    constexpr uint8_t NWK_ADDR_REQ      = 0x00;
    constexpr uint8_t IEEE_ADDR_REQ     = 0x01;
    constexpr uint8_t NODE_DESC_REQ     = 0x02;
    constexpr uint8_t SIMPLE_DESC_REQ   = 0x04;
    constexpr uint8_t ACTIVE_EP_REQ     = 0x05;
    constexpr uint8_t MATCH_DESC_REQ    = 0x06;
    constexpr uint8_t BIND_REQ          = 0x21;
    constexpr uint8_t UNBIND_REQ        = 0x22;
    constexpr uint8_t MGMT_LQI_REQ      = 0x31;
    constexpr uint8_t MGMT_LEAVE_REQ    = 0x34;
    constexpr uint8_t MGMT_PERMIT_JOIN_REQ = 0x36;
    constexpr uint8_t STARTUP_FROM_APP  = 0x40;

    // AREQ indications
    constexpr uint8_t NWK_ADDR_RSP      = 0x80;
    constexpr uint8_t IEEE_ADDR_RSP     = 0x81;
    constexpr uint8_t NODE_DESC_RSP     = 0x82;
    constexpr uint8_t SIMPLE_DESC_RSP   = 0x84;
    constexpr uint8_t ACTIVE_EP_RSP     = 0x85;
    constexpr uint8_t STATE_CHANGE_IND  = 0xC0;
    constexpr uint8_t END_DEVICE_ANNCE_IND = 0xC1;
    constexpr uint8_t SRC_RTG_IND       = 0xC4;
    constexpr uint8_t LEAVE_IND         = 0xC9;
    constexpr uint8_t TC_DEV_IND        = 0xCA;
    constexpr uint8_t PERMIT_JOIN_IND   = 0xCB;
}

// UTIL subsystem commands
namespace util {
    constexpr uint8_t GET_DEVICE_INFO   = 0x00;
    constexpr uint8_t GET_NV_INFO       = 0x01;
    constexpr uint8_t LED_CONTROL       = 0x0E;
    constexpr uint8_t CALLBACK_SUB_CMD  = 0x06;
}

// APP_CNF subsystem commands
namespace app_cnf {
    constexpr uint8_t SET_NWK_FRAME_COUNTER   = 0x01;
    constexpr uint8_t SET_DEFAULT_REMOTE_ENDDEV_TIMEOUT = 0x02;
    constexpr uint8_t BDB_START_COMMISSIONING = 0x05;
    constexpr uint8_t BDB_SET_CHANNEL         = 0x08;
    constexpr uint8_t BDB_SET_TC_REQUIRE_KEY_EXCHANGE = 0x09;
    constexpr uint8_t BDB_SET_JOINUSES_INSTALL_CODE_KEY = 0x06;
    constexpr uint8_t BDB_SET_ACTIVE_DEFAULT_CENTRALIZED_KEY = 0x07;
}

} // namespace cmd

/**
 * ZNP device states (reported in ZDO_STATE_CHANGE_IND)
 */
enum class ZnpDeviceState : uint8_t {
    HOLD                    = 0x00,
    INIT                    = 0x01,
    NWK_DISC                = 0x02,
    NWK_JOINING             = 0x03,
    NWK_REJOIN              = 0x04,
    END_DEVICE_UNAUTH       = 0x05,
    END_DEVICE              = 0x06,
    ROUTER                  = 0x07,
    COORD_STARTING          = 0x08,
    ZB_COORD                = 0x09,
    NWK_ORPHAN              = 0x0A
};

/**
 * ZNP Frame class for parsing and building Z-Stack protocol frames
 */
class ZnpFrame {
public:
    static constexpr uint8_t SOF = 0xFE;
    static constexpr size_t MIN_FRAME_SIZE = 5;  // SOF + Len + Cmd0 + Cmd1 + FCS
    static constexpr size_t MAX_PAYLOAD_SIZE = 250;

    ZnpFrame() = default;

    /**
     * Construct a frame with type, subsystem, and command
     */
    ZnpFrame(ZnpType type, ZnpSubsystem subsystem, uint8_t command);

    /**
     * Construct a frame with payload
     */
    ZnpFrame(ZnpType type, ZnpSubsystem subsystem, uint8_t command,
             const std::vector<uint8_t>& payload);

    // Builder pattern methods
    ZnpFrame& setPayload(const std::vector<uint8_t>& payload);
    ZnpFrame& appendByte(uint8_t b);
    ZnpFrame& appendWord(uint16_t w);      // Little-endian
    ZnpFrame& appendDWord(uint32_t d);     // Little-endian
    ZnpFrame& appendQWord(uint64_t q);     // Little-endian (for IEEE addresses)
    ZnpFrame& appendBytes(const uint8_t* data, size_t len);
    ZnpFrame& appendBytes(const std::vector<uint8_t>& data);

    /**
     * Serialize frame to bytes for transmission
     */
    std::vector<uint8_t> serialize() const;

    /**
     * Parse a frame from received bytes
     * @param data Pointer to data buffer
     * @param len Length of data buffer
     * @return Parsed frame if valid, nullopt otherwise
     */
    static std::optional<ZnpFrame> parse(const uint8_t* data, size_t len);

    /**
     * Find a complete frame in a buffer
     * @param data Pointer to data buffer
     * @param len Length of data buffer
     * @param frameStart Output: offset where frame starts
     * @param frameLen Output: total frame length
     * @return true if a complete frame was found
     */
    static bool findFrame(const uint8_t* data, size_t len,
                          size_t& frameStart, size_t& frameLen);

    // Accessors
    ZnpType type() const { return m_type; }
    ZnpSubsystem subsystem() const { return m_subsystem; }
    uint8_t command() const { return m_command; }
    const std::vector<uint8_t>& payload() const { return m_payload; }

    // Computed values
    uint8_t cmd0() const;
    uint8_t cmd1() const { return m_command; }

    // Type checks
    bool isRequest() const { return m_type == ZnpType::SREQ; }
    bool isResponse() const { return m_type == ZnpType::SRSP; }
    bool isIndication() const { return m_type == ZnpType::AREQ; }

    // Payload helpers
    uint8_t getByte(size_t offset) const;
    uint16_t getWord(size_t offset) const;   // Little-endian
    uint32_t getDWord(size_t offset) const;  // Little-endian
    uint64_t getQWord(size_t offset) const;  // Little-endian
    std::vector<uint8_t> getBytes(size_t offset, size_t len) const;

    // Debug
    std::string toString() const;

private:
    static uint8_t calculateFCS(const uint8_t* data, size_t len);

    ZnpType m_type = ZnpType::SREQ;
    ZnpSubsystem m_subsystem = ZnpSubsystem::SYS;
    uint8_t m_command = 0;
    std::vector<uint8_t> m_payload;
};

/**
 * Convert ZnpType to string for debugging
 */
const char* znpTypeToString(ZnpType type);

/**
 * Convert ZnpSubsystem to string for debugging
 */
const char* znpSubsystemToString(ZnpSubsystem subsystem);

/**
 * Convert ZnpDeviceState to string for debugging
 */
const char* znpDeviceStateToString(ZnpDeviceState state);

} // namespace smarthub::zigbee
