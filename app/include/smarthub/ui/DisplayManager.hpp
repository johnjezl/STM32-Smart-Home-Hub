/**
 * Display Manager
 *
 * Manages display settings: brightness, screen timeout, wake on touch.
 * Uses sysfs for backlight control on STM32MP1.
 */
#pragma once

#include <string>
#include <functional>
#include <cstdint>

namespace smarthub {
namespace ui {

/**
 * Display Manager
 *
 * Handles:
 * - Brightness control (0-100%)
 * - Screen timeout with dimming
 * - Wake on touch
 */
class DisplayManager {
public:
    using TimeoutCallback = std::function<void(bool dimmed)>;

    DisplayManager();
    ~DisplayManager();

    /**
     * Initialize display manager
     * @param backlightPath Path to backlight sysfs directory
     * @return true if successful
     */
    bool initialize(const std::string& backlightPath = "/sys/class/backlight/backlight");

    /**
     * Shutdown and restore defaults
     */
    void shutdown();

    /**
     * Update - call periodically to handle timeout
     * @param deltaMs Milliseconds since last update
     */
    void update(uint32_t deltaMs);

    // Brightness control (0-100)
    void setBrightness(int percent);
    int brightness() const { return m_brightness; }
    int maxBrightness() const { return m_maxBrightness; }

    // Screen timeout (seconds, 0 = disabled)
    void setTimeoutSeconds(int seconds);
    int timeoutSeconds() const { return m_timeoutSeconds; }

    // Dim level when timing out (0-100, before full off)
    void setDimLevel(int percent);
    int dimLevel() const { return m_dimLevel; }

    // Check if screen is currently dimmed or off
    bool isDimmed() const { return m_isDimmed; }
    bool isOff() const { return m_isOff; }

    /**
     * Wake the display (reset timeout, restore brightness)
     */
    void wake();

    /**
     * Register for timeout state changes
     */
    void setTimeoutCallback(TimeoutCallback callback);

    /**
     * Force screen on/off
     */
    void setScreenOn(bool on);

    // Default values
    static constexpr int DEFAULT_BRIGHTNESS = 80;
    static constexpr int DEFAULT_TIMEOUT = 60;     // 1 minute
    static constexpr int DEFAULT_DIM_LEVEL = 20;
    static constexpr int DIM_DELAY_MS = 5000;      // Dim 5s before off

private:
    void applyBrightness(int percent);
    int readBrightness();
    int readMaxBrightness();

    std::string m_backlightPath;
    std::string m_brightnessFile;
    std::string m_maxBrightnessFile;

    int m_brightness = DEFAULT_BRIGHTNESS;
    int m_maxBrightness = 255;
    int m_timeoutSeconds = DEFAULT_TIMEOUT;
    int m_dimLevel = DEFAULT_DIM_LEVEL;

    uint32_t m_idleMs = 0;
    bool m_isDimmed = false;
    bool m_isOff = false;
    bool m_initialized = false;

    TimeoutCallback m_timeoutCallback;
};

} // namespace ui
} // namespace smarthub
