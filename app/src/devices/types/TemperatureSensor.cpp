/**
 * SmartHub Temperature Sensor Implementation
 */

#include <smarthub/devices/types/TemperatureSensor.hpp>
#include <algorithm>

namespace smarthub {

TemperatureSensor::TemperatureSensor(const std::string& id,
                                     const std::string& name,
                                     const std::string& protocol,
                                     const std::string& protocolAddress,
                                     bool hasHumidity,
                                     bool hasBattery)
    : Device(id, name, DeviceType::TemperatureSensor, protocol, protocolAddress)
    , m_hasHumidity(hasHumidity)
    , m_hasBattery(hasBattery)
{
    addCapability(DeviceCapability::Temperature);

    if (hasHumidity) {
        addCapability(DeviceCapability::Humidity);
        m_type = DeviceType::MultiSensor;
    }

    if (hasBattery) {
        addCapability(DeviceCapability::Battery);
    }

    TemperatureSensor::setConfig(nlohmann::json::object());

    // Temperature and humidity stay unset until the first report: 0 would read as a real 32°F
    if (hasBattery) {
        setStateInternal("battery", 100);
    }

    setAvailability(DeviceAvailability::Online);
}

double TemperatureSensor::temperature() const {
    auto state = getProperty("temperature");
    if (state.is_number()) {
        return state.get<double>();
    }
    return 0.0;
}

double TemperatureSensor::humidity() const {
    if (!m_hasHumidity) {
        return -1.0;
    }
    auto state = getProperty("humidity");
    if (state.is_number()) {
        return state.get<double>();
    }
    return 0.0;
}

int TemperatureSensor::batteryLevel() const {
    if (!m_hasBattery) {
        return -1;
    }
    auto state = getProperty("battery");
    if (state.is_number()) {
        return state.get<int>();
    }
    return 100;
}

void TemperatureSensor::setTemperature(double celsius) {
    setState("temperature", celsius);
    updateLastSeen();
}

void TemperatureSensor::setHumidity(double percent) {
    if (m_hasHumidity) {
        percent = std::clamp(percent, 0.0, 100.0);
        setState("humidity", percent);
        updateLastSeen();
    }
}

void TemperatureSensor::setBatteryLevel(int percent) {
    if (m_hasBattery) {
        percent = std::clamp(percent, 0, 100);
        setState("battery", percent);
    }
}

void TemperatureSensor::onStateChange(const std::string& property, const nlohmann::json& value) {
    (void)property;
    (void)value;
}

void TemperatureSensor::setConfig(const nlohmann::json& config) {
    nlohmann::json merged = config.is_object() ? config : nlohmann::json::object();
    merged["has_humidity"] = m_hasHumidity;
    merged["has_battery"] = m_hasBattery;
    Device::setConfig(merged);
}

} // namespace smarthub
