/**
 * Dashboard Screen Implementation
 */

#include "smarthub/ui/screens/DashboardScreen.hpp"
#include "smarthub/ui/screens/RoomDetailScreen.hpp"
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
    // Update NavBar active indicator
    if (m_navBar) {
        m_navBar->setActiveTab("home");
    }

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
    // Display is 800x480 landscape, header=50, navbar=60, so content=370
    constexpr int CONTENT_HEIGHT = 480 - Header::HEIGHT - NavBar::HEIGHT;

    m_content = lv_obj_create(m_container);
    lv_obj_set_size(m_content, LV_PCT(100), CONTENT_HEIGHT);
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

    m_navBar->setActiveTab("home");

    m_navBar->onTabSelected([this](const std::string& tabId) {
        onNavTabSelected(tabId);
    });
}

void DashboardScreen::createRoomCards() {
    // Clear the room names map
    m_roomNames.clear();

    // Load rooms from database
    auto rooms = m_deviceManager.getAllRooms();

    // If no rooms in database, add default rooms
    if (rooms.empty()) {
        std::vector<std::pair<std::string, std::string>> defaultRooms = {
            {"living_room", "Living Room"},
            {"bedroom", "Bedroom"},
            {"kitchen", "Kitchen"},
            {"bathroom", "Bathroom"},
            {"garage", "Garage"},
            {"office", "Office"},
            {"patio", "Patio"}
        };

        for (const auto& [id, name] : defaultRooms) {
            m_deviceManager.addRoom(id, name);
        }

        // Reload after adding defaults
        rooms = m_deviceManager.getAllRooms();
    }

    for (const auto& [roomId, roomName] : rooms) {
        // Store room name for lookup in onRoomClicked
        m_roomNames[roomId] = roomName;
        auto card = std::make_unique<RoomCard>(m_content, m_theme);

        RoomData data;
        data.id = roomId;
        data.name = roomName;
        data.hasTemperature = false;
        data.activeDevices = 0;

        // Count active devices in this room
        auto devices = m_deviceManager.getDevicesByRoom(roomId);
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
        card->onClick([this](const std::string& id) {
            onRoomClicked(id);
        });

        m_roomCards.push_back(std::move(card));
    }

    // Add "Add Room" card
    lv_obj_t* addCard = lv_obj_create(m_content);
    lv_obj_set_size(addCard, 180, 100);
    m_theme.applyCardStyle(addCard);
    lv_obj_add_flag(addCard, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(addCard, LV_OBJ_FLAG_SCROLLABLE);

    // Plus icon
    lv_obj_t* plusLabel = lv_label_create(addCard);
    lv_label_set_text(plusLabel, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_color(plusLabel, m_theme.primary(), 0);
    lv_obj_set_style_text_font(plusLabel, &lv_font_montserrat_24, 0);
    lv_obj_align(plusLabel, LV_ALIGN_CENTER, 0, -10);

    // "Add Room" text
    lv_obj_t* addLabel = lv_label_create(addCard);
    lv_label_set_text(addLabel, "Add Room");
    lv_obj_set_style_text_color(addLabel, m_theme.textSecondary(), 0);
    lv_obj_align(addLabel, LV_ALIGN_CENTER, 0, 20);

    lv_obj_add_event_cb(addCard, [](lv_event_t* e) {
        DashboardScreen* self = static_cast<DashboardScreen*>(lv_event_get_user_data(e));
        if (self) {
            self->onAddRoomClicked();
        }
    }, LV_EVENT_CLICKED, this);
}

void DashboardScreen::updateRoomCards() {
    // Refresh room cards from database to pick up any changes
    refreshRoomCards();
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

    // Look up room name from our map
    std::string roomName = roomId;  // Fallback to ID
    auto it = m_roomNames.find(roomId);
    if (it != m_roomNames.end()) {
        roomName = it->second;
    }

    // Get the room detail screen and set the room
    auto* roomScreen = dynamic_cast<RoomDetailScreen*>(
        m_screenManager.getScreen("room_detail"));
    if (roomScreen) {
        roomScreen->setRoom(roomId, roomName);
        m_screenManager.showScreen("room_detail");
    }
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
    m_screenManager.showScreen("notifications");
}

void DashboardScreen::onAddRoomClicked() {
    LOG_DEBUG("Add room clicked");

    // Create modal background overlay to block clicks on background
    lv_obj_t* overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);  // Absorb clicks

    // Create the dialog centered
    lv_obj_t* modal = lv_obj_create(overlay);
    lv_obj_set_size(modal, 400, 200);
    lv_obj_align(modal, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(modal, m_theme.surface(), 0);
    lv_obj_set_style_radius(modal, ThemeManager::CARD_RADIUS, 0);
    lv_obj_set_style_shadow_width(modal, 20, 0);
    lv_obj_set_style_shadow_opa(modal, LV_OPA_30, 0);
    lv_obj_set_style_pad_all(modal, 0, 0);  // No default padding
    lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* title = lv_label_create(modal);
    lv_label_set_text(title, "Add New Room");
    lv_obj_set_style_text_color(title, m_theme.textPrimary(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    // Text input (use USB keyboard)
    lv_obj_t* textarea = lv_textarea_create(modal);
    lv_obj_set_size(textarea, 360, 44);
    lv_obj_align(textarea, LV_ALIGN_TOP_MID, 0, 50);
    lv_textarea_set_placeholder_text(textarea, "Room name...");
    lv_textarea_set_one_line(textarea, true);
    lv_obj_set_style_bg_color(textarea, m_theme.background(), 0);
    lv_obj_set_style_text_color(textarea, m_theme.textPrimary(), 0);
    lv_obj_set_style_border_color(textarea, m_theme.primary(), LV_STATE_FOCUSED);
    // Disable scroll-on-focus to prevent screen shifting
    lv_obj_clear_flag(textarea, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    // Add to keyboard input group for keyboard navigation and focus it
    if (lv_group_get_default()) {
        lv_group_add_obj(lv_group_get_default(), textarea);
        lv_group_focus_obj(textarea);
    }

    // Button row - use flex layout for proper spacing
    lv_obj_t* btnRow = lv_obj_create(modal);
    lv_obj_set_size(btnRow, 260, 50);
    lv_obj_align(btnRow, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_style_bg_opa(btnRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_style_pad_all(btnRow, 0, 0);
    lv_obj_clear_flag(btnRow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Cancel button
    lv_obj_t* cancelBtn = lv_btn_create(btnRow);
    lv_obj_set_size(cancelBtn, 120, 40);
    lv_obj_set_style_bg_color(cancelBtn, m_theme.surface(), 0);
    lv_obj_set_style_border_width(cancelBtn, 1, 0);
    lv_obj_set_style_border_color(cancelBtn, m_theme.textSecondary(), 0);

    lv_obj_t* cancelLabel = lv_label_create(cancelBtn);
    lv_label_set_text(cancelLabel, "Cancel");
    lv_obj_set_style_text_color(cancelLabel, m_theme.textPrimary(), 0);
    lv_obj_center(cancelLabel);

    // Save button
    lv_obj_t* saveBtn = lv_btn_create(btnRow);
    lv_obj_set_size(saveBtn, 120, 40);
    lv_obj_set_style_bg_color(saveBtn, m_theme.primary(), 0);

    lv_obj_t* saveLabel = lv_label_create(saveBtn);
    lv_label_set_text(saveLabel, "Add");
    lv_obj_set_style_text_color(saveLabel, lv_color_white(), 0);
    lv_obj_center(saveLabel);

    // Store context for handlers
    struct AddRoomCtx {
        DashboardScreen* screen;
        lv_obj_t* textarea;
        lv_obj_t* overlay;
    };
    auto* ctx = new AddRoomCtx{this, textarea, overlay};
    lv_obj_set_user_data(saveBtn, ctx);
    lv_obj_set_user_data(cancelBtn, ctx);

    // Cancel handler
    lv_obj_add_event_cb(cancelBtn, [](lv_event_t* e) {
        lv_obj_t* btn = lv_event_get_target(e);
        auto* ctx = static_cast<AddRoomCtx*>(lv_obj_get_user_data(btn));
        if (!ctx) return;
        lv_obj_del(ctx->overlay);
        delete ctx;
    }, LV_EVENT_CLICKED, nullptr);

    // Save handler
    lv_obj_add_event_cb(saveBtn, [](lv_event_t* e) {
        lv_obj_t* btn = lv_event_get_target(e);
        auto* ctx = static_cast<AddRoomCtx*>(lv_obj_get_user_data(btn));
        if (!ctx) return;

        const char* text = lv_textarea_get_text(ctx->textarea);
        if (text && strlen(text) > 0) {
            std::string name(text);
            std::string id;
            for (char c : name) {
                if (c == ' ') {
                    id += '_';
                } else {
                    id += static_cast<char>(tolower(c));
                }
            }
            ctx->screen->m_deviceManager.addRoom(id, name);
            ctx->screen->refreshRoomCards();
        }

        lv_obj_del(ctx->overlay);
        delete ctx;
    }, LV_EVENT_CLICKED, nullptr);
}

void DashboardScreen::refreshRoomCards() {
    // Clear existing cards
    if (m_content) {
        lv_obj_clean(m_content);
    }
    m_roomCards.clear();

    // Recreate cards
    createRoomCards();
}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
