/**
 * Header Widget
 *
 * Top bar with title, clock, and action buttons (notifications, settings).
 * Height: 50px, full width
 */
#pragma once

#include <string>
#include <functional>
#include <cstdint>

#ifdef SMARTHUB_ENABLE_LVGL
#include <lvgl.h>
#endif

namespace smarthub {
namespace ui {

class ThemeManager;

/**
 * Header widget for screen top bar
 *
 * Layout (800x50):
 * +---------------------------------------------------------------+
 * |  SmartHub                                    🔔  ⚙️  12:34 PM |
 * +---------------------------------------------------------------+
 */
class Header {
public:
    using ClickCallback = std::function<void()>;

    /**
     * Create header widget
     * @param parent Parent LVGL object
     * @param theme Theme manager for styling
     */
#ifdef SMARTHUB_ENABLE_LVGL
    Header(lv_obj_t* parent, const ThemeManager& theme);
#else
    Header();
#endif
    ~Header();

    /**
     * Set the title text
     */
    void setTitle(const std::string& title);

    /**
     * Update the clock display
     * @param hour 0-23
     * @param minute 0-59
     */
    void setTime(int hour, int minute);

    /**
     * Set notification indicator visibility
     */
    void setNotificationVisible(bool visible);

    /**
     * Set notification count badge
     */
    void setNotificationCount(int count);

    /**
     * Set callbacks for button clicks
     */
    void onNotificationClick(ClickCallback callback);
    void onSettingsClick(ClickCallback callback);

#ifdef SMARTHUB_ENABLE_LVGL
    /**
     * Get the root object
     */
    lv_obj_t* obj() const { return m_container; }
#endif

    static constexpr int HEIGHT = 50;

private:
#ifdef SMARTHUB_ENABLE_LVGL
    void createLayout();

    static void notificationClickHandler(lv_event_t* e);
    static void settingsClickHandler(lv_event_t* e);

    lv_obj_t* m_container = nullptr;
    lv_obj_t* m_titleLabel = nullptr;
    lv_obj_t* m_timeLabel = nullptr;
    lv_obj_t* m_notificationBtn = nullptr;
    lv_obj_t* m_notificationBadge = nullptr;
    lv_obj_t* m_settingsBtn = nullptr;

    const ThemeManager& m_theme;
#endif

    std::string m_title = "SmartHub";
    ClickCallback m_onNotificationClick;
    ClickCallback m_onSettingsClick;
};

} // namespace ui
} // namespace smarthub
