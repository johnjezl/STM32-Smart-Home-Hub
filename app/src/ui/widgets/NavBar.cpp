/**
 * NavBar Widget Implementation
 */

#include "smarthub/ui/widgets/NavBar.hpp"
#include "smarthub/ui/ThemeManager.hpp"

namespace smarthub {
namespace ui {

#ifdef SMARTHUB_ENABLE_LVGL

NavBar::NavBar(lv_obj_t* parent, const ThemeManager& theme)
    : m_theme(theme)
{
    // Create container
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), HEIGHT);
    lv_obj_align(m_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(m_container, LV_OBJ_FLAG_SCROLLABLE);

    // Apply navbar style
    m_theme.applyNavBarStyle(m_container);

    // Use flexbox for even spacing
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(m_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
}

NavBar::~NavBar() {
    // LVGL objects are deleted when parent is deleted
}

void NavBar::addTab(const NavTab& tab) {
    m_tabs.push_back(tab);

    // Create button
    lv_obj_t* btn = lv_btn_create(m_container);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, HEIGHT - ThemeManager::SPACING_MD);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_hor(btn, ThemeManager::SPACING_MD, 0);

    // Create column layout for icon + label
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Icon
    lv_obj_t* icon = lv_label_create(btn);
    lv_label_set_text(icon, tab.icon.c_str());
    lv_obj_set_style_text_color(icon, m_theme.textSecondary(), 0);

    // Label
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, tab.label.c_str());
    lv_obj_set_style_text_color(label, m_theme.textSecondary(), 0);

    // Store tab index in user data
    lv_obj_set_user_data(btn, reinterpret_cast<void*>(m_tabs.size() - 1));
    lv_obj_add_event_cb(btn, tabClickHandler, LV_EVENT_CLICKED, this);

    m_tabButtons.push_back(btn);

    // Set first tab as active by default
    if (m_tabs.size() == 1) {
        setActiveTab(tab.id);
    }
}

void NavBar::setActiveTab(const std::string& tabId) {
    m_activeTabId = tabId;
    updateTabStyles();
}

void NavBar::onTabSelected(TabCallback callback) {
    m_onTabSelected = std::move(callback);
}

void NavBar::updateTabStyles() {
    for (size_t i = 0; i < m_tabs.size() && i < m_tabButtons.size(); ++i) {
        lv_obj_t* btn = m_tabButtons[i];
        bool isActive = (m_tabs[i].id == m_activeTabId);

        // Get icon and label children
        lv_obj_t* icon = lv_obj_get_child(btn, 0);
        lv_obj_t* label = lv_obj_get_child(btn, 1);

        if (isActive) {
            lv_obj_set_style_text_color(icon, m_theme.primary(), 0);
            lv_obj_set_style_text_color(label, m_theme.primary(), 0);
        } else {
            lv_obj_set_style_text_color(icon, m_theme.textSecondary(), 0);
            lv_obj_set_style_text_color(label, m_theme.textSecondary(), 0);
        }
    }
}

void NavBar::tabClickHandler(lv_event_t* e) {
    NavBar* self = static_cast<NavBar*>(lv_event_get_user_data(e));
    lv_obj_t* btn = lv_event_get_target(e);
    size_t index = reinterpret_cast<size_t>(lv_obj_get_user_data(btn));

    if (self && index < self->m_tabs.size()) {
        const std::string& tabId = self->m_tabs[index].id;
        self->setActiveTab(tabId);

        if (self->m_onTabSelected) {
            self->m_onTabSelected(tabId);
        }
    }
}

#else // !SMARTHUB_ENABLE_LVGL

NavBar::NavBar() {}
NavBar::~NavBar() {}
void NavBar::addTab(const NavTab&) {}
void NavBar::setActiveTab(const std::string&) {}
void NavBar::onTabSelected(TabCallback) {}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
