/**
 * SmartHub Switch Device
 *
 * Simple on/off switch device.
 */
#pragma once

#include <smarthub/devices/Device.hpp>

namespace smarthub {

/**
 * Switch device - supports on/off control
 */
class SwitchDevice : public Device {
public:
    /**
     * Constructor
     * @param id Unique device identifier
     * @param name Human-readable name
     * @param protocol Protocol name
     * @param protocolAddress Protocol-specific address
     */
    SwitchDevice(const std::string& id,
                 const std::string& name,
                 const std::string& protocol = "local",
                 const std::string& protocolAddress = "");

    /**
     * Check if switch is on
     */
    bool isOn() const;

    /**
     * Turn the switch on
     */
    void turnOn();

    /**
     * Turn the switch off
     */
    void turnOff();

    /**
     * Toggle the switch state
     */
    void toggle();

protected:
    void onStateChange(const std::string& property, const nlohmann::json& value) override;
};

} // namespace smarthub
