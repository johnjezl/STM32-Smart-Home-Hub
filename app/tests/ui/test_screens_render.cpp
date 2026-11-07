/**
 * Renders every real screen against real LVGL, with no hardware.
 *
 * Set SMARTHUB_UI_FRAME_DIR to save each rendered screen as a PPM image.
 */

#include <gtest/gtest.h>

#include "headless_display.hpp"

#include <lvgl.h>

#include <smarthub/automation/Automation.hpp>
#include <smarthub/automation/AutomationManager.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/database/Database.hpp>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/devices/types/ColorLightDevice.hpp>
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
#include <smarthub/devices/types/AlarmSensor.hpp>
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
        automationManager->initialize();
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

        Automation automation;
        automation.id = "evening";
        automation.name = "Evening Lights";
        automation.description = "Lights on at dusk";
        automation.enabled = true;

        Trigger trigger;
        trigger.type = TriggerType::DeviceState;
        trigger.deviceId = "motion";
        trigger.property = "motion";
        trigger.toValue = true;
        automation.triggers.push_back(trigger);

        Action action;
        action.type = ActionType::SetDeviceState;
        action.deviceId = "lamp";
        action.property = "on";
        action.value = true;
        automation.actions.push_back(action);

        automationManager->addAutomation(automation);
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
            fs::create_directories(dir);
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

    EXPECT_GE(test::countText(root, "Living Room"), 1);
}

TEST_F(ScreenRenderTest, DeviceList) {
    auto* root = showAndRender("devices", std::make_unique<DeviceListScreen>(
        *screenManager, *themeManager, *deviceManager));

    EXPECT_GE(test::countText(root, "Reading Lamp"), 1);
}

TEST_F(ScreenRenderTest, RoomDetail) {
    auto* root = showAndRender("room_detail", std::make_unique<RoomDetailScreen>(
        *screenManager, *themeManager, *deviceManager),
        [](Screen& screen) {
            static_cast<RoomDetailScreen&>(screen).setRoom("living", "Living Room");
        });

    EXPECT_GE(test::countText(root, "Living Room"), 1);
    EXPECT_GE(test::countText(root, "Reading Lamp"), 1);
}

TEST_F(ScreenRenderTest, AlarmShowsOnTheDashboardSensorsAndRoom) {
    auto leak = std::make_shared<AlarmSensor>("leak", "Basement Leak", DeviceType::WaterSensor);
    leak->setRoom("alarm-room");
    auto multi = std::make_shared<MotionSensor>("multi", "Hall Multi");
    multi->setRoom("alarm-room");
    multi->setMotionDetected(true);
    multi->setState("temperature", 22.5);
    multi->setState("humidity", 45);
    multi->setState("illuminance", 120);
    ASSERT_TRUE(deviceManager->addDevice(leak));
    ASSERT_TRUE(deviceManager->addDevice(multi));
    leak->setState("alarm", false);

    auto* dashboard = showAndRender("dashboard", std::make_unique<DashboardScreen>(
        *screenManager, *themeManager, *deviceManager));
    EXPECT_EQ(test::countText(dashboard, "SmartHub", true), 1);

    leak->setState("alarm", true);
    auto* screen = screenManager->getScreen("dashboard");
    screen->onShow();
    test::render();
    EXPECT_EQ(test::countText(dashboard, "SmartHub", true), 0);
    EXPECT_EQ(test::countText(dashboard, LV_SYMBOL_WARNING " Leak: Basement Leak", true), 1);
    if (const char* dir = std::getenv("SMARTHUB_UI_FRAME_DIR")) {
        EXPECT_TRUE(test::saveFrame(std::string(dir) + "/alarm_dashboard.ppm"));
    }

    auto* sensors = showAndRender("sensors", std::make_unique<SensorListScreen>(
        *screenManager, *themeManager, *deviceManager));
    EXPECT_EQ(test::countText(sensors, "Leak", true), 1);
    EXPECT_EQ(test::countText(sensors, "Motion / 72.5°F / 45% / 120 lux", true), 1);

    auto* room = showAndRender("room_detail", std::make_unique<RoomDetailScreen>(
        *screenManager, *themeManager, *deviceManager),
        [](Screen& detail) { static_cast<RoomDetailScreen&>(detail).setRoom("alarm-room", "Basement"); });
    EXPECT_EQ(test::countText(room, "Alarm", true), 1);
    EXPECT_GE(test::countText(room, "Leak", true), 1);
}

TEST_F(ScreenRenderTest, LongAlarmStaysOnOneLineOfTheHeader) {
    for (int i = 0; i < 6; i++) {
        auto leak = std::make_shared<AlarmSensor>("leak" + std::to_string(i), "Sensor With A Long Name " + std::to_string(i),
                                                  DeviceType::WaterSensor);
        ASSERT_TRUE(deviceManager->addDevice(leak));
        leak->setState("alarm", true);
    }
    auto* root = showAndRender("dashboard", std::make_unique<DashboardScreen>(
        *screenManager, *themeManager, *deviceManager));

    const std::function<lv_obj_t*(lv_obj_t*)> findTitle = [&](lv_obj_t* obj) -> lv_obj_t* {
        if (lv_obj_check_type(obj, &lv_label_class) &&
            std::string(lv_label_get_text(obj)).rfind(LV_SYMBOL_WARNING, 0) == 0) {
            return obj;
        }
        for (uint32_t i = 0; i < lv_obj_get_child_cnt(obj); i++) {
            if (auto* found = findTitle(lv_obj_get_child(obj, static_cast<int32_t>(i)))) return found;
        }
        return nullptr;
    };
    lv_obj_t* title = findTitle(root);
    ASSERT_NE(title, nullptr);
    lv_obj_update_layout(title);
    EXPECT_EQ(lv_obj_get_height(title), lv_font_get_line_height(LV_FONT_DEFAULT));
    EXPECT_LE(lv_obj_get_x(title) + lv_obj_get_width(title), lv_obj_get_width(lv_obj_get_parent(title)) * 3 / 4 + 20);
}

TEST_F(ScreenRenderTest, RoomDetailShowsTheSensorsOfEachRoom) {
    auto motion = std::make_shared<MotionSensor>("detail-motion", "Hall Motion");
    motion->setRoom("detail-a");
    motion->setMotionDetected(false);
    auto temp = std::make_shared<TemperatureSensor>("detail-temp", "Hall Temp");
    temp->setRoom("detail-a");
    temp->setTemperature(20.0);
    auto silent = std::make_shared<TemperatureSensor>("detail-silent", "Attic Temp");
    silent->setRoom("detail-b");
    ASSERT_TRUE(deviceManager->addDevice(motion));
    ASSERT_TRUE(deviceManager->addDevice(temp));
    ASSERT_TRUE(deviceManager->addDevice(silent));

    auto* root = showAndRender("room_detail", std::make_unique<RoomDetailScreen>(
        *screenManager, *themeManager, *deviceManager),
        [](Screen& screen) {
            static_cast<RoomDetailScreen&>(screen).setRoom("detail-a", "Hall");
        });
    auto* screen = static_cast<RoomDetailScreen*>(screenManager->getScreen("room_detail"));

    const auto visible = [&](const char* text) { return test::countText(root, text, true); };
    EXPECT_EQ(visible("68°F"), 1);
    EXPECT_EQ(visible("68.0°F"), 1) << "the sensor's own row";
    EXPECT_EQ(visible("Clear"), 2) << "the panel and the sensor's own row";
    EXPECT_EQ(visible("Humidity"), 0);

    screen->setRoom("detail-b", "Attic");
    screen->onShow();
    test::render();
    EXPECT_EQ(visible("--°F"), 1) << "a sensor that hasn't reported, not 32°F";
    EXPECT_EQ(visible("Clear"), 0) << "no motion sensor here";

    screen->setRoom("detail-a", "Hall");
    screen->onShow();
    test::render();
    EXPECT_EQ(visible("Clear"), 2) << "the section has to come back for the next room";

    screen->setRoom("empty-room", "Empty");
    screen->onShow();
    test::render();
    EXPECT_EQ(visible("Temperature"), 0) << "no panel for a room without sensors";
}

TEST_F(ScreenRenderTest, LightControl) {
    auto* root = showAndRender("light_control", std::make_unique<LightControlScreen>(
        *screenManager, *themeManager, *deviceManager),
        [](Screen& screen) { static_cast<LightControlScreen&>(screen).setDeviceId("lamp"); });

    EXPECT_GE(test::countText(root, "Reading Lamp"), 1);
}

TEST_F(ScreenRenderTest, LightControlShowsTheControlsADeviceSupports) {
    auto showFor = [&](const std::string& deviceId) {
        const std::string name = "light_control_" + deviceId;
        return showAndRender(name, std::make_unique<LightControlScreen>(
            *screenManager, *themeManager, *deviceManager),
            [&](Screen& screen) { static_cast<LightControlScreen&>(screen).setDeviceId(deviceId); });
    };

    deviceManager->addDevice(std::make_shared<SwitchDevice>("plug", "Plug", "local"));
    deviceManager->addDevice(std::make_shared<ColorLightDevice>("color", "Color Light", "local"));

    EXPECT_EQ(test::countVisible(showFor("plug"), &lv_slider_class), 0);
    EXPECT_EQ(test::countVisible(showFor("lamp"), &lv_slider_class), 1);
    EXPECT_EQ(test::countVisible(showFor("color"), &lv_slider_class), 2);
}

TEST_F(ScreenRenderTest, SensorList) {
    auto* root = showAndRender("sensors", std::make_unique<SensorListScreen>(
        *screenManager, *themeManager, *deviceManager));

    EXPECT_GE(test::countText(root, "Hall Sensor"), 1);
}

TEST_F(ScreenRenderTest, SensorListIncludesSensorsWithHumidity) {
    deviceManager->addDevice(std::make_shared<TemperatureSensor>(
        "climate", "Climate Sensor", "local", "", true, false));

    auto* root = showAndRender("sensors", std::make_unique<SensorListScreen>(
        *screenManager, *themeManager, *deviceManager));

    EXPECT_GE(test::countText(root, "Climate Sensor"), 1);
}

TEST_F(ScreenRenderTest, SensorHistoryWithNothingRecordedSaysSoInsteadOfMakingItUp) {
    auto* root = showAndRender("sensor_history", std::make_unique<SensorHistoryScreen>(
        *screenManager, *themeManager, *deviceManager),
        [](Screen& screen) { static_cast<SensorHistoryScreen&>(screen).setSensorId("thermo"); });
    EXPECT_EQ(test::countText(root, "No history yet", true), 1);
}

TEST_F(ScreenRenderTest, SensorHistoryOfAMotionSensorSaysThereIsNone) {
    auto* root = showAndRender("sensor_history", std::make_unique<SensorHistoryScreen>(
        *screenManager, *themeManager, *deviceManager),
        [](Screen& screen) { static_cast<SensorHistoryScreen&>(screen).setSensorId("motion"); });
    EXPECT_EQ(test::countText(root, "No history for this sensor", true), 1);
}

TEST_F(ScreenRenderTest, SensorHistoryShowsWhatWasRecorded) {
    const int64_t now = static_cast<int64_t>(time(nullptr));
    for (int i = 0; i < 40; i++) {
        auto stmt = database->prepare(
            "INSERT INTO sensor_history (device_id, property, value, timestamp) "
            "VALUES ('thermo', 'temperature', ?, ?)");
        stmt->bind(1, 20.0 + (i % 10) * 0.2);
        stmt->bind(2, now - 86000 + i * 2000);
        ASSERT_TRUE(stmt->execute());
    }

    auto* root = showAndRender("sensor_history", std::make_unique<SensorHistoryScreen>(
        *screenManager, *themeManager, *deviceManager),
        [](Screen& screen) { static_cast<SensorHistoryScreen&>(screen).setSensorId("thermo"); });

    EXPECT_EQ(test::countText(root, "No history yet", true), 0);
    EXPECT_GE(test::countText(root, "Hall Sensor"), 1);
}

TEST_F(ScreenRenderTest, SensorHistory) {
    auto* root = showAndRender("sensor_history", std::make_unique<SensorHistoryScreen>(
        *screenManager, *themeManager, *deviceManager),
        [](Screen& screen) { static_cast<SensorHistoryScreen&>(screen).setSensorId("thermo"); });

    EXPECT_GE(test::countText(root, "Hall Sensor"), 1);
}

TEST_F(ScreenRenderTest, EditDevice) {
    auto* root = showAndRender("edit_device", std::make_unique<EditDeviceScreen>(
        *screenManager, *themeManager, *deviceManager),
        [](Screen& screen) { static_cast<EditDeviceScreen&>(screen).setDeviceId("lamp"); });

    EXPECT_GE(test::countText(root, "Edit Device"), 1);
    EXPECT_GE(test::countText(root, "Reading Lamp"), 1);
}

TEST_F(ScreenRenderTest, AddDevice) {
    auto* root = showAndRender("add_device", std::make_unique<AddDeviceScreen>(
        *screenManager, *themeManager, *deviceManager, *eventBus));

    EXPECT_GE(test::countText(root, "Add Device (1/4)"), 1);
    EXPECT_GE(test::countText(root, "Switch"), 1);
}

TEST_F(ScreenRenderTest, Notifications) {
    auto* root = showAndRender("notifications", std::make_unique<NotificationScreen>(
        *screenManager, *themeManager, *deviceManager));

    EXPECT_GE(test::countText(root, "Notifications"), 1);
}

TEST_F(ScreenRenderTest, Settings) {
    auto* root = showAndRender("settings", std::make_unique<SettingsScreen>(
        *screenManager, *themeManager, *deviceManager));

    EXPECT_GE(test::countText(root, "Settings"), 1);
    EXPECT_GE(test::countText(root, "Security"), 1);
    EXPECT_GE(test::countText(root, "Brightness, timeout"), 1);
}

TEST_F(ScreenRenderTest, DisplaySettings) {
    auto* root = showAndRender("display_settings", std::make_unique<DisplaySettingsScreen>(
        *screenManager, *themeManager, *displayManager));

    EXPECT_GE(test::countText(root, "Display Settings"), 1);
    EXPECT_GE(test::countText(root, "Brightness"), 1);
    // No theme choice, as it only reached screens built after it (#28)
    EXPECT_EQ(test::countText(root, "Theme"), 0);
    EXPECT_EQ(test::countText(root, "Light"), 0);
}

TEST_F(ScreenRenderTest, SecuritySettings) {
    auto* root = showAndRender("security_settings", std::make_unique<SecuritySettingsScreen>(
        *screenManager, *themeManager));

    EXPECT_GE(test::countText(root, "Security"), 1);
}

TEST_F(ScreenRenderTest, WifiSetup) {
    auto* root = showAndRender("wifi_setup", std::make_unique<WifiSetupScreen>(
        *screenManager, *themeManager, *networkManager));

    EXPECT_GE(test::countText(root, "WiFi Setup"), 1);
}

TEST_F(ScreenRenderTest, About) {
    auto* root = showAndRender("about", std::make_unique<AboutScreen>(*screenManager, *themeManager));

    EXPECT_GE(test::countText(root, "About"), 1);
}

TEST_F(ScreenRenderTest, AutomationList) {
    auto* root = showAndRender("automations", std::make_unique<AutomationListScreen>(
        *screenManager, *themeManager, *automationManager));

    EXPECT_GE(test::countText(root, "Automations"), 1);
    EXPECT_GE(test::countText(root, "Evening Lights"), 1);
}

TEST_F(ScreenRenderTest, AddAutomation) {
    auto* root = showAndRender("add_automation", std::make_unique<AddAutomationScreen>(
        *screenManager, *themeManager, *automationManager, *deviceManager));

    EXPECT_GE(test::countText(root, "Add Automation (Step 1/3)"), 1);
}

TEST_F(ScreenRenderTest, EditAutomation) {
    auto* root = showAndRender("edit_automation", std::make_unique<EditAutomationScreen>(
        *screenManager, *themeManager, *automationManager),
        [](Screen& screen) { static_cast<EditAutomationScreen&>(screen).setAutomationId("evening"); });

    EXPECT_GE(test::countText(root, "Edit Automation"), 1);
    EXPECT_GE(test::countText(root, "Evening Lights"), 1);
}

TEST_F(ScreenRenderTest, ThemeToggleKeepsScreensDrawing) {
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
        EXPECT_GT(test::frameColorCount(), MIN_COLORS);
        EXPECT_TRUE(screenManager->showScreen("about", TransitionType::None, false));
        screenManager->update(16);
        test::render();
        EXPECT_GT(test::frameColorCount(), MIN_COLORS);
    }
}
