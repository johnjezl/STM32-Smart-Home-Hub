/**
 * SmartHub Color Light Device
 *
 * Color-capable light with brightness, color temperature, and RGB control.
 */
#pragma once

#include <smarthub/devices/types/DimmerDevice.hpp>

namespace smarthub {

/**
 * RGB color structure
 */
struct RGB {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    bool operator==(const RGB& other) const {
        return r == other.r && g == other.g && b == other.b;
    }
};

/**
 * HSV color structure
 */
struct HSV {
    uint16_t h = 0;  // Hue: 0-360
    uint8_t s = 0;   // Saturation: 0-100
    uint8_t v = 0;   // Value: 0-100

    bool operator==(const HSV& other) const {
        return h == other.h && s == other.s && v == other.v;
    }
};

/**
 * Color light device - supports on/off, brightness, color temperature, and RGB
 */
class ColorLightDevice : public DimmerDevice {
public:
    /**
     * Constructor
     * @param id Unique device identifier
     * @param name Human-readable name
     * @param protocol Protocol name
     * @param protocolAddress Protocol-specific address
     */
    ColorLightDevice(const std::string& id,
                     const std::string& name,
                     const std::string& protocol = "local",
                     const std::string& protocolAddress = "");

    /**
     * Get color temperature in Kelvin (typically 2700-6500)
     */
    int colorTemperature() const;

    /**
     * Set color temperature
     * @param kelvin Temperature in Kelvin
     */
    void setColorTemperature(int kelvin);

    /**
     * Get RGB color values
     */
    RGB colorRGB() const;

    /**
     * Set RGB color
     */
    void setColorRGB(uint8_t r, uint8_t g, uint8_t b);
    void setColorRGB(const RGB& rgb);

    /**
     * Get HSV color values
     */
    HSV colorHSV() const;

    /**
     * Set HSV color
     */
    void setColorHSV(uint16_t h, uint8_t s, uint8_t v);
    void setColorHSV(const HSV& hsv);

    /**
     * Color conversion utilities
     */
    static RGB hsvToRgb(const HSV& hsv);
    static HSV rgbToHsv(const RGB& rgb);

protected:
    void onStateChange(const std::string& property, const nlohmann::json& value) override;
};

} // namespace smarthub
