/**
 * ZNP Frame Implementation
 */

#include <smarthub/protocols/zigbee/ZnpFrame.hpp>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace smarthub::zigbee {

ZnpFrame::ZnpFrame(ZnpType type, ZnpSubsystem subsystem, uint8_t command)
    : m_type(type)
    , m_subsystem(subsystem)
    , m_command(command)
{
}

ZnpFrame::ZnpFrame(ZnpType type, ZnpSubsystem subsystem, uint8_t command,
                   const std::vector<uint8_t>& payload)
    : m_type(type)
    , m_subsystem(subsystem)
    , m_command(command)
    , m_payload(payload)
{
}

ZnpFrame& ZnpFrame::setPayload(const std::vector<uint8_t>& payload) {
    m_payload = payload;
    return *this;
}

ZnpFrame& ZnpFrame::appendByte(uint8_t b) {
    m_payload.push_back(b);
    return *this;
}

ZnpFrame& ZnpFrame::appendWord(uint16_t w) {
    // Little-endian
    m_payload.push_back(static_cast<uint8_t>(w & 0xFF));
    m_payload.push_back(static_cast<uint8_t>((w >> 8) & 0xFF));
    return *this;
}

ZnpFrame& ZnpFrame::appendDWord(uint32_t d) {
    // Little-endian
    m_payload.push_back(static_cast<uint8_t>(d & 0xFF));
    m_payload.push_back(static_cast<uint8_t>((d >> 8) & 0xFF));
    m_payload.push_back(static_cast<uint8_t>((d >> 16) & 0xFF));
    m_payload.push_back(static_cast<uint8_t>((d >> 24) & 0xFF));
    return *this;
}

ZnpFrame& ZnpFrame::appendQWord(uint64_t q) {
    // Little-endian
    for (int i = 0; i < 8; i++) {
        m_payload.push_back(static_cast<uint8_t>((q >> (i * 8)) & 0xFF));
    }
    return *this;
}

ZnpFrame& ZnpFrame::appendBytes(const uint8_t* data, size_t len) {
    m_payload.insert(m_payload.end(), data, data + len);
    return *this;
}

ZnpFrame& ZnpFrame::appendBytes(const std::vector<uint8_t>& data) {
    m_payload.insert(m_payload.end(), data.begin(), data.end());
    return *this;
}

uint8_t ZnpFrame::cmd0() const {
    return static_cast<uint8_t>(m_type) | static_cast<uint8_t>(m_subsystem);
}

std::vector<uint8_t> ZnpFrame::serialize() const {
    std::vector<uint8_t> frame;
    frame.reserve(MIN_FRAME_SIZE + m_payload.size());

    // SOF
    frame.push_back(SOF);

    // Length (payload only)
    frame.push_back(static_cast<uint8_t>(m_payload.size()));

    // Cmd0 (type | subsystem)
    frame.push_back(cmd0());

    // Cmd1 (command)
    frame.push_back(m_command);

    // Payload
    frame.insert(frame.end(), m_payload.begin(), m_payload.end());

    // FCS (XOR of length, cmd0, cmd1, and payload)
    uint8_t fcs = calculateFCS(frame.data() + 1, frame.size() - 1);
    frame.push_back(fcs);

    return frame;
}

uint8_t ZnpFrame::calculateFCS(const uint8_t* data, size_t len) {
    uint8_t fcs = 0;
    for (size_t i = 0; i < len; i++) {
        fcs ^= data[i];
    }
    return fcs;
}

bool ZnpFrame::findFrame(const uint8_t* data, size_t len,
                          size_t& frameStart, size_t& frameLen) {
    // Search for SOF
    for (size_t i = 0; i < len; i++) {
        if (data[i] == SOF) {
            // Need at least MIN_FRAME_SIZE bytes from here
            if (i + MIN_FRAME_SIZE > len) {
                return false;  // Not enough data yet
            }

            // Get payload length
            uint8_t payloadLen = data[i + 1];

            // Check if we have the complete frame
            size_t totalLen = MIN_FRAME_SIZE + payloadLen;
            if (i + totalLen > len) {
                return false;  // Not enough data yet
            }

            // Verify FCS
            uint8_t expectedFcs = calculateFCS(data + i + 1, payloadLen + 3);
            uint8_t actualFcs = data[i + totalLen - 1];

            if (expectedFcs == actualFcs) {
                frameStart = i;
                frameLen = totalLen;
                return true;
            }
            // FCS mismatch - skip this SOF and keep looking
        }
    }

    return false;
}

std::optional<ZnpFrame> ZnpFrame::parse(const uint8_t* data, size_t len) {
    size_t frameStart, frameLen;

    if (!findFrame(data, len, frameStart, frameLen)) {
        return std::nullopt;
    }

    const uint8_t* frame = data + frameStart;

    // Extract fields
    uint8_t payloadLen = frame[1];
    uint8_t cmd0 = frame[2];
    uint8_t cmd1 = frame[3];

    // Decode type and subsystem from cmd0
    ZnpType type = static_cast<ZnpType>(cmd0 & 0xE0);
    ZnpSubsystem subsystem = static_cast<ZnpSubsystem>(cmd0 & 0x1F);

    // Extract payload
    std::vector<uint8_t> payload;
    if (payloadLen > 0) {
        payload.assign(frame + 4, frame + 4 + payloadLen);
    }

    return ZnpFrame(type, subsystem, cmd1, payload);
}

uint8_t ZnpFrame::getByte(size_t offset) const {
    if (offset >= m_payload.size()) {
        return 0;
    }
    return m_payload[offset];
}

uint16_t ZnpFrame::getWord(size_t offset) const {
    if (offset + 1 >= m_payload.size()) {
        return 0;
    }
    return static_cast<uint16_t>(m_payload[offset]) |
           (static_cast<uint16_t>(m_payload[offset + 1]) << 8);
}

uint32_t ZnpFrame::getDWord(size_t offset) const {
    if (offset + 3 >= m_payload.size()) {
        return 0;
    }
    return static_cast<uint32_t>(m_payload[offset]) |
           (static_cast<uint32_t>(m_payload[offset + 1]) << 8) |
           (static_cast<uint32_t>(m_payload[offset + 2]) << 16) |
           (static_cast<uint32_t>(m_payload[offset + 3]) << 24);
}

uint64_t ZnpFrame::getQWord(size_t offset) const {
    if (offset + 7 >= m_payload.size()) {
        return 0;
    }
    uint64_t result = 0;
    for (int i = 0; i < 8; i++) {
        result |= static_cast<uint64_t>(m_payload[offset + i]) << (i * 8);
    }
    return result;
}

std::vector<uint8_t> ZnpFrame::getBytes(size_t offset, size_t len) const {
    if (offset >= m_payload.size()) {
        return {};
    }
    size_t actualLen = std::min(len, m_payload.size() - offset);
    return std::vector<uint8_t>(m_payload.begin() + offset,
                                 m_payload.begin() + offset + actualLen);
}

std::string ZnpFrame::toString() const {
    std::ostringstream ss;
    ss << "ZnpFrame{type=" << znpTypeToString(m_type)
       << ", subsystem=" << znpSubsystemToString(m_subsystem)
       << ", cmd=0x" << std::hex << std::setfill('0') << std::setw(2)
       << static_cast<int>(m_command)
       << ", payload=[";

    for (size_t i = 0; i < m_payload.size(); i++) {
        if (i > 0) ss << " ";
        ss << std::hex << std::setfill('0') << std::setw(2)
           << static_cast<int>(m_payload[i]);
    }
    ss << "]}";

    return ss.str();
}

const char* znpTypeToString(ZnpType type) {
    switch (type) {
        case ZnpType::POLL: return "POLL";
        case ZnpType::SREQ: return "SREQ";
        case ZnpType::AREQ: return "AREQ";
        case ZnpType::SRSP: return "SRSP";
        default: return "UNKNOWN";
    }
}

const char* znpSubsystemToString(ZnpSubsystem subsystem) {
    switch (subsystem) {
        case ZnpSubsystem::RPC_ERROR: return "RPC_ERROR";
        case ZnpSubsystem::SYS: return "SYS";
        case ZnpSubsystem::MAC: return "MAC";
        case ZnpSubsystem::NWK: return "NWK";
        case ZnpSubsystem::AF: return "AF";
        case ZnpSubsystem::ZDO: return "ZDO";
        case ZnpSubsystem::SAPI: return "SAPI";
        case ZnpSubsystem::UTIL: return "UTIL";
        case ZnpSubsystem::DEBUG: return "DEBUG";
        case ZnpSubsystem::APP: return "APP";
        case ZnpSubsystem::APP_CNF: return "APP_CNF";
        case ZnpSubsystem::GREENPOWER: return "GREENPOWER";
        default: return "UNKNOWN";
    }
}

const char* znpDeviceStateToString(ZnpDeviceState state) {
    switch (state) {
        case ZnpDeviceState::HOLD: return "HOLD";
        case ZnpDeviceState::INIT: return "INIT";
        case ZnpDeviceState::NWK_DISC: return "NWK_DISC";
        case ZnpDeviceState::NWK_JOINING: return "NWK_JOINING";
        case ZnpDeviceState::NWK_REJOIN: return "NWK_REJOIN";
        case ZnpDeviceState::END_DEVICE_UNAUTH: return "END_DEVICE_UNAUTH";
        case ZnpDeviceState::END_DEVICE: return "END_DEVICE";
        case ZnpDeviceState::ROUTER: return "ROUTER";
        case ZnpDeviceState::COORD_STARTING: return "COORD_STARTING";
        case ZnpDeviceState::ZB_COORD: return "ZB_COORD";
        case ZnpDeviceState::NWK_ORPHAN: return "NWK_ORPHAN";
        default: return "UNKNOWN";
    }
}

} // namespace smarthub::zigbee
