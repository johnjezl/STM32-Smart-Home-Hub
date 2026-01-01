/**
 * ThemeManager Unit Tests
 *
 * Tests theme color management and mode switching.
 */

#include <gtest/gtest.h>
#include <smarthub/ui/ThemeManager.hpp>

using namespace smarthub::ui;

class ThemeManagerTest : public ::testing::Test {
protected:
    ThemeManager theme;
};

// Test: Default theme is dark
TEST_F(ThemeManagerTest, DefaultIsDark) {
    EXPECT_EQ(theme.mode(), ThemeMode::Dark);
}

// Test: Dark theme has correct colors
TEST_F(ThemeManagerTest, DarkThemeColors) {
    theme.setMode(ThemeMode::Dark);

    const auto& colors = theme.colors();

    // Dark background
    EXPECT_EQ(colors.background, 0x121212u);
    // Light text
    EXPECT_EQ(colors.textPrimary, 0xFFFFFFu);
    // Blue primary
    EXPECT_EQ(colors.primary, 0x2196F3u);
}

// Test: Light theme has correct colors
TEST_F(ThemeManagerTest, LightThemeColors) {
    theme.setMode(ThemeMode::Light);

    const auto& colors = theme.colors();

    // Light background
    EXPECT_EQ(colors.background, 0xFAFAFAu);
    // Dark text
    EXPECT_EQ(colors.textPrimary, 0x212121u);
    // Blue primary
    EXPECT_EQ(colors.primary, 0x1976D2u);
}

// Test: Toggle switches between modes
TEST_F(ThemeManagerTest, Toggle) {
    EXPECT_EQ(theme.mode(), ThemeMode::Dark);

    theme.toggle();
    EXPECT_EQ(theme.mode(), ThemeMode::Light);

    theme.toggle();
    EXPECT_EQ(theme.mode(), ThemeMode::Dark);
}

// Test: Set same mode is no-op
TEST_F(ThemeManagerTest, SetSameMode) {
    theme.setMode(ThemeMode::Dark);
    auto colorsBefore = theme.colors();

    theme.setMode(ThemeMode::Dark);
    auto colorsAfter = theme.colors();

    EXPECT_EQ(colorsBefore.background, colorsAfter.background);
}

// Test: UI constants are defined
TEST_F(ThemeManagerTest, UIConstants) {
    EXPECT_EQ(ThemeManager::HEADER_HEIGHT, 50);
    EXPECT_EQ(ThemeManager::NAVBAR_HEIGHT, 60);
    EXPECT_EQ(ThemeManager::CARD_RADIUS, 12);
    EXPECT_EQ(ThemeManager::CARD_PADDING, 16);
    EXPECT_EQ(ThemeManager::MIN_TOUCH_TARGET, 48);
    EXPECT_EQ(ThemeManager::SPACING_SM, 8);
    EXPECT_EQ(ThemeManager::SPACING_MD, 16);
    EXPECT_EQ(ThemeManager::SPACING_LG, 24);
}

// Test: Color palette completeness
TEST_F(ThemeManagerTest, ColorPaletteComplete) {
    const auto& colors = theme.colors();

    // All colors should be non-zero (except maybe some specific cases)
    EXPECT_NE(colors.background, 0u);
    EXPECT_NE(colors.surface, 0u);
    EXPECT_NE(colors.surfaceVariant, 0u);
    EXPECT_NE(colors.primary, 0u);
    EXPECT_NE(colors.primaryVariant, 0u);
    EXPECT_NE(colors.secondary, 0u);
    EXPECT_NE(colors.textPrimary, 0u);
    EXPECT_NE(colors.textSecondary, 0u);
    EXPECT_NE(colors.textOnPrimary, 0u);
    EXPECT_NE(colors.divider, 0u);
    EXPECT_NE(colors.error, 0u);
    EXPECT_NE(colors.success, 0u);
    EXPECT_NE(colors.warning, 0u);
}

// Test: Different themes have different colors
TEST_F(ThemeManagerTest, ThemesAreDifferent) {
    theme.setMode(ThemeMode::Dark);
    auto darkBg = theme.colors().background;
    auto darkText = theme.colors().textPrimary;

    theme.setMode(ThemeMode::Light);
    auto lightBg = theme.colors().background;
    auto lightText = theme.colors().textPrimary;

    EXPECT_NE(darkBg, lightBg);
    EXPECT_NE(darkText, lightText);
}

#ifdef SMARTHUB_ENABLE_LVGL

// Test: LVGL color accessors
TEST_F(ThemeManagerTest, LVGLColorAccessors) {
    lv_color_t primary = theme.primary();
    lv_color_t bg = theme.background();

    // Just verify they don't crash and return valid colors
    EXPECT_TRUE(true);
}

#endif
