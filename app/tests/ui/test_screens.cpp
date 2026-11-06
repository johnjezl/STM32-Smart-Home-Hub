/**
 * Screen Unit Tests (Phase 8.C + 8.D + 8.E)
 *
 * Tests for DeviceListScreen, LightControlScreen, SensorListScreen,
 * SensorHistoryScreen, and WifiSetupScreen.
 * LVGL-specific rendering tested on hardware; these tests focus on
 * screen registration, navigation, and data handling.
 */

#include <gtest/gtest.h>

#include "headless_display.hpp"

#ifdef SMARTHUB_ENABLE_LVGL

#include <smarthub/ui/screens/DashboardScreen.hpp>
#include <smarthub/ui/screens/DeviceListScreen.hpp>
#include <smarthub/ui/screens/LightControlScreen.hpp>
#include <smarthub/ui/screens/SensorListScreen.hpp>
#include <smarthub/ui/screens/SensorHistoryScreen.hpp>
#include <smarthub/ui/screens/AddDeviceScreen.hpp>
#include <smarthub/ui/screens/WifiSetupScreen.hpp>
#include <smarthub/devices/types/SwitchDevice.hpp>
#include <smarthub/ui/ScreenManager.hpp>
#include <smarthub/ui/ThemeManager.hpp>
#include <smarthub/ui/UIManager.hpp>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/devices/Device.hpp>
#include <smarthub/network/NetworkManager.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/database/Database.hpp>

#include <chrono>
#include <memory>
#include <thread>
#include <filesystem>

namespace fs = std::filesystem;
using namespace smarthub;
using namespace smarthub::ui;

// ============================================================================
// Test Fixture
// ============================================================================

class ScreenTest : public ::testing::Test {
protected:
    /** Navigation takes effect immediately, unless a test sets a duration itself. */
    void createScreenManager() {
        screenManager = std::make_unique<ScreenManager>(*uiManager);
        screenManager->setTransitionDuration(0);
    }

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
        networkManager = std::make_unique<network::NetworkManager>();
        uiManager = std::make_unique<UIManager>(*eventBus, *deviceManager, *database);

        // Note: We don't initialize UIManager (no display in CI)
        // but screens can still be constructed for testing
    }

    void TearDown() override {
        screenManager.reset();
        uiManager.reset();
        networkManager.reset();
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
    std::unique_ptr<network::NetworkManager> networkManager;
    std::unique_ptr<UIManager> uiManager;
    std::unique_ptr<ScreenManager> screenManager;
};

// ============================================================================
// DashboardScreen Tests
// ============================================================================

TEST_F(ScreenTest, DashboardScreenRegistration) {
    createScreenManager();

    auto dashboard = std::make_unique<DashboardScreen>(
        *screenManager, *themeManager, *deviceManager);

    EXPECT_EQ(dashboard->name(), "dashboard");

    screenManager->registerScreen("dashboard", std::move(dashboard));
    EXPECT_TRUE(screenManager->hasScreen("dashboard"));
}

TEST_F(ScreenTest, DashboardScreenNavigation) {
    createScreenManager();

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
    createScreenManager();

    auto deviceList = std::make_unique<DeviceListScreen>(
        *screenManager, *themeManager, *deviceManager);

    EXPECT_EQ(deviceList->name(), "devices");

    screenManager->registerScreen("devices", std::move(deviceList));
    EXPECT_TRUE(screenManager->hasScreen("devices"));
}

TEST_F(ScreenTest, DeviceListScreenNavigation) {
    createScreenManager();

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
    createScreenManager();

    auto lightControl = std::make_unique<LightControlScreen>(
        *screenManager, *themeManager, *deviceManager);

    EXPECT_EQ(lightControl->name(), "light_control");

    screenManager->registerScreen("light_control", std::move(lightControl));
    EXPECT_TRUE(screenManager->hasScreen("light_control"));
}

TEST_F(ScreenTest, LightControlScreenDeviceId) {
    createScreenManager();

    auto lightControl = std::make_unique<LightControlScreen>(
        *screenManager, *themeManager, *deviceManager);

    // Set device ID before showing screen
    lightControl->setDeviceId("light_001");

    screenManager->registerScreen("light_control", std::move(lightControl));
    EXPECT_TRUE(screenManager->hasScreen("light_control"));
}

TEST_F(ScreenTest, LightControlScreenBackNavigation) {
    createScreenManager();

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
    createScreenManager();

    auto sensorList = std::make_unique<SensorListScreen>(
        *screenManager, *themeManager, *deviceManager);

    EXPECT_EQ(sensorList->name(), "sensors");

    screenManager->registerScreen("sensors", std::move(sensorList));
    EXPECT_TRUE(screenManager->hasScreen("sensors"));
}

TEST_F(ScreenTest, SensorListScreenNavigation) {
    createScreenManager();

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
// SensorHistoryScreen Tests (Phase 8.D)
// ============================================================================

TEST_F(ScreenTest, SensorHistoryScreenRegistration) {
    createScreenManager();

    auto sensorHistory = std::make_unique<SensorHistoryScreen>(
        *screenManager, *themeManager, *deviceManager);

    EXPECT_EQ(sensorHistory->name(), "sensor_history");

    screenManager->registerScreen("sensor_history", std::move(sensorHistory));
    EXPECT_TRUE(screenManager->hasScreen("sensor_history"));
}

TEST_F(ScreenTest, SensorHistoryScreenSensorId) {
    createScreenManager();

    auto sensorHistory = std::make_unique<SensorHistoryScreen>(
        *screenManager, *themeManager, *deviceManager);

    // Set sensor ID before showing screen
    sensorHistory->setSensorId("temp_sensor_001");
    EXPECT_EQ(sensorHistory->sensorId(), "temp_sensor_001");

    screenManager->registerScreen("sensor_history", std::move(sensorHistory));
    EXPECT_TRUE(screenManager->hasScreen("sensor_history"));
}

TEST_F(ScreenTest, SensorHistoryScreenNavigation) {
    createScreenManager();

    auto sensorList = std::make_unique<SensorListScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto sensorHistory = std::make_unique<SensorHistoryScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto* sensorListPtr = sensorList.get();
    auto* sensorHistoryPtr = sensorHistory.get();

    screenManager->registerScreen("sensors", std::move(sensorList));
    screenManager->registerScreen("sensor_history", std::move(sensorHistory));

    // Navigate from sensor list to history
    screenManager->showScreen("sensors");
    screenManager->showScreen("sensor_history");

    EXPECT_EQ(screenManager->currentScreen(), sensorHistoryPtr);
    EXPECT_EQ(screenManager->stackDepth(), 1);

    // Navigate back to sensor list
    screenManager->goBack();
    EXPECT_EQ(screenManager->currentScreen(), sensorListPtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);
}

TEST_F(ScreenTest, SensorHistoryScreenUpdateInterval) {
    createScreenManager();

    auto sensorHistory = std::make_unique<SensorHistoryScreen>(
        *screenManager, *themeManager, *deviceManager);

    screenManager->registerScreen("sensor_history", std::move(sensorHistory));
    screenManager->showScreen("sensor_history");

    // Multiple updates shouldn't crash, even over the refresh interval
    for (int i = 0; i < 70; i++) {
        ASSERT_NO_THROW({
            screenManager->update(1000);  // 1 second, total 70 seconds > 60s interval
        });
    }
}

TEST_F(ScreenTest, SensorHistoryScreenSensorIdPersistence) {
    createScreenManager();

    auto sensorList = std::make_unique<SensorListScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto sensorHistory = std::make_unique<SensorHistoryScreen>(
        *screenManager, *themeManager, *deviceManager);

    screenManager->registerScreen("sensors", std::move(sensorList));
    screenManager->registerScreen("sensor_history", std::move(sensorHistory));

    // Get the history screen and set sensor ID
    auto* historyScreen = dynamic_cast<SensorHistoryScreen*>(
        screenManager->getScreen("sensor_history"));
    ASSERT_NE(historyScreen, nullptr);

    historyScreen->setSensorId("motion_001");
    EXPECT_EQ(historyScreen->sensorId(), "motion_001");

    // Navigate away and back
    screenManager->showScreen("sensors");
    screenManager->showScreen("sensor_history");

    // Sensor ID should persist
    EXPECT_EQ(historyScreen->sensorId(), "motion_001");
}

TEST_F(ScreenTest, SensorHistoryScreenMultipleSensors) {
    createScreenManager();

    auto sensorHistory = std::make_unique<SensorHistoryScreen>(
        *screenManager, *themeManager, *deviceManager);

    screenManager->registerScreen("sensor_history", std::move(sensorHistory));

    auto* historyScreen = dynamic_cast<SensorHistoryScreen*>(
        screenManager->getScreen("sensor_history"));
    ASSERT_NE(historyScreen, nullptr);

    // Test with different sensor types
    std::vector<std::string> sensorIds = {
        "temp_living_room",
        "humidity_bathroom",
        "motion_hallway",
        "contact_front_door"
    };

    for (const auto& id : sensorIds) {
        historyScreen->setSensorId(id);
        EXPECT_EQ(historyScreen->sensorId(), id);
    }
}

// ============================================================================
// Full Navigation Flow Tests
// ============================================================================

TEST_F(ScreenTest, FullNavigationFlow) {
    createScreenManager();

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

TEST_F(ScreenTest, FullNavigationFlowWithSensorHistory) {
    createScreenManager();

    // Register all screens including sensor history
    auto dashboard = std::make_unique<DashboardScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto sensorList = std::make_unique<SensorListScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto sensorHistory = std::make_unique<SensorHistoryScreen>(
        *screenManager, *themeManager, *deviceManager);

    auto* dashPtr = dashboard.get();
    auto* sensorListPtr = sensorList.get();
    auto* sensorHistoryPtr = sensorHistory.get();

    screenManager->registerScreen("dashboard", std::move(dashboard));
    screenManager->registerScreen("sensors", std::move(sensorList));
    screenManager->registerScreen("sensor_history", std::move(sensorHistory));
    screenManager->setHomeScreen("dashboard");

    // Start at dashboard
    screenManager->showScreen("dashboard");
    EXPECT_EQ(screenManager->currentScreen(), dashPtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);

    // Navigate to sensor list (tab navigation, no push)
    screenManager->showScreen("sensors", TransitionType::None, false);
    EXPECT_EQ(screenManager->currentScreen(), sensorListPtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);  // Tab navigation doesn't push

    // Navigate to sensor history (detail navigation, pushes)
    auto* historyScreen = dynamic_cast<SensorHistoryScreen*>(
        screenManager->getScreen("sensor_history"));
    ASSERT_NE(historyScreen, nullptr);
    historyScreen->setSensorId("temp_001");

    screenManager->showScreen("sensor_history");
    EXPECT_EQ(screenManager->currentScreen(), sensorHistoryPtr);
    EXPECT_EQ(screenManager->stackDepth(), 1);  // Detail view pushes
    EXPECT_EQ(historyScreen->sensorId(), "temp_001");

    // Go back to sensor list
    screenManager->goBack();
    EXPECT_EQ(screenManager->currentScreen(), sensorListPtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);

    // Navigate to different sensor's history
    historyScreen->setSensorId("humidity_001");
    screenManager->showScreen("sensor_history");
    EXPECT_EQ(screenManager->currentScreen(), sensorHistoryPtr);
    EXPECT_EQ(historyScreen->sensorId(), "humidity_001");

    // Go home from sensor history
    screenManager->goHome();
    EXPECT_EQ(screenManager->currentScreen(), dashPtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);
}

TEST_F(ScreenTest, TabNavigation) {
    createScreenManager();

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

TEST_F(ScreenTest, CompleteAppNavigationFlow) {
    createScreenManager();

    // Register ALL screens used in the app
    auto dashboard = std::make_unique<DashboardScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto deviceList = std::make_unique<DeviceListScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto lightControl = std::make_unique<LightControlScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto sensorList = std::make_unique<SensorListScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto sensorHistory = std::make_unique<SensorHistoryScreen>(
        *screenManager, *themeManager, *deviceManager);

    auto* dashPtr = dashboard.get();
    auto* deviceListPtr = deviceList.get();
    auto* lightControlPtr = lightControl.get();
    auto* sensorListPtr = sensorList.get();
    auto* sensorHistoryPtr = sensorHistory.get();

    screenManager->registerScreen("dashboard", std::move(dashboard));
    screenManager->registerScreen("devices", std::move(deviceList));
    screenManager->registerScreen("light_control", std::move(lightControl));
    screenManager->registerScreen("sensors", std::move(sensorList));
    screenManager->registerScreen("sensor_history", std::move(sensorHistory));
    screenManager->setHomeScreen("dashboard");

    // ========== Scenario 1: Device control flow ==========
    // User opens app → taps Devices tab → selects light → controls → back → home

    screenManager->showScreen("dashboard");
    EXPECT_EQ(screenManager->currentScreen(), dashPtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);

    // Tab to devices (no push)
    screenManager->showScreen("devices", TransitionType::None, false);
    EXPECT_EQ(screenManager->currentScreen(), deviceListPtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);

    // Select a light (detail push)
    auto* lightScreen = dynamic_cast<LightControlScreen*>(
        screenManager->getScreen("light_control"));
    lightScreen->setDeviceId("living_room_light");
    screenManager->showScreen("light_control");
    EXPECT_EQ(screenManager->currentScreen(), lightControlPtr);
    EXPECT_EQ(screenManager->stackDepth(), 1);

    // Go back
    screenManager->goBack();
    EXPECT_EQ(screenManager->currentScreen(), deviceListPtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);

    // Go home
    screenManager->goHome();
    EXPECT_EQ(screenManager->currentScreen(), dashPtr);

    // ========== Scenario 2: Sensor history flow ==========
    // User taps Sensors tab → selects sensor → views history → back → home

    screenManager->showScreen("sensors", TransitionType::None, false);
    EXPECT_EQ(screenManager->currentScreen(), sensorListPtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);

    // Select a sensor for history (detail push)
    auto* historyScreen = dynamic_cast<SensorHistoryScreen*>(
        screenManager->getScreen("sensor_history"));
    historyScreen->setSensorId("kitchen_temp");
    screenManager->showScreen("sensor_history");
    EXPECT_EQ(screenManager->currentScreen(), sensorHistoryPtr);
    EXPECT_EQ(screenManager->stackDepth(), 1);
    EXPECT_EQ(historyScreen->sensorId(), "kitchen_temp");

    // Go back to sensor list
    screenManager->goBack();
    EXPECT_EQ(screenManager->currentScreen(), sensorListPtr);

    // View another sensor's history
    historyScreen->setSensorId("bedroom_humidity");
    screenManager->showScreen("sensor_history");
    EXPECT_EQ(historyScreen->sensorId(), "bedroom_humidity");

    // Go home directly
    screenManager->goHome();
    EXPECT_EQ(screenManager->currentScreen(), dashPtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);

    // ========== Scenario 3: Deep navigation ==========
    // Dashboard → Devices → Light → back → Sensors → History → back → back → home

    screenManager->showScreen("devices");
    screenManager->showScreen("light_control");
    EXPECT_EQ(screenManager->stackDepth(), 2);

    screenManager->goBack();  // to devices
    EXPECT_EQ(screenManager->currentScreen(), deviceListPtr);

    screenManager->showScreen("sensors");
    screenManager->showScreen("sensor_history");
    EXPECT_EQ(screenManager->stackDepth(), 3);  // dashboard → devices → sensors; history is current

    // Back through entire stack
    screenManager->goBack();  // to sensors
    EXPECT_EQ(screenManager->currentScreen(), sensorListPtr);

    screenManager->goBack();  // to devices
    EXPECT_EQ(screenManager->currentScreen(), deviceListPtr);

    screenManager->goBack();  // to dashboard
    EXPECT_EQ(screenManager->currentScreen(), dashPtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);
}

// ============================================================================
// Screen Lifecycle Tests
// ============================================================================

TEST_F(ScreenTest, ScreenUpdateCalled) {
    createScreenManager();

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
    createScreenManager();

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

// ============================================================================
// WifiSetupScreen Tests (Phase 8.E)
// ============================================================================

TEST_F(ScreenTest, WifiSetupScreenRegistration) {
    createScreenManager();

    auto wifiSetup = std::make_unique<WifiSetupScreen>(
        *screenManager, *themeManager, *networkManager);

    EXPECT_EQ(wifiSetup->name(), "wifi_setup");

    screenManager->registerScreen("wifi_setup", std::move(wifiSetup));
    EXPECT_TRUE(screenManager->hasScreen("wifi_setup"));
}

TEST_F(ScreenTest, WifiSetupScreenNavigation) {
    createScreenManager();

    auto dashboard = std::make_unique<DashboardScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto wifiSetup = std::make_unique<WifiSetupScreen>(
        *screenManager, *themeManager, *networkManager);
    auto* wifiSetupPtr = wifiSetup.get();

    screenManager->registerScreen("dashboard", std::move(dashboard));
    screenManager->registerScreen("wifi_setup", std::move(wifiSetup));
    screenManager->setHomeScreen("dashboard");

    // Navigate from dashboard to WiFi setup
    screenManager->showScreen("dashboard");
    screenManager->showScreen("wifi_setup");

    EXPECT_EQ(screenManager->currentScreen(), wifiSetupPtr);
    EXPECT_EQ(screenManager->stackDepth(), 1);

    // Navigate back
    screenManager->goBack();
    EXPECT_EQ(screenManager->stackDepth(), 0);
}

TEST_F(ScreenTest, WifiSetupScreenUpdate) {
    createScreenManager();

    auto wifiSetup = std::make_unique<WifiSetupScreen>(
        *screenManager, *themeManager, *networkManager);

    screenManager->registerScreen("wifi_setup", std::move(wifiSetup));
    screenManager->showScreen("wifi_setup");

    // Multiple updates shouldn't crash
    for (int i = 0; i < 10; i++) {
        ASSERT_NO_THROW({
            screenManager->update(1000);
        });
    }
}

TEST_F(ScreenTest, WifiSetupScreenFromSettings) {
    createScreenManager();

    auto dashboard = std::make_unique<DashboardScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto sensorList = std::make_unique<SensorListScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto wifiSetup = std::make_unique<WifiSetupScreen>(
        *screenManager, *themeManager, *networkManager);

    auto* dashPtr = dashboard.get();
    auto* wifiPtr = wifiSetup.get();

    screenManager->registerScreen("dashboard", std::move(dashboard));
    screenManager->registerScreen("sensors", std::move(sensorList));
    screenManager->registerScreen("wifi_setup", std::move(wifiSetup));
    screenManager->setHomeScreen("dashboard");

    // Simulate: Dashboard -> Sensors (tab) -> WiFi Setup (from settings)
    screenManager->showScreen("dashboard");
    screenManager->showScreen("sensors", TransitionType::None, false);  // Tab nav
    screenManager->showScreen("wifi_setup");  // Push to stack

    EXPECT_EQ(screenManager->currentScreen(), wifiPtr);
    EXPECT_EQ(screenManager->stackDepth(), 1);

    // Go home should return to dashboard
    screenManager->goHome();
    EXPECT_EQ(screenManager->currentScreen(), dashPtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);
}

TEST_F(ScreenTest, WifiSetupScreenName) {
    createScreenManager();

    auto wifiSetup = std::make_unique<WifiSetupScreen>(
        *screenManager, *themeManager, *networkManager);

    EXPECT_EQ(wifiSetup->name(), "wifi_setup");
}

TEST_F(ScreenTest, WifiSetupScreenAutoRefresh) {
    createScreenManager();

    auto wifiSetup = std::make_unique<WifiSetupScreen>(
        *screenManager, *themeManager, *networkManager);

    screenManager->registerScreen("wifi_setup", std::move(wifiSetup));
    screenManager->showScreen("wifi_setup");

    // Simulate 35 seconds of updates (past 30s auto-refresh interval)
    for (int i = 0; i < 35; i++) {
        ASSERT_NO_THROW({
            screenManager->update(1000);  // 1 second each
        });
    }
}

TEST_F(ScreenTest, WifiSetupScreenMultipleShowHide) {
    createScreenManager();

    auto dashboard = std::make_unique<DashboardScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto wifiSetup = std::make_unique<WifiSetupScreen>(
        *screenManager, *themeManager, *networkManager);

    screenManager->registerScreen("dashboard", std::move(dashboard));
    screenManager->registerScreen("wifi_setup", std::move(wifiSetup));
    screenManager->setHomeScreen("dashboard");

    // Show/hide multiple times
    for (int i = 0; i < 3; i++) {
        screenManager->showScreen("dashboard");
        screenManager->showScreen("wifi_setup");
        screenManager->goBack();
    }

    // Should end at dashboard
    EXPECT_EQ(screenManager->stackDepth(), 0);
}

TEST_F(ScreenTest, WifiSetupWithNetworkManagerNotInitialized) {
    // NetworkManager not initialized - screen should still work
    createScreenManager();

    auto wifiSetup = std::make_unique<WifiSetupScreen>(
        *screenManager, *themeManager, *networkManager);

    screenManager->registerScreen("wifi_setup", std::move(wifiSetup));

    ASSERT_NO_THROW({
        screenManager->showScreen("wifi_setup");
        screenManager->update(1000);
    });
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

// ============================================================================
// Phase 8.F: Settings and Display Management Tests
// ============================================================================

#ifdef SMARTHUB_ENABLE_LVGL

#include <smarthub/ui/screens/SettingsScreen.hpp>
#include <smarthub/ui/screens/DisplaySettingsScreen.hpp>
#include <smarthub/ui/screens/AboutScreen.hpp>
#include <smarthub/ui/DisplayManager.hpp>

// ============================================================================
// SettingsScreen Tests
// ============================================================================

TEST_F(ScreenTest, SettingsScreenRegistration) {
    createScreenManager();

    auto settings = std::make_unique<SettingsScreen>(
        *screenManager, *themeManager, *deviceManager);

    EXPECT_EQ(settings->name(), "settings");

    screenManager->registerScreen("settings", std::move(settings));
    EXPECT_TRUE(screenManager->hasScreen("settings"));
}

TEST_F(ScreenTest, SettingsScreenNavigation) {
    createScreenManager();

    auto dashboard = std::make_unique<DashboardScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto settings = std::make_unique<SettingsScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto* settingsPtr = settings.get();

    screenManager->registerScreen("dashboard", std::move(dashboard));
    screenManager->registerScreen("settings", std::move(settings));
    screenManager->setHomeScreen("dashboard");

    screenManager->showScreen("dashboard");
    screenManager->showScreen("settings");

    EXPECT_EQ(screenManager->currentScreen(), settingsPtr);
    EXPECT_EQ(screenManager->stackDepth(), 1);

    screenManager->goBack();
    EXPECT_EQ(screenManager->stackDepth(), 0);
}

TEST_F(ScreenTest, SettingsScreenUpdate) {
    createScreenManager();

    auto settings = std::make_unique<SettingsScreen>(
        *screenManager, *themeManager, *deviceManager);

    screenManager->registerScreen("settings", std::move(settings));
    screenManager->showScreen("settings");

    // Updates should not crash
    for (int i = 0; i < 10; i++) {
        ASSERT_NO_THROW({
            screenManager->update(100);
        });
    }
}

// ============================================================================
// DisplaySettingsScreen Tests
// ============================================================================

TEST_F(ScreenTest, DisplaySettingsScreenRegistration) {
    createScreenManager();

    DisplayManager displayManager;
    displayManager.initialize();

    auto displaySettings = std::make_unique<DisplaySettingsScreen>(
        *screenManager, *themeManager, displayManager);

    EXPECT_EQ(displaySettings->name(), "display_settings");

    screenManager->registerScreen("display_settings", std::move(displaySettings));
    EXPECT_TRUE(screenManager->hasScreen("display_settings"));
}

TEST_F(ScreenTest, DisplaySettingsScreenNavigation) {
    createScreenManager();

    DisplayManager displayManager;
    displayManager.initialize();

    auto settings = std::make_unique<SettingsScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto displaySettings = std::make_unique<DisplaySettingsScreen>(
        *screenManager, *themeManager, displayManager);
    auto* displaySettingsPtr = displaySettings.get();

    screenManager->registerScreen("settings", std::move(settings));
    screenManager->registerScreen("display_settings", std::move(displaySettings));

    screenManager->showScreen("settings");
    screenManager->showScreen("display_settings");

    EXPECT_EQ(screenManager->currentScreen(), displaySettingsPtr);
    EXPECT_EQ(screenManager->stackDepth(), 1);

    screenManager->goBack();
    EXPECT_EQ(screenManager->stackDepth(), 0);
}

TEST_F(ScreenTest, DisplaySettingsScreenUpdate) {
    createScreenManager();

    DisplayManager displayManager;
    displayManager.initialize();

    auto displaySettings = std::make_unique<DisplaySettingsScreen>(
        *screenManager, *themeManager, displayManager);

    screenManager->registerScreen("display_settings", std::move(displaySettings));
    screenManager->showScreen("display_settings");

    // Updates should not crash
    for (int i = 0; i < 10; i++) {
        ASSERT_NO_THROW({
            screenManager->update(100);
        });
    }
}

// ============================================================================
// AboutScreen Tests
// ============================================================================

TEST_F(ScreenTest, AboutScreenRegistration) {
    createScreenManager();

    auto about = std::make_unique<AboutScreen>(*screenManager, *themeManager);

    EXPECT_EQ(about->name(), "about");

    screenManager->registerScreen("about", std::move(about));
    EXPECT_TRUE(screenManager->hasScreen("about"));
}

TEST_F(ScreenTest, AboutScreenNavigation) {
    createScreenManager();

    auto settings = std::make_unique<SettingsScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto about = std::make_unique<AboutScreen>(*screenManager, *themeManager);
    auto* aboutPtr = about.get();

    screenManager->registerScreen("settings", std::move(settings));
    screenManager->registerScreen("about", std::move(about));

    screenManager->showScreen("settings");
    screenManager->showScreen("about");

    EXPECT_EQ(screenManager->currentScreen(), aboutPtr);
    EXPECT_EQ(screenManager->stackDepth(), 1);

    screenManager->goBack();
    EXPECT_EQ(screenManager->stackDepth(), 0);
}

TEST_F(ScreenTest, AboutScreenSystemInfo) {
    createScreenManager();

    auto about = std::make_unique<AboutScreen>(*screenManager, *themeManager);
    auto* aboutPtr = about.get();

    screenManager->registerScreen("about", std::move(about));
    screenManager->showScreen("about");

    // SystemInfo should be populated after show
    auto info = aboutPtr->getSystemInfo();
    EXPECT_FALSE(info.version.empty());
    EXPECT_FALSE(info.buildDate.empty());
    EXPECT_FALSE(info.platform.empty());
}

TEST_F(ScreenTest, AboutScreenUpdate) {
    createScreenManager();

    auto about = std::make_unique<AboutScreen>(*screenManager, *themeManager);

    screenManager->registerScreen("about", std::move(about));
    screenManager->showScreen("about");

    // Updates should not crash
    for (int i = 0; i < 10; i++) {
        ASSERT_NO_THROW({
            screenManager->update(100);
        });
    }
}

// ============================================================================
// Settings Navigation Flow Tests
// ============================================================================

TEST_F(ScreenTest, SettingsNavigationToSubScreens) {
    createScreenManager();

    DisplayManager displayManager;
    displayManager.initialize();

    auto dashboard = std::make_unique<DashboardScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto settings = std::make_unique<SettingsScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto wifiSetup = std::make_unique<WifiSetupScreen>(
        *screenManager, *themeManager, *networkManager);
    auto displaySettings = std::make_unique<DisplaySettingsScreen>(
        *screenManager, *themeManager, displayManager);
    auto about = std::make_unique<AboutScreen>(*screenManager, *themeManager);

    auto* dashPtr = dashboard.get();
    auto* settingsPtr = settings.get();
    auto* wifiPtr = wifiSetup.get();
    auto* displayPtr = displaySettings.get();
    auto* aboutPtr = about.get();

    screenManager->registerScreen("dashboard", std::move(dashboard));
    screenManager->registerScreen("settings", std::move(settings));
    screenManager->registerScreen("wifi_setup", std::move(wifiSetup));
    screenManager->registerScreen("display_settings", std::move(displaySettings));
    screenManager->registerScreen("about", std::move(about));
    screenManager->setHomeScreen("dashboard");

    // Dashboard -> Settings
    screenManager->showScreen("dashboard");
    screenManager->showScreen("settings");
    EXPECT_EQ(screenManager->currentScreen(), settingsPtr);
    EXPECT_EQ(screenManager->stackDepth(), 1);

    // Settings -> WiFi Setup
    screenManager->showScreen("wifi_setup");
    EXPECT_EQ(screenManager->currentScreen(), wifiPtr);
    EXPECT_EQ(screenManager->stackDepth(), 2);

    // Back to Settings
    screenManager->goBack();
    EXPECT_EQ(screenManager->currentScreen(), settingsPtr);

    // Settings -> Display Settings
    screenManager->showScreen("display_settings");
    EXPECT_EQ(screenManager->currentScreen(), displayPtr);
    EXPECT_EQ(screenManager->stackDepth(), 2);

    // Back to Settings
    screenManager->goBack();
    EXPECT_EQ(screenManager->currentScreen(), settingsPtr);

    // Settings -> About
    screenManager->showScreen("about");
    EXPECT_EQ(screenManager->currentScreen(), aboutPtr);
    EXPECT_EQ(screenManager->stackDepth(), 2);

    // Go home
    screenManager->goHome();
    EXPECT_EQ(screenManager->currentScreen(), dashPtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);
}

TEST_F(ScreenTest, CompleteSettingsFlow) {
    createScreenManager();

    DisplayManager displayManager;
    displayManager.initialize();

    auto dashboard = std::make_unique<DashboardScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto settings = std::make_unique<SettingsScreen>(
        *screenManager, *themeManager, *deviceManager);
    auto displaySettings = std::make_unique<DisplaySettingsScreen>(
        *screenManager, *themeManager, displayManager);
    auto about = std::make_unique<AboutScreen>(*screenManager, *themeManager);

    auto* dashPtr = dashboard.get();

    screenManager->registerScreen("dashboard", std::move(dashboard));
    screenManager->registerScreen("settings", std::move(settings));
    screenManager->registerScreen("display_settings", std::move(displaySettings));
    screenManager->registerScreen("about", std::move(about));
    screenManager->setHomeScreen("dashboard");

    // Simulate complete settings exploration flow
    screenManager->showScreen("dashboard");

    // User opens settings via gear icon on dashboard
    screenManager->showScreen("settings");

    // User checks display settings
    screenManager->showScreen("display_settings");
    screenManager->update(100);  // Simulate UI update
    screenManager->goBack();

    // User checks about
    screenManager->showScreen("about");
    screenManager->update(100);  // Simulate UI update
    screenManager->goBack();

    // User goes back to dashboard
    screenManager->goBack();
    EXPECT_EQ(screenManager->currentScreen(), dashPtr);
    EXPECT_EQ(screenManager->stackDepth(), 0);
}

#endif // SMARTHUB_ENABLE_LVGL

// ============================================================================
// DisplayManager Unit Tests (No LVGL required)
// ============================================================================

#include <smarthub/ui/DisplayManager.hpp>
#include <filesystem>
#include <fstream>

class DisplayManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temp backlight directory for testing
        testBacklightPath = "/tmp/backlight_test_" + std::to_string(getpid());
        std::filesystem::create_directories(testBacklightPath);

        // Create max_brightness file
        std::ofstream maxFile(testBacklightPath + "/max_brightness");
        maxFile << "255";
        maxFile.close();

        // Create brightness file
        std::ofstream brightFile(testBacklightPath + "/brightness");
        brightFile << "200";
        brightFile.close();
    }

    void TearDown() override {
        std::filesystem::remove_all(testBacklightPath);
    }

    std::string testBacklightPath;
};

TEST_F(DisplayManagerTest, InitializeWithValidPath) {
    ui::DisplayManager manager;
    EXPECT_TRUE(manager.initialize(testBacklightPath));
    EXPECT_GT(manager.maxBrightness(), 0);
}

TEST_F(DisplayManagerTest, InitializeWithInvalidPath) {
    ui::DisplayManager manager;
    // Should still return true (graceful degradation)
    EXPECT_TRUE(manager.initialize("/nonexistent/backlight/path"));
}

TEST_F(DisplayManagerTest, DefaultBrightness) {
    ui::DisplayManager manager;
    // Before initialization, default should be DEFAULT_BRIGHTNESS
    EXPECT_EQ(manager.brightness(), ui::DisplayManager::DEFAULT_BRIGHTNESS);

    // After initialization, it reads current value from sysfs file
    // (test file has 200/255 = ~78%)
    manager.initialize(testBacklightPath);
    EXPECT_GE(manager.brightness(), 0);
    EXPECT_LE(manager.brightness(), 100);
}

TEST_F(DisplayManagerTest, SetBrightness) {
    ui::DisplayManager manager;
    manager.initialize(testBacklightPath);

    manager.setBrightness(50);
    EXPECT_EQ(manager.brightness(), 50);

    manager.setBrightness(100);
    EXPECT_EQ(manager.brightness(), 100);
}

TEST_F(DisplayManagerTest, BrightnessClamps) {
    ui::DisplayManager manager;
    manager.initialize(testBacklightPath);

    manager.setBrightness(-10);
    EXPECT_EQ(manager.brightness(), 0);

    manager.setBrightness(150);
    EXPECT_EQ(manager.brightness(), 100);
}

TEST_F(DisplayManagerTest, DefaultTimeout) {
    ui::DisplayManager manager;
    manager.initialize(testBacklightPath);
    EXPECT_EQ(manager.timeoutSeconds(), ui::DisplayManager::DEFAULT_TIMEOUT);
}

TEST_F(DisplayManagerTest, SetTimeout) {
    ui::DisplayManager manager;
    manager.initialize(testBacklightPath);

    manager.setTimeoutSeconds(120);
    EXPECT_EQ(manager.timeoutSeconds(), 120);

    manager.setTimeoutSeconds(0);  // Disable
    EXPECT_EQ(manager.timeoutSeconds(), 0);
}

TEST_F(DisplayManagerTest, TimeoutClamps) {
    ui::DisplayManager manager;
    manager.initialize(testBacklightPath);

    manager.setTimeoutSeconds(-10);
    EXPECT_EQ(manager.timeoutSeconds(), 0);
}

TEST_F(DisplayManagerTest, DimLevel) {
    ui::DisplayManager manager;
    manager.initialize(testBacklightPath);

    EXPECT_EQ(manager.dimLevel(), ui::DisplayManager::DEFAULT_DIM_LEVEL);

    manager.setDimLevel(30);
    EXPECT_EQ(manager.dimLevel(), 30);
}

TEST_F(DisplayManagerTest, DimLevelClamps) {
    ui::DisplayManager manager;
    manager.initialize(testBacklightPath);

    manager.setDimLevel(-10);
    EXPECT_EQ(manager.dimLevel(), 0);

    manager.setDimLevel(150);
    EXPECT_EQ(manager.dimLevel(), 100);
}

TEST_F(DisplayManagerTest, InitialState) {
    ui::DisplayManager manager;
    manager.initialize(testBacklightPath);

    EXPECT_FALSE(manager.isDimmed());
    EXPECT_FALSE(manager.isOff());
}

TEST_F(DisplayManagerTest, TimeoutDisabledNoStateChange) {
    ui::DisplayManager manager;
    manager.initialize(testBacklightPath);
    manager.setTimeoutSeconds(0);  // Disable timeout

    // Update for a long time - should not dim or turn off
    for (int i = 0; i < 1000; i++) {
        manager.update(100);
    }

    EXPECT_FALSE(manager.isDimmed());
    EXPECT_FALSE(manager.isOff());
}

TEST_F(DisplayManagerTest, TimeoutDimsThenOff) {
    ui::DisplayManager manager;
    manager.initialize(testBacklightPath);
    manager.setTimeoutSeconds(10);  // 10 second timeout

    // Initially not dimmed
    EXPECT_FALSE(manager.isDimmed());
    EXPECT_FALSE(manager.isOff());

    // Update just before dim threshold (10s - 5s dim delay = 5s)
    manager.update(4900);
    EXPECT_FALSE(manager.isDimmed());

    // Cross dim threshold
    manager.update(200);
    EXPECT_TRUE(manager.isDimmed());
    EXPECT_FALSE(manager.isOff());

    // Update to timeout
    manager.update(5000);
    EXPECT_TRUE(manager.isOff());
}

TEST_F(DisplayManagerTest, Wake) {
    ui::DisplayManager manager;
    manager.initialize(testBacklightPath);
    manager.setTimeoutSeconds(10);

    // Timeout the screen
    manager.update(15000);
    EXPECT_TRUE(manager.isOff());

    // Wake
    manager.wake();
    EXPECT_FALSE(manager.isDimmed());
    EXPECT_FALSE(manager.isOff());
}

TEST_F(DisplayManagerTest, WakeResetsBrightness) {
    ui::DisplayManager manager;
    manager.initialize(testBacklightPath);
    manager.setBrightness(75);
    manager.setTimeoutSeconds(10);

    // Timeout the screen
    manager.update(15000);
    EXPECT_TRUE(manager.isOff());

    // Wake - should restore original brightness
    manager.wake();
    EXPECT_EQ(manager.brightness(), 75);
}

TEST_F(DisplayManagerTest, SetScreenOnOff) {
    ui::DisplayManager manager;
    manager.initialize(testBacklightPath);

    manager.setScreenOn(false);
    EXPECT_TRUE(manager.isOff());
    EXPECT_FALSE(manager.isDimmed());

    manager.setScreenOn(true);
    EXPECT_FALSE(manager.isOff());
    EXPECT_FALSE(manager.isDimmed());
}

TEST_F(DisplayManagerTest, TimeoutCallback) {
    ui::DisplayManager manager;
    manager.initialize(testBacklightPath);
    manager.setTimeoutSeconds(10);

    bool callbackCalled = false;
    bool dimmedState = false;

    manager.setTimeoutCallback([&](bool dimmed) {
        callbackCalled = true;
        dimmedState = dimmed;
    });

    // Trigger dim
    manager.update(6000);  // Past dim threshold

    EXPECT_TRUE(callbackCalled);
    EXPECT_TRUE(dimmedState);

    // Reset and trigger wake
    callbackCalled = false;
    manager.wake();

    EXPECT_TRUE(callbackCalled);
    EXPECT_FALSE(dimmedState);
}

TEST_F(DisplayManagerTest, UpdateWithoutInitialize) {
    ui::DisplayManager manager;
    // Should not crash without initialization
    ASSERT_NO_THROW({
        manager.update(1000);
    });
}

TEST_F(DisplayManagerTest, Shutdown) {
    ui::DisplayManager manager;
    manager.initialize(testBacklightPath);
    manager.setBrightness(50);

    manager.shutdown();
    // After shutdown, screen should be at full brightness (restored)
    // But we can't easily verify this without reading the file
    // Just verify it doesn't crash
    ASSERT_NO_THROW({
        manager.shutdown();  // Double shutdown should be safe
    });
}

#ifdef SMARTHUB_ENABLE_LVGL

// ---------- Screens updated from other threads

namespace smarthub {
namespace ui {

// Reaches the callbacks these screens really register, without a radio or WiFi hardware
struct ScreenTestAccess {
    static void showPairingStep(AddDeviceScreen& screen) {
        screen.onProtocolSelected(3);  // Zigbee
        screen.showStep(3);
        screen.startZigbeePairing();
    }
    static EventHandler pairingEventHandler(AddDeviceScreen& screen) {
        return screen.makePairingEventHandler();
    }
    static std::function<void(DevicePtr)> pendingDeviceCallback(AddDeviceScreen& screen) {
        return screen.makePendingDeviceCallback();
    }
    static void showStep(AddDeviceScreen& screen, int step) { screen.showStep(step); }
    static lv_obj_t* statusLabel(AddDeviceScreen& screen) { return screen.m_pairStatusLabel; }
    static lv_obj_t* foundLabel(AddDeviceScreen& screen) { return screen.m_discoveredDeviceLabel; }
    static bool isPairing(AddDeviceScreen& screen) { return screen.m_isPairing; }
    static const std::string& address(AddDeviceScreen& screen) {
        return screen.m_discoveredIeeeAddress;
    }

    static network::NetworkManager::ScanCallback scanCallback(WifiSetupScreen& screen) {
        screen.m_scanning = true;
        return screen.makeScanCallback();
    }
    static network::NetworkManager::ConnectCallback connectCallback(WifiSetupScreen& screen) {
        return screen.makeConnectCallback();
    }
    static bool isScanning(WifiSetupScreen& screen) { return screen.m_scanning; }
    static const std::vector<network::WifiNetwork>& networks(WifiSetupScreen& screen) {
        return screen.m_networks;
    }
};

}  // namespace ui
}  // namespace smarthub

namespace {

// As a protocol or network thread would
template <typename Function>
void onAnotherThread(Function function) {
    std::thread thread(function);
    thread.join();
}

}  // namespace

TEST_F(ScreenTest, AddDeviceScreenHandlesPairingEventsOnTheUiThread) {
    createScreenManager();
    auto addDevice = std::make_unique<AddDeviceScreen>(
        *screenManager, *themeManager, *deviceManager, *eventBus);
    AddDeviceScreen* screen = addDevice.get();
    screenManager->registerScreen("add_device", std::move(addDevice));
    screenManager->showScreen("add_device");
    ScreenTestAccess::showPairingStep(*screen);
    lv_obj_t* status = ScreenTestAccess::statusLabel(*screen);
    const std::string before = lv_label_get_text(status);

    const EventHandler handler = ScreenTestAccess::pairingEventHandler(*screen);
    DeviceStateEvent detecting;
    detecting.property = "_detecting";
    onAnotherThread([&] { handler(detecting); });
    EXPECT_EQ(std::string(lv_label_get_text(status)), before);

    screenManager->update(16);
    EXPECT_STREQ(lv_label_get_text(status), "Device found! Getting details...");

    auto device = std::make_shared<SwitchDevice>("zigbee_0011223344556677", "Found");
    device->setState("manufacturer", "Acme");
    const auto pending = ScreenTestAccess::pendingDeviceCallback(*screen);
    onAnotherThread([&] { pending(device); });
    EXPECT_TRUE(ScreenTestAccess::address(*screen).empty());

    screenManager->update(16);
    EXPECT_EQ(ScreenTestAccess::address(*screen), "0x0011223344556677");
    EXPECT_NE(std::string(lv_label_get_text(ScreenTestAccess::foundLabel(*screen))).find("Acme"),
              std::string::npos);
}

TEST_F(ScreenTest, AddDeviceScreenStopsPairingWhenItsWidgetsGoAway) {
    createScreenManager();
    auto addDevice = std::make_unique<AddDeviceScreen>(
        *screenManager, *themeManager, *deviceManager, *eventBus);
    AddDeviceScreen* screen = addDevice.get();
    screenManager->registerScreen("add_device", std::move(addDevice));
    screenManager->showScreen("add_device");
    ScreenTestAccess::showPairingStep(*screen);
    ASSERT_TRUE(ScreenTestAccess::isPairing(*screen));

    // Queued before the widgets go, and run after
    DeviceStateEvent detecting;
    detecting.property = "_detecting";
    ScreenTestAccess::pairingEventHandler(*screen)(detecting);
    const auto pending = ScreenTestAccess::pendingDeviceCallback(*screen);
    pending(std::make_shared<SwitchDevice>("zigbee_0011223344556677", "Found"));

    ScreenTestAccess::showStep(*screen, 2);
    EXPECT_FALSE(ScreenTestAccess::isPairing(*screen));
    EXPECT_EQ(ScreenTestAccess::statusLabel(*screen), nullptr);

    EXPECT_NO_THROW(screenManager->update(16));
}

TEST_F(ScreenTest, AddDeviceScreenDestroyedWithQueuedEventsIgnoresThem) {
    createScreenManager();
    auto addDevice = std::make_unique<AddDeviceScreen>(
        *screenManager, *themeManager, *deviceManager, *eventBus);
    AddDeviceScreen* screen = addDevice.get();
    screenManager->registerScreen("add_device", std::move(addDevice));
    screenManager->showScreen("add_device");
    ScreenTestAccess::showPairingStep(*screen);

    const auto pending = ScreenTestAccess::pendingDeviceCallback(*screen);
    screenManager->unregisterScreen("add_device");

    pending(std::make_shared<SwitchDevice>("zigbee_0011223344556677", "Found"));
    EXPECT_NO_THROW(screenManager->update(16));
}

TEST_F(ScreenTest, WifiSetupScreenFinishesAScanOnTheUiThread) {
    createScreenManager();
    auto wifi = std::make_unique<WifiSetupScreen>(*screenManager, *themeManager, *networkManager);
    WifiSetupScreen* screen = wifi.get();
    screenManager->registerScreen("wifi_setup", std::move(wifi));
    screenManager->showScreen("wifi_setup");
    screenManager->update(16);

    network::WifiNetwork found;
    found.ssid = "Home";
    const auto callback = ScreenTestAccess::scanCallback(*screen);
    onAnotherThread([&] { callback({found}); });
    EXPECT_TRUE(ScreenTestAccess::isScanning(*screen));
    EXPECT_TRUE(ScreenTestAccess::networks(*screen).empty());

    screenManager->update(16);
    EXPECT_FALSE(ScreenTestAccess::isScanning(*screen));
    ASSERT_EQ(ScreenTestAccess::networks(*screen).size(), 1u);
    EXPECT_EQ(ScreenTestAccess::networks(*screen)[0].ssid, "Home");
}

TEST_F(ScreenTest, WifiSetupScreenIgnoresAnOutOfDateScan) {
    createScreenManager();
    auto wifi = std::make_unique<WifiSetupScreen>(*screenManager, *themeManager, *networkManager);
    WifiSetupScreen* screen = wifi.get();
    screenManager->registerScreen("wifi_setup", std::move(wifi));
    screenManager->showScreen("wifi_setup");
    screenManager->update(16);

    network::WifiNetwork old;
    old.ssid = "Old";
    const auto oldScan = ScreenTestAccess::scanCallback(*screen);
    const auto newScan = ScreenTestAccess::scanCallback(*screen);

    oldScan({old});
    screenManager->update(16);
    EXPECT_TRUE(ScreenTestAccess::isScanning(*screen));
    EXPECT_TRUE(ScreenTestAccess::networks(*screen).empty());

    newScan({});
    screenManager->update(16);
    EXPECT_FALSE(ScreenTestAccess::isScanning(*screen));
}

TEST_F(ScreenTest, WifiSetupScreenShowsAFailedConnection) {
    createScreenManager();
    auto wifi = std::make_unique<WifiSetupScreen>(*screenManager, *themeManager, *networkManager);
    WifiSetupScreen* screen = wifi.get();
    screenManager->registerScreen("wifi_setup", std::move(wifi));
    screenManager->showScreen("wifi_setup");
    screenManager->update(16);

    network::ConnectionResult result;
    result.success = false;
    result.error = "Wrong password";
    const auto callback = ScreenTestAccess::connectCallback(*screen);
    onAnotherThread([&] { callback(result); });
    screenManager->update(16);

    EXPECT_GE(test::countText(lv_layer_top(), "Wrong password"), 1);
}

#endif // SMARTHUB_ENABLE_LVGL
