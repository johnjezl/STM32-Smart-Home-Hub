/**
 * SmartHub Alarm Sensor
 *
 * A sensor whose job is to raise an alarm: smoke, water, carbon monoxide and the like.
 */
#pragma once

#include <smarthub/devices/Device.hpp>

namespace smarthub {

class AlarmSensor : public Device {
public:
    /**
     * @param type SmokeSensor, WaterSensor, CoSensor or AlarmSensor
     */
    AlarmSensor(const std::string& id,
                const std::string& name,
                DeviceType type,
                const std::string& protocol = "local",
                const std::string& protocolAddress = "");

    /** Whether it's raising its alarm. False before the first report. */
    bool alarmActive() const;

protected:
    void onStateChange(const std::string& property, const nlohmann::json& value) override;
};

} // namespace smarthub
