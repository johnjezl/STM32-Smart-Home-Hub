/**
 * WiFi Setup Screen
 *
 * Screen for scanning and connecting to WiFi networks.
 * Features network list, signal strength indicators, and password entry.
 */
#pragma once

#include "smarthub/ui/Screen.hpp"
#include "smarthub/network/NetworkManager.hpp"
#include <memory>
#include <string>
#include <vector>

namespace smarthub {

class DeviceManager;

namespace network {
class NetworkManager;
}

namespace ui {

class ThemeManager;

/**
 * WiFi Setup Screen
 *
 * Layout (800x480):
 * +---------------------------------------------------------------+
 * |  < WiFi Setup                                         Refresh |
 * +---------------------------------------------------------------+
 * |  Status: Connected to "MyNetwork" (192.168.1.100)             |
 * +---------------------------------------------------------------+
 * |  +----------------------------------------------------------+ |
 * |  | ████  MyNetwork                               Connected  | |
 * |  +----------------------------------------------------------+ |
 * |  | ███░  OfficeWiFi                              WPA2       | |
 * |  +----------------------------------------------------------+ |
 * |  | ██░░  Guest                                   Open       | |
 * |  +----------------------------------------------------------+ |
 * |  | █░░░  Neighbor                                WPA2       | |
 * |  +----------------------------------------------------------+ |
 * +---------------------------------------------------------------+
 */
class WifiSetupScreen : public Screen {
public:
    WifiSetupScreen(ScreenManager& screenManager,
                    ThemeManager& theme,
                    network::NetworkManager& networkManager);
    ~WifiSetupScreen() override;

    void onCreate() override;
    void onShow() override;
    void onHide() override;
    void onUpdate(uint32_t deltaMs) override;
    void onDestroy() override;

private:
#ifdef SMARTHUB_ENABLE_LVGL
    void createHeader();
    void createStatusBar();
    void createNetworkList();
    void createPasswordDialog();
    void createLoadingOverlay();

    void updateStatusBar();
    void updateNetworkList();
    void refreshNetworks();

    void onNetworkSelected(const network::WifiNetwork& network);
    void onConnectClicked();
    void onCancelClicked();
    void onPasswordEntered(const std::string& password);

    void showPasswordDialog(const std::string& ssid);
    void hidePasswordDialog();
    void showLoading(const std::string& message);
    void hideLoading();
    void showError(const std::string& message);

    // Signal strength icon based on level (0-4)
    static const char* getSignalIcon(int level);

    // Event handlers
    static void backButtonHandler(lv_event_t* e);
    static void refreshButtonHandler(lv_event_t* e);
    static void networkItemHandler(lv_event_t* e);
    static void connectButtonHandler(lv_event_t* e);
    static void cancelButtonHandler(lv_event_t* e);
    static void keyboardHandler(lv_event_t* e);

    // Header
    lv_obj_t* m_backBtn = nullptr;
    lv_obj_t* m_titleLabel = nullptr;
    lv_obj_t* m_refreshBtn = nullptr;

    // Status bar
    lv_obj_t* m_statusBar = nullptr;
    lv_obj_t* m_statusIcon = nullptr;
    lv_obj_t* m_statusLabel = nullptr;

    // Network list
    lv_obj_t* m_networkList = nullptr;
    std::vector<lv_obj_t*> m_networkItems;

    // Password dialog
    lv_obj_t* m_dialogOverlay = nullptr;
    lv_obj_t* m_dialog = nullptr;
    lv_obj_t* m_dialogTitle = nullptr;
    lv_obj_t* m_passwordInput = nullptr;
    lv_obj_t* m_showPasswordCheckbox = nullptr;
    lv_obj_t* m_connectBtn = nullptr;
    lv_obj_t* m_cancelBtn = nullptr;
    lv_obj_t* m_keyboard = nullptr;

    // Loading overlay
    lv_obj_t* m_loadingOverlay = nullptr;
    lv_obj_t* m_loadingSpinner = nullptr;
    lv_obj_t* m_loadingLabel = nullptr;

    // Error message
    lv_obj_t* m_errorLabel = nullptr;
#endif

    ThemeManager& m_theme;
    network::NetworkManager& m_networkManager;

    std::vector<network::WifiNetwork> m_networks;
    std::string m_selectedSsid;
    bool m_scanning = false;

    uint32_t m_refreshMs = 0;
    static constexpr uint32_t AUTO_REFRESH_INTERVAL = 30000;  // 30 seconds
};

} // namespace ui
} // namespace smarthub
