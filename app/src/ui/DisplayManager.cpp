/**
 * Display Manager Implementation
 */

#include "smarthub/ui/DisplayManager.hpp"
#include "smarthub/core/Logger.hpp"

#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace smarthub {
namespace ui {

DisplayManager::DisplayManager() = default;

DisplayManager::~DisplayManager() {
    shutdown();
}

bool DisplayManager::initialize(const std::string& backlightPath) {
    if (m_initialized) return true;

    m_backlightPath = backlightPath;
    m_brightnessFile = backlightPath + "/brightness";
    m_maxBrightnessFile = backlightPath + "/max_brightness";

    // Check if backlight sysfs exists
    if (!fs::exists(m_backlightPath)) {
        LOG_WARN("DisplayManager: Backlight path not found: %s", backlightPath.c_str());
        // Still initialize, but brightness control won't work
        m_initialized = true;
        return true;
    }

    // Read max brightness
    m_maxBrightness = readMaxBrightness();
    if (m_maxBrightness <= 0) {
        m_maxBrightness = 255;  // Default
    }

    LOG_INFO("DisplayManager: Initialized, max brightness: %d", m_maxBrightness);

    // Read current brightness
    int current = readBrightness();
    if (current >= 0) {
        m_brightness = (current * 100) / m_maxBrightness;
    }

    m_initialized = true;
    return true;
}

void DisplayManager::shutdown() {
    if (!m_initialized) return;

    // Restore full brightness on shutdown
    applyBrightness(100);

    m_initialized = false;
}

void DisplayManager::update(uint32_t deltaMs) {
    if (!m_initialized || m_timeoutSeconds <= 0) return;

    m_idleMs += deltaMs;

    uint32_t timeoutMs = static_cast<uint32_t>(m_timeoutSeconds) * 1000;
    uint32_t dimMs = timeoutMs - DIM_DELAY_MS;

    // Check for dimming
    if (!m_isDimmed && !m_isOff && m_idleMs >= dimMs) {
        m_isDimmed = true;
        applyBrightness(m_dimLevel);
        LOG_DEBUG("DisplayManager: Screen dimmed");

        if (m_timeoutCallback) {
            m_timeoutCallback(true);
        }
    }

    // Check for screen off
    if (!m_isOff && m_idleMs >= timeoutMs) {
        m_isOff = true;
        applyBrightness(0);
        LOG_DEBUG("DisplayManager: Screen off");
    }
}

void DisplayManager::setBrightness(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    m_brightness = percent;

    // Only apply if not dimmed/off
    if (!m_isDimmed && !m_isOff) {
        applyBrightness(percent);
    }
}

void DisplayManager::setTimeoutSeconds(int seconds) {
    if (seconds < 0) seconds = 0;
    m_timeoutSeconds = seconds;
}

void DisplayManager::setDimLevel(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    m_dimLevel = percent;
}

void DisplayManager::wake() {
    m_idleMs = 0;

    if (m_isDimmed || m_isOff) {
        m_isDimmed = false;
        m_isOff = false;
        applyBrightness(m_brightness);
        LOG_DEBUG("DisplayManager: Wake - brightness restored to %d%%", m_brightness);

        if (m_timeoutCallback) {
            m_timeoutCallback(false);
        }
    }
}

void DisplayManager::setTimeoutCallback(TimeoutCallback callback) {
    m_timeoutCallback = callback;
}

void DisplayManager::setScreenOn(bool on) {
    if (on) {
        wake();
    } else {
        m_isOff = true;
        m_isDimmed = false;
        applyBrightness(0);
    }
}

void DisplayManager::applyBrightness(int percent) {
    if (!m_initialized) return;
    if (!fs::exists(m_brightnessFile)) return;

    int value = (percent * m_maxBrightness) / 100;

    try {
        std::ofstream file(m_brightnessFile);
        if (file.is_open()) {
            file << value;
            file.close();
        } else {
            LOG_WARN("DisplayManager: Failed to open brightness file");
        }
    } catch (const std::exception& e) {
        LOG_WARN("DisplayManager: Failed to set brightness: %s", e.what());
    }
}

int DisplayManager::readBrightness() {
    if (!fs::exists(m_brightnessFile)) return -1;

    try {
        std::ifstream file(m_brightnessFile);
        if (file.is_open()) {
            int value;
            file >> value;
            return value;
        }
    } catch (const std::exception& e) {
        LOG_WARN("DisplayManager: Failed to read brightness: %s", e.what());
    }
    return -1;
}

int DisplayManager::readMaxBrightness() {
    if (!fs::exists(m_maxBrightnessFile)) return -1;

    try {
        std::ifstream file(m_maxBrightnessFile);
        if (file.is_open()) {
            int value;
            file >> value;
            return value;
        }
    } catch (const std::exception& e) {
        LOG_WARN("DisplayManager: Failed to read max brightness: %s", e.what());
    }
    return -1;
}

} // namespace ui
} // namespace smarthub
