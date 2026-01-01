/**
 * Theme Manager
 *
 * Manages UI theming including colors, fonts, and styles.
 * Supports light and dark themes with consistent styling.
 */
#pragma once

#include <cstdint>
#include <string>

#ifdef SMARTHUB_ENABLE_LVGL
#include <lvgl.h>
#endif

namespace smarthub {
namespace ui {

/**
 * Available theme modes
 */
enum class ThemeMode {
    Light,
    Dark
};

/**
 * Color palette for a theme
 */
struct ThemeColors {
    uint32_t background;      // Main background
    uint32_t surface;         // Cards, dialogs
    uint32_t surfaceVariant;  // Alternate surface
    uint32_t primary;         // Primary accent (buttons, links)
    uint32_t primaryVariant;  // Darker primary
    uint32_t secondary;       // Secondary accent
    uint32_t textPrimary;     // Main text
    uint32_t textSecondary;   // Subdued text
    uint32_t textOnPrimary;   // Text on primary color
    uint32_t divider;         // Lines, separators
    uint32_t error;           // Error states
    uint32_t success;         // Success states
    uint32_t warning;         // Warning states
};

/**
 * Theme Manager - controls visual styling
 *
 * Usage:
 *   ThemeManager theme;
 *   theme.setMode(ThemeMode::Dark);
 *   theme.apply();
 *
 *   // Use colors
 *   lv_obj_set_style_bg_color(obj, theme.primary(), 0);
 */
class ThemeManager {
public:
    ThemeManager();
    ~ThemeManager();

    /**
     * Set the theme mode
     * @param mode Light or Dark
     */
    void setMode(ThemeMode mode);

    /**
     * Get the current theme mode
     */
    ThemeMode mode() const { return m_mode; }

    /**
     * Toggle between light and dark mode
     */
    void toggle();

    /**
     * Apply the current theme to LVGL
     * Call after setMode() or toggle()
     */
    void apply();

    /**
     * Get the current color palette
     */
    const ThemeColors& colors() const { return m_colors; }

#ifdef SMARTHUB_ENABLE_LVGL
    // Color accessors (returns lv_color_t)
    lv_color_t background() const { return lv_color_hex(m_colors.background); }
    lv_color_t surface() const { return lv_color_hex(m_colors.surface); }
    lv_color_t surfaceVariant() const { return lv_color_hex(m_colors.surfaceVariant); }
    lv_color_t primary() const { return lv_color_hex(m_colors.primary); }
    lv_color_t primaryVariant() const { return lv_color_hex(m_colors.primaryVariant); }
    lv_color_t secondary() const { return lv_color_hex(m_colors.secondary); }
    lv_color_t textPrimary() const { return lv_color_hex(m_colors.textPrimary); }
    lv_color_t textSecondary() const { return lv_color_hex(m_colors.textSecondary); }
    lv_color_t textOnPrimary() const { return lv_color_hex(m_colors.textOnPrimary); }
    lv_color_t divider() const { return lv_color_hex(m_colors.divider); }
    lv_color_t error() const { return lv_color_hex(m_colors.error); }
    lv_color_t success() const { return lv_color_hex(m_colors.success); }
    lv_color_t warning() const { return lv_color_hex(m_colors.warning); }

    // Style presets
    void applyCardStyle(lv_obj_t* obj) const;
    void applyButtonStyle(lv_obj_t* obj) const;
    void applyHeaderStyle(lv_obj_t* obj) const;
    void applyNavBarStyle(lv_obj_t* obj) const;
#endif

    // UI constants
    static constexpr int HEADER_HEIGHT = 50;
    static constexpr int NAVBAR_HEIGHT = 60;
    static constexpr int CARD_RADIUS = 12;
    static constexpr int CARD_PADDING = 16;
    static constexpr int MIN_TOUCH_TARGET = 48;
    static constexpr int SPACING_SM = 8;
    static constexpr int SPACING_MD = 16;
    static constexpr int SPACING_LG = 24;

private:
    void loadLightTheme();
    void loadDarkTheme();

    ThemeMode m_mode = ThemeMode::Dark;
    ThemeColors m_colors;
};

} // namespace ui
} // namespace smarthub
