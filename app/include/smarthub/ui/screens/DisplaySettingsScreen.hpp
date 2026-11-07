/**
 * Display Settings Screen
 *
 * Settings for display brightness and screen timeout.
 *
 * There's no theme choice: screens take their colours when they're built, so a change would
 * only reach screens built afterwards (#28).
 */
#pragma once

#include "smarthub/ui/Screen.hpp"
#include <memory>
#include <string>

namespace smarthub {
namespace ui {

class ThemeManager;
class DisplayManager;

/**
 * Display Settings Screen
 *
 * Layout (800x480):
 * +---------------------------------------------------------------+
 * |  < Display Settings                                           |
 * +---------------------------------------------------------------+
 * |  +----------------------------------------------------------+ |
 * |  | Brightness                                          80%  | |
 * |  | [=========================-------]                       | |
 * |  +----------------------------------------------------------+ |
 * |  | Screen Timeout                                    1 min  | |
 * |  | [Never] [30s] [1m] [5m] [10m]                            | |
 * |  +----------------------------------------------------------+ |
 * +---------------------------------------------------------------+
 */
class DisplaySettingsScreen : public Screen {
public:
    DisplaySettingsScreen(ScreenManager& screenManager,
                          ThemeManager& theme,
                          DisplayManager& displayManager);
    ~DisplaySettingsScreen() override;

    void onCreate() override;
    void onShow() override;
    void onUpdate(uint32_t deltaMs) override;
    void onDestroy() override;

private:
#ifdef SMARTHUB_ENABLE_LVGL
    void createHeader();
    void createBrightnessSection();
    void createTimeoutSection();

    void updateBrightnessLabel();
    void updateTimeoutButtons();

    void onBrightnessChanged(int percent);
    void onTimeoutSelected(int seconds);
    void onBackClicked();

    static void brightnessSliderHandler(lv_event_t* e);
    static void timeoutButtonHandler(lv_event_t* e);
    static void backButtonHandler(lv_event_t* e);

    lv_obj_t* m_backBtn = nullptr;
    lv_obj_t* m_titleLabel = nullptr;
    lv_obj_t* m_content = nullptr;

    // Brightness
    lv_obj_t* m_brightnessSlider = nullptr;
    lv_obj_t* m_brightnessLabel = nullptr;

    // Timeout
    lv_obj_t* m_timeoutButtons[5] = {nullptr};
    int m_timeoutValues[5] = {0, 30, 60, 300, 600};  // Never, 30s, 1m, 5m, 10m
#endif

    ThemeManager& m_theme;
    DisplayManager& m_displayManager;
};

} // namespace ui
} // namespace smarthub
