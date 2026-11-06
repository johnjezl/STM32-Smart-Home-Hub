/**
 * SmartHub Contact Sensor Implementation
 */

#include <smarthub/devices/types/ContactSensor.hpp>
#include <algorithm>

namespace smarthub {

ContactSensor::ContactSensor(const std::string& id,
                             const std::string& name,
                             const std::string& protocol,
                             const std::string& protocolAddress,
                             bool hasBattery)
    : Device(id, name, DeviceType::ContactSensor, protocol, protocolAddress)
    , m_hasBattery(hasBattery)
{
    addCapability(DeviceCapability::Contact);
    if (hasBattery) {
        addCapability(DeviceCapability::Battery);
        setStateInternal("battery", 100);
    }

    ContactSensor::setConfig(nlohmann::json::object());
    setAvailability(DeviceAvailability::Online);
}

bool ContactSensor::isOpen() const {
    const auto contact = getProperty("contact");
    return contact.is_boolean() && !contact.get<bool>();
}

void ContactSensor::setOpen(bool open) {
    setState("contact", !open);
    updateLastSeen();
}

int ContactSensor::batteryLevel() const {
    if (!m_hasBattery) {
        return -1;
    }
    const auto state = getProperty("battery");
    return state.is_number() ? state.get<int>() : 100;
}

void ContactSensor::setBatteryLevel(int percent) {
    if (m_hasBattery) {
        setState("battery", std::clamp(percent, 0, 100));
    }
}

void ContactSensor::onStateChange(const std::string& property, const nlohmann::json& value) {
    (void)property;
    (void)value;
}

void ContactSensor::setConfig(const nlohmann::json& config) {
    nlohmann::json merged = config.is_object() ? config : nlohmann::json::object();
    merged["has_battery"] = m_hasBattery;
    Device::setConfig(merged);
}

} // namespace smarthub
