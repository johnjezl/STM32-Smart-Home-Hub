/**
 * Loading Spinner Widget
 *
 * Animated loading indicator for async operations.
 * Shows a rotating arc/circle animation.
 */
#pragma once

#include <cstdint>

#ifdef SMARTHUB_ENABLE_LVGL
#include <lvgl.h>
#endif

namespace smarthub {
namespace ui {

class ThemeManager;

/**
 * Loading Spinner - animated loading indicator
 *
 * Usage:
 *   LoadingSpinner spinner(parent, theme);
 *   spinner.show();
 *   // ... async operation ...
 *   spinner.hide();
 */
class LoadingSpinner {
public:
    /**
     * Create a loading spinner
     * @param parent Parent LVGL object
     * @param theme Theme manager for colors
     * @param size Spinner diameter in pixels (default 48)
     */
#ifdef SMARTHUB_ENABLE_LVGL
    LoadingSpinner(lv_obj_t* parent, const ThemeManager& theme, int size = 48);
#else
    LoadingSpinner(void* parent, const ThemeManager& theme, int size = 48);
#endif
    ~LoadingSpinner();

    // Non-copyable
    LoadingSpinner(const LoadingSpinner&) = delete;
    LoadingSpinner& operator=(const LoadingSpinner&) = delete;

    /**
     * Show the spinner and start animation
     */
    void show();

    /**
     * Hide the spinner and stop animation
     */
    void hide();

    /**
     * Check if spinner is currently visible
     */
    bool isVisible() const { return m_visible; }

    /**
     * Set spinner position (centered on point)
     * @param x Center X coordinate
     * @param y Center Y coordinate
     */
    void setPosition(int x, int y);

    /**
     * Set spinner size
     * @param size Diameter in pixels
     */
    void setSize(int size);

    /**
     * Set animation speed
     * @param durationMs Time for one full rotation
     */
    void setSpeed(uint32_t durationMs);

#ifdef SMARTHUB_ENABLE_LVGL
    /**
     * Get the underlying LVGL object
     */
    lv_obj_t* object() const { return m_spinner; }
#endif

    // Default values
    static constexpr int DEFAULT_SIZE = 48;
    static constexpr uint32_t DEFAULT_DURATION = 1000;  // 1 second per rotation
    static constexpr int ARC_LENGTH = 60;  // degrees of arc

private:
#ifdef SMARTHUB_ENABLE_LVGL
    lv_obj_t* m_spinner = nullptr;
    lv_anim_t m_anim;
#endif
    const ThemeManager& m_theme;
    int m_size;
    uint32_t m_duration;
    bool m_visible = false;
    bool m_animating = false;

#ifdef SMARTHUB_ENABLE_LVGL
    void createSpinner(lv_obj_t* parent);
    void startAnimation();
    void stopAnimation();

    static void rotationAnimCallback(void* obj, int32_t value);
#endif
};

} // namespace ui
} // namespace smarthub
