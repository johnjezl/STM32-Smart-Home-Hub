/**
 * SmartHub Contact Sensor
 *
 * Door/window/tilt contact sensor device.
 */
#pragma once

#include <smarthub/devices/Device.hpp>

namespace smarthub {

/**
 * Contact sensor device (door/window/tilt sensors)
 */
class ContactSensor : public Device {
public:
    /**
     * Constructor
     * @param id Unique device identifier
     * @param name Human-readable name
     * @param protocol Protocol name
     * @param protocolAddress Protocol-specific address
     * @param hasBattery Whether this sensor has battery level reporting
     */
    ContactSensor(const std::string& id,
                  const std::string& name,
                  const std::string& protocol = "local",
                  const std::string& protocolAddress = "",
                  bool hasBattery = false);

    /**
     * Check if contact is open (true = open/no contact, false = closed/contact)
     */
    bool isOpen() const;

    /**
     * Get timestamp of last state change (Unix epoch seconds)
     */
    uint64_t lastChangeTime() const;

    /**
     * Get battery level percentage (0-100)
     * Returns -1 if battery level is not supported
     */
    int batteryLevel() const;

    /**
     * Update contact state (typically called by protocol handler)
     */
    void setOpen(bool open);

    /**
     * Update battery level
     */
    void setBatteryLevel(int percent);

protected:
    void onStateChange(const std::string& property, const nlohmann::json& value) override;

private:
    bool m_hasBattery;
    uint64_t m_lastChangeTime = 0;
};

} // namespace smarthub
