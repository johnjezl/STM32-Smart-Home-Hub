/**
 * SmartHub Motion Sensor
 *
 * Motion/occupancy detection sensor device.
 */
#pragma once

#include <smarthub/devices/Device.hpp>

namespace smarthub {

/**
 * Motion sensor device
 */
class MotionSensor : public Device {
public:
    /**
     * Constructor
     * @param id Unique device identifier
     * @param name Human-readable name
     * @param protocol Protocol name
     * @param protocolAddress Protocol-specific address
     * @param hasIlluminance Whether this sensor also reports illuminance
     * @param hasBattery Whether this sensor has battery level reporting
     */
    MotionSensor(const std::string& id,
                 const std::string& name,
                 const std::string& protocol = "local",
                 const std::string& protocolAddress = "",
                 bool hasIlluminance = false,
                 bool hasBattery = false);

    /**
     * Check if motion is currently detected
     */
    bool motionDetected() const;

    /**
     * Get timestamp of last motion detection (Unix epoch seconds)
     */
    uint64_t lastMotionTime() const;

    /**
     * Get illuminance level in lux
     * Returns -1 if illuminance is not supported
     */
    int illuminance() const;

    /**
     * Get battery level percentage (0-100)
     * Returns -1 if battery level is not supported
     */
    int batteryLevel() const;

    /**
     * Update motion state (typically called by protocol handler)
     */
    void setMotionDetected(bool detected);

    /**
     * Update illuminance reading
     */
    void setIlluminance(int lux);

    /**
     * Update battery level
     */
    void setBatteryLevel(int percent);

protected:
    void onStateChange(const std::string& property, const nlohmann::json& value) override;

private:
    bool m_hasIlluminance;
    bool m_hasBattery;
    uint64_t m_lastMotionTime = 0;
};

} // namespace smarthub
