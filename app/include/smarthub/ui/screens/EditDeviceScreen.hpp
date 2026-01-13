/**
 * Edit Device Screen
 *
 * Screen for editing existing device properties and deleting devices.
 */
#pragma once

#include "smarthub/ui/Screen.hpp"
#include "smarthub/devices/IDevice.hpp"
#include <string>
#include <vector>
#include <functional>

namespace smarthub {

class DeviceManager;

namespace ui {

class ThemeManager;

/**
 * Edit device screen
 *
 * Allows editing:
 * - Device name
 * - Room assignment
 *
 * Read-only display:
 * - Device type
 * - Protocol and address
 *
 * Actions:
 * - Save changes
 * - Delete device
 */
class EditDeviceScreen : public Screen {
public:
    using DeviceUpdatedCallback = std::function<void()>;
    using DeviceDeletedCallback = std::function<void()>;

    EditDeviceScreen(ScreenManager& screenManager,
                     ThemeManager& theme,
                     DeviceManager& deviceManager);
    ~EditDeviceScreen() override;

    /**
     * Set the device to edit
     */
    void setDeviceId(const std::string& deviceId);

    /**
     * Set callbacks
     */
    void onDeviceUpdated(DeviceUpdatedCallback callback);
    void onDeviceDeleted(DeviceDeletedCallback callback);

    void onCreate() override;
    void onShow() override;
    void onHide() override;
    void onUpdate(uint32_t deltaMs) override;
    void onDestroy() override;

private:
#ifdef SMARTHUB_ENABLE_LVGL
    void createHeader();
    void createContent();
    void populateFields();

    void onBackClicked();
    void onSaveClicked();
    void onDeleteClicked();
    void showDeleteConfirmation();

    static void backButtonHandler(lv_event_t* e);
    static void saveButtonHandler(lv_event_t* e);
    static void deleteButtonHandler(lv_event_t* e);
    static void roomDropdownHandler(lv_event_t* e);

    // UI elements
    lv_obj_t* m_header = nullptr;
    lv_obj_t* m_backBtn = nullptr;
    lv_obj_t* m_content = nullptr;

    // Editable fields
    lv_obj_t* m_nameInput = nullptr;
    lv_obj_t* m_roomDropdown = nullptr;

    // Read-only labels
    lv_obj_t* m_typeLabel = nullptr;
    lv_obj_t* m_protocolLabel = nullptr;
    lv_obj_t* m_addressLabel = nullptr;

    // Buttons
    lv_obj_t* m_saveBtn = nullptr;
    lv_obj_t* m_deleteBtn = nullptr;

    // Keyboard
    lv_obj_t* m_keyboard = nullptr;
#endif

    std::string m_deviceId;
    std::vector<std::pair<std::string, std::string>> m_rooms;  // id, name

    ThemeManager& m_theme;
    DeviceManager& m_deviceManager;
    DeviceUpdatedCallback m_onDeviceUpdated;
    DeviceDeletedCallback m_onDeviceDeleted;
};

} // namespace ui
} // namespace smarthub
