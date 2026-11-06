/**
 * The messages between the Cortex-A7 application and the Cortex-M4 firmware.
 *
 * Both sides build against this one definition, so that they can't drift apart. Both cores are
 * little-endian. It's used in firmware, so nothing here allocates.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace smarthub::protocol {

enum class MsgType : uint8_t {
    /* A7 -> M4 commands */
    CMD_PING            = 0x01,
    CMD_GET_SENSOR_DATA = 0x10,
    CMD_SET_INTERVAL    = 0x11,
    CMD_GET_STATUS      = 0x20,
    CMD_SET_GPIO        = 0x30,
    CMD_GET_GPIO        = 0x31,

    /* M4 -> A7 responses */
    RSP_PONG            = 0x81,
    RSP_SENSOR_DATA     = 0x90,
    RSP_STATUS          = 0xA0,
    RSP_GPIO_STATE      = 0xB1,

    /* M4 -> A7 events (unsolicited) */
    EVT_SENSOR_UPDATE   = 0xC0,
    EVT_GPIO_CHANGE     = 0xC1,
    EVT_ERROR           = 0xE0,
};

enum class SensorType : uint8_t {
    Unknown         = 0,
    Temperature     = 1,
    Humidity        = 2,
    Pressure        = 3,
    Light           = 4,
    Motion          = 5,
    Proximity       = 6,
    CO2             = 7,
    VOC             = 8,
};

/** Starts every message */
struct MsgHeader {
    uint8_t type;       /* MsgType */
    uint8_t flags;
    uint16_t seq;       /* Sequence number */
    uint16_t len;       /* Payload length */
    uint16_t reserved;
} __attribute__((packed));

struct SensorDataPayload {
    uint8_t sensorId;
    uint8_t sensorType;     /* SensorType */
    uint16_t reserved;
    int32_t value;          /* Fixed point: value / scale */
    int32_t scale;          /* For example, 100 for two decimal places */
    uint32_t timestamp;     /* Milliseconds */
} __attribute__((packed));

struct StatusPayload {
    uint32_t uptime;        /* Milliseconds */
    uint8_t sensorCount;
    uint8_t errorCount;
    uint16_t pollInterval;  /* Milliseconds */
    uint32_t freeMemory;    /* Bytes */
} __attribute__((packed));

struct GPIOPayload {
    uint8_t port;           /* 0=A, 1=B, and so on */
    uint8_t pin;            /* 0-15 */
    uint8_t state;          /* 0=low, 1=high, 2=toggle */
    uint8_t mode;           /* For CMD_SET_GPIO */
} __attribute__((packed));

struct IntervalPayload {
    uint16_t intervalMs;
} __attribute__((packed));

/* The layout is the contract: changing one of these changes it for both sides */
static_assert(sizeof(MsgHeader) == 8, "RPMsg header layout changed");
static_assert(sizeof(SensorDataPayload) == 16, "RPMsg sensor payload layout changed");
static_assert(sizeof(StatusPayload) == 12, "RPMsg status payload layout changed");
static_assert(sizeof(GPIOPayload) == 4, "RPMsg GPIO payload layout changed");
static_assert(sizeof(IntervalPayload) == 2, "RPMsg interval payload layout changed");

/** The largest message, header included, that either side sends */
constexpr size_t MAX_MESSAGE_SIZE = 496;

/**
 * Write a message into a buffer. Returns its total size, or 0 if it doesn't fit.
 */
inline size_t encodeMessage(MsgType type, uint16_t seq, const void* payload, size_t payloadLen,
                            uint8_t* out, size_t capacity) {
    const size_t total = sizeof(MsgHeader) + payloadLen;
    if (total > capacity || total > MAX_MESSAGE_SIZE || (payload == nullptr && payloadLen != 0)) {
        return 0;
    }
    MsgHeader header{};
    header.type = static_cast<uint8_t>(type);
    header.seq = seq;
    header.len = static_cast<uint16_t>(payloadLen);
    std::memcpy(out, &header, sizeof(header));
    if (payloadLen != 0) {
        std::memcpy(out + sizeof(header), payload, payloadLen);
    }
    return total;
}

enum class DecodeResult {
    Ok,         /* A whole message is there */
    NeedMore,   /* The start of one is there */
    Invalid,    /* This can't be the start of a message */
};

struct DecodedMessage {
    MsgHeader header;
    const uint8_t* payload;     /* Inside the buffer passed in */
    size_t size;                /* Total, header included */
};

inline bool isKnownType(uint8_t type) {
    switch (static_cast<MsgType>(type)) {
        case MsgType::CMD_PING:
        case MsgType::CMD_GET_SENSOR_DATA:
        case MsgType::CMD_SET_INTERVAL:
        case MsgType::CMD_GET_STATUS:
        case MsgType::CMD_SET_GPIO:
        case MsgType::CMD_GET_GPIO:
        case MsgType::RSP_PONG:
        case MsgType::RSP_SENSOR_DATA:
        case MsgType::RSP_STATUS:
        case MsgType::RSP_GPIO_STATE:
        case MsgType::EVT_SENSOR_UPDATE:
        case MsgType::EVT_GPIO_CHANGE:
        case MsgType::EVT_ERROR:
            return true;
    }
    return false;
}

/**
 * Read the message at the start of a buffer. The header is copied, so the buffer needs no alignment.
 */
inline DecodeResult decodeMessage(const uint8_t* data, size_t len, DecodedMessage& out) {
    if (len < sizeof(MsgHeader)) {
        return DecodeResult::NeedMore;
    }
    std::memcpy(&out.header, data, sizeof(MsgHeader));
    out.size = sizeof(MsgHeader) + out.header.len;
    if (!isKnownType(out.header.type) || out.header.reserved != 0 || out.size > MAX_MESSAGE_SIZE) {
        return DecodeResult::Invalid;
    }
    if (len < out.size) {
        return DecodeResult::NeedMore;
    }
    out.payload = data + sizeof(MsgHeader);
    return DecodeResult::Ok;
}

} // namespace smarthub::protocol
