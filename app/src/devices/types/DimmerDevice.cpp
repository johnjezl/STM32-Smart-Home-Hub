/**
 * SmartHub Dimmer Device Implementation
 */

#include <smarthub/devices/types/DimmerDevice.hpp>
#include <algorithm>

namespace smarthub {

DimmerDevice::DimmerDevice(const std::string& id,
                           const std::string& name,
                           const std::string& protocol,
                           const std::string& protocolAddress)
    : Device(id, name, DeviceType::Dimmer, protocol, protocolAddress)
{
    addCapability(DeviceCapability::OnOff);
    addCapability(DeviceCapability::Brightness);
    setStateInternal("on", false);
    setStateInternal("brightness", 0);
    setAvailability(DeviceAvailability::Online);
}

bool DimmerDevice::isOn() const {
    auto state = getProperty("on");
    if (state.is_boolean()) {
        return state.get<bool>();
    }
    return false;
}

int DimmerDevice::brightness() const {
    auto state = getProperty("brightness");
    if (state.is_number()) {
        return state.get<int>();
    }
    return 0;
}

void DimmerDevice::turnOn() {
    int level = m_lastBrightness > 0 ? m_lastBrightness : 100;
    setBrightness(level);
}

void DimmerDevice::turnOff() {
    setState("on", false);
    setState("brightness", 0);
}

void DimmerDevice::setBrightness(int level) {
    level = std::clamp(level, 0, 100);

    if (level > 0) {
        m_lastBrightness = level;
        setState("on", true);
        setState("brightness", level);
    } else {
        turnOff();
    }
}

void DimmerDevice::onStateChange(const std::string& property, const nlohmann::json& value) {
    // Track brightness for restore on turnOn()
    if (property == "brightness" && value.is_number()) {
        int level = value.get<int>();
        if (level > 0) {
            m_lastBrightness = level;
        }
    }
}

} // namespace smarthub
