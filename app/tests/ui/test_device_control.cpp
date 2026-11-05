/**
 * Touch controls send commands through DeviceManager, and go back to the real state
 * when a command fails.
 */

#include <gtest/gtest.h>

#include "../mocks/MockProtocolHandler.hpp"
#include "headless_display.hpp"

#include <lvgl.h>

#include <smarthub/core/EventBus.hpp>
#include <smarthub/database/Database.hpp>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/devices/types/DimmerDevice.hpp>
#include <smarthub/ui/ScreenManager.hpp>
#include <smarthub/ui/ThemeManager.hpp>
#include <smarthub/ui/UIManager.hpp>
#include <smarthub/ui/screens/DeviceListScreen.hpp>
#include <smarthub/ui/screens/LightControlScreen.hpp>
#include <smarthub/ui/screens/RoomDetailScreen.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <unistd.h>

using namespace smarthub;
using namespace smarthub::ui;

class DeviceControlTest : public ::testing::Test {
protected:
    static constexpr const char* PROTOCOL = "ui_control_test";

    void SetUp() override {
        dbPath = "/tmp/device_control_test_" + std::to_string(getpid()) + ".db";
        std::filesystem::remove(dbPath);

        database = std::make_unique<Database>(dbPath);
        database->initialize();
        deviceManager = std::make_unique<DeviceManager>(eventBus, *database);
        deviceManager->initialize();

        ProtocolFactory::instance().registerProtocol(PROTOCOL,
            [this](EventBus& bus, const nlohmann::json& config) -> ProtocolHandlerPtr {
                protocol = std::make_shared<smarthub::testing::MockProtocolHandler>(bus, config);
                return protocol;
            });
        ASSERT_TRUE(deviceManager->loadProtocol(PROTOCOL, nlohmann::json::object()));

        deviceManager->addRoom("living", "Living Room");
        lamp = std::make_shared<DimmerDevice>("lamp", "Reading Lamp", PROTOCOL);
        lamp->setRoom("living");
        deviceManager->addDevice(lamp);

        uiManager = std::make_unique<UIManager>(eventBus, *deviceManager, *database);
        screenManager = std::make_unique<ScreenManager>(*uiManager);
        screenManager->setTransitionDuration(0);
    }

    void TearDown() override {
        screenManager.reset();
        uiManager.reset();
        deviceManager.reset();
        database.reset();
        ProtocolFactory::instance().unregisterProtocol(PROTOCOL);
        std::filesystem::remove(dbPath);
    }

    lv_obj_t* show(const std::string& name, std::unique_ptr<Screen> screen) {
        Screen* raw = screen.get();
        screenManager->registerScreen(name, std::move(screen));
        EXPECT_TRUE(screenManager->showScreen(name));
        test::render();
        return raw->container();
    }

    /** What a tap on a switch does. */
    void tap(lv_obj_t* sw) {
        if (lv_obj_has_state(sw, LV_STATE_CHECKED)) {
            lv_obj_clear_state(sw, LV_STATE_CHECKED);
        } else {
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        }
        lv_event_send(sw, LV_EVENT_VALUE_CHANGED, nullptr);
    }

    void expectToggleControlsTheDevice(lv_obj_t* root) {
        lv_obj_t* sw = test::findFirst(root, &lv_switch_class);
        ASSERT_NE(sw, nullptr);

        tap(sw);

        EXPECT_EQ(protocol->commandCount, 1);
        EXPECT_EQ(protocol->lastCommand, "on");
        EXPECT_EQ(protocol->lastCommandAddress, "lamp");
        EXPECT_EQ(lamp->getProperty("on"), true);
        EXPECT_TRUE(lv_obj_has_state(sw, LV_STATE_CHECKED));
    }

    void expectFailedToggleGoesBack(lv_obj_t* root) {
        lv_obj_t* sw = test::findFirst(root, &lv_switch_class);
        ASSERT_NE(sw, nullptr);
        protocol->commandResult = false;

        tap(sw);

        EXPECT_EQ(protocol->commandCount, 1);
        EXPECT_EQ(lamp->getProperty("on"), false);
        EXPECT_FALSE(lv_obj_has_state(sw, LV_STATE_CHECKED));
    }

    std::string dbPath;
    EventBus eventBus;
    ThemeManager themeManager;
    std::unique_ptr<Database> database;
    std::unique_ptr<DeviceManager> deviceManager;
    std::unique_ptr<UIManager> uiManager;
    std::unique_ptr<ScreenManager> screenManager;
    std::shared_ptr<smarthub::testing::MockProtocolHandler> protocol;
    DevicePtr lamp;
};

TEST_F(DeviceControlTest, DeviceListToggle) {
    expectToggleControlsTheDevice(show("devices", std::make_unique<DeviceListScreen>(
        *screenManager, themeManager, *deviceManager)));
}

TEST_F(DeviceControlTest, DeviceListToggleGoesBackOnFailure) {
    expectFailedToggleGoesBack(show("devices", std::make_unique<DeviceListScreen>(
        *screenManager, themeManager, *deviceManager)));
}

TEST_F(DeviceControlTest, RoomDetailToggle) {
    auto screen = std::make_unique<RoomDetailScreen>(*screenManager, themeManager, *deviceManager);
    screen->setRoom("living", "Living Room");
    expectToggleControlsTheDevice(show("room_detail", std::move(screen)));
}

TEST_F(DeviceControlTest, RoomDetailToggleGoesBackOnFailure) {
    auto screen = std::make_unique<RoomDetailScreen>(*screenManager, themeManager, *deviceManager);
    screen->setRoom("living", "Living Room");
    expectFailedToggleGoesBack(show("room_detail", std::move(screen)));
}

class LightControlTest : public DeviceControlTest {
protected:
    lv_obj_t* showLightControl() {
        auto screen = std::make_unique<LightControlScreen>(*screenManager, themeManager, *deviceManager);
        screen->setDeviceId("lamp");
        return show("light_control", std::move(screen));
    }
};

TEST_F(LightControlTest, PowerSwitch) {
    expectToggleControlsTheDevice(showLightControl());
}

TEST_F(LightControlTest, PowerSwitchGoesBackOnFailure) {
    expectFailedToggleGoesBack(showLightControl());
}

TEST_F(LightControlTest, SliderOnlySendsWhenReleased) {
    lv_obj_t* slider = test::findFirst(showLightControl(), &lv_slider_class);
    ASSERT_NE(slider, nullptr);

    for (int percent : {10, 20, 30}) {
        lv_slider_set_value(slider, percent, LV_ANIM_OFF);
        lv_event_send(slider, LV_EVENT_VALUE_CHANGED, nullptr);
    }
    EXPECT_EQ(protocol->commandCount, 0);

    lv_event_send(slider, LV_EVENT_RELEASED, nullptr);

    EXPECT_EQ(protocol->commandCount, 1);
    EXPECT_EQ(protocol->lastCommand, "brightness");
    EXPECT_EQ(lamp->getProperty("brightness"), 30);
}

TEST_F(LightControlTest, SliderGoesBackOnFailure) {
    lv_obj_t* slider = test::findFirst(showLightControl(), &lv_slider_class);
    ASSERT_NE(slider, nullptr);
    const int before = lv_slider_get_value(slider);
    protocol->commandResult = false;

    lv_slider_set_value(slider, before + 25, LV_ANIM_OFF);
    lv_event_send(slider, LV_EVENT_RELEASED, nullptr);

    EXPECT_EQ(lv_slider_get_value(slider), before);
    EXPECT_NE(lamp->getProperty("brightness"), before + 25);
}
