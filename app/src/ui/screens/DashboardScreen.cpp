/**
 * Dashboard Screen Implementation
 */

#include "smarthub/ui/screens/DashboardScreen.hpp"
#include "smarthub/ui/ScreenManager.hpp"
#include "smarthub/ui/ThemeManager.hpp"
#include "smarthub/ui/widgets/Header.hpp"
#include "smarthub/ui/widgets/NavBar.hpp"
#include "smarthub/ui/widgets/RoomCard.hpp"
#include "smarthub/devices/DeviceManager.hpp"
#include "smarthub/core/Logger.hpp"

#include <ctime>

namespace smarthub {
namespace ui {

DashboardScreen::DashboardScreen(ScreenManager& screenManager,
                                 ThemeManager& theme,
                                 DeviceManager& deviceManager)
    : Screen(screenManager, "dashboard")
    , m_theme(theme)
    , m_deviceManager(deviceManager)
{
}

DashboardScreen::~DashboardScreen() = default;

void DashboardScreen::onCreate() {
#ifdef SMARTHUB_ENABLE_LVGL
    if (!m_container) return;

    // Set background color
    lv_obj_set_style_bg_color(m_container, m_theme.background(), 0);

    createHeader();
    createContent();
    createNavBar();
    createRoomCards();

    LOG_DEBUG("DashboardScreen created");
#endif
}

void DashboardScreen::onShow() {
#ifdef SMARTHUB_ENABLE_LVGL
    updateRoomCards();
    updateClock();
    m_clockUpdateMs = 0;

    LOG_DEBUG("DashboardScreen shown");
#endif
}

void DashboardScreen::onUpdate(uint32_t deltaMs) {
#ifdef SMARTHUB_ENABLE_LVGL
    m_clockUpdateMs += deltaMs;

    // Update clock every minute
    if (m_clockUpdateMs >= CLOCK_UPDATE_INTERVAL) {
        updateClock();
        m_clockUpdateMs = 0;
    }
#else
    (void)deltaMs;
#endif
}

void DashboardScreen::onHide() {
    LOG_DEBUG("DashboardScreen hidden");
}

void DashboardScreen::onDestroy() {
#ifdef SMARTHUB_ENABLE_LVGL
    m_roomCards.clear();
    m_navBar.reset();
    m_header.reset();
#endif
    LOG_DEBUG("DashboardScreen destroyed");
}

#ifdef SMARTHUB_ENABLE_LVGL

void DashboardScreen::createHeader() {
    m_header = std::make_unique<Header>(m_container, m_theme);
    m_header->setTitle("SmartHub");

    m_header->onSettingsClick([this]() {
        onSettingsClicked();
    });

    m_header->onNotificationClick([this]() {
        onNotificationClicked();
    });

    updateClock();
}

void DashboardScreen::createContent() {
    // Content area between header and navbar
    m_content = lv_obj_create(m_container);
    lv_obj_set_size(m_content, LV_PCT(100),
                    lv_pct(100) - Header::HEIGHT - NavBar::HEIGHT);
    lv_obj_align(m_content, LV_ALIGN_TOP_MID, 0, Header::HEIGHT);

    // Transparent background
    lv_obj_set_style_bg_opa(m_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_content, 0, 0);
    lv_obj_set_style_pad_all(m_content, ThemeManager::SPACING_MD, 0);

    // Enable scrolling
    lv_obj_set_scroll_dir(m_content, LV_DIR_VER);

    // Use flex layout for room cards
    lv_obj_set_flex_flow(m_content, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(m_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(m_content, ThemeManager::SPACING_MD, 0);
    lv_obj_set_style_pad_column(m_content, ThemeManager::SPACING_MD, 0);
}

void DashboardScreen::createNavBar() {
    m_navBar = std::make_unique<NavBar>(m_container, m_theme);

    m_navBar->addTab({"home", "Home", LV_SYMBOL_HOME});
    m_navBar->addTab({"devices", "Devices", LV_SYMBOL_POWER});
    m_navBar->addTab({"sensors", "Sensors", LV_SYMBOL_CHARGE});
    m_navBar->addTab({"settings", "Settings", LV_SYMBOL_SETTINGS});

    m_navBar->setActiveTab("home");

    m_navBar->onTabSelected([this](const std::string& tabId) {
        onNavTabSelected(tabId);
    });
}

void DashboardScreen::createRoomCards() {
    // Default rooms for demo - in production, these would come from config
    std::vector<std::string> defaultRooms = {
        "Living Room", "Bedroom", "Kitchen", "Bathroom",
        "Garage", "Office", "Patio"
    };

    for (const auto& roomName : defaultRooms) {
        auto card = std::make_unique<RoomCard>(m_content, m_theme);

        RoomData data;
        data.id = roomName;
        data.name = roomName;
        data.hasTemperature = false;
        data.activeDevices = 0;

        // Count active devices in this room
        auto devices = m_deviceManager.getDevicesByRoom(roomName);
        for (const auto& device : devices) {
            if (device) {
                // Check if device is on via state JSON
                auto state = device->getState();
                if (state.contains("on") && state["on"].get<bool>()) {
                    data.activeDevices++;
                }
            }
            // TODO: Get temperature from sensor devices
        }

        card->setRoomData(data);
        card->onClick([this](const std::string& roomId) {
            onRoomClicked(roomId);
        });

        m_roomCards.push_back(std::move(card));
    }
}

void DashboardScreen::updateRoomCards() {
    // TODO: Update room cards with actual device states
}

void DashboardScreen::updateClock() {
    if (!m_header) return;

    time_t now = time(nullptr);
    struct tm* tm = localtime(&now);
    if (tm) {
        m_header->setTime(tm->tm_hour, tm->tm_min);
    }
}

void DashboardScreen::onRoomClicked(const std::string& roomId) {
    LOG_DEBUG("Room clicked: %s", roomId.c_str());
    // TODO: Navigate to room detail screen
    // m_screenManager.showScreen("room_" + roomId);
}

void DashboardScreen::onNavTabSelected(const std::string& tabId) {
    LOG_DEBUG("Nav tab selected: %s", tabId.c_str());

    if (tabId == "home") {
        // Already on home
    } else if (tabId == "devices") {
        m_screenManager.showScreen("devices");
    } else if (tabId == "sensors") {
        m_screenManager.showScreen("sensors");
    } else if (tabId == "settings") {
        m_screenManager.showScreen("settings");
    }
}

void DashboardScreen::onSettingsClicked() {
    LOG_DEBUG("Settings clicked");
    m_screenManager.showScreen("settings");
}

void DashboardScreen::onNotificationClicked() {
    LOG_DEBUG("Notifications clicked");
    // TODO: Show notification panel
}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
