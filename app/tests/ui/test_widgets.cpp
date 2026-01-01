/**
 * Widget Unit Tests (Phase 8.B)
 *
 * Tests for Header, NavBar, and RoomCard widgets.
 * LVGL-specific rendering tested on hardware; these tests focus on
 * data handling, callbacks, and configuration.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <climits>

#include <smarthub/ui/widgets/Header.hpp>
#include <smarthub/ui/widgets/NavBar.hpp>
#include <smarthub/ui/widgets/RoomCard.hpp>
#include <smarthub/ui/widgets/TimeSeriesChart.hpp>
#include <smarthub/ui/ThemeManager.hpp>

#include <string>
#include <vector>

using namespace smarthub::ui;

// ============================================================================
// Header Widget Tests
// ============================================================================

class HeaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        themeManager = std::make_unique<ThemeManager>();
    }

    std::unique_ptr<ThemeManager> themeManager;
};

TEST_F(HeaderTest, Constants) {
    // Verify header height constant
    EXPECT_EQ(Header::HEIGHT, 50);
}

#ifdef SMARTHUB_ENABLE_LVGL

TEST_F(HeaderTest, TitleCallback) {
    // Would test title setting with LVGL
    // Placeholder for LVGL-dependent test
    GTEST_SKIP() << "Requires LVGL display initialization";
}

TEST_F(HeaderTest, TimeFormat) {
    // Would test time display formatting
    GTEST_SKIP() << "Requires LVGL display initialization";
}

TEST_F(HeaderTest, NotificationCallback) {
    // Would test notification click callback
    GTEST_SKIP() << "Requires LVGL display initialization";
}

TEST_F(HeaderTest, SettingsCallback) {
    // Would test settings click callback
    GTEST_SKIP() << "Requires LVGL display initialization";
}

#endif // SMARTHUB_ENABLE_LVGL

// ============================================================================
// NavBar Widget Tests
// ============================================================================

class NavBarTest : public ::testing::Test {
protected:
    void SetUp() override {
        themeManager = std::make_unique<ThemeManager>();
    }

    std::unique_ptr<ThemeManager> themeManager;
};

TEST_F(NavBarTest, Constants) {
    // Verify navbar height constant
    EXPECT_EQ(NavBar::HEIGHT, 60);
}

TEST(NavTabTest, Construction) {
    // Test NavTab struct construction
    NavTab tab;
    tab.id = "home";
    tab.label = "Home";
    tab.icon = "H";

    EXPECT_EQ(tab.id, "home");
    EXPECT_EQ(tab.label, "Home");
    EXPECT_EQ(tab.icon, "H");
}

TEST(NavTabTest, Initialization) {
    // Test aggregate initialization
    NavTab tab{"settings", "Settings", "S"};

    EXPECT_EQ(tab.id, "settings");
    EXPECT_EQ(tab.label, "Settings");
    EXPECT_EQ(tab.icon, "S");
}

#ifdef SMARTHUB_ENABLE_LVGL

TEST_F(NavBarTest, AddTab) {
    GTEST_SKIP() << "Requires LVGL display initialization";
}

TEST_F(NavBarTest, SetActiveTab) {
    GTEST_SKIP() << "Requires LVGL display initialization";
}

TEST_F(NavBarTest, TabSelectedCallback) {
    GTEST_SKIP() << "Requires LVGL display initialization";
}

TEST_F(NavBarTest, MultipleTabs) {
    GTEST_SKIP() << "Requires LVGL display initialization";
}

#endif // SMARTHUB_ENABLE_LVGL

// ============================================================================
// RoomCard Widget Tests
// ============================================================================

class RoomCardTest : public ::testing::Test {
protected:
    void SetUp() override {
        themeManager = std::make_unique<ThemeManager>();
    }

    std::unique_ptr<ThemeManager> themeManager;
};

TEST_F(RoomCardTest, Constants) {
    // Verify room card dimensions
    EXPECT_EQ(RoomCard::WIDTH, 180);
    EXPECT_EQ(RoomCard::HEIGHT, 100);
}

TEST(RoomDataTest, DefaultValues) {
    // Test RoomData default construction
    RoomData data;

    EXPECT_EQ(data.id, "");
    EXPECT_EQ(data.name, "");
    EXPECT_FLOAT_EQ(data.temperature, 0.0f);
    EXPECT_EQ(data.activeDevices, 0);
    EXPECT_FALSE(data.hasTemperature);
}

TEST(RoomDataTest, Initialization) {
    // Test RoomData with values
    RoomData data;
    data.id = "living_room";
    data.name = "Living Room";
    data.temperature = 72.5f;
    data.activeDevices = 3;
    data.hasTemperature = true;

    EXPECT_EQ(data.id, "living_room");
    EXPECT_EQ(data.name, "Living Room");
    EXPECT_FLOAT_EQ(data.temperature, 72.5f);
    EXPECT_EQ(data.activeDevices, 3);
    EXPECT_TRUE(data.hasTemperature);
}

TEST(RoomDataTest, TemperatureRange) {
    // Test temperature values (Fahrenheit)
    RoomData cold;
    cold.temperature = 32.0f;  // Freezing
    cold.hasTemperature = true;
    EXPECT_FLOAT_EQ(cold.temperature, 32.0f);

    RoomData hot;
    hot.temperature = 100.0f;  // Very hot
    hot.hasTemperature = true;
    EXPECT_FLOAT_EQ(hot.temperature, 100.0f);

    RoomData normal;
    normal.temperature = 68.0f;  // Comfortable
    normal.hasTemperature = true;
    EXPECT_FLOAT_EQ(normal.temperature, 68.0f);
}

TEST(RoomDataTest, DeviceCounts) {
    // Test various device counts
    RoomData noDevices;
    noDevices.activeDevices = 0;
    EXPECT_EQ(noDevices.activeDevices, 0);

    RoomData manyDevices;
    manyDevices.activeDevices = 15;
    EXPECT_EQ(manyDevices.activeDevices, 15);
}

#ifdef SMARTHUB_ENABLE_LVGL

TEST_F(RoomCardTest, SetRoomData) {
    GTEST_SKIP() << "Requires LVGL display initialization";
}

TEST_F(RoomCardTest, ClickCallback) {
    GTEST_SKIP() << "Requires LVGL display initialization";
}

TEST_F(RoomCardTest, RoomIdRetrieval) {
    GTEST_SKIP() << "Requires LVGL display initialization";
}

#endif // SMARTHUB_ENABLE_LVGL

// ============================================================================
// TimeSeriesChart Widget Tests
// ============================================================================

class TimeSeriesChartTest : public ::testing::Test {
protected:
    void SetUp() override {
        themeManager = std::make_unique<ThemeManager>();
    }

    std::unique_ptr<ThemeManager> themeManager;
};

TEST_F(TimeSeriesChartTest, Constants) {
    // Verify chart constants
    EXPECT_EQ(TimeSeriesChart::MAX_POINTS, 60);
}

TEST(TimeRangeTest, Labels) {
    // Test time range labels
    EXPECT_STREQ(timeRangeLabel(TimeRange::Hour1), "1 Hour");
    EXPECT_STREQ(timeRangeLabel(TimeRange::Hours6), "6 Hours");
    EXPECT_STREQ(timeRangeLabel(TimeRange::Hours24), "24 Hours");
    EXPECT_STREQ(timeRangeLabel(TimeRange::Days7), "7 Days");
}

TEST(TimeRangeTest, Seconds) {
    // Test time range in seconds
    EXPECT_EQ(timeRangeSeconds(TimeRange::Hour1), 3600u);
    EXPECT_EQ(timeRangeSeconds(TimeRange::Hours6), 6 * 3600u);
    EXPECT_EQ(timeRangeSeconds(TimeRange::Hours24), 24 * 3600u);
    EXPECT_EQ(timeRangeSeconds(TimeRange::Days7), 7 * 24 * 3600u);
}

TEST(DataPointTest, Construction) {
    // Test DataPoint struct
    DataPoint dp;
    dp.timestamp = 1704067200;  // 2024-01-01 00:00:00
    dp.value = 72.5f;

    EXPECT_EQ(dp.timestamp, 1704067200u);
    EXPECT_FLOAT_EQ(dp.value, 72.5f);
}

TEST(DataPointTest, Vector) {
    // Test vector of data points
    std::vector<DataPoint> data;

    for (int i = 0; i < 10; i++) {
        DataPoint dp;
        dp.timestamp = 1704067200 + i * 60;
        dp.value = 70.0f + i * 0.5f;
        data.push_back(dp);
    }

    EXPECT_EQ(data.size(), 10u);
    EXPECT_FLOAT_EQ(data[0].value, 70.0f);
    EXPECT_FLOAT_EQ(data[9].value, 74.5f);
}

TEST(DataPointTest, AggregateInit) {
    // Test aggregate initialization
    DataPoint dp{1704067200, 72.5f};
    EXPECT_EQ(dp.timestamp, 1704067200u);
    EXPECT_FLOAT_EQ(dp.value, 72.5f);
}

TEST(DataPointTest, EdgeValues) {
    // Test edge values
    DataPoint negative;
    negative.timestamp = 0;
    negative.value = -40.0f;  // Very cold temperature
    EXPECT_EQ(negative.timestamp, 0u);
    EXPECT_FLOAT_EQ(negative.value, -40.0f);

    DataPoint maxTimestamp;
    maxTimestamp.timestamp = UINT64_MAX;
    maxTimestamp.value = 150.0f;
    EXPECT_EQ(maxTimestamp.timestamp, UINT64_MAX);
}

TEST(DataPointTest, LargeDataSet) {
    // Test with MAX_POINTS amount of data
    std::vector<DataPoint> data;
    data.reserve(TimeSeriesChart::MAX_POINTS);

    for (int i = 0; i < TimeSeriesChart::MAX_POINTS; i++) {
        DataPoint dp;
        dp.timestamp = 1704067200 + i * 60;
        dp.value = 70.0f + sinf(i * 0.1f) * 5.0f;
        data.push_back(dp);
    }

    EXPECT_EQ(data.size(), static_cast<size_t>(TimeSeriesChart::MAX_POINTS));
    EXPECT_EQ(data.size(), 60u);
}

TEST(TimeRangeTest, DefaultLabel) {
    // Test default case returns valid string
    // Cast an invalid value to test default case
    TimeRange invalidRange = static_cast<TimeRange>(99);
    const char* label = timeRangeLabel(invalidRange);
    EXPECT_STREQ(label, "24 Hours");
}

TEST(TimeRangeTest, DefaultSeconds) {
    // Test default case returns valid value
    TimeRange invalidRange = static_cast<TimeRange>(99);
    uint64_t seconds = timeRangeSeconds(invalidRange);
    EXPECT_EQ(seconds, 24 * 3600u);
}

TEST(TimeRangeTest, AllRangesHaveLabels) {
    // Verify all enum values have labels
    EXPECT_NE(timeRangeLabel(TimeRange::Hour1), nullptr);
    EXPECT_NE(timeRangeLabel(TimeRange::Hours6), nullptr);
    EXPECT_NE(timeRangeLabel(TimeRange::Hours24), nullptr);
    EXPECT_NE(timeRangeLabel(TimeRange::Days7), nullptr);
}

TEST(TimeRangeTest, SecondsCalculations) {
    // Verify correct time calculations
    EXPECT_EQ(timeRangeSeconds(TimeRange::Hour1), 60u * 60u);
    EXPECT_EQ(timeRangeSeconds(TimeRange::Hours6), 6u * 60u * 60u);
    EXPECT_EQ(timeRangeSeconds(TimeRange::Hours24), 24u * 60u * 60u);
    EXPECT_EQ(timeRangeSeconds(TimeRange::Days7), 7u * 24u * 60u * 60u);
}

TEST(TimeRangeTest, RangeOrdering) {
    // Verify ranges are in increasing order
    EXPECT_LT(timeRangeSeconds(TimeRange::Hour1), timeRangeSeconds(TimeRange::Hours6));
    EXPECT_LT(timeRangeSeconds(TimeRange::Hours6), timeRangeSeconds(TimeRange::Hours24));
    EXPECT_LT(timeRangeSeconds(TimeRange::Hours24), timeRangeSeconds(TimeRange::Days7));
}

#ifdef SMARTHUB_ENABLE_LVGL

TEST_F(TimeSeriesChartTest, SetTitle) {
    GTEST_SKIP() << "Requires LVGL display initialization";
}

TEST_F(TimeSeriesChartTest, SetYRange) {
    GTEST_SKIP() << "Requires LVGL display initialization";
}

TEST_F(TimeSeriesChartTest, SetData) {
    GTEST_SKIP() << "Requires LVGL display initialization";
}

TEST_F(TimeSeriesChartTest, TimeRangeCallback) {
    GTEST_SKIP() << "Requires LVGL display initialization";
}

#endif // SMARTHUB_ENABLE_LVGL

// ============================================================================
// Integration Tests - Multiple Widgets
// ============================================================================

#ifdef SMARTHUB_ENABLE_LVGL

TEST(WidgetIntegration, DashboardLayoutConstants) {
    // Verify widgets fit in 800x480 landscape layout
    constexpr int SCREEN_WIDTH = 800;
    constexpr int SCREEN_HEIGHT = 480;

    int contentHeight = SCREEN_HEIGHT - Header::HEIGHT - NavBar::HEIGHT;

    EXPECT_GT(contentHeight, 0);
    EXPECT_EQ(contentHeight, 370);  // 480 - 50 - 60 = 370px for content

    // Room cards should fit in content area
    int cardsPerRow = SCREEN_WIDTH / (RoomCard::WIDTH + 16);  // 16px spacing
    EXPECT_GE(cardsPerRow, 4);  // At least 4 cards per row

    int rowsVisible = contentHeight / (RoomCard::HEIGHT + 16);
    EXPECT_GE(rowsVisible, 3);  // At least 3 rows visible
}

#endif // SMARTHUB_ENABLE_LVGL
