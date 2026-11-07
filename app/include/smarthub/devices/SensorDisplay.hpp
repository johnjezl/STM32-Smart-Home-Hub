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
    bool motionReported = false;
    bool motionDetected = false;
    bool hasAlarmSensor = false;
    bool alarmReported = false;
    std::vector<std::string> activeAlarms;  // "Leak", "Smoke" and so on

    bool any() const { return hasTemperatureSensor || hasHumiditySensor || hasMotionSensor || hasAlarmSensor; }
};

/** Temperature reading in Fahrenheit; sensors report Celsius. Empty until the first report. */
std::optional<double> temperatureFahrenheit(const nlohmann::json& state);

/** Humidity percentage. Empty until the first report. */
std::optional<double> humidityPercent(const nlohmann::json& state);

/** "72°F" or "--°F" */
std::string temperatureText(const std::optional<double>& fahrenheit, int decimals = 0);

/** "45%" or "--%" */
std::string humidityText(const std::optional<double>& percent);

/** "Motion" or "Clear", or "--" before any motion sensor in the room has reported */
std::string roomMotionText(const RoomSensors& sensors);

/** "Clear" or what's going off, such as "Leak, Smoke". "--" before any of the room's alarm sensors has reported. */
std::string roomAlarmText(const RoomSensors& sensors);

/** Whether this is a smoke, water, carbon monoxide or other sensor whose job is to raise an alarm */
bool isAlarmSensor(DeviceType type);

/** What's going off, such as "Leak", or empty if this sensor's alarm isn't active */
std::string activeAlarmText(DeviceType type, const nlohmann::json& state);

/** Every active alarm on these devices, such as "Leak: Basement, Smoke: Kitchen". Empty if there are none. */
std::string activeAlarmsSummary(const std::vector<DevicePtr>& devices);

/**
 * One device's current value, such as "72.5°F / 45%", "Motion" or "Closed", followed by
 * whatever else it measures, such as "Motion / 72.5°F". "--" with no reading.
 */
std::string sensorValueText(DeviceType type, const nlohmann::json& state);

/** The property a device's history charts. Empty if it has none, like a motion sensor. */
std::string historyProperty(DeviceType type);

RoomSensors summarizeRoomSensors(const std::vector<DevicePtr>& devices);

/** A room's climate for a card, such as "72°F / 45%". Empty if the room has no such sensors. */
std::string roomClimateText(const RoomSensors& sensors);

} // namespace smarthub
