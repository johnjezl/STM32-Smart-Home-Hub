/**
 * Add Automation Screen Implementation
 */

#include "smarthub/ui/screens/AddAutomationScreen.hpp"
#include "smarthub/ui/ScreenManager.hpp"
#include "smarthub/ui/ThemeManager.hpp"
#include "smarthub/ui/widgets/Header.hpp"
#include "smarthub/automation/AutomationManager.hpp"
#include "smarthub/devices/DeviceManager.hpp"
#include "smarthub/devices/Device.hpp"
#include "smarthub/core/Logger.hpp"

namespace smarthub {
namespace ui {

AddAutomationScreen::AddAutomationScreen(ScreenManager& screenManager,
                                         ThemeManager& theme,
                                         AutomationManager& automationManager,
                                         DeviceManager& deviceManager)
    : Screen(screenManager, "add_automation")
    , m_theme(theme)
    , m_automationManager(automationManager)
    , m_deviceManager(deviceManager)
{
}

AddAutomationScreen::~AddAutomationScreen() = default;

void AddAutomationScreen::onAutomationAdded(AutomationAddedCallback callback) {
    m_onAutomationAdded = callback;
}

void AddAutomationScreen::onCreate() {
#ifdef SMARTHUB_ENABLE_LVGL
    if (!m_container) return;

    lv_obj_set_style_bg_color(m_container, m_theme.background(), 0);

    createHeader();
    createContent();
    createNavigationButtons();

    LOG_DEBUG("AddAutomationScreen created");
#endif
}

void AddAutomationScreen::onShow() {
#ifdef SMARTHUB_ENABLE_LVGL
    resetForm();

    // Load devices for dropdowns
    m_devices.clear();
    auto devices = m_deviceManager.getAllDevices();
    for (const auto& device : devices) {
        if (device) {
            m_devices.push_back({device->id(), device->name()});
        }
    }

    showStep(1);
    LOG_DEBUG("AddAutomationScreen shown");
#endif
}

void AddAutomationScreen::onHide() {
#ifdef SMARTHUB_ENABLE_LVGL
    LOG_DEBUG("AddAutomationScreen hidden");
#endif
}

void AddAutomationScreen::onUpdate(uint32_t deltaMs) {
    (void)deltaMs;
}

void AddAutomationScreen::onDestroy() {
#ifdef SMARTHUB_ENABLE_LVGL
    LOG_DEBUG("AddAutomationScreen destroyed");
#endif
}

#ifdef SMARTHUB_ENABLE_LVGL

void AddAutomationScreen::createHeader() {
    m_header = lv_obj_create(m_container);
    lv_obj_set_size(m_header, LV_PCT(100), Header::HEIGHT);
    lv_obj_align(m_header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(m_header, LV_OBJ_FLAG_SCROLLABLE);
    m_theme.applyHeaderStyle(m_header);

    // Back button
    m_backBtn = lv_btn_create(m_header);
    lv_obj_set_size(m_backBtn, 48, 48);
    lv_obj_align(m_backBtn, LV_ALIGN_LEFT_MID, ThemeManager::SPACING_SM, 0);
    lv_obj_set_style_bg_opa(m_backBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(m_backBtn, 0, 0);

    lv_obj_t* backIcon = lv_label_create(m_backBtn);
    lv_label_set_text(backIcon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(backIcon, m_theme.textPrimary(), 0);
    lv_obj_center(backIcon);

    lv_obj_add_event_cb(m_backBtn, [](lv_event_t* e) {
        auto* self = static_cast<AddAutomationScreen*>(lv_event_get_user_data(e));
        if (self) self->m_screenManager.goBack();
    }, LV_EVENT_CLICKED, this);

    // Title
    m_titleLabel = lv_label_create(m_header);
    lv_label_set_text(m_titleLabel, "Add Automation (Step 1/4)");
    lv_obj_set_style_text_color(m_titleLabel, m_theme.textPrimary(), 0);
    lv_obj_align(m_titleLabel, LV_ALIGN_CENTER, 0, 0);
}

void AddAutomationScreen::createContent() {
    constexpr int CONTENT_HEIGHT = 480 - Header::HEIGHT - 60;  // Leave space for nav buttons

    m_content = lv_obj_create(m_container);
    lv_obj_set_size(m_content, LV_PCT(100), CONTENT_HEIGHT);
    lv_obj_align(m_content, LV_ALIGN_TOP_MID, 0, Header::HEIGHT);
    lv_obj_set_style_bg_opa(m_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_content, 0, 0);
    lv_obj_set_style_pad_all(m_content, ThemeManager::SPACING_MD, 0);
    // Disable scrolling to prevent content from shifting when textarea is focused
    lv_obj_clear_flag(m_content, LV_OBJ_FLAG_SCROLLABLE);

    // Create all step containers (hidden by default)
    createStep1_BasicInfo();
    createStep2_Triggers();
    createStep3_Conditions();
    createStep4_Actions();
}

void AddAutomationScreen::createNavigationButtons() {
    // Previous button
    m_prevBtn = lv_btn_create(m_container);
    lv_obj_set_size(m_prevBtn, 120, 45);
    lv_obj_align(m_prevBtn, LV_ALIGN_BOTTOM_LEFT, ThemeManager::SPACING_MD, -ThemeManager::SPACING_SM);
    lv_obj_set_style_bg_color(m_prevBtn, m_theme.surfaceVariant(), 0);

    lv_obj_t* prevLabel = lv_label_create(m_prevBtn);
    lv_label_set_text(prevLabel, "Previous");
    lv_obj_set_style_text_color(prevLabel, m_theme.textPrimary(), 0);
    lv_obj_center(prevLabel);

    lv_obj_add_event_cb(m_prevBtn, [](lv_event_t* e) {
        auto* self = static_cast<AddAutomationScreen*>(lv_event_get_user_data(e));
        if (self) self->prevStep();
    }, LV_EVENT_CLICKED, this);

    // Next button
    m_nextBtn = lv_btn_create(m_container);
    lv_obj_set_size(m_nextBtn, 120, 45);
    lv_obj_align(m_nextBtn, LV_ALIGN_BOTTOM_RIGHT, -ThemeManager::SPACING_MD, -ThemeManager::SPACING_SM);
    lv_obj_set_style_bg_color(m_nextBtn, m_theme.primary(), 0);

    lv_obj_t* nextLabel = lv_label_create(m_nextBtn);
    lv_label_set_text(nextLabel, "Next");
    lv_obj_set_style_text_color(nextLabel, lv_color_white(), 0);
    lv_obj_center(nextLabel);

    lv_obj_add_event_cb(m_nextBtn, [](lv_event_t* e) {
        auto* self = static_cast<AddAutomationScreen*>(lv_event_get_user_data(e));
        if (self) self->nextStep();
    }, LV_EVENT_CLICKED, this);
}

void AddAutomationScreen::createStep1_BasicInfo() {
    m_step1Container = lv_obj_create(m_content);
    lv_obj_set_size(m_step1Container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_step1Container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_step1Container, 0, 0);
    lv_obj_set_style_pad_all(m_step1Container, 0, 0);
    lv_obj_clear_flag(m_step1Container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(m_step1Container, LV_OBJ_FLAG_HIDDEN);

    // Name label
    lv_obj_t* nameLabel = lv_label_create(m_step1Container);
    lv_label_set_text(nameLabel, "Automation Name");
    lv_obj_set_style_text_color(nameLabel, m_theme.textPrimary(), 0);
    lv_obj_align(nameLabel, LV_ALIGN_TOP_LEFT, 0, 0);

    // Name input
    m_nameInput = lv_textarea_create(m_step1Container);
    lv_obj_set_size(m_nameInput, LV_PCT(100), 45);
    lv_obj_align(m_nameInput, LV_ALIGN_TOP_LEFT, 0, 25);
    lv_textarea_set_placeholder_text(m_nameInput, "Enter automation name...");
    lv_textarea_set_one_line(m_nameInput, true);
    lv_obj_set_style_bg_color(m_nameInput, m_theme.surface(), 0);
    lv_obj_set_style_text_color(m_nameInput, m_theme.textPrimary(), 0);
    lv_obj_set_style_border_color(m_nameInput, m_theme.divider(), 0);
    // Disable scroll-on-focus to prevent screen shifting
    lv_obj_clear_flag(m_nameInput, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    // Add to keyboard input group
    if (lv_group_get_default()) {
        lv_group_add_obj(lv_group_get_default(), m_nameInput);
    }

    // Description label
    lv_obj_t* descLabel = lv_label_create(m_step1Container);
    lv_label_set_text(descLabel, "Description (optional)");
    lv_obj_set_style_text_color(descLabel, m_theme.textPrimary(), 0);
    lv_obj_align(descLabel, LV_ALIGN_TOP_LEFT, 0, 85);

    // Description input
    m_descInput = lv_textarea_create(m_step1Container);
    lv_obj_set_size(m_descInput, LV_PCT(100), 80);
    lv_obj_align(m_descInput, LV_ALIGN_TOP_LEFT, 0, 110);
    lv_textarea_set_placeholder_text(m_descInput, "What does this automation do?");
    lv_obj_set_style_bg_color(m_descInput, m_theme.surface(), 0);
    lv_obj_set_style_text_color(m_descInput, m_theme.textPrimary(), 0);
    lv_obj_set_style_border_color(m_descInput, m_theme.divider(), 0);
    // Disable scroll-on-focus to prevent screen shifting
    lv_obj_clear_flag(m_descInput, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    // Add to keyboard input group
    if (lv_group_get_default()) {
        lv_group_add_obj(lv_group_get_default(), m_descInput);
    }
}

void AddAutomationScreen::createStep2_Triggers() {
    m_step2Container = lv_obj_create(m_content);
    lv_obj_set_size(m_step2Container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_step2Container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_step2Container, 0, 0);
    lv_obj_set_style_pad_all(m_step2Container, 0, 0);
    lv_obj_clear_flag(m_step2Container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(m_step2Container, LV_OBJ_FLAG_HIDDEN);

    // Trigger type label
    lv_obj_t* typeLabel = lv_label_create(m_step2Container);
    lv_label_set_text(typeLabel, "Trigger Type");
    lv_obj_set_style_text_color(typeLabel, m_theme.textPrimary(), 0);
    lv_obj_align(typeLabel, LV_ALIGN_TOP_LEFT, 0, 0);

    // Trigger type dropdown
    m_triggerTypeList = lv_dropdown_create(m_step2Container);
    lv_obj_set_size(m_triggerTypeList, LV_PCT(100), 45);
    lv_obj_align(m_triggerTypeList, LV_ALIGN_TOP_LEFT, 0, 25);
    lv_dropdown_set_options(m_triggerTypeList,
        "Device State Change\n"
        "Specific Time\n"
        "Time Interval\n"
        "Sensor Threshold");
    styleDropdown(m_triggerTypeList);

    lv_obj_add_event_cb(m_triggerTypeList, [](lv_event_t* e) {
        auto* self = static_cast<AddAutomationScreen*>(lv_event_get_user_data(e));
        if (self) {
            int sel = lv_dropdown_get_selected(lv_event_get_target(e));
            TriggerType type = static_cast<TriggerType>(sel);
            self->showTriggerConfig(type);
        }
    }, LV_EVENT_VALUE_CHANGED, this);

    // Trigger config container (changes based on type)
    m_triggerConfigContainer = lv_obj_create(m_step2Container);
    lv_obj_set_size(m_triggerConfigContainer, LV_PCT(100), 200);
    lv_obj_align(m_triggerConfigContainer, LV_ALIGN_TOP_LEFT, 0, 85);
    lv_obj_set_style_bg_opa(m_triggerConfigContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_triggerConfigContainer, 0, 0);
    lv_obj_set_style_pad_all(m_triggerConfigContainer, 0, 0);
}

void AddAutomationScreen::showTriggerConfig(TriggerType type) {
    m_selectedTriggerType = type;

    // Clear existing config
    lv_obj_clean(m_triggerConfigContainer);

    switch (type) {
        case TriggerType::DeviceState:
            createDeviceStateTrigger();
            break;
        case TriggerType::Time:
            createTimeTrigger();
            break;
        case TriggerType::TimeInterval:
            createIntervalTrigger();
            break;
        case TriggerType::SensorThreshold:
            createSensorThresholdTrigger();
            break;
    }
}

void AddAutomationScreen::createDeviceStateTrigger() {
    lv_obj_t* deviceLabel = lv_label_create(m_triggerConfigContainer);
    lv_label_set_text(deviceLabel, "Device");
    lv_obj_set_style_text_color(deviceLabel, m_theme.textPrimary(), 0);
    lv_obj_align(deviceLabel, LV_ALIGN_TOP_LEFT, 0, 0);

    m_triggerDeviceDropdown = lv_dropdown_create(m_triggerConfigContainer);
    lv_obj_set_size(m_triggerDeviceDropdown, 350, 40);
    lv_obj_align(m_triggerDeviceDropdown, LV_ALIGN_TOP_LEFT, 0, 22);
    populateDeviceDropdown(m_triggerDeviceDropdown);
    styleDropdown(m_triggerDeviceDropdown);

    lv_obj_t* propLabel = lv_label_create(m_triggerConfigContainer);
    lv_label_set_text(propLabel, "Property");
    lv_obj_set_style_text_color(propLabel, m_theme.textPrimary(), 0);
    lv_obj_align(propLabel, LV_ALIGN_TOP_LEFT, 370, 0);

    m_triggerPropertyDropdown = lv_dropdown_create(m_triggerConfigContainer);
    lv_obj_set_size(m_triggerPropertyDropdown, 200, 40);
    lv_obj_align(m_triggerPropertyDropdown, LV_ALIGN_TOP_LEFT, 370, 22);
    lv_dropdown_set_options(m_triggerPropertyDropdown, "on\nbrightness\ncolor\nmotion\ntemperature");
    styleDropdown(m_triggerPropertyDropdown);

    lv_obj_t* valueLabel = lv_label_create(m_triggerConfigContainer);
    lv_label_set_text(valueLabel, "Trigger when value becomes (leave empty for any change)");
    lv_obj_set_style_text_color(valueLabel, m_theme.textPrimary(), 0);
    lv_obj_align(valueLabel, LV_ALIGN_TOP_LEFT, 0, 75);

    m_triggerValueInput = lv_textarea_create(m_triggerConfigContainer);
    lv_obj_set_size(m_triggerValueInput, 200, 40);
    lv_obj_align(m_triggerValueInput, LV_ALIGN_TOP_LEFT, 0, 97);
    lv_textarea_set_placeholder_text(m_triggerValueInput, "true, false, or number");
    lv_textarea_set_one_line(m_triggerValueInput, true);
    lv_obj_set_style_bg_color(m_triggerValueInput, m_theme.surface(), 0);
    lv_obj_set_style_text_color(m_triggerValueInput, m_theme.textPrimary(), 0);
    if (lv_group_get_default()) {
        lv_group_add_obj(lv_group_get_default(), m_triggerValueInput);
    }
}

void AddAutomationScreen::createTimeTrigger() {
    lv_obj_t* hourLabel = lv_label_create(m_triggerConfigContainer);
    lv_label_set_text(hourLabel, "Hour (0-23)");
    lv_obj_set_style_text_color(hourLabel, m_theme.textPrimary(), 0);
    lv_obj_align(hourLabel, LV_ALIGN_TOP_LEFT, 0, 0);

    m_triggerHourDropdown = lv_dropdown_create(m_triggerConfigContainer);
    lv_obj_set_size(m_triggerHourDropdown, 100, 40);
    lv_obj_align(m_triggerHourDropdown, LV_ALIGN_TOP_LEFT, 0, 22);
    lv_dropdown_set_options(m_triggerHourDropdown,
        "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n"
        "13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23");
    styleDropdown(m_triggerHourDropdown);

    lv_obj_t* minLabel = lv_label_create(m_triggerConfigContainer);
    lv_label_set_text(minLabel, "Minute (0-59)");
    lv_obj_set_style_text_color(minLabel, m_theme.textPrimary(), 0);
    lv_obj_align(minLabel, LV_ALIGN_TOP_LEFT, 120, 0);

    m_triggerMinuteDropdown = lv_dropdown_create(m_triggerConfigContainer);
    lv_obj_set_size(m_triggerMinuteDropdown, 100, 40);
    lv_obj_align(m_triggerMinuteDropdown, LV_ALIGN_TOP_LEFT, 120, 22);
    lv_dropdown_set_options(m_triggerMinuteDropdown, "0\n15\n30\n45");
    styleDropdown(m_triggerMinuteDropdown);
}

void AddAutomationScreen::createIntervalTrigger() {
    lv_obj_t* intervalLabel = lv_label_create(m_triggerConfigContainer);
    lv_label_set_text(intervalLabel, "Repeat every (minutes)");
    lv_obj_set_style_text_color(intervalLabel, m_theme.textPrimary(), 0);
    lv_obj_align(intervalLabel, LV_ALIGN_TOP_LEFT, 0, 0);

    m_triggerIntervalInput = lv_textarea_create(m_triggerConfigContainer);
    lv_obj_set_size(m_triggerIntervalInput, 150, 40);
    lv_obj_align(m_triggerIntervalInput, LV_ALIGN_TOP_LEFT, 0, 22);
    lv_textarea_set_placeholder_text(m_triggerIntervalInput, "e.g., 30");
    lv_textarea_set_one_line(m_triggerIntervalInput, true);
    lv_textarea_set_accepted_chars(m_triggerIntervalInput, "0123456789");
    lv_obj_set_style_bg_color(m_triggerIntervalInput, m_theme.surface(), 0);
    lv_obj_set_style_text_color(m_triggerIntervalInput, m_theme.textPrimary(), 0);
    if (lv_group_get_default()) {
        lv_group_add_obj(lv_group_get_default(), m_triggerIntervalInput);
    }
}

void AddAutomationScreen::createSensorThresholdTrigger() {
    lv_obj_t* deviceLabel = lv_label_create(m_triggerConfigContainer);
    lv_label_set_text(deviceLabel, "Sensor");
    lv_obj_set_style_text_color(deviceLabel, m_theme.textPrimary(), 0);
    lv_obj_align(deviceLabel, LV_ALIGN_TOP_LEFT, 0, 0);

    m_triggerDeviceDropdown = lv_dropdown_create(m_triggerConfigContainer);
    lv_obj_set_size(m_triggerDeviceDropdown, 300, 40);
    lv_obj_align(m_triggerDeviceDropdown, LV_ALIGN_TOP_LEFT, 0, 22);
    populateDeviceDropdown(m_triggerDeviceDropdown);
    styleDropdown(m_triggerDeviceDropdown);

    lv_obj_t* opLabel = lv_label_create(m_triggerConfigContainer);
    lv_label_set_text(opLabel, "Condition");
    lv_obj_set_style_text_color(opLabel, m_theme.textPrimary(), 0);
    lv_obj_align(opLabel, LV_ALIGN_TOP_LEFT, 320, 0);

    m_triggerOpDropdown = lv_dropdown_create(m_triggerConfigContainer);
    lv_obj_set_size(m_triggerOpDropdown, 80, 40);
    lv_obj_align(m_triggerOpDropdown, LV_ALIGN_TOP_LEFT, 320, 22);
    lv_dropdown_set_options(m_triggerOpDropdown, ">\n>=\n<\n<=\n=");
    styleDropdown(m_triggerOpDropdown);

    lv_obj_t* threshLabel = lv_label_create(m_triggerConfigContainer);
    lv_label_set_text(threshLabel, "Value");
    lv_obj_set_style_text_color(threshLabel, m_theme.textPrimary(), 0);
    lv_obj_align(threshLabel, LV_ALIGN_TOP_LEFT, 420, 0);

    m_triggerThresholdInput = lv_textarea_create(m_triggerConfigContainer);
    lv_obj_set_size(m_triggerThresholdInput, 100, 40);
    lv_obj_align(m_triggerThresholdInput, LV_ALIGN_TOP_LEFT, 420, 22);
    lv_textarea_set_placeholder_text(m_triggerThresholdInput, "75");
    lv_textarea_set_one_line(m_triggerThresholdInput, true);
    lv_obj_set_style_bg_color(m_triggerThresholdInput, m_theme.surface(), 0);
    lv_obj_set_style_text_color(m_triggerThresholdInput, m_theme.textPrimary(), 0);
    if (lv_group_get_default()) {
        lv_group_add_obj(lv_group_get_default(), m_triggerThresholdInput);
    }
}

void AddAutomationScreen::createStep3_Conditions() {
    m_step3Container = lv_obj_create(m_content);
    lv_obj_set_size(m_step3Container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_step3Container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_step3Container, 0, 0);
    lv_obj_set_style_pad_all(m_step3Container, 0, 0);
    lv_obj_clear_flag(m_step3Container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(m_step3Container, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* label = lv_label_create(m_step3Container);
    lv_label_set_text(label, "Conditions (Optional)");
    lv_obj_set_style_text_color(label, m_theme.textPrimary(), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* helpText = lv_label_create(m_step3Container);
    lv_label_set_text(helpText, "Add conditions that must be true for the automation to run.\n"
                                "Skip this step to run the automation whenever the trigger fires.");
    lv_obj_set_style_text_color(helpText, m_theme.textSecondary(), 0);
    lv_obj_align(helpText, LV_ALIGN_TOP_LEFT, 0, 25);
    lv_label_set_long_mode(helpText, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(helpText, 700);

    m_conditionList = lv_obj_create(m_step3Container);
    lv_obj_set_size(m_conditionList, LV_PCT(100), 180);
    lv_obj_align(m_conditionList, LV_ALIGN_TOP_LEFT, 0, 80);
    lv_obj_set_style_bg_opa(m_conditionList, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_conditionList, 0, 0);
    lv_obj_set_flex_flow(m_conditionList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_conditionList, ThemeManager::SPACING_SM, 0);

    // Skip conditions for now - just show a note
    lv_obj_t* skipNote = lv_label_create(m_conditionList);
    lv_label_set_text(skipNote, "No conditions added. Automation will always run when triggered.");
    lv_obj_set_style_text_color(skipNote, m_theme.textSecondary(), 0);
}

void AddAutomationScreen::createStep4_Actions() {
    m_step4Container = lv_obj_create(m_content);
    lv_obj_set_size(m_step4Container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_step4Container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_step4Container, 0, 0);
    lv_obj_set_style_pad_all(m_step4Container, 0, 0);
    lv_obj_clear_flag(m_step4Container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(m_step4Container, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* label = lv_label_create(m_step4Container);
    lv_label_set_text(label, "Actions");
    lv_obj_set_style_text_color(label, m_theme.textPrimary(), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);

    // Device dropdown
    lv_obj_t* deviceLabel = lv_label_create(m_step4Container);
    lv_label_set_text(deviceLabel, "Device");
    lv_obj_set_style_text_color(deviceLabel, m_theme.textPrimary(), 0);
    lv_obj_align(deviceLabel, LV_ALIGN_TOP_LEFT, 0, 30);

    m_actionDeviceDropdown = lv_dropdown_create(m_step4Container);
    lv_obj_set_size(m_actionDeviceDropdown, 300, 40);
    lv_obj_align(m_actionDeviceDropdown, LV_ALIGN_TOP_LEFT, 0, 52);
    populateDeviceDropdown(m_actionDeviceDropdown);
    styleDropdown(m_actionDeviceDropdown);

    // Property dropdown
    lv_obj_t* propLabel = lv_label_create(m_step4Container);
    lv_label_set_text(propLabel, "Property");
    lv_obj_set_style_text_color(propLabel, m_theme.textPrimary(), 0);
    lv_obj_align(propLabel, LV_ALIGN_TOP_LEFT, 320, 30);

    m_actionPropertyDropdown = lv_dropdown_create(m_step4Container);
    lv_obj_set_size(m_actionPropertyDropdown, 150, 40);
    lv_obj_align(m_actionPropertyDropdown, LV_ALIGN_TOP_LEFT, 320, 52);
    lv_dropdown_set_options(m_actionPropertyDropdown, "on\nbrightness\ncolor");
    styleDropdown(m_actionPropertyDropdown);

    // Value input
    lv_obj_t* valueLabel = lv_label_create(m_step4Container);
    lv_label_set_text(valueLabel, "Set to value");
    lv_obj_set_style_text_color(valueLabel, m_theme.textPrimary(), 0);
    lv_obj_align(valueLabel, LV_ALIGN_TOP_LEFT, 490, 30);

    m_actionValueInput = lv_textarea_create(m_step4Container);
    lv_obj_set_size(m_actionValueInput, 150, 40);
    lv_obj_align(m_actionValueInput, LV_ALIGN_TOP_LEFT, 490, 52);
    lv_textarea_set_placeholder_text(m_actionValueInput, "true");
    lv_textarea_set_one_line(m_actionValueInput, true);
    lv_obj_set_style_bg_color(m_actionValueInput, m_theme.surface(), 0);
    lv_obj_set_style_text_color(m_actionValueInput, m_theme.textPrimary(), 0);
    if (lv_group_get_default()) {
        lv_group_add_obj(lv_group_get_default(), m_actionValueInput);
    }

    // Action list
    m_actionList = lv_obj_create(m_step4Container);
    lv_obj_set_size(m_actionList, LV_PCT(100), 150);
    lv_obj_align(m_actionList, LV_ALIGN_TOP_LEFT, 0, 110);
    lv_obj_set_style_bg_opa(m_actionList, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_actionList, 0, 0);
    lv_obj_set_flex_flow(m_actionList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_actionList, ThemeManager::SPACING_SM, 0);
}

void AddAutomationScreen::showStep(int step) {
    LOG_DEBUG("showStep: transitioning to step %d (was %d)", step, m_currentStep);
    m_currentStep = step;

    // Update title
    char title[64];
    snprintf(title, sizeof(title), "Add Automation (Step %d/%d)", step, TOTAL_STEPS);
    lv_label_set_text(m_titleLabel, title);

    // Hide all steps
    lv_obj_add_flag(m_step1Container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(m_step2Container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(m_step3Container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(m_step4Container, LV_OBJ_FLAG_HIDDEN);

    // Show current step
    switch (step) {
        case 1: lv_obj_clear_flag(m_step1Container, LV_OBJ_FLAG_HIDDEN); break;
        case 2:
            lv_obj_clear_flag(m_step2Container, LV_OBJ_FLAG_HIDDEN);
            showTriggerConfig(m_selectedTriggerType);
            break;
        case 3: lv_obj_clear_flag(m_step3Container, LV_OBJ_FLAG_HIDDEN); break;
        case 4: lv_obj_clear_flag(m_step4Container, LV_OBJ_FLAG_HIDDEN); break;
    }

    updateNavigationButtons();
}

void AddAutomationScreen::nextStep() {
    // Prevent re-entrancy during step transitions
    if (m_transitioning) {
        LOG_DEBUG("nextStep blocked (transition in progress)");
        return;
    }

    // Debounce rapid clicks to prevent skipping steps (class-level, shared with prevStep)
    uint32_t now = lv_tick_get();
    if (now - m_lastNavTime < 300) {
        LOG_DEBUG("nextStep debounced (called within 300ms of last nav)");
        return;
    }
    m_lastNavTime = now;

    LOG_DEBUG("nextStep: current=%d, advancing to %d", m_currentStep, m_currentStep + 1);

    if (m_currentStep < TOTAL_STEPS) {
        // Save current step data
        if (m_currentStep == 1) {
            m_automationName = lv_textarea_get_text(m_nameInput);
            m_automationDescription = lv_textarea_get_text(m_descInput);

            if (m_automationName.empty()) {
                // TODO: Show error
                LOG_WARN("Automation name is required");
                return;
            }
        }

        m_transitioning = true;
        showStep(m_currentStep + 1);
        m_transitioning = false;
    } else {
        // Create the automation
        onCreateAutomation();
    }
}

void AddAutomationScreen::prevStep() {
    // Prevent re-entrancy during step transitions
    if (m_transitioning) {
        LOG_DEBUG("prevStep blocked (transition in progress)");
        return;
    }

    // Debounce rapid clicks (class-level, shared with nextStep)
    uint32_t now = lv_tick_get();
    if (now - m_lastNavTime < 300) {
        LOG_DEBUG("prevStep debounced (called within 300ms of last nav)");
        return;
    }
    m_lastNavTime = now;

    if (m_currentStep > 1) {
        LOG_DEBUG("prevStep: current=%d, going back to %d", m_currentStep, m_currentStep - 1);
        m_transitioning = true;
        showStep(m_currentStep - 1);
        m_transitioning = false;
    }
}

void AddAutomationScreen::updateNavigationButtons() {
    // Update Previous button
    if (m_currentStep == 1) {
        lv_obj_add_flag(m_prevBtn, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(m_prevBtn, LV_OBJ_FLAG_HIDDEN);
    }

    // Update Next button text
    lv_obj_t* nextLabel = lv_obj_get_child(m_nextBtn, 0);
    if (m_currentStep == TOTAL_STEPS) {
        lv_label_set_text(nextLabel, "Create");
    } else {
        lv_label_set_text(nextLabel, "Next");
    }
}

void AddAutomationScreen::onCreateAutomation() {
    LOG_DEBUG("Creating automation: %s", m_automationName.c_str());

    Automation automation;
    automation.id = m_automationManager.generateId();
    automation.name = m_automationName;
    automation.description = m_automationDescription;
    automation.enabled = true;

    // Build trigger from form data
    Trigger trigger;
    trigger.type = m_selectedTriggerType;

    switch (m_selectedTriggerType) {
        case TriggerType::DeviceState:
            if (m_triggerDeviceDropdown && m_triggerPropertyDropdown) {
                int deviceIdx = lv_dropdown_get_selected(m_triggerDeviceDropdown);
                if (deviceIdx >= 0 && static_cast<size_t>(deviceIdx) < m_devices.size()) {
                    trigger.deviceId = m_devices[deviceIdx].first;
                }

                char propBuf[64];
                lv_dropdown_get_selected_str(m_triggerPropertyDropdown, propBuf, sizeof(propBuf));
                trigger.property = propBuf;

                if (m_triggerValueInput) {
                    const char* val = lv_textarea_get_text(m_triggerValueInput);
                    if (val && strlen(val) > 0) {
                        // Parse value
                        if (strcmp(val, "true") == 0) {
                            trigger.toValue = true;
                        } else if (strcmp(val, "false") == 0) {
                            trigger.toValue = false;
                        } else {
                            try {
                                trigger.toValue = std::stod(val);
                            } catch (...) {
                                trigger.toValue = val;
                            }
                        }
                    }
                }
            }
            break;

        case TriggerType::Time:
            if (m_triggerHourDropdown && m_triggerMinuteDropdown) {
                trigger.hour = lv_dropdown_get_selected(m_triggerHourDropdown);
                int minIdx = lv_dropdown_get_selected(m_triggerMinuteDropdown);
                trigger.minute = minIdx * 15;  // 0, 15, 30, 45
            }
            break;

        case TriggerType::TimeInterval:
            if (m_triggerIntervalInput) {
                const char* interval = lv_textarea_get_text(m_triggerIntervalInput);
                if (interval && strlen(interval) > 0) {
                    trigger.intervalMinutes = std::stoi(interval);
                }
            }
            break;

        case TriggerType::SensorThreshold:
            if (m_triggerDeviceDropdown && m_triggerOpDropdown && m_triggerThresholdInput) {
                int deviceIdx = lv_dropdown_get_selected(m_triggerDeviceDropdown);
                if (deviceIdx >= 0 && static_cast<size_t>(deviceIdx) < m_devices.size()) {
                    trigger.deviceId = m_devices[deviceIdx].first;
                }
                trigger.property = "value";  // Default sensor property

                int opIdx = lv_dropdown_get_selected(m_triggerOpDropdown);
                switch (opIdx) {
                    case 0: trigger.compareOp = CompareOp::GreaterThan; break;
                    case 1: trigger.compareOp = CompareOp::GreaterOrEqual; break;
                    case 2: trigger.compareOp = CompareOp::LessThan; break;
                    case 3: trigger.compareOp = CompareOp::LessOrEqual; break;
                    case 4: trigger.compareOp = CompareOp::Equal; break;
                }

                const char* thresh = lv_textarea_get_text(m_triggerThresholdInput);
                if (thresh && strlen(thresh) > 0) {
                    trigger.threshold = std::stod(thresh);
                }
            }
            break;
    }

    automation.triggers.push_back(trigger);

    // Build action from form data
    if (m_actionDeviceDropdown && m_actionPropertyDropdown && m_actionValueInput) {
        Action action;
        action.type = ActionType::SetDeviceState;

        int deviceIdx = lv_dropdown_get_selected(m_actionDeviceDropdown);
        if (deviceIdx >= 0 && static_cast<size_t>(deviceIdx) < m_devices.size()) {
            action.deviceId = m_devices[deviceIdx].first;
        }

        char propBuf[64];
        lv_dropdown_get_selected_str(m_actionPropertyDropdown, propBuf, sizeof(propBuf));
        action.property = propBuf;

        const char* val = lv_textarea_get_text(m_actionValueInput);
        if (val && strlen(val) > 0) {
            if (strcmp(val, "true") == 0) {
                action.value = true;
            } else if (strcmp(val, "false") == 0) {
                action.value = false;
            } else {
                try {
                    action.value = std::stod(val);
                } catch (...) {
                    action.value = val;
                }
            }
        }

        automation.actions.push_back(action);
    }

    // Add to manager
    if (m_automationManager.addAutomation(automation)) {
        LOG_INFO("Created automation: %s", automation.name.c_str());

        if (m_onAutomationAdded) {
            m_onAutomationAdded(std::make_shared<Automation>(automation));
        }

        m_screenManager.goBack();
    } else {
        LOG_ERROR("Failed to create automation");
        // TODO: Show error message
    }
}

void AddAutomationScreen::resetForm() {
    m_currentStep = 1;
    m_lastNavTime = 0;
    m_transitioning = false;
    m_automationName.clear();
    m_automationDescription.clear();
    m_selectedTriggerType = TriggerType::DeviceState;
    m_triggers.clear();
    m_condition.reset();
    m_actions.clear();

    if (m_nameInput) lv_textarea_set_text(m_nameInput, "");
    if (m_descInput) lv_textarea_set_text(m_descInput, "");
}

void AddAutomationScreen::populateDeviceDropdown(lv_obj_t* dropdown) {
    if (!dropdown) return;

    std::string options;
    for (size_t i = 0; i < m_devices.size(); ++i) {
        if (i > 0) options += "\n";
        options += m_devices[i].second;  // device name
    }

    if (options.empty()) {
        options = "No devices";
    }

    lv_dropdown_set_options(dropdown, options.c_str());
}

void AddAutomationScreen::styleDropdown(lv_obj_t* dropdown) {
    if (!dropdown) return;

    // Style the main dropdown button (closed state)
    lv_obj_set_style_bg_color(dropdown, m_theme.surface(), LV_PART_MAIN);
    lv_obj_set_style_text_color(dropdown, m_theme.textPrimary(), LV_PART_MAIN);
    lv_obj_set_style_border_color(dropdown, m_theme.divider(), LV_PART_MAIN);
    lv_obj_set_style_border_width(dropdown, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(dropdown, 8, LV_PART_MAIN);

    // Add callback to style the dropdown list when it opens
    lv_obj_add_event_cb(dropdown, [](lv_event_t* e) {
        lv_obj_t* dd = lv_event_get_target(e);
        lv_obj_t* list = lv_dropdown_get_list(dd);
        if (list) {
            auto* self = static_cast<AddAutomationScreen*>(lv_event_get_user_data(e));
            if (self) {
                lv_obj_set_style_bg_color(list, self->m_theme.surface(), LV_PART_MAIN);
                lv_obj_set_style_text_color(list, self->m_theme.textPrimary(), LV_PART_MAIN);
                lv_obj_set_style_border_color(list, self->m_theme.divider(), LV_PART_MAIN);
                lv_obj_set_style_border_width(list, 1, LV_PART_MAIN);
                lv_obj_set_style_max_height(list, 200, LV_PART_MAIN);
                // Selected item highlight
                lv_obj_set_style_bg_color(list, self->m_theme.primary(), LV_PART_SELECTED);
                lv_obj_set_style_text_color(list, lv_color_white(), LV_PART_SELECTED);
            }
        }
    }, LV_EVENT_READY, this);
}

void AddAutomationScreen::populatePropertyDropdown(lv_obj_t* dropdown, const std::string& deviceId) {
    (void)deviceId;  // Could be used to get device-specific properties
    if (!dropdown) return;

    // Default properties for now
    lv_dropdown_set_options(dropdown, "on\nbrightness\ncolor\ntemperature\nmotion");
}

void AddAutomationScreen::addCondition() {
    // TODO: Implement condition adding UI
}

void AddAutomationScreen::addAction() {
    // TODO: Implement multiple actions
}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
