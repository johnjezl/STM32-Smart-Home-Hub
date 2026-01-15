/**
 * Room Detail Screen Implementation
 */

#include "smarthub/ui/screens/RoomDetailScreen.hpp"
#include "smarthub/ui/screens/LightControlScreen.hpp"
#include "smarthub/ui/screens/AddDeviceScreen.hpp"
#include "smarthub/ui/screens/EditDeviceScreen.hpp"
#include "smarthub/ui/ScreenManager.hpp"
#include "smarthub/ui/ThemeManager.hpp"
#include "smarthub/ui/UIManager.hpp"
#include "smarthub/ui/widgets/Header.hpp"
#include "smarthub/devices/DeviceManager.hpp"
#include "smarthub/devices/Device.hpp"
#include "smarthub/core/Logger.hpp"

namespace smarthub {
namespace ui {

RoomDetailScreen::RoomDetailScreen(ScreenManager& screenManager,
                                   ThemeManager& theme,
                                   DeviceManager& deviceManager)
    : Screen(screenManager, "room_detail")
    , m_theme(theme)
    , m_deviceManager(deviceManager)
{
}

RoomDetailScreen::~RoomDetailScreen() = default;

void RoomDetailScreen::setRoom(const std::string& roomId, const std::string& roomName) {
    m_roomId = roomId;
    m_roomName = roomName;
}

void RoomDetailScreen::onCreate() {
#ifdef SMARTHUB_ENABLE_LVGL
    if (!m_container) return;

    lv_obj_set_style_bg_color(m_container, m_theme.background(), 0);
    createHeader();
    createContent();

    LOG_DEBUG("RoomDetailScreen created");
#endif
}

void RoomDetailScreen::onShow() {
#ifdef SMARTHUB_ENABLE_LVGL
    // Update title with room name
    if (m_titleLabel) {
        lv_label_set_text(m_titleLabel, m_roomName.c_str());
    }
    refreshDeviceList();
    LOG_DEBUG("RoomDetailScreen shown for room: %s", m_roomName.c_str());
#endif
}

void RoomDetailScreen::onHide() {
    LOG_DEBUG("RoomDetailScreen hidden");
}

void RoomDetailScreen::onUpdate(uint32_t /* deltaMs */) {
    // Could refresh device states periodically
}

void RoomDetailScreen::onDestroy() {
    LOG_DEBUG("RoomDetailScreen destroyed");
}

#ifdef SMARTHUB_ENABLE_LVGL

void RoomDetailScreen::createHeader() {
    m_header = lv_obj_create(m_container);
    lv_obj_set_size(m_header, LV_PCT(100), 50);
    lv_obj_align(m_header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(m_header, LV_OBJ_FLAG_SCROLLABLE);
    m_theme.applyHeaderStyle(m_header);

    // Back button
    m_backBtn = lv_btn_create(m_header);
    lv_obj_set_size(m_backBtn, 48, 48);
    lv_obj_align(m_backBtn, LV_ALIGN_LEFT_MID, ThemeManager::SPACING_SM, 0);
    lv_obj_set_style_bg_opa(m_backBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(m_backBtn, 0, 0);
    lv_obj_add_event_cb(m_backBtn, backButtonHandler, LV_EVENT_CLICKED, this);

    lv_obj_t* backIcon = lv_label_create(m_backBtn);
    lv_label_set_text(backIcon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(backIcon, m_theme.textPrimary(), 0);
    lv_obj_center(backIcon);

    // Title (room name)
    m_titleLabel = lv_label_create(m_header);
    lv_label_set_text(m_titleLabel, m_roomName.c_str());
    lv_obj_set_style_text_color(m_titleLabel, m_theme.textPrimary(), 0);
    lv_obj_set_style_text_font(m_titleLabel, &lv_font_montserrat_20, 0);
    lv_obj_align(m_titleLabel, LV_ALIGN_CENTER, 0, 0);

    // Add device button (right side, before edit)
    m_addDeviceBtn = lv_btn_create(m_header);
    lv_obj_set_size(m_addDeviceBtn, 48, 48);
    lv_obj_align(m_addDeviceBtn, LV_ALIGN_RIGHT_MID, -ThemeManager::SPACING_SM - 52, 0);
    lv_obj_set_style_bg_opa(m_addDeviceBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(m_addDeviceBtn, 0, 0);
    lv_obj_add_event_cb(m_addDeviceBtn, addDeviceButtonHandler, LV_EVENT_CLICKED, this);

    lv_obj_t* addIcon = lv_label_create(m_addDeviceBtn);
    lv_label_set_text(addIcon, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_color(addIcon, m_theme.textPrimary(), 0);
    lv_obj_center(addIcon);

    // Edit button (right side)
    m_editBtn = lv_btn_create(m_header);
    lv_obj_set_size(m_editBtn, 48, 48);
    lv_obj_align(m_editBtn, LV_ALIGN_RIGHT_MID, -ThemeManager::SPACING_SM, 0);
    lv_obj_set_style_bg_opa(m_editBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(m_editBtn, 0, 0);
    lv_obj_add_event_cb(m_editBtn, editButtonHandler, LV_EVENT_CLICKED, this);

    lv_obj_t* editIcon = lv_label_create(m_editBtn);
    lv_label_set_text(editIcon, LV_SYMBOL_EDIT);
    lv_obj_set_style_text_color(editIcon, m_theme.textPrimary(), 0);
    lv_obj_center(editIcon);
}

void RoomDetailScreen::createContent() {
    // Content area
    constexpr int HEADER_HEIGHT = 50;
    constexpr int BOTTOM_MARGIN = 8;  // Small margin to prevent display edge clipping
    constexpr int CONTENT_HEIGHT = 480 - HEADER_HEIGHT - BOTTOM_MARGIN;

    m_content = lv_obj_create(m_container);
    lv_obj_set_size(m_content, LV_PCT(100), CONTENT_HEIGHT);
    lv_obj_align(m_content, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT);
    lv_obj_set_style_bg_opa(m_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_content, 0, 0);
    lv_obj_set_style_pad_all(m_content, ThemeManager::SPACING_MD, 0);
    lv_obj_set_scroll_dir(m_content, LV_DIR_VER);

    // Device list container
    m_deviceList = lv_obj_create(m_content);
    lv_obj_set_size(m_deviceList, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(m_deviceList, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_deviceList, 0, 0);
    lv_obj_set_style_pad_all(m_deviceList, 0, 0);
    lv_obj_set_flex_flow(m_deviceList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_deviceList, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(m_deviceList, ThemeManager::SPACING_SM, 0);

    // Empty state label (hidden by default)
    m_emptyLabel = lv_label_create(m_content);
    lv_label_set_text(m_emptyLabel, "No devices in this room");
    lv_obj_set_style_text_color(m_emptyLabel, m_theme.textSecondary(), 0);
    lv_obj_align(m_emptyLabel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(m_emptyLabel, LV_OBJ_FLAG_HIDDEN);
}

void RoomDetailScreen::refreshDeviceList() {
    if (!m_deviceList) return;

    // Clear existing items
    lv_obj_clean(m_deviceList);

    // Get devices in this room
    auto devices = m_deviceManager.getDevicesByRoom(m_roomId);

    if (devices.empty()) {
        lv_obj_add_flag(m_deviceList, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(m_emptyLabel, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(m_deviceList, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(m_emptyLabel, LV_OBJ_FLAG_HIDDEN);

    for (const auto& device : devices) {
        if (!device) continue;

        std::string typeStr;
        switch (device->type()) {
            case DeviceType::Switch: typeStr = "Switch"; break;
            case DeviceType::Dimmer: typeStr = "Dimmer"; break;
            case DeviceType::ColorLight: typeStr = "Color Light"; break;
            case DeviceType::TemperatureSensor: typeStr = "Temperature"; break;
            case DeviceType::HumiditySensor: typeStr = "Humidity"; break;
            case DeviceType::MotionSensor: typeStr = "Motion"; break;
            case DeviceType::ContactSensor: typeStr = "Contact"; break;
            default: typeStr = "Device"; break;
        }

        bool isOn = false;
        auto state = device->getState();
        if (state.contains("on")) {
            isOn = state["on"].get<bool>();
        }

        createDeviceRow(m_deviceList, device->id(), device->name(), typeStr, isOn);
    }
}

lv_obj_t* RoomDetailScreen::createDeviceRow(lv_obj_t* parent,
                                             const std::string& deviceId,
                                             const std::string& name,
                                             const std::string& type,
                                             bool isOn) {
    // Row container
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), 60);
    lv_obj_set_style_bg_color(row, m_theme.surface(), 0);
    lv_obj_set_style_radius(row, ThemeManager::CARD_RADIUS, 0);
    lv_obj_set_style_pad_hor(row, ThemeManager::SPACING_MD, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    // Store device ID
    char* idCopy = new char[deviceId.size() + 1];
    strcpy(idCopy, deviceId.c_str());
    lv_obj_set_user_data(row, idCopy);
    lv_obj_add_event_cb(row, [](lv_event_t* e) {
        delete[] static_cast<char*>(lv_obj_get_user_data(lv_event_get_target(e)));
    }, LV_EVENT_DELETE, nullptr);

    // Make row clickable
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, rowClickHandler, LV_EVENT_CLICKED, this);

    // Icon
    lv_obj_t* icon = lv_label_create(row);
    lv_label_set_text(icon, LV_SYMBOL_POWER);
    lv_obj_set_style_text_color(icon, isOn ? m_theme.warning() : m_theme.textSecondary(), 0);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);

    // Device name
    lv_obj_t* nameLabel = lv_label_create(row);
    lv_label_set_text(nameLabel, name.c_str());
    lv_obj_set_style_text_color(nameLabel, m_theme.textPrimary(), 0);
    lv_obj_align(nameLabel, LV_ALIGN_LEFT_MID, 40, -8);

    // Device type
    lv_obj_t* typeLabel = lv_label_create(row);
    lv_label_set_text(typeLabel, type.c_str());
    lv_obj_set_style_text_color(typeLabel, m_theme.textSecondary(), 0);
    lv_obj_set_style_text_font(typeLabel, &lv_font_montserrat_12, 0);
    lv_obj_align(typeLabel, LV_ALIGN_LEFT_MID, 40, 10);

    // Power toggle
    lv_obj_t* toggle = lv_switch_create(row);
    lv_obj_align(toggle, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_size(toggle, 50, 26);

    if (isOn) {
        lv_obj_add_state(toggle, LV_STATE_CHECKED);
    }

    // Style the toggle
    lv_obj_set_style_bg_color(toggle, m_theme.surface(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(toggle, m_theme.primary(), LV_PART_INDICATOR | LV_STATE_CHECKED);

    // Store device ID for toggle
    char* toggleIdCopy = new char[deviceId.size() + 1];
    strcpy(toggleIdCopy, deviceId.c_str());
    lv_obj_set_user_data(toggle, toggleIdCopy);
    lv_obj_add_event_cb(toggle, [](lv_event_t* e) {
        delete[] static_cast<char*>(lv_obj_get_user_data(lv_event_get_target(e)));
    }, LV_EVENT_DELETE, nullptr);
    lv_obj_add_event_cb(toggle, toggleHandler, LV_EVENT_VALUE_CHANGED, this);

    return row;
}

void RoomDetailScreen::onDeviceToggle(const std::string& deviceId, bool newState) {
    auto device = m_deviceManager.getDevice(deviceId);
    if (device) {
        device->setState("on", newState);
    }
}

void RoomDetailScreen::onDeviceClicked(const std::string& deviceId) {
    LOG_DEBUG("Device clicked in room: %s", deviceId.c_str());

    auto device = m_deviceManager.getDevice(deviceId);
    if (!device) return;

    switch (device->type()) {
        case DeviceType::Dimmer:
        case DeviceType::ColorLight: {
            auto* lightScreen = dynamic_cast<LightControlScreen*>(
                m_screenManager.getScreen("lights"));
            if (lightScreen) {
                lightScreen->setDeviceId(deviceId);
                m_screenManager.showScreen("lights");
            }
            break;
        }
        default:
            break;
    }
}

void RoomDetailScreen::onBackClicked() {
    m_screenManager.goBack();
}

void RoomDetailScreen::onEditRoom() {
    LOG_DEBUG("Edit room: %s", m_roomName.c_str());

    // Create modal focus group for keyboard navigation containment
    lv_group_t* modalGroup = pushModalFocusGroup();

    // Create modal background overlay to block clicks on background
    lv_obj_t* overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);  // Absorb clicks

    // Create the dialog centered
    lv_obj_t* modal = lv_obj_create(overlay);
    lv_obj_set_size(modal, 420, 220);
    lv_obj_align(modal, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(modal, m_theme.surface(), 0);
    lv_obj_set_style_bg_opa(modal, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(modal, ThemeManager::CARD_RADIUS, 0);
    lv_obj_set_style_shadow_width(modal, 20, 0);
    lv_obj_set_style_shadow_opa(modal, LV_OPA_30, 0);
    lv_obj_set_style_pad_all(modal, 20, 0);
    lv_obj_set_style_border_width(modal, 0, 0);
    lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* title = lv_label_create(modal);
    lv_label_set_text(title, "Edit Room");
    lv_obj_set_style_text_color(title, m_theme.textPrimary(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    // Text input for room name (use USB keyboard)
    lv_obj_t* textarea = lv_textarea_create(modal);
    lv_obj_set_size(textarea, LV_PCT(100), 44);
    lv_obj_align(textarea, LV_ALIGN_TOP_MID, 0, 40);
    lv_textarea_set_text(textarea, m_roomName.c_str());
    lv_textarea_set_one_line(textarea, true);
    lv_obj_set_style_bg_color(textarea, m_theme.background(), 0);
    lv_obj_set_style_text_color(textarea, m_theme.textPrimary(), 0);
    lv_obj_set_style_border_color(textarea, m_theme.primary(), LV_STATE_FOCUSED);

    // Button row - use flex layout for proper spacing
    lv_obj_t* btnRow = lv_obj_create(modal);
    lv_obj_set_size(btnRow, LV_PCT(100), 50);
    lv_obj_align(btnRow, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(btnRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_style_pad_all(btnRow, 0, 0);
    lv_obj_clear_flag(btnRow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Delete button (red)
    lv_obj_t* deleteBtn = lv_btn_create(btnRow);
    lv_obj_set_size(deleteBtn, 110, 40);
    lv_obj_set_style_bg_color(deleteBtn, m_theme.error(), 0);

    lv_obj_t* deleteLabel = lv_label_create(deleteBtn);
    lv_label_set_text(deleteLabel, "Delete");
    lv_obj_set_style_text_color(deleteLabel, lv_color_white(), 0);
    lv_obj_center(deleteLabel);

    // Cancel button
    lv_obj_t* cancelBtn = lv_btn_create(btnRow);
    lv_obj_set_size(cancelBtn, 110, 40);
    lv_obj_set_style_bg_color(cancelBtn, m_theme.surface(), 0);
    lv_obj_set_style_border_width(cancelBtn, 1, 0);
    lv_obj_set_style_border_color(cancelBtn, m_theme.textSecondary(), 0);

    lv_obj_t* cancelLabel = lv_label_create(cancelBtn);
    lv_label_set_text(cancelLabel, "Cancel");
    lv_obj_set_style_text_color(cancelLabel, m_theme.textPrimary(), 0);
    lv_obj_center(cancelLabel);

    // Save button
    lv_obj_t* saveBtn = lv_btn_create(btnRow);
    lv_obj_set_size(saveBtn, 110, 40);
    lv_obj_set_style_bg_color(saveBtn, m_theme.primary(), 0);

    lv_obj_t* saveLabel = lv_label_create(saveBtn);
    lv_label_set_text(saveLabel, "Save");
    lv_obj_set_style_text_color(saveLabel, lv_color_white(), 0);
    lv_obj_center(saveLabel);

    // Add interactive widgets to modal group for keyboard navigation
    if (modalGroup) {
        lv_group_add_obj(modalGroup, textarea);
        lv_group_add_obj(modalGroup, deleteBtn);
        lv_group_add_obj(modalGroup, cancelBtn);
        lv_group_add_obj(modalGroup, saveBtn);
    }

    // Store context for handlers
    struct EditRoomCtx {
        RoomDetailScreen* screen;
        lv_obj_t* textarea;
        lv_obj_t* overlay;
        std::string roomId;
    };
    auto* ctx = new EditRoomCtx{this, textarea, overlay, m_roomId};
    lv_obj_set_user_data(saveBtn, ctx);
    lv_obj_set_user_data(deleteBtn, ctx);
    lv_obj_set_user_data(cancelBtn, ctx);

    // Cancel handler
    lv_obj_add_event_cb(cancelBtn, [](lv_event_t* e) {
        lv_obj_t* btn = lv_event_get_target(e);
        auto* ctx = static_cast<EditRoomCtx*>(lv_obj_get_user_data(btn));
        if (!ctx) return;
        popModalFocusGroup();  // Restore focus to main group
        lv_obj_del(ctx->overlay);
        delete ctx;
    }, LV_EVENT_CLICKED, nullptr);

    // Save handler
    lv_obj_add_event_cb(saveBtn, [](lv_event_t* e) {
        lv_obj_t* btn = lv_event_get_target(e);
        auto* ctx = static_cast<EditRoomCtx*>(lv_obj_get_user_data(btn));
        if (!ctx) return;

        const char* text = lv_textarea_get_text(ctx->textarea);
        if (text && strlen(text) > 0) {
            std::string newName(text);
            ctx->screen->m_deviceManager.updateRoom(ctx->roomId, newName);
            ctx->screen->m_roomName = newName;

            if (ctx->screen->m_titleLabel) {
                lv_label_set_text(ctx->screen->m_titleLabel, newName.c_str());
            }
        }

        popModalFocusGroup();  // Restore focus to main group
        lv_obj_del(ctx->overlay);
        delete ctx;
    }, LV_EVENT_CLICKED, nullptr);

    // Delete handler - show confirmation
    lv_obj_add_event_cb(deleteBtn, [](lv_event_t* e) {
        lv_obj_t* btn = lv_event_get_target(e);
        auto* ctx = static_cast<EditRoomCtx*>(lv_obj_get_user_data(btn));
        if (!ctx) return;

        // Create nested modal focus group for confirmation dialog
        lv_group_t* confirmGroup = pushModalFocusGroup();

        // Create confirmation dialog overlay
        lv_obj_t* confirmOverlay = lv_obj_create(lv_layer_top());
        lv_obj_set_size(confirmOverlay, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(confirmOverlay, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(confirmOverlay, LV_OPA_70, 0);
        lv_obj_set_style_border_width(confirmOverlay, 0, 0);
        lv_obj_clear_flag(confirmOverlay, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(confirmOverlay, LV_OBJ_FLAG_CLICKABLE);

        // Confirmation dialog centered
        lv_obj_t* confirmModal = lv_obj_create(confirmOverlay);
        lv_obj_set_size(confirmModal, 380, 200);
        lv_obj_align(confirmModal, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(confirmModal, ctx->screen->m_theme.surface(), 0);
        lv_obj_set_style_bg_opa(confirmModal, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(confirmModal, ThemeManager::CARD_RADIUS, 0);
        lv_obj_set_style_shadow_width(confirmModal, 20, 0);
        lv_obj_set_style_shadow_opa(confirmModal, LV_OPA_50, 0);
        lv_obj_set_style_pad_all(confirmModal, 24, 0);
        lv_obj_set_style_border_width(confirmModal, 0, 0);
        lv_obj_clear_flag(confirmModal, LV_OBJ_FLAG_SCROLLABLE);

        // Title
        lv_obj_t* confirmTitle = lv_label_create(confirmModal);
        lv_label_set_text(confirmTitle, "Delete Room?");
        lv_obj_set_style_text_color(confirmTitle, ctx->screen->m_theme.textPrimary(), 0);
        lv_obj_set_style_text_font(confirmTitle, &lv_font_montserrat_20, 0);
        lv_obj_align(confirmTitle, LV_ALIGN_TOP_MID, 0, 0);

        // Message
        lv_obj_t* message = lv_label_create(confirmModal);
        lv_label_set_text(message, "This will remove the room.\nDevices will be unassigned.");
        lv_obj_set_style_text_color(message, ctx->screen->m_theme.textSecondary(), 0);
        lv_obj_set_style_text_align(message, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(message, LV_ALIGN_CENTER, 0, -10);

        // Cancel button
        lv_obj_t* cancelConfirmBtn = lv_btn_create(confirmModal);
        lv_obj_set_size(cancelConfirmBtn, 130, 44);
        lv_obj_align(cancelConfirmBtn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_set_style_bg_color(cancelConfirmBtn, ctx->screen->m_theme.surface(), 0);
        lv_obj_set_style_border_width(cancelConfirmBtn, 1, 0);
        lv_obj_set_style_border_color(cancelConfirmBtn, ctx->screen->m_theme.textSecondary(), 0);

        lv_obj_t* cancelConfirmLabel = lv_label_create(cancelConfirmBtn);
        lv_label_set_text(cancelConfirmLabel, "Cancel");
        lv_obj_set_style_text_color(cancelConfirmLabel, ctx->screen->m_theme.textPrimary(), 0);
        lv_obj_center(cancelConfirmLabel);

        // Add cancel button to confirm group for keyboard navigation
        if (confirmGroup) {
            lv_group_add_obj(confirmGroup, cancelConfirmBtn);
        }

        lv_obj_set_user_data(cancelConfirmBtn, confirmOverlay);
        lv_obj_add_event_cb(cancelConfirmBtn, [](lv_event_t* ce) {
            lv_obj_t* overlay = static_cast<lv_obj_t*>(lv_obj_get_user_data(lv_event_get_target(ce)));
            popModalFocusGroup();  // Pop confirmation group, restore to edit modal group
            if (overlay) lv_obj_del(overlay);
        }, LV_EVENT_CLICKED, nullptr);

        // Confirm delete button
        lv_obj_t* confirmDeleteBtn = lv_btn_create(confirmModal);
        lv_obj_set_size(confirmDeleteBtn, 130, 44);
        lv_obj_align(confirmDeleteBtn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        lv_obj_set_style_bg_color(confirmDeleteBtn, ctx->screen->m_theme.error(), 0);

        lv_obj_t* confirmDeleteLabel = lv_label_create(confirmDeleteBtn);
        lv_label_set_text(confirmDeleteLabel, "Delete");
        lv_obj_set_style_text_color(confirmDeleteLabel, lv_color_white(), 0);
        lv_obj_center(confirmDeleteLabel);

        // Add delete button to confirm group for keyboard navigation
        if (confirmGroup) {
            lv_group_add_obj(confirmGroup, confirmDeleteBtn);
        }

        // Context for confirm button
        struct ConfirmDeleteCtx {
            RoomDetailScreen* screen;
            lv_obj_t* editOverlay;
            lv_obj_t* confirmOverlay;
            std::string roomId;
        };
        auto* confirmCtx = new ConfirmDeleteCtx{ctx->screen, ctx->overlay, confirmOverlay, ctx->roomId};
        lv_obj_set_user_data(confirmDeleteBtn, confirmCtx);

        lv_obj_add_event_cb(confirmDeleteBtn, [](lv_event_t* ce) {
            auto* cctx = static_cast<ConfirmDeleteCtx*>(lv_obj_get_user_data(lv_event_get_target(ce)));
            if (!cctx) return;

            cctx->screen->m_deviceManager.deleteRoom(cctx->roomId);

            // Pop both modal groups (confirmation + edit room)
            popModalFocusGroup();  // Pop confirmation group
            popModalFocusGroup();  // Pop edit room group

            lv_obj_del(cctx->confirmOverlay);
            lv_obj_del(cctx->editOverlay);

            cctx->screen->m_screenManager.goBack();

            delete cctx;
        }, LV_EVENT_CLICKED, nullptr);
    }, LV_EVENT_CLICKED, nullptr);
}

void RoomDetailScreen::toggleHandler(lv_event_t* e) {
    RoomDetailScreen* self = static_cast<RoomDetailScreen*>(lv_event_get_user_data(e));
    lv_obj_t* toggle = lv_event_get_target(e);
    const char* deviceId = static_cast<const char*>(lv_obj_get_user_data(toggle));
    bool newState = lv_obj_has_state(toggle, LV_STATE_CHECKED);
    if (self && deviceId) {
        self->onDeviceToggle(deviceId, newState);
    }
}

void RoomDetailScreen::rowClickHandler(lv_event_t* e) {
    RoomDetailScreen* self = static_cast<RoomDetailScreen*>(lv_event_get_user_data(e));
    lv_obj_t* row = lv_event_get_target(e);
    const char* deviceId = static_cast<const char*>(lv_obj_get_user_data(row));
    if (self && deviceId) {
        self->onDeviceClicked(deviceId);
    }
}

void RoomDetailScreen::backButtonHandler(lv_event_t* e) {
    RoomDetailScreen* self = static_cast<RoomDetailScreen*>(lv_event_get_user_data(e));
    if (self) {
        self->onBackClicked();
    }
}

void RoomDetailScreen::editButtonHandler(lv_event_t* e) {
    RoomDetailScreen* self = static_cast<RoomDetailScreen*>(lv_event_get_user_data(e));
    if (self) {
        self->onEditRoom();
    }
}

void RoomDetailScreen::addDeviceButtonHandler(lv_event_t* e) {
    RoomDetailScreen* self = static_cast<RoomDetailScreen*>(lv_event_get_user_data(e));
    if (self) {
        self->onAddDeviceClicked();
    }
}

void RoomDetailScreen::onAddDeviceClicked() {
    LOG_DEBUG("Add device to room: %s", m_roomName.c_str());

    auto* addScreen = dynamic_cast<AddDeviceScreen*>(
        m_screenManager.getScreen("add_device"));
    if (addScreen) {
        addScreen->setPreselectedRoom(m_roomId, m_roomName);
        addScreen->onDeviceAdded([this](DevicePtr /* device */) {
            refreshDeviceList();
        });
        m_screenManager.showScreen("add_device");
    }
}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
