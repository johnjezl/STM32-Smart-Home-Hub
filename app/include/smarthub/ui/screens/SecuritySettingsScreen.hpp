/**
 * Security Settings Screen
 *
 * Manages security-related settings like users, passwords, and API tokens.
 */
#pragma once

#include "smarthub/ui/Screen.hpp"
#include <memory>
#include <string>

namespace smarthub {
namespace ui {

class ThemeManager;

/**
 * Security settings screen for user and token management
 */
class SecuritySettingsScreen : public Screen {
public:
    SecuritySettingsScreen(ScreenManager& screenManager,
                           ThemeManager& theme);
    ~SecuritySettingsScreen() override;

    void onCreate() override;
    void onShow() override;
    void onHide() override;
    void onUpdate(uint32_t deltaMs) override;
    void onDestroy() override;

private:
#ifdef SMARTHUB_ENABLE_LVGL
    void createHeader();
    void createContent();
    void createOptionsList();

    void onBackClicked();
    void onChangePassword();
    void onManageTokens();

    static void backButtonHandler(lv_event_t* e);
    static void optionClickHandler(lv_event_t* e);

    lv_obj_t* m_header = nullptr;
    lv_obj_t* m_backBtn = nullptr;
    lv_obj_t* m_content = nullptr;
    lv_obj_t* m_optionsList = nullptr;
#endif

    ThemeManager& m_theme;
};

} // namespace ui
} // namespace smarthub
