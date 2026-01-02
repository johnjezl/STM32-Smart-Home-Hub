/**
 * Animation Manager Implementation
 */

#include "smarthub/ui/AnimationManager.hpp"
#include "smarthub/core/Logger.hpp"

namespace smarthub {
namespace ui {

AnimationManager::AnimationManager() = default;
AnimationManager::~AnimationManager() = default;

#ifdef SMARTHUB_ENABLE_LVGL

void AnimationManager::setupButtonPressEffect(lv_obj_t* btn) {
    if (!btn) return;

    // Set default transform origin to center
    lv_obj_set_style_transform_pivot_x(btn, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(btn, lv_pct(50), 0);

    // Pressed state scale (256 = 100%, so 95% = 243)
    lv_obj_set_style_transform_zoom(btn, 256 * PRESS_SCALE / 100, LV_STATE_PRESSED);

    // Enable transition for smooth animation
    static lv_style_transition_dsc_t trans;
    static lv_style_prop_t props[] = {
        LV_STYLE_TRANSFORM_ZOOM,
        LV_STYLE_PROP_INV  // End marker
    };
    lv_style_transition_dsc_init(&trans, props, lv_anim_path_ease_out, DURATION_FAST, 0, nullptr);

    lv_obj_set_style_transition(btn, &trans, LV_STATE_PRESSED);
    lv_obj_set_style_transition(btn, &trans, LV_STATE_DEFAULT);
}

void AnimationManager::fadeIn(lv_obj_t* obj, uint32_t durationMs, AnimationEasing easing) {
    if (!obj) return;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_time(&a, durationMs);
    lv_anim_set_path_cb(&a, getAnimPath(easing));
    lv_anim_set_exec_cb(&a, opacityAnimCallback);
    lv_anim_start(&a);
}

void AnimationManager::fadeOut(lv_obj_t* obj, uint32_t durationMs, AnimationEasing easing) {
    if (!obj) return;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, 255, 0);
    lv_anim_set_time(&a, durationMs);
    lv_anim_set_path_cb(&a, getAnimPath(easing));
    lv_anim_set_exec_cb(&a, opacityAnimCallback);
    lv_anim_start(&a);
}

void AnimationManager::animateValue(lv_obj_t* obj, int32_t startVal, int32_t endVal,
                                     uint32_t durationMs,
                                     std::function<void(lv_obj_t*, int32_t)> setter) {
    if (!obj) return;

    // For simple cases without custom setter, we can use LVGL's built-in
    // animation callbacks for common widgets
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, startVal, endVal);
    lv_anim_set_time(&a, durationMs);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);

    // Use the provided setter if available, otherwise use a default
    if (setter) {
        // Store the setter in user data and use a wrapper callback
        // Note: This is simplified - a production version would need proper
        // lifetime management of the callback
        lv_anim_set_exec_cb(&a, [](void* var, int32_t v) {
            // Default to slider value for now
            lv_slider_set_value(static_cast<lv_obj_t*>(var), v, LV_ANIM_OFF);
        });
    } else {
        // Try to detect widget type and use appropriate setter
        if (lv_obj_check_type(obj, &lv_slider_class)) {
            lv_anim_set_exec_cb(&a, [](void* var, int32_t v) {
                lv_slider_set_value(static_cast<lv_obj_t*>(var), v, LV_ANIM_OFF);
            });
        } else if (lv_obj_check_type(obj, &lv_bar_class)) {
            lv_anim_set_exec_cb(&a, [](void* var, int32_t v) {
                lv_bar_set_value(static_cast<lv_obj_t*>(var), v, LV_ANIM_OFF);
            });
        } else if (lv_obj_check_type(obj, &lv_arc_class)) {
            lv_anim_set_exec_cb(&a, [](void* var, int32_t v) {
                lv_arc_set_value(static_cast<lv_obj_t*>(var), v);
            });
        }
    }

    lv_anim_start(&a);
}

void AnimationManager::pulse(lv_obj_t* obj, int scale, uint32_t durationMs) {
    if (!obj) return;

    // Set transform pivot to center
    lv_obj_set_style_transform_pivot_x(obj, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(obj, lv_pct(50), 0);

    // Scale value: 256 = 100%, so scale of 110 means 110% = 281
    int32_t targetScale = 256 * scale / 100;

    // Phase 1: Scale up
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, 256, targetScale);
    lv_anim_set_time(&a, durationMs / 2);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, scaleAnimCallback);
    lv_anim_set_playback_time(&a, durationMs / 2);
    lv_anim_set_playback_delay(&a, 0);
    lv_anim_start(&a);
}

void AnimationManager::shake(lv_obj_t* obj, int amplitude, uint32_t durationMs) {
    if (!obj) return;

    int32_t origX = lv_obj_get_x(obj);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, origX - amplitude, origX + amplitude);
    lv_anim_set_time(&a, durationMs / 4);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_set_exec_cb(&a, xAnimCallback);
    lv_anim_set_playback_time(&a, durationMs / 4);
    lv_anim_set_repeat_count(&a, 2);

    // After shake, return to original position
    lv_anim_set_ready_cb(&a, [](lv_anim_t* anim) {
        lv_obj_t* target = static_cast<lv_obj_t*>(anim->var);
        lv_obj_update_layout(lv_obj_get_parent(target));
    });

    lv_anim_start(&a);
}

void AnimationManager::slideTo(lv_obj_t* obj, int32_t x, int32_t y,
                                uint32_t durationMs, AnimationEasing easing) {
    if (!obj) return;

    // Animate X
    lv_anim_t aX;
    lv_anim_init(&aX);
    lv_anim_set_var(&aX, obj);
    lv_anim_set_values(&aX, lv_obj_get_x(obj), x);
    lv_anim_set_time(&aX, durationMs);
    lv_anim_set_path_cb(&aX, getAnimPath(easing));
    lv_anim_set_exec_cb(&aX, xAnimCallback);
    lv_anim_start(&aX);

    // Animate Y
    lv_anim_t aY;
    lv_anim_init(&aY);
    lv_anim_set_var(&aY, obj);
    lv_anim_set_values(&aY, lv_obj_get_y(obj), y);
    lv_anim_set_time(&aY, durationMs);
    lv_anim_set_path_cb(&aY, getAnimPath(easing));
    lv_anim_set_exec_cb(&aY, yAnimCallback);
    lv_anim_start(&aY);
}

lv_anim_path_cb_t AnimationManager::getAnimPath(AnimationEasing easing) {
    switch (easing) {
        case AnimationEasing::Linear:
            return lv_anim_path_linear;
        case AnimationEasing::EaseOut:
            return lv_anim_path_ease_out;
        case AnimationEasing::EaseIn:
            return lv_anim_path_ease_in;
        case AnimationEasing::EaseInOut:
            return lv_anim_path_ease_in_out;
        case AnimationEasing::Overshoot:
            return lv_anim_path_overshoot;
        case AnimationEasing::Bounce:
            return lv_anim_path_bounce;
        default:
            return lv_anim_path_linear;
    }
}

void AnimationManager::scaleAnimCallback(void* obj, int32_t value) {
    lv_obj_t* target = static_cast<lv_obj_t*>(obj);
    lv_obj_set_style_transform_zoom(target, value, 0);
}

void AnimationManager::opacityAnimCallback(void* obj, int32_t value) {
    lv_obj_t* target = static_cast<lv_obj_t*>(obj);
    lv_obj_set_style_opa(target, static_cast<lv_opa_t>(value), 0);
}

void AnimationManager::xAnimCallback(void* obj, int32_t value) {
    lv_obj_t* target = static_cast<lv_obj_t*>(obj);
    lv_obj_set_x(target, value);
}

void AnimationManager::yAnimCallback(void* obj, int32_t value) {
    lv_obj_t* target = static_cast<lv_obj_t*>(obj);
    lv_obj_set_y(target, value);
}

void AnimationManager::onButtonPressed(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    lv_obj_set_style_transform_zoom(btn, 256 * PRESS_SCALE / 100, 0);
}

void AnimationManager::onButtonReleased(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    lv_obj_set_style_transform_zoom(btn, 256, 0);
}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
