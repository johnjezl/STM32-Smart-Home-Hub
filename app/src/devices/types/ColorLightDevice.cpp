/**
 * SmartHub Color Light Device Implementation
 */

#include <smarthub/devices/types/ColorLightDevice.hpp>
#include <algorithm>
#include <cmath>

namespace smarthub {

ColorLightDevice::ColorLightDevice(const std::string& id,
                                   const std::string& name,
                                   const std::string& protocol,
                                   const std::string& protocolAddress)
    : DimmerDevice(id, name, protocol, protocolAddress)
{
    // Override type from Dimmer to ColorLight
    m_type = DeviceType::ColorLight;

    addCapability(DeviceCapability::ColorTemperature);
    addCapability(DeviceCapability::ColorRGB);
    addCapability(DeviceCapability::ColorHSV);

    // Default to warm white
    setStateInternal("color_temp", 2700);
    setStateInternal("color_r", 255);
    setStateInternal("color_g", 255);
    setStateInternal("color_b", 255);
    setStateInternal("color_h", 0);
    setStateInternal("color_s", 0);
    setStateInternal("color_v", 100);
}

int ColorLightDevice::colorTemperature() const {
    auto state = getProperty("color_temp");
    if (state.is_number()) {
        return state.get<int>();
    }
    return 2700;
}

void ColorLightDevice::setColorTemperature(int kelvin) {
    kelvin = std::clamp(kelvin, 1000, 10000);
    setState("color_temp", kelvin);
}

RGB ColorLightDevice::colorRGB() const {
    RGB rgb;
    auto r = getProperty("color_r");
    auto g = getProperty("color_g");
    auto b = getProperty("color_b");

    if (r.is_number()) rgb.r = static_cast<uint8_t>(r.get<int>());
    if (g.is_number()) rgb.g = static_cast<uint8_t>(g.get<int>());
    if (b.is_number()) rgb.b = static_cast<uint8_t>(b.get<int>());

    return rgb;
}

void ColorLightDevice::setColorRGB(uint8_t r, uint8_t g, uint8_t b) {
    setState("color_r", static_cast<int>(r));
    setState("color_g", static_cast<int>(g));
    setState("color_b", static_cast<int>(b));

    // Also update HSV
    HSV hsv = rgbToHsv({r, g, b});
    setStateInternal("color_h", static_cast<int>(hsv.h));
    setStateInternal("color_s", static_cast<int>(hsv.s));
    setStateInternal("color_v", static_cast<int>(hsv.v));
}

void ColorLightDevice::setColorRGB(const RGB& rgb) {
    setColorRGB(rgb.r, rgb.g, rgb.b);
}

HSV ColorLightDevice::colorHSV() const {
    HSV hsv;
    auto h = getProperty("color_h");
    auto s = getProperty("color_s");
    auto v = getProperty("color_v");

    if (h.is_number()) hsv.h = static_cast<uint16_t>(h.get<int>());
    if (s.is_number()) hsv.s = static_cast<uint8_t>(s.get<int>());
    if (v.is_number()) hsv.v = static_cast<uint8_t>(v.get<int>());

    return hsv;
}

void ColorLightDevice::setColorHSV(uint16_t h, uint8_t s, uint8_t v) {
    h = h % 360;
    s = std::min(static_cast<uint8_t>(100), s);
    v = std::min(static_cast<uint8_t>(100), v);

    setState("color_h", static_cast<int>(h));
    setState("color_s", static_cast<int>(s));
    setState("color_v", static_cast<int>(v));

    // Also update RGB
    RGB rgb = hsvToRgb({h, s, v});
    setStateInternal("color_r", static_cast<int>(rgb.r));
    setStateInternal("color_g", static_cast<int>(rgb.g));
    setStateInternal("color_b", static_cast<int>(rgb.b));
}

void ColorLightDevice::setColorHSV(const HSV& hsv) {
    setColorHSV(hsv.h, hsv.s, hsv.v);
}

RGB ColorLightDevice::hsvToRgb(const HSV& hsv) {
    RGB rgb;

    if (hsv.s == 0) {
        // Achromatic (gray)
        uint8_t val = static_cast<uint8_t>(hsv.v * 255 / 100);
        rgb.r = rgb.g = rgb.b = val;
        return rgb;
    }

    float h = static_cast<float>(hsv.h) / 60.0f;
    float s = static_cast<float>(hsv.s) / 100.0f;
    float v = static_cast<float>(hsv.v) / 100.0f;

    int i = static_cast<int>(std::floor(h)) % 6;
    float f = h - std::floor(h);
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));

    float r, g, b;
    switch (i) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }

    rgb.r = static_cast<uint8_t>(r * 255.0f);
    rgb.g = static_cast<uint8_t>(g * 255.0f);
    rgb.b = static_cast<uint8_t>(b * 255.0f);

    return rgb;
}

HSV ColorLightDevice::rgbToHsv(const RGB& rgb) {
    HSV hsv;

    float r = static_cast<float>(rgb.r) / 255.0f;
    float g = static_cast<float>(rgb.g) / 255.0f;
    float b = static_cast<float>(rgb.b) / 255.0f;

    float maxVal = std::max({r, g, b});
    float minVal = std::min({r, g, b});
    float delta = maxVal - minVal;

    // Value
    hsv.v = static_cast<uint8_t>(maxVal * 100.0f);

    // Saturation
    if (maxVal > 0.0f) {
        hsv.s = static_cast<uint8_t>((delta / maxVal) * 100.0f);
    } else {
        hsv.s = 0;
    }

    // Hue
    if (delta > 0.0f) {
        float h;
        if (maxVal == r) {
            h = 60.0f * std::fmod((g - b) / delta, 6.0f);
        } else if (maxVal == g) {
            h = 60.0f * (((b - r) / delta) + 2.0f);
        } else {
            h = 60.0f * (((r - g) / delta) + 4.0f);
        }
        if (h < 0) h += 360.0f;
        hsv.h = static_cast<uint16_t>(h);
    } else {
        hsv.h = 0;
    }

    return hsv;
}

void ColorLightDevice::onStateChange(const std::string& property, const nlohmann::json& value) {
    // Call parent implementation
    DimmerDevice::onStateChange(property, value);
}

} // namespace smarthub
