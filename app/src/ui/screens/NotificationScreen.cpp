/**
 * Notification Screen Implementation
 */

#include "smarthub/ui/screens/NotificationScreen.hpp"
#include "smarthub/ui/ScreenManager.hpp"
#include "smarthub/ui/ThemeManager.hpp"
#include "smarthub/ui/widgets/Header.hpp"
#include "smarthub/devices/DeviceManager.hpp"
#include "smarthub/core/Logger.hpp"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace smarthub {
namespace ui {

NotificationScreen::NotificationScreen(ScreenManager& screenManager,
                                       ThemeManager& theme,
                                       DeviceManager& deviceManager)
    : Screen(screenManager, "notifications")
    , m_theme(theme)
    , m_deviceManager(deviceManager)
{
}

NotificationScreen::~NotificationScreen() = default;

void NotificationScreen::addNotification(const Notification& notification) {
    // Add to front of list (newest first)
    m_notifications.insert(m_notifications.begin(), notification);

    // Trim if too many
    if (m_notifications.size() > MAX_NOTIFICATIONS) {
        m_notifications.resize(MAX_NOTIFICATIONS);
    }
}

int NotificationScreen::getUnreadCount() const {
    return static_cast<int>(std::count_if(m_notifications.begin(), m_notifications.end(),
        [](const Notification& n) { return !n.read; }));
}

void NotificationScreen::markAllRead() {
    for (auto& notification : m_notifications) {
        notification.read = true;
    }
}

void NotificationScreen::clearAll() {
    m_notifications.clear();
    if (m_onCleared) {
        m_onCleared();
    }
}

void NotificationScreen::onNotificationsCleared(ClearCallback callback) {
    m_onCleared = std::move(callback);
}

void NotificationScreen::onCreate() {
#ifdef SMARTHUB_ENABLE_LVGL
    if (!m_container) return;

    lv_obj_set_style_bg_color(m_container, m_theme.background(), 0);

    createHeader();
    createNotificationList();

    LOG_DEBUG("NotificationScreen created");
#endif
}

void NotificationScreen::onShow() {
#ifdef SMARTHUB_ENABLE_LVGL
    markAllRead();
    updateNotificationList();
    LOG_DEBUG("NotificationScreen shown");
#endif
}

void NotificationScreen::onHide() {
    LOG_DEBUG("NotificationScreen hidden");
}

void NotificationScreen::onUpdate(uint32_t /* deltaMs */) {
}

void NotificationScreen::onDestroy() {
    LOG_DEBUG("NotificationScreen destroyed");
}

#ifdef SMARTHUB_ENABLE_LVGL

void NotificationScreen::createHeader() {
    lv_obj_t* header = lv_obj_create(m_container);
    lv_obj_set_size(header, LV_PCT(100), Header::HEIGHT);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    m_theme.applyHeaderStyle(header);

    // Back button
    m_backBtn = lv_btn_create(header);
    lv_obj_set_size(m_backBtn, 48, 48);
    lv_obj_align(m_backBtn, LV_ALIGN_LEFT_MID, ThemeManager::SPACING_SM, 0);
    lv_obj_set_style_bg_opa(m_backBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(m_backBtn, 0, 0);
    lv_obj_add_event_cb(m_backBtn, backButtonHandler, LV_EVENT_CLICKED, this);

    lv_obj_t* backIcon = lv_label_create(m_backBtn);
    lv_label_set_text(backIcon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(backIcon, m_theme.textPrimary(), 0);
    lv_obj_center(backIcon);

    // Title
    m_titleLabel = lv_label_create(header);
    lv_label_set_text(m_titleLabel, "Notifications");
    lv_obj_set_style_text_color(m_titleLabel, m_theme.textPrimary(), 0);
    lv_obj_set_style_text_font(m_titleLabel, &lv_font_montserrat_20, 0);
    lv_obj_align(m_titleLabel, LV_ALIGN_CENTER, 0, 0);

    // Clear all button
    m_clearBtn = lv_btn_create(header);
    lv_obj_set_size(m_clearBtn, 80, 36);
    lv_obj_align(m_clearBtn, LV_ALIGN_RIGHT_MID, -ThemeManager::SPACING_SM, 0);
    lv_obj_set_style_bg_color(m_clearBtn, m_theme.surfaceVariant(), 0);
    lv_obj_set_style_shadow_width(m_clearBtn, 0, 0);
    lv_obj_add_event_cb(m_clearBtn, clearButtonHandler, LV_EVENT_CLICKED, this);

    lv_obj_t* clearLabel = lv_label_create(m_clearBtn);
    lv_label_set_text(clearLabel, "Clear");
    lv_obj_set_style_text_color(clearLabel, m_theme.textPrimary(), 0);
    lv_obj_center(clearLabel);
}

void NotificationScreen::createNotificationList() {
    int topOffset = Header::HEIGHT;
    int height = 480 - topOffset;

    m_notificationList = lv_obj_create(m_container);
    lv_obj_set_size(m_notificationList, LV_PCT(100), height);
    lv_obj_align(m_notificationList, LV_ALIGN_TOP_MID, 0, topOffset);
    lv_obj_set_style_bg_opa(m_notificationList, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_notificationList, 0, 0);
    lv_obj_set_style_pad_all(m_notificationList, ThemeManager::SPACING_MD, 0);
    lv_obj_set_flex_flow(m_notificationList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_notificationList, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(m_notificationList, ThemeManager::SPACING_SM, 0);
    lv_obj_set_scroll_dir(m_notificationList, LV_DIR_VER);

    // Empty state label
    m_emptyLabel = lv_label_create(m_notificationList);
    lv_label_set_text(m_emptyLabel, "No notifications");
    lv_obj_set_style_text_color(m_emptyLabel, m_theme.textSecondary(), 0);
    lv_obj_add_flag(m_emptyLabel, LV_OBJ_FLAG_HIDDEN);
}

void NotificationScreen::updateNotificationList() {
    if (!m_notificationList) return;

    // Clear existing items (but not the empty label)
    for (auto* item : m_notificationItems) {
        lv_obj_del(item);
    }
    m_notificationItems.clear();

    if (m_notifications.empty()) {
        lv_obj_clear_flag(m_emptyLabel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(m_emptyLabel, LV_ALIGN_CENTER, 0, 0);
        return;
    }

    lv_obj_add_flag(m_emptyLabel, LV_OBJ_FLAG_HIDDEN);

    // Create notification items
    for (const auto& notification : m_notifications) {
        lv_obj_t* item = createNotificationItem(m_notificationList, notification);
        m_notificationItems.push_back(item);
    }
}

lv_obj_t* NotificationScreen::createNotificationItem(lv_obj_t* parent,
                                                      const Notification& notification) {
    lv_obj_t* item = lv_obj_create(parent);
    lv_obj_set_size(item, lv_pct(100) - 2 * ThemeManager::SPACING_MD, 80);
    m_theme.applyCardStyle(item);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

    // Icon based on type
    lv_obj_t* icon = lv_label_create(item);
    lv_label_set_text(icon, getNotificationIcon(notification.type));

    lv_color_t iconColor;
    switch (notification.type) {
        case NotificationType::Error:
            iconColor = m_theme.error();
            break;
        case NotificationType::Warning:
        case NotificationType::Alert:
            iconColor = m_theme.warning();
            break;
        case NotificationType::Motion:
        case NotificationType::Door:
            iconColor = m_theme.primary();
            break;
        default:
            iconColor = m_theme.textSecondary();
            break;
    }
    lv_obj_set_style_text_color(icon, iconColor, 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);

    // Title
    lv_obj_t* titleLabel = lv_label_create(item);
    lv_label_set_text(titleLabel, notification.title.c_str());
    lv_obj_set_style_text_color(titleLabel, m_theme.textPrimary(), 0);
    lv_obj_align(titleLabel, LV_ALIGN_TOP_LEFT, 45, 8);
    lv_label_set_long_mode(titleLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_width(titleLabel, 500);

    // Message
    lv_obj_t* messageLabel = lv_label_create(item);
    lv_label_set_text(messageLabel, notification.message.c_str());
    lv_obj_set_style_text_color(messageLabel, m_theme.textSecondary(), 0);
    lv_obj_set_style_text_font(messageLabel, &lv_font_montserrat_12, 0);
    lv_obj_align(messageLabel, LV_ALIGN_TOP_LEFT, 45, 32);
    lv_label_set_long_mode(messageLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_width(messageLabel, 500);

    // Timestamp
    lv_obj_t* timeLabel = lv_label_create(item);
    lv_label_set_text(timeLabel, formatTimestamp(notification.timestamp).c_str());
    lv_obj_set_style_text_color(timeLabel, m_theme.textSecondary(), 0);
    lv_obj_set_style_text_font(timeLabel, &lv_font_montserrat_12, 0);
    lv_obj_align(timeLabel, LV_ALIGN_TOP_RIGHT, 0, 8);

    // Device name if available
    if (!notification.deviceName.empty()) {
        lv_obj_t* deviceLabel = lv_label_create(item);
        lv_label_set_text(deviceLabel, notification.deviceName.c_str());
        lv_obj_set_style_text_color(deviceLabel, m_theme.primary(), 0);
        lv_obj_set_style_text_font(deviceLabel, &lv_font_montserrat_12, 0);
        lv_obj_align(deviceLabel, LV_ALIGN_BOTTOM_RIGHT, 0, -8);
    }

    return item;
}

const char* NotificationScreen::getNotificationIcon(NotificationType type) {
    switch (type) {
        case NotificationType::Alert:
        case NotificationType::Warning:
            return LV_SYMBOL_WARNING;
        case NotificationType::Error:
            return LV_SYMBOL_CLOSE;
        case NotificationType::Motion:
            return LV_SYMBOL_EYE_OPEN;
        case NotificationType::Door:
            return LV_SYMBOL_HOME;
        case NotificationType::Sensor:
            return LV_SYMBOL_CHARGE;
        case NotificationType::Info:
        default:
            return LV_SYMBOL_BELL;
    }
}

std::string NotificationScreen::formatTimestamp(const std::chrono::system_clock::time_point& time) {
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::minutes>(now - time).count();

    if (diff < 1) {
        return "Just now";
    } else if (diff < 60) {
        return std::to_string(diff) + "m ago";
    } else if (diff < 1440) {
        return std::to_string(diff / 60) + "h ago";
    } else {
        auto t = std::chrono::system_clock::to_time_t(time);
        std::tm* tm = std::localtime(&t);
        std::ostringstream oss;
        oss << std::put_time(tm, "%b %d");
        return oss.str();
    }
}

void NotificationScreen::backButtonHandler(lv_event_t* e) {
    NotificationScreen* self = static_cast<NotificationScreen*>(lv_event_get_user_data(e));
    if (self) {
        self->m_screenManager.goBack();
    }
}

void NotificationScreen::clearButtonHandler(lv_event_t* e) {
    NotificationScreen* self = static_cast<NotificationScreen*>(lv_event_get_user_data(e));
    if (self) {
        self->clearAll();
        self->updateNotificationList();
    }
}

void NotificationScreen::notificationItemHandler(lv_event_t* e) {
    (void)e;
    // Could navigate to device or show details
}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
