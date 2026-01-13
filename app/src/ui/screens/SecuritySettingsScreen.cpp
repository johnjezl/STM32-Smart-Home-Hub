/**
 * Security Settings Screen Implementation
 */

#include "smarthub/ui/screens/SecuritySettingsScreen.hpp"
#include "smarthub/ui/ScreenManager.hpp"
#include "smarthub/ui/ThemeManager.hpp"
#include "smarthub/ui/widgets/Header.hpp"
#include "smarthub/core/Logger.hpp"

namespace smarthub {
namespace ui {

SecuritySettingsScreen::SecuritySettingsScreen(ScreenManager& screenManager,
                                               ThemeManager& theme)
    : Screen(screenManager, "security")
    , m_theme(theme)
{
}

SecuritySettingsScreen::~SecuritySettingsScreen() = default;

void SecuritySettingsScreen::onCreate() {
#ifdef SMARTHUB_ENABLE_LVGL
    if (!m_container) return;

    lv_obj_set_style_bg_color(m_container, m_theme.background(), 0);
    createHeader();
    createContent();

    LOG_DEBUG("SecuritySettingsScreen created");
#endif
}

void SecuritySettingsScreen::onShow() {
    LOG_DEBUG("SecuritySettingsScreen shown");
}

void SecuritySettingsScreen::onHide() {
    LOG_DEBUG("SecuritySettingsScreen hidden");
}

void SecuritySettingsScreen::onUpdate(uint32_t /* deltaMs */) {
}

void SecuritySettingsScreen::onDestroy() {
    LOG_DEBUG("SecuritySettingsScreen destroyed");
}

#ifdef SMARTHUB_ENABLE_LVGL

void SecuritySettingsScreen::createHeader() {
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
    lv_obj_add_event_cb(m_backBtn, backButtonHandler, LV_EVENT_CLICKED, this);

    lv_obj_t* backIcon = lv_label_create(m_backBtn);
    lv_label_set_text(backIcon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(backIcon, m_theme.textPrimary(), 0);
    lv_obj_center(backIcon);

    // Title
    lv_obj_t* title = lv_label_create(m_header);
    lv_label_set_text(title, "Security");
    lv_obj_set_style_text_color(title, m_theme.textPrimary(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
}

void SecuritySettingsScreen::createContent() {
    constexpr int CONTENT_HEIGHT = 480 - Header::HEIGHT;

    m_content = lv_obj_create(m_container);
    lv_obj_set_size(m_content, LV_PCT(100), CONTENT_HEIGHT);
    lv_obj_align(m_content, LV_ALIGN_TOP_MID, 0, Header::HEIGHT);
    lv_obj_set_style_bg_opa(m_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_content, 0, 0);
    lv_obj_set_style_pad_all(m_content, ThemeManager::SPACING_MD, 0);
    lv_obj_set_flex_flow(m_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_content, ThemeManager::SPACING_SM, 0);

    createOptionsList();
}

void SecuritySettingsScreen::createOptionsList() {
    struct SecurityOption {
        const char* icon;
        const char* title;
        const char* subtitle;
        const char* id;
    };

    const SecurityOption options[] = {
        {LV_SYMBOL_EDIT, "Change Password", "Update your account password", "password"},
        {LV_SYMBOL_LIST, "API Tokens", "Manage API access tokens", "tokens"},
        {LV_SYMBOL_REFRESH, "Active Sessions", "View and manage logged-in sessions", "sessions"},
        {LV_SYMBOL_SETTINGS, "Two-Factor Auth", "Enable 2FA for extra security", "2fa"},
    };

    for (const auto& opt : options) {
        lv_obj_t* item = lv_obj_create(m_content);
        lv_obj_set_size(item, LV_PCT(100), 70);
        lv_obj_set_style_bg_color(item, m_theme.surface(), 0);
        lv_obj_set_style_radius(item, ThemeManager::CARD_RADIUS, 0);
        lv_obj_set_style_pad_hor(item, ThemeManager::SPACING_MD, 0);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);

        // Store option ID
        char* idCopy = new char[strlen(opt.id) + 1];
        strcpy(idCopy, opt.id);
        lv_obj_set_user_data(item, idCopy);
        lv_obj_add_event_cb(item, [](lv_event_t* e) {
            delete[] static_cast<char*>(lv_obj_get_user_data(lv_event_get_target(e)));
        }, LV_EVENT_DELETE, nullptr);
        lv_obj_add_event_cb(item, optionClickHandler, LV_EVENT_CLICKED, this);

        // Icon
        lv_obj_t* icon = lv_label_create(item);
        lv_label_set_text(icon, opt.icon);
        lv_obj_set_style_text_color(icon, m_theme.primary(), 0);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);

        // Title
        lv_obj_t* title = lv_label_create(item);
        lv_label_set_text(title, opt.title);
        lv_obj_set_style_text_color(title, m_theme.textPrimary(), 0);
        lv_obj_align(title, LV_ALIGN_LEFT_MID, 45, -12);

        // Subtitle
        lv_obj_t* subtitle = lv_label_create(item);
        lv_label_set_text(subtitle, opt.subtitle);
        lv_obj_set_style_text_color(subtitle, m_theme.textSecondary(), 0);
        lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_12, 0);
        lv_obj_align(subtitle, LV_ALIGN_LEFT_MID, 45, 10);

        // Chevron
        lv_obj_t* chevron = lv_label_create(item);
        lv_label_set_text(chevron, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(chevron, m_theme.textSecondary(), 0);
        lv_obj_align(chevron, LV_ALIGN_RIGHT_MID, 0, 0);
    }
}

void SecuritySettingsScreen::onBackClicked() {
    m_screenManager.goBack();
}

void SecuritySettingsScreen::onChangePassword() {
    // TODO: Show password change dialog
    LOG_DEBUG("Change password clicked");
}

void SecuritySettingsScreen::onManageTokens() {
    // TODO: Show API token management screen
    LOG_DEBUG("Manage tokens clicked");
}

void SecuritySettingsScreen::backButtonHandler(lv_event_t* e) {
    SecuritySettingsScreen* self = static_cast<SecuritySettingsScreen*>(lv_event_get_user_data(e));
    if (self) {
        self->onBackClicked();
    }
}

void SecuritySettingsScreen::optionClickHandler(lv_event_t* e) {
    SecuritySettingsScreen* self = static_cast<SecuritySettingsScreen*>(lv_event_get_user_data(e));
    lv_obj_t* item = lv_event_get_target(e);
    const char* optionId = static_cast<const char*>(lv_obj_get_user_data(item));

    if (self && optionId) {
        LOG_DEBUG("Security option clicked: %s", optionId);

        if (strcmp(optionId, "password") == 0) {
            self->onChangePassword();
        } else if (strcmp(optionId, "tokens") == 0) {
            self->onManageTokens();
        }
        // Other options are placeholders for now
    }
}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
