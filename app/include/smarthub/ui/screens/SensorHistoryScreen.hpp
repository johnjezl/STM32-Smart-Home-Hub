/**
 * Sensor History Screen
 *
 * Shows historical sensor data in a time series chart.
 */
#pragma once

#include "smarthub/ui/Screen.hpp"
#include "smarthub/ui/widgets/TimeSeriesChart.hpp"
#include <memory>
#include <string>
#include <vector>

namespace smarthub {

class DeviceManager;

namespace ui {

class ThemeManager;
class TimeSeriesChart;

/**
 * Sensor history screen showing time series chart
 */
class SensorHistoryScreen : public Screen {
public:
    SensorHistoryScreen(ScreenManager& screenManager,
                        ThemeManager& theme,
                        DeviceManager& deviceManager);
    ~SensorHistoryScreen() override;

    /**
     * Set the sensor to display history for
     */
    void setSensorId(const std::string& sensorId);

    /**
     * Get current sensor ID
     */
    const std::string& sensorId() const { return m_sensorId; }

    void onCreate() override;
    void onShow() override;
    void onUpdate(uint32_t deltaMs) override;
    void onDestroy() override;

private:
#ifdef SMARTHUB_ENABLE_LVGL
    void createHeader();
    void createChart();
    void loadSensorData();
    void generateMockData();

    void onBackClicked();
    void onTimeRangeChanged(TimeRange range);

    static void backButtonHandler(lv_event_t* e);

    lv_obj_t* m_content = nullptr;
    lv_obj_t* m_backBtn = nullptr;
    lv_obj_t* m_titleLabel = nullptr;
    lv_obj_t* m_subtitleLabel = nullptr;
    lv_obj_t* m_currentValueLabel = nullptr;

    std::unique_ptr<TimeSeriesChart> m_chart;
#endif

    ThemeManager& m_theme;
    DeviceManager& m_deviceManager;
    std::string m_sensorId;
    std::vector<DataPoint> m_historyData;

    uint32_t m_updateMs = 0;
    static constexpr uint32_t UPDATE_INTERVAL = 60000;  // 1 minute
};

} // namespace ui
} // namespace smarthub
