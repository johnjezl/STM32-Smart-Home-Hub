/**
 * Theme Manager Implementation
 */

#include "smarthub/ui/ThemeManager.hpp"
#include "smarthub/core/Logger.hpp"

namespace smarthub {
namespace ui {

ThemeManager::ThemeManager() {
    // Load default theme (dark)
    loadDarkTheme();
}

ThemeManager::~ThemeManager() = default;

void ThemeManager::setMode(ThemeMode mode) {
    if (m_mode == mode) {
        return;
    }

    m_mode = mode;

    switch (mode) {
        case ThemeMode::Light:
            loadLightTheme();
            break;
        case ThemeMode::Dark:
            loadDarkTheme();
            break;
        case ThemeMode::HighContrast:
            loadHighContrastTheme();
            break;
    }

    const char* modeName = (mode == ThemeMode::Light) ? "Light" :
                           (mode == ThemeMode::Dark) ? "Dark" : "High Contrast";
    LOG_INFO("Theme changed to: %s", modeName);
}

void ThemeManager::toggle() {
    setMode(m_mode == ThemeMode::Light ? ThemeMode::Dark : ThemeMode::Light);
    apply();
}

void ThemeManager::apply() {
#ifdef SMARTHUB_ENABLE_LVGL
    // Create and apply LVGL theme
    lv_theme_t* theme = lv_theme_default_init(
        lv_disp_get_default(),
        lv_color_hex(m_colors.primary),
        lv_color_hex(m_colors.secondary),
        m_mode == ThemeMode::Dark,  // dark mode flag
        LV_FONT_DEFAULT
    );

    if (lv_disp_get_default()) {
        lv_disp_set_theme(lv_disp_get_default(), theme);
    }

    LOG_DEBUG("Theme applied");
#endif
}

void ThemeManager::loadLightTheme() {
    m_colors.background = 0xFAFAFA;       // Light gray background
    m_colors.surface = 0xFFFFFF;          // White surface
    m_colors.surfaceVariant = 0xF5F5F5;   // Slightly darker surface
    m_colors.primary = 0x1976D2;          // Blue
    m_colors.primaryVariant = 0x1565C0;   // Darker blue
    m_colors.secondary = 0x26A69A;        // Teal
    m_colors.textPrimary = 0x212121;      // Near black
    m_colors.textSecondary = 0x757575;    // Gray
    m_colors.textOnPrimary = 0xFFFFFF;    // White
    m_colors.divider = 0xE0E0E0;          // Light gray
    m_colors.error = 0xD32F2F;            // Red
    m_colors.success = 0x388E3C;          // Green
    m_colors.warning = 0xF57C00;          // Orange
}

void ThemeManager::loadDarkTheme() {
    m_colors.background = 0x121212;       // Dark background
    m_colors.surface = 0x1E1E1E;          // Dark surface
    m_colors.surfaceVariant = 0x2D2D2D;   // Slightly lighter surface
    m_colors.primary = 0x2196F3;          // Blue
    m_colors.primaryVariant = 0x1976D2;   // Darker blue
    m_colors.secondary = 0x03DAC6;        // Teal
    m_colors.textPrimary = 0xFFFFFF;      // White
    m_colors.textSecondary = 0xB3B3B3;    // Light gray
    m_colors.textOnPrimary = 0xFFFFFF;    // White
    m_colors.divider = 0x424242;          // Dark gray
    m_colors.error = 0xCF6679;            // Light red
    m_colors.success = 0x4CAF50;          // Green
    m_colors.warning = 0xFFB74D;          // Light orange
}

void ThemeManager::loadHighContrastTheme() {
    // High contrast for accessibility - maximum contrast ratios
    m_colors.background = 0x000000;       // Pure black
    m_colors.surface = 0x000000;          // Pure black
    m_colors.surfaceVariant = 0x1A1A1A;   // Very dark gray
    m_colors.primary = 0x00FFFF;          // Cyan (high visibility)
    m_colors.primaryVariant = 0x00CCCC;   // Darker cyan
    m_colors.secondary = 0xFFFF00;        // Yellow (high visibility)
    m_colors.textPrimary = 0xFFFFFF;      // Pure white
    m_colors.textSecondary = 0xFFFFFF;    // White (no gray in high contrast)
    m_colors.textOnPrimary = 0x000000;    // Black on primary
    m_colors.divider = 0xFFFFFF;          // White borders
    m_colors.error = 0xFF0000;            // Pure red
    m_colors.success = 0x00FF00;          // Pure green
    m_colors.warning = 0xFFFF00;          // Yellow
}

#ifdef SMARTHUB_ENABLE_LVGL

void ThemeManager::applyCardStyle(lv_obj_t* obj) const {
    lv_obj_set_style_bg_color(obj, surface(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, CARD_RADIUS, 0);
    lv_obj_set_style_pad_all(obj, CARD_PADDING, 0);

    // Subtle shadow for depth
    lv_obj_set_style_shadow_width(obj, 8, 0);
    lv_obj_set_style_shadow_color(obj, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_20, 0);
    lv_obj_set_style_shadow_ofs_y(obj, 2, 0);

    // Border
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, divider(), 0);
    lv_obj_set_style_border_opa(obj, LV_OPA_50, 0);
}

void ThemeManager::applyButtonStyle(lv_obj_t* obj) const {
    lv_obj_set_style_bg_color(obj, primary(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, 8, 0);
    lv_obj_set_style_text_color(obj, textOnPrimary(), 0);

    // Pressed state
    lv_obj_set_style_bg_color(obj, primaryVariant(), LV_STATE_PRESSED);

    // Ensure minimum touch target
    if (lv_obj_get_height(obj) < MIN_TOUCH_TARGET) {
        lv_obj_set_height(obj, MIN_TOUCH_TARGET);
    }
    if (lv_obj_get_width(obj) < MIN_TOUCH_TARGET) {
        lv_obj_set_width(obj, MIN_TOUCH_TARGET);
    }
}

void ThemeManager::applyHeaderStyle(lv_obj_t* obj) const {
    lv_obj_set_style_bg_color(obj, surface(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(obj, SPACING_MD, 0);
    lv_obj_set_style_pad_ver(obj, SPACING_SM, 0);

    // Bottom border
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, divider(), 0);
    lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_BOTTOM, 0);
}

void ThemeManager::applyNavBarStyle(lv_obj_t* obj) const {
    lv_obj_set_style_bg_color(obj, surface(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(obj, SPACING_SM, 0);

    // Top border
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, divider(), 0);
    lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_TOP, 0);
}

void ThemeManager::applyButtonPressAnimation(lv_obj_t* obj) const {
    if (!obj) return;

    // Set transform pivot to center for scale animation
    lv_obj_set_style_transform_pivot_x(obj, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(obj, lv_pct(50), 0);

    // Scale down to 95% when pressed (256 = 100% in LVGL)
    lv_obj_set_style_transform_zoom(obj, 243, LV_STATE_PRESSED);  // 95% of 256

    // Darken background on press
    lv_obj_set_style_bg_color(obj, primaryVariant(), LV_STATE_PRESSED);

    // Create smooth transition
    static lv_style_transition_dsc_t pressTrans;
    static lv_style_prop_t pressProps[] = {
        LV_STYLE_TRANSFORM_ZOOM,
        LV_STYLE_BG_COLOR,
        LV_STYLE_PROP_INV
    };
    lv_style_transition_dsc_init(&pressTrans, pressProps, lv_anim_path_ease_out, 100, 0, nullptr);

    lv_obj_set_style_transition(obj, &pressTrans, LV_STATE_PRESSED);
    lv_obj_set_style_transition(obj, &pressTrans, LV_STATE_DEFAULT);
}

void ThemeManager::applyErrorStyle(lv_obj_t* obj) const {
    if (!obj) return;

    // Red border for error state
    lv_obj_set_style_border_color(obj, error(), 0);
    lv_obj_set_style_border_width(obj, 2, 0);

    // Optional: tint background slightly red
    if (m_mode == ThemeMode::HighContrast) {
        lv_obj_set_style_bg_color(obj, lv_color_hex(0x330000), 0);
    }
}

void ThemeManager::applySuccessStyle(lv_obj_t* obj) const {
    if (!obj) return;

    // Green border for success state
    lv_obj_set_style_border_color(obj, success(), 0);
    lv_obj_set_style_border_width(obj, 2, 0);

    // Optional: tint background slightly green
    if (m_mode == ThemeMode::HighContrast) {
        lv_obj_set_style_bg_color(obj, lv_color_hex(0x003300), 0);
    }
}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
