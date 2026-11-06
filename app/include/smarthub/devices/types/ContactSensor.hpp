#pragma once

#include <smarthub/devices/Device.hpp>

namespace smarthub {

/**
 * Door or window sensor.
 *
 * State "contact" is true when closed. It stays unset until the first report.
 */
class ContactSensor : public Device {
public:
    ContactSensor(const std::string& id,
                  const std::string& name,
                  const std::string& protocol = "local",
                  const std::string& protocolAddress = "",
                  bool hasBattery = false);

    // Keeps has_battery, which decides what this is when it's loaded
    void setConfig(const nlohmann::json& config) override;

    /** Whether it's open. False before the first report. */
    bool isOpen() const;

    /** Update the state (typically called by protocol handler) */
    void setOpen(bool open);

    /** Battery percentage (0-100), or -1 if it doesn't report one */
    int batteryLevel() const;
    void setBatteryLevel(int percent);

protected:
    void onStateChange(const std::string& property, const nlohmann::json& value) override;

private:
    bool m_hasBattery;
};

} // namespace smarthub
