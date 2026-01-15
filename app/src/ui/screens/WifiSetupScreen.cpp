/**
 * WiFi Setup Screen Implementation
 */

#include "smarthub/ui/screens/WifiSetupScreen.hpp"
#include "smarthub/ui/ScreenManager.hpp"
#include "smarthub/ui/ThemeManager.hpp"
#include "smarthub/ui/widgets/Header.hpp"
#include "smarthub/core/Logger.hpp"

namespace smarthub {
namespace ui {

WifiSetupScreen::WifiSetupScreen(ScreenManager& screenManager,
                                 ThemeManager& theme,
                                 network::NetworkManager& networkManager)
    : Screen(screenManager, "wifi_setup")
    , m_theme(theme)
    , m_networkManager(networkManager)
{
}

WifiSetupScreen::~WifiSetupScreen() = default;

void WifiSetupScreen::onCreate() {
#ifdef SMARTHUB_ENABLE_LVGL
    if (!m_container) return;

    lv_obj_set_style_bg_color(m_container, m_theme.background(), 0);

    createHeader();
    createStatusBar();
    createNetworkList();
    createPasswordDialog();
    createLoadingOverlay();

    LOG_DEBUG("WifiSetupScreen created");
#endif
}

void WifiSetupScreen::onShow() {
#ifdef SMARTHUB_ENABLE_LVGL
    updateStatusBar();
    refreshNetworks();
    LOG_DEBUG("WifiSetupScreen shown");
#endif
}

void WifiSetupScreen::onHide() {
#ifdef SMARTHUB_ENABLE_LVGL
    hidePasswordDialog();
    hideLoading();
#endif
}

void WifiSetupScreen::onUpdate(uint32_t deltaMs) {
#ifdef SMARTHUB_ENABLE_LVGL
    m_refreshMs += deltaMs;

    // Auto-refresh periodically
    if (m_refreshMs >= AUTO_REFRESH_INTERVAL && !m_scanning) {
        m_refreshMs = 0;
        updateStatusBar();
    }
#else
    (void)deltaMs;
#endif
}

void WifiSetupScreen::onDestroy() {
    LOG_DEBUG("WifiSetupScreen destroyed");
}

#ifdef SMARTHUB_ENABLE_LVGL

void WifiSetupScreen::createHeader() {
    lv_obj_t* header = lv_obj_create(m_container);
    lv_obj_set_size(header, LV_PCT(100), Header::HEIGHT);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    m_theme.applyHeaderStyle(header);

    // Back button
    m_backBtn = lv_btn_create(header);
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
    m_titleLabel = lv_label_create(header);
    lv_label_set_text(m_titleLabel, "WiFi Setup");
    lv_obj_set_style_text_color(m_titleLabel, m_theme.textPrimary(), 0);
    lv_obj_align(m_titleLabel, LV_ALIGN_LEFT_MID, 60, 0);

    // Refresh button
    m_refreshBtn = lv_btn_create(header);
    lv_obj_set_size(m_refreshBtn, 48, 48);
    lv_obj_align(m_refreshBtn, LV_ALIGN_RIGHT_MID, -ThemeManager::SPACING_SM, 0);
    lv_obj_set_style_bg_opa(m_refreshBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(m_refreshBtn, 0, 0);
    lv_obj_add_event_cb(m_refreshBtn, refreshButtonHandler, LV_EVENT_CLICKED, this);

    lv_obj_t* refreshIcon = lv_label_create(m_refreshBtn);
    lv_label_set_text(refreshIcon, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_color(refreshIcon, m_theme.textPrimary(), 0);
    lv_obj_center(refreshIcon);
}

void WifiSetupScreen::createStatusBar() {
    m_statusBar = lv_obj_create(m_container);
    lv_obj_set_size(m_statusBar, LV_PCT(100), 50);
    lv_obj_align(m_statusBar, LV_ALIGN_TOP_MID, 0, Header::HEIGHT);
    lv_obj_set_style_bg_color(m_statusBar, m_theme.surfaceVariant(), 0);
    lv_obj_set_style_border_width(m_statusBar, 0, 0);
    lv_obj_set_style_pad_hor(m_statusBar, ThemeManager::SPACING_MD, 0);
    lv_obj_clear_flag(m_statusBar, LV_OBJ_FLAG_SCROLLABLE);

    m_statusIcon = lv_label_create(m_statusBar);
    lv_label_set_text(m_statusIcon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(m_statusIcon, m_theme.textSecondary(), 0);
    lv_obj_align(m_statusIcon, LV_ALIGN_LEFT_MID, 0, 0);

    m_statusLabel = lv_label_create(m_statusBar);
    lv_label_set_text(m_statusLabel, "Not connected");
    lv_obj_set_style_text_color(m_statusLabel, m_theme.textSecondary(), 0);
    lv_obj_align(m_statusLabel, LV_ALIGN_LEFT_MID, 30, 0);
}

void WifiSetupScreen::createNetworkList() {
    int topOffset = Header::HEIGHT + 50;
    int height = 480 - topOffset;  // Remaining height

    m_networkList = lv_obj_create(m_container);
    lv_obj_set_size(m_networkList, LV_PCT(100), height);
    lv_obj_align(m_networkList, LV_ALIGN_TOP_MID, 0, topOffset);
    lv_obj_set_style_bg_opa(m_networkList, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_networkList, 0, 0);
    lv_obj_set_style_pad_all(m_networkList, ThemeManager::SPACING_SM, 0);
    lv_obj_set_flex_flow(m_networkList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_networkList, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(m_networkList, ThemeManager::SPACING_SM, 0);

    // Scrollable
    lv_obj_add_flag(m_networkList, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(m_networkList, LV_DIR_VER);
}

void WifiSetupScreen::createPasswordDialog() {
    // Overlay
    m_dialogOverlay = lv_obj_create(m_container);
    lv_obj_set_size(m_dialogOverlay, LV_PCT(100), LV_PCT(100));
    lv_obj_align(m_dialogOverlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(m_dialogOverlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(m_dialogOverlay, LV_OPA_50, 0);
    lv_obj_clear_flag(m_dialogOverlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(m_dialogOverlay, LV_OBJ_FLAG_HIDDEN);

    // Dialog box
    m_dialog = lv_obj_create(m_dialogOverlay);
    lv_obj_set_size(m_dialog, 400, 280);
    lv_obj_align(m_dialog, LV_ALIGN_TOP_MID, 0, 30);
    m_theme.applyCardStyle(m_dialog);
    lv_obj_clear_flag(m_dialog, LV_OBJ_FLAG_SCROLLABLE);

    // Dialog title
    m_dialogTitle = lv_label_create(m_dialog);
    lv_label_set_text(m_dialogTitle, "Enter Password");
    lv_obj_set_style_text_color(m_dialogTitle, m_theme.textPrimary(), 0);
    lv_obj_align(m_dialogTitle, LV_ALIGN_TOP_MID, 0, 10);

    // Password input
    m_passwordInput = lv_textarea_create(m_dialog);
    lv_obj_set_size(m_passwordInput, 360, 45);
    lv_obj_align(m_passwordInput, LV_ALIGN_TOP_MID, 0, 50);
    lv_textarea_set_placeholder_text(m_passwordInput, "Password");
    lv_textarea_set_password_mode(m_passwordInput, true);
    lv_textarea_set_one_line(m_passwordInput, true);
    lv_obj_set_style_border_color(m_passwordInput, m_theme.divider(), 0);
    lv_obj_set_style_border_width(m_passwordInput, 1, 0);
    lv_obj_set_style_bg_color(m_passwordInput, m_theme.surface(), 0);
    lv_obj_set_style_text_color(m_passwordInput, m_theme.textPrimary(), 0);
    // Disable scroll-on-focus to prevent screen shifting
    lv_obj_clear_flag(m_passwordInput, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    // Add to keyboard input group
    if (lv_group_get_default()) {
        lv_group_add_obj(lv_group_get_default(), m_passwordInput);
    }

    // Show password checkbox
    m_showPasswordCheckbox = lv_checkbox_create(m_dialog);
    lv_checkbox_set_text(m_showPasswordCheckbox, "Show password");
    lv_obj_set_style_text_color(m_showPasswordCheckbox, m_theme.textSecondary(), 0);
    lv_obj_align(m_showPasswordCheckbox, LV_ALIGN_TOP_LEFT, 20, 105);
    lv_obj_add_event_cb(m_showPasswordCheckbox, [](lv_event_t* e) {
        WifiSetupScreen* self = static_cast<WifiSetupScreen*>(lv_event_get_user_data(e));
        if (self && self->m_passwordInput) {
            bool checked = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
            lv_textarea_set_password_mode(self->m_passwordInput, !checked);
        }
    }, LV_EVENT_VALUE_CHANGED, this);

    // Error label
    m_errorLabel = lv_label_create(m_dialog);
    lv_label_set_text(m_errorLabel, "");
    lv_obj_set_style_text_color(m_errorLabel, m_theme.error(), 0);
    lv_obj_align(m_errorLabel, LV_ALIGN_TOP_MID, 0, 135);
    lv_obj_add_flag(m_errorLabel, LV_OBJ_FLAG_HIDDEN);

    // Buttons
    m_cancelBtn = lv_btn_create(m_dialog);
    lv_obj_set_size(m_cancelBtn, 100, 40);
    lv_obj_align(m_cancelBtn, LV_ALIGN_BOTTOM_LEFT, 20, -15);
    lv_obj_set_style_bg_color(m_cancelBtn, m_theme.surfaceVariant(), 0);
    lv_obj_add_event_cb(m_cancelBtn, cancelButtonHandler, LV_EVENT_CLICKED, this);

    lv_obj_t* cancelLabel = lv_label_create(m_cancelBtn);
    lv_label_set_text(cancelLabel, "Cancel");
    lv_obj_set_style_text_color(cancelLabel, m_theme.textPrimary(), 0);
    lv_obj_center(cancelLabel);

    m_connectBtn = lv_btn_create(m_dialog);
    lv_obj_set_size(m_connectBtn, 100, 40);
    lv_obj_align(m_connectBtn, LV_ALIGN_BOTTOM_RIGHT, -20, -15);
    lv_obj_set_style_bg_color(m_connectBtn, m_theme.primary(), 0);
    lv_obj_add_event_cb(m_connectBtn, connectButtonHandler, LV_EVENT_CLICKED, this);

    lv_obj_t* connectLabel = lv_label_create(m_connectBtn);
    lv_label_set_text(connectLabel, "Connect");
    lv_obj_set_style_text_color(connectLabel, lv_color_white(), 0);
    lv_obj_center(connectLabel);

    // Keyboard
    m_keyboard = lv_keyboard_create(m_dialogOverlay);
    lv_obj_set_size(m_keyboard, LV_PCT(100), 180);
    lv_obj_align(m_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(m_keyboard, m_passwordInput);
    lv_obj_add_event_cb(m_keyboard, keyboardHandler, LV_EVENT_READY, this);
    lv_obj_add_event_cb(m_keyboard, cancelButtonHandler, LV_EVENT_CANCEL, this);
}

void WifiSetupScreen::createLoadingOverlay() {
    m_loadingOverlay = lv_obj_create(m_container);
    lv_obj_set_size(m_loadingOverlay, LV_PCT(100), LV_PCT(100));
    lv_obj_align(m_loadingOverlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(m_loadingOverlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(m_loadingOverlay, LV_OPA_70, 0);
    lv_obj_clear_flag(m_loadingOverlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(m_loadingOverlay, LV_OBJ_FLAG_HIDDEN);

    // Spinner
    m_loadingSpinner = lv_spinner_create(m_loadingOverlay, 1000, 60);
    lv_obj_set_size(m_loadingSpinner, 60, 60);
    lv_obj_align(m_loadingSpinner, LV_ALIGN_CENTER, 0, -20);

    // Label
    m_loadingLabel = lv_label_create(m_loadingOverlay);
    lv_label_set_text(m_loadingLabel, "Connecting...");
    lv_obj_set_style_text_color(m_loadingLabel, lv_color_white(), 0);
    lv_obj_align(m_loadingLabel, LV_ALIGN_CENTER, 0, 40);
}

void WifiSetupScreen::updateStatusBar() {
    auto status = m_networkManager.getStatus();

    if (!m_statusLabel || !m_statusIcon) return;

    switch (status.state) {
        case network::ConnectionState::Connected:
            lv_obj_set_style_text_color(m_statusIcon, m_theme.success(), 0);
            if (!status.ipAddress.empty()) {
                std::string text = "Connected to \"" + status.ssid +
                                   "\" (" + status.ipAddress + ")";
                lv_label_set_text(m_statusLabel, text.c_str());
            } else {
                std::string text = "Connected to \"" + status.ssid + "\"";
                lv_label_set_text(m_statusLabel, text.c_str());
            }
            lv_obj_set_style_text_color(m_statusLabel, m_theme.textPrimary(), 0);
            break;

        case network::ConnectionState::Connecting:
            lv_obj_set_style_text_color(m_statusIcon, m_theme.warning(), 0);
            lv_label_set_text(m_statusLabel, ("Connecting to \"" + status.ssid + "\"...").c_str());
            lv_obj_set_style_text_color(m_statusLabel, m_theme.textSecondary(), 0);
            break;

        case network::ConnectionState::Failed:
            lv_obj_set_style_text_color(m_statusIcon, m_theme.error(), 0);
            lv_label_set_text(m_statusLabel, status.error.c_str());
            lv_obj_set_style_text_color(m_statusLabel, m_theme.error(), 0);
            break;

        case network::ConnectionState::Scanning:
            lv_obj_set_style_text_color(m_statusIcon, m_theme.textSecondary(), 0);
            lv_label_set_text(m_statusLabel, "Scanning for networks...");
            lv_obj_set_style_text_color(m_statusLabel, m_theme.textSecondary(), 0);
            break;

        case network::ConnectionState::Disconnected:
        default:
            lv_obj_set_style_text_color(m_statusIcon, m_theme.textSecondary(), 0);
            lv_label_set_text(m_statusLabel, "Not connected");
            lv_obj_set_style_text_color(m_statusLabel, m_theme.textSecondary(), 0);
            break;
    }
}

void WifiSetupScreen::updateNetworkList() {
    if (!m_networkList) return;

    // Clear existing items
    for (auto* item : m_networkItems) {
        lv_obj_del(item);
    }
    m_networkItems.clear();

    // Get current status for connected network
    auto status = m_networkManager.getStatus();

    // Create new items
    for (const auto& network : m_networks) {
        lv_obj_t* item = lv_obj_create(m_networkList);
        lv_obj_set_size(item, lv_pct(100) - 2 * ThemeManager::SPACING_SM, 60);
        m_theme.applyCardStyle(item);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

        // Make clickable
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(item, networkItemHandler, LV_EVENT_CLICKED, this);

        // Store SSID as user data
        char* ssidCopy = new char[network.ssid.length() + 1];
        strcpy(ssidCopy, network.ssid.c_str());
        lv_obj_set_user_data(item, ssidCopy);
        lv_obj_add_event_cb(item, [](lv_event_t* e) {
            delete[] static_cast<char*>(lv_obj_get_user_data(lv_event_get_target(e)));
        }, LV_EVENT_DELETE, nullptr);

        // Signal icon
        int signalLevel = network::NetworkManager::signalToIconIndex(network.signalStrength);
        lv_obj_t* signalIcon = lv_label_create(item);
        lv_label_set_text(signalIcon, getSignalIcon(signalLevel));
        lv_obj_set_style_text_color(signalIcon, m_theme.textSecondary(), 0);
        lv_obj_align(signalIcon, LV_ALIGN_LEFT_MID, 0, 0);

        // Network name
        lv_obj_t* nameLabel = lv_label_create(item);
        lv_label_set_text(nameLabel, network.ssid.c_str());
        lv_obj_set_style_text_color(nameLabel, m_theme.textPrimary(), 0);
        lv_obj_align(nameLabel, LV_ALIGN_LEFT_MID, 35, 0);
        lv_label_set_long_mode(nameLabel, LV_LABEL_LONG_DOT);
        lv_obj_set_width(nameLabel, 250);

        // Status or security label
        lv_obj_t* statusLabel = lv_label_create(item);
        if (status.state == network::ConnectionState::Connected &&
            status.ssid == network.ssid) {
            lv_label_set_text(statusLabel, "Connected");
            lv_obj_set_style_text_color(statusLabel, m_theme.success(), 0);
        } else if (network.secured) {
            lv_label_set_text(statusLabel, network.security.c_str());
            lv_obj_set_style_text_color(statusLabel, m_theme.textSecondary(), 0);
        } else {
            lv_label_set_text(statusLabel, "Open");
            lv_obj_set_style_text_color(statusLabel, m_theme.textSecondary(), 0);
        }
        lv_obj_align(statusLabel, LV_ALIGN_RIGHT_MID, 0, 0);

        // Lock icon for secured networks
        if (network.secured && !(status.state == network::ConnectionState::Connected &&
                                  status.ssid == network.ssid)) {
            lv_obj_t* lockIcon = lv_label_create(item);
            lv_label_set_text(lockIcon, LV_SYMBOL_EYE_CLOSE);  // Lock icon substitute
            lv_obj_set_style_text_color(lockIcon, m_theme.textSecondary(), 0);
            lv_obj_align(lockIcon, LV_ALIGN_RIGHT_MID, -60, 0);
        }

        m_networkItems.push_back(item);
    }
}

void WifiSetupScreen::refreshNetworks() {
    m_scanning = true;
    showLoading("Scanning for networks...");

    m_networkManager.startScan([this](const std::vector<network::WifiNetwork>& networks) {
        m_networks = networks;
        m_scanning = false;

        hideLoading();
        updateStatusBar();
        updateNetworkList();
    });
}

void WifiSetupScreen::onNetworkSelected(const network::WifiNetwork& network) {
    m_selectedSsid = network.ssid;

    auto status = m_networkManager.getStatus();

    // If already connected to this network, disconnect
    if (status.state == network::ConnectionState::Connected &&
        status.ssid == network.ssid) {
        m_networkManager.disconnect();
        updateStatusBar();
        updateNetworkList();
        return;
    }

    // If open network, connect directly
    if (!network.secured) {
        showLoading("Connecting...");
        m_networkManager.connect(network.ssid, "", [this](const network::ConnectionResult& result) {
            hideLoading();
            updateStatusBar();
            updateNetworkList();

            if (!result.success) {
                showError(result.error);
            }
        });
        return;
    }

    // Show password dialog for secured networks
    showPasswordDialog(network.ssid);
}

void WifiSetupScreen::onConnectClicked() {
    if (!m_passwordInput) return;

    const char* password = lv_textarea_get_text(m_passwordInput);
    if (!password || strlen(password) == 0) {
        showError("Please enter a password");
        return;
    }

    hidePasswordDialog();
    showLoading("Connecting...");

    m_networkManager.connect(m_selectedSsid, password, [this](const network::ConnectionResult& result) {
        hideLoading();
        updateStatusBar();
        updateNetworkList();

        if (!result.success) {
            showError(result.error);
        }
    });
}

void WifiSetupScreen::onCancelClicked() {
    hidePasswordDialog();
}

void WifiSetupScreen::onPasswordEntered(const std::string& password) {
    (void)password;
    onConnectClicked();
}

void WifiSetupScreen::showPasswordDialog(const std::string& ssid) {
    if (!m_dialogOverlay || !m_dialog || !m_dialogTitle || !m_passwordInput) return;

    std::string title = "Connect to \"" + ssid + "\"";
    lv_label_set_text(m_dialogTitle, title.c_str());
    lv_textarea_set_text(m_passwordInput, "");
    lv_obj_clear_state(m_showPasswordCheckbox, LV_STATE_CHECKED);
    lv_textarea_set_password_mode(m_passwordInput, true);
    lv_obj_add_flag(m_errorLabel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_clear_flag(m_dialogOverlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(m_dialogOverlay);

    // Focus on password input
    lv_group_focus_obj(m_passwordInput);
}

void WifiSetupScreen::hidePasswordDialog() {
    if (m_dialogOverlay) {
        lv_obj_add_flag(m_dialogOverlay, LV_OBJ_FLAG_HIDDEN);
    }
}

void WifiSetupScreen::showLoading(const std::string& message) {
    if (!m_loadingOverlay || !m_loadingLabel) return;

    lv_label_set_text(m_loadingLabel, message.c_str());
    lv_obj_clear_flag(m_loadingOverlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(m_loadingOverlay);
}

void WifiSetupScreen::hideLoading() {
    if (m_loadingOverlay) {
        lv_obj_add_flag(m_loadingOverlay, LV_OBJ_FLAG_HIDDEN);
    }
}

void WifiSetupScreen::showError(const std::string& message) {
    if (!m_errorLabel) return;

    lv_label_set_text(m_errorLabel, message.c_str());
    lv_obj_clear_flag(m_errorLabel, LV_OBJ_FLAG_HIDDEN);
}

const char* WifiSetupScreen::getSignalIcon(int level) {
    // Use custom signal bars representation
    // LVGL doesn't have signal strength icons, so we use text representation
    switch (level) {
        case 4: return "\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88";  // ████
        case 3: return "\xE2\x96\x88\xE2\x96\x88\xE2\x96\x88\xE2\x96\x91";  // ███░
        case 2: return "\xE2\x96\x88\xE2\x96\x88\xE2\x96\x91\xE2\x96\x91";  // ██░░
        case 1: return "\xE2\x96\x88\xE2\x96\x91\xE2\x96\x91\xE2\x96\x91";  // █░░░
        default: return "\xE2\x96\x91\xE2\x96\x91\xE2\x96\x91\xE2\x96\x91"; // ░░░░
    }
}

void WifiSetupScreen::backButtonHandler(lv_event_t* e) {
    WifiSetupScreen* self = static_cast<WifiSetupScreen*>(lv_event_get_user_data(e));
    if (self) {
        self->m_screenManager.goBack();
    }
}

void WifiSetupScreen::refreshButtonHandler(lv_event_t* e) {
    WifiSetupScreen* self = static_cast<WifiSetupScreen*>(lv_event_get_user_data(e));
    if (self && !self->m_scanning) {
        self->refreshNetworks();
    }
}

void WifiSetupScreen::networkItemHandler(lv_event_t* e) {
    WifiSetupScreen* self = static_cast<WifiSetupScreen*>(lv_event_get_user_data(e));
    lv_obj_t* item = lv_event_get_target(e);

    if (self && item) {
        char* ssid = static_cast<char*>(lv_obj_get_user_data(item));
        if (ssid) {
            // Find the network in our list
            for (const auto& network : self->m_networks) {
                if (network.ssid == ssid) {
                    self->onNetworkSelected(network);
                    break;
                }
            }
        }
    }
}

void WifiSetupScreen::connectButtonHandler(lv_event_t* e) {
    WifiSetupScreen* self = static_cast<WifiSetupScreen*>(lv_event_get_user_data(e));
    if (self) {
        self->onConnectClicked();
    }
}

void WifiSetupScreen::cancelButtonHandler(lv_event_t* e) {
    WifiSetupScreen* self = static_cast<WifiSetupScreen*>(lv_event_get_user_data(e));
    if (self) {
        self->onCancelClicked();
    }
}

void WifiSetupScreen::keyboardHandler(lv_event_t* e) {
    WifiSetupScreen* self = static_cast<WifiSetupScreen*>(lv_event_get_user_data(e));
    if (self) {
        self->onConnectClicked();
    }
}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
