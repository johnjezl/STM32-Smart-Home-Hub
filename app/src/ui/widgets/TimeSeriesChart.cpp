/**
 * Time Series Chart Widget Implementation
 */

#include "smarthub/ui/widgets/TimeSeriesChart.hpp"
#include "smarthub/ui/ThemeManager.hpp"
#include "smarthub/core/Logger.hpp"

namespace smarthub {
namespace ui {

#ifdef SMARTHUB_ENABLE_LVGL

TimeSeriesChart::TimeSeriesChart(lv_obj_t* parent, const ThemeManager& theme)
    : m_theme(theme)
{
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 0, 0);
    lv_obj_clear_flag(m_container, LV_OBJ_FLAG_SCROLLABLE);

    createLayout();
}

TimeSeriesChart::~TimeSeriesChart() = default;

void TimeSeriesChart::createLayout() {
    // Title label
    m_titleLabel = lv_label_create(m_container);
    lv_label_set_text(m_titleLabel, "Sensor History");
    lv_obj_set_style_text_color(m_titleLabel, m_theme.textPrimary(), 0);
    lv_obj_align(m_titleLabel, LV_ALIGN_TOP_LEFT, 0, 0);

    // Time range dropdown
    createTimeRangeDropdown();

    // Y-axis label (rotated)
    m_yAxisLabel = lv_label_create(m_container);
    lv_label_set_text(m_yAxisLabel, "Value");
    lv_obj_set_style_text_color(m_yAxisLabel, m_theme.textSecondary(), 0);
    lv_obj_align(m_yAxisLabel, LV_ALIGN_LEFT_MID, 5, 0);

    // Chart
    m_chart = lv_chart_create(m_container);
    lv_obj_set_size(m_chart, lv_pct(85), lv_pct(70));
    lv_obj_align(m_chart, LV_ALIGN_CENTER, 20, 15);

    // Chart styling
    lv_obj_set_style_bg_color(m_chart, m_theme.surface(), 0);
    lv_obj_set_style_border_color(m_chart, m_theme.surfaceVariant(), 0);
    lv_obj_set_style_border_width(m_chart, 1, 0);
    lv_obj_set_style_radius(m_chart, ThemeManager::CARD_RADIUS, 0);
    lv_obj_set_style_pad_all(m_chart, ThemeManager::SPACING_SM, 0);

    // Line style
    lv_obj_set_style_line_color(m_chart, m_theme.primary(), LV_PART_ITEMS);
    lv_obj_set_style_line_width(m_chart, 2, LV_PART_ITEMS);

    // Grid lines
    lv_obj_set_style_line_color(m_chart, m_theme.surfaceVariant(), LV_PART_MAIN);
    lv_obj_set_style_line_opa(m_chart, LV_OPA_50, LV_PART_MAIN);

    // Configure chart
    lv_chart_set_type(m_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(m_chart, MAX_POINTS);
    lv_chart_set_range(m_chart, LV_CHART_AXIS_PRIMARY_Y,
                       static_cast<int>(m_yMin), static_cast<int>(m_yMax));

    // Add series
    m_series = lv_chart_add_series(m_chart, m_theme.primary(), LV_CHART_AXIS_PRIMARY_Y);

    // Min/Max labels
    m_maxLabel = lv_label_create(m_container);
    lv_obj_set_style_text_color(m_maxLabel, m_theme.textSecondary(), 0);
    lv_obj_align_to(m_maxLabel, m_chart, LV_ALIGN_OUT_LEFT_TOP, -5, 10);

    m_minLabel = lv_label_create(m_container);
    lv_obj_set_style_text_color(m_minLabel, m_theme.textSecondary(), 0);
    lv_obj_align_to(m_minLabel, m_chart, LV_ALIGN_OUT_LEFT_BOTTOM, -5, -10);

    updateChartData();
}

void TimeSeriesChart::createTimeRangeDropdown() {
    m_timeRangeDropdown = lv_dropdown_create(m_container);
    lv_dropdown_set_options(m_timeRangeDropdown,
                            "1 Hour\n6 Hours\n24 Hours\n7 Days");
    lv_dropdown_set_selected(m_timeRangeDropdown, 2);  // Default: 24 Hours
    lv_obj_set_width(m_timeRangeDropdown, 120);
    lv_obj_align(m_timeRangeDropdown, LV_ALIGN_TOP_RIGHT, 0, 0);

    // Style dropdown
    lv_obj_set_style_bg_color(m_timeRangeDropdown, m_theme.surface(), 0);
    lv_obj_set_style_text_color(m_timeRangeDropdown, m_theme.textPrimary(), 0);
    lv_obj_set_style_border_color(m_timeRangeDropdown, m_theme.surfaceVariant(), 0);

    lv_obj_add_event_cb(m_timeRangeDropdown, timeRangeHandler, LV_EVENT_VALUE_CHANGED, this);
}

void TimeSeriesChart::setTitle(const std::string& title) {
    m_title = title;
    if (m_titleLabel) {
        lv_label_set_text(m_titleLabel, title.c_str());
    }
}

void TimeSeriesChart::setYAxis(const std::string& label, const std::string& unit) {
    m_yLabel = label;
    m_unit = unit;
    if (m_yAxisLabel) {
        lv_label_set_text(m_yAxisLabel, label.c_str());
    }
    updateChartData();
}

void TimeSeriesChart::setYRange(float min, float max) {
    m_yMin = min;
    m_yMax = max;
    if (m_chart) {
        lv_chart_set_range(m_chart, LV_CHART_AXIS_PRIMARY_Y,
                           static_cast<int>(min), static_cast<int>(max));
    }
    updateChartData();
}

void TimeSeriesChart::setData(const std::vector<DataPoint>& data) {
    m_data = data;
    updateChartData();
}

void TimeSeriesChart::setData(const float* values, size_t count) {
    m_data.clear();
    uint64_t now = static_cast<uint64_t>(time(nullptr));
    uint64_t interval = timeRangeSeconds(m_timeRange) / count;

    for (size_t i = 0; i < count; i++) {
        DataPoint dp;
        dp.timestamp = now - (count - 1 - i) * interval;
        dp.value = values[i];
        m_data.push_back(dp);
    }
    updateChartData();
}

void TimeSeriesChart::clearData() {
    m_data.clear();
    if (m_chart && m_series) {
        for (int i = 0; i < MAX_POINTS; i++) {
            lv_chart_set_value_by_id(m_chart, m_series, i, LV_CHART_POINT_NONE);
        }
        lv_chart_refresh(m_chart);
    }
}

void TimeSeriesChart::setTimeRange(TimeRange range) {
    m_timeRange = range;
    if (m_timeRangeDropdown) {
        lv_dropdown_set_selected(m_timeRangeDropdown, static_cast<int>(range));
    }
    updateChartData();
}

void TimeSeriesChart::onTimeRangeChanged(TimeRangeCallback callback) {
    m_onTimeRangeChanged = std::move(callback);
}

void TimeSeriesChart::showTimeRangeSelector(bool show) {
    if (m_timeRangeDropdown) {
        if (show) {
            lv_obj_clear_flag(m_timeRangeDropdown, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(m_timeRangeDropdown, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void TimeSeriesChart::updateChartData() {
    if (!m_chart || !m_series) return;

    // Update min/max labels
    if (m_minLabel) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f%s", m_yMin, m_unit.c_str());
        lv_label_set_text(m_minLabel, buf);
    }
    if (m_maxLabel) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f%s", m_yMax, m_unit.c_str());
        lv_label_set_text(m_maxLabel, buf);
    }

    if (m_data.empty()) {
        clearData();
        return;
    }

    // Resample data to fit MAX_POINTS
    size_t dataSize = m_data.size();
    if (dataSize <= MAX_POINTS) {
        // Use data directly
        for (size_t i = 0; i < MAX_POINTS; i++) {
            if (i < dataSize) {
                int value = static_cast<int>(m_data[i].value);
                lv_chart_set_value_by_id(m_chart, m_series, i, value);
            } else {
                lv_chart_set_value_by_id(m_chart, m_series, i, LV_CHART_POINT_NONE);
            }
        }
    } else {
        // Resample: take evenly spaced samples
        for (int i = 0; i < MAX_POINTS; i++) {
            size_t idx = (i * (dataSize - 1)) / (MAX_POINTS - 1);
            int value = static_cast<int>(m_data[idx].value);
            lv_chart_set_value_by_id(m_chart, m_series, i, value);
        }
    }

    lv_chart_refresh(m_chart);
}

void TimeSeriesChart::timeRangeHandler(lv_event_t* e) {
    TimeSeriesChart* self = static_cast<TimeSeriesChart*>(lv_event_get_user_data(e));
    lv_obj_t* dropdown = lv_event_get_target(e);

    if (self && dropdown) {
        uint16_t selected = lv_dropdown_get_selected(dropdown);
        TimeRange range = static_cast<TimeRange>(selected);
        self->m_timeRange = range;

        if (self->m_onTimeRangeChanged) {
            self->m_onTimeRangeChanged(range);
        }
    }
}

#else // !SMARTHUB_ENABLE_LVGL

TimeSeriesChart::TimeSeriesChart() = default;
TimeSeriesChart::~TimeSeriesChart() = default;

void TimeSeriesChart::setTitle(const std::string& title) { m_title = title; }
void TimeSeriesChart::setYAxis(const std::string& label, const std::string& unit) {
    m_yLabel = label;
    m_unit = unit;
}
void TimeSeriesChart::setYRange(float min, float max) { m_yMin = min; m_yMax = max; }
void TimeSeriesChart::setData(const std::vector<DataPoint>& data) { m_data = data; }
void TimeSeriesChart::setData(const float*, size_t) {}
void TimeSeriesChart::clearData() { m_data.clear(); }
void TimeSeriesChart::setTimeRange(TimeRange range) { m_timeRange = range; }
void TimeSeriesChart::onTimeRangeChanged(TimeRangeCallback callback) {
    m_onTimeRangeChanged = std::move(callback);
}
void TimeSeriesChart::showTimeRangeSelector(bool) {}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
