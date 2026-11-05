/**
 * Zigbee flow tests using scripted ZNP frames.
 *
 * Runs the real transport, coordinator and handler against FakeZnpCoordinator.
 *
 * DISABLED_ tests describe required behaviour that doesn't work yet. Each names
 * the issue that should enable it.
 */

#include <gtest/gtest.h>

#include "../mocks/FakeZnpCoordinator.hpp"

#include <smarthub/core/EventBus.hpp>
#include <smarthub/protocols/zigbee/ZigbeeCoordinator.hpp>
#include <smarthub/protocols/zigbee/ZigbeeHandler.hpp>

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace smarthub;
using namespace smarthub::zigbee;
using namespace std::chrono_literals;

namespace {

const FakeZigbeeDevice LIGHT{
    0x1234, 0x00124B0000001234ULL,
    {{1, {zcl::cluster::ON_OFF, zcl::cluster::LEVEL_CONTROL}}}};

const FakeZigbeeDevice PLUG{
    0x2222, 0x00124B0000002222ULL,
    {{3, {zcl::cluster::ON_OFF}}}};

const FakeZigbeeDevice CLIMATE{
    0x3333, 0x00124B0000003333ULL,
    {{1, {zcl::cluster::TEMPERATURE_MEASUREMENT, zcl::cluster::RELATIVE_HUMIDITY}}}};

const FakeZigbeeDevice MOTION{
    0x4444, 0x00124B0000004444ULL,
    {{1, {zcl::cluster::IAS_ZONE}}}};

std::string idOf(const FakeZigbeeDevice& device) {
    char id[32];
    snprintf(id, sizeof(id), "zigbee_%016llx", static_cast<unsigned long long>(device.ieeeAddr));
    return id;
}

/** Fields of an AF_DATA_REQUEST frame, which is what goes out over the air. */
struct AirFrame {
    uint16_t nwkAddr;
    uint8_t endpoint;
    uint16_t cluster;
    uint8_t zclFrameControl;
    uint8_t zclCommand;
    std::vector<uint8_t> zclPayload;

    explicit AirFrame(const ZnpFrame& frame)
        : nwkAddr(frame.getWord(0))
        , endpoint(frame.getByte(2))
        , cluster(frame.getWord(4))
        , zclFrameControl(frame.getByte(10))
        , zclCommand(frame.getByte(12))
        , zclPayload(frame.getBytes(13, frame.getByte(9) - 3)) {}
};

std::vector<AirFrame> airFrames(const FakeZnpCoordinator& fake) {
    std::vector<AirFrame> out;
    for (const auto& frame : fake.sentFrames(ZnpSubsystem::AF, cmd::af::DATA_REQUEST)) {
        out.emplace_back(frame);
    }
    return out;
}

std::unique_ptr<ZigbeeCoordinator> coordinatorFor(FakeZnpCoordinator& fake) {
    return std::make_unique<ZigbeeCoordinator>(
        std::make_unique<ZnpTransport>(fake.createSerialPort(), "fake"));
}

template <typename Predicate>
bool eventually(Predicate predicate, std::chrono::milliseconds timeout = 5s) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        std::this_thread::sleep_for(5ms);
    }
    return predicate();
}

} // namespace

// ============================================================================
// Coordinator bring-up
// ============================================================================

class CoordinatorStartupTest : public ::testing::Test {
protected:
    FakeZnpCoordinator fake;
    std::unique_ptr<ZigbeeCoordinator> coordinator = coordinatorFor(fake);
};

TEST_F(CoordinatorStartupTest, InitializesAndStartsNetwork) {
    ASSERT_TRUE(coordinator->initialize());
    ASSERT_TRUE(coordinator->startNetwork());

    EXPECT_EQ(fake.sentFrames(ZnpSubsystem::SYS, cmd::sys::PING).size(), 1u);
    EXPECT_EQ(fake.sentFrames(ZnpSubsystem::AF, cmd::af::REGISTER).size(), 2u);
    EXPECT_EQ(fake.sentFrames(ZnpSubsystem::ZDO, cmd::zdo::STARTUP_FROM_APP).size(), 1u);
    EXPECT_TRUE(coordinator->isNetworkUp());
}

TEST_F(CoordinatorStartupTest, InitializeFailsWhenRadioDoesNotAnswer) {
    fake.setUnresponsive(true);

    EXPECT_FALSE(coordinator->initialize());
}

TEST_F(CoordinatorStartupTest, StartNetworkFailsOnBadStartupStatus) {
    fake.setResponder(ZnpSubsystem::ZDO, cmd::zdo::STARTUP_FROM_APP,
                      [](const ZnpFrame&) { return std::vector<uint8_t>{2}; });

    ASSERT_TRUE(coordinator->initialize());
    EXPECT_FALSE(coordinator->startNetwork());
    EXPECT_FALSE(coordinator->isNetworkUp());
}

TEST_F(CoordinatorStartupTest, ResetIndicationMarksNetworkDown) {
    ASSERT_TRUE(coordinator->initialize());
    ASSERT_TRUE(coordinator->startNetwork());

    fake.injectIndication(ZnpFrame(ZnpType::AREQ, ZnpSubsystem::SYS, cmd::sys::RESET_IND));

    EXPECT_TRUE(eventually([&] { return !coordinator->isNetworkUp(); }));
}

// ============================================================================
// Handler flows
// ============================================================================

class ZigbeeHandlerFlowTest : public ::testing::Test {
protected:
    struct StateChange {
        std::string deviceId;
        std::string property;
        nlohmann::json value;
    };

    void SetUp() override {
        for (const auto& device : {LIGHT, PLUG, CLIMATE, MOTION}) {
            fake.addDevice(device);
        }
        handler = makeHandler(fake);
        ASSERT_TRUE(handler->initialize());
    }

    std::unique_ptr<ZigbeeHandler> makeHandler(FakeZnpCoordinator& radio) {
        auto created = std::make_unique<ZigbeeHandler>(
            eventBus, nlohmann::json::object(), coordinatorFor(radio));

        created->setDeviceDiscoveredCallback([this](DevicePtr device) {
            std::lock_guard<std::mutex> lock(mutex);
            discovered.push_back(device);
        });
        created->setPendingDeviceCallback([this](DevicePtr device) {
            std::lock_guard<std::mutex> lock(mutex);
            pending.push_back(device);
        });
        created->setDeviceStateCallback(
            [this](const std::string& id, const std::string& property, const nlohmann::json& value) {
                std::lock_guard<std::mutex> lock(mutex);
                states.push_back({id, property, value});
            });
        created->setDeviceAvailabilityCallback(
            [this](const std::string& id, DeviceAvailability availability) {
                std::lock_guard<std::mutex> lock(mutex);
                availabilityChanges.emplace_back(id, availability);
            });
        return created;
    }

    /** Runs the whole join sequence and waits for it to finish. */
    DevicePtr pair(const FakeZigbeeDevice& device) {
        size_t before = discoveredCount();
        fake.announceDevice(device);
        if (!eventually([&] { return discoveredCount() > before; }, 10s)) {
            return nullptr;
        }
        std::lock_guard<std::mutex> lock(mutex);
        return discovered.back();
    }

    size_t discoveredCount() {
        std::lock_guard<std::mutex> lock(mutex);
        return discovered.size();
    }

    size_t pendingCount() {
        std::lock_guard<std::mutex> lock(mutex);
        return pending.size();
    }

    std::optional<nlohmann::json> stateOf(const std::string& id, const std::string& property) {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto it = states.rbegin(); it != states.rend(); ++it) {
            if (it->deviceId == id && it->property == property) return it->value;
        }
        return std::nullopt;
    }

    nlohmann::json waitForState(const std::string& id, const std::string& property) {
        eventually([&] { return stateOf(id, property).has_value(); });
        return stateOf(id, property).value_or(nlohmann::json());
    }

    std::mutex mutex;
    std::vector<DevicePtr> discovered;
    std::vector<DevicePtr> pending;
    std::vector<StateChange> states;
    std::vector<std::pair<std::string, DeviceAvailability>> availabilityChanges;

    FakeZnpCoordinator fake;
    EventBus eventBus;
    std::unique_ptr<ZigbeeHandler> handler;
};

// ---- Pairing ---------------------------------------------------------------

TEST_F(ZigbeeHandlerFlowTest, DiscoveryOpensNetworkForJoining) {
    handler->startDiscovery();

    EXPECT_TRUE(handler->isDiscovering());
    auto frames = fake.sentFrames(ZnpSubsystem::ZDO, cmd::zdo::MGMT_PERMIT_JOIN_REQ);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].getByte(3), 60);    // Duration in seconds

    handler->stopDiscovery();
    EXPECT_FALSE(handler->isDiscovering());
}

TEST_F(ZigbeeHandlerFlowTest, JoinDuringDiscoveryIsHeldAsPendingNotAdded) {
    handler->startDiscovery();

    fake.announceDevice(LIGHT);

    ASSERT_TRUE(eventually([&] { return pendingCount() == 1; }, 10s));
    EXPECT_EQ(discoveredCount(), 0u);
    ASSERT_NE(handler->getPendingDevice(), nullptr);
    EXPECT_EQ(handler->getPendingDevice()->id(), idOf(LIGHT));

    handler->clearPendingDevice();
    EXPECT_EQ(handler->getPendingDevice(), nullptr);
}

TEST_F(ZigbeeHandlerFlowTest, JoinOutsideDiscoveryIsAddedDirectly) {
    auto device = pair(PLUG);

    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->id(), idOf(PLUG));
    EXPECT_EQ(pendingCount(), 0u);
}

TEST_F(ZigbeeHandlerFlowTest, DeviceTypeComesFromClusters) {
    auto light = pair(LIGHT);
    auto plug = pair(PLUG);
    auto climate = pair(CLIMATE);
    auto motion = pair(MOTION);

    ASSERT_TRUE(light && plug && climate && motion);
    EXPECT_EQ(light->type(), DeviceType::Dimmer);
    EXPECT_EQ(plug->type(), DeviceType::Switch);
    EXPECT_EQ(climate->type(), DeviceType::TemperatureSensor);
    EXPECT_EQ(motion->type(), DeviceType::MotionSensor);
}

TEST_F(ZigbeeHandlerFlowTest, PairingConfiguresAttributeReporting) {
    ASSERT_NE(pair(LIGHT), nullptr);

    bool onOff = false;
    bool level = false;
    for (const auto& frame : airFrames(fake)) {
        if (frame.zclCommand != zcl::global_cmd::CONFIGURE_REPORTING) continue;
        EXPECT_EQ(frame.nwkAddr, LIGHT.nwkAddr);
        onOff |= frame.cluster == zcl::cluster::ON_OFF;
        level |= frame.cluster == zcl::cluster::LEVEL_CONTROL;
    }
    EXPECT_TRUE(onOff);
    EXPECT_TRUE(level);
}

TEST_F(ZigbeeHandlerFlowTest, LeavingMarksDeviceOffline) {
    ASSERT_NE(pair(PLUG), nullptr);

    fake.deviceLeft(PLUG);

    ASSERT_TRUE(eventually([&] {
        std::lock_guard<std::mutex> lock(mutex);
        return !availabilityChanges.empty();
    }));
    std::lock_guard<std::mutex> lock(mutex);
    EXPECT_EQ(availabilityChanges[0].first, idOf(PLUG));
    EXPECT_EQ(availabilityChanges[0].second, DeviceAvailability::Offline);
}

// ---- Commands --------------------------------------------------------------

class ZigbeeCommandTest : public ZigbeeHandlerFlowTest {
protected:
    void SetUp() override {
        ZigbeeHandlerFlowTest::SetUp();
        ASSERT_NE(pair(LIGHT), nullptr);
        ASSERT_NE(pair(PLUG), nullptr);
        fake.clearSentFrames();
    }
};

TEST_F(ZigbeeCommandTest, OnReachesTheRadio) {
    ASSERT_TRUE(handler->sendCommand(idOf(LIGHT), "on", {{"on", true}}));

    auto frames = airFrames(fake);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].nwkAddr, LIGHT.nwkAddr);
    EXPECT_EQ(frames[0].endpoint, 1);
    EXPECT_EQ(frames[0].cluster, zcl::cluster::ON_OFF);
    EXPECT_EQ(frames[0].zclCommand, zcl::cmd::onoff::ON);
    EXPECT_TRUE(frames[0].zclFrameControl & zcl::frame_ctrl::CLUSTER_SPECIFIC);
}

TEST_F(ZigbeeCommandTest, OffReachesTheRadio) {
    ASSERT_TRUE(handler->sendCommand(idOf(LIGHT), "off", {{"on", false}}));

    auto frames = airFrames(fake);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].zclCommand, zcl::cmd::onoff::OFF);
}

TEST_F(ZigbeeCommandTest, CommandUsesTheDevicesOwnEndpoint) {
    ASSERT_TRUE(handler->sendCommand(idOf(PLUG), "on", {{"on", true}}));

    auto frames = airFrames(fake);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].nwkAddr, PLUG.nwkAddr);
    EXPECT_EQ(frames[0].endpoint, 3);
}

TEST_F(ZigbeeCommandTest, BrightnessSendsMoveToLevel) {
    ASSERT_TRUE(handler->sendCommand(idOf(LIGHT), "brightness", {{"brightness", 128}}));

    auto frames = airFrames(fake);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].cluster, zcl::cluster::LEVEL_CONTROL);
    EXPECT_EQ(frames[0].zclCommand, zcl::cmd::level::MOVE_TO_LEVEL_ONOFF);
    ASSERT_FALSE(frames[0].zclPayload.empty());
    EXPECT_EQ(frames[0].zclPayload[0], 128);
}

TEST_F(ZigbeeCommandTest, RadioRejectionIsReported) {
    fake.setResponder(ZnpSubsystem::AF, cmd::af::DATA_REQUEST,
                      [](const ZnpFrame&) { return std::vector<uint8_t>{0x01}; });

    EXPECT_FALSE(handler->sendCommand(idOf(LIGHT), "on", {{"on", true}}));
}

TEST_F(ZigbeeCommandTest, UnknownDeviceSendsNothing) {
    EXPECT_FALSE(handler->sendCommand("zigbee_00000000deadbeef", "on", {{"on", true}}));
    EXPECT_TRUE(airFrames(fake).empty());
}

TEST_F(ZigbeeCommandTest, UnknownCommandSendsNothing) {
    EXPECT_FALSE(handler->sendCommand(idOf(LIGHT), "explode", nlohmann::json::object()));
    EXPECT_TRUE(airFrames(fake).empty());
}

// Issue #3: DeviceManager::setDeviceState sends the command "set" with the
// property in its parameters, which the handler doesn't accept.
TEST_F(ZigbeeCommandTest, DISABLED_SetCommandFromDeviceManagerReachesRadio) {
    ASSERT_TRUE(handler->sendCommand(idOf(LIGHT), "set", {{"on", true}}));

    auto frames = airFrames(fake);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].cluster, zcl::cluster::ON_OFF);
    EXPECT_EQ(frames[0].zclCommand, zcl::cmd::onoff::ON);
}

// ---- Incoming reports ------------------------------------------------------

class ZigbeeReportTest : public ZigbeeHandlerFlowTest {};

TEST_F(ZigbeeReportTest, TemperatureIsConvertedToCelsius) {
    ASSERT_NE(pair(CLIMATE), nullptr);

    fake.reportAttribute(CLIMATE.nwkAddr, 1, zcl::cluster::TEMPERATURE_MEASUREMENT,
                         zcl::attr::temperature::MEASURED_VALUE, zcl::datatype::INT16,
                         {0x66, 0x08});     // 2150 = 21.50 C

    EXPECT_DOUBLE_EQ(waitForState(idOf(CLIMATE), "temperature").get<double>(), 21.5);
}

TEST_F(ZigbeeReportTest, NegativeTemperature) {
    ASSERT_NE(pair(CLIMATE), nullptr);

    fake.reportAttribute(CLIMATE.nwkAddr, 1, zcl::cluster::TEMPERATURE_MEASUREMENT,
                         zcl::attr::temperature::MEASURED_VALUE, zcl::datatype::INT16,
                         {0x9C, 0xFF});     // -100 = -1.00 C

    EXPECT_DOUBLE_EQ(waitForState(idOf(CLIMATE), "temperature").get<double>(), -1.0);
}

TEST_F(ZigbeeReportTest, HumidityIsConvertedToPercent) {
    ASSERT_NE(pair(CLIMATE), nullptr);

    fake.reportAttribute(CLIMATE.nwkAddr, 1, zcl::cluster::RELATIVE_HUMIDITY,
                         zcl::attr::humidity::MEASURED_VALUE, zcl::datatype::UINT16,
                         {0xC6, 0x11});     // 4550 = 45.50 %

    EXPECT_DOUBLE_EQ(waitForState(idOf(CLIMATE), "humidity").get<double>(), 45.5);
}

TEST_F(ZigbeeReportTest, OnOffReport) {
    ASSERT_NE(pair(PLUG), nullptr);

    fake.reportAttribute(PLUG.nwkAddr, 3, zcl::cluster::ON_OFF,
                         zcl::attr::onoff::ON_OFF, zcl::datatype::BOOLEAN, {0x01});

    EXPECT_EQ(waitForState(idOf(PLUG), "on"), true);
}

TEST_F(ZigbeeReportTest, IasZoneNotificationSetsAlarm) {
    ASSERT_NE(pair(MOTION), nullptr);

    fake.clusterCommand(MOTION.nwkAddr, 1, zcl::cluster::IAS_ZONE, 0x00, {0x01, 0x00});

    EXPECT_EQ(waitForState(idOf(MOTION), "alarm"), true);
}

TEST_F(ZigbeeReportTest, ReportFromUnknownDeviceIsIgnored) {
    fake.reportAttribute(0x7777, 1, zcl::cluster::ON_OFF,
                         zcl::attr::onoff::ON_OFF, zcl::datatype::BOOLEAN, {0x01});

    // Prove the frame was handled by sending a known one after it.
    ASSERT_NE(pair(PLUG), nullptr);
    fake.reportAttribute(PLUG.nwkAddr, 3, zcl::cluster::ON_OFF,
                         zcl::attr::onoff::ON_OFF, zcl::datatype::BOOLEAN, {0x01});
    ASSERT_EQ(waitForState(idOf(PLUG), "on"), true);

    std::lock_guard<std::mutex> lock(mutex);
    EXPECT_EQ(states.size(), 1u);
}

// ---- Bad data from the radio -----------------------------------------------

class ZigbeeBadDataTest : public ZigbeeHandlerFlowTest {
protected:
    /** The handler still works after whatever the test injected. */
    void expectStillWorking() {
        auto device = pair(PLUG);
        ASSERT_NE(device, nullptr);
        fake.reportAttribute(PLUG.nwkAddr, 3, zcl::cluster::ON_OFF,
                             zcl::attr::onoff::ON_OFF, zcl::datatype::BOOLEAN, {0x01});
        EXPECT_EQ(waitForState(idOf(PLUG), "on"), true);
    }
};

TEST_F(ZigbeeBadDataTest, GarbageBetweenFramesIsSkipped) {
    fake.injectRaw({0x00, 0x13, 0x37, 0xAA, 0x55});

    expectStillWorking();
}

TEST_F(ZigbeeBadDataTest, FrameWithBadChecksumIsDropped) {
    auto bytes = ZnpFrame(ZnpType::AREQ, ZnpSubsystem::ZDO, cmd::zdo::STATE_CHANGE_IND,
                          {static_cast<uint8_t>(ZnpDeviceState::HOLD)}).serialize();
    bytes.back() ^= 0xFF;
    fake.injectRaw(bytes);

    expectStillWorking();
}

TEST_F(ZigbeeBadDataTest, FramesSplitAcrossReadsAreReassembled) {
    fake.setMaxReadChunk(1);

    expectStillWorking();
}

TEST_F(ZigbeeBadDataTest, TruncatedIndicationsAreIgnored) {
    for (uint8_t command : {cmd::zdo::END_DEVICE_ANNCE_IND, cmd::zdo::LEAVE_IND,
                            cmd::zdo::ACTIVE_EP_RSP, cmd::zdo::SIMPLE_DESC_RSP}) {
        fake.injectIndication(ZnpFrame(ZnpType::AREQ, ZnpSubsystem::ZDO, command, {0x01, 0x02}));
    }
    fake.injectIndication(ZnpFrame(ZnpType::AREQ, ZnpSubsystem::AF, cmd::af::INCOMING_MSG, {0x01}));

    expectStillWorking();
}

TEST_F(ZigbeeBadDataTest, IncomingMessageLengthLongerThanPayloadIsIgnored) {
    auto frame = FakeZnpCoordinator::incomingMessage(PLUG.nwkAddr, 3, zcl::cluster::ON_OFF,
                                                     {0x18, 0x01, 0x0A});
    auto payload = frame.payload();
    payload[16] = 200;  // Claims far more ZCL data than the frame holds
    frame.setPayload(payload);
    fake.injectIndication(frame);

    expectStillWorking();
}

TEST_F(ZigbeeBadDataTest, TruncatedAttributeReportIsIgnored) {
    ASSERT_NE(pair(PLUG), nullptr);

    // Attribute header promises an INT16, but no value follows.
    fake.injectIndication(FakeZnpCoordinator::incomingMessage(
        PLUG.nwkAddr, 3, zcl::cluster::TEMPERATURE_MEASUREMENT,
        {0x18, 0x01, zcl::global_cmd::REPORT_ATTRIBUTES, 0x00, 0x00, zcl::datatype::INT16}));
    fake.reportAttribute(PLUG.nwkAddr, 3, zcl::cluster::ON_OFF,
                         zcl::attr::onoff::ON_OFF, zcl::datatype::BOOLEAN, {0x01});

    EXPECT_EQ(waitForState(idOf(PLUG), "on"), true);
    EXPECT_FALSE(stateOf(idOf(PLUG), "temperature").has_value());
}

// ---- Restart ---------------------------------------------------------------

class ZigbeeRestartTest : public ZigbeeHandlerFlowTest {
protected:
    void SetUp() override {
        ZigbeeHandlerFlowTest::SetUp();
        ASSERT_NE(pair(PLUG), nullptr);

        // The app restarts. The radio keeps its network, so devices stay joined
        // and never re-announce.
        handler.reset();
        for (const auto& device : {LIGHT, PLUG, CLIMATE, MOTION}) {
            restartedFake.addDevice(device);
        }
        handler = makeHandler(restartedFake);
        ASSERT_TRUE(handler->initialize());
    }

    void TearDown() override {
        // The handler holds a port into restartedFake, which is destroyed first.
        handler.reset();
    }

    FakeZnpCoordinator restartedFake;
};

TEST_F(ZigbeeRestartTest, NetworkComesBackUp) {
    EXPECT_TRUE(handler->isConnected());
}

// Issue #7: mappings aren't rebuilt after a restart, so reports are dropped
// until the device re-announces.
TEST_F(ZigbeeRestartTest, DISABLED_ReportFromExistingDeviceStillReachesState) {
    restartedFake.reportAttribute(PLUG.nwkAddr, 3, zcl::cluster::ON_OFF,
                                  zcl::attr::onoff::ON_OFF, zcl::datatype::BOOLEAN, {0x01});

    EXPECT_EQ(waitForState(idOf(PLUG), "on"), true);
}

// Issue #7: a restored device must be controllable before it next reports. The
// fix decides how known devices reach the handler, so this may need to seed them.
TEST_F(ZigbeeRestartTest, DISABLED_CommandWorksBeforeDeviceReports) {
    ASSERT_TRUE(handler->sendCommand(idOf(PLUG), "on", {{"on", true}}));

    auto frames = restartedFake.sentFrames(ZnpSubsystem::AF, cmd::af::DATA_REQUEST);
    ASSERT_EQ(frames.size(), 1u);
    AirFrame frame(frames[0]);
    EXPECT_EQ(frame.nwkAddr, PLUG.nwkAddr);
    EXPECT_EQ(frame.endpoint, 3);
}
