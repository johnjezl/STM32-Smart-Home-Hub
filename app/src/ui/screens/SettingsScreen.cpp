/**
 * Settings Screen Implementation
 */

#include "smarthub/ui/screens/SettingsScreen.hpp"
#include "smarthub/ui/ScreenManager.hpp"
#include "smarthub/ui/ThemeManager.hpp"
#include "smarthub/ui/widgets/Header.hpp"
#include "smarthub/core/Logger.hpp"

namespace smarthub {
namespace ui {

SettingsScreen::SettingsScreen(ScreenManager& screenManager,
                               ThemeManager& theme,
                               DeviceManager& deviceManager)
    : Screen(screenManager, "settings")
    , m_theme(theme)
    , m_deviceManager(deviceManager)
{
    // Initialize settings categories
    m_categories = {
        {"network", "Network", "WiFi, IP settings", LV_SYMBOL_WIFI},
        {"display", "Display", "Brightness, theme, timeout", LV_SYMBOL_IMAGE},
        {"devices", "Devices", "Manage paired devices", LV_SYMBOL_SETTINGS},
        {"security", "Security", "Password, API tokens", LV_SYMBOL_LOCK},
        {"about", "About", "Version, system info", LV_SYMBOL_LIST}
    };
}

SettingsScreen::~SettingsScreen() = default;

void SettingsScreen::onCreate() {
#ifdef SMARTHUB_ENABLE_LVGL
    if (!m_container) return;

    lv_obj_set_style_bg_color(m_container, m_theme.background(), 0);

    createHeader();
    createCategoryList();

    LOG_DEBUG("SettingsScreen created");
#endif
}

void SettingsScreen::onShow() {
    LOG_DEBUG("SettingsScreen shown");
}

void SettingsScreen::onUpdate(uint32_t deltaMs) {
    (void)deltaMs;
}

void SettingsScreen::onDestroy() {
    LOG_DEBUG("SettingsScreen destroyed");
}

#ifdef SMARTHUB_ENABLE_LVGL

void SettingsScreen::createHeader() {
    lv_obj_t* header = lv_obj_create(m_container);
    lv_obj_set_size(header, LV_PCT(100), Header::HEIGHT);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    m_theme.applyHeaderStyle(header);

    // Back button
    m_backBtn = lv_btn_create(header);
    lv_obj_set_size(m_backBtn, 40, 40);
    lv_obj_align(m_backBtn, LV_ALIGN_LEFT_MID, ThemeManager::SPACING_SM, 0);
    lv_obj_set_style_bg_opa(m_backBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(m_backBtn, 0, 0);
    lv_obj_add_event_cb(m_backBtn, backButtonHandler, LV_EVENT_CLICKED, this);

    lv_obj_t* backIcon = lv_label_create(m_backBtn);
    lv_label_set_text(backIcon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(backIcon, m_theme.textPrimary(), 0);
    lv_obj_center(backIcon);

    // Title
    m_titleLabel = lv_label_create(header);
    lv_label_set_text(m_titleLabel, "Settings");
    lv_obj_set_style_text_color(m_titleLabel, m_theme.textPrimary(), 0);
    lv_obj_align(m_titleLabel, LV_ALIGN_LEFT_MID, 60, 0);
}

void SettingsScreen::createCategoryList() {
    int topOffset = Header::HEIGHT;
    int height = 480 - topOffset;

    m_categoryList = lv_obj_create(m_container);
    lv_obj_set_size(m_categoryList, LV_PCT(100), height);
    lv_obj_align(m_categoryList, LV_ALIGN_TOP_MID, 0, topOffset);
    lv_obj_set_style_bg_opa(m_categoryList, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_categoryList, 0, 0);
    lv_obj_set_style_pad_all(m_categoryList, ThemeManager::SPACING_MD, 0);
    lv_obj_set_flex_flow(m_categoryList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_categoryList, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(m_categoryList, ThemeManager::SPACING_SM, 0);

    // Scrollable
    lv_obj_add_flag(m_categoryList, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(m_categoryList, LV_DIR_VER);

    // Create category items
    m_categoryItems.clear();
    for (const auto& category : m_categories) {
        lv_obj_t* item = createCategoryItem(m_categoryList, category);
        m_categoryItems.push_back(item);
    }
}

lv_obj_t* SettingsScreen::createCategoryItem(lv_obj_t* parent,
                                              const SettingsCategory& category) {
    lv_obj_t* item = lv_obj_create(parent);
    lv_obj_set_size(item, lv_pct(100) - 2 * ThemeManager::SPACING_MD, 70);
    m_theme.applyCardStyle(item);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

    // Make clickable
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(item, categoryClickHandler, LV_EVENT_CLICKED, this);

    // Store category ID
    char* idCopy = new char[category.id.length() + 1];
    strcpy(idCopy, category.id.c_str());
    lv_obj_set_user_data(item, idCopy);

    // Icon
    lv_obj_t* icon = lv_label_create(item);
    lv_label_set_text(icon, category.icon.c_str());
    lv_obj_set_style_text_color(icon, m_theme.primary(), 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);

    // Title
    lv_obj_t* title = lv_label_create(item);
    lv_label_set_text(title, category.title.c_str());
    lv_obj_set_style_text_color(title, m_theme.textPrimary(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 45, 8);

    // Subtitle
    lv_obj_t* subtitle = lv_label_create(item);
    lv_label_set_text(subtitle, category.subtitle.c_str());
    lv_obj_set_style_text_color(subtitle, m_theme.textSecondary(), 0);
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_12, 0);
    lv_obj_align(subtitle, LV_ALIGN_BOTTOM_LEFT, 45, -8);

    // Chevron
    lv_obj_t* chevron = lv_label_create(item);
    lv_label_set_text(chevron, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(chevron, m_theme.textSecondary(), 0);
    lv_obj_align(chevron, LV_ALIGN_RIGHT_MID, 0, 0);

    return item;
}

void SettingsScreen::onCategoryClicked(const std::string& categoryId) {
    LOG_DEBUG("Settings category clicked: %s", categoryId.c_str());

    if (categoryId == "network") {
        // Navigate to WiFi setup (already implemented)
        m_screenManager.showScreen("wifi_setup");
    } else if (categoryId == "display") {
        m_screenManager.showScreen("display_settings");
    } else if (categoryId == "devices") {
        m_screenManager.showScreen("devices");
    } else if (categoryId == "security") {
        // Security settings - future implementation
        LOG_DEBUG("Security settings not yet implemented");
    } else if (categoryId == "about") {
        m_screenManager.showScreen("about");
    }
}

void SettingsScreen::onBackClicked() {
    m_screenManager.goBack();
}

void SettingsScreen::categoryClickHandler(lv_event_t* e) {
    SettingsScreen* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    lv_obj_t* item = lv_event_get_target(e);

    if (self && item) {
        char* categoryId = static_cast<char*>(lv_obj_get_user_data(item));
        if (categoryId) {
            self->onCategoryClicked(categoryId);
        }
    }
}

void SettingsScreen::backButtonHandler(lv_event_t* e) {
    SettingsScreen* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (self) {
        self->onBackClicked();
    }
}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
