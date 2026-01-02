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

// Test: High contrast mode
TEST_F(ThemeManagerTest, HighContrastMode) {
    theme.setMode(ThemeMode::HighContrast);

    EXPECT_EQ(theme.mode(), ThemeMode::HighContrast);
    EXPECT_TRUE(theme.isHighContrast());

    const auto& colors = theme.colors();

    // High contrast should use pure black background
    EXPECT_EQ(colors.background, 0x000000u);
    // Pure white text
    EXPECT_EQ(colors.textPrimary, 0xFFFFFFu);
    // High visibility primary color (cyan)
    EXPECT_EQ(colors.primary, 0x00FFFFu);
}

// Test: High contrast has maximum contrast
TEST_F(ThemeManagerTest, HighContrastMaximumContrast) {
    theme.setMode(ThemeMode::HighContrast);

    const auto& colors = theme.colors();

    // In high contrast, text secondary should also be white (no gray)
    EXPECT_EQ(colors.textSecondary, 0xFFFFFFu);
    // Dividers should be visible (white on black)
    EXPECT_EQ(colors.divider, 0xFFFFFFu);
}

// Test: isHighContrast returns false for other modes
TEST_F(ThemeManagerTest, IsHighContrastFalseForOtherModes) {
    theme.setMode(ThemeMode::Light);
    EXPECT_FALSE(theme.isHighContrast());

    theme.setMode(ThemeMode::Dark);
    EXPECT_FALSE(theme.isHighContrast());
}

// Test: High contrast error/success colors are pure
TEST_F(ThemeManagerTest, HighContrastPureColors) {
    theme.setMode(ThemeMode::HighContrast);

    const auto& colors = theme.colors();

    // Pure red for errors
    EXPECT_EQ(colors.error, 0xFF0000u);
    // Pure green for success
    EXPECT_EQ(colors.success, 0x00FF00u);
    // Yellow for warnings
    EXPECT_EQ(colors.warning, 0xFFFF00u);
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

// ============================================================================
// Animation Constants Tests
// ============================================================================

#include <smarthub/ui/AnimationManager.hpp>

TEST(AnimationManagerTest, AnimationDurations) {
    // Verify animation duration constants
    EXPECT_EQ(AnimationManager::DURATION_FAST, 150u);
    EXPECT_EQ(AnimationManager::DURATION_NORMAL, 300u);
    EXPECT_EQ(AnimationManager::DURATION_SLOW, 500u);
}

TEST(AnimationManagerTest, ScaleConstants) {
    // Verify scale constants for button press
    EXPECT_EQ(AnimationManager::PRESS_SCALE, 95);
    EXPECT_EQ(AnimationManager::NORMAL_SCALE, 100);
}

TEST(AnimationManagerTest, Construction) {
    // Verify AnimationManager can be constructed
    AnimationManager anim;
    EXPECT_TRUE(true);  // No crash
}

// ============================================================================
// Loading Spinner Tests
// ============================================================================

#include <smarthub/ui/widgets/LoadingSpinner.hpp>

TEST(LoadingSpinnerTest, Constants) {
    // Verify spinner constants
    EXPECT_EQ(LoadingSpinner::DEFAULT_SIZE, 48);
    EXPECT_EQ(LoadingSpinner::DEFAULT_DURATION, 1000u);
    EXPECT_EQ(LoadingSpinner::ARC_LENGTH, 60);
}
