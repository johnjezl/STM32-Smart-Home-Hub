/**
 * Screen Base Class Implementation
 */

#include "smarthub/ui/Screen.hpp"
#include "smarthub/ui/ScreenManager.hpp"

namespace smarthub {
namespace ui {

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
        lv_obj_del(m_container);
        m_container = nullptr;
    }
#endif
}

} // namespace ui
} // namespace smarthub
