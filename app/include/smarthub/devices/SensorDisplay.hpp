#pragma once

#include <smarthub/devices/IDevice.hpp>

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace smarthub {

/**
 * What the sensors in one room report.
 *
 * Having a sensor and having a reading are separate: a sensor that hasn't
 * reported yet shows as "--", while a room with no sensor shows nothing.
 */
struct RoomSensors {
    bool hasTemperatureSensor = false;
    std::optional<double> temperatureF;
    bool hasHumiditySensor = false;
    std::optional<double> humidity;
    bool hasMotionSensor = false;
    bool motionDetected = false;

    bool any() const { return hasTemperatureSensor || hasHumiditySensor || hasMotionSensor; }
};

/** Temperature reading in Fahrenheit; sensors report Celsius. Empty until the first report. */
std::optional<double> temperatureFahrenheit(const nlohmann::json& state);

/** Humidity percentage. Empty until the first report. */
std::optional<double> humidityPercent(const nlohmann::json& state);

/** "72°F" or "--°F" */
std::string temperatureText(const std::optional<double>& fahrenheit, int decimals = 0);

/** "45%" or "--%" */
std::string humidityText(const std::optional<double>& percent);

/** One device's current value, such as "72.5°F / 45%", "Motion" or "Closed". "--" with no reading. */
std::string sensorValueText(DeviceType type, const nlohmann::json& state);

RoomSensors summarizeRoomSensors(const std::vector<DevicePtr>& devices);

/** A room's climate for a card, such as "72°F / 45%". Empty if the room has no such sensors. */
std::string roomClimateText(const RoomSensors& sensors);

} // namespace smarthub
