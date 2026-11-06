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
#include <smarthub/devices/types/ContactSensor.hpp>
#include <smarthub/devices/types/SwitchDevice.hpp>
#include <smarthub/protocols/zigbee/ZigbeeCoordinator.hpp>
#include <smarthub/protocols/zigbee/ZigbeeHandler.hpp>

#include <atomic>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstdio>
#include <memory>
#include <mutex>
#include <optional>
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

// Endpoint 1 only has Basic; the light is on endpoint 11.
const FakeZigbeeDevice MULTI_ENDPOINT_LIGHT{
    0x5555, 0x00124B0000005555ULL,
    {{1, {zcl::cluster::BASIC}},
     {11, {zcl::cluster::ON_OFF, zcl::cluster::LEVEL_CONTROL}}}};

// Temperature comes first, but the device is a dimmer on endpoint 2.
const FakeZigbeeDevice LIGHT_WITH_SENSOR{
    0x6666, 0x00124B0000006666ULL,
    {{1, {zcl::cluster::TEMPERATURE_MEASUREMENT}},
     {2, {zcl::cluster::ON_OFF, zcl::cluster::LEVEL_CONTROL}}}};

// Routers commonly list the Green Power endpoint.
const FakeZigbeeDevice ROUTER_WITH_GREEN_POWER{
    0x7777, 0x00124B0000007777ULL,
    {{1, {zcl::cluster::ON_OFF}},
     {GREEN_POWER_ENDPOINT, {}}}};

// Nothing we can control or read, so an unknown device type.
const FakeZigbeeDevice BASIC_ONLY{
    0x8888, 0x00124B0000008888ULL,
    {{1, {zcl::cluster::BASIC}}}};

const FakeZigbeeDevice COLOR_LIGHT{
    0x9999, 0x00124B0000009999ULL,
    {{1, {zcl::cluster::ON_OFF, zcl::cluster::LEVEL_CONTROL, zcl::cluster::COLOR_CONTROL}}}};

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
        for (const auto& device : {LIGHT, PLUG, CLIMATE, MOTION, MULTI_ENDPOINT_LIGHT,
                                   LIGHT_WITH_SENSOR, ROUTER_WITH_GREEN_POWER, BASIC_ONLY,
                                   COLOR_LIGHT}) {
            fake.addDevice(device);
        }
        handler = makeHandler(fake);
        ASSERT_TRUE(handler->initialize());
    }

    std::unique_ptr<ZigbeeHandler> makeHandler(FakeZnpCoordinator& radio) {
        auto created = std::make_unique<ZigbeeHandler>(
            eventBus, handlerConfig, [&radio] { return coordinatorFor(radio); });

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

    nlohmann::json handlerConfig = nlohmann::json::object();

    /** Watches for pairing failures. */
    void watchForPairingErrors() {
        eventBus.subscribe("device.state", [this](const Event& event) {
            if (static_cast<const DeviceStateEvent&>(event).property == "_pairing_error") {
                pairingFailed = true;
            }
        });
    }

    std::atomic<bool> pairingFailed{false};

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

TEST_F(ZigbeeHandlerFlowTest, PairingWaitsForEveryEndpoint) {
    fake.holdDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 11);
    fake.announceDevice(MULTI_ENDPOINT_LIGHT);

    // Endpoint 1 alone would make this an unknown device. It must keep waiting.
    ASSERT_TRUE(fake.waitForSent(ZnpSubsystem::ZDO, cmd::zdo::SIMPLE_DESC_REQ, 2));
    std::this_thread::sleep_for(500ms);
    EXPECT_EQ(discoveredCount(), 0u);

    fake.releaseDescriptors();

    ASSERT_TRUE(eventually([&] { return discoveredCount() == 1; }, 10s));
    std::lock_guard<std::mutex> lock(mutex);
    EXPECT_EQ(discovered[0]->type(), DeviceType::Dimmer);
}

TEST_F(ZigbeeHandlerFlowTest, CommandsGoToTheEndpointWithTheCluster) {
    ASSERT_NE(pair(MULTI_ENDPOINT_LIGHT), nullptr);
    fake.clearSentFrames();

    ASSERT_TRUE(handler->sendCommand(idOf(MULTI_ENDPOINT_LIGHT), "on", {{"on", true}}));

    auto frames = airFrames(fake);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].endpoint, 11);
}

TEST_F(ZigbeeHandlerFlowTest, EndpointIsChosenByDeviceType) {
    auto device = pair(LIGHT_WITH_SENSOR);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->type(), DeviceType::Dimmer);

    // The dimmer's reporting and commands go to endpoint 2, not the sensor's.
    for (const auto& frame : airFrames(fake)) {
        if (frame.zclCommand == zcl::global_cmd::CONFIGURE_REPORTING) {
            EXPECT_EQ(frame.endpoint, 2);
        }
    }
    fake.clearSentFrames();
    ASSERT_TRUE(handler->sendCommand(idOf(LIGHT_WITH_SENSOR), "on", {{"on", true}}));
    auto frames = airFrames(fake);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].endpoint, 2);
}

TEST_F(ZigbeeHandlerFlowTest, GreenPowerEndpointIsNotDescribedOrWaitedFor) {
    auto started = std::chrono::steady_clock::now();
    ASSERT_NE(pair(ROUTER_WITH_GREEN_POWER), nullptr);

    EXPECT_LT(std::chrono::steady_clock::now() - started, 3s);
    for (const auto& frame : fake.sentFrames(ZnpSubsystem::ZDO, cmd::zdo::SIMPLE_DESC_REQ)) {
        EXPECT_NE(frame.getByte(4), GREEN_POWER_ENDPOINT);
    }
}

TEST_F(ZigbeeHandlerFlowTest, RefusedDescriptorEndsTheWait) {
    fake.failDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 1);

    auto started = std::chrono::steady_clock::now();
    auto device = pair(MULTI_ENDPOINT_LIGHT);

    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->type(), DeviceType::Dimmer);
    EXPECT_LT(std::chrono::steady_clock::now() - started, 3s);  // Well inside the settle time
}

TEST_F(ZigbeeHandlerFlowTest, RepeatAnnounceDuringPairingCreatesOneDevice) {
    fake.holdDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 11);
    fake.announceDevice(MULTI_ENDPOINT_LIGHT);
    ASSERT_TRUE(fake.waitForSent(ZnpSubsystem::ZDO, cmd::zdo::SIMPLE_DESC_REQ, 2));

    fake.announceDevice(MULTI_ENDPOINT_LIGHT);
    std::this_thread::sleep_for(200ms);
    fake.releaseDescriptors();

    ASSERT_TRUE(eventually([&] { return discoveredCount() >= 1; }, 10s));
    std::this_thread::sleep_for(300ms);
    EXPECT_EQ(discoveredCount(), 1u);
}

TEST_F(ZigbeeHandlerFlowTest, ShutdownStopsPairingPromptly) {
    fake.holdDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 1);
    fake.announceDevice(MULTI_ENDPOINT_LIGHT);
    ASSERT_TRUE(fake.waitForSent(ZnpSubsystem::ZDO, cmd::zdo::SIMPLE_DESC_REQ, 1));

    auto started = std::chrono::steady_clock::now();
    handler.reset();

    EXPECT_LT(std::chrono::steady_clock::now() - started, 2s);
    EXPECT_EQ(discoveredCount(), 0u);
}

TEST_F(ZigbeeHandlerFlowTest, ShutdownStopsBetweenReportingRequests) {
    // Requests time out from here on, as with a radio that's stopped answering.
    fake.setResponder(ZnpSubsystem::AF, cmd::af::DATA_REQUEST,
                      [](const ZnpFrame&) { return std::nullopt; });
    fake.announceDevice(COLOR_LIGHT);
    ASSERT_TRUE(fake.waitForSent(ZnpSubsystem::AF, cmd::af::DATA_REQUEST, 1, 10s));

    handler.reset();

    // Only the request in progress when shutdown started, of three
    EXPECT_EQ(fake.sentFrames(ZnpSubsystem::AF, cmd::af::DATA_REQUEST).size(), 1u);
    EXPECT_EQ(discoveredCount(), 0u);
}

TEST_F(ZigbeeHandlerFlowTest, FailedStartupWithADeviceJoiningDoesNotCrash) {
    FakeZnpCoordinator radio;
    radio.addDevice(PLUG);
    radio.setResponder(ZnpSubsystem::ZDO, cmd::zdo::STARTUP_FROM_APP, [&](const ZnpFrame&) {
        radio.announceDevice(PLUG);
        std::this_thread::sleep_for(200ms);  // Let the announce start pairing
        return std::vector<uint8_t>{2};      // Then startup fails
    });

    auto failing = makeHandler(radio);
    EXPECT_FALSE(failing->initialize());
    failing.reset();

    EXPECT_EQ(discoveredCount(), 0u);
}

TEST_F(ZigbeeHandlerFlowTest, RejoiningAtANewAddressRestartsPairing) {
    FakeZigbeeDevice moved = MULTI_ENDPOINT_LIGHT;
    moved.nwkAddr = 0xBEEF;
    fake.addDevice(moved);
    fake.holdDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 1);

    fake.announceDevice(MULTI_ENDPOINT_LIGHT);
    ASSERT_TRUE(fake.waitForSent(ZnpSubsystem::ZDO, cmd::zdo::SIMPLE_DESC_REQ, 1));
    fake.announceDevice(moved);

    ASSERT_TRUE(eventually([&] { return discoveredCount() == 1; }, 10s));
    fake.clearSentFrames();
    ASSERT_TRUE(handler->sendCommand(idOf(moved), "on", {{"on", true}}));
    auto frames = airFrames(fake);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].nwkAddr, 0xBEEF);
}

TEST_F(ZigbeeHandlerFlowTest, RejoiningKeepsAnotherDevicesClaimOnTheOldAddress) {
    FakeZigbeeDevice first{0x1111, 0x00124B000000AAAAULL, {{1, {zcl::cluster::ON_OFF}}}};
    FakeZigbeeDevice second{0x1111, 0x00124B000000BBBBULL, {{1, {zcl::cluster::ON_OFF}}}};
    FakeZigbeeDevice firstMoved = first;
    firstMoved.nwkAddr = 0x2222;
    fake.addDevice(first);
    fake.addDevice(firstMoved);

    fake.announceDevice(first);
    fake.announceDevice(second);
    fake.announceDevice(firstMoved);

    auto* coordinator = handler->coordinator();
    ASSERT_TRUE(eventually([&] {
        auto moved = coordinator->getDeviceByNwkAddr(0x2222);
        return moved && moved->ieeeAddress == first.ieeeAddr;
    }));
    auto holder = coordinator->getDeviceByNwkAddr(0x1111);
    ASSERT_TRUE(holder.has_value());
    EXPECT_EQ(holder->ieeeAddress, second.ieeeAddr);
}

TEST_F(ZigbeeHandlerFlowTest, LeavingKeepsAnotherDevicesClaimOnTheAddress) {
    FakeZigbeeDevice first{0x1111, 0x00124B000000AAAAULL, {{1, {zcl::cluster::ON_OFF}}}};
    FakeZigbeeDevice second{0x1111, 0x00124B000000BBBBULL, {{1, {zcl::cluster::ON_OFF}}}};
    fake.addDevice(first);

    auto* coordinator = handler->coordinator();
    fake.announceDevice(first);
    fake.announceDevice(second);
    ASSERT_TRUE(eventually([&] { return coordinator->getDevice(second.ieeeAddr).has_value(); }));

    fake.deviceLeft(first);

    ASSERT_TRUE(eventually([&] { return !coordinator->getDevice(first.ieeeAddr).has_value(); }));
    auto holder = coordinator->getDeviceByNwkAddr(0x1111);
    ASSERT_TRUE(holder.has_value());
    EXPECT_EQ(holder->ieeeAddress, second.ieeeAddr);
}

TEST_F(ZigbeeHandlerFlowTest, LeavingDuringPairingAbandonsIt) {
    watchForPairingErrors();
    handler->startDiscovery();
    fake.holdDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 1);
    fake.announceDevice(MULTI_ENDPOINT_LIGHT);
    ASSERT_TRUE(fake.waitForSent(ZnpSubsystem::ZDO, cmd::zdo::SIMPLE_DESC_REQ, 1));

    fake.deviceLeft(MULTI_ENDPOINT_LIGHT);
    std::this_thread::sleep_for(500ms);

    // It stopped: no more questions for a device that's gone, and no failure to report.
    EXPECT_EQ(fake.sentFrames(ZnpSubsystem::ZDO, cmd::zdo::SIMPLE_DESC_REQ).size(), 1u);
    EXPECT_EQ(pendingCount(), 0u);
    EXPECT_FALSE(pairingFailed);
}

TEST_F(ZigbeeHandlerFlowTest, RejoiningAfterLeavingDuringPairingPairsIt) {
    fake.holdDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 1);
    fake.announceDevice(MULTI_ENDPOINT_LIGHT);
    ASSERT_TRUE(fake.waitForSent(ZnpSubsystem::ZDO, cmd::zdo::SIMPLE_DESC_REQ, 1));
    fake.deviceLeft(MULTI_ENDPOINT_LIGHT);
    fake.releaseDescriptors();

    fake.announceDevice(MULTI_ENDPOINT_LIGHT);

    EXPECT_TRUE(eventually([&] { return discoveredCount() == 1; }, 10s));
}

TEST_F(ZigbeeHandlerFlowTest, ReannouncingAfterRefusalsStillEndsPromptly) {
    watchForPairingErrors();
    handler->startDiscovery();
    fake.failDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 1);
    fake.failDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 11);

    auto started = std::chrono::steady_clock::now();
    fake.announceDevice(MULTI_ENDPOINT_LIGHT);
    fake.announceDevice(MULTI_ENDPOINT_LIGHT);

    ASSERT_TRUE(eventually([&] { return pairingFailed.load(); }));
    EXPECT_LT(std::chrono::steady_clock::now() - started, 3s);
}

// ---- After a restart ---------------------------------------------------------

/** A device as the hub loads it from the database, having paired before. */
static std::shared_ptr<SwitchDevice> savedDevice(const FakeZigbeeDevice& device, int endpoint = 0) {
    auto saved = std::make_shared<SwitchDevice>(idOf(device), "Saved", "zigbee");
    saved->setState("ieeeAddress", idOf(device));
    saved->setState("networkAddress", static_cast<int>(device.nwkAddr));
    if (endpoint != 0) {
        saved->setState("endpoint", endpoint);
    }
    return saved;
}

TEST_F(ZigbeeHandlerFlowTest, PairingRecordsWhatARestartNeeds) {
    auto device = pair(MULTI_ENDPOINT_LIGHT);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->getProperty("endpoint"), 11);
    EXPECT_EQ(device->getProperty("networkAddress"), MULTI_ENDPOINT_LIGHT.nwkAddr);
}

TEST_F(ZigbeeHandlerFlowTest, RestoredDeviceCanBeCommandedBeforeItReports) {
    handler->restoreDevice(*savedDevice(MULTI_ENDPOINT_LIGHT, 11));
    fake.clearSentFrames();

    ASSERT_TRUE(handler->sendCommand(idOf(MULTI_ENDPOINT_LIGHT), "on", {{"on", true}}));

    auto frames = airFrames(fake);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].endpoint, 11);
    EXPECT_EQ(frames[0].nwkAddr, MULTI_ENDPOINT_LIGHT.nwkAddr);
}

TEST_F(ZigbeeHandlerFlowTest, RestoredDeviceSavedByAnOlderBuildUsesTheDefaultEndpoint) {
    handler->restoreDevice(*savedDevice(LIGHT));
    fake.clearSentFrames();

    ASSERT_TRUE(handler->sendCommand(idOf(LIGHT), "on", {{"on", true}}));

    auto frames = airFrames(fake);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].endpoint, 1);
}

TEST_F(ZigbeeHandlerFlowTest, ReportFromARestoredDeviceReachesIt) {
    handler->restoreDevice(*savedDevice(LIGHT, 1));

    fake.reportAttribute(LIGHT.nwkAddr, 1, zcl::cluster::ON_OFF, zcl::attr::onoff::ON_OFF, zcl::datatype::BOOLEAN, {0x01});

    EXPECT_EQ(waitForState(idOf(LIGHT), "on"), true);
}

TEST_F(ZigbeeHandlerFlowTest, RestoringDoesNotClaimTheDeviceIsAvailable) {
    handler->restoreDevice(*savedDevice(LIGHT, 1));
    std::lock_guard<std::mutex> lock(mutex);
    EXPECT_TRUE(availabilityChanges.empty());
}

TEST_F(ZigbeeHandlerFlowTest, DeviceAddedByHandFindsItsNetworkAddress) {
    SwitchDevice typed("my-lamp", "Lamp", "zigbee", "00:12:4B:00:00:00:12:34");
    ASSERT_EQ(LIGHT.ieeeAddr, 0x00124B0000001234ULL) << "this test types in LIGHT's address";
    handler->restoreDevice(typed);

    // Pairing follows, since it has never been asked about its endpoints
    EXPECT_EQ(waitForState("my-lamp", "endpoint"), 1);
    EXPECT_EQ(waitForState("my-lamp", "networkAddress"), LIGHT.nwkAddr);

    fake.clearSentFrames();
    ASSERT_TRUE(handler->sendCommand("my-lamp", "on", {{"on", true}}));
    auto frames = airFrames(fake);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].nwkAddr, LIGHT.nwkAddr);
}

TEST_F(ZigbeeHandlerFlowTest, CommandAsksAgainWhenTheAddressIsStillUnknown) {
    handlerConfig["addressLookupIntervalMs"] = 100;
    handler->shutdown();
    handler = makeHandler(fake);
    ASSERT_TRUE(handler->initialize());

    FakeZigbeeDevice late{0x7ABC, 0x00124B000000ABCDULL, {{1, {zcl::cluster::ON_OFF}}}};
    SwitchDevice typed("late", "Late", "zigbee", "00124B000000ABCD");
    handler->restoreDevice(typed);  // It isn't on the network yet
    EXPECT_FALSE(handler->sendCommand("late", "on", {{"on", true}}));

    fake.addDevice(late);
    EXPECT_TRUE(eventually([&] { return handler->sendCommand("late", "on", {{"on", true}}); }));
}

TEST_F(ZigbeeHandlerFlowTest, RepeatedCommandsDoNotFloodTheNetworkWithLookups) {
    SwitchDevice typed("absent", "Absent", "zigbee", "00124B000000ABCD");
    fake.clearSentFrames();
    handler->restoreDevice(typed);
    for (int i = 0; i < 5; i++) {
        EXPECT_FALSE(handler->sendCommand("absent", "on", {{"on", true}}));
    }
    ASSERT_TRUE(fake.waitForSent(ZnpSubsystem::ZDO, cmd::zdo::NWK_ADDR_REQ, 1));
    std::this_thread::sleep_for(100ms);  // Long enough for any more to arrive
    EXPECT_EQ(fake.sentFrames(ZnpSubsystem::ZDO, cmd::zdo::NWK_ADDR_REQ).size(), 1u);
}

TEST_F(ZigbeeHandlerFlowTest, SavedAddressIsCheckedAndFollowedIfOutOfDate) {
    auto saved = savedDevice(LIGHT, 1);
    saved->setState("networkAddress", 0x0BAD);  // It moved while the hub was off
    handler->restoreDevice(*saved);

    handler->poll();

    EXPECT_EQ(waitForState(idOf(LIGHT), "networkAddress"), LIGHT.nwkAddr);
    fake.clearSentFrames();
    ASSERT_TRUE(handler->sendCommand(idOf(LIGHT), "on", {{"on", true}}));
    auto frames = airFrames(fake);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].nwkAddr, LIGHT.nwkAddr);
}

TEST_F(ZigbeeHandlerFlowTest, SavedAddressChecksAreSpreadOut) {
    handler->restoreDevice(*savedDevice(LIGHT, 1));
    handler->restoreDevice(*savedDevice(PLUG, 1));
    fake.clearSentFrames();

    handler->poll();
    handler->poll();
    handler->poll();

    ASSERT_TRUE(fake.waitForSent(ZnpSubsystem::ZDO, cmd::zdo::NWK_ADDR_REQ, 1));
    std::this_thread::sleep_for(100ms);  // Long enough for any more to arrive
    EXPECT_EQ(fake.sentFrames(ZnpSubsystem::ZDO, cmd::zdo::NWK_ADDR_REQ).size(), 1u);
}

TEST_F(ZigbeeHandlerFlowTest, SavedDeviceWithAnAddressNowUsedElsewhereGetsNoCommands) {
    // Both saved at PLUG's address, so one of them is out of date
    auto outOfDate = savedDevice(LIGHT, 1);
    outOfDate->setState("networkAddress", static_cast<int>(PLUG.nwkAddr));
    handler->restoreDevice(*savedDevice(PLUG, 1));

    handler->restoreDevice(*outOfDate);

    // It's looked up rather than trusted, and commands go where the network says it is
    EXPECT_EQ(waitForState(idOf(LIGHT), "networkAddress"), LIGHT.nwkAddr);
    fake.clearSentFrames();
    ASSERT_TRUE(handler->sendCommand(idOf(LIGHT), "on", {{"on", true}}));
    auto frames = airFrames(fake);
    ASSERT_GE(frames.size(), 1u);
    EXPECT_EQ(frames.back().nwkAddr, LIGHT.nwkAddr);
}

TEST_F(ZigbeeHandlerFlowTest, DeviceFoundAtASavedDevicesAddressTakesIt) {
    // LIGHT was saved at this address, and has since moved, and PLUG has it now
    auto outOfDate = savedDevice(LIGHT, 1);
    outOfDate->setState("networkAddress", 0x4242);
    handler->restoreDevice(*outOfDate);
    fake.removeDevice(LIGHT.nwkAddr);
    FakeZigbeeDevice plugMoved = PLUG;
    plugMoved.nwkAddr = 0x4242;
    fake.addDevice(plugMoved);

    ASSERT_NE(pair(plugMoved), nullptr);

    // LIGHT's old address is PLUG's now, so a command to LIGHT must not be sent there
    fake.clearSentFrames();
    EXPECT_FALSE(handler->sendCommand(idOf(LIGHT), "on", {{"on", true}}));
    EXPECT_TRUE(airFrames(fake).empty());
}

TEST_F(ZigbeeHandlerFlowTest, DeviceThatLeftIsBackOnlineWhenALookupFindsIt) {
    auto saved = savedDevice(LIGHT, 1);
    handler->restoreDevice(*saved);
    fake.deviceLeft(LIGHT);
    EXPECT_TRUE(eventually([&] {
        std::lock_guard<std::mutex> lock(mutex);
        return !availabilityChanges.empty() && availabilityChanges.back().second == DeviceAvailability::Offline;
    }));

    // Asks the network, which still has it
    handler->sendCommand(idOf(LIGHT), "on", {{"on", true}});

    EXPECT_TRUE(eventually([&] {
        std::lock_guard<std::mutex> lock(mutex);
        return availabilityChanges.back().second == DeviceAvailability::Online;
    }));
}

TEST_F(ZigbeeHandlerFlowTest, TwoDevicesWithOneAddressKeepTheFirst) {
    handler->restoreDevice(*savedDevice(LIGHT, 1));
    SwitchDevice typed("copy", "Copy", "zigbee", "00124B0000001234");

    handler->restoreDevice(typed);

    EXPECT_TRUE(handler->sendCommand(idOf(LIGHT), "on", {{"on", true}}));
    EXPECT_FALSE(handler->sendCommand("copy", "on", {{"on", true}}));

    handler->forgetDevice("copy");
    EXPECT_TRUE(handler->sendCommand(idOf(LIGHT), "on", {{"on", true}})) << "the copy going away changes nothing";
}

TEST_F(ZigbeeHandlerFlowTest, DeviceAnnouncingAsTheNetworkComesUpIsNotNew) {
    handler->shutdown();
    handler = makeHandler(fake);
    auto saved = savedDevice(MULTI_ENDPOINT_LIGHT, 11);

    handler->restoreDevice(*saved);  // Before it starts, which is what the device manager does
    ASSERT_TRUE(handler->initialize());
    fake.announceDevice(MULTI_ENDPOINT_LIGHT);

    EXPECT_EQ(waitForState(idOf(MULTI_ENDPOINT_LIGHT), "endpoint"), 11);
    EXPECT_EQ(discoveredCount(), 0u);
}

TEST_F(ZigbeeHandlerFlowTest, AddressThatIsNotOneIsIgnored) {
    for (const char* bad : {"", "lamp", "00124B", "00124B00000011ZZ", "00124B000000111100"}) {
        SwitchDevice typed("bad", "Bad", "zigbee", bad);
        handler->restoreDevice(typed);
        EXPECT_FALSE(handler->sendCommand("bad", "on", {{"on", true}})) << bad;
    }
}

TEST_F(ZigbeeHandlerFlowTest, RestoredDeviceAnnouncingIsNotANewDevice) {
    handler->restoreDevice(*savedDevice(MULTI_ENDPOINT_LIGHT));

    fake.announceDevice(MULTI_ENDPOINT_LIGHT);

    // It finishes by learning the endpoint
    EXPECT_EQ(waitForState(idOf(MULTI_ENDPOINT_LIGHT), "endpoint"), 11);
    EXPECT_EQ(discoveredCount(), 0u);

    fake.clearSentFrames();
    ASSERT_TRUE(handler->sendCommand(idOf(MULTI_ENDPOINT_LIGHT), "on", {{"on", true}}));
    auto frames = airFrames(fake);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].endpoint, 11);
}

TEST_F(ZigbeeHandlerFlowTest, RestoredDeviceAnnouncingDuringDiscoveryIsNotOfferedAsNew) {
    handler->restoreDevice(*savedDevice(MULTI_ENDPOINT_LIGHT));
    handler->startDiscovery();

    fake.announceDevice(MULTI_ENDPOINT_LIGHT);

    EXPECT_EQ(waitForState(idOf(MULTI_ENDPOINT_LIGHT), "endpoint"), 11);
    EXPECT_EQ(pendingCount(), 0u);
    EXPECT_EQ(handler->getPendingDevice(), nullptr);
}

TEST_F(ZigbeeHandlerFlowTest, RestoredDeviceThatMovedIsFollowed) {
    handler->restoreDevice(*savedDevice(LIGHT, 1));

    FakeZigbeeDevice moved = LIGHT;
    moved.nwkAddr = 0x4242;
    fake.addDevice(moved);
    fake.announceDevice(moved);

    EXPECT_EQ(waitForState(idOf(LIGHT), "networkAddress"), 0x4242);
    fake.clearSentFrames();
    ASSERT_TRUE(handler->sendCommand(idOf(LIGHT), "on", {{"on", true}}));
    auto frames = airFrames(fake);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].nwkAddr, 0x4242);
}

TEST_F(ZigbeeHandlerFlowTest, RestoredDeviceRejoiningAfterLeavingIsStillTheSameDevice) {
    handler->restoreDevice(*savedDevice(LIGHT, 1));
    fake.deviceLeft(LIGHT);
    EXPECT_TRUE(eventually([&] { return !handler->sendCommand(idOf(LIGHT), "on", {{"on", true}}); }));

    fake.announceDevice(LIGHT);

    EXPECT_TRUE(eventually([&] { return handler->sendCommand(idOf(LIGHT), "on", {{"on", true}}); }));
    EXPECT_EQ(waitForState(idOf(LIGHT), "endpoint"), 1);
    EXPECT_EQ(discoveredCount(), 0u);
    std::lock_guard<std::mutex> lock(mutex);
    ASSERT_FALSE(availabilityChanges.empty());
    EXPECT_EQ(availabilityChanges.back().second, DeviceAvailability::Online) << "it left, and now it's back";
}

TEST_F(ZigbeeHandlerFlowTest, RestoredDeviceRejoiningWorksWhileItIsNotAnsweringPairing) {
    handler->restoreDevice(*savedDevice(LIGHT, 1));
    fake.deviceLeft(LIGHT);
    fake.holdDescriptor(LIGHT.nwkAddr, 1);

    fake.announceDevice(LIGHT);

    EXPECT_TRUE(eventually([&] { return handler->sendCommand(idOf(LIGHT), "on", {{"on", true}}); }));
}

TEST_F(ZigbeeHandlerFlowTest, DeviceRemovedAfterLeavingIsNewWhenItRejoins) {
    handler->restoreDevice(*savedDevice(LIGHT, 1));
    fake.deviceLeft(LIGHT);
    EXPECT_TRUE(eventually([&] { return !handler->sendCommand(idOf(LIGHT), "on", {{"on", true}}); }));

    handler->forgetDevice(idOf(LIGHT));

    EXPECT_NE(pair(LIGHT), nullptr);
}

TEST_F(ZigbeeHandlerFlowTest, ForgottenDeviceAnnouncingIsNewAgain) {
    handler->restoreDevice(*savedDevice(LIGHT, 1));

    handler->forgetDevice(idOf(LIGHT));
    EXPECT_FALSE(handler->sendCommand(idOf(LIGHT), "on", {{"on", true}}));

    EXPECT_NE(pair(LIGHT), nullptr);
}

TEST_F(ZigbeeHandlerFlowTest, PairingADeviceWhenTheHubKeepsItDoesNotChangeItsId) {
    SwitchDevice typed("my-lamp", "Lamp", "zigbee", "00124B0000001234");
    handler->restoreDevice(typed);

    fake.announceDevice(LIGHT);

    EXPECT_EQ(waitForState("my-lamp", "endpoint"), 1);  // Pairing has finished
    EXPECT_EQ(discoveredCount(), 0u);
}

TEST_F(ZigbeeHandlerFlowTest, SensorsNoReadingValuesAreNotReported) {
    ASSERT_NE(pair(CLIMATE), nullptr);

    fake.reportAttribute(CLIMATE.nwkAddr, 1, zcl::cluster::TEMPERATURE_MEASUREMENT,
                         zcl::attr::temperature::MEASURED_VALUE, zcl::datatype::INT16, {0x00, 0x80});
    fake.reportAttribute(CLIMATE.nwkAddr, 1, zcl::cluster::RELATIVE_HUMIDITY,
                         zcl::attr::humidity::MEASURED_VALUE, zcl::datatype::UINT16, {0xFF, 0xFF});
    fake.reportAttribute(CLIMATE.nwkAddr, 1, zcl::cluster::TEMPERATURE_MEASUREMENT,
                         zcl::attr::temperature::MEASURED_VALUE, zcl::datatype::INT16, {0x34, 0x08});

    EXPECT_DOUBLE_EQ(waitForState(idOf(CLIMATE), "temperature").get<double>(), 21.0);
    EXPECT_FALSE(stateOf(idOf(CLIMATE), "humidity").has_value());
}

// ---- Contact and motion sensors -----------------------------------------------

namespace {

const FakeZigbeeDevice DOOR{
    0x8101, 0x00124B0000008101ULL,
    {{1, {zcl::cluster::BASIC, zcl::cluster::IAS_ZONE}}},
    "Acme", "Door Sensor", zcl::zone_type::CONTACT_SWITCH};
const FakeZigbeeDevice PIR{
    0x8102, 0x00124B0000008102ULL,
    {{1, {zcl::cluster::BASIC, zcl::cluster::IAS_ZONE}}},
    "Acme", "Door Sensor Named Badly", zcl::zone_type::MOTION_SENSOR};
const FakeZigbeeDevice SILENT_ZONE{
    0x8103, 0x00124B0000008103ULL,
    {{1, {zcl::cluster::BASIC, zcl::cluster::IAS_ZONE}}},
    "", "", std::nullopt, std::nullopt, true};
const FakeZigbeeDevice NO_ZONE_TYPE{
    0x8105, 0x00124B0000008105ULL,
    {{1, {zcl::cluster::IAS_ZONE}}}};
const FakeZigbeeDevice LEAK{
    0x8106, 0x00124B0000008106ULL,
    {{1, {zcl::cluster::IAS_ZONE}}},
    "", "", zcl::zone_type::WATER_SENSOR};
const FakeZigbeeDevice OPEN_DOOR{
    0x8107, 0x00124B0000008107ULL,
    {{1, {zcl::cluster::IAS_ZONE}}},
    "", "", zcl::zone_type::CONTACT_SWITCH, uint16_t{0x0001}};
const FakeZigbeeDevice CLOSED_DOOR{
    0x8108, 0x00124B0000008108ULL,
    {{1, {zcl::cluster::IAS_ZONE}}},
    "", "", zcl::zone_type::CONTACT_SWITCH, uint16_t{0x0000}};
const FakeZigbeeDevice MOTION_WITH_TEMPERATURE{
    0x8109, 0x00124B0000008109ULL,
    {{1, {zcl::cluster::OCCUPANCY_SENSING, zcl::cluster::TEMPERATURE_MEASUREMENT}}}};
const FakeZigbeeDevice OCCUPANCY{
    0x8104, 0x00124B0000008104ULL,
    {{1, {zcl::cluster::OCCUPANCY_SENSING}}}};

} // namespace

TEST_F(ZigbeeHandlerFlowTest, PairingReadsManufacturerAndModel) {
    fake.addDevice(DOOR);
    auto device = pair(DOOR);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->getProperty("manufacturer"), "Acme");
    EXPECT_EQ(device->getProperty("model"), "Door Sensor");
    EXPECT_EQ(device->name(), "Acme Door Sensor");
}

TEST_F(ZigbeeHandlerFlowTest, ZoneTypeDecidesContactSensor) {
    fake.addDevice(DOOR);
    auto device = pair(DOOR);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->type(), DeviceType::ContactSensor);
}

TEST_F(ZigbeeHandlerFlowTest, ZoneTypeBeatsTheModelName) {
    fake.addDevice(PIR);
    auto device = pair(PIR);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->type(), DeviceType::MotionSensor);
}

TEST_F(ZigbeeHandlerFlowTest, ZoneDeviceThatDoesNotAnswerIsNotAddedAsAGuess) {
    handlerConfig["pairingReadTimeoutMs"] = 100;
    handler->shutdown();
    handler = makeHandler(fake);
    ASSERT_TRUE(handler->initialize());
    fake.addDevice(SILENT_ZONE);
    watchForPairingErrors();
    handler->startDiscovery();

    fake.announceDevice(SILENT_ZONE);

    EXPECT_TRUE(eventually([&] { return pairingFailed.load(); }));
    EXPECT_EQ(pendingCount(), 0u);
    EXPECT_EQ(discoveredCount(), 0u);
}

TEST_F(ZigbeeHandlerFlowTest, ZoneDeviceWithoutAZoneTypeIsAssumedToBeMotion) {
    fake.addDevice(NO_ZONE_TYPE);
    auto device = pair(NO_ZONE_TYPE);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->type(), DeviceType::MotionSensor);
}

TEST_F(ZigbeeHandlerFlowTest, AnnouncingDuringTheReadsStartsThemOver) {
    handlerConfig["pairingReadTimeoutMs"] = 1000;
    handler->shutdown();
    handler = makeHandler(fake);
    ASSERT_TRUE(handler->initialize());
    FakeZigbeeDevice oldAddress = DOOR;
    oldAddress.ignoresReads = true;
    fake.addDevice(oldAddress);
    fake.clearSentFrames();
    fake.announceDevice(oldAddress);
    ASSERT_TRUE(fake.waitForSent(ZnpSubsystem::AF, cmd::af::DATA_REQUEST, 3));  // Write, enroll, read

    FakeZigbeeDevice moved = DOOR;
    moved.nwkAddr = 0x8201;
    fake.addDevice(moved);
    fake.announceDevice(moved);

    EXPECT_TRUE(eventually([&] { return discoveredCount() == 1; }, 10s));
    std::lock_guard<std::mutex> lock(mutex);
    ASSERT_EQ(discovered.size(), 1u);
    EXPECT_EQ(discovered[0]->type(), DeviceType::ContactSensor);
}

TEST_F(ZigbeeHandlerFlowTest, AlarmFromAnotherKindOfZoneIsNotMotion) {
    fake.addDevice(LEAK);
    ASSERT_NE(pair(LEAK), nullptr);

    fake.clusterCommand(LEAK.nwkAddr, 1, zcl::cluster::IAS_ZONE, 0x00, {0x01, 0x00});

    EXPECT_EQ(waitForState(idOf(LEAK), "alarm"), true);
    EXPECT_FALSE(stateOf(idOf(LEAK), "motion").has_value());
}

TEST_F(ZigbeeHandlerFlowTest, AlarmFromAnotherKindOfZoneIsNotMotionAfterARestart) {
    fake.addDevice(LEAK);
    auto paired = pair(LEAK);
    ASSERT_NE(paired, nullptr);
    handler->forgetDevice(paired->id());
    handler->restoreDevice(*paired);

    fake.clusterCommand(LEAK.nwkAddr, 1, zcl::cluster::IAS_ZONE, 0x00, {0x01, 0x00});

    EXPECT_EQ(waitForState(idOf(LEAK), "alarm"), true);
    EXPECT_FALSE(stateOf(idOf(LEAK), "motion").has_value());
}

TEST_F(ZigbeeHandlerFlowTest, PairingKnowsWhetherADoorIsOpenAlready) {
    fake.addDevice(OPEN_DOOR);
    fake.addDevice(CLOSED_DOOR);
    auto open = pair(OPEN_DOOR);
    auto closed = pair(CLOSED_DOOR);
    ASSERT_NE(open, nullptr);
    ASSERT_NE(closed, nullptr);
    EXPECT_EQ(open->getProperty("contact"), false);
    EXPECT_EQ(closed->getProperty("contact"), true);
}

TEST_F(ZigbeeHandlerFlowTest, OccupancySensorThatAlsoMeasuresTemperatureIsAMotionSensor) {
    fake.addDevice(MOTION_WITH_TEMPERATURE);
    auto device = pair(MOTION_WITH_TEMPERATURE);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->type(), DeviceType::MotionSensor);
}

TEST_F(ZigbeeHandlerFlowTest, PairingEnrollsAnIasZone) {
    fake.addDevice(DOOR);
    fake.clearSentFrames();
    ASSERT_NE(pair(DOOR), nullptr);

    bool wroteAddress = false;
    bool enrolled = false;
    for (const AirFrame& frame : airFrames(fake)) {
        if (frame.cluster != zcl::cluster::IAS_ZONE || frame.nwkAddr != DOOR.nwkAddr) continue;
        if ((frame.zclFrameControl & zcl::frame_ctrl::CLUSTER_SPECIFIC) == 0 &&
            frame.zclCommand == zcl::global_cmd::WRITE_ATTRIBUTES) {
            ASSERT_GE(frame.zclPayload.size(), 11u);
            EXPECT_EQ(frame.zclPayload[0], zcl::attr::ias_zone::IAS_CIE_ADDRESS);
            EXPECT_EQ(frame.zclPayload[2], zcl::datatype::IEEE_ADDR);
            uint64_t address = 0;
            for (int i = 0; i < 8; i++) {
                address |= static_cast<uint64_t>(frame.zclPayload[3 + i]) << (8 * i);
            }
            EXPECT_EQ(address, FakeZnpCoordinator::COORDINATOR_IEEE);
            wroteAddress = true;
        }
        if ((frame.zclFrameControl & zcl::frame_ctrl::CLUSTER_SPECIFIC) != 0 && frame.zclCommand == 0x00) {
            EXPECT_EQ(frame.zclPayload[0], 0x00) << "success";
            enrolled = true;
        }
    }
    EXPECT_TRUE(wroteAddress);
    EXPECT_TRUE(enrolled);
}

TEST_F(ZigbeeHandlerFlowTest, EnrollRequestIsAnsweredEvenDuringPairing) {
    fake.addDevice(DOOR);
    fake.holdDescriptor(DOOR.nwkAddr, 1);
    fake.announceDevice(DOOR);
    ASSERT_TRUE(fake.waitForSent(ZnpSubsystem::ZDO, cmd::zdo::SIMPLE_DESC_REQ, 1));
    fake.clearSentFrames();

    fake.clusterCommand(DOOR.nwkAddr, 1, zcl::cluster::IAS_ZONE, 0x01, {0x15, 0x00, 0x00, 0x00});

    ASSERT_TRUE(fake.waitForSent(ZnpSubsystem::AF, cmd::af::DATA_REQUEST, 1));
    auto frames = airFrames(fake);
    ASSERT_GE(frames.size(), 1u);
    EXPECT_EQ(frames[0].cluster, zcl::cluster::IAS_ZONE);
    EXPECT_EQ(frames[0].zclCommand, 0x00);
    fake.releaseDescriptors();
}

TEST_F(ZigbeeHandlerFlowTest, MotionReachesTheMotionProperty) {
    ASSERT_NE(pair(MOTION), nullptr);

    fake.clusterCommand(MOTION.nwkAddr, 1, zcl::cluster::IAS_ZONE, 0x00, {0x01, 0x00});
    EXPECT_EQ(waitForState(idOf(MOTION), "motion"), true);

    fake.clusterCommand(MOTION.nwkAddr, 1, zcl::cluster::IAS_ZONE, 0x00, {0x00, 0x00});
    EXPECT_TRUE(eventually([&] { return stateOf(idOf(MOTION), "motion") == nlohmann::json(false); }));
}

TEST_F(ZigbeeHandlerFlowTest, ContactReachesTheContactProperty) {
    fake.addDevice(DOOR);
    ASSERT_NE(pair(DOOR), nullptr);

    fake.clusterCommand(DOOR.nwkAddr, 1, zcl::cluster::IAS_ZONE, 0x00, {0x01, 0x00});
    EXPECT_EQ(waitForState(idOf(DOOR), "contact"), false) << "alarm means it's open";

    fake.clusterCommand(DOOR.nwkAddr, 1, zcl::cluster::IAS_ZONE, 0x00, {0x00, 0x00});
    EXPECT_TRUE(eventually([&] { return stateOf(idOf(DOOR), "contact") == nlohmann::json(true); }));
}

TEST_F(ZigbeeHandlerFlowTest, ContactSensorReportingOverOnOffReachesTheContactProperty) {
    ContactSensor saved(idOf(DOOR), "Door", "zigbee");
    saved.setState("ieeeAddress", idOf(DOOR));
    saved.setState("networkAddress", static_cast<int>(DOOR.nwkAddr));
    handler->restoreDevice(saved);

    fake.reportAttribute(DOOR.nwkAddr, 1, zcl::cluster::ON_OFF, zcl::attr::onoff::ON_OFF,
                         zcl::datatype::BOOLEAN, {0x01});

    EXPECT_EQ(waitForState(idOf(DOOR), "contact"), false) << "on means open";
    EXPECT_FALSE(stateOf(idOf(DOOR), "on").has_value());
}

TEST_F(ZigbeeHandlerFlowTest, ZoneStatusInAnAttributeReportCountsToo) {
    ASSERT_NE(pair(MOTION), nullptr);
    fake.reportAttribute(MOTION.nwkAddr, 1, zcl::cluster::IAS_ZONE,
                         zcl::attr::ias_zone::ZONE_STATUS, zcl::datatype::BITMAP16, {0x02, 0x00});
    EXPECT_EQ(waitForState(idOf(MOTION), "motion"), true) << "the second alarm bit";
}

TEST_F(ZigbeeHandlerFlowTest, OccupancyReachesTheMotionProperty) {
    fake.addDevice(OCCUPANCY);
    fake.clearSentFrames();
    auto device = pair(OCCUPANCY);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->type(), DeviceType::MotionSensor);

    bool askedToReport = false;
    for (const AirFrame& frame : airFrames(fake)) {
        askedToReport = askedToReport || (frame.cluster == zcl::cluster::OCCUPANCY_SENSING &&
                                          frame.zclCommand == zcl::global_cmd::CONFIGURE_REPORTING);
    }
    EXPECT_TRUE(askedToReport);

    fake.reportAttribute(OCCUPANCY.nwkAddr, 1, zcl::cluster::OCCUPANCY_SENSING,
                         zcl::attr::occupancy::OCCUPANCY, zcl::datatype::BITMAP8, {0x01});
    EXPECT_EQ(waitForState(idOf(OCCUPANCY), "motion"), true);
}

TEST_F(ZigbeeHandlerFlowTest, RestoredContactSensorStillReadsAsContact) {
    ContactSensor saved(idOf(DOOR), "Door", "zigbee");
    saved.setState("ieeeAddress", idOf(DOOR));
    saved.setState("networkAddress", static_cast<int>(DOOR.nwkAddr));
    handler->restoreDevice(saved);

    fake.clusterCommand(DOOR.nwkAddr, 1, zcl::cluster::IAS_ZONE, 0x00, {0x01, 0x00});

    EXPECT_EQ(waitForState(idOf(DOOR), "contact"), false);
}

TEST_F(ZigbeeHandlerFlowTest, ShutdownDoesNotWaitOutARead) {
    handlerConfig["pairingReadTimeoutMs"] = 60000;
    handler->shutdown();
    handler = makeHandler(fake);
    ASSERT_TRUE(handler->initialize());
    fake.addDevice(SILENT_ZONE);
    fake.clearSentFrames();
    fake.announceDevice(SILENT_ZONE);
    ASSERT_TRUE(fake.waitForSent(ZnpSubsystem::AF, cmd::af::DATA_REQUEST, 3));  // Write, enroll, read
    std::this_thread::sleep_for(100ms);

    const auto start = std::chrono::steady_clock::now();
    handler->shutdown();
    EXPECT_LT(std::chrono::steady_clock::now() - start, 5s);
}

// ---- The Add Device wizard ----------------------------------------------------

TEST_F(ZigbeeHandlerFlowTest, DeviceJoiningDuringACancelledWizardIsNotAdded) {
    handler->startDiscovery();
    fake.holdDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 1);
    fake.announceDevice(MULTI_ENDPOINT_LIGHT);
    ASSERT_TRUE(fake.waitForSent(ZnpSubsystem::ZDO, cmd::zdo::SIMPLE_DESC_REQ, 1));

    // Cancelled while pairing waits on the device
    handler->stopDiscovery();
    handler->clearPendingDevice();
    fake.releaseDescriptors();

    std::this_thread::sleep_for(500ms);  // Long enough for pairing to have finished
    EXPECT_FALSE(handler->sendCommand(idOf(MULTI_ENDPOINT_LIGHT), "on", {{"on", true}}));
    EXPECT_EQ(discoveredCount(), 0u);
    EXPECT_EQ(pendingCount(), 0u);
}

TEST_F(ZigbeeHandlerFlowTest, DeviceHeldByACancelledWizardIsNotAddedWhenItAnnounces) {
    handler->startDiscovery();
    fake.announceDevice(LIGHT);
    ASSERT_TRUE(eventually([&] { return pendingCount() == 1; }));

    handler->stopDiscovery();
    handler->clearPendingDevice();
    fake.announceDevice(LIGHT);

    ASSERT_TRUE(fake.waitForSent(ZnpSubsystem::ZDO, cmd::zdo::ACTIVE_EP_REQ, 2));
    std::this_thread::sleep_for(500ms);
    EXPECT_EQ(discoveredCount(), 0u);
}

TEST_F(ZigbeeHandlerFlowTest, DeclinedDeviceIsOfferedInTheNextWizard) {
    handler->startDiscovery();
    fake.announceDevice(LIGHT);
    ASSERT_TRUE(eventually([&] { return pendingCount() == 1; }));
    handler->stopDiscovery();
    handler->clearPendingDevice();

    handler->startDiscovery();
    fake.announceDevice(LIGHT);

    EXPECT_TRUE(eventually([&] { return pendingCount() == 2; }));
    EXPECT_NE(handler->getPendingDevice(), nullptr);
}

TEST_F(ZigbeeHandlerFlowTest, RepeatAnnounceCannotMakeTheWizardsAddFail) {
    handler->startDiscovery();
    fake.announceDevice(LIGHT);
    ASSERT_TRUE(eventually([&] { return pendingCount() == 1; }));
    DevicePtr held = handler->getPendingDevice();

    // The wizard has stopped discovery and is asking for a name
    handler->stopDiscovery();
    FakeZigbeeDevice moved = LIGHT;
    moved.nwkAddr = 0x4242;
    fake.addDevice(moved);
    fake.announceDevice(moved);

    ASSERT_TRUE(eventually([&] { return held->getProperty("networkAddress") == 0x4242; }));
    EXPECT_EQ(discoveredCount(), 0u) << "this is what would make the wizard's add fail";
    EXPECT_EQ(pendingCount(), 1u);
    EXPECT_EQ(handler->getPendingDevice(), held);
}

TEST_F(ZigbeeHandlerFlowTest, DeviceAddedByTheWizardIsNotDeclinedByClearingIt) {
    handler->startDiscovery();
    fake.announceDevice(LIGHT);
    ASSERT_TRUE(eventually([&] { return pendingCount() == 1; }));
    DevicePtr held = handler->getPendingDevice();
    handler->stopDiscovery();

    handler->restoreDevice(*held);  // The device manager does this when it adds it
    handler->clearPendingDevice();

    EXPECT_TRUE(handler->sendCommand(held->id(), "on", {{"on", true}}));
}

TEST_F(ZigbeeHandlerFlowTest, RemovedDeviceThatWasDeclinedBeforeIsNewAgain) {
    handler->startDiscovery();
    fake.announceDevice(LIGHT);
    ASSERT_TRUE(eventually([&] { return pendingCount() == 1; }));
    DevicePtr held = handler->getPendingDevice();
    handler->stopDiscovery();
    handler->clearPendingDevice();  // Declined

    handler->restoreDevice(*held);  // Then added by hand, with the same address
    handler->forgetDevice(held->id());

    EXPECT_NE(pair(LIGHT), nullptr);
}

TEST_F(ZigbeeHandlerFlowTest, WizardStartedAgainHearsAboutTheDeviceItIsStillHolding) {
    handler->startDiscovery();
    fake.announceDevice(LIGHT);
    ASSERT_TRUE(eventually([&] { return pendingCount() == 1; }));
    handler->stopDiscovery();

    handler->startDiscovery();
    fake.announceDevice(LIGHT);

    EXPECT_TRUE(eventually([&] { return pendingCount() == 2; }));
}

TEST_F(ZigbeeHandlerFlowTest, WizardCancelledDuringReportingSetupDoesNotHoldTheDevice) {
    handler->startDiscovery();
    std::atomic<bool> cancelled{false};
    fake.setResponder(ZnpSubsystem::AF, cmd::af::DATA_REQUEST, [&](const ZnpFrame&) {
        // Reporting setup is the last thing before the device would be held
        if (!cancelled.exchange(true)) {
            std::thread([this] { handler->stopDiscovery(); handler->clearPendingDevice(); }).detach();
            std::this_thread::sleep_for(100ms);
        }
        return std::optional<std::vector<uint8_t>>(std::vector<uint8_t>{0});
    });

    fake.announceDevice(LIGHT);

    ASSERT_TRUE(eventually([&] { return cancelled.load(); }));
    std::this_thread::sleep_for(500ms);
    EXPECT_EQ(handler->getPendingDevice(), nullptr);
    EXPECT_EQ(discoveredCount(), 0u);
    EXPECT_FALSE(handler->sendCommand(idOf(LIGHT), "on", {{"on", true}})) << "and it isn't left half added";
}

TEST_F(ZigbeeHandlerFlowTest, SecondDeviceFoundDeclinesTheOneItReplaces) {
    handler->startDiscovery();
    fake.announceDevice(LIGHT);
    ASSERT_TRUE(eventually([&] { return pendingCount() == 1; }));
    fake.announceDevice(PLUG);
    ASSERT_TRUE(eventually([&] { return pendingCount() == 2; }));
    handler->stopDiscovery();
    handler->clearPendingDevice();

    fake.announceDevice(LIGHT);

    ASSERT_TRUE(fake.waitForSent(ZnpSubsystem::ZDO, cmd::zdo::ACTIVE_EP_REQ, 3));
    std::this_thread::sleep_for(500ms);
    EXPECT_EQ(discoveredCount(), 0u);
}

TEST_F(ZigbeeHandlerFlowTest, HeldDeviceThatLeftAndRejoinedIsStillHeld) {
    handler->startDiscovery();
    fake.announceDevice(LIGHT);
    ASSERT_TRUE(eventually([&] { return pendingCount() == 1; }));
    DevicePtr held = handler->getPendingDevice();
    handler->stopDiscovery();

    fake.deviceLeft(LIGHT);
    FakeZigbeeDevice moved = LIGHT;
    moved.nwkAddr = 0x4242;
    fake.addDevice(moved);
    fake.announceDevice(moved);

    ASSERT_TRUE(eventually([&] { return held->getProperty("networkAddress") == 0x4242; }));
    EXPECT_EQ(discoveredCount(), 0u);
    EXPECT_EQ(handler->getPendingDevice(), held);
}

TEST_F(ZigbeeHandlerFlowTest, DeviceWhosePairingFailedInAWizardIsNotAddedLater) {
    handlerConfig["pairingTimeoutMs"] = 100;
    handler->shutdown();
    handler = makeHandler(fake);
    ASSERT_TRUE(handler->initialize());
    watchForPairingErrors();
    handler->startDiscovery();
    const FakeZigbeeDevice late{0x8301, 0x00124B0000008301ULL, {{1, {zcl::cluster::ON_OFF}}}};
    fake.announceDevice(late);  // Not on the fake's network yet, so it doesn't answer
    ASSERT_TRUE(eventually([&] { return pairingFailed.load(); }));
    handler->stopDiscovery();

    fake.addDevice(late);
    fake.announceDevice(late);

    std::this_thread::sleep_for(500ms);
    EXPECT_EQ(discoveredCount(), 0u);
}

TEST_F(ZigbeeHandlerFlowTest, UnknownDeviceGetsNoReporting) {
    auto device = pair(BASIC_ONLY);
    ASSERT_NE(device, nullptr);

    for (const auto& frame : airFrames(fake)) {
        EXPECT_NE(frame.zclCommand, zcl::global_cmd::CONFIGURE_REPORTING);
    }
}

TEST_F(ZigbeeHandlerFlowTest, PairingWorksAfterReinitializing) {
    handler->shutdown();
    ASSERT_TRUE(handler->initialize());

    EXPECT_NE(pair(PLUG), nullptr);
}

TEST_F(ZigbeeHandlerFlowTest, PairingReportingUsesTheEndpointWithTheCluster) {
    ASSERT_NE(pair(MULTI_ENDPOINT_LIGHT), nullptr);

    bool configured = false;
    for (const auto& frame : airFrames(fake)) {
        if (frame.zclCommand == zcl::global_cmd::CONFIGURE_REPORTING) {
            EXPECT_EQ(frame.endpoint, 11);
            configured = true;
        }
    }
    EXPECT_TRUE(configured);
}

TEST_F(ZigbeeHandlerFlowTest, ReinitializingUsesTheFactoryAgain) {
    handler->shutdown();
    EXPECT_FALSE(handler->isConnected());

    ASSERT_TRUE(handler->initialize());

    EXPECT_TRUE(handler->isConnected());
    EXPECT_EQ(fake.sentFrames(ZnpSubsystem::SYS, cmd::sys::PING).size(), 2u);
}

TEST_F(ZigbeeHandlerFlowTest, ResponderCanCallBackIntoTheFake) {
    // Shared, because shutdown sends this request again after the test body returns.
    auto answered = std::make_shared<std::atomic<bool>>(false);
    fake.setResponder(ZnpSubsystem::ZDO, cmd::zdo::MGMT_PERMIT_JOIN_REQ,
                      [this, answered](const ZnpFrame&) {
                          *answered = !fake.sentFrames(ZnpSubsystem::ZDO,
                                                       cmd::zdo::MGMT_PERMIT_JOIN_REQ).empty();
                          return std::vector<uint8_t>{0};
                      });

    handler->startDiscovery();

    EXPECT_TRUE(*answered);
    EXPECT_TRUE(handler->isDiscovering());
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

// ---- Devices that don't finish answering -----------------------------------

class ZigbeePairingTimeoutTest : public ZigbeeHandlerFlowTest {
protected:
    void SetUp() override {
        handlerConfig["pairingEndpointTimeoutMs"] = 300;
        ZigbeeHandlerFlowTest::SetUp();
    }
};

TEST_F(ZigbeePairingTimeoutTest, UsesWhatArrivedWhenAnEndpointNeverAnswers) {
    fake.holdDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 1);

    auto device = pair(MULTI_ENDPOINT_LIGHT);

    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->type(), DeviceType::Dimmer);
}

TEST_F(ZigbeePairingTimeoutTest, ReportsAFailureWhenNoEndpointAnswers) {
    watchForPairingErrors();
    handler->startDiscovery();
    fake.holdDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 1);
    fake.holdDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 11);

    fake.announceDevice(MULTI_ENDPOINT_LIGHT);

    ASSERT_TRUE(eventually([&] { return pairingFailed.load(); }));
    EXPECT_EQ(pendingCount(), 0u);
    EXPECT_EQ(discoveredCount(), 0u);
}

// Long enough that the steps in these tests can't outlast a wait on a slow machine,
// though each test sits through it at most once.
class ZigbeeLateRefusalTest : public ZigbeeHandlerFlowTest {
protected:
    void SetUp() override {
        handlerConfig["pairingEndpointTimeoutMs"] = 2000;
        ZigbeeHandlerFlowTest::SetUp();
    }
};

TEST_F(ZigbeeLateRefusalTest, DoesNotEndTheNextEndpointsWait) {
    fake.failDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 1);
    fake.holdDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 1);
    fake.holdDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 11);
    fake.announceDevice(MULTI_ENDPOINT_LIGHT);

    // Endpoint 1 times out, and pairing moves on to endpoint 11.
    ASSERT_TRUE(fake.waitForSent(ZnpSubsystem::ZDO, cmd::zdo::SIMPLE_DESC_REQ, 2));

    // Endpoint 1's refusal arrives late, then endpoint 11 answers.
    fake.releaseDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 1);
    std::this_thread::sleep_for(300ms);
    EXPECT_EQ(discoveredCount(), 0u);
    fake.releaseDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 11);

    ASSERT_TRUE(eventually([&] { return discoveredCount() == 1; }));
    std::lock_guard<std::mutex> lock(mutex);
    EXPECT_EQ(discovered[0]->type(), DeviceType::Dimmer);
}

TEST_F(ZigbeeLateRefusalTest, RestartDoesNotLetASecondRefusalEndTheNextEndpointsWait) {
    fake.failDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 1);
    fake.holdDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 1);
    fake.holdDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 11);

    // Announcing twice asks about endpoint 1 twice, so it refuses twice.
    fake.announceDevice(MULTI_ENDPOINT_LIGHT);
    ASSERT_TRUE(fake.waitForSent(ZnpSubsystem::ZDO, cmd::zdo::SIMPLE_DESC_REQ, 1));
    fake.announceDevice(MULTI_ENDPOINT_LIGHT);
    ASSERT_TRUE(fake.waitForSent(ZnpSubsystem::ZDO, cmd::zdo::SIMPLE_DESC_REQ, 2));

    // The first refusal arrives. Pairing moves on to endpoint 11 either way...
    fake.releaseNextDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 1);
    ASSERT_TRUE(fake.waitForSent(ZnpSubsystem::ZDO, cmd::zdo::SIMPLE_DESC_REQ, 3));

    // ...and the second refusal, for the earlier request, mustn't end that wait.
    fake.releaseNextDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 1);
    std::this_thread::sleep_for(300ms);
    EXPECT_EQ(discoveredCount(), 0u);
    fake.releaseDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 11);

    ASSERT_TRUE(eventually([&] { return discoveredCount() == 1; }));
    std::lock_guard<std::mutex> lock(mutex);
    EXPECT_EQ(discovered[0]->type(), DeviceType::Dimmer);
}

class ZigbeeEndpointListTimeoutTest : public ZigbeeHandlerFlowTest {
protected:
    void SetUp() override {
        handlerConfig["pairingTimeoutMs"] = 300;
        handlerConfig["pairingEndpointTimeoutMs"] = 3000;
        ZigbeeHandlerFlowTest::SetUp();
    }
};

TEST_F(ZigbeeEndpointListTimeoutTest, ReportsAFailureWhenTheDeviceListsNoEndpoints) {
    watchForPairingErrors();
    handler->startDiscovery();
    FakeZigbeeDevice silent{0x1111, 0x00124B0000001111ULL, {}};  // The fake doesn't know it

    fake.announceDevice(silent);

    EXPECT_TRUE(eventually([&] { return pairingFailed.load(); }));
}

TEST_F(ZigbeeEndpointListTimeoutTest, EachEndpointGetsItsOwnFullWait) {
    // A slow answer well past pairingTimeoutMs still counts.
    fake.holdDescriptor(MULTI_ENDPOINT_LIGHT.nwkAddr, 1);
    fake.announceDevice(MULTI_ENDPOINT_LIGHT);
    ASSERT_TRUE(fake.waitForSent(ZnpSubsystem::ZDO, cmd::zdo::SIMPLE_DESC_REQ, 1));

    std::this_thread::sleep_for(1s);
    fake.releaseDescriptors();

    ASSERT_TRUE(eventually([&] { return discoveredCount() == 1; }, 10s));
    auto info = handler->coordinator()->getDevice(MULTI_ENDPOINT_LIGHT.ieeeAddr);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->inClusters.count(1), 1u);
    EXPECT_EQ(info->inClusters.count(11), 1u);
}

class ZigbeeBadSettingTest : public ZigbeeHandlerFlowTest,
                             public ::testing::WithParamInterface<nlohmann::json> {
protected:
    void SetUp() override {
        handlerConfig["pairingTimeoutMs"] = GetParam();
        handlerConfig["pairingEndpointTimeoutMs"] = GetParam();
        ZigbeeHandlerFlowTest::SetUp();
    }
};

TEST_P(ZigbeeBadSettingTest, FallsBackToDefaults) {
    EXPECT_NE(pair(PLUG), nullptr);
}

INSTANTIATE_TEST_SUITE_P(InvalidValues, ZigbeeBadSettingTest,
                         ::testing::Values(nlohmann::json("soon"), nlohmann::json(-1),
                                           nlohmann::json(50), nlohmann::json(INT_MAX)));

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
    ASSERT_TRUE(handler->sendCommand(idOf(LIGHT), "brightness", {{"brightness", 50}}));

    auto frames = airFrames(fake);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].cluster, zcl::cluster::LEVEL_CONTROL);
    EXPECT_EQ(frames[0].zclCommand, zcl::cmd::level::MOVE_TO_LEVEL_ONOFF);
    ASSERT_FALSE(frames[0].zclPayload.empty());
    EXPECT_EQ(frames[0].zclPayload[0], 127);  // 50% of ZCL's 254
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

// A command is in the middle of writing to the radio when shutdown starts.
TEST_F(ZigbeeCommandTest, CommandInProgressSurvivesShutdown) {
    std::atomic<bool> writing{false};
    fake.setResponder(ZnpSubsystem::AF, cmd::af::DATA_REQUEST, [&](const ZnpFrame&) {
        writing = true;
        std::this_thread::sleep_for(300ms);
        return std::vector<uint8_t>{0};
    });

    std::thread command([&] { handler->sendCommand(idOf(LIGHT), "on", {{"on", true}}); });
    ASSERT_TRUE(eventually([&] { return writing.load(); }));

    handler->shutdown();
    command.join();

    EXPECT_FALSE(handler->isConnected());
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
        DevicePtr paired = pair(PLUG);
        ASSERT_NE(paired, nullptr);

        // The app restarts. The radio keeps its network, so devices stay joined
        // and never re-announce.
        handler.reset();
        for (const auto& device : {LIGHT, PLUG, CLIMATE, MOTION}) {
            restartedFake.addDevice(device);
        }
        handler = makeHandler(restartedFake);
        ASSERT_TRUE(handler->initialize());

        // What the device manager does with the devices it loads, here with what pairing recorded
        handler->restoreDevice(*paired);
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

TEST_F(ZigbeeRestartTest, ReportFromExistingDeviceStillReachesState) {
    restartedFake.reportAttribute(PLUG.nwkAddr, 3, zcl::cluster::ON_OFF,
                                  zcl::attr::onoff::ON_OFF, zcl::datatype::BOOLEAN, {0x01});

    EXPECT_EQ(waitForState(idOf(PLUG), "on"), true);
}

TEST_F(ZigbeeRestartTest, CommandWorksBeforeDeviceReports) {
    ASSERT_TRUE(handler->sendCommand(idOf(PLUG), "on", {{"on", true}}));

    auto frames = restartedFake.sentFrames(ZnpSubsystem::AF, cmd::af::DATA_REQUEST);
    ASSERT_EQ(frames.size(), 1u);
    AirFrame frame(frames[0]);
    EXPECT_EQ(frame.nwkAddr, PLUG.nwkAddr);
    EXPECT_EQ(frame.endpoint, 3);
}
