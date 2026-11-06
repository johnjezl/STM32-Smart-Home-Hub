/**
 * Turns sensor state into what the screens show.
 */

#include <smarthub/devices/SensorDisplay.hpp>

#include <cstdio>

namespace smarthub {

namespace {

std::optional<double> numberIn(const nlohmann::json& state, const char* key) {
    if (!state.is_object() || !state.contains(key) || !state[key].is_number()) {
        return std::nullopt;
    }
    return state[key].get<double>();
}

std::optional<bool> boolIn(const nlohmann::json& state, const char* key) {
    if (!state.is_object() || !state.contains(key) || !state[key].is_boolean()) {
        return std::nullopt;
    }
    return state[key].get<bool>();
}

bool reportsTemperature(DeviceType type) {
    return type == DeviceType::TemperatureSensor || type == DeviceType::MultiSensor;
}

// A plain temperature sensor can still report humidity, for instance over Zigbee
bool reportsHumidity(DeviceType type, const nlohmann::json& state) {
    return type == DeviceType::HumiditySensor || type == DeviceType::MultiSensor ||
           (type == DeviceType::TemperatureSensor && humidityPercent(state).has_value());
}

} // namespace

std::optional<double> temperatureFahrenheit(const nlohmann::json& state) {
    auto celsius = numberIn(state, "temperature");
    if (!celsius) return std::nullopt;
    return *celsius * 9.0 / 5.0 + 32.0;
}

std::optional<double> humidityPercent(const nlohmann::json& state) {
    return numberIn(state, "humidity");
}

std::string temperatureText(const std::optional<double>& fahrenheit, int decimals) {
    if (!fahrenheit) return "--°F";
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.*f°F", decimals, *fahrenheit);
    return buf;
}

std::string humidityText(const std::optional<double>& percent) {
    if (!percent) return "--%";
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.0f%%", *percent);
    return buf;
}

std::string sensorValueText(DeviceType type, const nlohmann::json& state) {
    const auto temperature = temperatureFahrenheit(state);
    const auto humidity = humidityPercent(state);

    switch (type) {
        case DeviceType::TemperatureSensor:
        case DeviceType::MultiSensor:
            if (!temperature && !humidity) return "--";
            if (!humidity) return temperatureText(temperature, 1);
            return temperatureText(temperature, 1) + " / " + humidityText(humidity);
        case DeviceType::HumiditySensor:
            return humidity ? humidityText(humidity) : "--";
        case DeviceType::MotionSensor: {
            const auto motion = boolIn(state, "motion");
            if (!motion) return "--";
            return *motion ? "Motion" : "Clear";
        }
        case DeviceType::ContactSensor: {
            const auto contact = boolIn(state, "contact");
            if (!contact) return "--";
            return *contact ? "Closed" : "Open";
        }
        default:
            return "--";
    }
}

RoomSensors summarizeRoomSensors(const std::vector<DevicePtr>& devices) {
    RoomSensors room;
    for (const auto& device : devices) {
        if (!device) continue;
        const auto type = device->type();
        const auto state = device->getState();

        if (reportsTemperature(type)) {
            room.hasTemperatureSensor = true;
            if (!room.temperatureF) room.temperatureF = temperatureFahrenheit(state);
        }
        if (reportsHumidity(type, state)) {
            room.hasHumiditySensor = true;
            if (!room.humidity) room.humidity = humidityPercent(state);
        }
        if (type == DeviceType::MotionSensor) {
            room.hasMotionSensor = true;
            room.motionDetected = room.motionDetected || boolIn(state, "motion").value_or(false);
        }
    }
    return room;
}

std::string roomClimateText(const RoomSensors& sensors) {
    std::string text;
    if (sensors.hasTemperatureSensor) {
        text = temperatureText(sensors.temperatureF);
    }
    if (sensors.hasHumiditySensor) {
        if (!text.empty()) text += " / ";
        text += humidityText(sensors.humidity);
    }
    return text;
}

} // namespace smarthub
