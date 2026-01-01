/**
 * Light Control Screen
 *
 * Controls for dimmable and color lights.
 */
#pragma once

#include "smarthub/ui/Screen.hpp"
#include <memory>
#include <string>

namespace smarthub {

class DeviceManager;

namespace ui {

class ThemeManager;

/**
 * Light control screen with brightness and color controls
 */
class LightControlScreen : public Screen {
public:
    LightControlScreen(ScreenManager& screenManager,
                       ThemeManager& theme,
                       DeviceManager& deviceManager);
    ~LightControlScreen() override;

    /**
     * Set the device to control
     */
    void setDeviceId(const std::string& deviceId);

    void onCreate() override;
    void onShow() override;
    void onUpdate(uint32_t deltaMs) override;
    void onDestroy() override;

private:
#ifdef SMARTHUB_ENABLE_LVGL
    void createHeader();
    void createContent();
    void updateControls();

    void onPowerToggle(bool on);
    void onBrightnessChange(int percent);
    void onColorTempChange(int kelvin);
    void onBackClicked();

    static void powerToggleHandler(lv_event_t* e);
    static void brightnessSliderHandler(lv_event_t* e);
    static void colorTempSliderHandler(lv_event_t* e);
    static void backButtonHandler(lv_event_t* e);

    lv_obj_t* m_content = nullptr;
    lv_obj_t* m_backBtn = nullptr;
    lv_obj_t* m_titleLabel = nullptr;
    lv_obj_t* m_powerSwitch = nullptr;
    lv_obj_t* m_brightnessSlider = nullptr;
    lv_obj_t* m_brightnessLabel = nullptr;
    lv_obj_t* m_colorTempSlider = nullptr;
    lv_obj_t* m_colorTempLabel = nullptr;
    lv_obj_t* m_previewCircle = nullptr;
#endif

    ThemeManager& m_theme;
    DeviceManager& m_deviceManager;
    std::string m_deviceId;
};

} // namespace ui
} // namespace smarthub
