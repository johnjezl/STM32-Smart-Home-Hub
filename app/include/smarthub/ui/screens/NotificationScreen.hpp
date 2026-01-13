/**
 * Notification Screen
 *
 * Displays system notifications and alerts from devices.
 */
#pragma once

#include "smarthub/ui/Screen.hpp"
#include <vector>
#include <string>
#include <chrono>
#include <functional>

#ifdef SMARTHUB_ENABLE_LVGL
#include <lvgl.h>
#endif

namespace smarthub {

class DeviceManager;

namespace ui {

class ThemeManager;

/**
 * Notification type
 */
enum class NotificationType {
    Info,       // General information
    Alert,      // Attention needed
    Warning,    // Potential issue
    Error,      // Problem occurred
    Motion,     // Motion detected
    Door,       // Door/window opened/closed
    Sensor      // Sensor reading alert
};

/**
 * Single notification entry
 */
struct Notification {
    std::string id;
    NotificationType type;
    std::string title;
    std::string message;
    std::string deviceId;
    std::string deviceName;
    std::chrono::system_clock::time_point timestamp;
    bool read = false;
};

/**
 * Notification Screen - displays list of notifications
 */
class NotificationScreen : public Screen {
public:
    using ClearCallback = std::function<void()>;

    NotificationScreen(ScreenManager& screenManager,
                       ThemeManager& theme,
                       DeviceManager& deviceManager);
    ~NotificationScreen() override;

    void onCreate() override;
    void onShow() override;
    void onHide() override;
    void onUpdate(uint32_t deltaMs) override;
    void onDestroy() override;

    /**
     * Add a notification
     */
    void addNotification(const Notification& notification);

    /**
     * Get unread count
     */
    int getUnreadCount() const;

    /**
     * Mark all as read
     */
    void markAllRead();

    /**
     * Clear all notifications
     */
    void clearAll();

    /**
     * Set callback for when notifications are cleared
     */
    void onNotificationsCleared(ClearCallback callback);

private:
#ifdef SMARTHUB_ENABLE_LVGL
    void createHeader();
    void createNotificationList();
    void updateNotificationList();
    void refreshBadge();

    lv_obj_t* createNotificationItem(lv_obj_t* parent, const Notification& notification);
    const char* getNotificationIcon(NotificationType type);
    std::string formatTimestamp(const std::chrono::system_clock::time_point& time);

    static void backButtonHandler(lv_event_t* e);
    static void clearButtonHandler(lv_event_t* e);
    static void notificationItemHandler(lv_event_t* e);

    lv_obj_t* m_backBtn = nullptr;
    lv_obj_t* m_titleLabel = nullptr;
    lv_obj_t* m_clearBtn = nullptr;
    lv_obj_t* m_notificationList = nullptr;
    lv_obj_t* m_emptyLabel = nullptr;

    std::vector<lv_obj_t*> m_notificationItems;
#endif

    ThemeManager& m_theme;
    DeviceManager& m_deviceManager;
    std::vector<Notification> m_notifications;
    ClearCallback m_onCleared;

    static constexpr int MAX_NOTIFICATIONS = 50;
};

} // namespace ui
} // namespace smarthub
