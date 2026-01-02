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

// ============================================================================
// Additional AnimationManager Tests
// ============================================================================

TEST(AnimationManagerTest, DurationFastIsQuickest) {
    // FAST should be less than NORMAL
    EXPECT_LT(AnimationManager::DURATION_FAST, AnimationManager::DURATION_NORMAL);
}

TEST(AnimationManagerTest, DurationNormalIsMiddle) {
    // NORMAL should be between FAST and SLOW
    EXPECT_GT(AnimationManager::DURATION_NORMAL, AnimationManager::DURATION_FAST);
    EXPECT_LT(AnimationManager::DURATION_NORMAL, AnimationManager::DURATION_SLOW);
}

TEST(AnimationManagerTest, DurationSlowIsLongest) {
    // SLOW should be greater than NORMAL
    EXPECT_GT(AnimationManager::DURATION_SLOW, AnimationManager::DURATION_NORMAL);
}

TEST(AnimationManagerTest, PressScaleLessThanNormal) {
    // Press scale should be less than normal (button shrinks when pressed)
    EXPECT_LT(AnimationManager::PRESS_SCALE, AnimationManager::NORMAL_SCALE);
}

TEST(AnimationManagerTest, ScaleValuesArePercentages) {
    // Scale values should be percentages (0-100 range makes sense)
    EXPECT_GE(AnimationManager::PRESS_SCALE, 0);
    EXPECT_LE(AnimationManager::PRESS_SCALE, 100);
    EXPECT_EQ(AnimationManager::NORMAL_SCALE, 100);
}

TEST(AnimationManagerTest, MultipleInstances) {
    // Multiple AnimationManager instances should work
    AnimationManager anim1;
    AnimationManager anim2;
    AnimationManager anim3;
    EXPECT_TRUE(true);  // No crash with multiple instances
}

TEST(AnimationManagerTest, ConstructDestruct) {
    // Construct and destruct in a loop
    for (int i = 0; i < 10; i++) {
        AnimationManager anim;
        (void)anim;
    }
    EXPECT_TRUE(true);  // No crash or memory issues
}

// ============================================================================
// AnimationEasing Enum Tests
// ============================================================================

TEST(AnimationEasingTest, EnumValues) {
    // Verify all easing values are distinct
    EXPECT_NE(static_cast<int>(AnimationEasing::Linear),
              static_cast<int>(AnimationEasing::EaseOut));
    EXPECT_NE(static_cast<int>(AnimationEasing::EaseIn),
              static_cast<int>(AnimationEasing::EaseInOut));
    EXPECT_NE(static_cast<int>(AnimationEasing::Overshoot),
              static_cast<int>(AnimationEasing::Bounce));
}

TEST(AnimationEasingTest, AllEasingTypesExist) {
    // Verify we can use all easing types
    AnimationEasing e1 = AnimationEasing::Linear;
    AnimationEasing e2 = AnimationEasing::EaseOut;
    AnimationEasing e3 = AnimationEasing::EaseIn;
    AnimationEasing e4 = AnimationEasing::EaseInOut;
    AnimationEasing e5 = AnimationEasing::Overshoot;
    AnimationEasing e6 = AnimationEasing::Bounce;

    // Suppress unused variable warnings
    (void)e1; (void)e2; (void)e3; (void)e4; (void)e5; (void)e6;
    EXPECT_TRUE(true);
}

// ============================================================================
// Additional LoadingSpinner Tests
// ============================================================================

TEST(LoadingSpinnerTest, DefaultSizeIsTouchTarget) {
    // Default size should meet minimum touch target
    EXPECT_GE(LoadingSpinner::DEFAULT_SIZE, ThemeManager::MIN_TOUCH_TARGET);
}

TEST(LoadingSpinnerTest, ArcLengthIsReasonable) {
    // Arc length should be less than full circle (360)
    EXPECT_GT(LoadingSpinner::ARC_LENGTH, 0);
    EXPECT_LT(LoadingSpinner::ARC_LENGTH, 360);
}

TEST(LoadingSpinnerTest, DurationIsReasonable) {
    // Duration should be between 100ms and 10s
    EXPECT_GE(LoadingSpinner::DEFAULT_DURATION, 100u);
    EXPECT_LE(LoadingSpinner::DEFAULT_DURATION, 10000u);
}

// ============================================================================
// ThemeManager High Contrast Additional Tests
// ============================================================================

TEST_F(ThemeManagerTest, HighContrastSecondaryIsHighVisibility) {
    theme.setMode(ThemeMode::HighContrast);
    const auto& colors = theme.colors();

    // Secondary color should be yellow (high visibility)
    EXPECT_EQ(colors.secondary, 0xFFFF00u);
}

TEST_F(ThemeManagerTest, HighContrastSurfaceIsPureBlack) {
    theme.setMode(ThemeMode::HighContrast);
    const auto& colors = theme.colors();

    // Surface should also be pure black
    EXPECT_EQ(colors.surface, 0x000000u);
}

TEST_F(ThemeManagerTest, HighContrastTextOnPrimaryIsBlack) {
    theme.setMode(ThemeMode::HighContrast);
    const auto& colors = theme.colors();

    // Text on primary (cyan) should be black for contrast
    EXPECT_EQ(colors.textOnPrimary, 0x000000u);
}

TEST_F(ThemeManagerTest, HighContrastPrimaryVariantExists) {
    theme.setMode(ThemeMode::HighContrast);
    const auto& colors = theme.colors();

    // Primary variant should be different from primary
    EXPECT_NE(colors.primary, colors.primaryVariant);
    // But still cyan-ish
    EXPECT_EQ(colors.primaryVariant, 0x00CCCCu);
}

TEST_F(ThemeManagerTest, AllThreeModesAreDifferent) {
    theme.setMode(ThemeMode::Light);
    auto lightBg = theme.colors().background;

    theme.setMode(ThemeMode::Dark);
    auto darkBg = theme.colors().background;

    theme.setMode(ThemeMode::HighContrast);
    auto highContrastBg = theme.colors().background;

    // All three should have different backgrounds
    EXPECT_NE(lightBg, darkBg);
    EXPECT_NE(darkBg, highContrastBg);
    EXPECT_NE(lightBg, highContrastBg);
}

TEST_F(ThemeManagerTest, SwitchBetweenAllModes) {
    // Should be able to switch between all modes without issues
    theme.setMode(ThemeMode::Light);
    EXPECT_EQ(theme.mode(), ThemeMode::Light);

    theme.setMode(ThemeMode::Dark);
    EXPECT_EQ(theme.mode(), ThemeMode::Dark);

    theme.setMode(ThemeMode::HighContrast);
    EXPECT_EQ(theme.mode(), ThemeMode::HighContrast);

    theme.setMode(ThemeMode::Light);
    EXPECT_EQ(theme.mode(), ThemeMode::Light);

    theme.setMode(ThemeMode::HighContrast);
    EXPECT_EQ(theme.mode(), ThemeMode::HighContrast);

    theme.setMode(ThemeMode::Dark);
    EXPECT_EQ(theme.mode(), ThemeMode::Dark);
}
