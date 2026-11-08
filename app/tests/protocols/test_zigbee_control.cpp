/**
 * Controlling Zigbee devices through DeviceManager, the path the UI, web API and
 * automations all use, against a fake coordinator.
 */

#include <gtest/gtest.h>

#include "../mocks/FakeZnpCoordinator.hpp"

#include <smarthub/automation/AutomationManager.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/database/Database.hpp>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/devices/types/SwitchDevice.hpp>
#include <smarthub/protocols/ProtocolFactory.hpp>
#include <smarthub/protocols/zigbee/ZigbeeCoordinator.hpp>
#include <smarthub/protocols/zigbee/ZigbeeHandler.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace smarthub;
using namespace smarthub::zigbee;
using namespace std::chrono_literals;

namespace {

const FakeZigbeeDevice LIGHT{
    0x1234, 0x00124B0000001234ULL,
    {{3, {zcl::cluster::ON_OFF, zcl::cluster::LEVEL_CONTROL, zcl::cluster::COLOR_CONTROL}}}};

const FakeZigbeeDevice PLUG{
    0x2222, 0x00124B0000002222ULL,
    {{1, {zcl::cluster::ON_OFF}}}};

const FakeZigbeeDevice MOTION{
    0x4444, 0x00124B0000004444ULL,
    {{1, {zcl::cluster::IAS_ZONE}}}};

constexpr const char* LIGHT_ID = "zigbee_00124b0000001234";
constexpr const char* PLUG_ID = "zigbee_00124b0000002222";
constexpr const char* MOTION_ID = "zigbee_00124b0000004444";

template <typename Predicate>
bool eventually(Predicate predicate, std::chrono::milliseconds timeout = 10s) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        std::this_thread::sleep_for(5ms);
    }
    return predicate();
}

} // namespace

class ZigbeeControlTest : public ::testing::Test {
protected:
    void SetUp() override {
        dbPath = "/tmp/zigbee_control_test_" + std::to_string(getpid()) + ".db";
        std::filesystem::remove(dbPath);
        database = std::make_unique<Database>(dbPath);
        database->initialize();

        fake.addDevice(LIGHT);
        fake.addDevice(PLUG);
        fake.addDevice(MOTION);

        // Same name as the real handler, so devices find it by their protocol
        ProtocolFactory::instance().registerProtocol("zigbee",
            [this](EventBus& bus, const nlohmann::json& config) -> ProtocolHandlerPtr {
                return std::make_shared<ZigbeeHandler>(bus, config, [this] {
                    return std::make_unique<ZigbeeCoordinator>(
                        std::make_unique<ZnpTransport>(fake.createSerialPort(), "fake"));
                });
            });

        manager = std::make_unique<DeviceManager>(eventBus, *database);
        manager->initialize();
    }

    void TearDown() override {
        manager.reset();
        database.reset();
        std::filesystem::remove(dbPath);

        // Put the real handler back for any tests that follow in this process
        ProtocolFactory::instance().registerProtocol("zigbee",
            [](EventBus& bus, const nlohmann::json& config) -> ProtocolHandlerPtr {
                return std::make_shared<ZigbeeHandler>(bus, config);
            },
            { "zigbee", "1.0.0", "Zigbee protocol via CC2652P coordinator" });
    }

    // As the dashboard's Add Device wizard does it, the only way to add a device (#63)
    void addThroughWizard(const FakeZigbeeDevice& device) {
        auto* handler = dynamic_cast<ZigbeeHandler*>(manager->getProtocol("zigbee"));
        ASSERT_NE(handler, nullptr);
        handler->startDiscovery();
        fake.announceDevice(device);
        DevicePtr pending;
        ASSERT_TRUE(eventually([&] {
            pending = handler->getPendingDevice();
            return pending != nullptr;
        }));
        handler->stopDiscovery();
        ASSERT_TRUE(manager->addDevice(pending));
        handler->clearPendingDevice();
    }

    void loadAndPair() {
        ASSERT_TRUE(manager->loadProtocol("zigbee", nlohmann::json::object()));
        addThroughWizard(LIGHT);
        addThroughWizard(PLUG);
        ASSERT_EQ(manager->deviceCount(), 2u);
        fake.clearSentFrames();
    }

    struct AirFrame {
        uint16_t nwkAddr;
        uint8_t endpoint;
        uint16_t cluster;
        uint8_t zclCommand;
        std::vector<uint8_t> payload;
    };

    std::vector<AirFrame> airFrames() const {
        std::vector<AirFrame> out;
        for (const auto& frame : fake.sentFrames(ZnpSubsystem::AF, cmd::af::DATA_REQUEST)) {
            out.push_back({frame.getWord(0), frame.getByte(2), frame.getWord(4), frame.getByte(12),
                           frame.getBytes(13, frame.getByte(9) - 3)});
        }
        return out;
    }

    nlohmann::json state(const std::string& id, const std::string& property) {
        return manager->getDevice(id)->getProperty(property);
    }

    std::string dbPath;
    FakeZnpCoordinator fake;
    EventBus eventBus;
    std::unique_ptr<Database> database;
    std::unique_ptr<DeviceManager> manager;
};

TEST_F(ZigbeeControlTest, OnReachesTheRadioAndUpdatesState) {
    loadAndPair();

    ASSERT_TRUE(manager->setDeviceState(LIGHT_ID, "on", true));

    auto frames = airFrames();
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].nwkAddr, LIGHT.nwkAddr);
    EXPECT_EQ(frames[0].endpoint, 3);
    EXPECT_EQ(frames[0].cluster, zcl::cluster::ON_OFF);
    EXPECT_EQ(frames[0].zclCommand, zcl::cmd::onoff::ON);
    EXPECT_EQ(state(LIGHT_ID, "on"), true);
}

TEST_F(ZigbeeControlTest, OffSendsOff) {
    loadAndPair();

    ASSERT_TRUE(manager->setDeviceState(LIGHT_ID, "on", false));

    auto frames = airFrames();
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].zclCommand, zcl::cmd::onoff::OFF);
}

TEST_F(ZigbeeControlTest, BrightnessPercentBecomesAZclLevel) {
    loadAndPair();

    ASSERT_TRUE(manager->setDeviceState(LIGHT_ID, "brightness", 50));

    auto frames = airFrames();
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].cluster, zcl::cluster::LEVEL_CONTROL);
    ASSERT_FALSE(frames[0].payload.empty());
    EXPECT_EQ(frames[0].payload[0], 127);
    EXPECT_EQ(state(LIGHT_ID, "brightness"), 50);
}

TEST_F(ZigbeeControlTest, BrightnessOutOfRangeIsClamped) {
    loadAndPair();

    ASSERT_TRUE(manager->setDeviceState(LIGHT_ID, "brightness", 500));

    auto frames = airFrames();
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].payload[0], 254);
}

TEST_F(ZigbeeControlTest, ColorTemperatureInKelvinBecomesMireds) {
    loadAndPair();

    ASSERT_TRUE(manager->setDeviceState(LIGHT_ID, "colorTemp", 4000));

    auto frames = airFrames();
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].cluster, zcl::cluster::COLOR_CONTROL);
    ASSERT_GE(frames[0].payload.size(), 2u);
    EXPECT_EQ(frames[0].payload[0] | (frames[0].payload[1] << 8), 250);
    EXPECT_EQ(state(LIGHT_ID, "colorTemp"), 4000);
}

TEST_F(ZigbeeControlTest, BrightnessAlsoSetsWhetherTheLightIsOn) {
    loadAndPair();

    ASSERT_TRUE(manager->setDeviceState(LIGHT_ID, "brightness", 50));
    EXPECT_EQ(state(LIGHT_ID, "on"), true);

    ASSERT_TRUE(manager->setDeviceState(LIGHT_ID, "brightness", 0));
    EXPECT_EQ(state(LIGHT_ID, "on"), false);

    EXPECT_EQ(airFrames().size(), 2u);  // Level commands already switch it; no extra frames
}

TEST_F(ZigbeeControlTest, DevicesCantBeSentCommandsTheyDontSupport) {
    loadAndPair();
    addThroughWizard(MOTION);
    ASSERT_EQ(manager->deviceCount(), 3u);
    fake.clearSentFrames();

    EXPECT_FALSE(manager->setDeviceState(MOTION_ID, "on", true));
    EXPECT_FALSE(manager->setDeviceState(PLUG_ID, "brightness", 50));

    EXPECT_TRUE(airFrames().empty());
    EXPECT_TRUE(state(MOTION_ID, "on").is_null());
}

TEST_F(ZigbeeControlTest, InvalidColorTemperatureIsRejected) {
    loadAndPair();

    EXPECT_FALSE(manager->setDeviceState(LIGHT_ID, "colorTemp", 0));
    EXPECT_TRUE(airFrames().empty());
}

TEST_F(ZigbeeControlTest, ReportsAreConvertedToTheDevicesUnits) {
    loadAndPair();

    fake.reportAttribute(LIGHT.nwkAddr, 3, zcl::cluster::LEVEL_CONTROL,
                         zcl::attr::level::CURRENT_LEVEL, zcl::datatype::UINT8, {127});
    fake.reportAttribute(LIGHT.nwkAddr, 3, zcl::cluster::COLOR_CONTROL,
                         zcl::attr::color::COLOR_TEMP, zcl::datatype::UINT16, {250, 0});

    EXPECT_TRUE(eventually([&] { return state(LIGHT_ID, "brightness") == 50; }));
    EXPECT_TRUE(eventually([&] { return state(LIGHT_ID, "colorTemp") == 4000; }));
}

TEST_F(ZigbeeControlTest, RadioRejectionLeavesStateUnchanged) {
    loadAndPair();
    fake.setResponder(ZnpSubsystem::AF, cmd::af::DATA_REQUEST,
                      [](const ZnpFrame&) { return std::vector<uint8_t>{0x01}; });

    EXPECT_FALSE(manager->setDeviceState(LIGHT_ID, "on", true));

    EXPECT_EQ(state(LIGHT_ID, "on"), false);
}

TEST_F(ZigbeeControlTest, SilentRadioFailsWithinASecondOrSo) {
    loadAndPair();
    fake.setResponder(ZnpSubsystem::AF, cmd::af::DATA_REQUEST,
                      [](const ZnpFrame&) { return std::nullopt; });

    auto started = std::chrono::steady_clock::now();
    EXPECT_FALSE(manager->setDeviceState(LIGHT_ID, "on", true));

    EXPECT_LT(std::chrono::steady_clock::now() - started, 2s);
    EXPECT_EQ(state(LIGHT_ID, "on"), false);
}

TEST_F(ZigbeeControlTest, PropertiesTheProtocolCantChangeAreRejected) {
    loadAndPair();

    EXPECT_FALSE(manager->setDeviceState(LIGHT_ID, "hue", 120));
    EXPECT_FALSE(manager->setDeviceState(LIGHT_ID, "nonsense", 1));

    EXPECT_TRUE(airFrames().empty());
    EXPECT_TRUE(state(LIGHT_ID, "nonsense").is_null());
}

TEST_F(ZigbeeControlTest, WrongValueTypesAreRejected) {
    loadAndPair();

    EXPECT_FALSE(manager->setDeviceState(LIGHT_ID, "on", "yes"));
    EXPECT_FALSE(manager->setDeviceState(LIGHT_ID, "brightness", "bright"));

    EXPECT_TRUE(airFrames().empty());
    EXPECT_EQ(state(LIGHT_ID, "on"), false);
}

TEST_F(ZigbeeControlTest, UnknownDeviceFails) {
    loadAndPair();

    EXPECT_FALSE(manager->setDeviceState("zigbee_00000000deadbeef", "on", true));
    EXPECT_TRUE(airFrames().empty());
}

TEST_F(ZigbeeControlTest, DeviceWhoseProtocolIsntLoadedFails) {
    auto device = std::make_shared<SwitchDevice>("orphan", "Orphan", "zigbee");
    ASSERT_TRUE(manager->addDevice(device));

    EXPECT_FALSE(manager->setDeviceState("orphan", "on", true));

    EXPECT_EQ(device->getProperty("on"), false);
}

TEST_F(ZigbeeControlTest, LocalDevicesChangeDirectly) {
    auto device = std::make_shared<SwitchDevice>("local1", "Local");
    ASSERT_TRUE(manager->addDevice(device));

    EXPECT_TRUE(manager->setDeviceState("local1", "on", true));

    EXPECT_EQ(device->getProperty("on"), true);
    EXPECT_FALSE(manager->setDeviceState("local1", "on", "yes"));
    EXPECT_EQ(device->getProperty("on"), true);
}

TEST_F(ZigbeeControlTest, ReportConfirmingACommandPublishesNothing) {
    loadAndPair();
    std::atomic<int> onEvents{0};
    std::atomic<int> levelEvents{0};
    eventBus.subscribe("device.state", [&](const Event& event) {
        const auto& change = static_cast<const DeviceStateEvent&>(event);
        if (change.property == "on") ++onEvents;
        if (change.property == "brightness") ++levelEvents;
    });

    ASSERT_TRUE(manager->setDeviceState(PLUG_ID, "on", true));
    ASSERT_EQ(onEvents.load(), 1);

    fake.reportAttribute(PLUG.nwkAddr, 1, zcl::cluster::ON_OFF,
                         zcl::attr::onoff::ON_OFF, zcl::datatype::BOOLEAN, {0x01});
    // A level report on another device shows when the first has been handled.
    fake.reportAttribute(LIGHT.nwkAddr, 3, zcl::cluster::LEVEL_CONTROL,
                         zcl::attr::level::CURRENT_LEVEL, zcl::datatype::UINT8, {127});
    ASSERT_TRUE(eventually([&] { return levelEvents.load() == 1; }));

    EXPECT_EQ(onEvents.load(), 1);
}

TEST_F(ZigbeeControlTest, EventsOnlyPublishRealChanges) {
    loadAndPair();
    std::atomic<int> events{0};
    eventBus.subscribe("device.state", [&](const Event& event) {
        if (static_cast<const DeviceStateEvent&>(event).property == "on") ++events;
    });

    ASSERT_TRUE(manager->setDeviceState(LIGHT_ID, "on", true));
    ASSERT_TRUE(manager->setDeviceState(LIGHT_ID, "on", true));

    EXPECT_EQ(events.load(), 1);
}

// Automations react to reports this way, and only the reader thread delivers the reply
// to the command they send, so this would time out if reports arrived on that thread.
TEST_F(ZigbeeControlTest, CommandSentFromAReportHandlerSucceeds) {
    loadAndPair();
    std::atomic<bool> commanded{false};
    std::atomic<bool> succeeded{false};
    eventBus.subscribe("device.state", [&](const Event& event) {
        const auto& change = static_cast<const DeviceStateEvent&>(event);
        if (change.deviceId == LIGHT_ID && change.property == "on" && !commanded.exchange(true)) {
            auto started = std::chrono::steady_clock::now();
            succeeded = manager->setDeviceState(PLUG_ID, "on", true) &&
                        std::chrono::steady_clock::now() - started < 500ms;
        }
    });

    fake.reportAttribute(LIGHT.nwkAddr, 3, zcl::cluster::ON_OFF,
                         zcl::attr::onoff::ON_OFF, zcl::datatype::BOOLEAN, {0x01});

    ASSERT_TRUE(eventually([&] { return commanded.load() && state(PLUG_ID, "on") == true; }));
    EXPECT_TRUE(succeeded.load());
}

TEST_F(ZigbeeControlTest, AutomationActionReachesTheRadio) {
    loadAndPair();
    AutomationManager automations(eventBus, *database, *manager);
    automations.initialize();

    Automation automation;
    automation.id = "lights_on";
    automation.name = "Lights on";
    automation.enabled = true;
    Action action;
    action.type = ActionType::SetDeviceState;
    action.deviceId = LIGHT_ID;
    action.property = "on";
    action.value = true;
    automation.actions.push_back(action);
    ASSERT_TRUE(automations.addAutomation(automation));

    ASSERT_TRUE(automations.triggerAutomation("lights_on"));

    ASSERT_TRUE(eventually([&] { return !airFrames().empty(); }));
    EXPECT_EQ(airFrames()[0].zclCommand, zcl::cmd::onoff::ON);
}

TEST_F(ZigbeeControlTest, ConcurrentFailuresAreSafe) {
    loadAndPair();
    // The handler doesn't know this one, so each command fails and records an error.
    ASSERT_TRUE(manager->addDevice(std::make_shared<SwitchDevice>("unpaired", "Unpaired", "zigbee")));

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < 25; ++i) {
                EXPECT_FALSE(manager->setDeviceState("unpaired", "on", true));
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
}

TEST_F(ZigbeeControlTest, CommandsDuringUnloadAreSafe) {
    loadAndPair();

    std::atomic<bool> stop{false};
    std::thread commands([&] {
        bool on = true;
        while (!stop) {
            manager->setDeviceState(PLUG_ID, "on", on);
            on = !on;
        }
    });

    std::this_thread::sleep_for(100ms);
    manager->unloadProtocol("zigbee");
    std::this_thread::sleep_for(50ms);
    stop = true;
    commands.join();

    // Nothing left to send through, so it has to say so.
    EXPECT_FALSE(manager->setDeviceState(PLUG_ID, "on", true));
}

TEST_F(ZigbeeControlTest, ConcurrentCommandsAllSucceed) {
    loadAndPair();

    std::atomic<int> failures{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t] {
            for (int i = 0; i < 25; ++i) {
                if (!manager->setDeviceState(t % 2 ? PLUG_ID : LIGHT_ID, "on", i % 2 == 0)) {
                    ++failures;
                }
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(failures.load(), 0);
    EXPECT_EQ(airFrames().size(), 100u);
}
