/**
 * Automation System Unit Tests
 *
 * Tests for Automation data structures, AutomationManager CRUD operations,
 * trigger evaluation, condition checking, and action execution.
 */

#include <gtest/gtest.h>

#include <smarthub/devices/types/DimmerDevice.hpp>
#include <smarthub/devices/types/SwitchDevice.hpp>
#include <smarthub/devices/types/TemperatureSensor.hpp>
#include <smarthub/automation/Automation.hpp>
#include <smarthub/automation/AutomationManager.hpp>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/database/Database.hpp>
#include <smarthub/core/EventBus.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <filesystem>

namespace fs = std::filesystem;
using namespace smarthub;

// ============================================================================
// Automation Data Structures Tests
// ============================================================================

TEST(AutomationTest, TriggerTypes) {
    EXPECT_NE(TriggerType::DeviceState, TriggerType::Time);
    EXPECT_NE(TriggerType::Time, TriggerType::TimeInterval);
    EXPECT_NE(TriggerType::TimeInterval, TriggerType::SensorThreshold);
}

TEST(AutomationTest, ActionTypes) {
    EXPECT_NE(ActionType::SetDeviceState, ActionType::Delay);
}

TEST(AutomationTest, CompareOperators) {
    EXPECT_NE(CompareOp::Equal, CompareOp::NotEqual);
    EXPECT_NE(CompareOp::GreaterThan, CompareOp::LessThan);
    EXPECT_NE(CompareOp::GreaterOrEqual, CompareOp::LessOrEqual);
}

TEST(AutomationTest, LogicOperators) {
    EXPECT_NE(LogicOp::And, LogicOp::Or);
}

TEST(AutomationTest, TriggerDefaultValues) {
    Trigger trigger;
    trigger.type = TriggerType::DeviceState;
    trigger.deviceId = "test_device";
    trigger.property = "on";

    EXPECT_EQ(trigger.type, TriggerType::DeviceState);
    EXPECT_EQ(trigger.deviceId, "test_device");
    EXPECT_EQ(trigger.property, "on");
}

TEST(AutomationTest, ActionDefaultValues) {
    Action action;
    action.type = ActionType::SetDeviceState;
    action.deviceId = "test_device";
    action.property = "on";
    action.value = true;

    EXPECT_EQ(action.type, ActionType::SetDeviceState);
    EXPECT_EQ(action.deviceId, "test_device");
    EXPECT_TRUE(action.value.get<bool>());
}

TEST(AutomationTest, ConditionLeaf) {
    ConditionLeaf leaf;
    leaf.deviceId = "sensor_001";
    leaf.property = "temperature";
    leaf.op = CompareOp::GreaterThan;
    leaf.value = 75.0;

    EXPECT_EQ(leaf.deviceId, "sensor_001");
    EXPECT_EQ(leaf.property, "temperature");
    EXPECT_EQ(leaf.op, CompareOp::GreaterThan);
    EXPECT_DOUBLE_EQ(leaf.value.get<double>(), 75.0);
}

TEST(AutomationTest, ConditionGroup) {
    Condition cond;
    cond.isGroup = true;
    cond.groupOp = LogicOp::And;

    // Add child conditions
    Condition child1, child2;
    child1.isGroup = false;
    child1.leaf.deviceId = "sensor_001";
    child1.leaf.property = "motion";
    child1.leaf.op = CompareOp::Equal;
    child1.leaf.value = true;

    child2.isGroup = false;
    child2.leaf.deviceId = "light_001";
    child2.leaf.property = "on";
    child2.leaf.op = CompareOp::Equal;
    child2.leaf.value = false;

    cond.children.push_back(child1);
    cond.children.push_back(child2);

    EXPECT_TRUE(cond.isGroup);
    EXPECT_EQ(cond.groupOp, LogicOp::And);
    EXPECT_EQ(cond.children.size(), 2u);
}

TEST(AutomationTest, AutomationStructure) {
    Automation automation;
    automation.id = "auto_001";
    automation.name = "Turn on lights when motion detected";
    automation.description = "Test automation";
    automation.enabled = true;

    // Add trigger
    Trigger trigger;
    trigger.type = TriggerType::DeviceState;
    trigger.deviceId = "motion_sensor";
    trigger.property = "motion";
    trigger.toValue = true;
    automation.triggers.push_back(trigger);

    // Add action
    Action action;
    action.type = ActionType::SetDeviceState;
    action.deviceId = "living_room_light";
    action.property = "on";
    action.value = true;
    automation.actions.push_back(action);

    EXPECT_EQ(automation.id, "auto_001");
    EXPECT_EQ(automation.name, "Turn on lights when motion detected");
    EXPECT_TRUE(automation.enabled);
    EXPECT_EQ(automation.triggers.size(), 1u);
    EXPECT_EQ(automation.actions.size(), 1u);
}

// ============================================================================
// AutomationManager Test Fixture
// ============================================================================

class AutomationManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDbPath = "/tmp/automation_test_" + std::to_string(getpid()) + ".db";

        if (fs::exists(testDbPath)) {
            fs::remove(testDbPath);
        }

        eventBus = std::make_unique<EventBus>();
        database = std::make_unique<Database>(testDbPath);
        database->initialize();
        deviceManager = std::make_unique<DeviceManager>(*eventBus, *database);
        deviceManager->initialize();
        automationManager = std::make_unique<AutomationManager>(
            *eventBus, *database, *deviceManager);
    }

    void TearDown() override {
        automationManager.reset();
        deviceManager.reset();
        database.reset();
        eventBus.reset();

        if (fs::exists(testDbPath)) {
            fs::remove(testDbPath);
        }
    }

    Automation createTestAutomation(const std::string& id = "test_auto") {
        Automation automation;
        automation.id = id;
        automation.name = "Test Automation";
        automation.description = "A test automation";
        automation.enabled = true;

        Trigger trigger;
        trigger.type = TriggerType::DeviceState;
        trigger.deviceId = "motion_sensor";
        trigger.property = "motion";
        trigger.toValue = true;
        automation.triggers.push_back(trigger);

        Action action;
        action.type = ActionType::SetDeviceState;
        action.deviceId = "light_001";
        action.property = "on";
        action.value = true;
        automation.actions.push_back(action);

        return automation;
    }

    std::string testDbPath;
    std::unique_ptr<EventBus> eventBus;
    std::unique_ptr<Database> database;
    std::unique_ptr<DeviceManager> deviceManager;
    std::unique_ptr<AutomationManager> automationManager;
};

// ============================================================================
// AutomationManager Initialization Tests
// ============================================================================

TEST_F(AutomationManagerTest, Initialize) {
    EXPECT_TRUE(automationManager->initialize());
    EXPECT_EQ(automationManager->automationCount(), 0u);
}

TEST_F(AutomationManagerTest, InitializeTwice) {
    EXPECT_TRUE(automationManager->initialize());
    // Second initialization should also succeed (idempotent)
    EXPECT_TRUE(automationManager->initialize());
}

TEST_F(AutomationManagerTest, Shutdown) {
    automationManager->initialize();
    ASSERT_NO_THROW({
        automationManager->shutdown();
    });
}

// ============================================================================
// AutomationManager CRUD Tests
// ============================================================================

TEST_F(AutomationManagerTest, AddAutomation) {
    automationManager->initialize();

    auto automation = createTestAutomation();
    EXPECT_TRUE(automationManager->addAutomation(automation));
    EXPECT_EQ(automationManager->automationCount(), 1u);
}

TEST_F(AutomationManagerTest, ActionThatChangesADeviceDoesNotDeadlock) {
    automationManager->initialize();
    auto light = std::make_shared<SwitchDevice>("light_001", "Light");
    deviceManager->addDevice(light);

    // The change publishes an event back to the manager that's running this action.
    auto automation = createTestAutomation();
    ASSERT_TRUE(automationManager->addAutomation(automation));
    ASSERT_TRUE(automationManager->triggerAutomation(automation.id));

    automationManager->waitForRuns();
    EXPECT_EQ(light->getProperty("on"), true);
}

namespace {

// Turns light_001 on when the trigger fires
Automation lightOnWhen(const Trigger& trigger) {
    Automation automation;
    automation.id = "runs_action";
    automation.name = "Runs Action";
    automation.enabled = true;
    automation.triggers.push_back(trigger);

    Action action;
    action.type = ActionType::SetDeviceState;
    action.deviceId = "light_001";
    action.property = "on";
    action.value = true;
    automation.actions.push_back(action);
    return automation;
}

}  // namespace

TEST_F(AutomationManagerTest, DeviceStateChangeWithToValueRunsTheAction) {
    automationManager->initialize();
    auto source = std::make_shared<SwitchDevice>("source", "Source");
    auto light = std::make_shared<SwitchDevice>("light_001", "Light");
    deviceManager->addDevice(source);
    deviceManager->addDevice(light);

    Trigger trigger;
    trigger.type = TriggerType::DeviceState;
    trigger.deviceId = "source";
    trigger.property = "on";
    trigger.toValue = true;
    ASSERT_TRUE(automationManager->addAutomation(lightOnWhen(trigger)));

    automationManager->waitForRuns();
    EXPECT_NE(light->getProperty("on"), true);
    ASSERT_TRUE(deviceManager->setDeviceState("source", "on", true));
    automationManager->waitForRuns();
    EXPECT_EQ(light->getProperty("on"), true);
}

TEST_F(AutomationManagerTest, ToValueThatDoesNotMatchDoesNotRunTheAction) {
    automationManager->initialize();
    auto source = std::make_shared<SwitchDevice>("source", "Source");
    auto light = std::make_shared<SwitchDevice>("light_001", "Light");
    deviceManager->addDevice(source);
    deviceManager->addDevice(light);
    ASSERT_TRUE(deviceManager->setDeviceState("source", "on", true));

    Trigger trigger;
    trigger.type = TriggerType::DeviceState;
    trigger.deviceId = "source";
    trigger.property = "on";
    trigger.toValue = true;
    ASSERT_TRUE(automationManager->addAutomation(lightOnWhen(trigger)));

    ASSERT_TRUE(deviceManager->setDeviceState("source", "on", false));
    automationManager->waitForRuns();
    EXPECT_NE(light->getProperty("on"), true);
}

TEST_F(AutomationManagerTest, FromValueMatchesThePreviousValue) {
    automationManager->initialize();
    auto source = std::make_shared<DimmerDevice>("source", "Source");
    auto light = std::make_shared<SwitchDevice>("light_001", "Light");
    deviceManager->addDevice(source);
    deviceManager->addDevice(light);
    ASSERT_TRUE(deviceManager->setDeviceState("source", "brightness", 10));

    Trigger trigger;
    trigger.type = TriggerType::DeviceState;
    trigger.deviceId = "source";
    trigger.property = "brightness";
    trigger.fromValue = 20;
    ASSERT_TRUE(automationManager->addAutomation(lightOnWhen(trigger)));

    // From 10, which doesn't match
    ASSERT_TRUE(deviceManager->setDeviceState("source", "brightness", 20));
    automationManager->waitForRuns();
    EXPECT_NE(light->getProperty("on"), true);

    // From 20, which does
    ASSERT_TRUE(deviceManager->setDeviceState("source", "brightness", 30));
    automationManager->waitForRuns();
    EXPECT_EQ(light->getProperty("on"), true);
}

TEST_F(AutomationManagerTest, ThresholdTriggerUsesTheNewValue) {
    automationManager->initialize();
    auto source = std::make_shared<DimmerDevice>("source", "Source");
    auto light = std::make_shared<SwitchDevice>("light_001", "Light");
    deviceManager->addDevice(source);
    deviceManager->addDevice(light);

    Trigger trigger;
    trigger.type = TriggerType::SensorThreshold;
    trigger.deviceId = "source";
    trigger.property = "brightness";
    trigger.compareOp = CompareOp::GreaterThan;
    trigger.threshold = 50.0;
    ASSERT_TRUE(automationManager->addAutomation(lightOnWhen(trigger)));

    ASSERT_TRUE(deviceManager->setDeviceState("source", "brightness", 40));
    automationManager->waitForRuns();
    EXPECT_NE(light->getProperty("on"), true);

    ASSERT_TRUE(deviceManager->setDeviceState("source", "brightness", 60));
    automationManager->waitForRuns();
    EXPECT_EQ(light->getProperty("on"), true);
}

TEST_F(AutomationManagerTest, EachIntervalTriggerKeepsItsOwnTimer) {
    automationManager->initialize();
    auto light = std::make_shared<SwitchDevice>("light_001", "Light");
    deviceManager->addDevice(light);

    Trigger every7;
    every7.type = TriggerType::TimeInterval;
    every7.intervalMinutes = 7;
    Automation automation = lightOnWhen(every7);
    Trigger every5;
    every5.type = TriggerType::TimeInterval;
    every5.intervalMinutes = 5;
    automation.triggers.push_back(every5);
    ASSERT_TRUE(automationManager->addAutomation(automation));

    const uint64_t minute = 60 * 1000;
    automationManager->poll(1);

    automationManager->poll(1 + 5 * minute);
    automationManager->waitForRuns();
    EXPECT_EQ(light->getProperty("on"), true);
    ASSERT_TRUE(deviceManager->setDeviceState("light_001", "on", false));

    // The 7-minute trigger is due, even though the other one fired 2 minutes ago
    automationManager->poll(1 + 7 * minute);
    automationManager->waitForRuns();
    EXPECT_EQ(light->getProperty("on"), true);
}

TEST_F(AutomationManagerTest, IntervalTriggersDueTogetherRunOnce) {
    automationManager->initialize();
    auto light = std::make_shared<SwitchDevice>("light_001", "Light");
    deviceManager->addDevice(light);

    Trigger every5;
    every5.type = TriggerType::TimeInterval;
    every5.intervalMinutes = 5;
    Automation automation = lightOnWhen(every5);
    automation.triggers.push_back(every5);
    ASSERT_TRUE(automationManager->addAutomation(automation));

    const uint64_t minute = 60 * 1000;
    automationManager->poll(1);
    automationManager->poll(1 + 5 * minute);
    automationManager->waitForRuns();
    EXPECT_EQ(light->getProperty("on"), true);
    ASSERT_TRUE(deviceManager->setDeviceState("light_001", "on", false));

    automationManager->poll(2 + 5 * minute);
    automationManager->waitForRuns();
    EXPECT_NE(light->getProperty("on"), true);
}

TEST_F(AutomationManagerTest, ActionCantSetAReportedProperty) {
    automationManager->initialize();
    auto sensor = std::make_shared<TemperatureSensor>("sensor", "Sensor");
    auto source = std::make_shared<SwitchDevice>("source", "Source");
    deviceManager->addDevice(sensor);
    deviceManager->addDevice(source);
    const nlohmann::json before = sensor->getProperty("temperature");

    Trigger trigger;
    trigger.type = TriggerType::DeviceState;
    trigger.deviceId = "source";
    trigger.property = "on";
    Automation automation = lightOnWhen(trigger);
    automation.actions[0].deviceId = "sensor";
    automation.actions[0].property = "temperature";
    automation.actions[0].value = 500;
    ASSERT_TRUE(automationManager->addAutomation(automation));

    ASSERT_TRUE(deviceManager->setDeviceState("source", "on", true));
    automationManager->waitForRuns();
    EXPECT_EQ(sensor->getProperty("temperature"), before);
}

TEST_F(AutomationManagerTest, ReEnablingRestartsAnIntervalCountdown) {
    automationManager->initialize();
    auto light = std::make_shared<SwitchDevice>("light_001", "Light");
    deviceManager->addDevice(light);

    Trigger trigger;
    trigger.type = TriggerType::TimeInterval;
    trigger.intervalMinutes = 60;
    const Automation automation = lightOnWhen(trigger);
    ASSERT_TRUE(automationManager->addAutomation(automation));

    const uint64_t minute = 60 * 1000;
    automationManager->poll(1);
    ASSERT_TRUE(automationManager->setEnabled(automation.id, false));
    automationManager->poll(1 + 120 * minute);
    ASSERT_TRUE(automationManager->setEnabled(automation.id, true));

    automationManager->poll(2 + 120 * minute);
    automationManager->waitForRuns();
    EXPECT_NE(light->getProperty("on"), true);
    automationManager->poll(2 + 180 * minute);
    automationManager->waitForRuns();
    EXPECT_EQ(light->getProperty("on"), true);
}

TEST_F(AutomationManagerTest, InternalEventsDontRunAutomations) {
    automationManager->initialize();
    auto source = std::make_shared<SwitchDevice>("source", "Source");
    auto light = std::make_shared<SwitchDevice>("light_001", "Light");
    deviceManager->addDevice(source);
    deviceManager->addDevice(light);

    Trigger trigger;
    trigger.type = TriggerType::DeviceState;
    trigger.deviceId = "source";
    trigger.property = "_removed";
    ASSERT_TRUE(automationManager->addAutomation(lightOnWhen(trigger)));

    // Internal events aren't changes to a device, so nothing should run
    EXPECT_TRUE(deviceManager->removeDevice("source"));
    automationManager->waitForRuns();
    EXPECT_NE(light->getProperty("on"), true);
}

TEST_F(AutomationManagerTest, ProblemWithChecksWhatTheDeviceCanDo) {
    automationManager->initialize();
    auto light = std::make_shared<SwitchDevice>("light_001", "Light");
    deviceManager->addDevice(light);

    Trigger trigger;
    trigger.type = TriggerType::TimeInterval;
    trigger.intervalMinutes = 5;
    Automation automation = lightOnWhen(trigger);
    EXPECT_EQ(automationManager->problemWith(automation), "");

    automation.actions[0].property = "brightness";
    automation.actions[0].value = 50;
    EXPECT_NE(automationManager->problemWith(automation), "");

    automation.actions[0].property = "on";
    automation.actions[0].value = true;
    automation.name = "";
    EXPECT_NE(automationManager->problemWith(automation), "");
}

TEST_F(AutomationManagerTest, OrderingConditionOnAMissingValueDoesNotHold) {
    automationManager->initialize();
    auto source = std::make_shared<SwitchDevice>("source", "Source");
    auto light = std::make_shared<SwitchDevice>("light_001", "Light");
    deviceManager->addDevice(source);
    deviceManager->addDevice(light);

    Trigger trigger;
    trigger.type = TriggerType::DeviceState;
    trigger.deviceId = "source";
    trigger.property = "on";
    Automation automation = lightOnWhen(trigger);

    Condition condition;
    condition.leaf.deviceId = "source";
    condition.leaf.property = "temperature";  // A switch has none
    condition.leaf.op = CompareOp::GreaterThan;
    condition.leaf.value = 30;
    automation.condition = condition;
    ASSERT_TRUE(automationManager->addAutomation(automation));

    ASSERT_TRUE(deviceManager->setDeviceState("source", "on", true));
    automationManager->waitForRuns();
    EXPECT_NE(light->getProperty("on"), true);
}

TEST(AutomationTest, SavedDelayWithoutATypeLoadsAsADelay) {
    const Action action = Action::fromJson({{"delayMs", 500}});
    EXPECT_EQ(action.type, ActionType::Delay);
    EXPECT_EQ(action.delayMs, 500);

    const Action deviceAction = Action::fromJson(
        {{"deviceId", "light"}, {"property", "on"}, {"value", true}, {"delayMs", 500}});
    EXPECT_EQ(deviceAction.type, ActionType::SetDeviceState);
}

TEST_F(AutomationManagerTest, TurningOffDuringADelayStopsTheRemainingActions) {
    automationManager->initialize();
    auto light = std::make_shared<SwitchDevice>("light_001", "Light");
    deviceManager->addDevice(light);

    Trigger trigger;
    trigger.type = TriggerType::TimeInterval;
    trigger.intervalMinutes = 60;
    Automation automation = lightOnWhen(trigger);
    Action delay;
    delay.type = ActionType::Delay;
    delay.delayMs = 300;
    automation.actions.insert(automation.actions.begin(), delay);
    ASSERT_TRUE(automationManager->addAutomation(automation));

    ASSERT_TRUE(automationManager->triggerAutomation(automation.id));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_TRUE(automationManager->setEnabled(automation.id, false));

    automationManager->waitForRuns();
    EXPECT_NE(light->getProperty("on"), true);
}

namespace {

// Runs a delay, then turns on light_001, when the source device changes
Automation delayedLightOn(int delayMs) {
    Trigger trigger;
    trigger.type = TriggerType::DeviceState;
    trigger.deviceId = "source";
    trigger.property = "on";
    Automation automation = lightOnWhen(trigger);
    Action delay;
    delay.type = ActionType::Delay;
    delay.delayMs = delayMs;
    automation.actions.insert(automation.actions.begin(), delay);
    return automation;
}

}  // namespace

TEST_F(AutomationManagerTest, ADelayDoesNotHoldUpWhoChangedTheDevice) {
    automationManager->initialize();
    auto source = std::make_shared<SwitchDevice>("source", "Source");
    deviceManager->addDevice(source);
    deviceManager->addDevice(std::make_shared<SwitchDevice>("light_001", "Light"));
    ASSERT_TRUE(automationManager->addAutomation(delayedLightOn(30000)));

    const auto start = std::chrono::steady_clock::now();
    ASSERT_TRUE(deviceManager->setDeviceState("source", "on", true));
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(5));

    // Nor does it hold up shutdown, which drops the run
    automationManager->shutdown();
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(5));
}

TEST_F(AutomationManagerTest, ANewTriggerRestartsARunInADelay) {
    automationManager->initialize();
    auto source = std::make_shared<SwitchDevice>("source", "Source");
    auto light = std::make_shared<SwitchDevice>("light_001", "Light");
    deviceManager->addDevice(source);
    deviceManager->addDevice(light);
    ASSERT_TRUE(automationManager->addAutomation(delayedLightOn(400)));

    ASSERT_TRUE(deviceManager->setDeviceState("source", "on", true));
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    ASSERT_TRUE(deviceManager->setDeviceState("source", "on", false));

    // The first run would be finished by now, if it hadn't been restarted
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    EXPECT_NE(light->getProperty("on"), true);

    automationManager->waitForRuns();
    EXPECT_EQ(light->getProperty("on"), true);
}

TEST_F(AutomationManagerTest, DeletedAutomationDoesNotRunFromTheQueue) {
    automationManager->initialize();
    auto source = std::make_shared<SwitchDevice>("source", "Source");
    auto light = std::make_shared<SwitchDevice>("light_001", "Light");
    deviceManager->addDevice(source);
    deviceManager->addDevice(light);
    const Automation automation = delayedLightOn(200);
    ASSERT_TRUE(automationManager->addAutomation(automation));

    ASSERT_TRUE(deviceManager->setDeviceState("source", "on", true));
    ASSERT_TRUE(automationManager->deleteAutomation(automation.id));

    automationManager->waitForRuns();
    EXPECT_NE(light->getProperty("on"), true);
}

TEST_F(AutomationManagerTest, ADelayDoesNotHoldUpOtherAutomations) {
    automationManager->initialize();
    auto source = std::make_shared<SwitchDevice>("source", "Source");
    auto other = std::make_shared<SwitchDevice>("other", "Other");
    auto light = std::make_shared<SwitchDevice>("light_001", "Light");
    deviceManager->addDevice(source);
    deviceManager->addDevice(other);
    deviceManager->addDevice(light);

    ASSERT_TRUE(automationManager->addAutomation(delayedLightOn(30000)));
    Trigger trigger;
    trigger.type = TriggerType::DeviceState;
    trigger.deviceId = "source";
    trigger.property = "on";
    Automation immediate = lightOnWhen(trigger);
    immediate.id = "immediate";
    immediate.actions[0].deviceId = "other";
    ASSERT_TRUE(automationManager->addAutomation(immediate));

    ASSERT_TRUE(deviceManager->setDeviceState("source", "on", true));

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (other->getProperty("on") != true && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_EQ(other->getProperty("on"), true);
    EXPECT_NE(light->getProperty("on"), true);
}

TEST_F(AutomationManagerTest, ActionsRunAwayFromThePublishingThread) {
    automationManager->initialize();
    auto source = std::make_shared<SwitchDevice>("source", "Source");
    auto light = std::make_shared<SwitchDevice>("light_001", "Light");
    deviceManager->addDevice(source);
    deviceManager->addDevice(light);

    std::thread::id actionThread;
    const auto subscription = eventBus->subscribe("device.state", [&](const Event& event) {
        const auto& state = static_cast<const DeviceStateEvent&>(event);
        if (state.deviceId == "light_001") {
            actionThread = std::this_thread::get_id();
        }
    });

    Trigger trigger;
    trigger.type = TriggerType::DeviceState;
    trigger.deviceId = "source";
    trigger.property = "on";
    ASSERT_TRUE(automationManager->addAutomation(lightOnWhen(trigger)));

    ASSERT_TRUE(deviceManager->setDeviceState("source", "on", true));
    automationManager->waitForRuns();
    eventBus->unsubscribe(subscription);

    EXPECT_EQ(light->getProperty("on"), true);
    EXPECT_NE(actionThread, std::this_thread::get_id());
}

TEST_F(AutomationManagerTest, HandlerCanCallBackWhenADeviceIsAddedOrRemoved) {
    automationManager->initialize();
    std::vector<bool> found;
    const auto subscription = eventBus->subscribe("device.state", [&](const Event& event) {
        const auto& state = static_cast<const DeviceStateEvent&>(event);
        found.push_back(deviceManager->getDevice(state.deviceId) != nullptr);
    });

    deviceManager->addDevice(std::make_shared<SwitchDevice>("new", "New"));
    deviceManager->removeDevice("new");
    eventBus->unsubscribe(subscription);

    EXPECT_EQ(found, (std::vector<bool>{true, false}));
}

TEST_F(AutomationManagerTest, ConcurrentDeviceChangesAndAutomations) {
    automationManager->initialize();
    auto source = std::make_shared<SwitchDevice>("source", "Source");
    auto light = std::make_shared<SwitchDevice>("light_001", "Light");
    deviceManager->addDevice(source);
    deviceManager->addDevice(light);

    Trigger trigger;
    trigger.type = TriggerType::DeviceState;
    trigger.deviceId = "source";
    trigger.property = "on";
    ASSERT_TRUE(automationManager->addAutomation(lightOnWhen(trigger)));

    // The light is turned off below, so each run that turns it on is a change
    std::atomic<int> actionsRun{0};
    const auto subscription = eventBus->subscribe("device.state", [&](const Event& event) {
        const auto& state = static_cast<const DeviceStateEvent&>(event);
        if (state.deviceId == "light_001" && state.property == "on") {
            ++actionsRun;
        }
    });

    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;
    threads.emplace_back([&] {
        bool on = false;
        while (!stop) {
            on = !on;
            deviceManager->setDeviceState("source", "on", on);
            deviceManager->setDeviceState("light_001", "on", false);
        }
    });
    threads.emplace_back([&] {
        int count = 0;
        while (!stop) {
            const std::string id = "extra" + std::to_string(count++ % 4);
            deviceManager->addDevice(std::make_shared<SwitchDevice>(id, id));
            deviceManager->removeDevice(id);
        }
    });
    threads.emplace_back([&] {
        uint64_t time = 1;
        while (!stop) {
            automationManager->poll(time += 1000);
            automationManager->getAllAutomations();
            automationManager->setEnabled("runs_action", (time / 1000) % 16 != 0);
        }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    stop = true;
    for (auto& thread : threads) {
        thread.join();
    }
    automationManager->waitForRuns();
    EXPECT_GT(actionsRun.load(), 0);
    eventBus->unsubscribe(subscription);
}

TEST_F(AutomationManagerTest, ReEnablingAnEnabledAutomationKeepsItsCountdown) {
    automationManager->initialize();
    auto light = std::make_shared<SwitchDevice>("light_001", "Light");
    deviceManager->addDevice(light);

    Trigger trigger;
    trigger.type = TriggerType::TimeInterval;
    trigger.intervalMinutes = 60;
    const Automation automation = lightOnWhen(trigger);
    ASSERT_TRUE(automationManager->addAutomation(automation));

    const uint64_t minute = 60 * 1000;
    automationManager->poll(1);
    ASSERT_TRUE(automationManager->setEnabled(automation.id, true));
    automationManager->poll(1 + 61 * minute);
    automationManager->waitForRuns();
    EXPECT_EQ(light->getProperty("on"), true);
}

TEST_F(AutomationManagerTest, ReturnedAutomationsAreCopies) {
    automationManager->initialize();
    ASSERT_TRUE(automationManager->addAutomation(createTestAutomation()));
    const std::string id = createTestAutomation().id;

    automationManager->getAutomation(id)->name = "Changed";
    automationManager->getAllAutomations()[0]->name = "Changed";

    EXPECT_NE(automationManager->getAutomation(id)->name, "Changed");
}

TEST(AutomationTest, JsonRoundTripKeepsEveryPart) {
    Automation automation;
    automation.id = "round_trip";
    automation.name = "Round Trip";

    Trigger first;
    first.type = TriggerType::DeviceState;
    first.deviceId = "a";
    first.property = "on";
    automation.triggers.push_back(first);
    Trigger second;
    second.type = TriggerType::TimeInterval;
    second.intervalMinutes = 5;
    automation.triggers.push_back(second);

    Condition condition;
    condition.leaf.deviceId = "a";
    condition.leaf.property = "on";
    condition.leaf.value = true;
    automation.condition = condition;

    Action delay;
    delay.type = ActionType::Delay;
    delay.delayMs = 250;
    automation.actions.push_back(delay);

    const nlohmann::json json = automation.toJson();
    EXPECT_FALSE(json.contains("trigger"));

    const Automation copy = Automation::fromJson(json);
    ASSERT_EQ(copy.triggers.size(), 2u);
    EXPECT_EQ(copy.triggers[1].intervalMinutes, 5);
    ASSERT_TRUE(copy.condition.has_value());
    EXPECT_EQ(copy.condition->leaf.deviceId, "a");
    ASSERT_EQ(copy.actions.size(), 1u);
    EXPECT_EQ(copy.actions[0].type, ActionType::Delay);
    EXPECT_EQ(copy.actions[0].delayMs, 250);
}

TEST_F(AutomationManagerTest, AutomationsTriggeringEachOtherStopAtTheDepthLimit) {
    automationManager->initialize();
    auto lamp = std::make_shared<SwitchDevice>("lamp", "Lamp");
    deviceManager->addDevice(lamp);

    auto makeAutomation = [](const std::string& id, bool value) {
        Automation automation;
        automation.id = id;
        automation.name = id;
        automation.enabled = true;

        Trigger trigger;
        trigger.type = TriggerType::DeviceState;
        trigger.deviceId = "lamp";
        trigger.property = "on";
        automation.triggers.push_back(trigger);

        Action action;
        action.type = ActionType::SetDeviceState;
        action.deviceId = "lamp";
        action.property = "on";
        action.value = value;
        automation.actions.push_back(action);
        return automation;
    };

    // Each change makes one of these change the lamp back.
    ASSERT_TRUE(automationManager->addAutomation(makeAutomation("on", true)));
    ASSERT_TRUE(automationManager->addAutomation(makeAutomation("off", false)));

    ASSERT_TRUE(automationManager->triggerAutomation("on"));

    automationManager->waitForRuns();
    EXPECT_GT(automationManager->getAutomation("off")->lastTriggeredAt, 0u);
}

TEST_F(AutomationManagerTest, AddMultipleAutomations) {
    automationManager->initialize();

    EXPECT_TRUE(automationManager->addAutomation(createTestAutomation("auto_1")));
    EXPECT_TRUE(automationManager->addAutomation(createTestAutomation("auto_2")));
    EXPECT_TRUE(automationManager->addAutomation(createTestAutomation("auto_3")));

    EXPECT_EQ(automationManager->automationCount(), 3u);
}

TEST_F(AutomationManagerTest, AddDuplicateId) {
    automationManager->initialize();

    auto automation1 = createTestAutomation("same_id");
    auto automation2 = createTestAutomation("same_id");

    EXPECT_TRUE(automationManager->addAutomation(automation1));
    EXPECT_FALSE(automationManager->addAutomation(automation2));
    EXPECT_EQ(automationManager->automationCount(), 1u);
}

TEST_F(AutomationManagerTest, GetAutomation) {
    automationManager->initialize();

    auto automation = createTestAutomation("test_get");
    automationManager->addAutomation(automation);

    auto retrieved = automationManager->getAutomation("test_get");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->id, "test_get");
    EXPECT_EQ(retrieved->name, "Test Automation");
}

TEST_F(AutomationManagerTest, GetNonexistentAutomation) {
    automationManager->initialize();

    auto retrieved = automationManager->getAutomation("nonexistent");
    EXPECT_EQ(retrieved, nullptr);
}

TEST_F(AutomationManagerTest, GetAllAutomations) {
    automationManager->initialize();

    automationManager->addAutomation(createTestAutomation("auto_1"));
    automationManager->addAutomation(createTestAutomation("auto_2"));

    auto all = automationManager->getAllAutomations();
    EXPECT_EQ(all.size(), 2u);
}

TEST_F(AutomationManagerTest, UpdateAutomation) {
    automationManager->initialize();

    auto automation = createTestAutomation("update_test");
    automationManager->addAutomation(automation);

    automation.name = "Updated Name";
    automation.description = "Updated description";

    EXPECT_TRUE(automationManager->updateAutomation(automation));

    auto retrieved = automationManager->getAutomation("update_test");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->name, "Updated Name");
    EXPECT_EQ(retrieved->description, "Updated description");
}

TEST_F(AutomationManagerTest, UpdateNonexistentAutomation) {
    automationManager->initialize();

    auto automation = createTestAutomation("nonexistent");
    EXPECT_FALSE(automationManager->updateAutomation(automation));
}

TEST_F(AutomationManagerTest, DeleteAutomation) {
    automationManager->initialize();

    automationManager->addAutomation(createTestAutomation("delete_test"));
    EXPECT_EQ(automationManager->automationCount(), 1u);

    EXPECT_TRUE(automationManager->deleteAutomation("delete_test"));
    EXPECT_EQ(automationManager->automationCount(), 0u);

    auto retrieved = automationManager->getAutomation("delete_test");
    EXPECT_EQ(retrieved, nullptr);
}

TEST_F(AutomationManagerTest, DeleteNonexistentAutomation) {
    automationManager->initialize();

    EXPECT_FALSE(automationManager->deleteAutomation("nonexistent"));
}

// ============================================================================
// AutomationManager Enable/Disable Tests
// ============================================================================

TEST_F(AutomationManagerTest, SetEnabled) {
    automationManager->initialize();

    auto automation = createTestAutomation("enable_test");
    automationManager->addAutomation(automation);

    EXPECT_TRUE(automationManager->setEnabled("enable_test", false));

    auto retrieved = automationManager->getAutomation("enable_test");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_FALSE(retrieved->enabled);

    EXPECT_TRUE(automationManager->setEnabled("enable_test", true));

    retrieved = automationManager->getAutomation("enable_test");
    EXPECT_TRUE(retrieved->enabled);
}

TEST_F(AutomationManagerTest, SetEnabledNonexistent) {
    automationManager->initialize();

    EXPECT_FALSE(automationManager->setEnabled("nonexistent", true));
}

// ============================================================================
// AutomationManager ID Generation Tests
// ============================================================================

TEST_F(AutomationManagerTest, GenerateUniqueId) {
    automationManager->initialize();

    std::string id1 = automationManager->generateId();
    std::string id2 = automationManager->generateId();

    EXPECT_FALSE(id1.empty());
    EXPECT_FALSE(id2.empty());
    EXPECT_NE(id1, id2);
}

TEST_F(AutomationManagerTest, GeneratedIdFormat) {
    automationManager->initialize();

    std::string id = automationManager->generateId();

    // Should start with "auto_"
    EXPECT_EQ(id.substr(0, 5), "auto_");
}

// ============================================================================
// AutomationManager Persistence Tests
// ============================================================================

TEST_F(AutomationManagerTest, PersistenceAcrossRestart) {
    // Add automations
    {
        automationManager->initialize();
        automationManager->addAutomation(createTestAutomation("persist_1"));
        automationManager->addAutomation(createTestAutomation("persist_2"));
        automationManager->shutdown();
    }

    // Create new manager with same database
    automationManager = std::make_unique<AutomationManager>(
        *eventBus, *database, *deviceManager);
    automationManager->initialize();

    // Automations should be loaded from database
    EXPECT_EQ(automationManager->automationCount(), 2u);

    auto retrieved = automationManager->getAutomation("persist_1");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->name, "Test Automation");
}

TEST_F(AutomationManagerTest, DeletePersistence) {
    // Add and delete automation
    {
        automationManager->initialize();
        automationManager->addAutomation(createTestAutomation("delete_persist"));
        automationManager->deleteAutomation("delete_persist");
        automationManager->shutdown();
    }

    // Create new manager
    automationManager = std::make_unique<AutomationManager>(
        *eventBus, *database, *deviceManager);
    automationManager->initialize();

    // Deleted automation should not be loaded
    EXPECT_EQ(automationManager->automationCount(), 0u);
}

// ============================================================================
// AutomationManager Trigger Tests
// ============================================================================

TEST_F(AutomationManagerTest, ManualTrigger) {
    automationManager->initialize();

    auto automation = createTestAutomation("trigger_test");
    automationManager->addAutomation(automation);

    // Manual trigger should succeed
    EXPECT_TRUE(automationManager->triggerAutomation("trigger_test"));
}

TEST_F(AutomationManagerTest, TriggerDisabledAutomation) {
    automationManager->initialize();

    auto automation = createTestAutomation("disabled_trigger");
    automation.enabled = false;
    automationManager->addAutomation(automation);

    // Manual trigger via triggerAutomation() works even for disabled automations
    // (useful for testing). Disabled only affects automatic triggers.
    EXPECT_TRUE(automationManager->triggerAutomation("disabled_trigger"));
}

TEST_F(AutomationManagerTest, TriggerNonexistentAutomation) {
    automationManager->initialize();

    EXPECT_FALSE(automationManager->triggerAutomation("nonexistent"));
}

// ============================================================================
// AutomationManager Poll Tests
// ============================================================================

TEST_F(AutomationManagerTest, PollDoesNotCrash) {
    automationManager->initialize();

    // Add a time-based automation
    Automation automation;
    automation.id = "time_auto";
    automation.name = "Time Trigger Test";
    automation.enabled = true;

    Trigger trigger;
    trigger.type = TriggerType::Time;
    trigger.hour = 12;
    trigger.minute = 0;
    automation.triggers.push_back(trigger);

    automationManager->addAutomation(automation);

    // Poll should not crash
    ASSERT_NO_THROW({
        automationManager->poll(1000);
        automationManager->poll(2000);
        automationManager->poll(60000);
    });
}

TEST_F(AutomationManagerTest, PollIntervalTrigger) {
    automationManager->initialize();

    // Add interval-based automation
    Automation automation;
    automation.id = "interval_auto";
    automation.name = "Interval Trigger Test";
    automation.enabled = true;

    Trigger trigger;
    trigger.type = TriggerType::TimeInterval;
    trigger.intervalMinutes = 5;
    automation.triggers.push_back(trigger);

    automationManager->addAutomation(automation);

    // Poll over time - should not crash
    uint64_t time = 0;
    for (int i = 0; i < 10; i++) {
        ASSERT_NO_THROW({
            automationManager->poll(time);
        });
        time += 60000;  // Advance 1 minute
    }
}

// ============================================================================
// Time Trigger Tests
// ============================================================================

TEST_F(AutomationManagerTest, TimeTriggerFormat) {
    Trigger trigger;
    trigger.type = TriggerType::Time;
    trigger.hour = 18;
    trigger.minute = 30;

    EXPECT_EQ(trigger.hour, 18);
    EXPECT_EQ(trigger.minute, 30);
}

TEST_F(AutomationManagerTest, IntervalTriggerFormat) {
    Trigger trigger;
    trigger.type = TriggerType::TimeInterval;
    trigger.intervalMinutes = 15;

    EXPECT_EQ(trigger.intervalMinutes, 15);
}

// ============================================================================
// Sensor Threshold Trigger Tests
// ============================================================================

TEST_F(AutomationManagerTest, SensorThresholdTrigger) {
    Trigger trigger;
    trigger.type = TriggerType::SensorThreshold;
    trigger.deviceId = "temp_sensor";
    trigger.property = "temperature";
    trigger.compareOp = CompareOp::GreaterThan;
    trigger.threshold = 80.0;

    EXPECT_EQ(trigger.type, TriggerType::SensorThreshold);
    EXPECT_EQ(trigger.deviceId, "temp_sensor");
    EXPECT_EQ(trigger.compareOp, CompareOp::GreaterThan);
    EXPECT_DOUBLE_EQ(trigger.threshold, 80.0);
}

// ============================================================================
// Action Tests
// ============================================================================

TEST_F(AutomationManagerTest, SetDeviceStateAction) {
    Action action;
    action.type = ActionType::SetDeviceState;
    action.deviceId = "light_001";
    action.property = "brightness";
    action.value = 75;

    EXPECT_EQ(action.type, ActionType::SetDeviceState);
    EXPECT_EQ(action.value.get<int>(), 75);
}

TEST_F(AutomationManagerTest, DelayAction) {
    Action action;
    action.type = ActionType::Delay;
    action.delayMs = 5000;

    EXPECT_EQ(action.type, ActionType::Delay);
    EXPECT_EQ(action.delayMs, 5000u);
}

// Note: ActionType::Notify not yet implemented
// TEST_F(AutomationManagerTest, NotifyAction) { ... }

// ============================================================================
// Multiple Triggers/Actions Tests
// ============================================================================

TEST_F(AutomationManagerTest, MultipleTriggers) {
    automationManager->initialize();

    Automation automation;
    automation.id = "multi_trigger";
    automation.name = "Multiple Triggers";
    automation.enabled = true;

    // Add device state trigger
    Trigger trigger1;
    trigger1.type = TriggerType::DeviceState;
    trigger1.deviceId = "motion_sensor";
    trigger1.property = "motion";
    trigger1.toValue = true;
    automation.triggers.push_back(trigger1);

    // Add time trigger
    Trigger trigger2;
    trigger2.type = TriggerType::Time;
    trigger2.hour = 18;
    trigger2.minute = 0;
    automation.triggers.push_back(trigger2);

    EXPECT_TRUE(automationManager->addAutomation(automation));

    auto retrieved = automationManager->getAutomation("multi_trigger");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->triggers.size(), 2u);
}

TEST_F(AutomationManagerTest, MultipleActions) {
    automationManager->initialize();

    Automation automation;
    automation.id = "multi_action";
    automation.name = "Multiple Actions";
    automation.enabled = true;

    Trigger trigger;
    trigger.type = TriggerType::DeviceState;
    trigger.deviceId = "motion_sensor";
    trigger.property = "motion";
    automation.triggers.push_back(trigger);

    // Add multiple actions
    Action action1;
    action1.type = ActionType::SetDeviceState;
    action1.deviceId = "light_001";
    action1.property = "on";
    action1.value = true;
    automation.actions.push_back(action1);

    Action action2;
    action2.type = ActionType::Delay;
    action2.delayMs = 1000;
    automation.actions.push_back(action2);

    Action action3;
    action3.type = ActionType::SetDeviceState;
    action3.deviceId = "light_002";
    action3.property = "on";
    action3.value = true;
    automation.actions.push_back(action3);

    EXPECT_TRUE(automationManager->addAutomation(automation));

    auto retrieved = automationManager->getAutomation("multi_action");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->actions.size(), 3u);
}

// ============================================================================
// Condition Tests
// ============================================================================

TEST_F(AutomationManagerTest, AutomationWithCondition) {
    automationManager->initialize();

    Automation automation;
    automation.id = "with_condition";
    automation.name = "With Condition";
    automation.enabled = true;

    Trigger trigger;
    trigger.type = TriggerType::DeviceState;
    trigger.deviceId = "motion_sensor";
    trigger.property = "motion";
    trigger.toValue = true;
    automation.triggers.push_back(trigger);

    // Add condition: only if light is currently off
    Condition cond;
    cond.isGroup = false;
    cond.leaf.deviceId = "light_001";
    cond.leaf.property = "on";
    cond.leaf.op = CompareOp::Equal;
    cond.leaf.value = false;
    automation.condition = cond;

    Action action;
    action.type = ActionType::SetDeviceState;
    action.deviceId = "light_001";
    action.property = "on";
    action.value = true;
    automation.actions.push_back(action);

    EXPECT_TRUE(automationManager->addAutomation(automation));

    auto retrieved = automationManager->getAutomation("with_condition");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_TRUE(retrieved->condition.has_value());
}

TEST_F(AutomationManagerTest, AutomationWithoutCondition) {
    automationManager->initialize();

    auto automation = createTestAutomation("no_condition");
    EXPECT_TRUE(automationManager->addAutomation(automation));

    auto retrieved = automationManager->getAutomation("no_condition");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_FALSE(retrieved->condition.has_value());
}
