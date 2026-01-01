/**
 * Screen Base Class
 *
 * Abstract base class for all UI screens. Provides lifecycle management
 * and integration with the ScreenManager navigation system.
 */
#pragma once

#include <string>
#include <cstdint>

#ifdef SMARTHUB_ENABLE_LVGL
#include <lvgl.h>
#endif

namespace smarthub {

class UIManager;

namespace ui {

class ScreenManager;

/**
 * Screen lifecycle states
 */
enum class ScreenState {
    Created,    // onCreate() called
    Visible,    // onShow() called, currently displayed
    Hidden,     // onHide() called, not visible
    Destroyed   // onDestroy() called
};

/**
 * Base class for all UI screens
 *
 * Lifecycle:
 * 1. Constructor - allocate resources
 * 2. onCreate() - create LVGL objects
 * 3. onShow() - screen becomes visible (may be called multiple times)
 * 4. onUpdate() - called each frame while visible
 * 5. onHide() - screen becomes hidden (may be called multiple times)
 * 6. onDestroy() - cleanup before deletion
 * 7. Destructor - free resources
 */
class Screen {
public:
    /**
     * Construct a screen
     * @param screenManager Reference to the screen manager
     * @param name Unique screen identifier
     */
    Screen(ScreenManager& screenManager, const std::string& name);

    virtual ~Screen();

    // Non-copyable
    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;

    /**
     * Called once after construction to create LVGL objects
     * Subclasses must implement this to build their UI
     */
    virtual void onCreate() = 0;

    /**
     * Called when the screen becomes visible
     * Use for refreshing data, starting animations
     */
    virtual void onShow() {}

    /**
     * Called each frame while the screen is visible
     * Use for animations, live data updates
     * @param deltaMs Milliseconds since last update
     */
    virtual void onUpdate(uint32_t deltaMs) { (void)deltaMs; }

    /**
     * Called when the screen becomes hidden
     * Use for pausing animations, saving state
     */
    virtual void onHide() {}

    /**
     * Called before destruction to cleanup
     * LVGL objects are auto-deleted when container is deleted
     */
    virtual void onDestroy() {}

    /**
     * Get the screen's unique name
     */
    const std::string& name() const { return m_name; }

    /**
     * Get the current lifecycle state
     */
    ScreenState state() const { return m_state; }

    /**
     * Check if screen is currently visible
     */
    bool isVisible() const { return m_state == ScreenState::Visible; }

#ifdef SMARTHUB_ENABLE_LVGL
    /**
     * Get the root container for this screen
     * All screen content should be added as children of this object
     */
    lv_obj_t* container() const { return m_container; }
#endif

protected:
    ScreenManager& m_screenManager;
    std::string m_name;
    ScreenState m_state = ScreenState::Created;

#ifdef SMARTHUB_ENABLE_LVGL
    lv_obj_t* m_container = nullptr;
#endif

    // Allow ScreenManager to manage lifecycle
    friend class ScreenManager;

    void setState(ScreenState state) { m_state = state; }

#ifdef SMARTHUB_ENABLE_LVGL
    void setContainer(lv_obj_t* container) { m_container = container; }
#endif
};

} // namespace ui
} // namespace smarthub
