/**
 * Edit Automation Screen Implementation
 */

#include "smarthub/ui/screens/EditAutomationScreen.hpp"
#include "smarthub/ui/ScreenManager.hpp"
#include "smarthub/ui/ThemeManager.hpp"
#include "smarthub/ui/UIManager.hpp"
#include "smarthub/ui/widgets/Header.hpp"
#include "smarthub/automation/AutomationManager.hpp"
#include "smarthub/automation/Automation.hpp"
#include "smarthub/core/Logger.hpp"

namespace smarthub {
namespace ui {

EditAutomationScreen::EditAutomationScreen(ScreenManager& screenManager,
                                           ThemeManager& theme,
                                           AutomationManager& automationManager)
    : Screen(screenManager, "edit_automation")
    , m_theme(theme)
    , m_automationManager(automationManager)
{
}

EditAutomationScreen::~EditAutomationScreen() = default;

void EditAutomationScreen::setAutomationId(const std::string& id) {
    m_automationId = id;
}

void EditAutomationScreen::onAutomationUpdated(UpdatedCallback callback) {
    m_onUpdated = callback;
}

void EditAutomationScreen::onAutomationDeleted(DeletedCallback callback) {
    m_onDeleted = callback;
}

void EditAutomationScreen::onCreate() {
#ifdef SMARTHUB_ENABLE_LVGL
    if (!m_container) return;

    lv_obj_set_style_bg_color(m_container, m_theme.background(), 0);

    createHeader();
    createContent();

    LOG_DEBUG("EditAutomationScreen created");
#endif
}

void EditAutomationScreen::onShow() {
#ifdef SMARTHUB_ENABLE_LVGL
    populateFields();
    LOG_DEBUG("EditAutomationScreen shown for: %s", m_automationId.c_str());
#endif
}

void EditAutomationScreen::onHide() {
#ifdef SMARTHUB_ENABLE_LVGL
    LOG_DEBUG("EditAutomationScreen hidden");
#endif
}

void EditAutomationScreen::onUpdate(uint32_t deltaMs) {
    (void)deltaMs;
}

void EditAutomationScreen::onDestroy() {
#ifdef SMARTHUB_ENABLE_LVGL
    LOG_DEBUG("EditAutomationScreen destroyed");
#endif
}

#ifdef SMARTHUB_ENABLE_LVGL

void EditAutomationScreen::createHeader() {
    m_header = lv_obj_create(m_container);
    lv_obj_set_size(m_header, LV_PCT(100), Header::HEIGHT);
    lv_obj_align(m_header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(m_header, LV_OBJ_FLAG_SCROLLABLE);
    m_theme.applyHeaderStyle(m_header);

    // Back button
    lv_obj_t* backBtn = lv_btn_create(m_header);
    lv_obj_set_size(backBtn, 48, 48);
    lv_obj_align(backBtn, LV_ALIGN_LEFT_MID, ThemeManager::SPACING_SM, 0);
    lv_obj_set_style_bg_opa(backBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(backBtn, 0, 0);

    lv_obj_t* backIcon = lv_label_create(backBtn);
    lv_label_set_text(backIcon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(backIcon, m_theme.textPrimary(), 0);
    lv_obj_center(backIcon);

    lv_obj_add_event_cb(backBtn, [](lv_event_t* e) {
        auto* self = static_cast<EditAutomationScreen*>(lv_event_get_user_data(e));
        if (self) self->m_screenManager.goBack();
    }, LV_EVENT_CLICKED, this);

    // Title
    lv_obj_t* title = lv_label_create(m_header);
    lv_label_set_text(title, "Edit Automation");
    lv_obj_set_style_text_color(title, m_theme.textPrimary(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
}

void EditAutomationScreen::createContent() {
    constexpr int CONTENT_HEIGHT = 480 - Header::HEIGHT;

    m_content = lv_obj_create(m_container);
    lv_obj_set_size(m_content, LV_PCT(100), CONTENT_HEIGHT);
    lv_obj_align(m_content, LV_ALIGN_TOP_MID, 0, Header::HEIGHT);
    lv_obj_set_style_bg_opa(m_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_content, 0, 0);
    lv_obj_set_style_pad_all(m_content, ThemeManager::SPACING_MD, 0);

    // Name input
    lv_obj_t* nameLabel = lv_label_create(m_content);
    lv_label_set_text(nameLabel, "Name");
    lv_obj_set_style_text_color(nameLabel, m_theme.textPrimary(), 0);
    lv_obj_align(nameLabel, LV_ALIGN_TOP_LEFT, 0, 0);

    m_nameInput = lv_textarea_create(m_content);
    lv_obj_set_size(m_nameInput, 400, 45);
    lv_obj_align(m_nameInput, LV_ALIGN_TOP_LEFT, 0, 22);
    lv_textarea_set_one_line(m_nameInput, true);
    lv_obj_set_style_bg_color(m_nameInput, m_theme.surface(), 0);
    lv_obj_set_style_text_color(m_nameInput, m_theme.textPrimary(), 0);
    lv_obj_set_style_border_color(m_nameInput, m_theme.divider(), 0);
    lv_obj_clear_flag(m_nameInput, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    if (lv_group_get_default()) {
        lv_group_add_obj(lv_group_get_default(), m_nameInput);
    }

    // Enabled switch
    lv_obj_t* enabledLabel = lv_label_create(m_content);
    lv_label_set_text(enabledLabel, "Enabled");
    lv_obj_set_style_text_color(enabledLabel, m_theme.textPrimary(), 0);
    lv_obj_align(enabledLabel, LV_ALIGN_TOP_LEFT, 450, 0);

    m_enabledSwitch = lv_switch_create(m_content);
    lv_obj_align(m_enabledSwitch, LV_ALIGN_TOP_LEFT, 450, 25);
    lv_obj_set_size(m_enabledSwitch, 50, 26);
    lv_obj_set_style_bg_color(m_enabledSwitch, m_theme.surfaceVariant(), 0);
    lv_obj_set_style_bg_color(m_enabledSwitch, m_theme.primary(), LV_PART_INDICATOR | LV_STATE_CHECKED);

    // Description
    lv_obj_t* descLabel = lv_label_create(m_content);
    lv_label_set_text(descLabel, "Description");
    lv_obj_set_style_text_color(descLabel, m_theme.textPrimary(), 0);
    lv_obj_align(descLabel, LV_ALIGN_TOP_LEFT, 0, 80);

    m_descInput = lv_textarea_create(m_content);
    lv_obj_set_size(m_descInput, LV_PCT(100), 60);
    lv_obj_align(m_descInput, LV_ALIGN_TOP_LEFT, 0, 102);
    lv_obj_set_style_bg_color(m_descInput, m_theme.surface(), 0);
    lv_obj_set_style_text_color(m_descInput, m_theme.textPrimary(), 0);
    lv_obj_set_style_border_color(m_descInput, m_theme.divider(), 0);
    lv_obj_clear_flag(m_descInput, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    if (lv_group_get_default()) {
        lv_group_add_obj(lv_group_get_default(), m_descInput);
    }

    // Trigger info (read-only)
    lv_obj_t* triggerTitle = lv_label_create(m_content);
    lv_label_set_text(triggerTitle, "Trigger:");
    lv_obj_set_style_text_color(triggerTitle, m_theme.textPrimary(), 0);
    lv_obj_align(triggerTitle, LV_ALIGN_TOP_LEFT, 0, 180);

    m_triggerLabel = lv_label_create(m_content);
    lv_obj_set_style_text_color(m_triggerLabel, m_theme.textSecondary(), 0);
    lv_obj_align(m_triggerLabel, LV_ALIGN_TOP_LEFT, 80, 180);
    lv_label_set_long_mode(m_triggerLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(m_triggerLabel, 600);

    // Action info (read-only)
    lv_obj_t* actionTitle = lv_label_create(m_content);
    lv_label_set_text(actionTitle, "Action:");
    lv_obj_set_style_text_color(actionTitle, m_theme.textPrimary(), 0);
    lv_obj_align(actionTitle, LV_ALIGN_TOP_LEFT, 0, 210);

    m_actionLabel = lv_label_create(m_content);
    lv_obj_set_style_text_color(m_actionLabel, m_theme.textSecondary(), 0);
    lv_obj_align(m_actionLabel, LV_ALIGN_TOP_LEFT, 80, 210);
    lv_label_set_long_mode(m_actionLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(m_actionLabel, 600);

    // Buttons
    m_deleteBtn = lv_btn_create(m_content);
    lv_obj_set_size(m_deleteBtn, 100, 45);
    lv_obj_align(m_deleteBtn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(m_deleteBtn, m_theme.error(), 0);

    lv_obj_t* deleteLabel = lv_label_create(m_deleteBtn);
    lv_label_set_text(deleteLabel, "Delete");
    lv_obj_set_style_text_color(deleteLabel, lv_color_white(), 0);
    lv_obj_center(deleteLabel);

    lv_obj_add_event_cb(m_deleteBtn, [](lv_event_t* e) {
        auto* self = static_cast<EditAutomationScreen*>(lv_event_get_user_data(e));
        if (self) self->onDeleteClicked();
    }, LV_EVENT_CLICKED, this);

    m_saveBtn = lv_btn_create(m_content);
    lv_obj_set_size(m_saveBtn, 100, 45);
    lv_obj_align(m_saveBtn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(m_saveBtn, m_theme.primary(), 0);

    lv_obj_t* saveLabel = lv_label_create(m_saveBtn);
    lv_label_set_text(saveLabel, "Save");
    lv_obj_set_style_text_color(saveLabel, lv_color_white(), 0);
    lv_obj_center(saveLabel);

    lv_obj_add_event_cb(m_saveBtn, [](lv_event_t* e) {
        auto* self = static_cast<EditAutomationScreen*>(lv_event_get_user_data(e));
        if (self) self->onSaveClicked();
    }, LV_EVENT_CLICKED, this);
}

void EditAutomationScreen::populateFields() {
    auto automation = m_automationManager.getAutomation(m_automationId);
    if (!automation) {
        LOG_ERROR("Automation not found: %s", m_automationId.c_str());
        return;
    }

    if (m_nameInput) {
        lv_textarea_set_text(m_nameInput, automation->name.c_str());
    }

    if (m_descInput) {
        lv_textarea_set_text(m_descInput, automation->description.c_str());
    }

    if (m_enabledSwitch) {
        if (automation->enabled) {
            lv_obj_add_state(m_enabledSwitch, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(m_enabledSwitch, LV_STATE_CHECKED);
        }
    }

    // Build trigger description
    if (m_triggerLabel && !automation->triggers.empty()) {
        const auto& trigger = automation->triggers[0];
        std::string triggerDesc;

        switch (trigger.type) {
            case TriggerType::DeviceState:
                triggerDesc = "When " + trigger.deviceId + "." + trigger.property;
                if (!trigger.toValue.is_null()) {
                    triggerDesc += " = " + trigger.toValue.dump();
                } else {
                    triggerDesc += " changes";
                }
                break;

            case TriggerType::Time:
                triggerDesc = "At " + std::to_string(trigger.hour) + ":" +
                              (trigger.minute < 10 ? "0" : "") + std::to_string(trigger.minute);
                break;

            case TriggerType::TimeInterval:
                triggerDesc = "Every " + std::to_string(trigger.intervalMinutes) + " minutes";
                break;

            case TriggerType::SensorThreshold:
                triggerDesc = "When " + trigger.deviceId + " " +
                              compareOpToString(trigger.compareOp) + " " +
                              std::to_string(trigger.threshold);
                break;
        }

        lv_label_set_text(m_triggerLabel, triggerDesc.c_str());
    }

    // Build action description
    if (m_actionLabel && !automation->actions.empty()) {
        std::string actionDesc;
        for (const auto& action : automation->actions) {
            if (!actionDesc.empty()) actionDesc += ", ";

            if (action.type == ActionType::SetDeviceState) {
                actionDesc += "Set " + action.deviceId + "." + action.property +
                              " = " + action.value.dump();
            } else if (action.type == ActionType::Delay) {
                actionDesc += "Wait " + std::to_string(action.delayMs) + "ms";
            }
        }
        lv_label_set_text(m_actionLabel, actionDesc.c_str());
    }
}

void EditAutomationScreen::onSaveClicked() {
    auto automation = m_automationManager.getAutomation(m_automationId);
    if (!automation) return;

    // Update from form
    if (m_nameInput) {
        automation->name = lv_textarea_get_text(m_nameInput);
    }

    if (m_descInput) {
        automation->description = lv_textarea_get_text(m_descInput);
    }

    if (m_enabledSwitch) {
        automation->enabled = lv_obj_has_state(m_enabledSwitch, LV_STATE_CHECKED);
    }

    if (m_automationManager.updateAutomation(*automation)) {
        LOG_INFO("Automation updated: %s", automation->name.c_str());

        if (m_onUpdated) {
            m_onUpdated();
        }

        m_screenManager.goBack();
    } else {
        LOG_ERROR("Failed to update automation");
    }
}

void EditAutomationScreen::onDeleteClicked() {
    showDeleteConfirmation();
}

void EditAutomationScreen::showDeleteConfirmation() {
    // Create modal focus group for keyboard navigation containment
    lv_group_t* modalGroup = pushModalFocusGroup();

    // Create overlay
    lv_obj_t* overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);

    // Dialog
    lv_obj_t* dialog = lv_obj_create(overlay);
    lv_obj_set_size(dialog, 350, 150);
    lv_obj_align(dialog, LV_ALIGN_CENTER, 0, 0);
    m_theme.applyCardStyle(dialog);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);

    // Message
    lv_obj_t* msg = lv_label_create(dialog);
    lv_label_set_text(msg, "Delete this automation?");
    lv_obj_set_style_text_color(msg, m_theme.textPrimary(), 0);
    lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 20);

    // Store automation ID and screen pointer for callbacks
    struct CallbackData {
        EditAutomationScreen* screen;
        std::string automationId;
        lv_obj_t* overlay;
    };
    auto* data = new CallbackData{this, m_automationId, overlay};

    // Cancel button
    lv_obj_t* cancelBtn = lv_btn_create(dialog);
    lv_obj_set_size(cancelBtn, 100, 40);
    lv_obj_align(cancelBtn, LV_ALIGN_BOTTOM_LEFT, 20, -15);
    lv_obj_set_style_bg_color(cancelBtn, m_theme.surfaceVariant(), 0);

    lv_obj_t* cancelLabel = lv_label_create(cancelBtn);
    lv_label_set_text(cancelLabel, "Cancel");
    lv_obj_set_style_text_color(cancelLabel, m_theme.textPrimary(), 0);
    lv_obj_center(cancelLabel);

    // Add cancel button to modal group for keyboard navigation
    if (modalGroup) {
        lv_group_add_obj(modalGroup, cancelBtn);
    }

    lv_obj_add_event_cb(cancelBtn, [](lv_event_t* e) {
        auto* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data) {
            popModalFocusGroup();  // Restore focus to main group
            lv_obj_del(data->overlay);
            delete data;
        }
    }, LV_EVENT_CLICKED, data);

    // Delete button
    lv_obj_t* deleteBtn = lv_btn_create(dialog);
    lv_obj_set_size(deleteBtn, 100, 40);
    lv_obj_align(deleteBtn, LV_ALIGN_BOTTOM_RIGHT, -20, -15);
    lv_obj_set_style_bg_color(deleteBtn, m_theme.error(), 0);

    lv_obj_t* deleteLabel = lv_label_create(deleteBtn);
    lv_label_set_text(deleteLabel, "Delete");
    lv_obj_set_style_text_color(deleteLabel, lv_color_white(), 0);
    lv_obj_center(deleteLabel);

    // Add delete button to modal group for keyboard navigation
    if (modalGroup) {
        lv_group_add_obj(modalGroup, deleteBtn);
    }

    lv_obj_add_event_cb(deleteBtn, [](lv_event_t* e) {
        auto* data = static_cast<CallbackData*>(lv_event_get_user_data(e));
        if (data && data->screen) {
            data->screen->m_automationManager.deleteAutomation(data->automationId);

            if (data->screen->m_onDeleted) {
                data->screen->m_onDeleted();
            }

            popModalFocusGroup();  // Restore focus to main group
            lv_obj_del(data->overlay);
            data->screen->m_screenManager.goBack();
            delete data;
        }
    }, LV_EVENT_CLICKED, data);
}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
