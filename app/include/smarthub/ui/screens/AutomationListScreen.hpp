/**
 * Automation List Screen
 *
 * Shows all automations with enable/disable toggles.
 */
#pragma once

#include "smarthub/ui/Screen.hpp"
#include <memory>
#include <vector>
#include <string>

namespace smarthub {

class AutomationManager;

namespace ui {

class ThemeManager;

/**
 * Automation list screen showing all configured automations
 */
class AutomationListScreen : public Screen {
public:
    AutomationListScreen(ScreenManager& screenManager,
                         ThemeManager& theme,
                         AutomationManager& automationManager);
    ~AutomationListScreen() override;

    void onCreate() override;
    void onShow() override;
    void onUpdate(uint32_t deltaMs) override;
    void onDestroy() override;

private:
#ifdef SMARTHUB_ENABLE_LVGL
    void createHeader();
    void createContent();
    void refreshAutomationList();
    void createAddAutomationCard();

    lv_obj_t* createAutomationRow(lv_obj_t* parent,
                                   const std::string& automationId,
                                   const std::string& name,
                                   const std::string& triggerSummary,
                                   bool enabled,
                                   uint64_t lastTriggered);

    void onAutomationToggle(const std::string& automationId, bool enabled);
    void onAutomationClicked(const std::string& automationId);
    void onAddAutomationClicked();
    void onBackClicked();

    static void toggleHandler(lv_event_t* e);
    static void rowClickHandler(lv_event_t* e);

    std::string getTriggerSummary(const std::string& automationId);
    std::string formatLastTriggered(uint64_t timestamp);

    lv_obj_t* m_content = nullptr;
    lv_obj_t* m_automationList = nullptr;

    // Map automation ID to row widget for updates
    std::vector<std::pair<std::string, lv_obj_t*>> m_automationRows;
#endif

    ThemeManager& m_theme;
    AutomationManager& m_automationManager;
};

} // namespace ui
} // namespace smarthub
