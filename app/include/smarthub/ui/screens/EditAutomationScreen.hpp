/**
 * Edit Automation Screen
 *
 * View and edit an existing automation rule.
 */
#pragma once

#include "smarthub/ui/Screen.hpp"
#include <memory>
#include <string>
#include <functional>

namespace smarthub {

class AutomationManager;

namespace ui {

class ThemeManager;

/**
 * Edit automation screen
 */
class EditAutomationScreen : public Screen {
public:
    using UpdatedCallback = std::function<void()>;
    using DeletedCallback = std::function<void()>;

    EditAutomationScreen(ScreenManager& screenManager,
                         ThemeManager& theme,
                         AutomationManager& automationManager);
    ~EditAutomationScreen() override;

    void setAutomationId(const std::string& id);

    void onAutomationUpdated(UpdatedCallback callback);
    void onAutomationDeleted(DeletedCallback callback);

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

    void onSaveClicked();
    void onDeleteClicked();
    void showDeleteConfirmation();

    lv_obj_t* m_header = nullptr;
    lv_obj_t* m_content = nullptr;

    lv_obj_t* m_nameInput = nullptr;
    lv_obj_t* m_descInput = nullptr;
    lv_obj_t* m_enabledSwitch = nullptr;
    lv_obj_t* m_triggerLabel = nullptr;
    lv_obj_t* m_actionLabel = nullptr;

    lv_obj_t* m_saveBtn = nullptr;
    lv_obj_t* m_deleteBtn = nullptr;
#endif

    ThemeManager& m_theme;
    AutomationManager& m_automationManager;

    std::string m_automationId;

    UpdatedCallback m_onUpdated;
    DeletedCallback m_onDeleted;
};

} // namespace ui
} // namespace smarthub
