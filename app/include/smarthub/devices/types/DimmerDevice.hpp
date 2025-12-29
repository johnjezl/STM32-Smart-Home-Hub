/**
 * SmartHub Dimmer Device
 *
 * Dimmable light with brightness control.
 */
#pragma once

#include <smarthub/devices/Device.hpp>

namespace smarthub {

/**
 * Dimmer device - supports on/off and brightness control
 */
class DimmerDevice : public Device {
public:
    /**
     * Constructor
     * @param id Unique device identifier
     * @param name Human-readable name
     * @param protocol Protocol name
     * @param protocolAddress Protocol-specific address
     */
    DimmerDevice(const std::string& id,
                 const std::string& name,
                 const std::string& protocol = "local",
                 const std::string& protocolAddress = "");

    /**
     * Check if light is on
     */
    bool isOn() const;

    /**
     * Get current brightness level (0-100)
     */
    int brightness() const;

    /**
     * Turn the light on (restores last brightness or 100%)
     */
    void turnOn();

    /**
     * Turn the light off
     */
    void turnOff();

    /**
     * Set brightness level (0-100)
     * Setting to 0 turns off, >0 turns on
     */
    void setBrightness(int level);

protected:
    void onStateChange(const std::string& property, const nlohmann::json& value) override;

private:
    int m_lastBrightness = 100;  // Remember last brightness for turnOn()
};

} // namespace smarthub
