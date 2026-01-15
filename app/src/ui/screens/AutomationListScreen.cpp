/**
 * Automation List Screen Implementation
 */

#include "smarthub/ui/screens/AutomationListScreen.hpp"
#include "smarthub/ui/ScreenManager.hpp"
#include "smarthub/ui/ThemeManager.hpp"
#include "smarthub/ui/widgets/Header.hpp"
#include "smarthub/automation/AutomationManager.hpp"
#include "smarthub/automation/Automation.hpp"
#include "smarthub/core/Logger.hpp"

#include <ctime>
#include <sstream>
#include <iomanip>

namespace smarthub {
namespace ui {

AutomationListScreen::AutomationListScreen(ScreenManager& screenManager,
                                           ThemeManager& theme,
                                           AutomationManager& automationManager)
    : Screen(screenManager, "automations")
    , m_theme(theme)
    , m_automationManager(automationManager)
{
}

AutomationListScreen::~AutomationListScreen() = default;

void AutomationListScreen::onCreate() {
#ifdef SMARTHUB_ENABLE_LVGL
    if (!m_container) return;

    lv_obj_set_style_bg_color(m_container, m_theme.background(), 0);

    createHeader();
    createContent();

    LOG_DEBUG("AutomationListScreen created");
#endif
}

void AutomationListScreen::onShow() {
#ifdef SMARTHUB_ENABLE_LVGL
    refreshAutomationList();
    LOG_DEBUG("AutomationListScreen shown");
#endif
}

void AutomationListScreen::onUpdate(uint32_t deltaMs) {
    (void)deltaMs;
}

void AutomationListScreen::onDestroy() {
#ifdef SMARTHUB_ENABLE_LVGL
    m_automationRows.clear();
#endif
}

#ifdef SMARTHUB_ENABLE_LVGL

void AutomationListScreen::createHeader() {
    lv_obj_t* header = lv_obj_create(m_container);
    lv_obj_set_size(header, LV_PCT(100), Header::HEIGHT);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    m_theme.applyHeaderStyle(header);

    // Back button
    lv_obj_t* backBtn = lv_btn_create(header);
    lv_obj_set_size(backBtn, 48, 48);
    lv_obj_align(backBtn, LV_ALIGN_LEFT_MID, ThemeManager::SPACING_SM, 0);
    lv_obj_set_style_bg_opa(backBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(backBtn, 0, 0);
    lv_obj_add_event_cb(backBtn, [](lv_event_t* e) {
        auto* self = static_cast<AutomationListScreen*>(lv_event_get_user_data(e));
        if (self) self->onBackClicked();
    }, LV_EVENT_CLICKED, this);

    lv_obj_t* backIcon = lv_label_create(backBtn);
    lv_label_set_text(backIcon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(backIcon, m_theme.textPrimary(), 0);
    lv_obj_center(backIcon);

    // Title
    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Automations");
    lv_obj_set_style_text_color(title, m_theme.textPrimary(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 60, 0);
}

void AutomationListScreen::createContent() {
    // Full height minus header (no navbar for this screen)
    constexpr int CONTENT_HEIGHT = 480 - Header::HEIGHT;

    m_content = lv_obj_create(m_container);
    lv_obj_set_size(m_content, LV_PCT(100), CONTENT_HEIGHT);
    lv_obj_align(m_content, LV_ALIGN_TOP_MID, 0, Header::HEIGHT);

    lv_obj_set_style_bg_opa(m_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_content, 0, 0);
    lv_obj_set_style_pad_all(m_content, 0, 0);

    // Automation list container
    m_automationList = lv_obj_create(m_content);
    lv_obj_set_size(m_automationList, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_automationList, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_automationList, 0, 0);
    lv_obj_set_style_pad_all(m_automationList, ThemeManager::SPACING_SM, 0);

    // Vertical flex layout
    lv_obj_set_flex_flow(m_automationList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_automationList, ThemeManager::SPACING_SM, 0);
}

void AutomationListScreen::refreshAutomationList() {
    if (!m_automationList) return;

    // Clear existing rows
    lv_obj_clean(m_automationList);
    m_automationRows.clear();

    // Get all automations
    auto automations = m_automationManager.getAllAutomations();

    if (automations.empty()) {
        // Show "No automations" message
        lv_obj_t* emptyLabel = lv_label_create(m_automationList);
        lv_label_set_text(emptyLabel, "No automations configured");
        lv_obj_set_style_text_color(emptyLabel, m_theme.textSecondary(), 0);
        lv_obj_set_width(emptyLabel, LV_PCT(100));
        lv_obj_set_style_text_align(emptyLabel, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_top(emptyLabel, 40, 0);
    }

    for (const auto& automation : automations) {
        if (!automation) continue;

        std::string triggerSummary = getTriggerSummary(automation->id);

        lv_obj_t* row = createAutomationRow(
            m_automationList,
            automation->id,
            automation->name,
            triggerSummary,
            automation->enabled,
            automation->lastTriggeredAt);

        m_automationRows.push_back({automation->id, row});
    }

    // Add "Add Automation" card at the end
    createAddAutomationCard();
}

void AutomationListScreen::createAddAutomationCard() {
    lv_obj_t* addCard = lv_obj_create(m_automationList);
    lv_obj_set_size(addCard, lv_pct(100) - 2 * ThemeManager::SPACING_SM, 60);
    lv_obj_set_style_bg_color(addCard, m_theme.surface(), 0);
    lv_obj_set_style_border_color(addCard, m_theme.primary(), 0);
    lv_obj_set_style_border_width(addCard, 2, 0);
    lv_obj_set_style_border_opa(addCard, LV_OPA_50, 0);
    lv_obj_set_style_radius(addCard, ThemeManager::CARD_RADIUS, 0);
    lv_obj_clear_flag(addCard, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(addCard, LV_OBJ_FLAG_CLICKABLE);

    // Plus icon
    lv_obj_t* icon = lv_label_create(addCard);
    lv_label_set_text(icon, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_color(icon, m_theme.primary(), 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, ThemeManager::SPACING_MD, 0);

    // Text
    lv_obj_t* label = lv_label_create(addCard);
    lv_label_set_text(label, "Add Automation");
    lv_obj_set_style_text_color(label, m_theme.primary(), 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 50, 0);

    lv_obj_add_event_cb(addCard, [](lv_event_t* e) {
        auto* self = static_cast<AutomationListScreen*>(lv_event_get_user_data(e));
        if (self) self->onAddAutomationClicked();
    }, LV_EVENT_CLICKED, this);
}

lv_obj_t* AutomationListScreen::createAutomationRow(lv_obj_t* parent,
                                                     const std::string& automationId,
                                                     const std::string& name,
                                                     const std::string& triggerSummary,
                                                     bool enabled,
                                                     uint64_t lastTriggered) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100) - 2 * ThemeManager::SPACING_SM, 80);
    m_theme.applyCardStyle(row);
    lv_obj_set_style_pad_all(row, 0, 0);  // Remove card padding for better control
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);

    // Store automation ID as user data for the row
    char* idCopy = new char[automationId.length() + 1];
    strcpy(idCopy, automationId.c_str());
    lv_obj_set_user_data(row, idCopy);

    // Clean up ID on delete
    lv_obj_add_event_cb(row, [](lv_event_t* e) {
        delete[] static_cast<char*>(lv_obj_get_user_data(lv_event_get_target(e)));
    }, LV_EVENT_DELETE, nullptr);

    // Row click handler
    lv_obj_add_event_cb(row, rowClickHandler, LV_EVENT_CLICKED, this);

    // Left side: Name and trigger summary
    lv_obj_t* nameLabel = lv_label_create(row);
    lv_label_set_text(nameLabel, name.c_str());
    lv_obj_set_style_text_color(nameLabel, m_theme.textPrimary(), 0);
    lv_obj_set_style_text_font(nameLabel, &lv_font_montserrat_16, 0);
    lv_obj_align(nameLabel, LV_ALIGN_TOP_LEFT, ThemeManager::SPACING_MD, 15);
    lv_label_set_long_mode(nameLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_width(nameLabel, 450);

    // Trigger summary
    lv_obj_t* triggerLabel = lv_label_create(row);
    lv_label_set_text(triggerLabel, triggerSummary.c_str());
    lv_obj_set_style_text_color(triggerLabel, m_theme.textSecondary(), 0);
    lv_obj_align(triggerLabel, LV_ALIGN_BOTTOM_LEFT, ThemeManager::SPACING_MD, -15);
    lv_label_set_long_mode(triggerLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_width(triggerLabel, 350);

    // Last triggered time
    if (lastTriggered > 0) {
        lv_obj_t* timeLabel = lv_label_create(row);
        lv_label_set_text(timeLabel, formatLastTriggered(lastTriggered).c_str());
        lv_obj_set_style_text_color(timeLabel, m_theme.textSecondary(), 0);
        lv_obj_align(timeLabel, LV_ALIGN_BOTTOM_LEFT, 370, -15);
    }

    // Right side: Enable/disable switch
    lv_obj_t* toggle = lv_switch_create(row);
    lv_obj_align(toggle, LV_ALIGN_RIGHT_MID, -ThemeManager::SPACING_MD, 0);
    lv_obj_set_size(toggle, 50, 26);

    if (enabled) {
        lv_obj_add_state(toggle, LV_STATE_CHECKED);
    }

    // Style the switch
    lv_obj_set_style_bg_color(toggle, m_theme.surfaceVariant(), 0);
    lv_obj_set_style_bg_color(toggle, m_theme.primary(), LV_PART_INDICATOR | LV_STATE_CHECKED);

    // Store automation ID for toggle handler
    char* toggleIdCopy = new char[automationId.length() + 1];
    strcpy(toggleIdCopy, automationId.c_str());
    lv_obj_set_user_data(toggle, toggleIdCopy);

    lv_obj_add_event_cb(toggle, [](lv_event_t* e) {
        delete[] static_cast<char*>(lv_obj_get_user_data(lv_event_get_target(e)));
    }, LV_EVENT_DELETE, nullptr);

    lv_obj_add_event_cb(toggle, toggleHandler, LV_EVENT_VALUE_CHANGED, this);

    return row;
}

void AutomationListScreen::toggleHandler(lv_event_t* e) {
    auto* self = static_cast<AutomationListScreen*>(lv_event_get_user_data(e));
    lv_obj_t* toggle = lv_event_get_target(e);

    if (self && toggle) {
        char* automationId = static_cast<char*>(lv_obj_get_user_data(toggle));
        bool enabled = lv_obj_has_state(toggle, LV_STATE_CHECKED);

        if (automationId) {
            self->onAutomationToggle(automationId, enabled);
        }
    }
}

void AutomationListScreen::rowClickHandler(lv_event_t* e) {
    auto* self = static_cast<AutomationListScreen*>(lv_event_get_user_data(e));
    lv_obj_t* row = lv_event_get_current_target(e);
    lv_obj_t* clicked = lv_event_get_target(e);

    // Check if click was on the toggle switch (ignore those)
    if (clicked != row) return;

    if (self && row) {
        char* automationId = static_cast<char*>(lv_obj_get_user_data(row));
        if (automationId) {
            self->onAutomationClicked(automationId);
        }
    }
}

void AutomationListScreen::onAutomationToggle(const std::string& automationId, bool enabled) {
    LOG_DEBUG("Automation %s toggled to %s", automationId.c_str(),
              enabled ? "enabled" : "disabled");
    m_automationManager.setEnabled(automationId, enabled);
}

void AutomationListScreen::onAutomationClicked(const std::string& automationId) {
    LOG_DEBUG("Automation clicked: %s", automationId.c_str());

    // Navigate to edit screen
    auto* editScreen = m_screenManager.getScreen("edit_automation");
    if (editScreen) {
        // The edit screen should have a setAutomationId method
        // For now just navigate
        m_screenManager.showScreen("edit_automation");
    }
}

void AutomationListScreen::onAddAutomationClicked() {
    LOG_DEBUG("Add automation clicked");
    m_screenManager.showScreen("add_automation");
}

void AutomationListScreen::onBackClicked() {
    m_screenManager.goBack();
}

std::string AutomationListScreen::getTriggerSummary(const std::string& automationId) {
    auto automation = m_automationManager.getAutomation(automationId);
    if (!automation || automation->triggers.empty()) {
        return "No triggers";
    }

    const auto& trigger = automation->triggers[0];

    switch (trigger.type) {
        case TriggerType::DeviceState:
            if (!trigger.toValue.is_null()) {
                return "When " + trigger.deviceId + "." + trigger.property +
                       " = " + trigger.toValue.dump();
            }
            return "When " + trigger.deviceId + "." + trigger.property + " changes";

        case TriggerType::Time: {
            std::stringstream ss;
            ss << "At ";
            if (trigger.hour >= 0) {
                ss << std::setfill('0') << std::setw(2) << trigger.hour << ":";
                ss << std::setfill('0') << std::setw(2) << (trigger.minute >= 0 ? trigger.minute : 0);
            } else {
                ss << "every hour";
            }
            return ss.str();
        }

        case TriggerType::TimeInterval:
            return "Every " + std::to_string(trigger.intervalMinutes) + " minutes";

        case TriggerType::SensorThreshold: {
            std::string opStr;
            switch (trigger.compareOp) {
                case CompareOp::GreaterThan: opStr = ">"; break;
                case CompareOp::GreaterOrEqual: opStr = ">="; break;
                case CompareOp::LessThan: opStr = "<"; break;
                case CompareOp::LessOrEqual: opStr = "<="; break;
                default: opStr = "="; break;
            }
            return "When " + trigger.deviceId + "." + trigger.property +
                   " " + opStr + " " + std::to_string(trigger.threshold);
        }

        default:
            return "Unknown trigger";
    }
}

std::string AutomationListScreen::formatLastTriggered(uint64_t timestamp) {
    if (timestamp == 0) return "";

    uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    uint64_t diff = now - timestamp;

    if (diff < 60) {
        return "Just now";
    } else if (diff < 3600) {
        return std::to_string(diff / 60) + "m ago";
    } else if (diff < 86400) {
        return std::to_string(diff / 3600) + "h ago";
    } else {
        return std::to_string(diff / 86400) + "d ago";
    }
}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
