/**
 * SmartHub Motion Sensor Implementation
 */

#include <smarthub/devices/types/MotionSensor.hpp>
#include <algorithm>
#include <chrono>

namespace smarthub {

MotionSensor::MotionSensor(const std::string& id,
                           const std::string& name,
                           const std::string& protocol,
                           const std::string& protocolAddress,
                           bool hasIlluminance,
                           bool hasBattery)
    : Device(id, name, DeviceType::MotionSensor, protocol, protocolAddress)
    , m_hasIlluminance(hasIlluminance)
    , m_hasBattery(hasBattery)
{
    addCapability(DeviceCapability::Motion);
    addCapability(DeviceCapability::Occupancy);

    if (hasIlluminance) {
        addCapability(DeviceCapability::Illuminance);
        m_type = DeviceType::MultiSensor;
    }

    if (hasBattery) {
        addCapability(DeviceCapability::Battery);
    }

    // Initialize state
    setStateInternal("motion", false);
    setStateInternal("last_motion_time", 0);
    if (hasIlluminance) {
        setStateInternal("illuminance", 0);
    }
    if (hasBattery) {
        setStateInternal("battery", 100);
    }

    setAvailability(DeviceAvailability::Online);
}

bool MotionSensor::motionDetected() const {
    auto state = getProperty("motion");
    if (state.is_boolean()) {
        return state.get<bool>();
    }
    return false;
}

uint64_t MotionSensor::lastMotionTime() const {
    auto state = getProperty("last_motion_time");
    if (state.is_number()) {
        return state.get<uint64_t>();
    }
    return 0;
}

int MotionSensor::illuminance() const {
    if (!m_hasIlluminance) {
        return -1;
    }
    auto state = getProperty("illuminance");
    if (state.is_number()) {
        return state.get<int>();
    }
    return 0;
}

int MotionSensor::batteryLevel() const {
    if (!m_hasBattery) {
        return -1;
    }
    auto state = getProperty("battery");
    if (state.is_number()) {
        return state.get<int>();
    }
    return 100;
}

void MotionSensor::setMotionDetected(bool detected) {
    setState("motion", detected);
    updateLastSeen();

    if (detected) {
        uint64_t now = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
        m_lastMotionTime = now;
        setState("last_motion_time", now);
    }
}

void MotionSensor::setIlluminance(int lux) {
    if (m_hasIlluminance) {
        lux = std::max(0, lux);
        setState("illuminance", lux);
        updateLastSeen();
    }
}

void MotionSensor::setBatteryLevel(int percent) {
    if (m_hasBattery) {
        percent = std::clamp(percent, 0, 100);
        setState("battery", percent);
    }
}

void MotionSensor::onStateChange(const std::string& property, const nlohmann::json& value) {
    (void)property;
    (void)value;
}

} // namespace smarthub
