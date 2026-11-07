/**
 * SmartHub Alarm Sensor Implementation
 */

#include <smarthub/devices/types/AlarmSensor.hpp>

namespace smarthub {

AlarmSensor::AlarmSensor(const std::string& id,
                         const std::string& name,
                         DeviceType type,
                         const std::string& protocol,
                         const std::string& protocolAddress)
    : Device(id, name, type, protocol, protocolAddress)
{
    if (type == DeviceType::SmokeSensor) {
        addCapability(DeviceCapability::Smoke);
    } else if (type == DeviceType::WaterSensor) {
        addCapability(DeviceCapability::Water);
    } else if (type == DeviceType::CoSensor) {
        addCapability(DeviceCapability::CarbonMonoxide);
    }
}

bool AlarmSensor::alarmActive() const {
    const auto state = getProperty("alarm");
    return state.is_boolean() && state.get<bool>();
}

void AlarmSensor::onStateChange(const std::string& property, const nlohmann::json& value) {
    (void)property;
    (void)value;
}

} // namespace smarthub
