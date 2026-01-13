/**
 * Room Detail Screen
 *
 * Shows devices in a specific room with controls.
 */
#pragma once

#include "smarthub/ui/Screen.hpp"
#include <memory>
#include <vector>
#include <string>

namespace smarthub {

class DeviceManager;

namespace ui {

class ThemeManager;

/**
 * Room detail screen showing devices in a single room
 */
class RoomDetailScreen : public Screen {
public:
    RoomDetailScreen(ScreenManager& screenManager,
                     ThemeManager& theme,
                     DeviceManager& deviceManager);
    ~RoomDetailScreen() override;

    /**
     * Set the room to display (call before showing screen)
     */
    void setRoom(const std::string& roomId, const std::string& roomName);

    void onCreate() override;
    void onShow() override;
    void onHide() override;
    void onUpdate(uint32_t deltaMs) override;
    void onDestroy() override;

private:
#ifdef SMARTHUB_ENABLE_LVGL
    void createHeader();
    void createContent();
    void refreshDeviceList();

    lv_obj_t* createDeviceRow(lv_obj_t* parent, const std::string& deviceId,
                               const std::string& name, const std::string& type,
                               bool isOn);

    void onDeviceToggle(const std::string& deviceId, bool newState);
    void onDeviceClicked(const std::string& deviceId);
    void onBackClicked();
    void onEditRoom();
    void onAddDeviceClicked();

    static void toggleHandler(lv_event_t* e);
    static void rowClickHandler(lv_event_t* e);
    static void backButtonHandler(lv_event_t* e);
    static void editButtonHandler(lv_event_t* e);
    static void addDeviceButtonHandler(lv_event_t* e);

    lv_obj_t* m_header = nullptr;
    lv_obj_t* m_backBtn = nullptr;
    lv_obj_t* m_addDeviceBtn = nullptr;
    lv_obj_t* m_editBtn = nullptr;
    lv_obj_t* m_titleLabel = nullptr;
    lv_obj_t* m_content = nullptr;
    lv_obj_t* m_deviceList = nullptr;
    lv_obj_t* m_emptyLabel = nullptr;
#endif

    std::string m_roomId;
    std::string m_roomName;
    ThemeManager& m_theme;
    DeviceManager& m_deviceManager;
};

} // namespace ui
} // namespace smarthub
