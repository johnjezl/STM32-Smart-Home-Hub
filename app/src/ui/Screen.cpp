/**
 * Screen Base Class Implementation
 */

#include "smarthub/ui/Screen.hpp"
#include "smarthub/ui/ScreenManager.hpp"

namespace smarthub {
namespace ui {

#ifdef SMARTHUB_ENABLE_LVGL
namespace {

// LVGL keeps pointers to the screens it's showing and loading, and deleting one
// leaves them dangling: a later load then dereferences a stale or null pointer.
// Clear them first. LVGL 8.3 has no API to finish a load, so this sets the fields.
void detachFromDisplay(lv_obj_t* screen) {
    lv_disp_t* display = lv_obj_get_disp(screen);
    if (!display) {
        return;
    }

    // Only a load this screen is part of needs to finish early.
    lv_obj_t* loading = display->scr_to_load;
    if (loading && (screen == loading || screen == display->act_scr || screen == display->prev_scr)) {
        for (lv_obj_t* involved : {display->act_scr, display->prev_scr, loading}) {
            if (involved) {
                lv_anim_del(involved, nullptr);
                lv_obj_set_pos(involved, 0, 0);
                lv_obj_remove_local_style_prop(involved, LV_STYLE_OPA, 0);
            }
        }
        display->act_scr = loading;
        display->scr_to_load = nullptr;
        display->prev_scr = nullptr;
        display->draw_prev_over_act = false;
    }

    if (display->act_scr != screen) {
        return;
    }

    // Show another screen, preferring an existing one so nothing is left to free.
    // The first in the list is LVGL's original, empty screen.
    for (uint32_t i = 0; i < display->screen_cnt; ++i) {
        lv_obj_t* other = display->screens[i];
        if (other != screen && other != display->top_layer && other != display->sys_layer) {
            lv_scr_load(other);
            return;
        }
    }
    lv_scr_load(lv_obj_create(nullptr));
}

} // namespace
#endif

void Screen::postToUi(std::function<void()> task) {
    m_screenManager.postToUi([alive = std::weak_ptr<void>(m_aliveToken), task = std::move(task)] {
        if (alive.lock()) {
            task();
        }
    });
}

Screen::Screen(ScreenManager& screenManager, const std::string& name)
    : m_screenManager(screenManager)
    , m_name(name)
    , m_state(ScreenState::Created)
#ifdef SMARTHUB_ENABLE_LVGL
    , m_container(nullptr)
#endif
{
}

Screen::~Screen() {
#ifdef SMARTHUB_ENABLE_LVGL
    // Container is deleted by LVGL when parent screen is deleted
    // or explicitly in onDestroy()
    if (m_container) {
        detachFromDisplay(m_container);
        lv_obj_del(m_container);
        m_container = nullptr;
    }
#endif
}

} // namespace ui
} // namespace smarthub
