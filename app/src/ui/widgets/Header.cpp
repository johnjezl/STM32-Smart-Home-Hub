/**
 * Header Widget Implementation
 */

#include "smarthub/ui/widgets/Header.hpp"
#include "smarthub/ui/ThemeManager.hpp"

namespace smarthub {
namespace ui {

#ifdef SMARTHUB_ENABLE_LVGL

Header::Header(lv_obj_t* parent, const ThemeManager& theme)
    : m_theme(theme)
{
    // Create container
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), HEIGHT);
    lv_obj_align(m_container, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(m_container, LV_OBJ_FLAG_SCROLLABLE);

    // Apply header style
    m_theme.applyHeaderStyle(m_container);

    createLayout();
}

Header::~Header() {
    // LVGL objects are deleted when parent is deleted
}

void Header::createLayout() {
    // Title label (left side)
    m_titleLabel = lv_label_create(m_container);
    lv_label_set_text(m_titleLabel, m_title.c_str());
    lv_obj_set_style_text_color(m_titleLabel, m_theme.textPrimary(), 0);
    lv_obj_set_style_text_font(m_titleLabel, LV_FONT_DEFAULT, 0);
    lv_obj_align(m_titleLabel, LV_ALIGN_LEFT_MID, ThemeManager::SPACING_MD, 0);

    // Time label (right side)
    m_timeLabel = lv_label_create(m_container);
    lv_label_set_text(m_timeLabel, "12:00 PM");
    lv_obj_set_style_text_color(m_timeLabel, m_theme.textSecondary(), 0);
    lv_obj_align(m_timeLabel, LV_ALIGN_RIGHT_MID, -ThemeManager::SPACING_MD, 0);

    // Settings button (right of time)
    m_settingsBtn = lv_btn_create(m_container);
    lv_obj_set_size(m_settingsBtn, 40, 40);
    lv_obj_align_to(m_settingsBtn, m_timeLabel, LV_ALIGN_OUT_LEFT_MID, -ThemeManager::SPACING_SM, 0);
    lv_obj_set_style_bg_opa(m_settingsBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(m_settingsBtn, 0, 0);
    lv_obj_add_event_cb(m_settingsBtn, settingsClickHandler, LV_EVENT_CLICKED, this);

    lv_obj_t* settingsIcon = lv_label_create(m_settingsBtn);
    lv_label_set_text(settingsIcon, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(settingsIcon, m_theme.textSecondary(), 0);
    lv_obj_center(settingsIcon);

    // Notification button (left of settings)
    m_notificationBtn = lv_btn_create(m_container);
    lv_obj_set_size(m_notificationBtn, 40, 40);
    lv_obj_align_to(m_notificationBtn, m_settingsBtn, LV_ALIGN_OUT_LEFT_MID, -ThemeManager::SPACING_SM, 0);
    lv_obj_set_style_bg_opa(m_notificationBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(m_notificationBtn, 0, 0);
    lv_obj_add_event_cb(m_notificationBtn, notificationClickHandler, LV_EVENT_CLICKED, this);

    lv_obj_t* bellIcon = lv_label_create(m_notificationBtn);
    lv_label_set_text(bellIcon, LV_SYMBOL_BELL);
    lv_obj_set_style_text_color(bellIcon, m_theme.textSecondary(), 0);
    lv_obj_center(bellIcon);

    // Notification badge (hidden by default)
    m_notificationBadge = lv_obj_create(m_notificationBtn);
    lv_obj_set_size(m_notificationBadge, 16, 16);
    lv_obj_align(m_notificationBadge, LV_ALIGN_TOP_RIGHT, 2, -2);
    lv_obj_set_style_bg_color(m_notificationBadge, m_theme.error(), 0);
    lv_obj_set_style_radius(m_notificationBadge, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_flag(m_notificationBadge, LV_OBJ_FLAG_HIDDEN);
}

void Header::setTitle(const std::string& title) {
    m_title = title;
    if (m_titleLabel) {
        lv_label_set_text(m_titleLabel, title.c_str());
    }
}

void Header::setTime(int hour, int minute) {
    if (!m_timeLabel) return;

    // Convert to 12-hour format
    const char* ampm = hour >= 12 ? "PM" : "AM";
    int hour12 = hour % 12;
    if (hour12 == 0) hour12 = 12;

    char buf[16];
    snprintf(buf, sizeof(buf), "%d:%02d %s", hour12, minute, ampm);
    lv_label_set_text(m_timeLabel, buf);
}

void Header::setNotificationVisible(bool visible) {
    if (m_notificationBadge) {
        if (visible) {
            lv_obj_clear_flag(m_notificationBadge, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(m_notificationBadge, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void Header::setNotificationCount(int count) {
    setNotificationVisible(count > 0);
}

void Header::onNotificationClick(ClickCallback callback) {
    m_onNotificationClick = std::move(callback);
}

void Header::onSettingsClick(ClickCallback callback) {
    m_onSettingsClick = std::move(callback);
}

void Header::notificationClickHandler(lv_event_t* e) {
    Header* self = static_cast<Header*>(lv_event_get_user_data(e));
    if (self && self->m_onNotificationClick) {
        self->m_onNotificationClick();
    }
}

void Header::settingsClickHandler(lv_event_t* e) {
    Header* self = static_cast<Header*>(lv_event_get_user_data(e));
    if (self && self->m_onSettingsClick) {
        self->m_onSettingsClick();
    }
}

#else // !SMARTHUB_ENABLE_LVGL

Header::Header() {}
Header::~Header() {}
void Header::setTitle(const std::string&) {}
void Header::setTime(int, int) {}
void Header::setNotificationVisible(bool) {}
void Header::setNotificationCount(int) {}
void Header::onNotificationClick(ClickCallback) {}
void Header::onSettingsClick(ClickCallback) {}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
