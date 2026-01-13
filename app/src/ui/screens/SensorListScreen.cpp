/**
 * Sensor List Screen Implementation
 */

#include "smarthub/ui/screens/SensorListScreen.hpp"
#include "smarthub/ui/screens/SensorHistoryScreen.hpp"
#include "smarthub/ui/ScreenManager.hpp"
#include "smarthub/ui/ThemeManager.hpp"
#include "smarthub/ui/widgets/Header.hpp"
#include "smarthub/ui/widgets/NavBar.hpp"
#include "smarthub/devices/DeviceManager.hpp"
#include "smarthub/devices/Device.hpp"
#include "smarthub/core/Logger.hpp"

namespace smarthub {
namespace ui {

SensorListScreen::SensorListScreen(ScreenManager& screenManager,
                                   ThemeManager& theme,
                                   DeviceManager& deviceManager)
    : Screen(screenManager, "sensors")
    , m_theme(theme)
    , m_deviceManager(deviceManager)
{
}

SensorListScreen::~SensorListScreen() = default;

void SensorListScreen::onCreate() {
#ifdef SMARTHUB_ENABLE_LVGL
    if (!m_container) return;

    lv_obj_set_style_bg_color(m_container, m_theme.background(), 0);

    createHeader();
    createContent();
    createNavBar();

    LOG_DEBUG("SensorListScreen created");
#endif
}

void SensorListScreen::onShow() {
#ifdef SMARTHUB_ENABLE_LVGL
    createSensorCards();
    if (m_navBar) {
        m_navBar->setActiveTab("sensors");
    }
    m_updateMs = 0;
    LOG_DEBUG("SensorListScreen shown");
#endif
}

void SensorListScreen::onUpdate(uint32_t deltaMs) {
#ifdef SMARTHUB_ENABLE_LVGL
    m_updateMs += deltaMs;
    if (m_updateMs >= UPDATE_INTERVAL) {
        refreshSensorValues();
        m_updateMs = 0;
    }
#else
    (void)deltaMs;
#endif
}

void SensorListScreen::onDestroy() {
#ifdef SMARTHUB_ENABLE_LVGL
    m_sensorCards.clear();
    m_navBar.reset();
    m_header.reset();
#endif
}

#ifdef SMARTHUB_ENABLE_LVGL

void SensorListScreen::createHeader() {
    m_header = std::make_unique<Header>(m_container, m_theme);
    m_header->setTitle("Sensors");

    m_header->onSettingsClick([this]() {
        m_screenManager.showScreen("settings");
    });

    m_header->onNotificationClick([this]() {
        m_screenManager.showScreen("notifications");
    });
}

void SensorListScreen::createContent() {
    // Display is 800x480 landscape, header=50, navbar=60, so content=370
    constexpr int CONTENT_HEIGHT = 480 - Header::HEIGHT - NavBar::HEIGHT;

    m_content = lv_obj_create(m_container);
    lv_obj_set_size(m_content, LV_PCT(100), CONTENT_HEIGHT);
    lv_obj_align(m_content, LV_ALIGN_TOP_MID, 0, Header::HEIGHT);

    lv_obj_set_style_bg_opa(m_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_content, 0, 0);
    lv_obj_set_style_pad_all(m_content, ThemeManager::SPACING_MD, 0);

    // Grid layout for sensor cards
    lv_obj_set_flex_flow(m_content, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(m_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(m_content, ThemeManager::SPACING_MD, 0);
    lv_obj_set_style_pad_column(m_content, ThemeManager::SPACING_MD, 0);
}

void SensorListScreen::createNavBar() {
    m_navBar = std::make_unique<NavBar>(m_container, m_theme);

    m_navBar->addTab({"home", "Home", LV_SYMBOL_HOME});
    m_navBar->addTab({"devices", "Devices", LV_SYMBOL_POWER});
    m_navBar->addTab({"sensors", "Sensors", LV_SYMBOL_CHARGE});

    m_navBar->setActiveTab("sensors");

    m_navBar->onTabSelected([this](const std::string& tabId) {
        onNavTabSelected(tabId);
    });
}

void SensorListScreen::createSensorCards() {
    if (!m_content) return;

    lv_obj_clean(m_content);
    m_sensorCards.clear();

    // Get all sensor devices
    auto devices = m_deviceManager.getAllDevices();

    bool hasSensors = false;
    for (const auto& device : devices) {
        if (!device) continue;

        // Check if this is a sensor type
        bool isSensor = false;
        std::string icon;
        std::string unit;

        switch (device->type()) {
            case DeviceType::TemperatureSensor:
                isSensor = true;
                icon = LV_SYMBOL_CHARGE;  // Thermometer-like
                unit = "°F";
                break;
            case DeviceType::HumiditySensor:
                isSensor = true;
                icon = LV_SYMBOL_CHARGE;
                unit = "%";
                break;
            case DeviceType::MotionSensor:
                isSensor = true;
                icon = LV_SYMBOL_EYE_OPEN;
                unit = "";
                break;
            case DeviceType::ContactSensor:
                isSensor = true;
                icon = LV_SYMBOL_CLOSE;
                unit = "";
                break;
            default:
                break;
        }

        if (!isSensor) continue;
        hasSensors = true;

        // Create sensor card
        lv_obj_t* card = lv_obj_create(m_content);
        lv_obj_set_size(card, 180, 120);
        m_theme.applyCardStyle(card);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        // Store device ID in user data for click handler
        char* idCopy = new char[device->id().size() + 1];
        strcpy(idCopy, device->id().c_str());
        lv_obj_set_user_data(card, idCopy);
        lv_obj_add_event_cb(card, [](lv_event_t* e) {
            delete[] static_cast<char*>(lv_obj_get_user_data(lv_event_get_target(e)));
        }, LV_EVENT_DELETE, nullptr);
        lv_obj_add_event_cb(card, sensorCardClickHandler, LV_EVENT_CLICKED, this);

        // Icon
        lv_obj_t* iconLabel = lv_label_create(card);
        lv_label_set_text(iconLabel, icon.c_str());
        lv_obj_set_style_text_color(iconLabel, m_theme.primary(), 0);
        lv_obj_align(iconLabel, LV_ALIGN_TOP_LEFT, 0, 0);

        // Name
        lv_obj_t* nameLabel = lv_label_create(card);
        lv_label_set_text(nameLabel, device->name().c_str());
        lv_obj_set_style_text_color(nameLabel, m_theme.textPrimary(), 0);
        lv_obj_align(nameLabel, LV_ALIGN_TOP_LEFT, 30, 0);

        // Value (large)
        lv_obj_t* valueLabel = lv_label_create(card);
        lv_obj_set_style_text_color(valueLabel, m_theme.textPrimary(), 0);
        lv_obj_align(valueLabel, LV_ALIGN_CENTER, 0, 10);

        // Get current value
        auto value = device->getProperty("value");
        if (value.is_number()) {
            char buf[32];
            if (unit == "°F") {
                snprintf(buf, sizeof(buf), "%.1f%s", value.get<float>(), unit.c_str());
            } else if (unit == "%") {
                snprintf(buf, sizeof(buf), "%.0f%s", value.get<float>(), unit.c_str());
            } else {
                snprintf(buf, sizeof(buf), "%.1f", value.get<float>());
            }
            lv_label_set_text(valueLabel, buf);
        } else if (value.is_boolean()) {
            lv_label_set_text(valueLabel, value.get<bool>() ? "Active" : "Inactive");
        } else {
            lv_label_set_text(valueLabel, "--");
        }

        // Store reference
        SensorCard sc;
        sc.id = device->id();
        sc.card = card;
        sc.valueLabel = valueLabel;
        m_sensorCards.push_back(sc);
    }

    if (!hasSensors) {
        lv_obj_t* emptyLabel = lv_label_create(m_content);
        lv_label_set_text(emptyLabel, "No sensors registered");
        lv_obj_set_style_text_color(emptyLabel, m_theme.textSecondary(), 0);
        lv_obj_center(emptyLabel);
    }
}

void SensorListScreen::refreshSensorValues() {
    for (auto& sc : m_sensorCards) {
        auto device = m_deviceManager.getDevice(sc.id);
        if (!device || !sc.valueLabel) continue;

        std::string unit;
        switch (device->type()) {
            case DeviceType::TemperatureSensor: unit = "°F"; break;
            case DeviceType::HumiditySensor: unit = "%"; break;
            default: break;
        }

        auto value = device->getProperty("value");
        if (value.is_number()) {
            char buf[32];
            if (unit == "°F") {
                snprintf(buf, sizeof(buf), "%.1f%s", value.get<float>(), unit.c_str());
            } else if (unit == "%") {
                snprintf(buf, sizeof(buf), "%.0f%s", value.get<float>(), unit.c_str());
            } else {
                snprintf(buf, sizeof(buf), "%.1f", value.get<float>());
            }
            lv_label_set_text(sc.valueLabel, buf);
        } else if (value.is_boolean()) {
            lv_label_set_text(sc.valueLabel, value.get<bool>() ? "Active" : "Inactive");
        }
    }
}

void SensorListScreen::onSensorClicked(const std::string& sensorId) {
    LOG_DEBUG("Sensor clicked: %s", sensorId.c_str());

    // Navigate to sensor history screen
    auto* screen = m_screenManager.getScreen("sensor_history");
    if (screen) {
        auto* historyScreen = dynamic_cast<SensorHistoryScreen*>(screen);
        if (historyScreen) {
            historyScreen->setSensorId(sensorId);
        }
    }
    m_screenManager.showScreen("sensor_history");
}

void SensorListScreen::sensorCardClickHandler(lv_event_t* e) {
    SensorListScreen* self = static_cast<SensorListScreen*>(lv_event_get_user_data(e));
    lv_obj_t* card = lv_event_get_target(e);
    char* sensorId = static_cast<char*>(lv_obj_get_user_data(card));

    if (self && sensorId) {
        self->onSensorClicked(sensorId);
    }
}

void SensorListScreen::onNavTabSelected(const std::string& tabId) {
    if (tabId == "home") {
        m_screenManager.showScreen("dashboard");
    } else if (tabId == "devices") {
        m_screenManager.showScreen("devices");
    } else if (tabId == "sensors") {
        // Already here
    } else if (tabId == "settings") {
        m_screenManager.showScreen("settings");
    }
}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
