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

// Forward declare LVGL types for modal focus management
struct _lv_group_t;
typedef struct _lv_group_t lv_group_t;

namespace smarthub {

class EventBus;
class DeviceManager;
class AutomationManager;
class Database;

namespace network {
class NetworkManager;
}

namespace ui {
class ScreenManager;
class ThemeManager;

/**
 * Push a new focus group for a modal dialog.
 * Creates a new group, sets it as active for keyboard input, and returns it.
 * Add the modal's interactive widgets to this group using lv_group_add_obj().
 * Call popModalFocusGroup() when the modal is closed.
 * @return The new modal group, or nullptr if no keyboard is available
 */
lv_group_t* pushModalFocusGroup();

/**
 * Pop the current modal focus group and restore the previous one.
 * Call this when closing a modal dialog.
 */
void popModalFocusGroup();

}

/**
 * UI Manager for LVGL-based touchscreen interface
 */
class UIManager {
public:
    UIManager(EventBus& eventBus, DeviceManager& deviceManager, Database& database);
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

    /**
     * Get the screen manager
     */
    ui::ScreenManager* screenManager() const { return m_screenManager.get(); }

    /**
     * Get the theme manager
     */
    ui::ThemeManager* themeManager() const { return m_themeManager.get(); }

private:
    void setupScreens();

    EventBus& m_eventBus;
    DeviceManager& m_deviceManager;
    Database& m_database;

    std::string m_fbDevice;
    std::string m_touchDevice;
    int m_width = 800;
    int m_height = 480;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_initialized{false};
    uint32_t m_lastTick = 0;

    std::unique_ptr<ui::ThemeManager> m_themeManager;
    std::unique_ptr<ui::ScreenManager> m_screenManager;
    std::unique_ptr<network::NetworkManager> m_networkManager;
    std::unique_ptr<AutomationManager> m_automationManager;
};

} // namespace smarthub
