/**
 * M4 Firmware Message Types Tests
 *
 * Tests message structure layouts, sizes, and packing.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>

// Portable packed attribute
#ifdef _MSC_VER
#pragma pack(push, 1)
#define PACKED_STRUCT
#else
#define PACKED_STRUCT __attribute__((packed))
#endif

namespace smarthub::m4 {

/**
 * Message types for M4 <-> A7 communication
 */
enum class MsgType : uint8_t {
    CMD_PING            = 0x01,
    CMD_GET_SENSOR_DATA = 0x10,
    CMD_SET_INTERVAL    = 0x11,
    CMD_GET_STATUS      = 0x20,
    CMD_SET_GPIO        = 0x30,
    CMD_GET_GPIO        = 0x31,
    RSP_PONG            = 0x81,
    RSP_SENSOR_DATA     = 0x90,
    RSP_STATUS          = 0xA0,
    RSP_GPIO_STATE      = 0xB1,
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
};

struct MsgHeader {
    uint8_t type;
    uint8_t flags;
    uint16_t seq;
    uint16_t len;
    uint16_t reserved;
} PACKED_STRUCT;

struct SensorDataPayload {
    uint8_t sensorId;
    uint8_t sensorType;
    uint16_t reserved;
    int32_t value;
    int32_t scale;
    uint32_t timestamp;
} PACKED_STRUCT;

struct StatusPayload {
    uint32_t uptime;
    uint8_t sensorCount;
    uint8_t errorCount;
    uint16_t pollInterval;
    uint32_t freeMemory;
} PACKED_STRUCT;

struct GPIOPayload {
    uint8_t port;
    uint8_t pin;
    uint8_t state;
    uint8_t mode;
} PACKED_STRUCT;

} // namespace smarthub::m4

#ifdef _MSC_VER
#pragma pack(pop)
#endif

using namespace smarthub::m4;

class MessageTypesTest : public ::testing::Test {};

// ============================================================================
// Structure Size Tests
// ============================================================================

TEST_F(MessageTypesTest, MsgHeaderSize) {
    EXPECT_EQ(sizeof(MsgHeader), 8);
}

TEST_F(MessageTypesTest, SensorDataPayloadSize) {
    EXPECT_EQ(sizeof(SensorDataPayload), 16);
}

TEST_F(MessageTypesTest, StatusPayloadSize) {
    EXPECT_EQ(sizeof(StatusPayload), 12);
}

TEST_F(MessageTypesTest, GPIOPayloadSize) {
    EXPECT_EQ(sizeof(GPIOPayload), 4);
}

// ============================================================================
// Enum Value Tests
// ============================================================================

TEST_F(MessageTypesTest, CommandEnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(MsgType::CMD_PING), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(MsgType::CMD_GET_SENSOR_DATA), 0x10);
    EXPECT_EQ(static_cast<uint8_t>(MsgType::CMD_SET_INTERVAL), 0x11);
    EXPECT_EQ(static_cast<uint8_t>(MsgType::CMD_GET_STATUS), 0x20);
    EXPECT_EQ(static_cast<uint8_t>(MsgType::CMD_SET_GPIO), 0x30);
    EXPECT_EQ(static_cast<uint8_t>(MsgType::CMD_GET_GPIO), 0x31);
}

TEST_F(MessageTypesTest, ResponseEnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(MsgType::RSP_PONG), 0x81);
    EXPECT_EQ(static_cast<uint8_t>(MsgType::RSP_SENSOR_DATA), 0x90);
    EXPECT_EQ(static_cast<uint8_t>(MsgType::RSP_STATUS), 0xA0);
    EXPECT_EQ(static_cast<uint8_t>(MsgType::RSP_GPIO_STATE), 0xB1);
}

TEST_F(MessageTypesTest, EventEnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(MsgType::EVT_SENSOR_UPDATE), 0xC0);
    EXPECT_EQ(static_cast<uint8_t>(MsgType::EVT_GPIO_CHANGE), 0xC1);
    EXPECT_EQ(static_cast<uint8_t>(MsgType::EVT_ERROR), 0xE0);
}

TEST_F(MessageTypesTest, SensorTypeEnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(SensorType::Unknown), 0);
    EXPECT_EQ(static_cast<uint8_t>(SensorType::Temperature), 1);
    EXPECT_EQ(static_cast<uint8_t>(SensorType::Humidity), 2);
    EXPECT_EQ(static_cast<uint8_t>(SensorType::Pressure), 3);
    EXPECT_EQ(static_cast<uint8_t>(SensorType::Light), 4);
    EXPECT_EQ(static_cast<uint8_t>(SensorType::Motion), 5);
}

// ============================================================================
// Command/Response Pairing Tests
// ============================================================================

TEST_F(MessageTypesTest, PingPongPairing) {
    // Commands are 0x0X, responses are 0x8X
    uint8_t cmd = static_cast<uint8_t>(MsgType::CMD_PING);
    uint8_t rsp = static_cast<uint8_t>(MsgType::RSP_PONG);
    EXPECT_EQ(rsp, cmd | 0x80);
}

TEST_F(MessageTypesTest, SensorDataPairing) {
    uint8_t cmd = static_cast<uint8_t>(MsgType::CMD_GET_SENSOR_DATA);
    uint8_t rsp = static_cast<uint8_t>(MsgType::RSP_SENSOR_DATA);
    EXPECT_EQ(rsp, cmd | 0x80);
}

TEST_F(MessageTypesTest, StatusPairing) {
    uint8_t cmd = static_cast<uint8_t>(MsgType::CMD_GET_STATUS);
    uint8_t rsp = static_cast<uint8_t>(MsgType::RSP_STATUS);
    EXPECT_EQ(rsp, cmd | 0x80);
}

// ============================================================================
// Structure Layout Tests (verify packed correctly)
// ============================================================================

TEST_F(MessageTypesTest, MsgHeaderLayout) {
    MsgHeader hdr;
    memset(&hdr, 0, sizeof(hdr));

    hdr.type = 0x12;
    hdr.flags = 0x34;
    hdr.seq = 0x5678;
    hdr.len = 0x9ABC;
    hdr.reserved = 0xDEF0;

    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&hdr);

    EXPECT_EQ(bytes[0], 0x12);  // type
    EXPECT_EQ(bytes[1], 0x34);  // flags
    // seq is 16-bit little-endian
    EXPECT_EQ(bytes[2], 0x78);  // seq low
    EXPECT_EQ(bytes[3], 0x56);  // seq high
    // len is 16-bit little-endian
    EXPECT_EQ(bytes[4], 0xBC);  // len low
    EXPECT_EQ(bytes[5], 0x9A);  // len high
}

TEST_F(MessageTypesTest, SensorDataPayloadLayout) {
    SensorDataPayload payload;
    memset(&payload, 0, sizeof(payload));

    payload.sensorId = 1;
    payload.sensorType = 2;
    payload.value = 2345;      // 23.45 degrees * 100
    payload.scale = 100;
    payload.timestamp = 12345678;

    EXPECT_EQ(payload.sensorId, 1);
    EXPECT_EQ(payload.sensorType, 2);
    EXPECT_EQ(payload.value, 2345);
    EXPECT_EQ(payload.scale, 100);
    EXPECT_EQ(payload.timestamp, 12345678u);
}

TEST_F(MessageTypesTest, StatusPayloadLayout) {
    StatusPayload status;
    memset(&status, 0, sizeof(status));

    status.uptime = 3600000;    // 1 hour in ms
    status.sensorCount = 3;
    status.errorCount = 0;
    status.pollInterval = 1000;
    status.freeMemory = 65536;

    EXPECT_EQ(status.uptime, 3600000u);
    EXPECT_EQ(status.sensorCount, 3);
    EXPECT_EQ(status.errorCount, 0);
    EXPECT_EQ(status.pollInterval, 1000);
    EXPECT_EQ(status.freeMemory, 65536u);
}

TEST_F(MessageTypesTest, GPIOPayloadLayout) {
    GPIOPayload gpio;
    memset(&gpio, 0, sizeof(gpio));

    gpio.port = 1;   // GPIOB
    gpio.pin = 7;
    gpio.state = 1;  // HIGH
    gpio.mode = 0;

    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&gpio);

    EXPECT_EQ(bytes[0], 1);  // port
    EXPECT_EQ(bytes[1], 7);  // pin
    EXPECT_EQ(bytes[2], 1);  // state
    EXPECT_EQ(bytes[3], 0);  // mode
}

// ============================================================================
// Complete Message Tests
// ============================================================================

TEST_F(MessageTypesTest, CompletePongMessage) {
    uint8_t buffer[8];
    MsgHeader* hdr = reinterpret_cast<MsgHeader*>(buffer);

    hdr->type = static_cast<uint8_t>(MsgType::RSP_PONG);
    hdr->flags = 0;
    hdr->seq = 42;
    hdr->len = 0;
    hdr->reserved = 0;

    EXPECT_EQ(sizeof(buffer), 8);
    EXPECT_EQ(hdr->type, 0x81);
    EXPECT_EQ(hdr->seq, 42);
    EXPECT_EQ(hdr->len, 0);
}

TEST_F(MessageTypesTest, CompleteSensorDataMessage) {
    uint8_t buffer[24];  // 8 header + 16 payload
    MsgHeader* hdr = reinterpret_cast<MsgHeader*>(buffer);
    SensorDataPayload* payload = reinterpret_cast<SensorDataPayload*>(buffer + sizeof(MsgHeader));

    hdr->type = static_cast<uint8_t>(MsgType::EVT_SENSOR_UPDATE);
    hdr->flags = 0;
    hdr->seq = 100;
    hdr->len = sizeof(SensorDataPayload);
    hdr->reserved = 0;

    payload->sensorId = 0;
    payload->sensorType = static_cast<uint8_t>(SensorType::Temperature);
    payload->reserved = 0;
    payload->value = 2350;  // 23.50°C
    payload->scale = 100;
    payload->timestamp = 5000;

    EXPECT_EQ(sizeof(buffer), 24);
    EXPECT_EQ(hdr->type, 0xC0);
    EXPECT_EQ(hdr->len, 16);
    EXPECT_EQ(payload->sensorType, 1);
    EXPECT_EQ(payload->value, 2350);
}

TEST_F(MessageTypesTest, CompleteStatusMessage) {
    uint8_t buffer[20];  // 8 header + 12 payload
    MsgHeader* hdr = reinterpret_cast<MsgHeader*>(buffer);
    StatusPayload* payload = reinterpret_cast<StatusPayload*>(buffer + sizeof(MsgHeader));

    hdr->type = static_cast<uint8_t>(MsgType::RSP_STATUS);
    hdr->flags = 0;
    hdr->seq = 1;
    hdr->len = sizeof(StatusPayload);
    hdr->reserved = 0;

    payload->uptime = 1000000;
    payload->sensorCount = 2;
    payload->errorCount = 0;
    payload->pollInterval = 1000;
    payload->freeMemory = 32768;

    EXPECT_EQ(sizeof(buffer), 20);
    EXPECT_EQ(hdr->type, 0xA0);
    EXPECT_EQ(hdr->len, 12);
}

// ============================================================================
// Serialization/Deserialization Tests
// ============================================================================

TEST_F(MessageTypesTest, SerializeDeserializeSensorData) {
    // Create original
    SensorDataPayload original;
    original.sensorId = 5;
    original.sensorType = static_cast<uint8_t>(SensorType::Humidity);
    original.reserved = 0;
    original.value = 6789;  // 67.89%
    original.scale = 100;
    original.timestamp = 999999;

    // "Serialize" to byte array
    uint8_t wire[sizeof(SensorDataPayload)];
    memcpy(wire, &original, sizeof(original));

    // "Deserialize" from byte array
    SensorDataPayload received;
    memcpy(&received, wire, sizeof(received));

    // Verify
    EXPECT_EQ(received.sensorId, original.sensorId);
    EXPECT_EQ(received.sensorType, original.sensorType);
    EXPECT_EQ(received.value, original.value);
    EXPECT_EQ(received.scale, original.scale);
    EXPECT_EQ(received.timestamp, original.timestamp);
}

TEST_F(MessageTypesTest, MessageTypeIsCommand) {
    // Commands are < 0x80
    auto isCommand = [](MsgType t) {
        return static_cast<uint8_t>(t) < 0x80;
    };

    EXPECT_TRUE(isCommand(MsgType::CMD_PING));
    EXPECT_TRUE(isCommand(MsgType::CMD_GET_SENSOR_DATA));
    EXPECT_TRUE(isCommand(MsgType::CMD_SET_INTERVAL));
    EXPECT_TRUE(isCommand(MsgType::CMD_GET_STATUS));
    EXPECT_TRUE(isCommand(MsgType::CMD_SET_GPIO));
    EXPECT_TRUE(isCommand(MsgType::CMD_GET_GPIO));

    EXPECT_FALSE(isCommand(MsgType::RSP_PONG));
    EXPECT_FALSE(isCommand(MsgType::RSP_SENSOR_DATA));
    EXPECT_FALSE(isCommand(MsgType::EVT_SENSOR_UPDATE));
}

TEST_F(MessageTypesTest, MessageTypeIsResponse) {
    // Responses are 0x80-0xBF
    auto isResponse = [](MsgType t) {
        uint8_t v = static_cast<uint8_t>(t);
        return v >= 0x80 && v < 0xC0;
    };

    EXPECT_FALSE(isResponse(MsgType::CMD_PING));
    EXPECT_TRUE(isResponse(MsgType::RSP_PONG));
    EXPECT_TRUE(isResponse(MsgType::RSP_SENSOR_DATA));
    EXPECT_TRUE(isResponse(MsgType::RSP_STATUS));
    EXPECT_TRUE(isResponse(MsgType::RSP_GPIO_STATE));
    EXPECT_FALSE(isResponse(MsgType::EVT_SENSOR_UPDATE));
}

TEST_F(MessageTypesTest, MessageTypeIsEvent) {
    // Events are >= 0xC0
    auto isEvent = [](MsgType t) {
        return static_cast<uint8_t>(t) >= 0xC0;
    };

    EXPECT_FALSE(isEvent(MsgType::CMD_PING));
    EXPECT_FALSE(isEvent(MsgType::RSP_PONG));
    EXPECT_TRUE(isEvent(MsgType::EVT_SENSOR_UPDATE));
    EXPECT_TRUE(isEvent(MsgType::EVT_GPIO_CHANGE));
    EXPECT_TRUE(isEvent(MsgType::EVT_ERROR));
}
