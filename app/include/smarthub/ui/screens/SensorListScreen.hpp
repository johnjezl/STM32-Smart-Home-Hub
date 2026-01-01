/**
 * Sensor List Screen
 *
 * Shows all sensors with current readings.
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
 * Sensor list screen showing all sensors and their readings
 */
class SensorListScreen : public Screen {
public:
    SensorListScreen(ScreenManager& screenManager,
                     ThemeManager& theme,
                     DeviceManager& deviceManager);
    ~SensorListScreen() override;

    void onCreate() override;
    void onShow() override;
    void onUpdate(uint32_t deltaMs) override;
    void onDestroy() override;

private:
#ifdef SMARTHUB_ENABLE_LVGL
    void createHeader();
    void createContent();
    void createNavBar();
    void createSensorCards();
    void refreshSensorValues();

    void onSensorClicked(const std::string& sensorId);
    void onNavTabSelected(const std::string& tabId);

    lv_obj_t* m_content = nullptr;

    std::unique_ptr<Header> m_header;
    std::unique_ptr<NavBar> m_navBar;

    struct SensorCard {
        std::string id;
        lv_obj_t* card;
        lv_obj_t* valueLabel;
    };
    std::vector<SensorCard> m_sensorCards;
#endif

    ThemeManager& m_theme;
    DeviceManager& m_deviceManager;

    uint32_t m_updateMs = 0;
    static constexpr uint32_t UPDATE_INTERVAL = 5000;  // 5 seconds
};

} // namespace ui
} // namespace smarthub
