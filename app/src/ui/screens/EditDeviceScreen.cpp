/**
 * Edit Device Screen Implementation
 */

#include "smarthub/ui/screens/EditDeviceScreen.hpp"
#include "smarthub/ui/ScreenManager.hpp"
#include "smarthub/ui/ThemeManager.hpp"
#include "smarthub/ui/UIManager.hpp"
#include "smarthub/devices/DeviceManager.hpp"
#include "smarthub/devices/Device.hpp"
#include "smarthub/core/Logger.hpp"

namespace smarthub {
namespace ui {

EditDeviceScreen::EditDeviceScreen(ScreenManager& screenManager,
                                   ThemeManager& theme,
                                   DeviceManager& deviceManager)
    : Screen(screenManager, "edit_device")
    , m_theme(theme)
    , m_deviceManager(deviceManager)
{
}

EditDeviceScreen::~EditDeviceScreen() = default;

void EditDeviceScreen::setDeviceId(const std::string& deviceId) {
    m_deviceId = deviceId;
}

void EditDeviceScreen::onDeviceUpdated(DeviceUpdatedCallback callback) {
    m_onDeviceUpdated = std::move(callback);
}

void EditDeviceScreen::onDeviceDeleted(DeviceDeletedCallback callback) {
    m_onDeviceDeleted = std::move(callback);
}

void EditDeviceScreen::onCreate() {
#ifdef SMARTHUB_ENABLE_LVGL
    if (!m_container) return;

    lv_obj_set_style_bg_color(m_container, m_theme.background(), 0);

    createHeader();
    createContent();

    LOG_DEBUG("EditDeviceScreen created");
#endif
}

void EditDeviceScreen::onShow() {
#ifdef SMARTHUB_ENABLE_LVGL
    // Load rooms
    m_rooms = m_deviceManager.getAllRooms();

    populateFields();
    LOG_DEBUG("EditDeviceScreen shown for device: %s", m_deviceId.c_str());
#endif
}

void EditDeviceScreen::onHide() {
#ifdef SMARTHUB_ENABLE_LVGL
    if (m_keyboard) {
        lv_obj_add_flag(m_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
#endif
    LOG_DEBUG("EditDeviceScreen hidden");
}

void EditDeviceScreen::onUpdate(uint32_t /* deltaMs */) {
}

void EditDeviceScreen::onDestroy() {
    LOG_DEBUG("EditDeviceScreen destroyed");
}

#ifdef SMARTHUB_ENABLE_LVGL

void EditDeviceScreen::createHeader() {
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

    // Title
    lv_obj_t* title = lv_label_create(m_header);
    lv_label_set_text(title, "Edit Device");
    lv_obj_set_style_text_color(title, m_theme.textPrimary(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
}

void EditDeviceScreen::createContent() {
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

    // Device name (editable)
    lv_obj_t* nameLabel = lv_label_create(m_content);
    lv_label_set_text(nameLabel, "Device name:");
    lv_obj_set_style_text_color(nameLabel, m_theme.textPrimary(), 0);
    lv_obj_align(nameLabel, LV_ALIGN_TOP_LEFT, 0, 0);

    m_nameInput = lv_textarea_create(m_content);
    lv_obj_set_size(m_nameInput, LV_PCT(100), 50);
    lv_obj_align(m_nameInput, LV_ALIGN_TOP_LEFT, 0, 25);
    lv_textarea_set_one_line(m_nameInput, true);
    lv_obj_set_style_bg_color(m_nameInput, m_theme.surface(), 0);
    lv_obj_set_style_text_color(m_nameInput, m_theme.textPrimary(), 0);
    // Disable scroll-on-focus to prevent screen shifting
    lv_obj_clear_flag(m_nameInput, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    // Add to keyboard input group
    if (lv_group_get_default()) {
        lv_group_add_obj(lv_group_get_default(), m_nameInput);
    }

    // Room selection (editable)
    lv_obj_t* roomLabel = lv_label_create(m_content);
    lv_label_set_text(roomLabel, "Room:");
    lv_obj_set_style_text_color(roomLabel, m_theme.textPrimary(), 0);
    lv_obj_align(roomLabel, LV_ALIGN_TOP_LEFT, 0, 85);

    m_roomDropdown = lv_dropdown_create(m_content);
    lv_obj_set_size(m_roomDropdown, LV_PCT(100), 50);
    lv_obj_align(m_roomDropdown, LV_ALIGN_TOP_LEFT, 0, 110);

    // Device type (read-only)
    lv_obj_t* typeTitle = lv_label_create(m_content);
    lv_label_set_text(typeTitle, "Type:");
    lv_obj_set_style_text_color(typeTitle, m_theme.textSecondary(), 0);
    lv_obj_align(typeTitle, LV_ALIGN_TOP_LEFT, 0, 175);

    m_typeLabel = lv_label_create(m_content);
    lv_obj_set_style_text_color(m_typeLabel, m_theme.textPrimary(), 0);
    lv_obj_align(m_typeLabel, LV_ALIGN_TOP_LEFT, 80, 175);

    // Protocol (read-only)
    lv_obj_t* protoTitle = lv_label_create(m_content);
    lv_label_set_text(protoTitle, "Protocol:");
    lv_obj_set_style_text_color(protoTitle, m_theme.textSecondary(), 0);
    lv_obj_align(protoTitle, LV_ALIGN_TOP_LEFT, 0, 200);

    m_protocolLabel = lv_label_create(m_content);
    lv_obj_set_style_text_color(m_protocolLabel, m_theme.textPrimary(), 0);
    lv_obj_align(m_protocolLabel, LV_ALIGN_TOP_LEFT, 80, 200);

    // Address (read-only)
    lv_obj_t* addrTitle = lv_label_create(m_content);
    lv_label_set_text(addrTitle, "Address:");
    lv_obj_set_style_text_color(addrTitle, m_theme.textSecondary(), 0);
    lv_obj_align(addrTitle, LV_ALIGN_TOP_LEFT, 0, 225);

    m_addressLabel = lv_label_create(m_content);
    lv_obj_set_style_text_color(m_addressLabel, m_theme.textPrimary(), 0);
    lv_obj_align(m_addressLabel, LV_ALIGN_TOP_LEFT, 80, 225);

    // Delete button (left)
    m_deleteBtn = lv_btn_create(m_content);
    lv_obj_set_size(m_deleteBtn, 140, 48);
    lv_obj_align(m_deleteBtn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(m_deleteBtn, m_theme.error(), 0);
    lv_obj_add_event_cb(m_deleteBtn, deleteButtonHandler, LV_EVENT_CLICKED, this);

    lv_obj_t* deleteLabel = lv_label_create(m_deleteBtn);
    lv_label_set_text(deleteLabel, LV_SYMBOL_TRASH " Delete");
    lv_obj_set_style_text_color(deleteLabel, lv_color_white(), 0);
    lv_obj_center(deleteLabel);

    // Save button (right)
    m_saveBtn = lv_btn_create(m_content);
    lv_obj_set_size(m_saveBtn, 140, 48);
    lv_obj_align(m_saveBtn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(m_saveBtn, m_theme.primary(), 0);
    lv_obj_add_event_cb(m_saveBtn, saveButtonHandler, LV_EVENT_CLICKED, this);

    lv_obj_t* saveLabel = lv_label_create(m_saveBtn);
    lv_label_set_text(saveLabel, LV_SYMBOL_SAVE " Save");
    lv_obj_set_style_text_color(saveLabel, lv_color_white(), 0);
    lv_obj_center(saveLabel);

    // Keyboard (hidden by default)
    m_keyboard = lv_keyboard_create(m_container);
    lv_obj_add_flag(m_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(m_keyboard, LV_PCT(100), 200);
    lv_obj_align(m_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Keyboard events for name input
    lv_obj_add_event_cb(m_nameInput, [](lv_event_t* e) {
        auto* self = static_cast<EditDeviceScreen*>(lv_event_get_user_data(e));
        lv_keyboard_set_textarea(self->m_keyboard, self->m_nameInput);
        lv_obj_clear_flag(self->m_keyboard, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_FOCUSED, this);

    lv_obj_add_event_cb(m_nameInput, [](lv_event_t* e) {
        auto* self = static_cast<EditDeviceScreen*>(lv_event_get_user_data(e));
        lv_obj_add_flag(self->m_keyboard, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_DEFOCUSED, this);
}

void EditDeviceScreen::populateFields() {
    auto device = m_deviceManager.getDevice(m_deviceId);
    if (!device) {
        LOG_ERROR("Device not found: %s", m_deviceId.c_str());
        m_screenManager.goBack();
        return;
    }

    // Name
    if (m_nameInput) {
        lv_textarea_set_text(m_nameInput, device->name().c_str());
    }

    // Type
    if (m_typeLabel) {
        lv_label_set_text(m_typeLabel, device->typeString().c_str());
    }

    // Protocol
    if (m_protocolLabel) {
        lv_label_set_text(m_protocolLabel, device->protocol().c_str());
    }

    // Address
    if (m_addressLabel) {
        const std::string& addr = device->protocolAddress();
        lv_label_set_text(m_addressLabel, addr.empty() ? "(none)" : addr.c_str());
    }

    // Room dropdown
    if (m_roomDropdown) {
        std::string options = "(No room)";
        int selectedIdx = 0;

        for (size_t i = 0; i < m_rooms.size(); i++) {
            options += "\n" + m_rooms[i].second;
            if (m_rooms[i].first == device->room()) {
                selectedIdx = static_cast<int>(i) + 1;
            }
        }

        lv_dropdown_set_options(m_roomDropdown, options.c_str());
        lv_dropdown_set_selected(m_roomDropdown, selectedIdx);
    }
}

void EditDeviceScreen::onBackClicked() {
    m_screenManager.goBack();
}

void EditDeviceScreen::onSaveClicked() {
    auto device = m_deviceManager.getDevice(m_deviceId);
    if (!device) {
        LOG_ERROR("Device not found: %s", m_deviceId.c_str());
        return;
    }

    // Update name
    if (m_nameInput) {
        const char* name = lv_textarea_get_text(m_nameInput);
        if (name && strlen(name) > 0) {
            device->setName(name);
        }
    }

    // Update room
    if (m_roomDropdown) {
        int idx = lv_dropdown_get_selected(m_roomDropdown);
        if (idx == 0) {
            device->setRoom("");
        } else if (idx > 0 && idx <= static_cast<int>(m_rooms.size())) {
            device->setRoom(m_rooms[idx - 1].first);
        }
    }

    // Save to database
    m_deviceManager.saveAllDevices();

    LOG_INFO("Device updated: %s", m_deviceId.c_str());

    if (m_onDeviceUpdated) {
        m_onDeviceUpdated();
    }

    m_screenManager.goBack();
}

void EditDeviceScreen::onDeleteClicked() {
    showDeleteConfirmation();
}

void EditDeviceScreen::showDeleteConfirmation() {
    // Create modal focus group for keyboard navigation containment
    lv_group_t* modalGroup = pushModalFocusGroup();

    // Create confirmation dialog
    lv_obj_t* modal = lv_obj_create(lv_layer_top());
    lv_obj_set_size(modal, 350, 180);
    lv_obj_center(modal);
    lv_obj_set_style_bg_color(modal, m_theme.surface(), 0);
    lv_obj_set_style_radius(modal, ThemeManager::CARD_RADIUS, 0);
    lv_obj_set_style_shadow_width(modal, 20, 0);
    lv_obj_set_style_shadow_opa(modal, LV_OPA_30, 0);
    lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* title = lv_label_create(modal);
    lv_label_set_text(title, "Delete Device?");
    lv_obj_set_style_text_color(title, m_theme.textPrimary(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    // Message
    lv_obj_t* msg = lv_label_create(modal);
    lv_label_set_text(msg, "This action cannot be undone.");
    lv_obj_set_style_text_color(msg, m_theme.textSecondary(), 0);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, -10);

    // Cancel button
    lv_obj_t* cancelBtn = lv_btn_create(modal);
    lv_obj_set_size(cancelBtn, 120, 48);
    lv_obj_align(cancelBtn, LV_ALIGN_BOTTOM_LEFT, 30, -20);
    lv_obj_set_style_bg_color(cancelBtn, m_theme.surface(), 0);
    lv_obj_set_style_border_width(cancelBtn, 1, 0);
    lv_obj_set_style_border_color(cancelBtn, m_theme.textSecondary(), 0);

    lv_obj_t* cancelLabel = lv_label_create(cancelBtn);
    lv_label_set_text(cancelLabel, "Cancel");
    lv_obj_set_style_text_color(cancelLabel, m_theme.textPrimary(), 0);
    lv_obj_center(cancelLabel);

    // Add cancel button to modal group for keyboard navigation
    if (modalGroup) {
        lv_group_add_obj(modalGroup, cancelBtn);
    }

    lv_obj_add_event_cb(cancelBtn, [](lv_event_t* e) {
        lv_obj_t* btn = lv_event_get_target(e);
        lv_obj_t* modal = lv_obj_get_parent(btn);
        popModalFocusGroup();  // Restore focus to main group
        lv_obj_del(modal);
    }, LV_EVENT_CLICKED, nullptr);

    // Delete button
    lv_obj_t* deleteBtn = lv_btn_create(modal);
    lv_obj_set_size(deleteBtn, 120, 48);
    lv_obj_align(deleteBtn, LV_ALIGN_BOTTOM_RIGHT, -30, -20);
    lv_obj_set_style_bg_color(deleteBtn, m_theme.error(), 0);

    lv_obj_t* deleteLabel = lv_label_create(deleteBtn);
    lv_label_set_text(deleteLabel, "Delete");
    lv_obj_set_style_text_color(deleteLabel, lv_color_white(), 0);
    lv_obj_center(deleteLabel);

    // Add delete button to modal group for keyboard navigation
    if (modalGroup) {
        lv_group_add_obj(modalGroup, deleteBtn);
    }

    // Store context for delete
    struct DeleteCtx {
        EditDeviceScreen* screen;
        lv_obj_t* modal;
    };
    auto* ctx = new DeleteCtx{this, modal};
    lv_obj_set_user_data(deleteBtn, ctx);

    lv_obj_add_event_cb(deleteBtn, [](lv_event_t* e) {
        lv_obj_t* btn = lv_event_get_target(e);
        auto* ctx = static_cast<DeleteCtx*>(lv_obj_get_user_data(btn));
        if (!ctx) return;

        // Delete the device
        ctx->screen->m_deviceManager.removeDevice(ctx->screen->m_deviceId);

        LOG_INFO("Device deleted: %s", ctx->screen->m_deviceId.c_str());

        if (ctx->screen->m_onDeviceDeleted) {
            ctx->screen->m_onDeviceDeleted();
        }

        popModalFocusGroup();  // Restore focus to main group
        lv_obj_del(ctx->modal);
        ctx->screen->m_screenManager.goBack();

        delete ctx;
    }, LV_EVENT_CLICKED, nullptr);
}

void EditDeviceScreen::backButtonHandler(lv_event_t* e) {
    auto* self = static_cast<EditDeviceScreen*>(lv_event_get_user_data(e));
    if (self) {
        self->onBackClicked();
    }
}

void EditDeviceScreen::saveButtonHandler(lv_event_t* e) {
    auto* self = static_cast<EditDeviceScreen*>(lv_event_get_user_data(e));
    if (self) {
        self->onSaveClicked();
    }
}

void EditDeviceScreen::deleteButtonHandler(lv_event_t* e) {
    auto* self = static_cast<EditDeviceScreen*>(lv_event_get_user_data(e));
    if (self) {
        self->onDeleteClicked();
    }
}

void EditDeviceScreen::roomDropdownHandler(lv_event_t* e) {
    (void)e;  // Selection read at save time
}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
