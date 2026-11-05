/**
 * SmartHub Contact Sensor Implementation
 */

#include <smarthub/devices/types/ContactSensor.hpp>
#include <algorithm>
#include <chrono>

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
    }

    // Initialize state - "open" means no contact (door open, garage open, etc.)
    setStateInternal("open", false);
    setStateInternal("contact", true);  // Alias: true = closed/contact
    setStateInternal("last_change_time", 0);
    if (hasBattery) {
        setStateInternal("battery", 100);
    }

    setAvailability(DeviceAvailability::Online);
}

bool ContactSensor::isOpen() const {
    auto state = getProperty("open");
    if (state.is_boolean()) {
        return state.get<bool>();
    }
    return false;
}

uint64_t ContactSensor::lastChangeTime() const {
    auto state = getProperty("last_change_time");
    if (state.is_number()) {
        return state.get<uint64_t>();
    }
    return 0;
}

int ContactSensor::batteryLevel() const {
    if (!m_hasBattery) {
        return -1;
    }
    auto state = getProperty("battery");
    if (state.is_number()) {
        return state.get<int>();
    }
    return 100;
}

void ContactSensor::setOpen(bool open) {
    setState("open", open);
    setState("contact", !open);  // contact is inverse of open
    updateLastSeen();

    uint64_t now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
    m_lastChangeTime = now;
    setState("last_change_time", now);
}

void ContactSensor::setBatteryLevel(int percent) {
    if (m_hasBattery) {
        percent = std::clamp(percent, 0, 100);
        setState("battery", percent);
    }
}

void ContactSensor::onStateChange(const std::string& property, const nlohmann::json& value) {
    // Handle "alarm" state from IAS Zone - alarm bit 0 typically means open/triggered
    if (property == "alarm" && value.is_boolean()) {
        setOpen(value.get<bool>());
    }
}

} // namespace smarthub
