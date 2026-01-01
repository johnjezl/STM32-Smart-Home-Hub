/**
 * Device List Screen
 *
 * Shows all devices in a scrollable list with quick toggle controls.
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

/**
 * Device list screen showing all registered devices
 */
class DeviceListScreen : public Screen {
public:
    DeviceListScreen(ScreenManager& screenManager,
                     ThemeManager& theme,
                     DeviceManager& deviceManager);
    ~DeviceListScreen() override;

    void onCreate() override;
    void onShow() override;
    void onUpdate(uint32_t deltaMs) override;
    void onDestroy() override;

private:
#ifdef SMARTHUB_ENABLE_LVGL
    void createHeader();
    void createContent();
    void createNavBar();
    void createDeviceList();
    void refreshDeviceList();

    lv_obj_t* createDeviceRow(lv_obj_t* parent, const std::string& deviceId,
                               const std::string& name, const std::string& type,
                               bool isOn);

    void onDeviceToggle(const std::string& deviceId, bool newState);
    void onDeviceClicked(const std::string& deviceId);
    void onBackClicked();
    void onNavTabSelected(const std::string& tabId);

    static void toggleHandler(lv_event_t* e);
    static void rowClickHandler(lv_event_t* e);

    lv_obj_t* m_content = nullptr;
    lv_obj_t* m_deviceList = nullptr;

    std::unique_ptr<Header> m_header;
    std::unique_ptr<NavBar> m_navBar;

    // Map device ID to row widget for updates
    std::vector<std::pair<std::string, lv_obj_t*>> m_deviceRows;
#endif

    ThemeManager& m_theme;
    DeviceManager& m_deviceManager;
};

} // namespace ui
} // namespace smarthub
