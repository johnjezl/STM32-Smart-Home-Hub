/**
 * Settings Screen
 *
 * Main settings menu with categories for configuration.
 */
#pragma once

#include "smarthub/ui/Screen.hpp"
#include <memory>
#include <string>
#include <vector>

namespace smarthub {

class DeviceManager;

namespace ui {

class ThemeManager;

/**
 * Settings category item
 */
struct SettingsCategory {
    std::string id;
    std::string title;
    std::string subtitle;
    std::string icon;
};

/**
 * Settings screen with category list
 *
 * Layout (800x480):
 * +---------------------------------------------------------------+
 * |  < Settings                                                   |
 * +---------------------------------------------------------------+
 * |  +----------------------------------------------------------+ |
 * |  | 📶  Network                                              | |
 * |  |     WiFi, IP settings                                    | |
 * |  +----------------------------------------------------------+ |
 * |  | 🔆  Display                                              | |
 * |  |     Brightness, theme, timeout                           | |
 * |  +----------------------------------------------------------+ |
 * |  | 📱  Devices                                              | |
 * |  |     Manage paired devices                                | |
 * |  +----------------------------------------------------------+ |
 * |  | 🔒  Security                                             | |
 * |  |     Password, API tokens                                 | |
 * |  +----------------------------------------------------------+ |
 * |  | ℹ️  About                                                 | |
 * |  |     Version, system info                                 | |
 * |  +----------------------------------------------------------+ |
 * +---------------------------------------------------------------+
 */
class SettingsScreen : public Screen {
public:
    SettingsScreen(ScreenManager& screenManager,
                   ThemeManager& theme,
                   DeviceManager& deviceManager);
    ~SettingsScreen() override;

    void onCreate() override;
    void onShow() override;
    void onUpdate(uint32_t deltaMs) override;
    void onDestroy() override;

private:
#ifdef SMARTHUB_ENABLE_LVGL
    void createHeader();
    void createCategoryList();
    lv_obj_t* createCategoryItem(lv_obj_t* parent, const SettingsCategory& category);

    void onCategoryClicked(const std::string& categoryId);
    void onBackClicked();

    static void categoryClickHandler(lv_event_t* e);
    static void backButtonHandler(lv_event_t* e);

    lv_obj_t* m_backBtn = nullptr;
    lv_obj_t* m_titleLabel = nullptr;
    lv_obj_t* m_categoryList = nullptr;

    std::vector<lv_obj_t*> m_categoryItems;
#endif

    ThemeManager& m_theme;
    DeviceManager& m_deviceManager;

    std::vector<SettingsCategory> m_categories;
};

} // namespace ui
} // namespace smarthub
