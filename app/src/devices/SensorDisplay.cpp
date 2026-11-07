/**
 * Turns sensor state into what the screens show.
 */

#include <smarthub/devices/SensorDisplay.hpp>

#include <algorithm>
#include <cmath>
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

// printf rounds ties to even, which would show 72°F here and 73°F on the web for 72.5°F
double roundHalfUp(double value, int decimals) {
    const double scale = std::pow(10.0, decimals);
    const double rounded = std::round(value * scale) / scale;
    return rounded == 0.0 ? 0.0 : rounded;  // No "-0°F"
}

// Sensors for something else, which may also measure the room
bool mayAlsoMeasure(DeviceType type) {
    return type == DeviceType::MotionSensor || type == DeviceType::ContactSensor || isAlarmSensor(type);
}

bool reportsTemperature(DeviceType type, const nlohmann::json& state) {
    return type == DeviceType::TemperatureSensor || type == DeviceType::MultiSensor ||
           (mayAlsoMeasure(type) && temperatureFahrenheit(state).has_value());
}

// A plain temperature sensor can still report humidity, for instance over Zigbee
bool reportsHumidity(DeviceType type, const nlohmann::json& state) {
    return type == DeviceType::HumiditySensor || type == DeviceType::MultiSensor ||
           ((type == DeviceType::TemperatureSensor || mayAlsoMeasure(type)) && humidityPercent(state).has_value());
}

const char* alarmWord(DeviceType type) {
    switch (type) {
        case DeviceType::SmokeSensor: return "Smoke";
        case DeviceType::WaterSensor: return "Leak";
        case DeviceType::CoSensor:    return "CO";
        default:                      return "Alarm";
    }
}

void addReading(std::string& text, const std::string& reading) {
    text += text.empty() ? reading : " / " + reading;
}

std::string withLight(std::string text, const nlohmann::json& state) {
    if (const auto lux = numberIn(state, "illuminance")) {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%.0f lux", *lux);
        addReading(text, buf);
    }
    return text.empty() ? "--" : text;
}

// What it's for, with any other readings after it
std::string withOtherReadings(std::string text, const nlohmann::json& state) {
    if (const auto temperature = temperatureFahrenheit(state)) addReading(text, temperatureText(temperature, 1));
    if (const auto humidity = humidityPercent(state)) addReading(text, humidityText(humidity));
    return withLight(std::move(text), state);
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
    std::snprintf(buf, sizeof(buf), "%.*f°F", decimals, roundHalfUp(*fahrenheit, decimals));
    return buf;
}

std::string humidityText(const std::optional<double>& percent) {
    if (!percent) return "--%";
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.0f%%", roundHalfUp(*percent, 0));
    return buf;
}

std::string sensorValueText(DeviceType type, const nlohmann::json& state) {
    const auto temperature = temperatureFahrenheit(state);
    const auto humidity = humidityPercent(state);

    switch (type) {
        case DeviceType::TemperatureSensor:
        case DeviceType::MultiSensor:
            if (!temperature && !humidity) return withLight("", state);
            if (!humidity) return withLight(temperatureText(temperature, 1), state);
            return withLight(temperatureText(temperature, 1) + " / " + humidityText(humidity), state);
        case DeviceType::HumiditySensor:
            return withLight(humidity ? humidityText(humidity) : "", state);
        case DeviceType::MotionSensor: {
            const auto motion = boolIn(state, "motion");
            return withOtherReadings(!motion ? "" : (*motion ? "Motion" : "Clear"), state);
        }
        case DeviceType::ContactSensor: {
            const auto contact = boolIn(state, "contact");
            return withOtherReadings(!contact ? "" : (*contact ? "Closed" : "Open"), state);
        }
        case DeviceType::SmokeSensor:
        case DeviceType::WaterSensor:
        case DeviceType::CoSensor:
        case DeviceType::AlarmSensor: {
            const auto alarm = boolIn(state, "alarm");
            return withOtherReadings(!alarm ? "" : (*alarm ? alarmWord(type) : "Clear"), state);
        }
        default:
            return "--";
    }
}

bool isAlarmSensor(DeviceType type) {
    return type == DeviceType::SmokeSensor || type == DeviceType::WaterSensor ||
           type == DeviceType::CoSensor || type == DeviceType::AlarmSensor;
}

std::string activeAlarmText(DeviceType type, const nlohmann::json& state) {
    if (!isAlarmSensor(type) || !boolIn(state, "alarm").value_or(false)) return "";
    return alarmWord(type);
}

std::string activeAlarmsSummary(const std::vector<DevicePtr>& devices) {
    std::string summary;
    for (const auto& device : devices) {
        if (!device) continue;
        const std::string alarm = activeAlarmText(device->type(), device->getState());
        if (alarm.empty()) continue;
        if (!summary.empty()) summary += ", ";
        summary += alarm + ": " + device->name();
    }
    return summary;
}

std::string historyProperty(DeviceType type) {
    if (type == DeviceType::TemperatureSensor || type == DeviceType::MultiSensor) return "temperature";
    if (type == DeviceType::HumiditySensor) return "humidity";
    return "";
}

RoomSensors summarizeRoomSensors(const std::vector<DevicePtr>& devices) {
    RoomSensors room;
    for (const auto& device : devices) {
        if (!device) continue;
        const auto type = device->type();
        const auto state = device->getState();

        if (reportsTemperature(type, state)) {
            room.hasTemperatureSensor = true;
            if (!room.temperatureF) room.temperatureF = temperatureFahrenheit(state);
        }
        if (reportsHumidity(type, state)) {
            room.hasHumiditySensor = true;
            if (!room.humidity) room.humidity = humidityPercent(state);
        }
        if (type == DeviceType::MotionSensor) {
            room.hasMotionSensor = true;
            if (const auto motion = boolIn(state, "motion")) {
                room.motionReported = true;
                room.motionDetected = room.motionDetected || *motion;
            }
        }
        if (isAlarmSensor(type)) {
            room.hasAlarmSensor = true;
            room.alarmReported = room.alarmReported || boolIn(state, "alarm").has_value();
            const std::string alarm = activeAlarmText(type, state);
            if (!alarm.empty() && std::find(room.activeAlarms.begin(), room.activeAlarms.end(), alarm) == room.activeAlarms.end()) {
                room.activeAlarms.push_back(alarm);
            }
        }
    }
    return room;
}

std::string roomMotionText(const RoomSensors& sensors) {
    if (!sensors.motionReported) return "--";
    return sensors.motionDetected ? "Motion" : "Clear";
}

std::string roomAlarmText(const RoomSensors& sensors) {
    if (!sensors.activeAlarms.empty()) {
        std::string text;
        for (const auto& alarm : sensors.activeAlarms) {
            if (!text.empty()) text += ", ";
            text += alarm;
        }
        return text;
    }
    return sensors.alarmReported ? "Clear" : "--";
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
