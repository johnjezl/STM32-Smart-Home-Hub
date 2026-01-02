/**
 * Light Control Screen Implementation
 */

#include "smarthub/ui/screens/LightControlScreen.hpp"
#include "smarthub/ui/ScreenManager.hpp"
#include "smarthub/ui/ThemeManager.hpp"
#include "smarthub/ui/widgets/Header.hpp"
#include "smarthub/devices/DeviceManager.hpp"
#include "smarthub/devices/Device.hpp"
#include "smarthub/core/Logger.hpp"

namespace smarthub {
namespace ui {

LightControlScreen::LightControlScreen(ScreenManager& screenManager,
                                       ThemeManager& theme,
                                       DeviceManager& deviceManager)
    : Screen(screenManager, "light_control")
    , m_theme(theme)
    , m_deviceManager(deviceManager)
{
}

LightControlScreen::~LightControlScreen() = default;

void LightControlScreen::setDeviceId(const std::string& deviceId) {
    m_deviceId = deviceId;
}

void LightControlScreen::onCreate() {
#ifdef SMARTHUB_ENABLE_LVGL
    if (!m_container) return;

    lv_obj_set_style_bg_color(m_container, m_theme.background(), 0);

    createHeader();
    createContent();

    LOG_DEBUG("LightControlScreen created");
#endif
}

void LightControlScreen::onShow() {
#ifdef SMARTHUB_ENABLE_LVGL
    updateControls();
    LOG_DEBUG("LightControlScreen shown for device: %s", m_deviceId.c_str());
#endif
}

void LightControlScreen::onUpdate(uint32_t deltaMs) {
    (void)deltaMs;
}

void LightControlScreen::onDestroy() {
    LOG_DEBUG("LightControlScreen destroyed");
}

#ifdef SMARTHUB_ENABLE_LVGL

void LightControlScreen::createHeader() {
    // Header bar
    lv_obj_t* header = lv_obj_create(m_container);
    lv_obj_set_size(header, LV_PCT(100), Header::HEIGHT);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    m_theme.applyHeaderStyle(header);

    // Back button
    m_backBtn = lv_btn_create(header);
    lv_obj_set_size(m_backBtn, 40, 40);
    lv_obj_align(m_backBtn, LV_ALIGN_LEFT_MID, ThemeManager::SPACING_SM, 0);
    lv_obj_set_style_bg_opa(m_backBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(m_backBtn, 0, 0);
    lv_obj_add_event_cb(m_backBtn, backButtonHandler, LV_EVENT_CLICKED, this);

    lv_obj_t* backIcon = lv_label_create(m_backBtn);
    lv_label_set_text(backIcon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(backIcon, m_theme.textPrimary(), 0);
    lv_obj_center(backIcon);

    // Title
    m_titleLabel = lv_label_create(header);
    lv_label_set_text(m_titleLabel, "Light");
    lv_obj_set_style_text_color(m_titleLabel, m_theme.textPrimary(), 0);
    lv_obj_align(m_titleLabel, LV_ALIGN_LEFT_MID, 60, 0);
}

void LightControlScreen::createContent() {
    m_content = lv_obj_create(m_container);
    lv_obj_set_size(m_content, LV_PCT(100), lv_pct(100) - Header::HEIGHT);
    lv_obj_align(m_content, LV_ALIGN_TOP_MID, 0, Header::HEIGHT);
    lv_obj_set_style_bg_opa(m_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_content, 0, 0);
    lv_obj_set_style_pad_all(m_content, ThemeManager::SPACING_LG, 0);
    lv_obj_clear_flag(m_content, LV_OBJ_FLAG_SCROLLABLE);

    // Light preview circle (shows current color/brightness)
    m_previewCircle = lv_obj_create(m_content);
    lv_obj_set_size(m_previewCircle, 120, 120);
    lv_obj_align(m_previewCircle, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_radius(m_previewCircle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(m_previewCircle, m_theme.warning(), 0);
    lv_obj_set_style_border_width(m_previewCircle, 0, 0);
    lv_obj_clear_flag(m_previewCircle, LV_OBJ_FLAG_SCROLLABLE);

    // Power switch
    lv_obj_t* powerRow = lv_obj_create(m_content);
    lv_obj_set_size(powerRow, LV_PCT(100), 60);
    lv_obj_align(powerRow, LV_ALIGN_TOP_MID, 0, 160);
    m_theme.applyCardStyle(powerRow);
    lv_obj_clear_flag(powerRow, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* powerLabel = lv_label_create(powerRow);
    lv_label_set_text(powerLabel, "Power");
    lv_obj_set_style_text_color(powerLabel, m_theme.textPrimary(), 0);
    lv_obj_align(powerLabel, LV_ALIGN_LEFT_MID, 0, 0);

    m_powerSwitch = lv_switch_create(powerRow);
    lv_obj_align(m_powerSwitch, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(m_powerSwitch, m_theme.primary(), LV_STATE_CHECKED | LV_PART_INDICATOR);
    lv_obj_add_event_cb(m_powerSwitch, powerToggleHandler, LV_EVENT_VALUE_CHANGED, this);

    // Brightness slider
    lv_obj_t* brightnessRow = lv_obj_create(m_content);
    lv_obj_set_size(brightnessRow, LV_PCT(100), 80);
    lv_obj_align(brightnessRow, LV_ALIGN_TOP_MID, 0, 235);
    m_theme.applyCardStyle(brightnessRow);
    lv_obj_clear_flag(brightnessRow, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* brightnessTitle = lv_label_create(brightnessRow);
    lv_label_set_text(brightnessTitle, "Brightness");
    lv_obj_set_style_text_color(brightnessTitle, m_theme.textPrimary(), 0);
    lv_obj_align(brightnessTitle, LV_ALIGN_TOP_LEFT, 0, 0);

    m_brightnessLabel = lv_label_create(brightnessRow);
    lv_label_set_text(m_brightnessLabel, "100%");
    lv_obj_set_style_text_color(m_brightnessLabel, m_theme.textSecondary(), 0);
    lv_obj_align(m_brightnessLabel, LV_ALIGN_TOP_RIGHT, 0, 0);

    m_brightnessSlider = lv_slider_create(brightnessRow);
    lv_obj_set_width(m_brightnessSlider, lv_pct(100) - 2 * ThemeManager::CARD_PADDING);
    lv_obj_align(m_brightnessSlider, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_slider_set_range(m_brightnessSlider, 0, 100);
    lv_slider_set_value(m_brightnessSlider, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(m_brightnessSlider, m_theme.primary(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(m_brightnessSlider, m_theme.primary(), LV_PART_KNOB);
    lv_obj_add_event_cb(m_brightnessSlider, brightnessSliderHandler, LV_EVENT_VALUE_CHANGED, this);

    // Color temperature slider
    lv_obj_t* colorTempRow = lv_obj_create(m_content);
    lv_obj_set_size(colorTempRow, LV_PCT(100), 80);
    lv_obj_align(colorTempRow, LV_ALIGN_TOP_MID, 0, 330);
    m_theme.applyCardStyle(colorTempRow);
    lv_obj_clear_flag(colorTempRow, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* colorTempTitle = lv_label_create(colorTempRow);
    lv_label_set_text(colorTempTitle, "Color Temperature");
    lv_obj_set_style_text_color(colorTempTitle, m_theme.textPrimary(), 0);
    lv_obj_align(colorTempTitle, LV_ALIGN_TOP_LEFT, 0, 0);

    m_colorTempLabel = lv_label_create(colorTempRow);
    lv_label_set_text(m_colorTempLabel, "4000K");
    lv_obj_set_style_text_color(m_colorTempLabel, m_theme.textSecondary(), 0);
    lv_obj_align(m_colorTempLabel, LV_ALIGN_TOP_RIGHT, 0, 0);

    m_colorTempSlider = lv_slider_create(colorTempRow);
    lv_obj_set_width(m_colorTempSlider, lv_pct(100) - 2 * ThemeManager::CARD_PADDING);
    lv_obj_align(m_colorTempSlider, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_slider_set_range(m_colorTempSlider, 2700, 6500);
    lv_slider_set_value(m_colorTempSlider, 4000, LV_ANIM_OFF);
    lv_obj_add_event_cb(m_colorTempSlider, colorTempSliderHandler, LV_EVENT_VALUE_CHANGED, this);
}

void LightControlScreen::updateControls() {
    auto device = m_deviceManager.getDevice(m_deviceId);
    if (!device) return;

    if (m_titleLabel) {
        lv_label_set_text(m_titleLabel, device->name().c_str());
    }

    bool isOn = false;
    auto state = device->getState();
    if (state.contains("on")) {
        isOn = state["on"].get<bool>();
    }

    if (m_powerSwitch) {
        if (isOn) {
            lv_obj_add_state(m_powerSwitch, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(m_powerSwitch, LV_STATE_CHECKED);
        }
    }

    // Get brightness if available
    auto brightness = device->getProperty("brightness");
    if (brightness.is_number() && m_brightnessSlider) {
        int percent = brightness.get<int>();
        lv_slider_set_value(m_brightnessSlider, percent, LV_ANIM_OFF);

        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", percent);
        lv_label_set_text(m_brightnessLabel, buf);
    }

    // Get color temp if available
    auto colorTemp = device->getProperty("colorTemp");
    if (colorTemp.is_number() && m_colorTempSlider) {
        int kelvin = colorTemp.get<int>();
        lv_slider_set_value(m_colorTempSlider, kelvin, LV_ANIM_OFF);

        char buf[16];
        snprintf(buf, sizeof(buf), "%dK", kelvin);
        lv_label_set_text(m_colorTempLabel, buf);
    }

    // Update preview
    if (m_previewCircle) {
        if (isOn) {
            lv_obj_set_style_bg_color(m_previewCircle, m_theme.warning(), 0);
            lv_obj_set_style_bg_opa(m_previewCircle, LV_OPA_COVER, 0);
        } else {
            lv_obj_set_style_bg_color(m_previewCircle, m_theme.surfaceVariant(), 0);
            lv_obj_set_style_bg_opa(m_previewCircle, LV_OPA_50, 0);
        }
    }
}

void LightControlScreen::onPowerToggle(bool on) {
    auto device = m_deviceManager.getDevice(m_deviceId);
    if (!device) return;

    device->setState("on", on);

    updateControls();
}

void LightControlScreen::onBrightnessChange(int percent) {
    auto device = m_deviceManager.getDevice(m_deviceId);
    if (!device) return;

    device->setState("brightness", percent);

    if (m_brightnessLabel) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", percent);
        lv_label_set_text(m_brightnessLabel, buf);
    }
}

void LightControlScreen::onColorTempChange(int kelvin) {
    auto device = m_deviceManager.getDevice(m_deviceId);
    if (!device) return;

    device->setState("colorTemp", kelvin);

    if (m_colorTempLabel) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%dK", kelvin);
        lv_label_set_text(m_colorTempLabel, buf);
    }
}

void LightControlScreen::onBackClicked() {
    m_screenManager.goBack();
}

void LightControlScreen::powerToggleHandler(lv_event_t* e) {
    LightControlScreen* self = static_cast<LightControlScreen*>(lv_event_get_user_data(e));
    lv_obj_t* sw = lv_event_get_target(e);
    if (self) {
        self->onPowerToggle(lv_obj_has_state(sw, LV_STATE_CHECKED));
    }
}

void LightControlScreen::brightnessSliderHandler(lv_event_t* e) {
    LightControlScreen* self = static_cast<LightControlScreen*>(lv_event_get_user_data(e));
    lv_obj_t* slider = lv_event_get_target(e);
    if (self) {
        self->onBrightnessChange(lv_slider_get_value(slider));
    }
}

void LightControlScreen::colorTempSliderHandler(lv_event_t* e) {
    LightControlScreen* self = static_cast<LightControlScreen*>(lv_event_get_user_data(e));
    lv_obj_t* slider = lv_event_get_target(e);
    if (self) {
        self->onColorTempChange(lv_slider_get_value(slider));
    }
}

void LightControlScreen::backButtonHandler(lv_event_t* e) {
    LightControlScreen* self = static_cast<LightControlScreen*>(lv_event_get_user_data(e));
    if (self) {
        self->onBackClicked();
    }
}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
