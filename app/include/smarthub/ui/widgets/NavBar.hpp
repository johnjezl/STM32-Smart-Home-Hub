/**
 * NavBar Widget
 *
 * Bottom navigation bar with tab buttons.
 * Height: 60px, full width
 */
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

#ifdef SMARTHUB_ENABLE_LVGL
#include <lvgl.h>
#endif

namespace smarthub {
namespace ui {

class ThemeManager;

/**
 * Navigation tab definition
 */
struct NavTab {
    std::string id;       // Unique identifier
    std::string label;    // Display text
    std::string icon;     // LVGL symbol (e.g., LV_SYMBOL_HOME)
};

/**
 * NavBar widget for bottom navigation
 *
 * Layout (800x60):
 * +---------------------------------------------------------------+
 * |    [🏠 Home]    [💡 Devices]    [📊 Sensors]    [⚙️ Settings] |
 * +---------------------------------------------------------------+
 */
class NavBar {
public:
    using TabCallback = std::function<void(const std::string& tabId)>;

#ifdef SMARTHUB_ENABLE_LVGL
    /**
     * Create navbar widget
     * @param parent Parent LVGL object
     * @param theme Theme manager for styling
     */
    NavBar(lv_obj_t* parent, const ThemeManager& theme);
#else
    NavBar();
#endif
    ~NavBar();

    /**
     * Add a navigation tab
     * @param tab Tab definition
     */
    void addTab(const NavTab& tab);

    /**
     * Set the active tab
     * @param tabId Tab identifier
     */
    void setActiveTab(const std::string& tabId);

    /**
     * Get the currently active tab ID
     */
    const std::string& activeTab() const { return m_activeTabId; }

    /**
     * Set callback for tab selection
     */
    void onTabSelected(TabCallback callback);

#ifdef SMARTHUB_ENABLE_LVGL
    /**
     * Get the root object
     */
    lv_obj_t* obj() const { return m_container; }
#endif

    static constexpr int HEIGHT = 60;

private:
#ifdef SMARTHUB_ENABLE_LVGL
    void updateTabStyles();
    static void tabClickHandler(lv_event_t* e);

    lv_obj_t* m_container = nullptr;
    std::vector<lv_obj_t*> m_tabButtons;

    const ThemeManager& m_theme;
#endif

    std::vector<NavTab> m_tabs;
    std::string m_activeTabId;
    TabCallback m_onTabSelected;
};

} // namespace ui
} // namespace smarthub
