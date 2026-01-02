/**
 * Display Settings Screen Implementation
 */

#include "smarthub/ui/screens/DisplaySettingsScreen.hpp"
#include "smarthub/ui/ScreenManager.hpp"
#include "smarthub/ui/ThemeManager.hpp"
#include "smarthub/ui/DisplayManager.hpp"
#include "smarthub/ui/widgets/Header.hpp"
#include "smarthub/core/Logger.hpp"

namespace smarthub {
namespace ui {

DisplaySettingsScreen::DisplaySettingsScreen(ScreenManager& screenManager,
                                             ThemeManager& theme,
                                             DisplayManager& displayManager)
    : Screen(screenManager, "display_settings")
    , m_theme(theme)
    , m_displayManager(displayManager)
{
}

DisplaySettingsScreen::~DisplaySettingsScreen() = default;

void DisplaySettingsScreen::onCreate() {
#ifdef SMARTHUB_ENABLE_LVGL
    if (!m_container) return;

    lv_obj_set_style_bg_color(m_container, m_theme.background(), 0);

    createHeader();

    // Content area
    m_content = lv_obj_create(m_container);
    lv_obj_set_size(m_content, LV_PCT(100), 480 - Header::HEIGHT);
    lv_obj_align(m_content, LV_ALIGN_TOP_MID, 0, Header::HEIGHT);
    lv_obj_set_style_bg_opa(m_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_content, 0, 0);
    lv_obj_set_style_pad_all(m_content, ThemeManager::SPACING_MD, 0);
    lv_obj_set_flex_flow(m_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_content, ThemeManager::SPACING_MD, 0);
    lv_obj_add_flag(m_content, LV_OBJ_FLAG_SCROLLABLE);

    createBrightnessSection();
    createTimeoutSection();
    createThemeSection();

    LOG_DEBUG("DisplaySettingsScreen created");
#endif
}

void DisplaySettingsScreen::onShow() {
#ifdef SMARTHUB_ENABLE_LVGL
    // Update controls to reflect current settings
    if (m_brightnessSlider) {
        lv_slider_set_value(m_brightnessSlider, m_displayManager.brightness(), LV_ANIM_OFF);
        updateBrightnessLabel();
    }
    updateTimeoutButtons();
    updateThemeButtons();
#endif
    LOG_DEBUG("DisplaySettingsScreen shown");
}

void DisplaySettingsScreen::onUpdate(uint32_t deltaMs) {
    (void)deltaMs;
}

void DisplaySettingsScreen::onDestroy() {
    LOG_DEBUG("DisplaySettingsScreen destroyed");
}

#ifdef SMARTHUB_ENABLE_LVGL

void DisplaySettingsScreen::createHeader() {
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
    lv_label_set_text(m_titleLabel, "Display Settings");
    lv_obj_set_style_text_color(m_titleLabel, m_theme.textPrimary(), 0);
    lv_obj_align(m_titleLabel, LV_ALIGN_LEFT_MID, 60, 0);
}

void DisplaySettingsScreen::createBrightnessSection() {
    lv_obj_t* section = lv_obj_create(m_content);
    lv_obj_set_size(section, LV_PCT(100), 100);
    m_theme.applyCardStyle(section);
    lv_obj_clear_flag(section, LV_OBJ_FLAG_SCROLLABLE);

    // Title and value
    lv_obj_t* title = lv_label_create(section);
    lv_label_set_text(title, "Brightness");
    lv_obj_set_style_text_color(title, m_theme.textPrimary(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    m_brightnessLabel = lv_label_create(section);
    lv_obj_set_style_text_color(m_brightnessLabel, m_theme.textSecondary(), 0);
    lv_obj_align(m_brightnessLabel, LV_ALIGN_TOP_RIGHT, 0, 0);
    updateBrightnessLabel();

    // Slider
    m_brightnessSlider = lv_slider_create(section);
    lv_obj_set_width(m_brightnessSlider, lv_pct(100) - 2 * ThemeManager::CARD_PADDING);
    lv_obj_align(m_brightnessSlider, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_slider_set_range(m_brightnessSlider, 10, 100);  // Min 10% to avoid black screen
    lv_slider_set_value(m_brightnessSlider, m_displayManager.brightness(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(m_brightnessSlider, m_theme.primary(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(m_brightnessSlider, m_theme.primary(), LV_PART_KNOB);
    lv_obj_add_event_cb(m_brightnessSlider, brightnessSliderHandler, LV_EVENT_VALUE_CHANGED, this);
}

void DisplaySettingsScreen::createTimeoutSection() {
    lv_obj_t* section = lv_obj_create(m_content);
    lv_obj_set_size(section, LV_PCT(100), 100);
    m_theme.applyCardStyle(section);
    lv_obj_clear_flag(section, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* title = lv_label_create(section);
    lv_label_set_text(title, "Screen Timeout");
    lv_obj_set_style_text_color(title, m_theme.textPrimary(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    // Button row
    const char* labels[] = {"Never", "30s", "1m", "5m", "10m"};
    int btnWidth = 65;
    int startX = 0;

    for (int i = 0; i < 5; i++) {
        m_timeoutButtons[i] = lv_btn_create(section);
        lv_obj_set_size(m_timeoutButtons[i], btnWidth, 35);
        lv_obj_align(m_timeoutButtons[i], LV_ALIGN_BOTTOM_LEFT, startX, -5);
        lv_obj_set_style_radius(m_timeoutButtons[i], 5, 0);

        lv_obj_t* label = lv_label_create(m_timeoutButtons[i]);
        lv_label_set_text(label, labels[i]);
        lv_obj_center(label);

        // Store timeout value in user data
        lv_obj_set_user_data(m_timeoutButtons[i], reinterpret_cast<void*>(static_cast<intptr_t>(m_timeoutValues[i])));
        lv_obj_add_event_cb(m_timeoutButtons[i], timeoutButtonHandler, LV_EVENT_CLICKED, this);

        startX += btnWidth + 8;
    }

    updateTimeoutButtons();
}

void DisplaySettingsScreen::createThemeSection() {
    lv_obj_t* section = lv_obj_create(m_content);
    lv_obj_set_size(section, LV_PCT(100), 100);
    m_theme.applyCardStyle(section);
    lv_obj_clear_flag(section, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* title = lv_label_create(section);
    lv_label_set_text(title, "Theme");
    lv_obj_set_style_text_color(title, m_theme.textPrimary(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    // Button row
    const char* labels[] = {"Light", "Dark", "Auto"};
    int btnWidth = 80;
    int startX = 0;

    for (int i = 0; i < 3; i++) {
        m_themeButtons[i] = lv_btn_create(section);
        lv_obj_set_size(m_themeButtons[i], btnWidth, 35);
        lv_obj_align(m_themeButtons[i], LV_ALIGN_BOTTOM_LEFT, startX, -5);
        lv_obj_set_style_radius(m_themeButtons[i], 5, 0);

        lv_obj_t* label = lv_label_create(m_themeButtons[i]);
        lv_label_set_text(label, labels[i]);
        lv_obj_center(label);

        lv_obj_set_user_data(m_themeButtons[i], reinterpret_cast<void*>(static_cast<intptr_t>(i)));
        lv_obj_add_event_cb(m_themeButtons[i], themeButtonHandler, LV_EVENT_CLICKED, this);

        startX += btnWidth + 8;
    }

    updateThemeButtons();
}

void DisplaySettingsScreen::updateBrightnessLabel() {
    if (!m_brightnessLabel) return;

    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", m_displayManager.brightness());
    lv_label_set_text(m_brightnessLabel, buf);
}

void DisplaySettingsScreen::updateTimeoutButtons() {
    int currentTimeout = m_displayManager.timeoutSeconds();

    for (int i = 0; i < 5; i++) {
        if (!m_timeoutButtons[i]) continue;

        bool selected = (m_timeoutValues[i] == currentTimeout);

        if (selected) {
            lv_obj_set_style_bg_color(m_timeoutButtons[i], m_theme.primary(), 0);
            lv_obj_set_style_text_color(lv_obj_get_child(m_timeoutButtons[i], 0),
                                        lv_color_white(), 0);
        } else {
            lv_obj_set_style_bg_color(m_timeoutButtons[i], m_theme.surfaceVariant(), 0);
            lv_obj_set_style_text_color(lv_obj_get_child(m_timeoutButtons[i], 0),
                                        m_theme.textPrimary(), 0);
        }
    }
}

void DisplaySettingsScreen::updateThemeButtons() {
    int currentMode = static_cast<int>(m_theme.mode());

    for (int i = 0; i < 3; i++) {
        if (!m_themeButtons[i]) continue;

        bool selected = (i == currentMode);

        if (selected) {
            lv_obj_set_style_bg_color(m_themeButtons[i], m_theme.primary(), 0);
            lv_obj_set_style_text_color(lv_obj_get_child(m_themeButtons[i], 0),
                                        lv_color_white(), 0);
        } else {
            lv_obj_set_style_bg_color(m_themeButtons[i], m_theme.surfaceVariant(), 0);
            lv_obj_set_style_text_color(lv_obj_get_child(m_themeButtons[i], 0),
                                        m_theme.textPrimary(), 0);
        }
    }
}

void DisplaySettingsScreen::onBrightnessChanged(int percent) {
    m_displayManager.setBrightness(percent);
    updateBrightnessLabel();
}

void DisplaySettingsScreen::onTimeoutSelected(int seconds) {
    m_displayManager.setTimeoutSeconds(seconds);
    updateTimeoutButtons();
    LOG_DEBUG("Screen timeout set to %d seconds", seconds);
}

void DisplaySettingsScreen::onThemeSelected(int mode) {
    m_theme.setMode(static_cast<ThemeMode>(mode));
    updateThemeButtons();
    LOG_DEBUG("Theme mode set to %d", mode);
}

void DisplaySettingsScreen::onBackClicked() {
    m_screenManager.goBack();
}

void DisplaySettingsScreen::brightnessSliderHandler(lv_event_t* e) {
    DisplaySettingsScreen* self = static_cast<DisplaySettingsScreen*>(lv_event_get_user_data(e));
    lv_obj_t* slider = lv_event_get_target(e);
    if (self && slider) {
        self->onBrightnessChanged(lv_slider_get_value(slider));
    }
}

void DisplaySettingsScreen::timeoutButtonHandler(lv_event_t* e) {
    DisplaySettingsScreen* self = static_cast<DisplaySettingsScreen*>(lv_event_get_user_data(e));
    lv_obj_t* btn = lv_event_get_target(e);
    if (self && btn) {
        intptr_t timeout = reinterpret_cast<intptr_t>(lv_obj_get_user_data(btn));
        self->onTimeoutSelected(static_cast<int>(timeout));
    }
}

void DisplaySettingsScreen::themeButtonHandler(lv_event_t* e) {
    DisplaySettingsScreen* self = static_cast<DisplaySettingsScreen*>(lv_event_get_user_data(e));
    lv_obj_t* btn = lv_event_get_target(e);
    if (self && btn) {
        intptr_t mode = reinterpret_cast<intptr_t>(lv_obj_get_user_data(btn));
        self->onThemeSelected(static_cast<int>(mode));
    }
}

void DisplaySettingsScreen::backButtonHandler(lv_event_t* e) {
    DisplaySettingsScreen* self = static_cast<DisplaySettingsScreen*>(lv_event_get_user_data(e));
    if (self) {
        self->onBackClicked();
    }
}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
