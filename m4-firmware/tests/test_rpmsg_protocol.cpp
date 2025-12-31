/**
 * RPMsg Protocol Tests
 *
 * Tests message building, parsing, and protocol handling.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <vector>
#include <queue>

#ifdef _MSC_VER
#pragma pack(push, 1)
#define PACKED_STRUCT
#else
#define PACKED_STRUCT __attribute__((packed))
#endif

namespace {

// Message types (same as in rpmsg.hpp)
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

struct IntervalPayload {
    uint16_t intervalMs;
    uint16_t reserved;
} PACKED_STRUCT;

/**
 * Message builder for creating RPMsg messages
 */
class MessageBuilder {
public:
    MessageBuilder() : m_seq(0) {}

    std::vector<uint8_t> buildPong() {
        return buildMessage(MsgType::RSP_PONG, nullptr, 0);
    }

    std::vector<uint8_t> buildSensorData(uint8_t sensorId, SensorType type,
                                          int32_t value, int32_t scale,
                                          uint32_t timestamp) {
        SensorDataPayload payload;
        payload.sensorId = sensorId;
        payload.sensorType = static_cast<uint8_t>(type);
        payload.reserved = 0;
        payload.value = value;
        payload.scale = scale;
        payload.timestamp = timestamp;

        return buildMessage(MsgType::EVT_SENSOR_UPDATE, &payload, sizeof(payload));
    }

    std::vector<uint8_t> buildStatus(uint32_t uptime, uint8_t sensorCount,
                                      uint8_t errorCount, uint16_t pollInterval,
                                      uint32_t freeMemory) {
        StatusPayload payload;
        payload.uptime = uptime;
        payload.sensorCount = sensorCount;
        payload.errorCount = errorCount;
        payload.pollInterval = pollInterval;
        payload.freeMemory = freeMemory;

        return buildMessage(MsgType::RSP_STATUS, &payload, sizeof(payload));
    }

    std::vector<uint8_t> buildGpioState(uint8_t port, uint8_t pin, uint8_t state) {
        GPIOPayload payload;
        payload.port = port;
        payload.pin = pin;
        payload.state = state;
        payload.mode = 0;

        return buildMessage(MsgType::RSP_GPIO_STATE, &payload, sizeof(payload));
    }

    uint16_t getSeq() const { return m_seq; }

private:
    std::vector<uint8_t> buildMessage(MsgType type, const void* payload, size_t payloadLen) {
        std::vector<uint8_t> msg(sizeof(MsgHeader) + payloadLen);

        MsgHeader* hdr = reinterpret_cast<MsgHeader*>(msg.data());
        hdr->type = static_cast<uint8_t>(type);
        hdr->flags = 0;
        hdr->seq = m_seq++;
        hdr->len = static_cast<uint16_t>(payloadLen);
        hdr->reserved = 0;

        if (payload && payloadLen > 0) {
            memcpy(msg.data() + sizeof(MsgHeader), payload, payloadLen);
        }

        return msg;
    }

    uint16_t m_seq;
};

/**
 * Message parser for decoding RPMsg messages
 */
class MessageParser {
public:
    struct ParsedMessage {
        MsgType type;
        uint8_t flags;
        uint16_t seq;
        std::vector<uint8_t> payload;
        bool valid;
    };

    ParsedMessage parse(const uint8_t* data, size_t len) {
        ParsedMessage result;
        result.valid = false;

        if (len < sizeof(MsgHeader)) {
            return result;
        }

        const MsgHeader* hdr = reinterpret_cast<const MsgHeader*>(data);

        if (len < sizeof(MsgHeader) + hdr->len) {
            return result;
        }

        result.type = static_cast<MsgType>(hdr->type);
        result.flags = hdr->flags;
        result.seq = hdr->seq;
        result.payload.assign(data + sizeof(MsgHeader),
                              data + sizeof(MsgHeader) + hdr->len);
        result.valid = true;

        return result;
    }

    bool parseSensorData(const std::vector<uint8_t>& payload, SensorDataPayload& out) {
        if (payload.size() < sizeof(SensorDataPayload)) {
            return false;
        }
        memcpy(&out, payload.data(), sizeof(SensorDataPayload));
        return true;
    }

    bool parseStatus(const std::vector<uint8_t>& payload, StatusPayload& out) {
        if (payload.size() < sizeof(StatusPayload)) {
            return false;
        }
        memcpy(&out, payload.data(), sizeof(StatusPayload));
        return true;
    }

    bool parseGpio(const std::vector<uint8_t>& payload, GPIOPayload& out) {
        if (payload.size() < sizeof(GPIOPayload)) {
            return false;
        }
        memcpy(&out, payload.data(), sizeof(GPIOPayload));
        return true;
    }
};

} // anonymous namespace

#ifdef _MSC_VER
#pragma pack(pop)
#endif

class RpmsgProtocolTest : public ::testing::Test {
protected:
    MessageBuilder builder;
    MessageParser parser;
};

// ============================================================================
// Message Building Tests
// ============================================================================

TEST_F(RpmsgProtocolTest, BuildPong) {
    auto msg = builder.buildPong();

    EXPECT_EQ(msg.size(), sizeof(MsgHeader));

    const MsgHeader* hdr = reinterpret_cast<const MsgHeader*>(msg.data());
    EXPECT_EQ(hdr->type, static_cast<uint8_t>(MsgType::RSP_PONG));
    EXPECT_EQ(hdr->flags, 0);
    EXPECT_EQ(hdr->seq, 0);
    EXPECT_EQ(hdr->len, 0);
}

TEST_F(RpmsgProtocolTest, BuildSensorData) {
    auto msg = builder.buildSensorData(0, SensorType::Temperature, 2350, 100, 5000);

    EXPECT_EQ(msg.size(), sizeof(MsgHeader) + sizeof(SensorDataPayload));

    const MsgHeader* hdr = reinterpret_cast<const MsgHeader*>(msg.data());
    EXPECT_EQ(hdr->type, static_cast<uint8_t>(MsgType::EVT_SENSOR_UPDATE));
    EXPECT_EQ(hdr->len, sizeof(SensorDataPayload));

    const SensorDataPayload* payload = reinterpret_cast<const SensorDataPayload*>(
        msg.data() + sizeof(MsgHeader));
    EXPECT_EQ(payload->sensorId, 0);
    EXPECT_EQ(payload->sensorType, 1);
    EXPECT_EQ(payload->value, 2350);
    EXPECT_EQ(payload->scale, 100);
    EXPECT_EQ(payload->timestamp, 5000u);
}

TEST_F(RpmsgProtocolTest, BuildStatus) {
    auto msg = builder.buildStatus(3600000, 2, 0, 1000, 65536);

    EXPECT_EQ(msg.size(), sizeof(MsgHeader) + sizeof(StatusPayload));

    const MsgHeader* hdr = reinterpret_cast<const MsgHeader*>(msg.data());
    EXPECT_EQ(hdr->type, static_cast<uint8_t>(MsgType::RSP_STATUS));
    EXPECT_EQ(hdr->len, sizeof(StatusPayload));

    const StatusPayload* payload = reinterpret_cast<const StatusPayload*>(
        msg.data() + sizeof(MsgHeader));
    EXPECT_EQ(payload->uptime, 3600000u);
    EXPECT_EQ(payload->sensorCount, 2);
    EXPECT_EQ(payload->errorCount, 0);
    EXPECT_EQ(payload->pollInterval, 1000);
    EXPECT_EQ(payload->freeMemory, 65536u);
}

TEST_F(RpmsgProtocolTest, BuildGpioState) {
    auto msg = builder.buildGpioState(1, 7, 1);  // GPIOB, pin 7, HIGH

    EXPECT_EQ(msg.size(), sizeof(MsgHeader) + sizeof(GPIOPayload));

    const MsgHeader* hdr = reinterpret_cast<const MsgHeader*>(msg.data());
    EXPECT_EQ(hdr->type, static_cast<uint8_t>(MsgType::RSP_GPIO_STATE));

    const GPIOPayload* payload = reinterpret_cast<const GPIOPayload*>(
        msg.data() + sizeof(MsgHeader));
    EXPECT_EQ(payload->port, 1);
    EXPECT_EQ(payload->pin, 7);
    EXPECT_EQ(payload->state, 1);
}

TEST_F(RpmsgProtocolTest, SequenceNumberIncrementing) {
    builder.buildPong();
    EXPECT_EQ(builder.getSeq(), 1);

    builder.buildPong();
    EXPECT_EQ(builder.getSeq(), 2);

    builder.buildSensorData(0, SensorType::Temperature, 0, 100, 0);
    EXPECT_EQ(builder.getSeq(), 3);
}

// ============================================================================
// Message Parsing Tests
// ============================================================================

TEST_F(RpmsgProtocolTest, ParsePong) {
    auto msg = builder.buildPong();
    auto parsed = parser.parse(msg.data(), msg.size());

    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.type, MsgType::RSP_PONG);
    EXPECT_EQ(parsed.seq, 0);
    EXPECT_TRUE(parsed.payload.empty());
}

TEST_F(RpmsgProtocolTest, ParseSensorData) {
    auto msg = builder.buildSensorData(1, SensorType::Humidity, 6789, 100, 10000);
    auto parsed = parser.parse(msg.data(), msg.size());

    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.type, MsgType::EVT_SENSOR_UPDATE);
    EXPECT_EQ(parsed.payload.size(), sizeof(SensorDataPayload));

    SensorDataPayload data;
    EXPECT_TRUE(parser.parseSensorData(parsed.payload, data));
    EXPECT_EQ(data.sensorId, 1);
    EXPECT_EQ(data.sensorType, static_cast<uint8_t>(SensorType::Humidity));
    EXPECT_EQ(data.value, 6789);
    EXPECT_EQ(data.scale, 100);
    EXPECT_EQ(data.timestamp, 10000u);
}

TEST_F(RpmsgProtocolTest, ParseStatus) {
    auto msg = builder.buildStatus(1000000, 3, 1, 500, 32768);
    auto parsed = parser.parse(msg.data(), msg.size());

    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.type, MsgType::RSP_STATUS);

    StatusPayload status;
    EXPECT_TRUE(parser.parseStatus(parsed.payload, status));
    EXPECT_EQ(status.uptime, 1000000u);
    EXPECT_EQ(status.sensorCount, 3);
    EXPECT_EQ(status.errorCount, 1);
    EXPECT_EQ(status.pollInterval, 500);
    EXPECT_EQ(status.freeMemory, 32768u);
}

TEST_F(RpmsgProtocolTest, ParseGpio) {
    auto msg = builder.buildGpioState(2, 13, 0);  // GPIOC, pin 13, LOW
    auto parsed = parser.parse(msg.data(), msg.size());

    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.type, MsgType::RSP_GPIO_STATE);

    GPIOPayload gpio;
    EXPECT_TRUE(parser.parseGpio(parsed.payload, gpio));
    EXPECT_EQ(gpio.port, 2);
    EXPECT_EQ(gpio.pin, 13);
    EXPECT_EQ(gpio.state, 0);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(RpmsgProtocolTest, ParseTruncatedHeader) {
    uint8_t data[] = {0x81, 0x00, 0x00};  // Only 3 bytes, need 8
    auto parsed = parser.parse(data, 3);
    EXPECT_FALSE(parsed.valid);
}

TEST_F(RpmsgProtocolTest, ParseTruncatedPayload) {
    // Create a header that claims 16 bytes of payload but only provide 8
    uint8_t data[16];
    MsgHeader* hdr = reinterpret_cast<MsgHeader*>(data);
    hdr->type = static_cast<uint8_t>(MsgType::EVT_SENSOR_UPDATE);
    hdr->flags = 0;
    hdr->seq = 0;
    hdr->len = 16;  // Claims 16 bytes
    hdr->reserved = 0;
    // But total buffer is only 16 bytes (8 header + 8 payload)

    auto parsed = parser.parse(data, 16);
    EXPECT_FALSE(parsed.valid);  // Should fail: claims 16 byte payload but only 8 available
}

TEST_F(RpmsgProtocolTest, ParseEmptyData) {
    auto parsed = parser.parse(nullptr, 0);
    EXPECT_FALSE(parsed.valid);
}

TEST_F(RpmsgProtocolTest, ParsePayloadTooSmall) {
    std::vector<uint8_t> smallPayload = {0x01, 0x02};

    SensorDataPayload sensor;
    EXPECT_FALSE(parser.parseSensorData(smallPayload, sensor));

    StatusPayload status;
    EXPECT_FALSE(parser.parseStatus(smallPayload, status));

    GPIOPayload gpio;
    EXPECT_FALSE(parser.parseGpio(smallPayload, gpio));
}

// ============================================================================
// Command Parsing Tests
// ============================================================================

TEST_F(RpmsgProtocolTest, ParsePingCommand) {
    // Build a PING command (A7 -> M4)
    MsgHeader hdr;
    hdr.type = static_cast<uint8_t>(MsgType::CMD_PING);
    hdr.flags = 0;
    hdr.seq = 42;
    hdr.len = 0;
    hdr.reserved = 0;

    auto parsed = parser.parse(reinterpret_cast<uint8_t*>(&hdr), sizeof(hdr));

    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.type, MsgType::CMD_PING);
    EXPECT_EQ(parsed.seq, 42);
}

TEST_F(RpmsgProtocolTest, ParseSetIntervalCommand) {
    std::vector<uint8_t> msg(sizeof(MsgHeader) + sizeof(IntervalPayload));

    MsgHeader* hdr = reinterpret_cast<MsgHeader*>(msg.data());
    hdr->type = static_cast<uint8_t>(MsgType::CMD_SET_INTERVAL);
    hdr->flags = 0;
    hdr->seq = 1;
    hdr->len = sizeof(IntervalPayload);
    hdr->reserved = 0;

    IntervalPayload* payload = reinterpret_cast<IntervalPayload*>(
        msg.data() + sizeof(MsgHeader));
    payload->intervalMs = 2000;
    payload->reserved = 0;

    auto parsed = parser.parse(msg.data(), msg.size());

    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.type, MsgType::CMD_SET_INTERVAL);
    EXPECT_EQ(parsed.payload.size(), sizeof(IntervalPayload));

    const IntervalPayload* parsedPayload = reinterpret_cast<const IntervalPayload*>(
        parsed.payload.data());
    EXPECT_EQ(parsedPayload->intervalMs, 2000);
}

TEST_F(RpmsgProtocolTest, ParseSetGpioCommand) {
    std::vector<uint8_t> msg(sizeof(MsgHeader) + sizeof(GPIOPayload));

    MsgHeader* hdr = reinterpret_cast<MsgHeader*>(msg.data());
    hdr->type = static_cast<uint8_t>(MsgType::CMD_SET_GPIO);
    hdr->flags = 0;
    hdr->seq = 5;
    hdr->len = sizeof(GPIOPayload);
    hdr->reserved = 0;

    GPIOPayload* payload = reinterpret_cast<GPIOPayload*>(
        msg.data() + sizeof(MsgHeader));
    payload->port = 0;   // GPIOA
    payload->pin = 5;
    payload->state = 2;  // Toggle
    payload->mode = 0;

    auto parsed = parser.parse(msg.data(), msg.size());

    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.type, MsgType::CMD_SET_GPIO);

    GPIOPayload gpio;
    EXPECT_TRUE(parser.parseGpio(parsed.payload, gpio));
    EXPECT_EQ(gpio.port, 0);
    EXPECT_EQ(gpio.pin, 5);
    EXPECT_EQ(gpio.state, 2);
}

// ============================================================================
// Roundtrip Tests
// ============================================================================

TEST_F(RpmsgProtocolTest, RoundtripSensorData) {
    // Build
    auto msg = builder.buildSensorData(2, SensorType::Pressure, 101325, 1, 999999);

    // Parse
    auto parsed = parser.parse(msg.data(), msg.size());
    EXPECT_TRUE(parsed.valid);

    // Extract
    SensorDataPayload data;
    EXPECT_TRUE(parser.parseSensorData(parsed.payload, data));

    // Verify
    EXPECT_EQ(data.sensorId, 2);
    EXPECT_EQ(data.sensorType, static_cast<uint8_t>(SensorType::Pressure));
    EXPECT_EQ(data.value, 101325);  // ~1 atm in Pa
    EXPECT_EQ(data.scale, 1);
    EXPECT_EQ(data.timestamp, 999999u);
}

TEST_F(RpmsgProtocolTest, RoundtripMultipleMessages) {
    // Build multiple messages
    auto msg1 = builder.buildPong();
    auto msg2 = builder.buildSensorData(0, SensorType::Temperature, 2500, 100, 1000);
    auto msg3 = builder.buildSensorData(1, SensorType::Humidity, 5000, 100, 1000);
    auto msg4 = builder.buildStatus(60000, 2, 0, 1000, 40000);

    // Parse and verify each
    auto parsed1 = parser.parse(msg1.data(), msg1.size());
    EXPECT_TRUE(parsed1.valid);
    EXPECT_EQ(parsed1.type, MsgType::RSP_PONG);
    EXPECT_EQ(parsed1.seq, 0);

    auto parsed2 = parser.parse(msg2.data(), msg2.size());
    EXPECT_TRUE(parsed2.valid);
    EXPECT_EQ(parsed2.type, MsgType::EVT_SENSOR_UPDATE);
    EXPECT_EQ(parsed2.seq, 1);

    auto parsed3 = parser.parse(msg3.data(), msg3.size());
    EXPECT_TRUE(parsed3.valid);
    EXPECT_EQ(parsed3.seq, 2);

    auto parsed4 = parser.parse(msg4.data(), msg4.size());
    EXPECT_TRUE(parsed4.valid);
    EXPECT_EQ(parsed4.type, MsgType::RSP_STATUS);
    EXPECT_EQ(parsed4.seq, 3);
}

// ============================================================================
// Wire Format Tests
// ============================================================================

TEST_F(RpmsgProtocolTest, WireFormatPong) {
    auto msg = builder.buildPong();

    // Verify exact byte sequence
    EXPECT_EQ(msg[0], 0x81);  // RSP_PONG type
    EXPECT_EQ(msg[1], 0x00);  // flags
    EXPECT_EQ(msg[2], 0x00);  // seq low
    EXPECT_EQ(msg[3], 0x00);  // seq high
    EXPECT_EQ(msg[4], 0x00);  // len low
    EXPECT_EQ(msg[5], 0x00);  // len high
    EXPECT_EQ(msg[6], 0x00);  // reserved low
    EXPECT_EQ(msg[7], 0x00);  // reserved high
}

TEST_F(RpmsgProtocolTest, WireFormatSensorData) {
    // Second message, so seq = 1
    builder.buildPong();  // seq = 0
    auto msg = builder.buildSensorData(0, SensorType::Temperature, 2345, 100, 5000);

    // Header
    EXPECT_EQ(msg[0], 0xC0);  // EVT_SENSOR_UPDATE
    EXPECT_EQ(msg[1], 0x00);  // flags
    EXPECT_EQ(msg[2], 0x01);  // seq low (1)
    EXPECT_EQ(msg[3], 0x00);  // seq high
    EXPECT_EQ(msg[4], 0x10);  // len low (16)
    EXPECT_EQ(msg[5], 0x00);  // len high

    // Payload starts at offset 8
    EXPECT_EQ(msg[8], 0x00);   // sensorId
    EXPECT_EQ(msg[9], 0x01);   // sensorType (Temperature)

    // value = 2345 = 0x0929 (little-endian: 29 09 00 00)
    EXPECT_EQ(msg[12], 0x29);
    EXPECT_EQ(msg[13], 0x09);
    EXPECT_EQ(msg[14], 0x00);
    EXPECT_EQ(msg[15], 0x00);

    // scale = 100 = 0x64
    EXPECT_EQ(msg[16], 0x64);
    EXPECT_EQ(msg[17], 0x00);
    EXPECT_EQ(msg[18], 0x00);
    EXPECT_EQ(msg[19], 0x00);

    // timestamp = 5000 = 0x1388
    EXPECT_EQ(msg[20], 0x88);
    EXPECT_EQ(msg[21], 0x13);
    EXPECT_EQ(msg[22], 0x00);
    EXPECT_EQ(msg[23], 0x00);
}

// ============================================================================
// VirtIO Vring Structure Tests
// ============================================================================

namespace {

// VirtIO vring structures (matching Linux kernel and M4 firmware)
struct vring_desc {
    uint64_t addr;      // Buffer physical address
    uint32_t len;       // Buffer length
    uint16_t flags;     // Descriptor flags
    uint16_t next;      // Next descriptor index
} PACKED_STRUCT;

struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[8];   // For VRING_NUM = 8
} PACKED_STRUCT;

struct vring_used_elem {
    uint32_t id;        // Index of start of used descriptor chain
    uint32_t len;       // Total length of descriptor chain written
} PACKED_STRUCT;

struct vring_used {
    uint16_t flags;
    uint16_t idx;
    vring_used_elem ring[8];
} PACKED_STRUCT;

// RPMsg name service announcement (56 bytes)
struct rpmsg_ns_msg {
    char name[32];
    uint32_t addr;
    uint32_t flags;
} PACKED_STRUCT;

// RPMsg header (16 bytes)
struct rpmsg_hdr {
    uint32_t src;
    uint32_t dst;
    uint32_t reserved;
    uint16_t len;
    uint16_t flags;
} PACKED_STRUCT;

static constexpr size_t VRING_NUM = 8;
static constexpr size_t VRING_ALIGN = 16;

// Calculate vring offsets (same as in rpmsg.cpp)
static constexpr size_t AVAIL_OFFSET = sizeof(vring_desc) * VRING_NUM;  // 128 bytes
static constexpr size_t USED_OFFSET_RAW = AVAIL_OFFSET + sizeof(vring_avail);
static constexpr size_t USED_OFFSET = (USED_OFFSET_RAW + VRING_ALIGN - 1) & ~(VRING_ALIGN - 1);

} // anonymous namespace

class VirtioVringTest : public ::testing::Test {
protected:
    // Simulated vring memory
    alignas(16) uint8_t vring_mem[4096];

    vring_desc* desc() { return reinterpret_cast<vring_desc*>(vring_mem); }
    vring_avail* avail() { return reinterpret_cast<vring_avail*>(vring_mem + AVAIL_OFFSET); }
    vring_used* used() { return reinterpret_cast<vring_used*>(vring_mem + USED_OFFSET); }

    void SetUp() override {
        memset(vring_mem, 0, sizeof(vring_mem));
    }
};

TEST_F(VirtioVringTest, StructureSizes) {
    EXPECT_EQ(sizeof(vring_desc), 16);
    EXPECT_EQ(sizeof(vring_avail), 20);  // 4 + 8*2 = 20 (without used_event)
    EXPECT_EQ(sizeof(vring_used_elem), 8);
    EXPECT_EQ(sizeof(rpmsg_ns_msg), 40);  // 32 + 4 + 4
    EXPECT_EQ(sizeof(rpmsg_hdr), 16);
}

TEST_F(VirtioVringTest, VringOffsets) {
    // Descriptor table at start
    EXPECT_EQ(AVAIL_OFFSET, 128);  // 8 descriptors * 16 bytes

    // Available ring after descriptors
    EXPECT_EQ(reinterpret_cast<uint8_t*>(avail()) - vring_mem, 128);

    // Used ring aligned to VRING_ALIGN after available
    EXPECT_GE(USED_OFFSET, AVAIL_OFFSET + sizeof(vring_avail));
    EXPECT_EQ(USED_OFFSET % VRING_ALIGN, 0);
}

TEST_F(VirtioVringTest, DescriptorTableLayout) {
    // Set up a descriptor
    desc()[0].addr = 0x10042000;
    desc()[0].len = 512;
    desc()[0].flags = 0;
    desc()[0].next = 0;

    // Verify memory layout
    uint64_t* addr_ptr = reinterpret_cast<uint64_t*>(vring_mem);
    EXPECT_EQ(*addr_ptr, 0x10042000);

    uint32_t* len_ptr = reinterpret_cast<uint32_t*>(vring_mem + 8);
    EXPECT_EQ(*len_ptr, 512);
}

TEST_F(VirtioVringTest, AvailableRingLayout) {
    avail()->flags = 0;
    avail()->idx = 8;  // 8 buffers available
    avail()->ring[0] = 0;
    avail()->ring[1] = 1;
    avail()->ring[7] = 7;

    // Verify at expected offset
    uint16_t* flags_ptr = reinterpret_cast<uint16_t*>(vring_mem + AVAIL_OFFSET);
    EXPECT_EQ(*flags_ptr, 0);

    uint16_t* idx_ptr = reinterpret_cast<uint16_t*>(vring_mem + AVAIL_OFFSET + 2);
    EXPECT_EQ(*idx_ptr, 8);
}

TEST_F(VirtioVringTest, UsedRingLayout) {
    used()->flags = 0;
    used()->idx = 2;  // 2 buffers consumed
    used()->ring[0].id = 0;
    used()->ring[0].len = 56;  // Name service announcement
    used()->ring[1].id = 1;
    used()->ring[1].len = 36;  // Status message

    // Read back as raw memory
    uint16_t* idx_ptr = reinterpret_cast<uint16_t*>(vring_mem + USED_OFFSET + 2);
    EXPECT_EQ(*idx_ptr, 2);

    uint32_t* elem0_id = reinterpret_cast<uint32_t*>(vring_mem + USED_OFFSET + 4);
    EXPECT_EQ(*elem0_id, 0);

    uint32_t* elem0_len = reinterpret_cast<uint32_t*>(vring_mem + USED_OFFSET + 8);
    EXPECT_EQ(*elem0_len, 56);
}

TEST_F(VirtioVringTest, NameServiceAnnouncement) {
    // Build a name service announcement
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));

    rpmsg_hdr* hdr = reinterpret_cast<rpmsg_hdr*>(buffer);
    hdr->src = 0x400;           // Local endpoint address
    hdr->dst = 0x35;            // Name service address (53)
    hdr->len = sizeof(rpmsg_ns_msg);
    hdr->flags = 0;

    rpmsg_ns_msg* ns = reinterpret_cast<rpmsg_ns_msg*>(buffer + sizeof(rpmsg_hdr));
    strncpy(ns->name, "rpmsg-tty", sizeof(ns->name) - 1);
    ns->addr = 0x400;
    ns->flags = 0;  // RPMSG_NS_CREATE

    // Total message size
    size_t msg_size = sizeof(rpmsg_hdr) + sizeof(rpmsg_ns_msg);
    EXPECT_EQ(msg_size, 56);  // 16 + 40

    // Verify name
    EXPECT_STREQ(ns->name, "rpmsg-tty");
    EXPECT_EQ(ns->addr, 0x400u);
}

TEST_F(VirtioVringTest, SimulateM4ToA7Transfer) {
    // Simulate Linux providing buffers
    for (int i = 0; i < 8; i++) {
        desc()[i].addr = 0x10042000 + i * 512;
        desc()[i].len = 512;
        desc()[i].flags = 0;
        desc()[i].next = 0;
    }
    avail()->flags = 0;
    avail()->idx = 8;
    for (int i = 0; i < 8; i++) {
        avail()->ring[i] = i;
    }

    // M4 sends a message
    uint16_t lastAvailIdx = 0;
    uint16_t descIdx = avail()->ring[lastAvailIdx % VRING_NUM];
    EXPECT_EQ(descIdx, 0);

    // Write to buffer at descriptor address
    uint64_t bufAddr = desc()[descIdx].addr;
    EXPECT_EQ(bufAddr, 0x10042000);

    // Update used ring
    uint16_t usedIdx = used()->idx;
    used()->ring[usedIdx % VRING_NUM].id = descIdx;
    used()->ring[usedIdx % VRING_NUM].len = 56;  // NS announcement
    used()->idx = usedIdx + 1;

    // Verify
    EXPECT_EQ(used()->idx, 1);
    EXPECT_EQ(used()->ring[0].id, 0u);
    EXPECT_EQ(used()->ring[0].len, 56u);
}

TEST_F(VirtioVringTest, MultipleMessages) {
    // Setup buffers
    for (int i = 0; i < 8; i++) {
        desc()[i].addr = 0x10042000 + i * 512;
        desc()[i].len = 512;
    }
    avail()->idx = 8;
    for (int i = 0; i < 8; i++) {
        avail()->ring[i] = i;
    }

    // Send two messages
    used()->ring[0].id = 0;
    used()->ring[0].len = 56;   // NS announcement
    used()->ring[1].id = 1;
    used()->ring[1].len = 36;   // Status
    used()->idx = 2;

    // Verify used ring state
    EXPECT_EQ(used()->idx, 2);
    EXPECT_EQ(used()->ring[0].len, 56u);
    EXPECT_EQ(used()->ring[1].len, 36u);
}
