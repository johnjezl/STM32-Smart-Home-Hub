/**
 * Sensor History Screen Implementation
 */

#include "smarthub/ui/screens/SensorHistoryScreen.hpp"
#include "smarthub/ui/ScreenManager.hpp"
#include "smarthub/ui/ThemeManager.hpp"
#include "smarthub/ui/widgets/TimeSeriesChart.hpp"
#include "smarthub/devices/DeviceManager.hpp"
#include "smarthub/devices/Device.hpp"
#include "smarthub/core/Logger.hpp"

#include <cstdlib>
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
    lv_obj_set_size(m_backBtn, 40, 40);
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
    lv_obj_set_size(m_content, LV_PCT(100), lv_pct(100) - 60);
    lv_obj_align(m_content, LV_ALIGN_TOP_MID, 0, 60);
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

        // Update current value
        auto value = device->getProperty("value");
        if (value.is_number() && m_currentValueLabel) {
            char buf[32];
            std::string unit;

            switch (device->type()) {
                case DeviceType::TemperatureSensor:
                    unit = "°F";
                    snprintf(buf, sizeof(buf), "%.1f%s", value.get<float>(), unit.c_str());
                    if (m_chart) {
                        m_chart->setYAxis("Temperature", "°F");
                        m_chart->setYRange(50, 90);
                    }
                    break;
                case DeviceType::HumiditySensor:
                    unit = "%";
                    snprintf(buf, sizeof(buf), "%.0f%s", value.get<float>(), unit.c_str());
                    if (m_chart) {
                        m_chart->setYAxis("Humidity", "%");
                        m_chart->setYRange(0, 100);
                    }
                    break;
                default:
                    snprintf(buf, sizeof(buf), "%.1f", value.get<float>());
                    break;
            }
            lv_label_set_text(m_currentValueLabel, buf);
        }

        // Update chart title
        if (m_chart) {
            m_chart->setTitle(device->name() + " History");
        }
    }

    // Load or generate history data
    // TODO: In production, load from database
    generateMockData();

    if (m_chart && !m_historyData.empty()) {
        m_chart->setData(m_historyData);
    }
}

void SensorHistoryScreen::generateMockData() {
    m_historyData.clear();

    auto device = m_deviceManager.getDevice(m_sensorId);
    float baseValue = 72.0f;  // Default base temp
    float variation = 5.0f;

    if (device) {
        auto value = device->getProperty("value");
        if (value.is_number()) {
            baseValue = value.get<float>();
        }

        if (device->type() == DeviceType::HumiditySensor) {
            baseValue = 45.0f;
            variation = 15.0f;
        }
    }

    // Generate realistic-looking sensor data
    uint64_t now = static_cast<uint64_t>(time(nullptr));
    TimeRange range = m_chart ? m_chart->timeRange() : TimeRange::Hours24;
    uint64_t rangeSeconds = timeRangeSeconds(range);
    int numPoints = TimeSeriesChart::MAX_POINTS;
    uint64_t interval = rangeSeconds / numPoints;

    // Seed random with sensor ID for consistent patterns
    unsigned int seed = 0;
    for (char c : m_sensorId) {
        seed += static_cast<unsigned int>(c);
    }
    srand(seed + static_cast<unsigned int>(now / 3600));  // Change hourly

    float prevValue = baseValue;
    for (int i = 0; i < numPoints; i++) {
        DataPoint dp;
        dp.timestamp = now - (numPoints - 1 - i) * interval;

        // Random walk with mean reversion
        float delta = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 2.0f;
        float meanReversion = (baseValue - prevValue) * 0.1f;
        prevValue = prevValue + delta + meanReversion;

        // Add time-of-day variation for temperature
        if (device && device->type() == DeviceType::TemperatureSensor) {
            // Simulate daily temperature cycle
            float hourOfDay = fmod(static_cast<float>(dp.timestamp) / 3600.0f, 24.0f);
            float dailyCycle = sinf((hourOfDay - 6.0f) * 3.14159f / 12.0f) * 3.0f;
            dp.value = prevValue + dailyCycle;
        } else {
            dp.value = prevValue;
        }

        // Clamp to reasonable range
        float minVal = baseValue - variation * 2;
        float maxVal = baseValue + variation * 2;
        dp.value = std::max(minVal, std::min(maxVal, dp.value));

        m_historyData.push_back(dp);
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
