/**
 * Animation Manager
 *
 * Provides reusable animation utilities for UI elements.
 * Handles button feedback, value transitions, and loading states.
 */
#pragma once

#include <cstdint>
#include <functional>

#ifdef SMARTHUB_ENABLE_LVGL
#include <lvgl.h>
#endif

namespace smarthub {
namespace ui {

/**
 * Animation easing functions
 */
enum class AnimationEasing {
    Linear,
    EaseOut,
    EaseIn,
    EaseInOut,
    Overshoot,
    Bounce
};

/**
 * Animation Manager - provides animation utilities
 *
 * Usage:
 *   AnimationManager anim;
 *   anim.buttonPress(btn);  // Scale animation on press
 *   anim.fadeIn(obj, 300);  // Fade in over 300ms
 */
class AnimationManager {
public:
    AnimationManager();
    ~AnimationManager();

    // Animation timing defaults
    static constexpr uint32_t DURATION_FAST = 150;     // Quick feedback
    static constexpr uint32_t DURATION_NORMAL = 300;   // Standard transitions
    static constexpr uint32_t DURATION_SLOW = 500;     // Deliberate animations

    // Button press scale (100 = normal, 95 = pressed)
    static constexpr int PRESS_SCALE = 95;
    static constexpr int NORMAL_SCALE = 100;

#ifdef SMARTHUB_ENABLE_LVGL
    /**
     * Apply button press feedback (scale down on press, restore on release)
     * Call this once after button creation to set up the animations
     * @param btn Button object
     */
    void setupButtonPressEffect(lv_obj_t* btn);

    /**
     * Fade an object in
     * @param obj Object to animate
     * @param durationMs Animation duration
     * @param easing Easing function
     */
    void fadeIn(lv_obj_t* obj, uint32_t durationMs = DURATION_NORMAL,
                AnimationEasing easing = AnimationEasing::EaseOut);

    /**
     * Fade an object out
     * @param obj Object to animate
     * @param durationMs Animation duration
     * @param easing Easing function
     */
    void fadeOut(lv_obj_t* obj, uint32_t durationMs = DURATION_NORMAL,
                 AnimationEasing easing = AnimationEasing::EaseIn);

    /**
     * Animate a value change (e.g., for sliders, progress bars)
     * @param obj Object to animate
     * @param startVal Starting value
     * @param endVal Ending value
     * @param durationMs Animation duration
     * @param setter Callback to set the value on the object
     */
    void animateValue(lv_obj_t* obj, int32_t startVal, int32_t endVal,
                      uint32_t durationMs = DURATION_NORMAL,
                      std::function<void(lv_obj_t*, int32_t)> setter = nullptr);

    /**
     * Pulse animation (scale up then back to normal)
     * Good for attention/notification feedback
     * @param obj Object to animate
     * @param scale Maximum scale (e.g., 110 for 10% larger)
     * @param durationMs Total animation duration
     */
    void pulse(lv_obj_t* obj, int scale = 110, uint32_t durationMs = DURATION_NORMAL);

    /**
     * Shake animation (horizontal wiggle)
     * Good for error feedback
     * @param obj Object to animate
     * @param amplitude Shake distance in pixels
     * @param durationMs Total animation duration
     */
    void shake(lv_obj_t* obj, int amplitude = 10, uint32_t durationMs = DURATION_FAST);

    /**
     * Slide object to position
     * @param obj Object to animate
     * @param x Target X position
     * @param y Target Y position
     * @param durationMs Animation duration
     * @param easing Easing function
     */
    void slideTo(lv_obj_t* obj, int32_t x, int32_t y,
                 uint32_t durationMs = DURATION_NORMAL,
                 AnimationEasing easing = AnimationEasing::EaseOut);

    /**
     * Get the LVGL animation path for an easing type
     */
    static lv_anim_path_cb_t getAnimPath(AnimationEasing easing);

private:
    // Event callbacks for button press effect
    static void onButtonPressed(lv_event_t* e);
    static void onButtonReleased(lv_event_t* e);

    // Animation callbacks
    static void scaleAnimCallback(void* obj, int32_t value);
    static void opacityAnimCallback(void* obj, int32_t value);
    static void xAnimCallback(void* obj, int32_t value);
    static void yAnimCallback(void* obj, int32_t value);
#endif
};

} // namespace ui
} // namespace smarthub
