/**
 * Loading Spinner Widget Implementation
 */

#include "smarthub/ui/widgets/LoadingSpinner.hpp"
#include "smarthub/ui/ThemeManager.hpp"
#include "smarthub/core/Logger.hpp"

namespace smarthub {
namespace ui {

#ifdef SMARTHUB_ENABLE_LVGL

LoadingSpinner::LoadingSpinner(lv_obj_t* parent, const ThemeManager& theme, int size)
    : m_theme(theme)
    , m_size(size)
    , m_duration(DEFAULT_DURATION)
{
    createSpinner(parent);
}

#else

LoadingSpinner::LoadingSpinner(void* parent, const ThemeManager& theme, int size)
    : m_theme(theme)
    , m_size(size)
    , m_duration(DEFAULT_DURATION)
{
    (void)parent;
}

#endif

LoadingSpinner::~LoadingSpinner() {
#ifdef SMARTHUB_ENABLE_LVGL
    stopAnimation();
    if (m_spinner) {
        lv_obj_del(m_spinner);
        m_spinner = nullptr;
    }
#endif
}

void LoadingSpinner::show() {
    m_visible = true;
#ifdef SMARTHUB_ENABLE_LVGL
    if (m_spinner) {
        lv_obj_clear_flag(m_spinner, LV_OBJ_FLAG_HIDDEN);
        startAnimation();
    }
#endif
}

void LoadingSpinner::hide() {
    m_visible = false;
#ifdef SMARTHUB_ENABLE_LVGL
    if (m_spinner) {
        stopAnimation();
        lv_obj_add_flag(m_spinner, LV_OBJ_FLAG_HIDDEN);
    }
#endif
}

void LoadingSpinner::setPosition(int x, int y) {
#ifdef SMARTHUB_ENABLE_LVGL
    if (m_spinner) {
        lv_obj_set_pos(m_spinner, x - m_size / 2, y - m_size / 2);
    }
#else
    (void)x;
    (void)y;
#endif
}

void LoadingSpinner::setSize(int size) {
    m_size = size;
#ifdef SMARTHUB_ENABLE_LVGL
    if (m_spinner) {
        lv_obj_set_size(m_spinner, size, size);
        // Update arc width proportionally
        int arcWidth = size / 8;
        if (arcWidth < 3) arcWidth = 3;
        lv_obj_set_style_arc_width(m_spinner, arcWidth, 0);
        lv_obj_set_style_arc_width(m_spinner, arcWidth, LV_PART_INDICATOR);
    }
#endif
}

void LoadingSpinner::setSpeed(uint32_t durationMs) {
    m_duration = durationMs;
    // Restart animation with new speed if currently animating
#ifdef SMARTHUB_ENABLE_LVGL
    if (m_animating) {
        stopAnimation();
        startAnimation();
    }
#endif
}

#ifdef SMARTHUB_ENABLE_LVGL

void LoadingSpinner::createSpinner(lv_obj_t* parent) {
    // Create arc object for the spinner
    m_spinner = lv_arc_create(parent);
    lv_obj_set_size(m_spinner, m_size, m_size);
    lv_obj_center(m_spinner);

    // Remove the knob and background arc
    lv_obj_remove_style(m_spinner, nullptr, LV_PART_KNOB);
    lv_obj_set_style_arc_opa(m_spinner, LV_OPA_30, 0);  // Background arc (faint)
    lv_obj_set_style_arc_opa(m_spinner, LV_OPA_COVER, LV_PART_INDICATOR);

    // Set arc colors
    lv_obj_set_style_arc_color(m_spinner, m_theme.primary(), 0);
    lv_obj_set_style_arc_color(m_spinner, m_theme.primary(), LV_PART_INDICATOR);

    // Arc width
    int arcWidth = m_size / 8;
    if (arcWidth < 3) arcWidth = 3;
    lv_obj_set_style_arc_width(m_spinner, arcWidth, 0);
    lv_obj_set_style_arc_width(m_spinner, arcWidth, LV_PART_INDICATOR);

    // Set arc angles - indicator is the visible spinning part
    lv_arc_set_bg_angles(m_spinner, 0, 360);
    lv_arc_set_angles(m_spinner, 0, ARC_LENGTH);

    // Disable user interaction
    lv_obj_clear_flag(m_spinner, LV_OBJ_FLAG_CLICKABLE);

    // Start hidden
    lv_obj_add_flag(m_spinner, LV_OBJ_FLAG_HIDDEN);

    LOG_DEBUG("LoadingSpinner created, size: %d", m_size);
}

void LoadingSpinner::startAnimation() {
    if (m_animating || !m_spinner) return;

    lv_anim_init(&m_anim);
    lv_anim_set_var(&m_anim, m_spinner);
    lv_anim_set_values(&m_anim, 0, 360);
    lv_anim_set_time(&m_anim, m_duration);
    lv_anim_set_repeat_count(&m_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&m_anim, lv_anim_path_linear);
    lv_anim_set_exec_cb(&m_anim, rotationAnimCallback);
    lv_anim_start(&m_anim);

    m_animating = true;
}

void LoadingSpinner::stopAnimation() {
    if (!m_animating || !m_spinner) return;

    lv_anim_del(m_spinner, rotationAnimCallback);
    m_animating = false;
}

void LoadingSpinner::rotationAnimCallback(void* obj, int32_t value) {
    lv_obj_t* arc = static_cast<lv_obj_t*>(obj);
    lv_arc_set_angles(arc, value, value + ARC_LENGTH);
}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
