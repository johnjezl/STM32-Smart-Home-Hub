/**
 * Screen Unit Tests (Phase 8.C)
 *
 * Tests for DeviceListScreen, LightControlScreen, and SensorListScreen.
 * LVGL-specific rendering tested on hardware; these tests focus on
 * screen registration, navigation, and data handling.
 */

#include <gtest/gtest.h>

#ifdef SMARTHUB_ENABLE_LVGL

#include <smarthub/ui/screens/DashboardScreen.hpp>
#include <smarthub/ui/screens/DeviceListScreen.hpp>
#include <smarthub/ui/screens/LightControlScreen.hpp>
#include <smarthub/ui/screens/SensorListScreen.hpp>
#include <smarthub/ui/ScreenManager.hpp>
#include <smarthub/ui/ThemeManager.hpp>
#include <smarthub/ui/UIManager.hpp>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/devices/Device.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/database/Database.hpp>

#include <memory>
#include <filesystem>

namespace fs = std::filesystem;
using namespace smarthub;
using namespace smarthub::ui;

// ============================================================================
// Test Fixture
// ============================================================================

class ScreenTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDbPath = "/tmp/screen_test_" + std::to_string(getpid()) + ".db";

        if (fs::exists(testDbPath)) {
            fs::remove(testDbPath);
        }

        eventBus = std::make_unique<EventBus>();
        database = std::make_unique<Database>(testDbPath);
        database->initialize();
        deviceManager = std::make_unique<DeviceManager>(*eventBus, *database);
        deviceManager->initialize();
        themeManager = std::make_unique<ThemeManager>();
        uiManager = std::make_unique<UIManager>(*eventBus, *deviceManager);

        // Note: We don't initialize UIManager (no display in CI)
        // but screens can still be constructed for testing
    }

    void TearDown() override {
        screenManager.reset();
        uiManager.reset();
        themeManager.reset();
        deviceManager.reset();
        database.reset();
        eventBus.reset();

        if (fs::exists(testDbPath)) {
            fs::remove(testDbPath);
        }
    }

    std::string testDbPath;
    std::unique_ptr<EventBus> eventBus;
    std::unique_ptr<Database> database;
    std::unique_ptr<DeviceManager> deviceManager;
    std::unique_ptr<ThemeManager> themeManager;
    std::unique_ptr<UIManager> uiManager;
    std::unique_ptr<ScreenManager> screenManager;
};

// ============================================================================
// DashboardScreen Tests
// ============================================================================

TEST_F(ScreenTest, DashboardScreenRegistration) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto dashboard = std::make_unique<DashboardScreen>(
        *screenManager, *themeManager, *deviceManager);

    EXPECT_EQ(dashboard->name(), "dashboard");

    screenManager->registerScreen("dashboard", std::move(dashboard));
    EXPECT_TRUE(screenManager->hasScreen("dashboard"));
}

TEST_F(ScreenTest, DashboardScreenNavigation) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto dashboard = std::make_unique<DashboardScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto* dashPtr = dashboard.get();

    screenManager->registerScreen("dashboard", std::move(dashboard));
    screenManager->setHomeScreen("dashboard");

    EXPECT_TRUE(screenManager->showScreen("dashboard"));
    EXPECT_EQ(screenManager->currentScreen(), dashPtr);
}

// ============================================================================
// DeviceListScreen Tests
// ============================================================================

TEST_F(ScreenTest, DeviceListScreenRegistration) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto deviceList = std::make_unique<DeviceListScreen>(
        *screenManager, *themeManager, *deviceManager);

    EXPECT_EQ(deviceList->name(), "devices");

    screenManager->registerScreen("devices", std::move(deviceList));
    EXPECT_TRUE(screenManager->hasScreen("devices"));
}

TEST_F(ScreenTest, DeviceListScreenNavigation) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto dashboard = std::make_unique<DashboardScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto deviceList = std::make_unique<DeviceListScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto* deviceListPtr = deviceList.get();

    screenManager->registerScreen("dashboard", std::move(dashboard));
    screenManager->registerScreen("devices", std::move(deviceList));

    screenManager->showScreen("dashboard");
    screenManager->showScreen("devices");

    EXPECT_EQ(screenManager->currentScreen(), deviceListPtr);
    EXPECT_EQ(screenManager->stackDepth(), 1);
}

// ============================================================================
// LightControlScreen Tests
// ============================================================================

TEST_F(ScreenTest, LightControlScreenRegistration) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto lightControl = std::make_unique<LightControlScreen>(
        *screenManager, *themeManager, *deviceManager);

    EXPECT_EQ(lightControl->name(), "light_control");

    screenManager->registerScreen("light_control", std::move(lightControl));
    EXPECT_TRUE(screenManager->hasScreen("light_control"));
}

TEST_F(ScreenTest, LightControlScreenDeviceId) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto lightControl = std::make_unique<LightControlScreen>(
        *screenManager, *themeManager, *deviceManager);

    // Set device ID before showing screen
    lightControl->setDeviceId("light_001");

    screenManager->registerScreen("light_control", std::move(lightControl));
    EXPECT_TRUE(screenManager->hasScreen("light_control"));
}

TEST_F(ScreenTest, LightControlScreenBackNavigation) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto deviceList = std::make_unique<DeviceListScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto lightControl = std::make_unique<LightControlScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto* deviceListPtr = deviceList.get();

    screenManager->registerScreen("devices", std::move(deviceList));
    screenManager->registerScreen("light_control", std::move(lightControl));

    screenManager->showScreen("devices");
    screenManager->showScreen("light_control");

    EXPECT_EQ(screenManager->stackDepth(), 1);

    // Navigate back
    screenManager->goBack();
    EXPECT_EQ(screenManager->currentScreen(), deviceListPtr);
}

// ============================================================================
// SensorListScreen Tests
// ============================================================================

TEST_F(ScreenTest, SensorListScreenRegistration) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto sensorList = std::make_unique<SensorListScreen>(
        *screenManager, *themeManager, *deviceManager);

    EXPECT_EQ(sensorList->name(), "sensors");

    screenManager->registerScreen("sensors", std::move(sensorList));
    EXPECT_TRUE(screenManager->hasScreen("sensors"));
}

TEST_F(ScreenTest, SensorListScreenNavigation) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto dashboard = std::make_unique<DashboardScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto sensorList = std::make_unique<SensorListScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto* sensorListPtr = sensorList.get();

    screenManager->registerScreen("dashboard", std::move(dashboard));
    screenManager->registerScreen("sensors", std::move(sensorList));

    screenManager->showScreen("dashboard");
    screenManager->showScreen("sensors");

    EXPECT_EQ(screenManager->currentScreen(), sensorListPtr);
}

// ============================================================================
// Full Navigation Flow Tests
// ============================================================================

TEST_F(ScreenTest, FullNavigationFlow) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    // Register all screens
    auto dashboard = std::make_unique<DashboardScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto deviceList = std::make_unique<DeviceListScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto lightControl = std::make_unique<LightControlScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto sensorList = std::make_unique<SensorListScreen>(
        *screenManager, *themeManager, *deviceManager);

    auto* dashPtr = dashboard.get();
    auto* deviceListPtr = deviceList.get();
    auto* lightControlPtr = lightControl.get();
    auto* sensorListPtr = sensorList.get();

    screenManager->registerScreen("dashboard", std::move(dashboard));
    screenManager->registerScreen("devices", std::move(deviceList));
    screenManager->registerScreen("light_control", std::move(lightControl));
    screenManager->registerScreen("sensors", std::move(sensorList));
    screenManager->setHomeScreen("dashboard");

    // Start at dashboard
    screenManager->showScreen("dashboard");
    EXPECT_EQ(screenManager->currentScreen(), dashPtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);

    // Navigate to devices
    screenManager->showScreen("devices");
    EXPECT_EQ(screenManager->currentScreen(), deviceListPtr);
    EXPECT_EQ(screenManager->stackDepth(), 1);

    // Navigate to light control
    screenManager->showScreen("light_control");
    EXPECT_EQ(screenManager->currentScreen(), lightControlPtr);
    EXPECT_EQ(screenManager->stackDepth(), 2);

    // Go back to devices
    screenManager->goBack();
    EXPECT_EQ(screenManager->currentScreen(), deviceListPtr);
    EXPECT_EQ(screenManager->stackDepth(), 1);

    // Navigate to sensors (from devices)
    screenManager->showScreen("sensors");
    EXPECT_EQ(screenManager->currentScreen(), sensorListPtr);
    EXPECT_EQ(screenManager->stackDepth(), 2);

    // Go home
    screenManager->goHome();
    EXPECT_EQ(screenManager->currentScreen(), dashPtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);
}

TEST_F(ScreenTest, TabNavigation) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    // Register screens for tab navigation
    auto dashboard = std::make_unique<DashboardScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto deviceList = std::make_unique<DeviceListScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto sensorList = std::make_unique<SensorListScreen>(
        *screenManager, *themeManager, *deviceManager);

    auto* dashPtr = dashboard.get();
    auto* deviceListPtr = deviceList.get();
    auto* sensorListPtr = sensorList.get();

    screenManager->registerScreen("dashboard", std::move(dashboard));
    screenManager->registerScreen("devices", std::move(deviceList));
    screenManager->registerScreen("sensors", std::move(sensorList));
    screenManager->setHomeScreen("dashboard");

    // Tab navigation (no history push)
    screenManager->showScreen("dashboard");
    EXPECT_EQ(screenManager->currentScreen(), dashPtr);

    // Navigate with pushToStack=false (tab behavior)
    screenManager->showScreen("devices", TransitionType::None, false);
    EXPECT_EQ(screenManager->currentScreen(), deviceListPtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);  // No history

    screenManager->showScreen("sensors", TransitionType::None, false);
    EXPECT_EQ(screenManager->currentScreen(), sensorListPtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);  // Still no history

    // Can't go back since no history
    EXPECT_FALSE(screenManager->goBack());
}

// ============================================================================
// Screen Lifecycle Tests
// ============================================================================

TEST_F(ScreenTest, ScreenUpdateCalled) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto dashboard = std::make_unique<DashboardScreen>(
        *screenManager, *themeManager, *deviceManager);

    screenManager->registerScreen("dashboard", std::move(dashboard));
    screenManager->showScreen("dashboard");

    // Update should not crash
    ASSERT_NO_THROW({
        screenManager->update(16);
        screenManager->update(33);
        screenManager->update(16);
    });
}

TEST_F(ScreenTest, MultipleScreenUpdates) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto sensorList = std::make_unique<SensorListScreen>(
        *screenManager, *themeManager, *deviceManager);

    screenManager->registerScreen("sensors", std::move(sensorList));
    screenManager->showScreen("sensors");

    // SensorListScreen has periodic refresh (5000ms interval)
    // Multiple updates should work
    for (int i = 0; i < 100; i++) {
        ASSERT_NO_THROW({
            screenManager->update(50);  // 50ms * 100 = 5000ms
        });
    }
}

#else // !SMARTHUB_ENABLE_LVGL

// Placeholder tests when LVGL is not available
TEST(ScreenTest, LVGLNotAvailable) {
    GTEST_SKIP() << "LVGL not available, Screen tests skipped";
}

#endif // SMARTHUB_ENABLE_LVGL

// ============================================================================
// Device Type Tests (No LVGL required)
// ============================================================================

#include <smarthub/devices/Device.hpp>

using namespace smarthub;

TEST(DeviceTypeTest, SensorTypes) {
    // Verify sensor types used in SensorListScreen
    EXPECT_NE(DeviceType::TemperatureSensor, DeviceType::Unknown);
    EXPECT_NE(DeviceType::HumiditySensor, DeviceType::Unknown);
    EXPECT_NE(DeviceType::MotionSensor, DeviceType::Unknown);
    EXPECT_NE(DeviceType::ContactSensor, DeviceType::Unknown);
}

TEST(DeviceTypeTest, ControllableTypes) {
    // Verify controllable types used in DeviceListScreen
    EXPECT_NE(DeviceType::Switch, DeviceType::Unknown);
    EXPECT_NE(DeviceType::Dimmer, DeviceType::Unknown);
    EXPECT_NE(DeviceType::ColorLight, DeviceType::Unknown);
}

TEST(DeviceTypeTest, LightTypes) {
    // Types that should navigate to LightControlScreen
    EXPECT_NE(DeviceType::Dimmer, DeviceType::Switch);
    EXPECT_NE(DeviceType::ColorLight, DeviceType::Switch);
}
