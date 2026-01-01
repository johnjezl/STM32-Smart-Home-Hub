/**
 * Time Series Chart Widget
 *
 * LVGL chart wrapper for displaying sensor history data.
 */
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

#ifdef SMARTHUB_ENABLE_LVGL
#include <lvgl.h>
#endif

namespace smarthub {
namespace ui {

class ThemeManager;

/**
 * Time range for chart data
 */
enum class TimeRange {
    Hour1,    // Last 1 hour
    Hours6,   // Last 6 hours
    Hours24,  // Last 24 hours
    Days7     // Last 7 days
};

/**
 * Data point for time series
 */
struct DataPoint {
    uint64_t timestamp;  // Unix timestamp in seconds
    float value;
};

/**
 * TimeSeriesChart widget for sensor history
 *
 * Displays line chart with time-based X axis and value Y axis.
 */
class TimeSeriesChart {
public:
    using TimeRangeCallback = std::function<void(TimeRange)>;

#ifdef SMARTHUB_ENABLE_LVGL
    /**
     * Create chart widget
     * @param parent Parent LVGL object
     * @param theme Theme manager for styling
     */
    TimeSeriesChart(lv_obj_t* parent, const ThemeManager& theme);
#else
    TimeSeriesChart();
#endif
    ~TimeSeriesChart();

    /**
     * Set chart title
     */
    void setTitle(const std::string& title);

    /**
     * Set Y-axis label and unit
     */
    void setYAxis(const std::string& label, const std::string& unit);

    /**
     * Set Y-axis range
     */
    void setYRange(float min, float max);

    /**
     * Set the data points to display
     * @param data Vector of data points (will be resampled to fit chart)
     */
    void setData(const std::vector<DataPoint>& data);

    /**
     * Set data from simple float array (evenly spaced)
     * @param values Float values
     * @param count Number of values
     */
    void setData(const float* values, size_t count);

    /**
     * Clear all data
     */
    void clearData();

    /**
     * Set the current time range
     */
    void setTimeRange(TimeRange range);

    /**
     * Get current time range
     */
    TimeRange timeRange() const { return m_timeRange; }

    /**
     * Set callback for time range changes
     */
    void onTimeRangeChanged(TimeRangeCallback callback);

    /**
     * Show/hide time range selector
     */
    void showTimeRangeSelector(bool show);

#ifdef SMARTHUB_ENABLE_LVGL
    /**
     * Get the root object
     */
    lv_obj_t* obj() const { return m_container; }
#endif

    /**
     * Get max data points displayed
     */
    static constexpr int MAX_POINTS = 60;

private:
#ifdef SMARTHUB_ENABLE_LVGL
    void createLayout();
    void createTimeRangeDropdown();
    void updateChartData();

    static void timeRangeHandler(lv_event_t* e);

    lv_obj_t* m_container = nullptr;
    lv_obj_t* m_titleLabel = nullptr;
    lv_obj_t* m_chart = nullptr;
    lv_chart_series_t* m_series = nullptr;
    lv_obj_t* m_timeRangeDropdown = nullptr;
    lv_obj_t* m_yAxisLabel = nullptr;
    lv_obj_t* m_minLabel = nullptr;
    lv_obj_t* m_maxLabel = nullptr;

    const ThemeManager& m_theme;
#endif

    std::string m_title;
    std::string m_yLabel;
    std::string m_unit;
    float m_yMin = 0.0f;
    float m_yMax = 100.0f;
    TimeRange m_timeRange = TimeRange::Hours24;
    std::vector<DataPoint> m_data;
    TimeRangeCallback m_onTimeRangeChanged;
};

/**
 * Helper to get time range label
 */
inline const char* timeRangeLabel(TimeRange range) {
    switch (range) {
        case TimeRange::Hour1:   return "1 Hour";
        case TimeRange::Hours6:  return "6 Hours";
        case TimeRange::Hours24: return "24 Hours";
        case TimeRange::Days7:   return "7 Days";
        default: return "24 Hours";
    }
}

/**
 * Helper to get time range in seconds
 */
inline uint64_t timeRangeSeconds(TimeRange range) {
    switch (range) {
        case TimeRange::Hour1:   return 3600;
        case TimeRange::Hours6:  return 6 * 3600;
        case TimeRange::Hours24: return 24 * 3600;
        case TimeRange::Days7:   return 7 * 24 * 3600;
        default: return 24 * 3600;
    }
}

} // namespace ui
} // namespace smarthub
