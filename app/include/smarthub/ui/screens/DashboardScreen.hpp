/**
 * Dashboard Screen
 *
 * Main home screen showing room grid with device summaries.
 */
#pragma once

#include "smarthub/ui/Screen.hpp"
#include <memory>
#include <vector>
#include <string>

namespace smarthub {

class DeviceManager;

namespace ui {

class ThemeManager;
class Header;
class NavBar;
class RoomCard;
struct RoomData;

/**
 * Dashboard home screen
 *
 * Layout (800x480):
 * +---------------------------------------------------------------+
 * |  SmartHub                                    🔔  ⚙️  12:34 PM |
 * +---------------------------------------------------------------+
 * |                                                               |
 * |  [Room Cards in scrollable grid]                              |
 * |                                                               |
 * +---------------------------------------------------------------+
 * |    [🏠 Home]    [💡 Devices]    [📊 Sensors]    [⚙️ Settings] |
 * +---------------------------------------------------------------+
 */
class DashboardScreen : public Screen {
public:
    DashboardScreen(ScreenManager& screenManager,
                    ThemeManager& theme,
                    DeviceManager& deviceManager);
    ~DashboardScreen() override;

    void onCreate() override;
    void onShow() override;
    void onUpdate(uint32_t deltaMs) override;
    void onHide() override;
    void onDestroy() override;

private:
#ifdef SMARTHUB_ENABLE_LVGL
    void createHeader();
    void createContent();
    void createNavBar();
    void createRoomCards();
    void updateRoomCards();
    void updateClock();

    void onRoomClicked(const std::string& roomId);
    void onNavTabSelected(const std::string& tabId);
    void onSettingsClicked();
    void onNotificationClicked();

    lv_obj_t* m_content = nullptr;

    std::unique_ptr<Header> m_header;
    std::unique_ptr<NavBar> m_navBar;
    std::vector<std::unique_ptr<RoomCard>> m_roomCards;
#endif

    ThemeManager& m_theme;
    DeviceManager& m_deviceManager;

    uint32_t m_clockUpdateMs = 0;
    static constexpr uint32_t CLOCK_UPDATE_INTERVAL = 60000;  // 1 minute
};

} // namespace ui
} // namespace smarthub
