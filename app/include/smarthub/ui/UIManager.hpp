/**
 * SmartHub UI Manager
 *
 * LVGL-based touchscreen UI for the STM32MP157F-DK2.
 * Uses Linux DRM for display and evdev for touch input.
 */
#pragma once

#include <string>
#include <atomic>
#include <memory>
#include <cstdint>

namespace smarthub {

class EventBus;
class DeviceManager;

/**
 * UI Manager for LVGL-based touchscreen interface
 */
class UIManager {
public:
    UIManager(EventBus& eventBus, DeviceManager& deviceManager);
    ~UIManager();

    // Non-copyable
    UIManager(const UIManager&) = delete;
    UIManager& operator=(const UIManager&) = delete;

    /**
     * Initialize LVGL and display/input drivers
     * @param drmDevice DRM device (default: /dev/dri/card0)
     * @param touchDevice Touch input device (default: /dev/input/event0)
     * @return true on success
     */
    bool initialize(const std::string& drmDevice = "/dev/dri/card0",
                    const std::string& touchDevice = "/dev/input/event1");

    /**
     * Update the UI (called from main loop)
     * Handles LVGL tick and task processing
     */
    void update();

    /**
     * Shutdown the UI
     */
    void shutdown();

    /**
     * Check if UI is running
     */
    bool isRunning() const { return m_running; }

    /**
     * Get display width
     */
    int getWidth() const { return m_width; }

    /**
     * Get display height
     */
    int getHeight() const { return m_height; }

private:
    void createMainScreen();

    EventBus& m_eventBus;
    DeviceManager& m_deviceManager;

    std::string m_fbDevice;
    std::string m_touchDevice;
    int m_width = 800;
    int m_height = 480;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_initialized{false};
    uint32_t m_lastTick = 0;
};

} // namespace smarthub
