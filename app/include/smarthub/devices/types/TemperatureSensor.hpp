/**
 * SmartHub Temperature Sensor
 *
 * Temperature and optionally humidity sensor device.
 */
#pragma once

#include <smarthub/devices/Device.hpp>

namespace smarthub {

/**
 * Temperature sensor device
 */
class TemperatureSensor : public Device {
public:
    /**
     * Constructor
     * @param id Unique device identifier
     * @param name Human-readable name
     * @param protocol Protocol name
     * @param protocolAddress Protocol-specific address
     * @param hasHumidity Whether this sensor also reports humidity
     * @param hasBattery Whether this sensor has battery level reporting
     */
    TemperatureSensor(const std::string& id,
                      const std::string& name,
                      const std::string& protocol = "local",
                      const std::string& protocolAddress = "",
                      bool hasHumidity = false,
                      bool hasBattery = false);

    /**
     * Get current temperature in Celsius
     */
    double temperature() const;

    /**
     * Get current humidity percentage (0-100)
     * Returns -1 if humidity is not supported
     */
    double humidity() const;

    /**
     * Get battery level percentage (0-100)
     * Returns -1 if battery level is not supported
     */
    int batteryLevel() const;

    /**
     * Update temperature reading (typically called by protocol handler)
     */
    void setTemperature(double celsius);

    /**
     * Update humidity reading
     */
    void setHumidity(double percent);

    /**
     * Update battery level
     */
    void setBatteryLevel(int percent);

protected:
    void onStateChange(const std::string& property, const nlohmann::json& value) override;

private:
    bool m_hasHumidity;
    bool m_hasBattery;
};

} // namespace smarthub
