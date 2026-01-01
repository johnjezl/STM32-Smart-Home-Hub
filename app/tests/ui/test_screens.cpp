/**
 * Screen Unit Tests (Phase 8.C + 8.D + 8.E)
 *
 * Tests for DeviceListScreen, LightControlScreen, SensorListScreen,
 * SensorHistoryScreen, and WifiSetupScreen.
 * LVGL-specific rendering tested on hardware; these tests focus on
 * screen registration, navigation, and data handling.
 */

#include <gtest/gtest.h>

#ifdef SMARTHUB_ENABLE_LVGL

#include <smarthub/ui/screens/DashboardScreen.hpp>
#include <smarthub/ui/screens/DeviceListScreen.hpp>
#include <smarthub/ui/screens/LightControlScreen.hpp>
#include <smarthub/ui/screens/SensorListScreen.hpp>
#include <smarthub/ui/screens/SensorHistoryScreen.hpp>
#include <smarthub/ui/screens/WifiSetupScreen.hpp>
#include <smarthub/ui/ScreenManager.hpp>
#include <smarthub/ui/ThemeManager.hpp>
#include <smarthub/ui/UIManager.hpp>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/devices/Device.hpp>
#include <smarthub/network/NetworkManager.hpp>
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
        networkManager = std::make_unique<network::NetworkManager>();
        uiManager = std::make_unique<UIManager>(*eventBus, *deviceManager);

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
// SensorHistoryScreen Tests (Phase 8.D)
// ============================================================================

TEST_F(ScreenTest, SensorHistoryScreenRegistration) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto sensorHistory = std::make_unique<SensorHistoryScreen>(
        *screenManager, *themeManager, *deviceManager);

    EXPECT_EQ(sensorHistory->name(), "sensor_history");

    screenManager->registerScreen("sensor_history", std::move(sensorHistory));
    EXPECT_TRUE(screenManager->hasScreen("sensor_history"));
}

TEST_F(ScreenTest, SensorHistoryScreenSensorId) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto sensorHistory = std::make_unique<SensorHistoryScreen>(
        *screenManager, *themeManager, *deviceManager);

    // Set sensor ID before showing screen
    sensorHistory->setSensorId("temp_sensor_001");
    EXPECT_EQ(sensorHistory->sensorId(), "temp_sensor_001");

    screenManager->registerScreen("sensor_history", std::move(sensorHistory));
    EXPECT_TRUE(screenManager->hasScreen("sensor_history"));
}

TEST_F(ScreenTest, SensorHistoryScreenNavigation) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

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
    screenManager = std::make_unique<ScreenManager>(*uiManager);

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
    screenManager = std::make_unique<ScreenManager>(*uiManager);

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
    screenManager = std::make_unique<ScreenManager>(*uiManager);

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

TEST_F(ScreenTest, FullNavigationFlowWithSensorHistory) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

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

TEST_F(ScreenTest, CompleteAppNavigationFlow) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

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
    EXPECT_EQ(screenManager->stackDepth(), 4);  // dashboard → devices → sensors → history

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

// ============================================================================
// WifiSetupScreen Tests (Phase 8.E)
// ============================================================================

TEST_F(ScreenTest, WifiSetupScreenRegistration) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto wifiSetup = std::make_unique<WifiSetupScreen>(
        *screenManager, *themeManager, *networkManager);

    EXPECT_EQ(wifiSetup->name(), "wifi_setup");

    screenManager->registerScreen("wifi_setup", std::move(wifiSetup));
    EXPECT_TRUE(screenManager->hasScreen("wifi_setup"));
}

TEST_F(ScreenTest, WifiSetupScreenNavigation) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

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
    screenManager = std::make_unique<ScreenManager>(*uiManager);

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
    screenManager = std::make_unique<ScreenManager>(*uiManager);

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
    screenManager = std::make_unique<ScreenManager>(*uiManager);

    auto wifiSetup = std::make_unique<WifiSetupScreen>(
        *screenManager, *themeManager, *networkManager);

    EXPECT_EQ(wifiSetup->name(), "wifi_setup");
}

TEST_F(ScreenTest, WifiSetupScreenAutoRefresh) {
    screenManager = std::make_unique<ScreenManager>(*uiManager);

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
    screenManager = std::make_unique<ScreenManager>(*uiManager);

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
    screenManager = std::make_unique<ScreenManager>(*uiManager);

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
