/**
 * Add Automation Screen
 *
 * Multi-step wizard for creating automation rules.
 */
#pragma once

#include "smarthub/ui/Screen.hpp"
#include "smarthub/automation/Automation.hpp"
#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace smarthub {

class AutomationManager;
class DeviceManager;

namespace ui {

class ThemeManager;

/**
 * Add automation wizard screen
 *
 * Steps:
 * 1. Name and description
 * 2. Trigger configuration
 * 3. Conditions (optional)
 * 4. Actions
 */
class AddAutomationScreen : public Screen {
public:
    static constexpr int TOTAL_STEPS = 4;

    using AutomationAddedCallback = std::function<void(AutomationPtr)>;

    AddAutomationScreen(ScreenManager& screenManager,
                        ThemeManager& theme,
                        AutomationManager& automationManager,
                        DeviceManager& deviceManager);
    ~AddAutomationScreen() override;

    void onAutomationAdded(AutomationAddedCallback callback);

    void onCreate() override;
    void onShow() override;
    void onHide() override;
    void onUpdate(uint32_t deltaMs) override;
    void onDestroy() override;

private:
#ifdef SMARTHUB_ENABLE_LVGL
    void createHeader();
    void createContent();
    void createNavigationButtons();

    void showStep(int step);
    void nextStep();
    void prevStep();
    void updateNavigationButtons();

    // Step 1: Basic info
    void createStep1_BasicInfo();

    // Step 2: Triggers
    void createStep2_Triggers();
    void showTriggerConfig(TriggerType type);
    void createDeviceStateTrigger();
    void createTimeTrigger();
    void createIntervalTrigger();
    void createSensorThresholdTrigger();

    // Step 3: Conditions
    void createStep3_Conditions();
    void addCondition();

    // Step 4: Actions
    void createStep4_Actions();
    void addAction();

    void onCreateAutomation();
    void resetForm();
    void populateDeviceDropdown(lv_obj_t* dropdown);
    void populatePropertyDropdown(lv_obj_t* dropdown, const std::string& deviceId);
    void styleDropdown(lv_obj_t* dropdown);

    lv_obj_t* m_header = nullptr;
    lv_obj_t* m_backBtn = nullptr;
    lv_obj_t* m_titleLabel = nullptr;
    lv_obj_t* m_content = nullptr;

    lv_obj_t* m_step1Container = nullptr;
    lv_obj_t* m_step2Container = nullptr;
    lv_obj_t* m_step3Container = nullptr;
    lv_obj_t* m_step4Container = nullptr;

    lv_obj_t* m_prevBtn = nullptr;
    lv_obj_t* m_nextBtn = nullptr;

    // Step 1 widgets
    lv_obj_t* m_nameInput = nullptr;
    lv_obj_t* m_descInput = nullptr;

    // Step 2 widgets
    lv_obj_t* m_triggerTypeList = nullptr;
    lv_obj_t* m_triggerConfigContainer = nullptr;
    lv_obj_t* m_triggerDeviceDropdown = nullptr;
    lv_obj_t* m_triggerPropertyDropdown = nullptr;
    lv_obj_t* m_triggerValueInput = nullptr;
    lv_obj_t* m_triggerHourDropdown = nullptr;
    lv_obj_t* m_triggerMinuteDropdown = nullptr;
    lv_obj_t* m_triggerIntervalInput = nullptr;
    lv_obj_t* m_triggerOpDropdown = nullptr;
    lv_obj_t* m_triggerThresholdInput = nullptr;

    // Step 3 widgets
    lv_obj_t* m_conditionList = nullptr;

    // Step 4 widgets
    lv_obj_t* m_actionList = nullptr;
    lv_obj_t* m_actionDeviceDropdown = nullptr;
    lv_obj_t* m_actionPropertyDropdown = nullptr;
    lv_obj_t* m_actionValueInput = nullptr;
#endif

    ThemeManager& m_theme;
    AutomationManager& m_automationManager;
    DeviceManager& m_deviceManager;

    int m_currentStep = 1;
    uint32_t m_lastNavTime = 0;     // Class-level debounce timestamp
    bool m_transitioning = false;   // Prevent re-entrancy during step transitions

    // Form state
    std::string m_automationName;
    std::string m_automationDescription;
    TriggerType m_selectedTriggerType = TriggerType::DeviceState;
    std::vector<Trigger> m_triggers;
    std::optional<Condition> m_condition;
    std::vector<Action> m_actions;

    std::vector<std::pair<std::string, std::string>> m_devices;  // id, name

    AutomationAddedCallback m_onAutomationAdded;
};

} // namespace ui
} // namespace smarthub
