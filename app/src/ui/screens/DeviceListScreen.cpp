/**
 * Device List Screen Implementation
 */

#include "smarthub/ui/screens/DeviceListScreen.hpp"
#include "smarthub/ui/screens/LightControlScreen.hpp"
#include "smarthub/ui/screens/AddDeviceScreen.hpp"
#include "smarthub/ui/screens/EditDeviceScreen.hpp"
#include "smarthub/ui/ScreenManager.hpp"
#include "smarthub/ui/ThemeManager.hpp"
#include "smarthub/ui/widgets/Header.hpp"
#include "smarthub/ui/widgets/NavBar.hpp"
#include "smarthub/devices/DeviceManager.hpp"
#include "smarthub/devices/Device.hpp"
#include "smarthub/core/Logger.hpp"

namespace smarthub {
namespace ui {

DeviceListScreen::DeviceListScreen(ScreenManager& screenManager,
                                   ThemeManager& theme,
                                   DeviceManager& deviceManager)
    : Screen(screenManager, "devices")
    , m_theme(theme)
    , m_deviceManager(deviceManager)
{
}

DeviceListScreen::~DeviceListScreen() = default;

void DeviceListScreen::onCreate() {
#ifdef SMARTHUB_ENABLE_LVGL
    if (!m_container) return;

    lv_obj_set_style_bg_color(m_container, m_theme.background(), 0);

    createHeader();
    createContent();
    createNavBar();

    LOG_DEBUG("DeviceListScreen created");
#endif
}

void DeviceListScreen::onShow() {
#ifdef SMARTHUB_ENABLE_LVGL
    refreshDeviceList();
    if (m_navBar) {
        m_navBar->setActiveTab("devices");
    }
    LOG_DEBUG("DeviceListScreen shown");
#endif
}

void DeviceListScreen::onUpdate(uint32_t deltaMs) {
    (void)deltaMs;
    // Could periodically refresh device states here
}

void DeviceListScreen::onDestroy() {
#ifdef SMARTHUB_ENABLE_LVGL
    m_deviceRows.clear();
    m_navBar.reset();
    m_header.reset();
#endif
}

#ifdef SMARTHUB_ENABLE_LVGL

void DeviceListScreen::createHeader() {
    m_header = std::make_unique<Header>(m_container, m_theme);
    m_header->setTitle("Devices");

    m_header->onSettingsClick([this]() {
        m_screenManager.showScreen("settings");
    });

    m_header->onNotificationClick([this]() {
        m_screenManager.showScreen("notifications");
    });
}

void DeviceListScreen::createContent() {
    // Display is 800x480 landscape, header=50, navbar=60, so content=370
    constexpr int CONTENT_HEIGHT = 480 - Header::HEIGHT - NavBar::HEIGHT;

    m_content = lv_obj_create(m_container);
    lv_obj_set_size(m_content, LV_PCT(100), CONTENT_HEIGHT);
    lv_obj_align(m_content, LV_ALIGN_TOP_MID, 0, Header::HEIGHT);

    lv_obj_set_style_bg_opa(m_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_content, 0, 0);
    lv_obj_set_style_pad_all(m_content, 0, 0);

    // Device list container
    m_deviceList = lv_obj_create(m_content);
    lv_obj_set_size(m_deviceList, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_deviceList, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_deviceList, 0, 0);
    lv_obj_set_style_pad_all(m_deviceList, ThemeManager::SPACING_SM, 0);

    // Vertical flex layout
    lv_obj_set_flex_flow(m_deviceList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_deviceList, ThemeManager::SPACING_SM, 0);
}

void DeviceListScreen::createNavBar() {
    m_navBar = std::make_unique<NavBar>(m_container, m_theme);

    m_navBar->addTab({"home", "Home", LV_SYMBOL_HOME});
    m_navBar->addTab({"devices", "Devices", LV_SYMBOL_POWER});
    m_navBar->addTab({"sensors", "Sensors", LV_SYMBOL_CHARGE});

    m_navBar->setActiveTab("devices");

    m_navBar->onTabSelected([this](const std::string& tabId) {
        onNavTabSelected(tabId);
    });
}

void DeviceListScreen::refreshDeviceList() {
    if (!m_deviceList) return;

    // Clear existing rows
    lv_obj_clean(m_deviceList);
    m_deviceRows.clear();

    // Get all devices
    auto devices = m_deviceManager.getAllDevices();

    if (devices.empty()) {
        // Show "No devices" message
        lv_obj_t* emptyLabel = lv_label_create(m_deviceList);
        lv_label_set_text(emptyLabel, "No devices registered");
        lv_obj_set_style_text_color(emptyLabel, m_theme.textSecondary(), 0);
        lv_obj_center(emptyLabel);
        return;
    }

    for (const auto& device : devices) {
        if (!device) continue;

        std::string typeStr;
        switch (device->type()) {
            case DeviceType::Switch: typeStr = "Switch"; break;
            case DeviceType::Dimmer: typeStr = "Dimmer"; break;
            case DeviceType::ColorLight: typeStr = "Color Light"; break;
            case DeviceType::TemperatureSensor: typeStr = "Temperature"; break;
            case DeviceType::HumiditySensor: typeStr = "Humidity"; break;
            case DeviceType::MotionSensor: typeStr = "Motion"; break;
            case DeviceType::ContactSensor: typeStr = "Contact"; break;
            default: typeStr = "Device"; break;
        }

        // Check if device is on via state JSON
        bool isOn = false;
        auto state = device->getState();
        if (state.contains("on")) {
            isOn = state["on"].get<bool>();
        }

        lv_obj_t* row = createDeviceRow(m_deviceList,
                                         device->id(),
                                         device->name(),
                                         typeStr,
                                         isOn);
        m_deviceRows.emplace_back(device->id(), row);
    }

    // Add "Add Device" card at the end
    createAddDeviceCard();
}

void DeviceListScreen::createAddDeviceCard() {
    lv_obj_t* addCard = lv_obj_create(m_deviceList);
    lv_obj_set_size(addCard, LV_PCT(100), 60);
    lv_obj_set_style_bg_color(addCard, m_theme.surface(), 0);
    lv_obj_set_style_radius(addCard, ThemeManager::CARD_RADIUS, 0);
    lv_obj_add_flag(addCard, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(addCard, LV_OBJ_FLAG_SCROLLABLE);

    // Plus icon and text
    lv_obj_t* content = lv_label_create(addCard);
    lv_label_set_text(content, LV_SYMBOL_PLUS " Add Device");
    lv_obj_set_style_text_color(content, m_theme.primary(), 0);
    lv_obj_center(content);

    lv_obj_add_event_cb(addCard, [](lv_event_t* e) {
        auto* self = static_cast<DeviceListScreen*>(lv_event_get_user_data(e));
        if (self) {
            self->onAddDeviceClicked();
        }
    }, LV_EVENT_CLICKED, this);
}

void DeviceListScreen::onAddDeviceClicked() {
    LOG_DEBUG("Add device clicked");
    m_screenManager.showScreen("add_device");
}

lv_obj_t* DeviceListScreen::createDeviceRow(lv_obj_t* parent,
                                             const std::string& deviceId,
                                             const std::string& name,
                                             const std::string& type,
                                             bool isOn) {
    // Row container
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), 60);
    lv_obj_set_style_bg_color(row, m_theme.surface(), 0);
    lv_obj_set_style_radius(row, ThemeManager::CARD_RADIUS, 0);
    lv_obj_set_style_pad_hor(row, ThemeManager::SPACING_MD, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    // Store device ID (cleanup on object deletion)
    char* idCopy = new char[deviceId.size() + 1];
    strcpy(idCopy, deviceId.c_str());
    lv_obj_set_user_data(row, idCopy);
    lv_obj_add_event_cb(row, [](lv_event_t* e) {
        char* data = static_cast<char*>(lv_obj_get_user_data(lv_event_get_target(e)));
        delete[] data;
    }, LV_EVENT_DELETE, nullptr);

    // Make row clickable
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, rowClickHandler, LV_EVENT_CLICKED, this);

    // Icon based on device type
    lv_obj_t* icon = lv_label_create(row);
    lv_label_set_text(icon, LV_SYMBOL_POWER);
    lv_obj_set_style_text_color(icon, isOn ? m_theme.warning() : m_theme.textSecondary(), 0);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);

    // Device name
    lv_obj_t* nameLabel = lv_label_create(row);
    lv_label_set_text(nameLabel, name.c_str());
    lv_obj_set_style_text_color(nameLabel, m_theme.textPrimary(), 0);
    lv_obj_align(nameLabel, LV_ALIGN_LEFT_MID, 40, -8);

    // Device type
    lv_obj_t* typeLabel = lv_label_create(row);
    lv_label_set_text(typeLabel, type.c_str());
    lv_obj_set_style_text_color(typeLabel, m_theme.textSecondary(), 0);
    lv_obj_align(typeLabel, LV_ALIGN_LEFT_MID, 40, 10);

    // Power toggle switch
    lv_obj_t* sw = lv_switch_create(row);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
    if (isOn) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_set_style_bg_color(sw, m_theme.primary(), LV_STATE_CHECKED | LV_PART_INDICATOR);

    // Store device ID in switch user data too
    char* swIdCopy = new char[deviceId.size() + 1];
    strcpy(swIdCopy, deviceId.c_str());
    lv_obj_set_user_data(sw, swIdCopy);
    lv_obj_add_event_cb(sw, [](lv_event_t* e) {
        delete[] static_cast<char*>(lv_obj_get_user_data(lv_event_get_target(e)));
    }, LV_EVENT_DELETE, nullptr);
    lv_obj_add_event_cb(sw, toggleHandler, LV_EVENT_VALUE_CHANGED, this);

    return row;
}

void DeviceListScreen::onDeviceToggle(const std::string& deviceId, bool newState) {
    LOG_DEBUG("Device toggle: %s -> %s", deviceId.c_str(), newState ? "ON" : "OFF");

    auto device = m_deviceManager.getDevice(deviceId);
    if (device) {
        device->setState("on", newState);
    }
}

void DeviceListScreen::onDeviceClicked(const std::string& deviceId) {
    LOG_DEBUG("Device clicked: %s", deviceId.c_str());

    auto device = m_deviceManager.getDevice(deviceId);
    if (!device) return;

    // Navigate to edit screen for device configuration
    auto* editScreen = dynamic_cast<EditDeviceScreen*>(
        m_screenManager.getScreen("edit_device"));
    if (editScreen) {
        editScreen->setDeviceId(deviceId);
        editScreen->onDeviceUpdated([this]() {
            refreshDeviceList();
        });
        editScreen->onDeviceDeleted([this]() {
            refreshDeviceList();
        });
        m_screenManager.showScreen("edit_device");
    }
}

void DeviceListScreen::onNavTabSelected(const std::string& tabId) {
    if (tabId == "home") {
        m_screenManager.showScreen("dashboard");
    } else if (tabId == "devices") {
        // Already here
    } else if (tabId == "sensors") {
        m_screenManager.showScreen("sensors");
    } else if (tabId == "settings") {
        m_screenManager.showScreen("settings");
    }
}

void DeviceListScreen::toggleHandler(lv_event_t* e) {
    DeviceListScreen* self = static_cast<DeviceListScreen*>(lv_event_get_user_data(e));
    lv_obj_t* sw = lv_event_get_target(e);
    char* deviceId = static_cast<char*>(lv_obj_get_user_data(sw));

    if (self && deviceId) {
        bool isOn = lv_obj_has_state(sw, LV_STATE_CHECKED);
        self->onDeviceToggle(deviceId, isOn);
    }
}

void DeviceListScreen::rowClickHandler(lv_event_t* e) {
    DeviceListScreen* self = static_cast<DeviceListScreen*>(lv_event_get_user_data(e));
    lv_obj_t* row = lv_event_get_target(e);
    char* deviceId = static_cast<char*>(lv_obj_get_user_data(row));

    if (self && deviceId) {
        self->onDeviceClicked(deviceId);
    }
}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
