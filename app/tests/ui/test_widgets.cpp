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

TEST(ChartSlot, PlacesPointsByTime) {
    const uint64_t now = 1000000;
    EXPECT_EQ(chartSlot(now, now, 3600, 60), 59);
    EXPECT_EQ(chartSlot(now - 3600, now, 3600, 60), 0);
    EXPECT_EQ(chartSlot(now - 1800, now, 3600, 60), 29);
    EXPECT_EQ(chartSlot(now - 3601, now, 3600, 60), -1) << "too old";
    EXPECT_EQ(chartSlot(now + 1, now, 3600, 60), -1) << "the clock moved";
}

TEST(ChartValues, JoinsReadingsThatAreCloseTogether) {
    const uint64_t now = 1000000;
    // A reading every 5 minutes on the one-hour view, whose points are about a minute apart
    std::vector<DataPoint> data;
    for (int i = 12; i >= 0; i--) {
        data.push_back({now - i * 300, 70.0f});
    }

    const auto values = chartValues(data, now, 3600, 60);

    for (const auto& value : values) {
        ASSERT_TRUE(value.has_value());
        EXPECT_FLOAT_EQ(*value, 70.0f);
    }
}

TEST(ChartValues, InterpolatesBetweenReadings) {
    const uint64_t now = 1000000;
    const auto values = chartValues({{now - 1200, 60.0f}, {now, 80.0f}}, now, 3540, 60);
    EXPECT_FLOAT_EQ(*values[39], 60.0f);
    EXPECT_FLOAT_EQ(*values[49], 70.0f);
    EXPECT_FLOAT_EQ(*values[59], 80.0f);
    EXPECT_FALSE(values[38].has_value());
}

TEST(ChartValues, NeighbouringPointsOnALongViewAreJoined) {
    const uint64_t now = 1000000;
    const uint64_t week = 7 * 86400;
    const uint64_t slot = week / 59;
    // Averaged points whose timing skips a slot, from a sensor that never stopped reporting
    const auto values = chartValues({{now - 3 * slot, 70.0f}, {now - slot, 72.0f}}, now, week, 60);
    EXPECT_TRUE(values[57].has_value());
}

TEST(ChartValues, LongSilenceStaysAGap) {
    const uint64_t now = 1000000;
    const uint64_t day = 86400;
    const auto values = chartValues({{now - day + 100, 70.0f}, {now - 3600, 72.0f}}, now, day, 60);
    int drawn = 0;
    for (const auto& value : values) {
        drawn += value.has_value() ? 1 : 0;
    }
    EXPECT_EQ(drawn, 2);
}

TEST(ChartValues, ReadingsOutOfRangeAreLeftOut) {
    const uint64_t now = 1000000;
    const auto values = chartValues({{now - 7200, 1.0f}, {now + 50, 2.0f}}, now, 3600, 60);
    for (const auto& value : values) {
        EXPECT_FALSE(value.has_value());
    }
}

TEST(RoomDataTest, DefaultValues) {
    // Test RoomData default construction
    RoomData data;

    EXPECT_EQ(data.id, "");
    EXPECT_EQ(data.name, "");
    EXPECT_EQ(data.climate, "");
    EXPECT_EQ(data.activeDevices, 0);
    EXPECT_FALSE(data.motionDetected);
}

TEST(RoomDataTest, Initialization) {
    // Test RoomData with values
    RoomData data;
    data.id = "living_room";
    data.name = "Living Room";
    data.climate = "72°F";
    data.activeDevices = 3;
    data.motionDetected = true;

    EXPECT_EQ(data.id, "living_room");
    EXPECT_EQ(data.name, "Living Room");
    EXPECT_EQ(data.climate, "72°F");
    EXPECT_EQ(data.activeDevices, 3);
    EXPECT_TRUE(data.motionDetected);
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
