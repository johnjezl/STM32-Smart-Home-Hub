/**
 * SmartHub Switch Device Implementation
 */

#include <smarthub/devices/types/SwitchDevice.hpp>

namespace smarthub {

SwitchDevice::SwitchDevice(const std::string& id,
                           const std::string& name,
                           const std::string& protocol,
                           const std::string& protocolAddress)
    : Device(id, name, DeviceType::Switch, protocol, protocolAddress)
{
    addCapability(DeviceCapability::OnOff);
    setStateInternal("on", false);
    setAvailability(DeviceAvailability::Online);
}

bool SwitchDevice::isOn() const {
    auto state = getProperty("on");
    if (state.is_boolean()) {
        return state.get<bool>();
    }
    return false;
}

void SwitchDevice::turnOn() {
    setState("on", true);
}

void SwitchDevice::turnOff() {
    setState("on", false);
}

void SwitchDevice::toggle() {
    setState("on", !isOn());
}

void SwitchDevice::onStateChange(const std::string& property, const nlohmann::json& value) {
    // Can be extended for logging or additional logic
    (void)property;
    (void)value;
}

} // namespace smarthub
