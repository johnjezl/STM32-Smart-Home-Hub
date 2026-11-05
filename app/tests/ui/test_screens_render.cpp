/**
 * Renders every real screen against real LVGL, with no hardware.
 *
 * Set SMARTHUB_UI_FRAME_DIR to save each rendered screen as a PPM image.
 */

#include <gtest/gtest.h>

#include "headless_display.hpp"

#include <lvgl.h>

#include <smarthub/automation/AutomationManager.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/database/Database.hpp>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/devices/types/DimmerDevice.hpp>
#include <smarthub/devices/types/MotionSensor.hpp>
#include <smarthub/devices/types/SwitchDevice.hpp>
#include <smarthub/devices/types/TemperatureSensor.hpp>
#include <smarthub/network/NetworkManager.hpp>
#include <smarthub/ui/DisplayManager.hpp>
#include <smarthub/ui/ScreenManager.hpp>
#include <smarthub/ui/ThemeManager.hpp>
#include <smarthub/ui/UIManager.hpp>
#include <smarthub/ui/screens/AboutScreen.hpp>
#include <smarthub/ui/screens/AddAutomationScreen.hpp>
#include <smarthub/ui/screens/AddDeviceScreen.hpp>
#include <smarthub/ui/screens/AutomationListScreen.hpp>
#include <smarthub/ui/screens/DashboardScreen.hpp>
#include <smarthub/ui/screens/DeviceListScreen.hpp>
#include <smarthub/ui/screens/DisplaySettingsScreen.hpp>
#include <smarthub/ui/screens/EditAutomationScreen.hpp>
#include <smarthub/ui/screens/EditDeviceScreen.hpp>
#include <smarthub/ui/screens/LightControlScreen.hpp>
#include <smarthub/ui/screens/NotificationScreen.hpp>
#include <smarthub/ui/screens/RoomDetailScreen.hpp>
#include <smarthub/ui/screens/SecuritySettingsScreen.hpp>
#include <smarthub/ui/screens/SensorHistoryScreen.hpp>
#include <smarthub/ui/screens/SensorListScreen.hpp>
#include <smarthub/ui/screens/SettingsScreen.hpp>
#include <smarthub/ui/screens/WifiSetupScreen.hpp>

#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace smarthub;
using namespace smarthub::ui;

class ScreenRenderTest : public ::testing::Test {
protected:
    // Antialiased text alone produces more than this; a flat screen has one colour.
    static constexpr size_t MIN_COLORS = 10;

    void SetUp() override {
        dbPath = "/tmp/screen_render_test_" + std::to_string(getpid()) + ".db";
        fs::remove(dbPath);

        eventBus = std::make_unique<EventBus>();
        database = std::make_unique<Database>(dbPath);
        database->initialize();
        deviceManager = std::make_unique<DeviceManager>(*eventBus, *database);
        deviceManager->initialize();
        automationManager = std::make_unique<AutomationManager>(*eventBus, *database, *deviceManager);
        networkManager = std::make_unique<network::NetworkManager>();
        themeManager = std::make_unique<ThemeManager>();
        displayManager = std::make_unique<DisplayManager>();
        displayManager->initialize();
        uiManager = std::make_unique<UIManager>(*eventBus, *deviceManager, *database);
        screenManager = std::make_unique<ScreenManager>(*uiManager);
        screenManager->setTransitionDuration(0);

        addHomeData();
    }

    void TearDown() override {
        screenManager.reset();
        uiManager.reset();
        displayManager.reset();
        themeManager.reset();
        networkManager.reset();
        automationManager.reset();
        deviceManager.reset();
        database.reset();
        eventBus.reset();
        fs::remove(dbPath);
    }

    void addHomeData() {
        deviceManager->addRoom("living", "Living Room");

        lamp = std::make_shared<DimmerDevice>("lamp", "Reading Lamp", "local");
        lamp->setRoom("living");
        lamp->setState("on", true);
        deviceManager->addDevice(lamp);

        thermo = std::make_shared<TemperatureSensor>("thermo", "Hall Sensor", "local");
        thermo->setRoom("living");
        thermo->setState("temperature", 21.5);
        deviceManager->addDevice(thermo);

        motion = std::make_shared<MotionSensor>("motion", "Hall Motion", "local");
        motion->setRoom("living");
        deviceManager->addDevice(motion);
    }

    /** Register a screen, show it and draw it. Returns its container. */
    lv_obj_t* showAndRender(const std::string& name, std::unique_ptr<Screen> screen,
                            const std::function<void(Screen&)>& configure = nullptr) {
        Screen* raw = screen.get();
        screenManager->registerScreen(name, std::move(screen));
        if (configure) configure(*raw);

        const uint32_t flushesBefore = test::flushCount();
        EXPECT_TRUE(screenManager->showScreen(name));
        test::render();
        EXPECT_GT(test::flushCount(), flushesBefore) << "Nothing was drawn for " << name;
        EXPECT_GT(test::frameColorCount(), MIN_COLORS) << name << " rendered blank";

        if (const char* dir = std::getenv("SMARTHUB_UI_FRAME_DIR")) {
            EXPECT_TRUE(test::saveFrame(std::string(dir) + "/" + name + ".ppm"));
        }
        return raw->container();
    }

    std::string dbPath;
    std::unique_ptr<EventBus> eventBus;
    std::unique_ptr<Database> database;
    std::unique_ptr<DeviceManager> deviceManager;
    std::unique_ptr<AutomationManager> automationManager;
    std::unique_ptr<network::NetworkManager> networkManager;
    std::unique_ptr<ThemeManager> themeManager;
    std::unique_ptr<DisplayManager> displayManager;
    std::unique_ptr<UIManager> uiManager;
    std::unique_ptr<ScreenManager> screenManager;
    DevicePtr lamp;
    DevicePtr thermo;
    DevicePtr motion;
};

TEST_F(ScreenRenderTest, Dashboard) {
    auto* root = showAndRender("dashboard", std::make_unique<DashboardScreen>(
        *screenManager, *themeManager, *deviceManager));

    EXPECT_GE(test::countLabels(root, "Living Room"), 1);
}

TEST_F(ScreenRenderTest, DeviceList) {
    auto* root = showAndRender("devices", std::make_unique<DeviceListScreen>(
        *screenManager, *themeManager, *deviceManager));

    EXPECT_GE(test::countLabels(root, "Reading Lamp"), 1);
}

TEST_F(ScreenRenderTest, RoomDetail) {
    auto* root = showAndRender("room_detail", std::make_unique<RoomDetailScreen>(
        *screenManager, *themeManager, *deviceManager),
        [](Screen& screen) {
            static_cast<RoomDetailScreen&>(screen).setRoom("living", "Living Room");
        });

    EXPECT_GE(test::countLabels(root, "Living Room"), 1);
    EXPECT_GE(test::countLabels(root, "Reading Lamp"), 1);
}

TEST_F(ScreenRenderTest, LightControl) {
    auto* root = showAndRender("light_control", std::make_unique<LightControlScreen>(
        *screenManager, *themeManager, *deviceManager),
        [](Screen& screen) { static_cast<LightControlScreen&>(screen).setDeviceId("lamp"); });

    EXPECT_GE(test::countLabels(root, "Reading Lamp"), 1);
}

TEST_F(ScreenRenderTest, SensorList) {
    auto* root = showAndRender("sensors", std::make_unique<SensorListScreen>(
        *screenManager, *themeManager, *deviceManager));

    EXPECT_GE(test::countLabels(root, "Hall Sensor"), 1);
}

TEST_F(ScreenRenderTest, SensorHistory) {
    auto* root = showAndRender("sensor_history", std::make_unique<SensorHistoryScreen>(
        *screenManager, *themeManager, *deviceManager),
        [](Screen& screen) { static_cast<SensorHistoryScreen&>(screen).setSensorId("thermo"); });

    EXPECT_GE(test::countLabels(root, "Hall Sensor"), 1);
}

TEST_F(ScreenRenderTest, EditDevice) {
    showAndRender("edit_device", std::make_unique<EditDeviceScreen>(
        *screenManager, *themeManager, *deviceManager),
        [](Screen& screen) { static_cast<EditDeviceScreen&>(screen).setDeviceId("lamp"); });
}

TEST_F(ScreenRenderTest, AddDevice) {
    showAndRender("add_device", std::make_unique<AddDeviceScreen>(
        *screenManager, *themeManager, *deviceManager, *eventBus));
}

TEST_F(ScreenRenderTest, Notifications) {
    showAndRender("notifications", std::make_unique<NotificationScreen>(
        *screenManager, *themeManager, *deviceManager));
}

TEST_F(ScreenRenderTest, Settings) {
    showAndRender("settings", std::make_unique<SettingsScreen>(
        *screenManager, *themeManager, *deviceManager));
}

TEST_F(ScreenRenderTest, DisplaySettings) {
    showAndRender("display_settings", std::make_unique<DisplaySettingsScreen>(
        *screenManager, *themeManager, *displayManager));
}

TEST_F(ScreenRenderTest, SecuritySettings) {
    showAndRender("security_settings", std::make_unique<SecuritySettingsScreen>(
        *screenManager, *themeManager));
}

TEST_F(ScreenRenderTest, WifiSetup) {
    showAndRender("wifi_setup", std::make_unique<WifiSetupScreen>(
        *screenManager, *themeManager, *networkManager));
}

TEST_F(ScreenRenderTest, About) {
    showAndRender("about", std::make_unique<AboutScreen>(*screenManager, *themeManager));
}

TEST_F(ScreenRenderTest, AutomationList) {
    showAndRender("automations", std::make_unique<AutomationListScreen>(
        *screenManager, *themeManager, *automationManager));
}

TEST_F(ScreenRenderTest, AddAutomation) {
    showAndRender("add_automation", std::make_unique<AddAutomationScreen>(
        *screenManager, *themeManager, *automationManager, *deviceManager));
}

TEST_F(ScreenRenderTest, EditAutomation) {
    showAndRender("edit_automation", std::make_unique<EditAutomationScreen>(
        *screenManager, *themeManager, *automationManager));
}

// Issue #28: re-applying the theme leaves every screen flat, for the rest of the
// process. While this is broken it must stay disabled or later tests fail too.
TEST_F(ScreenRenderTest, DISABLED_ThemeToggleKeepsScreensDrawing) {
    themeManager->toggle();
    themeManager->toggle();

    showAndRender("about", std::make_unique<AboutScreen>(*screenManager, *themeManager));
}

TEST_F(ScreenRenderTest, ScreenSurvivesShowHideAndUpdates) {
    showAndRender("dashboard", std::make_unique<DashboardScreen>(
        *screenManager, *themeManager, *deviceManager));
    showAndRender("about", std::make_unique<AboutScreen>(*screenManager, *themeManager));

    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(screenManager->showScreen("dashboard", TransitionType::None, false));
        screenManager->update(16);
        test::render();
        EXPECT_TRUE(screenManager->showScreen("about", TransitionType::None, false));
        screenManager->update(16);
        test::render();
    }
}
