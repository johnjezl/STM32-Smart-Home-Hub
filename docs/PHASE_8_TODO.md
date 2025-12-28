# Phase 8: UI Refinement & Polish - Detailed TODO

## Overview
**Duration**: 2-3 weeks  
**Objective**: Create a polished, user-friendly touch interface.  
**Prerequisites**: Phases 4, 5, 6 complete

---

## 8.1 Screen Architecture

### 8.1.1 Screen Manager
```cpp
// app/include/smarthub/ui/ScreenManager.hpp
#pragma once

#include <map>
#include <stack>
#include <memory>
#include <string>

namespace smarthub::ui {

class Screen;

class ScreenManager {
public:
    void registerScreen(const std::string& name, std::unique_ptr<Screen> screen);
    
    void showScreen(const std::string& name);
    void goBack();
    void goHome();
    
    Screen* currentScreen();
    
private:
    std::map<std::string, std::unique_ptr<Screen>> m_screens;
    std::stack<std::string> m_history;
    std::string m_currentScreen;
};

} // namespace smarthub::ui
```

### 8.1.2 Screen Base Class
```cpp
// app/include/smarthub/ui/Screen.hpp
#pragma once

#include <lvgl.h>
#include <string>

namespace smarthub::ui {

class UIManager;

class Screen {
public:
    Screen(UIManager& uiManager, const std::string& name);
    virtual ~Screen();
    
    virtual void onCreate() = 0;
    virtual void onShow() {}
    virtual void onHide() {}
    virtual void onDestroy() {}
    virtual void onUpdate() {}  // Called each frame
    
    const std::string& name() const { return m_name; }
    lv_obj_t* container() const { return m_container; }
    
protected:
    UIManager& m_uiManager;
    std::string m_name;
    lv_obj_t* m_container = nullptr;
};

} // namespace smarthub::ui
```

---

## 8.2 Dashboard Home Screen

### 8.2.1 Dashboard Layout (480x800 Portrait)
```
+----------------------------------+
|  SmartHub          🔔  ⚙️  12:34 |  <- Header (50px)
+----------------------------------+
|                                  |
|  +------------+ +------------+   |
|  | Living Rm  | | Bedroom    |   |  <- Room Cards
|  | 72°F  💡3  | | 68°F  💡2  |   |     (scrollable grid)
|  +------------+ +------------+   |
|                                  |
|  +------------+ +------------+   |
|  | Kitchen    | | Bathroom   |   |
|  | 74°F  💡1  | | 70°F  💡0  |   |
|  +------------+ +------------+   |
|                                  |
+----------------------------------+
|  [Devices] [Sensors] [Settings]  |  <- Navigation (60px)
+----------------------------------+
```

### 8.2.2 Dashboard Implementation
```cpp
// app/include/smarthub/ui/screens/DashboardScreen.hpp
#pragma once

#include <smarthub/ui/Screen.hpp>
#include <vector>

namespace smarthub::ui {

class DashboardScreen : public Screen {
public:
    DashboardScreen(UIManager& uiManager);
    
    void onCreate() override;
    void onShow() override;
    void onUpdate() override;
    
private:
    void createHeader();
    void createRoomGrid();
    void createNavBar();
    void updateRoomCard(lv_obj_t* card, const Room& room);
    
    static void onRoomCardClicked(lv_event_t* e);
    static void onNavButtonClicked(lv_event_t* e);
    static void onSettingsClicked(lv_event_t* e);
    
    lv_obj_t* m_header = nullptr;
    lv_obj_t* m_timeLabel = nullptr;
    lv_obj_t* m_roomGrid = nullptr;
    lv_obj_t* m_navBar = nullptr;
    
    std::vector<lv_obj_t*> m_roomCards;
};

} // namespace smarthub::ui
```

### 8.2.3 Room Card Widget
```cpp
// app/include/smarthub/ui/widgets/RoomCard.hpp
#pragma once

#include <lvgl.h>
#include <string>

namespace smarthub::ui {

class RoomCard {
public:
    RoomCard(lv_obj_t* parent);
    
    void setName(const std::string& name);
    void setTemperature(float temp);
    void setActiveLights(int count);
    void setIcon(const char* symbol);
    
    lv_obj_t* obj() const { return m_card; }
    
    void setClickCallback(lv_event_cb_t cb, void* userData);
    
private:
    lv_obj_t* m_card = nullptr;
    lv_obj_t* m_nameLabel = nullptr;
    lv_obj_t* m_tempLabel = nullptr;
    lv_obj_t* m_lightsLabel = nullptr;
    lv_obj_t* m_iconLabel = nullptr;
};

} // namespace smarthub::ui
```

---

## 8.3 Device Control Screens

### 8.3.1 Device List Screen
```cpp
class DeviceListScreen : public Screen {
public:
    void onCreate() override;
    
private:
    void createDeviceList();
    void onDeviceClicked(const std::string& deviceId);
    
    lv_obj_t* m_list = nullptr;
};
```

### 8.3.2 Light Control Screen
```cpp
class LightControlScreen : public Screen {
public:
    LightControlScreen(UIManager& ui, const std::string& deviceId);
    
    void onCreate() override;
    void onUpdate() override;
    
private:
    void createPowerToggle();
    void createBrightnessSlider();
    void createColorTempSlider();
    void createColorPicker();
    
    static void onPowerToggle(lv_event_t* e);
    static void onBrightnessChange(lv_event_t* e);
    static void onColorTempChange(lv_event_t* e);
    
    std::string m_deviceId;
    lv_obj_t* m_powerSwitch = nullptr;
    lv_obj_t* m_brightnessSlider = nullptr;
    lv_obj_t* m_colorTempSlider = nullptr;
    lv_obj_t* m_colorWheel = nullptr;
};
```

### 8.3.3 Thermostat Control Screen
```cpp
class ThermostatScreen : public Screen {
public:
    void onCreate() override;
    
private:
    void createTemperatureArc();
    void createModeButtons();
    void createScheduleButton();
    
    lv_obj_t* m_tempArc = nullptr;
    lv_obj_t* m_currentTempLabel = nullptr;
    lv_obj_t* m_setpointLabel = nullptr;
};
```

---

## 8.4 Sensor Display Screens

### 8.4.1 Sensor Overview Screen
```cpp
class SensorOverviewScreen : public Screen {
public:
    void onCreate() override;
    void onUpdate() override;
    
private:
    void createSensorCards();
    void updateSensorCard(lv_obj_t* card, const SensorReading& reading);
    
    std::vector<lv_obj_t*> m_sensorCards;
};
```

### 8.4.2 Sensor History Chart
```cpp
class SensorHistoryScreen : public Screen {
public:
    SensorHistoryScreen(UIManager& ui, const std::string& sensorId);
    
    void onCreate() override;
    
private:
    void createChart();
    void loadHistoryData();
    void updateChart();
    
    static void onTimeRangeChanged(lv_event_t* e);
    
    std::string m_sensorId;
    lv_obj_t* m_chart = nullptr;
    lv_chart_series_t* m_series = nullptr;
    lv_obj_t* m_timeRangeDropdown = nullptr;
    
    std::vector<float> m_data;
};
```

---

## 8.5 WiFi Configuration Wizard

### 8.5.1 WiFi Setup Screen
```cpp
class WifiSetupScreen : public Screen {
public:
    void onCreate() override;
    
private:
    void createNetworkList();
    void createPasswordDialog();
    void scanNetworks();
    void connectToNetwork(const std::string& ssid, const std::string& password);
    
    static void onNetworkSelected(lv_event_t* e);
    static void onConnectClicked(lv_event_t* e);
    static void onRefreshClicked(lv_event_t* e);
    
    lv_obj_t* m_networkList = nullptr;
    lv_obj_t* m_passwordDialog = nullptr;
    lv_obj_t* m_keyboard = nullptr;
    lv_obj_t* m_statusLabel = nullptr;
    lv_obj_t* m_spinner = nullptr;
    
    std::string m_selectedSsid;
};
```

### 8.5.2 Password Entry with Virtual Keyboard
```cpp
void WifiSetupScreen::createPasswordDialog() {
    // Create modal dialog
    m_passwordDialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(m_passwordDialog, 440, 600);
    lv_obj_center(m_passwordDialog);
    
    // Title
    lv_obj_t* title = lv_label_create(m_passwordDialog);
    lv_label_set_text(title, "Enter Password");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Password text area
    lv_obj_t* ta = lv_textarea_create(m_passwordDialog);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_password_mode(ta, true);
    lv_obj_set_width(ta, 400);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 50);
    
    // Show/hide password toggle
    lv_obj_t* showBtn = lv_btn_create(m_passwordDialog);
    lv_obj_align_to(showBtn, ta, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    
    // Keyboard
    m_keyboard = lv_keyboard_create(m_passwordDialog);
    lv_keyboard_set_textarea(m_keyboard, ta);
    lv_obj_set_size(m_keyboard, 440, 300);
    lv_obj_align(m_keyboard, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    // Connect button
    lv_obj_t* connectBtn = lv_btn_create(m_passwordDialog);
    lv_obj_t* label = lv_label_create(connectBtn);
    lv_label_set_text(label, "Connect");
    lv_obj_align(connectBtn, LV_ALIGN_TOP_RIGHT, -10, 50);
    lv_obj_add_event_cb(connectBtn, onConnectClicked, LV_EVENT_CLICKED, this);
    
    // Initially hidden
    lv_obj_add_flag(m_passwordDialog, LV_OBJ_FLAG_HIDDEN);
}
```

---

## 8.6 Settings Screens

### 8.6.1 Settings Menu
```cpp
class SettingsScreen : public Screen {
public:
    void onCreate() override;
    
private:
    void createSettingsList();
    
    // Setting categories
    void openNetworkSettings();
    void openDisplaySettings();
    void openDeviceSettings();
    void openSecuritySettings();
    void openAbout();
};
```

### 8.6.2 Display Settings
```cpp
class DisplaySettingsScreen : public Screen {
public:
    void onCreate() override;
    
private:
    void createBrightnessSlider();
    void createThemeSelector();
    void createScreenTimeoutSlider();
    
    static void onBrightnessChanged(lv_event_t* e);
    static void onThemeChanged(lv_event_t* e);
    static void onTimeoutChanged(lv_event_t* e);
    
    lv_obj_t* m_brightnessSlider = nullptr;
    lv_obj_t* m_themeDropdown = nullptr;
    lv_obj_t* m_timeoutSlider = nullptr;
};
```

---

## 8.7 Theme System

### 8.7.1 Theme Manager
```cpp
// app/include/smarthub/ui/ThemeManager.hpp
#pragma once

namespace smarthub::ui {

enum class Theme { Light, Dark };

class ThemeManager {
public:
    void setTheme(Theme theme);
    Theme currentTheme() const { return m_theme; }
    
    // Color getters
    lv_color_t background() const;
    lv_color_t surface() const;
    lv_color_t primary() const;
    lv_color_t secondary() const;
    lv_color_t textPrimary() const;
    lv_color_t textSecondary() const;
    lv_color_t error() const;
    lv_color_t success() const;
    
private:
    void applyTheme();
    
    Theme m_theme = Theme::Dark;
};

} // namespace smarthub::ui
```

### 8.7.2 Dark Theme Colors
```cpp
void ThemeManager::applyDarkTheme() {
    // Background colors
    static lv_style_t style_bg;
    lv_style_init(&style_bg);
    lv_style_set_bg_color(&style_bg, lv_color_hex(0x1E1E1E));
    
    // Card/surface colors
    static lv_style_t style_card;
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, lv_color_hex(0x2D2D2D));
    lv_style_set_radius(&style_card, 12);
    lv_style_set_shadow_width(&style_card, 8);
    lv_style_set_shadow_color(&style_card, lv_color_hex(0x000000));
    
    // Primary color (blue accent)
    static lv_style_t style_primary;
    lv_style_init(&style_primary);
    lv_style_set_bg_color(&style_primary, lv_color_hex(0x2196F3));
    
    // Text colors
    static lv_style_t style_text;
    lv_style_init(&style_text);
    lv_style_set_text_color(&style_text, lv_color_hex(0xFFFFFF));
    
    // Apply to theme
    lv_theme_t* theme = lv_theme_default_init(
        lv_disp_get_default(),
        lv_color_hex(0x2196F3),  // Primary
        lv_color_hex(0x03DAC6),  // Secondary
        true,                     // Dark mode
        LV_FONT_DEFAULT
    );
    lv_disp_set_theme(lv_disp_get_default(), theme);
}
```

---

## 8.8 Animations and Transitions

### 8.8.1 Screen Transitions
```cpp
void ScreenManager::transitionTo(Screen* newScreen, TransitionType type) {
    lv_obj_t* oldScr = lv_scr_act();
    lv_obj_t* newScr = newScreen->container();
    
    switch (type) {
        case TransitionType::SlideLeft:
            lv_scr_load_anim(newScr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, true);
            break;
        case TransitionType::SlideRight:
            lv_scr_load_anim(newScr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, true);
            break;
        case TransitionType::Fade:
            lv_scr_load_anim(newScr, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, true);
            break;
        default:
            lv_scr_load(newScr);
            break;
    }
}
```

### 8.8.2 Widget Animations
```cpp
// Button press animation
void animateButtonPress(lv_obj_t* btn) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, btn);
    lv_anim_set_values(&a, 100, 95);
    lv_anim_set_time(&a, 100);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v) {
        lv_obj_set_style_transform_scale(static_cast<lv_obj_t*>(obj), v, 0);
    });
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

// Loading spinner
void showLoadingSpinner(lv_obj_t* parent) {
    lv_obj_t* spinner = lv_spinner_create(parent);
    lv_spinner_set_anim_params(spinner, 1000, 200);
    lv_obj_set_size(spinner, 50, 50);
    lv_obj_center(spinner);
}
```

---

## 8.9 Screen Saver / Display Sleep

### 8.9.1 Display Power Management
```cpp
class DisplayManager {
public:
    void setScreenTimeout(int seconds);
    void setBrightness(int percent);
    void wakeScreen();
    void sleepScreen();
    
    void onActivity();  // Called on any touch
    void update();      // Called each frame
    
private:
    void setBacklight(int percent);
    void dimScreen();
    
    int m_timeout = 60;
    int m_brightness = 100;
    int m_currentBrightness = 100;
    uint32_t m_lastActivityTime = 0;
    bool m_screenOn = true;
    bool m_dimmed = false;
};

void DisplayManager::update() {
    if (!m_screenOn) return;
    
    uint32_t elapsed = (lv_tick_get() - m_lastActivityTime) / 1000;
    
    // Dim after 30 seconds
    if (!m_dimmed && elapsed > 30) {
        dimScreen();
        m_dimmed = true;
    }
    
    // Sleep after timeout
    if (elapsed > m_timeout) {
        sleepScreen();
    }
}

void DisplayManager::onActivity() {
    m_lastActivityTime = lv_tick_get();
    if (!m_screenOn) {
        wakeScreen();
    } else if (m_dimmed) {
        setBrightness(m_brightness);
        m_dimmed = false;
    }
}
```

---

## 8.10 Font and Icon Assets

### 8.10.1 Custom Font Integration
```cpp
// Convert TTF to LVGL format
// lvgl/scripts/lv_font_conv --font Roboto-Regular.ttf -o roboto_16.c \
//     --size 16 --format lvgl --bpp 4 -r 0x20-0x7F

LV_FONT_DECLARE(roboto_16);
LV_FONT_DECLARE(roboto_24);
LV_FONT_DECLARE(roboto_32);
LV_FONT_DECLARE(roboto_48);

void setupFonts() {
    static lv_style_t style_title;
    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &roboto_32);
    
    static lv_style_t style_body;
    lv_style_init(&style_body);
    lv_style_set_text_font(&style_body, &roboto_16);
}
```

### 8.10.2 Icon Font (Material Design Icons)
```cpp
// Include Material Design Icons subset
LV_FONT_DECLARE(mdi_icons);

// Icon codes
#define ICON_LIGHTBULB      "\uF335"
#define ICON_THERMOMETER    "\uF50F"
#define ICON_HUMIDITY       "\uF58E"
#define ICON_MOTION         "\uF5F2"
#define ICON_WIFI           "\uF5A9"
#define ICON_SETTINGS       "\uF493"
#define ICON_POWER          "\uF425"
#define ICON_CHEVRON_RIGHT  "\uF142"
```

---

## 8.11 Accessibility

### 8.11.1 Touch Target Sizes
```cpp
// Minimum 48x48 dp touch targets
constexpr int MIN_TOUCH_TARGET = 48;

void ensureTouchTargetSize(lv_obj_t* obj) {
    lv_coord_t w = lv_obj_get_width(obj);
    lv_coord_t h = lv_obj_get_height(obj);
    
    if (w < MIN_TOUCH_TARGET) {
        lv_obj_set_ext_click_area(obj, (MIN_TOUCH_TARGET - w) / 2);
    }
    if (h < MIN_TOUCH_TARGET) {
        // Extend click area vertically
    }
}
```

### 8.11.2 High Contrast Mode
```cpp
void ThemeManager::setHighContrast(bool enabled) {
    if (enabled) {
        // Increase contrast ratios
        // Use pure black/white
        // Larger fonts
    }
}
```

---

## 8.12 Validation Checklist

| Item | Status | Notes |
|------|--------|-------|
| Dashboard displays rooms | ☐ | |
| Room cards show status | ☐ | |
| Device control works | ☐ | |
| Light controls work | ☐ | |
| Sensor history displays | ☐ | |
| WiFi setup works | ☐ | End-to-end |
| Virtual keyboard works | ☐ | |
| Settings accessible | ☐ | |
| Theme switching works | ☐ | |
| Screen timeout works | ☐ | |
| Animations smooth | ☐ | 30+ FPS |
| Touch response <100ms | ☐ | |
| All screens navigable | ☐ | No dead ends |

---

## Time Estimates

| Task | Estimated Time |
|------|----------------|
| 8.1-8.2 Dashboard | 6-8 hours |
| 8.3 Device Controls | 8-10 hours |
| 8.4 Sensor Displays | 4-6 hours |
| 8.5 WiFi Wizard | 6-8 hours |
| 8.6 Settings | 4-6 hours |
| 8.7 Theme System | 4-6 hours |
| 8.8 Animations | 3-4 hours |
| 8.9 Display Management | 2-3 hours |
| 8.10 Assets | 3-4 hours |
| 8.11-8.12 Polish/Test | 6-8 hours |
| **Total** | **46-63 hours** |

---

## References

- [LVGL Documentation](https://docs.lvgl.io/master/)
- [LVGL Examples](https://github.com/lvgl/lvgl/tree/master/examples)
- [Material Design Guidelines](https://material.io/design)
- [Touch Target Guidelines](https://www.w3.org/WAI/WCAG21/Understanding/target-size.html)
