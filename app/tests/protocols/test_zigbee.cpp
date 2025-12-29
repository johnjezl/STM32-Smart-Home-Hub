/**
 * Zigbee Protocol Tests
 *
 * Comprehensive tests for the Zigbee protocol stack without hardware.
 * Uses mock serial port for transport testing.
 */

#include <gtest/gtest.h>
#include <smarthub/protocols/zigbee/ZnpFrame.hpp>
#include <smarthub/protocols/zigbee/ZnpTransport.hpp>
#include <smarthub/protocols/zigbee/ZigbeeCoordinator.hpp>
#include <smarthub/protocols/zigbee/ZigbeeHandler.hpp>
#include <smarthub/protocols/zigbee/ZigbeeDeviceDatabase.hpp>
#include <smarthub/protocols/zigbee/ZclConstants.hpp>
#include <smarthub/core/EventBus.hpp>
#include <queue>
#include <mutex>
#include <condition_variable>

using namespace smarthub;
using namespace smarthub::zigbee;

// ============================================================================
// ZnpFrame Tests
// ============================================================================

class ZnpFrameTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ZnpFrameTest, BuildSimpleFrame) {
    // Build a SYS_PING request
    ZnpFrame frame(ZnpType::SREQ, ZnpSubsystem::SYS, cmd::sys::PING);

    EXPECT_TRUE(frame.isRequest());
    EXPECT_FALSE(frame.isResponse());
    EXPECT_EQ(frame.subsystem(), ZnpSubsystem::SYS);
    EXPECT_EQ(frame.command(), cmd::sys::PING);
    EXPECT_EQ(frame.payload().size(), 0);
}

TEST_F(ZnpFrameTest, BuildFrameWithPayload) {
    // Build AF_DATA_REQUEST with some payload
    ZnpFrame frame(ZnpType::SREQ, ZnpSubsystem::AF, cmd::af::DATA_REQUEST);
    frame.appendWord(0x1234);  // destination address
    frame.appendByte(0x01);    // destination endpoint
    frame.appendByte(0x02);    // source endpoint
    frame.appendWord(0x0006);  // cluster ID (On/Off)

    EXPECT_EQ(frame.payload().size(), 6);

    auto payload = frame.payload();
    // Little-endian word: 0x1234 -> [0x34, 0x12]
    EXPECT_EQ(payload[0], 0x34);
    EXPECT_EQ(payload[1], 0x12);
    EXPECT_EQ(payload[2], 0x01);
    EXPECT_EQ(payload[3], 0x02);
    EXPECT_EQ(payload[4], 0x06);
    EXPECT_EQ(payload[5], 0x00);
}

TEST_F(ZnpFrameTest, SerializeFrame) {
    ZnpFrame frame(ZnpType::SREQ, ZnpSubsystem::SYS, cmd::sys::PING);
    auto data = frame.serialize();

    // Format: SOF + LEN + CMD0 + CMD1 + FCS
    EXPECT_EQ(data.size(), 5);
    EXPECT_EQ(data[0], ZnpFrame::SOF);
    EXPECT_EQ(data[1], 0x00);  // Length = 0
    EXPECT_EQ(data[2], 0x21);  // CMD0: SREQ | SYS
    EXPECT_EQ(data[3], 0x01);  // CMD1: PING
    // FCS = XOR of LEN, CMD0, CMD1 = 0x00 ^ 0x21 ^ 0x01 = 0x20
    EXPECT_EQ(data[4], 0x20);
}

TEST_F(ZnpFrameTest, ParseFrame) {
    // Valid SYS_PING response
    std::vector<uint8_t> data = {
        ZnpFrame::SOF,
        0x02,        // Length = 2
        0x61,        // CMD0: SRSP | SYS
        0x01,        // CMD1: PING
        0xAB, 0xCD,  // Capabilities (example payload)
        0x00         // FCS (calculate: 0x02 ^ 0x61 ^ 0x01 ^ 0xAB ^ 0xCD)
    };

    // Calculate correct FCS
    uint8_t fcs = 0x02 ^ 0x61 ^ 0x01 ^ 0xAB ^ 0xCD;
    data[6] = fcs;

    auto frame = ZnpFrame::parse(data.data(), data.size());
    ASSERT_TRUE(frame.has_value());
    EXPECT_TRUE(frame->isResponse());
    EXPECT_EQ(frame->subsystem(), ZnpSubsystem::SYS);
    EXPECT_EQ(frame->command(), cmd::sys::PING);
    EXPECT_EQ(frame->payload().size(), 2);
}

TEST_F(ZnpFrameTest, ParseInvalidFrame) {
    // Invalid FCS
    std::vector<uint8_t> data = {
        ZnpFrame::SOF,
        0x00,  // Length
        0x21,  // CMD0
        0x01,  // CMD1
        0xFF   // Wrong FCS
    };

    auto frame = ZnpFrame::parse(data.data(), data.size());
    EXPECT_FALSE(frame.has_value());
}

TEST_F(ZnpFrameTest, FindFrameInBuffer) {
    // Buffer with garbage then valid frame
    std::vector<uint8_t> buffer = {
        0xFF, 0xFF, 0xFF,  // Garbage
        ZnpFrame::SOF,
        0x00,        // Length
        0x21,        // CMD0
        0x01,        // CMD1
        0x20         // FCS
    };

    size_t frameStart, frameLen;
    bool found = ZnpFrame::findFrame(buffer.data(), buffer.size(), frameStart, frameLen);

    EXPECT_TRUE(found);
    EXPECT_EQ(frameStart, 3);
    EXPECT_EQ(frameLen, 5);
}

TEST_F(ZnpFrameTest, AppendDword) {
    ZnpFrame frame(ZnpType::SREQ, ZnpSubsystem::SYS, 0x00);
    frame.appendDWord(0x12345678);

    auto payload = frame.payload();
    ASSERT_EQ(payload.size(), 4);
    // Little-endian
    EXPECT_EQ(payload[0], 0x78);
    EXPECT_EQ(payload[1], 0x56);
    EXPECT_EQ(payload[2], 0x34);
    EXPECT_EQ(payload[3], 0x12);
}

TEST_F(ZnpFrameTest, AppendQword) {
    ZnpFrame frame(ZnpType::SREQ, ZnpSubsystem::SYS, 0x00);
    frame.appendQWord(0x0123456789ABCDEF);

    auto payload = frame.payload();
    ASSERT_EQ(payload.size(), 8);
    // Little-endian
    EXPECT_EQ(payload[0], 0xEF);
    EXPECT_EQ(payload[1], 0xCD);
    EXPECT_EQ(payload[2], 0xAB);
    EXPECT_EQ(payload[3], 0x89);
    EXPECT_EQ(payload[4], 0x67);
    EXPECT_EQ(payload[5], 0x45);
    EXPECT_EQ(payload[6], 0x23);
    EXPECT_EQ(payload[7], 0x01);
}

TEST_F(ZnpFrameTest, ToString) {
    ZnpFrame frame(ZnpType::SREQ, ZnpSubsystem::SYS, cmd::sys::PING);
    std::string str = frame.toString();

    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("SREQ"), std::string::npos);
}

// ============================================================================
// Mock Serial Port for Transport Testing
// ============================================================================

class MockSerialPort : public ISerialPort {
public:
    bool open(const std::string& /*port*/, int /*baudRate*/) override {
        if (m_openSuccess) {
            m_open = true;
        }
        return m_openSuccess;
    }

    void close() override {
        m_open = false;
    }

    bool isOpen() const override {
        return m_open;
    }

    ssize_t write(const uint8_t* data, size_t len) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_writtenData.insert(m_writtenData.end(), data, data + len);
        return static_cast<ssize_t>(len);
    }

    ssize_t read(uint8_t* buffer, size_t maxLen, int /*timeoutMs*/) override {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_readQueue.empty()) {
            return 0;  // Timeout
        }

        size_t bytesToRead = std::min(maxLen, m_readQueue.size());
        for (size_t i = 0; i < bytesToRead; ++i) {
            buffer[i] = m_readQueue.front();
            m_readQueue.pop();
        }

        return static_cast<ssize_t>(bytesToRead);
    }

    bool setDTR(bool /*state*/) override { return true; }
    bool setRTS(bool /*state*/) override { return true; }

    // Test helpers
    void queueReadData(const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (uint8_t b : data) {
            m_readQueue.push(b);
        }
    }

    std::vector<uint8_t> getWrittenData() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_writtenData;
    }

    void clearWrittenData() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_writtenData.clear();
    }

    void setOpenSuccess(bool success) { m_openSuccess = success; }

private:
    bool m_open = false;
    bool m_openSuccess = true;
    std::queue<uint8_t> m_readQueue;
    std::vector<uint8_t> m_writtenData;
    std::mutex m_mutex;
};

// ============================================================================
// ZnpTransport Tests
// ============================================================================

class ZnpTransportTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_mockSerial = std::make_unique<MockSerialPort>();
        m_mockSerialPtr = m_mockSerial.get();
    }

    void TearDown() override {
        // Transport owns the serial port after construction
    }

    std::unique_ptr<MockSerialPort> m_mockSerial;
    MockSerialPort* m_mockSerialPtr = nullptr;
};

TEST_F(ZnpTransportTest, OpenClose) {
    auto transport = std::make_unique<ZnpTransport>(
        std::move(m_mockSerial), "/dev/ttyUSB0", 115200);

    EXPECT_FALSE(transport->isOpen());

    EXPECT_TRUE(transport->open());
    EXPECT_TRUE(transport->isOpen());

    transport->close();
    EXPECT_FALSE(transport->isOpen());
}

TEST_F(ZnpTransportTest, OpenFail) {
    m_mockSerialPtr->setOpenSuccess(false);

    auto transport = std::make_unique<ZnpTransport>(
        std::move(m_mockSerial), "/dev/ttyUSB0", 115200);

    EXPECT_FALSE(transport->open());
    EXPECT_FALSE(transport->isOpen());
}

TEST_F(ZnpTransportTest, Send) {
    auto transport = std::make_unique<ZnpTransport>(
        std::move(m_mockSerial), "/dev/ttyUSB0", 115200);

    transport->open();

    ZnpFrame frame(ZnpType::SREQ, ZnpSubsystem::SYS, cmd::sys::PING);
    EXPECT_TRUE(transport->send(frame));

    auto written = m_mockSerialPtr->getWrittenData();
    EXPECT_EQ(written.size(), 5);  // SOF + LEN + CMD0 + CMD1 + FCS
    EXPECT_EQ(written[0], ZnpFrame::SOF);

    transport->close();
}

TEST_F(ZnpTransportTest, PortName) {
    auto transport = std::make_unique<ZnpTransport>(
        std::move(m_mockSerial), "/dev/ttyUSB1", 115200);

    EXPECT_EQ(transport->portName(), "/dev/ttyUSB1");
    EXPECT_EQ(transport->baudRate(), 115200);
}

// ============================================================================
// ZCL Constants Tests
// ============================================================================

class ZclConstantsTest : public ::testing::Test {};

TEST_F(ZclConstantsTest, DataTypeSizes) {
    EXPECT_EQ(zcl::getDataTypeSize(zcl::datatype::BOOLEAN), 1);
    EXPECT_EQ(zcl::getDataTypeSize(zcl::datatype::UINT8), 1);
    EXPECT_EQ(zcl::getDataTypeSize(zcl::datatype::UINT16), 2);
    EXPECT_EQ(zcl::getDataTypeSize(zcl::datatype::UINT32), 4);
    EXPECT_EQ(zcl::getDataTypeSize(zcl::datatype::INT8), 1);
    EXPECT_EQ(zcl::getDataTypeSize(zcl::datatype::INT16), 2);
    EXPECT_EQ(zcl::getDataTypeSize(zcl::datatype::INT32), 4);
    EXPECT_EQ(zcl::getDataTypeSize(zcl::datatype::ENUM8), 1);
    EXPECT_EQ(zcl::getDataTypeSize(zcl::datatype::ENUM16), 2);
}

TEST_F(ZclConstantsTest, ClusterConstants) {
    EXPECT_EQ(zcl::cluster::BASIC, 0x0000);
    EXPECT_EQ(zcl::cluster::ON_OFF, 0x0006);
    EXPECT_EQ(zcl::cluster::LEVEL_CONTROL, 0x0008);
    EXPECT_EQ(zcl::cluster::COLOR_CONTROL, 0x0300);
    EXPECT_EQ(zcl::cluster::TEMPERATURE_MEASUREMENT, 0x0402);
    EXPECT_EQ(zcl::cluster::IAS_ZONE, 0x0500);
}

TEST_F(ZclConstantsTest, AttributeConstants) {
    EXPECT_EQ(zcl::attr::basic::ZCL_VERSION, 0x0000);
    EXPECT_EQ(zcl::attr::basic::MANUFACTURER_NAME, 0x0004);
    EXPECT_EQ(zcl::attr::basic::MODEL_ID, 0x0005);
    EXPECT_EQ(zcl::attr::onoff::ON_OFF, 0x0000);
    EXPECT_EQ(zcl::attr::level::CURRENT_LEVEL, 0x0000);
}

TEST_F(ZclConstantsTest, CommandConstants) {
    EXPECT_EQ(zcl::cmd::onoff::OFF, 0x00);
    EXPECT_EQ(zcl::cmd::onoff::ON, 0x01);
    EXPECT_EQ(zcl::cmd::onoff::TOGGLE, 0x02);
    EXPECT_EQ(zcl::cmd::level::MOVE_TO_LEVEL, 0x00);
}

// ============================================================================
// ZigbeeDeviceDatabase Tests
// ============================================================================

class ZigbeeDeviceDatabaseTest : public ::testing::Test {
protected:
    ZigbeeDeviceDatabase db;
};

TEST_F(ZigbeeDeviceDatabaseTest, InitialState) {
    EXPECT_FALSE(db.isLoaded());
    EXPECT_EQ(db.size(), 0);
}

TEST_F(ZigbeeDeviceDatabaseTest, LoadFromJson) {
    nlohmann::json json = {
        {"devices", {
            {
                {"manufacturer", "IKEA"},
                {"model", "TRADFRI bulb E27"},
                {"displayName", "IKEA TRADFRI E27 Bulb"},
                {"deviceType", "color_light"}
            },
            {
                {"manufacturer", "Philips"},
                {"model", "LWB010"},
                {"displayName", "Philips Hue White"},
                {"deviceType", "dimmer"}
            },
            {
                {"manufacturer", "Aqara"},
                {"model", "WSDCGQ11LM"},
                {"displayName", "Aqara Temperature Sensor"},
                {"deviceType", "temperature_sensor"}
            }
        }}
    };

    EXPECT_TRUE(db.loadFromJson(json));
    EXPECT_TRUE(db.isLoaded());
    EXPECT_EQ(db.size(), 3);
}

TEST_F(ZigbeeDeviceDatabaseTest, LookupExact) {
    nlohmann::json json = {
        {"devices", {
            {
                {"manufacturer", "IKEA"},
                {"model", "TRADFRI bulb"},
                {"displayName", "IKEA Bulb"},
                {"deviceType", "dimmer"}
            }
        }}
    };

    db.loadFromJson(json);

    auto entry = db.lookup("IKEA", "TRADFRI bulb");
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->displayName, "IKEA Bulb");
    EXPECT_EQ(entry->deviceType, DeviceType::Dimmer);
}

TEST_F(ZigbeeDeviceDatabaseTest, LookupCaseInsensitive) {
    nlohmann::json json = {
        {"devices", {
            {
                {"manufacturer", "IKEA"},
                {"model", "TRADFRI"},
                {"displayName", "IKEA Device"},
                {"deviceType", "switch"}
            }
        }}
    };

    db.loadFromJson(json);

    auto entry = db.lookup("ikea", "tradfri");
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->displayName, "IKEA Device");
}

TEST_F(ZigbeeDeviceDatabaseTest, LookupNotFound) {
    nlohmann::json json = {
        {"devices", {
            {
                {"manufacturer", "IKEA"},
                {"model", "TRADFRI"},
                {"displayName", "IKEA Device"},
                {"deviceType", "switch"}
            }
        }}
    };

    db.loadFromJson(json);

    auto entry = db.lookup("Unknown", "Device");
    EXPECT_FALSE(entry.has_value());
}

TEST_F(ZigbeeDeviceDatabaseTest, AddDevice) {
    ZigbeeDeviceEntry entry;
    entry.manufacturer = "Test";
    entry.model = "Device";
    entry.displayName = "Test Device";
    entry.deviceType = DeviceType::Switch;

    db.addDevice(entry);

    EXPECT_EQ(db.size(), 1);
    auto result = db.lookup("Test", "Device");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->displayName, "Test Device");
}

TEST_F(ZigbeeDeviceDatabaseTest, ParseDeviceTypes) {
    nlohmann::json json = {
        {"devices", {
            {{"manufacturer", "A"}, {"model", "1"}, {"deviceType", "switch"}},
            {{"manufacturer", "A"}, {"model", "2"}, {"deviceType", "dimmer"}},
            {{"manufacturer", "A"}, {"model", "3"}, {"deviceType", "color_light"}},
            {{"manufacturer", "A"}, {"model", "4"}, {"deviceType", "temperature_sensor"}},
            {{"manufacturer", "A"}, {"model", "5"}, {"deviceType", "motion_sensor"}},
            {{"manufacturer", "A"}, {"model", "6"}, {"deviceType", "outlet"}},  // Alias
            {{"manufacturer", "A"}, {"model", "7"}, {"deviceType", "occupancy"}}  // Alias
        }}
    };

    db.loadFromJson(json);

    EXPECT_EQ(db.lookup("A", "1")->deviceType, DeviceType::Switch);
    EXPECT_EQ(db.lookup("A", "2")->deviceType, DeviceType::Dimmer);
    EXPECT_EQ(db.lookup("A", "3")->deviceType, DeviceType::ColorLight);
    EXPECT_EQ(db.lookup("A", "4")->deviceType, DeviceType::TemperatureSensor);
    EXPECT_EQ(db.lookup("A", "5")->deviceType, DeviceType::MotionSensor);
    EXPECT_EQ(db.lookup("A", "6")->deviceType, DeviceType::Switch);  // outlet -> switch
    EXPECT_EQ(db.lookup("A", "7")->deviceType, DeviceType::MotionSensor);  // occupancy -> motion
}

TEST_F(ZigbeeDeviceDatabaseTest, InvalidJson) {
    nlohmann::json json = {
        {"invalid", "data"}  // Missing "devices" array
    };

    EXPECT_FALSE(db.loadFromJson(json));
    EXPECT_FALSE(db.isLoaded());
}

// ============================================================================
// ZigbeeCoordinator Tests (with mock transport)
// ============================================================================

class ZigbeeCoordinatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_mockSerial = std::make_unique<MockSerialPort>();
        m_mockSerialPtr = m_mockSerial.get();
    }

    // Helper to create coordinator with mock transport
    std::unique_ptr<ZigbeeCoordinator> createCoordinator() {
        auto transport = std::make_unique<ZnpTransport>(
            std::move(m_mockSerial), "/dev/ttyUSB0", 115200);
        return std::make_unique<ZigbeeCoordinator>(std::move(transport));
    }

    std::unique_ptr<MockSerialPort> m_mockSerial;
    MockSerialPort* m_mockSerialPtr = nullptr;
};

TEST_F(ZigbeeCoordinatorTest, InitialState) {
    auto coord = createCoordinator();

    EXPECT_FALSE(coord->isNetworkUp());
    EXPECT_EQ(coord->deviceCount(), 0);
    EXPECT_EQ(coord->panId(), 0);
    EXPECT_EQ(coord->channel(), 0);
}

TEST_F(ZigbeeCoordinatorTest, GetDevice) {
    auto coord = createCoordinator();

    // No devices initially
    auto device = coord->getDevice(0x0011223344556677);
    EXPECT_FALSE(device.has_value());
}

TEST_F(ZigbeeCoordinatorTest, GetAllDevices) {
    auto coord = createCoordinator();

    auto devices = coord->getAllDevices();
    EXPECT_TRUE(devices.empty());
}

// ============================================================================
// ZigbeeHandler Tests
// ============================================================================

class ZigbeeHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_eventBus = std::make_unique<EventBus>();
    }

    void TearDown() override {
        m_eventBus.reset();
    }

    std::unique_ptr<EventBus> m_eventBus;
};

TEST_F(ZigbeeHandlerTest, InitialConfig) {
    nlohmann::json config = {
        {"port", "/dev/ttyUSB0"},
        {"baudRate", 115200}
    };

    ZigbeeHandler handler(*m_eventBus, config);

    EXPECT_EQ(handler.name(), "zigbee");
    EXPECT_EQ(handler.version(), "1.0.0");
    EXPECT_TRUE(handler.supportsDiscovery());
    EXPECT_FALSE(handler.isDiscovering());
    EXPECT_FALSE(handler.isConnected());
}

TEST_F(ZigbeeHandlerTest, StatusBeforeInit) {
    nlohmann::json config = {
        {"port", "/dev/ttyUSB0"},
        {"baudRate", 115200}
    };

    ZigbeeHandler handler(*m_eventBus, config);

    auto status = handler.getStatus();

    EXPECT_EQ(status["protocol"], "zigbee");
    EXPECT_FALSE(status["initialized"].get<bool>());
    EXPECT_FALSE(status["connected"].get<bool>());
    EXPECT_FALSE(status["discovering"].get<bool>());
}

TEST_F(ZigbeeHandlerTest, CallbacksCanBeSet) {
    nlohmann::json config = {
        {"port", "/dev/ttyUSB0"}
    };

    ZigbeeHandler handler(*m_eventBus, config);

    bool discoveredCalled = false;
    bool stateCalled = false;
    bool availabilityCalled = false;

    handler.setDeviceDiscoveredCallback([&](DevicePtr) {
        discoveredCalled = true;
    });

    handler.setDeviceStateCallback([&](const std::string&, const std::string&, const nlohmann::json&) {
        stateCalled = true;
    });

    handler.setDeviceAvailabilityCallback([&](const std::string&, DeviceAvailability) {
        availabilityCalled = true;
    });

    // Callbacks are set but won't be called since no devices
    EXPECT_FALSE(discoveredCalled);
    EXPECT_FALSE(stateCalled);
    EXPECT_FALSE(availabilityCalled);
}

// ============================================================================
// ZclAttributeValue Tests
// ============================================================================

class ZclAttributeValueTest : public ::testing::Test {};

TEST_F(ZclAttributeValueTest, BooleanConversion) {
    ZclAttributeValue attr;
    attr.dataType = zcl::datatype::BOOLEAN;

    attr.data = {0x00};
    EXPECT_FALSE(attr.asBool());

    attr.data = {0x01};
    EXPECT_TRUE(attr.asBool());

    attr.data = {0xFF};  // Any non-zero is true
    EXPECT_TRUE(attr.asBool());
}

TEST_F(ZclAttributeValueTest, Uint8Conversion) {
    ZclAttributeValue attr;
    attr.dataType = zcl::datatype::UINT8;
    attr.data = {0xAB};

    EXPECT_EQ(attr.asUint8(), 0xAB);
}

TEST_F(ZclAttributeValueTest, Uint16Conversion) {
    ZclAttributeValue attr;
    attr.dataType = zcl::datatype::UINT16;
    attr.data = {0x34, 0x12};  // Little-endian

    EXPECT_EQ(attr.asUint16(), 0x1234);
}

TEST_F(ZclAttributeValueTest, Uint32Conversion) {
    ZclAttributeValue attr;
    attr.dataType = zcl::datatype::UINT32;
    attr.data = {0x78, 0x56, 0x34, 0x12};  // Little-endian

    EXPECT_EQ(attr.asUint32(), 0x12345678);
}

TEST_F(ZclAttributeValueTest, Int16Conversion) {
    ZclAttributeValue attr;
    attr.dataType = zcl::datatype::INT16;

    // Positive value
    attr.data = {0x34, 0x12};
    EXPECT_EQ(attr.asInt16(), 0x1234);

    // Negative value (-1)
    attr.data = {0xFF, 0xFF};
    EXPECT_EQ(attr.asInt16(), -1);

    // Temperature example: 2350 = 23.50°C
    attr.data = {0x2E, 0x09};  // 2350 in little-endian
    EXPECT_EQ(attr.asInt16(), 2350);
}

TEST_F(ZclAttributeValueTest, StringConversion) {
    ZclAttributeValue attr;
    attr.dataType = zcl::datatype::CHAR_STR;
    attr.data = {0x05, 'H', 'e', 'l', 'l', 'o'};  // Length-prefixed string

    EXPECT_EQ(attr.asString(), "Hello");
}

// ============================================================================
// ZigbeeDeviceInfo Tests
// ============================================================================

class ZigbeeDeviceInfoTest : public ::testing::Test {};

TEST_F(ZigbeeDeviceInfoTest, DefaultValues) {
    ZigbeeDeviceInfo info;

    EXPECT_EQ(info.networkAddress, 0);
    EXPECT_EQ(info.ieeeAddress, 0);
    EXPECT_EQ(info.deviceType, 0);
    EXPECT_TRUE(info.manufacturer.empty());
    EXPECT_TRUE(info.model.empty());
    EXPECT_TRUE(info.endpoints.empty());
    EXPECT_FALSE(info.available);
}

TEST_F(ZigbeeDeviceInfoTest, ClusterStorage) {
    ZigbeeDeviceInfo info;

    info.endpoints = {1, 2};
    info.inClusters[1] = {0x0000, 0x0006, 0x0008};
    info.inClusters[2] = {0x0402};

    EXPECT_EQ(info.endpoints.size(), 2);
    EXPECT_EQ(info.inClusters[1].size(), 3);
    EXPECT_EQ(info.inClusters[2].size(), 1);
}

// ============================================================================
// Integration-style Tests
// ============================================================================

class ZigbeeIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_eventBus = std::make_unique<EventBus>();
    }

    std::unique_ptr<EventBus> m_eventBus;
};

TEST_F(ZigbeeIntegrationTest, FrameBuildParseRoundtrip) {
    // Build a frame
    ZnpFrame original(ZnpType::SREQ, ZnpSubsystem::AF, cmd::af::DATA_REQUEST);
    original.appendWord(0x1234);
    original.appendByte(0x01);
    original.appendByte(0x02);
    original.appendWord(0x0006);

    // Serialize
    auto data = original.serialize();

    // Parse back
    auto parsed = ZnpFrame::parse(data.data(), data.size());

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->subsystem(), original.subsystem());
    EXPECT_EQ(parsed->command(), original.command());
    EXPECT_EQ(parsed->payload().size(), original.payload().size());
    EXPECT_EQ(parsed->payload(), original.payload());
}

TEST_F(ZigbeeIntegrationTest, DeviceDatabaseWithQuirks) {
    nlohmann::json json = {
        {"devices", {
            {
                {"manufacturer", "Xiaomi"},
                {"model", "lumi.sensor_motion"},
                {"displayName", "Xiaomi Motion Sensor"},
                {"deviceType", "motion_sensor"},
                {"quirks", {
                    {"occupancy_timeout", 90},
                    {"requires_reporting", false}
                }}
            }
        }}
    };

    ZigbeeDeviceDatabase db;
    EXPECT_TRUE(db.loadFromJson(json));

    auto entry = db.lookup("Xiaomi", "lumi.sensor_motion");
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->deviceType, DeviceType::MotionSensor);
    EXPECT_TRUE(entry->quirks.contains("occupancy_timeout"));
    EXPECT_EQ(entry->quirks["occupancy_timeout"], 90);
}

// ============================================================================
// Protocol Factory Registration Test
// ============================================================================

TEST(ZigbeeProtocolTest, HandlerCreation) {
    EventBus eventBus;
    nlohmann::json config = {
        {"port", "/dev/ttyUSB0"},
        {"baudRate", 115200}
    };

    // Handler can be created
    auto handler = std::make_unique<ZigbeeHandler>(eventBus, config);
    EXPECT_NE(handler, nullptr);
    EXPECT_EQ(handler->name(), "zigbee");
}
