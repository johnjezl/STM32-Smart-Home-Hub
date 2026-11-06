/**
 * Sensor History Screen Implementation
 */

#include "smarthub/ui/screens/SensorHistoryScreen.hpp"
#include "smarthub/ui/ScreenManager.hpp"
#include "smarthub/ui/ThemeManager.hpp"
#include "smarthub/ui/widgets/TimeSeriesChart.hpp"
#include "smarthub/devices/DeviceManager.hpp"
#include "smarthub/devices/Device.hpp"
#include "smarthub/devices/SensorDisplay.hpp"
#include <algorithm>
#include "smarthub/core/Logger.hpp"

#include <ctime>
#include <cmath>

namespace smarthub {
namespace ui {

SensorHistoryScreen::SensorHistoryScreen(ScreenManager& screenManager,
                                         ThemeManager& theme,
                                         DeviceManager& deviceManager)
    : Screen(screenManager, "sensor_history")
    , m_theme(theme)
    , m_deviceManager(deviceManager)
{
}

SensorHistoryScreen::~SensorHistoryScreen() = default;

void SensorHistoryScreen::setSensorId(const std::string& sensorId) {
    m_sensorId = sensorId;
}

void SensorHistoryScreen::onCreate() {
#ifdef SMARTHUB_ENABLE_LVGL
    if (!m_container) return;

    lv_obj_set_style_bg_color(m_container, m_theme.background(), 0);

    createHeader();
    createChart();

    LOG_DEBUG("SensorHistoryScreen created");
#endif
}

void SensorHistoryScreen::onShow() {
#ifdef SMARTHUB_ENABLE_LVGL
    loadSensorData();
    m_updateMs = 0;
    LOG_DEBUG("SensorHistoryScreen shown for sensor: %s", m_sensorId.c_str());
#endif
}

void SensorHistoryScreen::onUpdate(uint32_t deltaMs) {
#ifdef SMARTHUB_ENABLE_LVGL
    m_updateMs += deltaMs;
    if (m_updateMs >= UPDATE_INTERVAL) {
        loadSensorData();
        m_updateMs = 0;
    }
#else
    (void)deltaMs;
#endif
}

void SensorHistoryScreen::onDestroy() {
#ifdef SMARTHUB_ENABLE_LVGL
    m_chart.reset();
#endif
}

#ifdef SMARTHUB_ENABLE_LVGL

void SensorHistoryScreen::createHeader() {
    // Header bar
    lv_obj_t* header = lv_obj_create(m_container);
    lv_obj_set_size(header, LV_PCT(100), 60);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    m_theme.applyHeaderStyle(header);

    // Back button
    m_backBtn = lv_btn_create(header);
    lv_obj_set_size(m_backBtn, 48, 48);
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
    lv_label_set_text(m_titleLabel, "Sensor History");
    lv_obj_set_style_text_color(m_titleLabel, m_theme.textPrimary(), 0);
    lv_obj_align(m_titleLabel, LV_ALIGN_LEFT_MID, 60, -8);

    // Subtitle (sensor name)
    m_subtitleLabel = lv_label_create(header);
    lv_label_set_text(m_subtitleLabel, "");
    lv_obj_set_style_text_color(m_subtitleLabel, m_theme.textSecondary(), 0);
    lv_obj_align(m_subtitleLabel, LV_ALIGN_LEFT_MID, 60, 10);

    // Current value (large, right side)
    m_currentValueLabel = lv_label_create(header);
    lv_label_set_text(m_currentValueLabel, "--");
    lv_obj_set_style_text_color(m_currentValueLabel, m_theme.primary(), 0);
    lv_obj_align(m_currentValueLabel, LV_ALIGN_RIGHT_MID, -ThemeManager::SPACING_MD, 0);
}

void SensorHistoryScreen::createChart() {
    // Chart container
    m_content = lv_obj_create(m_container);
    constexpr int HEADER_HEIGHT = 60;
    lv_obj_set_size(m_content, LV_PCT(100), LV_VER_RES - HEADER_HEIGHT);
    lv_obj_align(m_content, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT);
    lv_obj_set_style_bg_opa(m_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_content, 0, 0);
    lv_obj_set_style_pad_all(m_content, ThemeManager::SPACING_MD, 0);
    lv_obj_clear_flag(m_content, LV_OBJ_FLAG_SCROLLABLE);

    // Create chart widget
    m_chart = std::make_unique<TimeSeriesChart>(m_content, m_theme);
    m_chart->setTitle("History");
    m_chart->setYAxis("Temperature", "°F");
    m_chart->setYRange(50, 90);
    m_chart->setTimeRange(TimeRange::Hours24);

    m_emptyLabel = lv_label_create(m_container);
    lv_obj_set_style_text_color(m_emptyLabel, m_theme.textSecondary(), 0);
    lv_obj_center(m_emptyLabel);
    lv_obj_add_flag(m_emptyLabel, LV_OBJ_FLAG_HIDDEN);

    m_chart->onTimeRangeChanged([this](TimeRange range) {
        onTimeRangeChanged(range);
    });
}

void SensorHistoryScreen::loadSensorData() {
    auto device = m_deviceManager.getDevice(m_sensorId);

    if (device) {
        // Update title with sensor name
        if (m_subtitleLabel) {
            lv_label_set_text(m_subtitleLabel, device->name().c_str());
        }

        if (m_currentValueLabel) {
            lv_label_set_text(m_currentValueLabel, sensorValueText(device->type(), device->getState()).c_str());
        }

        // Update chart title
        if (m_chart) {
            m_chart->setTitle(device->name() + " History");
        }
    }

    const std::string property = device ? historyProperty(device->type()) : "";
    const bool celsius = property == "temperature";

    m_historyData.clear();
    if (!property.empty() && m_chart) {
        const uint64_t range = timeRangeSeconds(m_chart->timeRange());
        const int64_t from = static_cast<int64_t>(time(nullptr)) - static_cast<int64_t>(range);
        for (const SensorSample& sample :
             m_deviceManager.sensorHistory(m_sensorId, property, from, TimeSeriesChart::MAX_POINTS)) {
            const double value = celsius ? sample.value * 9.0 / 5.0 + 32.0 : sample.value;
            m_historyData.push_back({static_cast<uint64_t>(sample.timestamp), static_cast<float>(value)});
        }
    }

    // The chart is reused from sensor to sensor, so this is all set every time
    if (m_chart) {
        if (celsius) {
            m_chart->setYAxis("Temperature", "°F");
        } else if (property == "humidity") {
            m_chart->setYAxis("Humidity", "%");
        } else {
            m_chart->setYAxis("", "");
        }

        // Wide enough to see the shape, and to fit whatever was recorded
        float low = celsius ? 60.0f : 0.0f;
        float high = celsius ? 80.0f : 100.0f;
        for (const DataPoint& point : m_historyData) {
            low = std::min(low, point.value - 5.0f);
            high = std::max(high, point.value + 5.0f);
        }
        m_chart->setYRange(low, high);
        m_chart->setData(m_historyData);
    }

    if (m_emptyLabel) {
        if (m_historyData.empty()) {
            lv_label_set_text(m_emptyLabel, property.empty() ? "No history for this sensor" : "No history yet");
            lv_obj_clear_flag(m_emptyLabel, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(m_emptyLabel, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void SensorHistoryScreen::onBackClicked() {
    m_screenManager.goBack();
}

void SensorHistoryScreen::onTimeRangeChanged(TimeRange range) {
    LOG_DEBUG("Time range changed to: %s", timeRangeLabel(range));
    loadSensorData();
}

void SensorHistoryScreen::backButtonHandler(lv_event_t* e) {
    SensorHistoryScreen* self = static_cast<SensorHistoryScreen*>(lv_event_get_user_data(e));
    if (self) {
        self->onBackClicked();
    }
}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
