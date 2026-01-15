/**
 * Add Device Screen Implementation
 */

#include "smarthub/ui/screens/AddDeviceScreen.hpp"
#include "smarthub/ui/ScreenManager.hpp"
#include "smarthub/ui/ThemeManager.hpp"
#include "smarthub/devices/DeviceManager.hpp"
#include "smarthub/devices/DeviceTypeRegistry.hpp"
#include "smarthub/core/Logger.hpp"
#include "smarthub/core/EventBus.hpp"

#include <algorithm>
#include <cctype>

namespace smarthub {
namespace ui {

// Device type info for selection grid
struct DeviceTypeInfo {
    DeviceType type;
    const char* name;
    const char* icon;
};

static const std::vector<DeviceTypeInfo> DEVICE_TYPES = {
    {DeviceType::Switch, "Switch", LV_SYMBOL_POWER},
    {DeviceType::Light, "Light", LV_SYMBOL_CHARGE},
    {DeviceType::Dimmer, "Dimmer", LV_SYMBOL_CHARGE},
    {DeviceType::ColorLight, "Color Light", LV_SYMBOL_CHARGE},
    {DeviceType::Outlet, "Outlet", LV_SYMBOL_POWER},
    {DeviceType::TemperatureSensor, "Temp Sensor", LV_SYMBOL_CHARGE},
    {DeviceType::MotionSensor, "Motion Sensor", LV_SYMBOL_EYE_OPEN},
    {DeviceType::ContactSensor, "Door/Window", LV_SYMBOL_CLOSE},
};

AddDeviceScreen::AddDeviceScreen(ScreenManager& screenManager,
                                 ThemeManager& theme,
                                 DeviceManager& deviceManager,
                                 EventBus& eventBus)
    : Screen(screenManager, "add_device")
    , m_theme(theme)
    , m_deviceManager(deviceManager)
    , m_eventBus(eventBus)
{
}

AddDeviceScreen::~AddDeviceScreen() = default;

void AddDeviceScreen::setPreselectedRoom(const std::string& roomId, const std::string& roomName) {
    m_preselectedRoomId = roomId;
    m_preselectedRoomName = roomName;
}

void AddDeviceScreen::onDeviceAdded(DeviceAddedCallback callback) {
    m_onDeviceAdded = std::move(callback);
}

void AddDeviceScreen::onCreate() {
#ifdef SMARTHUB_ENABLE_LVGL
    if (!m_container) return;

    lv_obj_set_style_bg_color(m_container, m_theme.background(), 0);

    createHeader();
    createContent();

    LOG_DEBUG("AddDeviceScreen created");
#endif
}

void AddDeviceScreen::onShow() {
#ifdef SMARTHUB_ENABLE_LVGL
    resetForm();

    // Load rooms for step 4
    m_rooms = m_deviceManager.getAllRooms();

    // Apply preselected room
    if (!m_preselectedRoomId.empty()) {
        m_selectedRoomId = m_preselectedRoomId;
    }

    showStep(1);
    LOG_DEBUG("AddDeviceScreen shown");
#endif
}

void AddDeviceScreen::onHide() {
#ifdef SMARTHUB_ENABLE_LVGL
    // Stop pairing if active
    if (m_isPairing) {
        stopZigbeePairing();
    }

    // Clear pending device callback and any pending device
    m_deviceManager.setPendingDeviceCallback("zigbee", nullptr);
    m_deviceManager.clearPendingDevice("zigbee");
    m_pendingDevice.reset();

    // Hide keyboard if visible
    if (m_keyboard) {
        lv_obj_add_flag(m_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
#endif
    LOG_DEBUG("AddDeviceScreen hidden");
}

void AddDeviceScreen::onUpdate(uint32_t /* deltaMs */) {
}

void AddDeviceScreen::onDestroy() {
    LOG_DEBUG("AddDeviceScreen destroyed");
}

#ifdef SMARTHUB_ENABLE_LVGL

void AddDeviceScreen::createHeader() {
    m_header = lv_obj_create(m_container);
    lv_obj_set_size(m_header, LV_PCT(100), 50);
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
    m_titleLabel = lv_label_create(m_header);
    lv_label_set_text(m_titleLabel, "Add Device");
    lv_obj_set_style_text_color(m_titleLabel, m_theme.textPrimary(), 0);
    lv_obj_set_style_text_font(m_titleLabel, &lv_font_montserrat_20, 0);
    lv_obj_align(m_titleLabel, LV_ALIGN_CENTER, 0, 0);
}

void AddDeviceScreen::createContent() {
    constexpr int HEADER_HEIGHT = 50;
    constexpr int BOTTOM_MARGIN = 8;  // Small margin to prevent display edge clipping
    constexpr int CONTENT_HEIGHT = 480 - HEADER_HEIGHT - BOTTOM_MARGIN;
    constexpr int CONTENT_PADDING = ThemeManager::SPACING_MD;  // 16px
    constexpr int NAV_BUTTON_HEIGHT = 48;
    constexpr int NAV_MARGIN = 8;
    // Step container height: content minus padding, minus nav buttons area
    constexpr int STEP_HEIGHT = CONTENT_HEIGHT - (CONTENT_PADDING * 2) - NAV_BUTTON_HEIGHT - NAV_MARGIN;

    m_content = lv_obj_create(m_container);
    lv_obj_set_size(m_content, LV_PCT(100), CONTENT_HEIGHT);
    lv_obj_align(m_content, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT);
    lv_obj_set_style_bg_opa(m_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_content, 0, 0);
    lv_obj_set_style_pad_all(m_content, CONTENT_PADDING, 0);
    lv_obj_clear_flag(m_content, LV_OBJ_FLAG_SCROLLABLE);

    // Step container (changes based on current step)
    m_stepContainer = lv_obj_create(m_content);
    lv_obj_set_size(m_stepContainer, LV_PCT(100), STEP_HEIGHT);
    lv_obj_align(m_stepContainer, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(m_stepContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_stepContainer, 0, 0);
    lv_obj_set_style_pad_all(m_stepContainer, 0, 0);
    lv_obj_add_flag(m_stepContainer, LV_OBJ_FLAG_SCROLLABLE);  // Allow scrolling if content overflows

    // Navigation buttons at bottom
    m_prevBtn = lv_btn_create(m_content);
    lv_obj_set_size(m_prevBtn, 120, 48);
    lv_obj_align(m_prevBtn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(m_prevBtn, m_theme.surface(), 0);
    lv_obj_set_style_border_width(m_prevBtn, 1, 0);
    lv_obj_set_style_border_color(m_prevBtn, m_theme.textSecondary(), 0);

    lv_obj_t* prevLabel = lv_label_create(m_prevBtn);
    lv_label_set_text(prevLabel, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(prevLabel, m_theme.textPrimary(), 0);
    lv_obj_center(prevLabel);

    lv_obj_add_event_cb(m_prevBtn, [](lv_event_t* e) {
        auto* self = static_cast<AddDeviceScreen*>(lv_event_get_user_data(e));
        self->prevStep();
    }, LV_EVENT_CLICKED, this);

    m_nextBtn = lv_btn_create(m_content);
    lv_obj_set_size(m_nextBtn, 120, 48);
    lv_obj_align(m_nextBtn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(m_nextBtn, m_theme.primary(), 0);

    lv_obj_t* nextLabel = lv_label_create(m_nextBtn);
    lv_label_set_text(nextLabel, "Next " LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(nextLabel, lv_color_white(), 0);
    lv_obj_center(nextLabel);

    lv_obj_add_event_cb(m_nextBtn, nextButtonHandler, LV_EVENT_CLICKED, this);

    // Keyboard (hidden by default)
    m_keyboard = lv_keyboard_create(m_container);
    lv_obj_add_flag(m_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(m_keyboard, LV_PCT(100), 200);
    lv_obj_align(m_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
}

void AddDeviceScreen::showStep(int step) {
    m_currentStep = step;

    // Update title
    char title[64];
    snprintf(title, sizeof(title), "Add Device (%d/%d)", step, TOTAL_STEPS);
    lv_label_set_text(m_titleLabel, title);

    // Show/hide prev button
    if (step == 1) {
        lv_obj_add_flag(m_prevBtn, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(m_prevBtn, LV_OBJ_FLAG_HIDDEN);
    }

    // Update next button text
    lv_obj_t* nextLabel = lv_obj_get_child(m_nextBtn, 0);
    if (step == TOTAL_STEPS) {
        lv_label_set_text(nextLabel, "Create");
        lv_obj_set_style_bg_color(m_nextBtn, m_theme.success(), 0);
    } else {
        lv_label_set_text(nextLabel, "Next " LV_SYMBOL_RIGHT);
        lv_obj_set_style_bg_color(m_nextBtn, m_theme.primary(), 0);
    }

    // Clear step container
    lv_obj_clean(m_stepContainer);

    // Create step content
    switch (step) {
        case 1: createStep1_DeviceType(); break;
        case 2: createStep2_NameProtocol(); break;
        case 3: createStep3_ProtocolConfig(); break;
        case 4: createStep4_RoomSelection(); break;
    }
}

void AddDeviceScreen::nextStep() {
    if (m_currentStep < TOTAL_STEPS) {
        // Validate current step
        if (m_currentStep == 1 && m_selectedType == DeviceType::Unknown) {
            LOG_WARN("No device type selected");
            return;
        }
        if (m_currentStep == 2) {
            if (m_nameInput) {
                m_deviceName = lv_textarea_get_text(m_nameInput);
            }
            if (m_deviceName.empty()) {
                LOG_WARN("Device name is empty");
                return;
            }
        }
        if (m_currentStep == 3) {
            // Get protocol address
            if (m_selectedProtocol == "mqtt" && m_mqttTopicInput) {
                m_protocolAddress = lv_textarea_get_text(m_mqttTopicInput);
            } else if (m_selectedProtocol == "http" && m_httpUrlInput) {
                m_protocolAddress = lv_textarea_get_text(m_httpUrlInput);
            } else if (m_selectedProtocol == "zigbee" && m_zigbeeAddressInput) {
                m_protocolAddress = lv_textarea_get_text(m_zigbeeAddressInput);
                if (m_zigbeeEndpointInput) {
                    m_protocolAddress += ":";
                    m_protocolAddress += lv_textarea_get_text(m_zigbeeEndpointInput);
                }
            }
        }

        showStep(m_currentStep + 1);
    } else {
        onCreateDevice();
    }
}

void AddDeviceScreen::prevStep() {
    if (m_currentStep > 1) {
        showStep(m_currentStep - 1);
    }
}

void AddDeviceScreen::createStep1_DeviceType() {
    // Instructions
    lv_obj_t* label = lv_label_create(m_stepContainer);
    lv_label_set_text(label, "Select device type:");
    lv_obj_set_style_text_color(label, m_theme.textPrimary(), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);

    // Grid of device type cards
    lv_obj_t* grid = lv_obj_create(m_stepContainer);
    lv_obj_set_size(grid, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_align(grid, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(grid, ThemeManager::SPACING_SM, 0);
    lv_obj_set_style_pad_column(grid, ThemeManager::SPACING_SM, 0);

    for (const auto& info : DEVICE_TYPES) {
        lv_obj_t* card = lv_obj_create(grid);
        lv_obj_set_size(card, 170, 70);
        m_theme.applyCardStyle(card);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        // Highlight if selected
        if (info.type == m_selectedType) {
            lv_obj_set_style_border_color(card, m_theme.primary(), 0);
            lv_obj_set_style_border_width(card, 2, 0);
        }

        // Icon
        lv_obj_t* icon = lv_label_create(card);
        lv_label_set_text(icon, info.icon);
        lv_obj_set_style_text_color(icon, m_theme.primary(), 0);
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 10, 0);

        // Name
        lv_obj_t* name = lv_label_create(card);
        lv_label_set_text(name, info.name);
        lv_obj_set_style_text_color(name, m_theme.textPrimary(), 0);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 45, 0);

        // Store type in user data
        lv_obj_set_user_data(card, reinterpret_cast<void*>(static_cast<intptr_t>(info.type)));
        lv_obj_add_event_cb(card, typeCardClickHandler, LV_EVENT_CLICKED, this);
    }
}

void AddDeviceScreen::onDeviceTypeSelected(DeviceType type) {
    m_selectedType = type;
    LOG_DEBUG("Device type selected: %d", static_cast<int>(type));
    showStep(1);  // Refresh to show selection
}

void AddDeviceScreen::createStep2_NameProtocol() {
    // Device name
    lv_obj_t* nameLabel = lv_label_create(m_stepContainer);
    lv_label_set_text(nameLabel, "Device name:");
    lv_obj_set_style_text_color(nameLabel, m_theme.textPrimary(), 0);
    lv_obj_align(nameLabel, LV_ALIGN_TOP_LEFT, 0, 0);

    m_nameInput = lv_textarea_create(m_stepContainer);
    lv_obj_set_size(m_nameInput, LV_PCT(100), 50);
    lv_obj_align(m_nameInput, LV_ALIGN_TOP_LEFT, 0, 25);
    lv_textarea_set_placeholder_text(m_nameInput, "e.g., Living Room Light");
    lv_textarea_set_one_line(m_nameInput, true);
    lv_textarea_set_text(m_nameInput, m_deviceName.c_str());
    lv_obj_set_style_bg_color(m_nameInput, m_theme.surface(), 0);
    lv_obj_set_style_text_color(m_nameInput, m_theme.textPrimary(), 0);

    lv_obj_add_event_cb(m_nameInput, [](lv_event_t* e) {
        auto* self = static_cast<AddDeviceScreen*>(lv_event_get_user_data(e));
        lv_keyboard_set_textarea(self->m_keyboard, self->m_nameInput);
        lv_obj_clear_flag(self->m_keyboard, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_FOCUSED, this);

    lv_obj_add_event_cb(m_nameInput, [](lv_event_t* e) {
        auto* self = static_cast<AddDeviceScreen*>(lv_event_get_user_data(e));
        lv_obj_add_flag(self->m_keyboard, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_DEFOCUSED, this);

    // Protocol selection
    lv_obj_t* protocolLabel = lv_label_create(m_stepContainer);
    lv_label_set_text(protocolLabel, "Protocol:");
    lv_obj_set_style_text_color(protocolLabel, m_theme.textPrimary(), 0);
    lv_obj_align(protocolLabel, LV_ALIGN_TOP_LEFT, 0, 90);

    m_protocolDropdown = lv_dropdown_create(m_stepContainer);
    lv_obj_set_size(m_protocolDropdown, LV_PCT(100), 50);
    lv_obj_align(m_protocolDropdown, LV_ALIGN_TOP_LEFT, 0, 115);
    lv_dropdown_set_options(m_protocolDropdown, "Local (Virtual)\nMQTT\nHTTP\nZigbee");

    // Set current selection
    int idx = 0;
    for (size_t i = 0; i < m_protocols.size(); i++) {
        if (m_protocols[i] == m_selectedProtocol) {
            idx = static_cast<int>(i);
            break;
        }
    }
    lv_dropdown_set_selected(m_protocolDropdown, idx);

    lv_obj_add_event_cb(m_protocolDropdown, protocolDropdownHandler, LV_EVENT_VALUE_CHANGED, this);
}

void AddDeviceScreen::onProtocolSelected(int index) {
    if (index >= 0 && index < static_cast<int>(m_protocols.size())) {
        m_selectedProtocol = m_protocols[index];
        LOG_DEBUG("Protocol selected: %s", m_selectedProtocol.c_str());
    }
}

void AddDeviceScreen::createStep3_ProtocolConfig() {
    lv_obj_t* label = lv_label_create(m_stepContainer);
    lv_obj_set_style_text_color(label, m_theme.textPrimary(), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);

    char title[64];
    snprintf(title, sizeof(title), "Configure %s:", m_selectedProtocol.c_str());
    lv_label_set_text(label, title);

    m_configContainer = lv_obj_create(m_stepContainer);
    lv_obj_set_size(m_configContainer, LV_PCT(100), LV_SIZE_CONTENT);  // Auto-size to content
    lv_obj_align(m_configContainer, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_opa(m_configContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_configContainer, 0, 0);
    lv_obj_set_style_pad_all(m_configContainer, 0, 0);

    if (m_selectedProtocol == "local") {
        createLocalConfig();
    } else if (m_selectedProtocol == "mqtt") {
        createMqttConfig();
    } else if (m_selectedProtocol == "http") {
        createHttpConfig();
    } else if (m_selectedProtocol == "zigbee") {
        createZigbeeConfig();
    }
}

void AddDeviceScreen::createLocalConfig() {
    lv_obj_t* label = lv_label_create(m_configContainer);
    lv_label_set_text(label, "No additional configuration needed.\n\nLocal devices are virtual devices\nfor testing and demonstration.");
    lv_obj_set_style_text_color(label, m_theme.textSecondary(), 0);
    lv_obj_center(label);
}

void AddDeviceScreen::createMqttConfig() {
    lv_obj_t* label = lv_label_create(m_configContainer);
    lv_label_set_text(label, "MQTT Topic:");
    lv_obj_set_style_text_color(label, m_theme.textPrimary(), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);

    m_mqttTopicInput = lv_textarea_create(m_configContainer);
    lv_obj_set_size(m_mqttTopicInput, LV_PCT(100), 50);
    lv_obj_align(m_mqttTopicInput, LV_ALIGN_TOP_LEFT, 0, 25);
    lv_textarea_set_placeholder_text(m_mqttTopicInput, "home/living_room/light1");
    lv_textarea_set_one_line(m_mqttTopicInput, true);
    lv_obj_set_style_bg_color(m_mqttTopicInput, m_theme.surface(), 0);
    lv_obj_set_style_text_color(m_mqttTopicInput, m_theme.textPrimary(), 0);

    lv_obj_add_event_cb(m_mqttTopicInput, [](lv_event_t* e) {
        auto* self = static_cast<AddDeviceScreen*>(lv_event_get_user_data(e));
        lv_keyboard_set_textarea(self->m_keyboard, self->m_mqttTopicInput);
        lv_obj_clear_flag(self->m_keyboard, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_FOCUSED, this);

    lv_obj_add_event_cb(m_mqttTopicInput, [](lv_event_t* e) {
        auto* self = static_cast<AddDeviceScreen*>(lv_event_get_user_data(e));
        lv_obj_add_flag(self->m_keyboard, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_DEFOCUSED, this);
}

void AddDeviceScreen::createHttpConfig() {
    lv_obj_t* label = lv_label_create(m_configContainer);
    lv_label_set_text(label, "Device IP or URL:");
    lv_obj_set_style_text_color(label, m_theme.textPrimary(), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);

    m_httpUrlInput = lv_textarea_create(m_configContainer);
    lv_obj_set_size(m_httpUrlInput, LV_PCT(100), 50);
    lv_obj_align(m_httpUrlInput, LV_ALIGN_TOP_LEFT, 0, 25);
    lv_textarea_set_placeholder_text(m_httpUrlInput, "192.168.1.100");
    lv_textarea_set_one_line(m_httpUrlInput, true);
    lv_obj_set_style_bg_color(m_httpUrlInput, m_theme.surface(), 0);
    lv_obj_set_style_text_color(m_httpUrlInput, m_theme.textPrimary(), 0);

    lv_obj_add_event_cb(m_httpUrlInput, [](lv_event_t* e) {
        auto* self = static_cast<AddDeviceScreen*>(lv_event_get_user_data(e));
        lv_keyboard_set_textarea(self->m_keyboard, self->m_httpUrlInput);
        lv_obj_clear_flag(self->m_keyboard, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_FOCUSED, this);

    lv_obj_add_event_cb(m_httpUrlInput, [](lv_event_t* e) {
        auto* self = static_cast<AddDeviceScreen*>(lv_event_get_user_data(e));
        lv_obj_add_flag(self->m_keyboard, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_DEFOCUSED, this);
}

void AddDeviceScreen::createZigbeeConfig() {
    // Pairing section
    m_pairBtn = lv_btn_create(m_configContainer);
    lv_obj_set_size(m_pairBtn, 160, 44);
    lv_obj_align(m_pairBtn, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(m_pairBtn, m_theme.primary(), 0);

    lv_obj_t* pairBtnLabel = lv_label_create(m_pairBtn);
    lv_label_set_text(pairBtnLabel, LV_SYMBOL_WIFI " Start Pairing");
    lv_obj_set_style_text_color(pairBtnLabel, lv_color_white(), 0);
    lv_obj_center(pairBtnLabel);

    lv_obj_add_event_cb(m_pairBtn, [](lv_event_t* e) {
        auto* self = static_cast<AddDeviceScreen*>(lv_event_get_user_data(e));
        if (self->m_isPairing) {
            self->stopZigbeePairing();
        } else {
            self->startZigbeePairing();
        }
    }, LV_EVENT_CLICKED, this);

    // Pairing spinner (hidden initially) - next to button
    m_pairSpinner = lv_spinner_create(m_configContainer, 1000, 60);
    lv_obj_set_size(m_pairSpinner, 32, 32);
    lv_obj_align(m_pairSpinner, LV_ALIGN_TOP_LEFT, 170, 6);
    lv_obj_add_flag(m_pairSpinner, LV_OBJ_FLAG_HIDDEN);

    // Status label - to the right of spinner with larger font
    m_pairStatusLabel = lv_label_create(m_configContainer);
    lv_label_set_text(m_pairStatusLabel, "Tap Start, then put device in pairing mode");
    lv_obj_set_style_text_color(m_pairStatusLabel, m_theme.textSecondary(), 0);
    lv_obj_set_style_text_font(m_pairStatusLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_width(m_pairStatusLabel, 500);  // Plenty of space for long status messages
    lv_label_set_long_mode(m_pairStatusLabel, LV_LABEL_LONG_WRAP);
    lv_obj_align(m_pairStatusLabel, LV_ALIGN_TOP_LEFT, 212, 12);

    // Discovered device info (below button row)
    m_discoveredDeviceLabel = lv_label_create(m_configContainer);
    lv_label_set_text(m_discoveredDeviceLabel, "");
    lv_obj_set_style_text_color(m_discoveredDeviceLabel, m_theme.success(), 0);
    lv_obj_set_style_text_font(m_discoveredDeviceLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_width(m_discoveredDeviceLabel, 600);
    lv_label_set_long_mode(m_discoveredDeviceLabel, LV_LABEL_LONG_WRAP);
    lv_obj_align(m_discoveredDeviceLabel, LV_ALIGN_TOP_LEFT, 0, 55);
    lv_obj_add_flag(m_discoveredDeviceLabel, LV_OBJ_FLAG_HIDDEN);

    // Manual entry section (below pairing)
    lv_obj_t* orLabel = lv_label_create(m_configContainer);
    lv_label_set_text(orLabel, "- OR enter manually -");
    lv_obj_set_style_text_color(orLabel, m_theme.textSecondary(), 0);
    lv_obj_align(orLabel, LV_ALIGN_TOP_MID, 0, 90);

    lv_obj_t* addrLabel = lv_label_create(m_configContainer);
    lv_label_set_text(addrLabel, "IEEE Address:");
    lv_obj_set_style_text_color(addrLabel, m_theme.textPrimary(), 0);
    lv_obj_align(addrLabel, LV_ALIGN_TOP_LEFT, 0, 115);

    m_zigbeeAddressInput = lv_textarea_create(m_configContainer);
    lv_obj_set_size(m_zigbeeAddressInput, 220, 44);
    lv_obj_align(m_zigbeeAddressInput, LV_ALIGN_TOP_LEFT, 0, 137);
    lv_textarea_set_placeholder_text(m_zigbeeAddressInput, "0x1234567890abcdef");
    lv_textarea_set_one_line(m_zigbeeAddressInput, true);
    lv_obj_set_style_bg_color(m_zigbeeAddressInput, m_theme.surface(), 0);
    lv_obj_set_style_text_color(m_zigbeeAddressInput, m_theme.textPrimary(), 0);

    // Pre-fill if we discovered a device
    if (!m_discoveredIeeeAddress.empty()) {
        lv_textarea_set_text(m_zigbeeAddressInput, m_discoveredIeeeAddress.c_str());
    }

    lv_obj_t* epLabel = lv_label_create(m_configContainer);
    lv_label_set_text(epLabel, "Endpoint:");
    lv_obj_set_style_text_color(epLabel, m_theme.textPrimary(), 0);
    lv_obj_align(epLabel, LV_ALIGN_TOP_LEFT, 235, 115);

    m_zigbeeEndpointInput = lv_textarea_create(m_configContainer);
    lv_obj_set_size(m_zigbeeEndpointInput, 80, 44);
    lv_obj_align(m_zigbeeEndpointInput, LV_ALIGN_TOP_LEFT, 235, 137);
    lv_textarea_set_placeholder_text(m_zigbeeEndpointInput, "1");
    lv_textarea_set_text(m_zigbeeEndpointInput, "1");
    lv_textarea_set_one_line(m_zigbeeEndpointInput, true);
    lv_textarea_set_accepted_chars(m_zigbeeEndpointInput, "0123456789");
    lv_obj_set_style_bg_color(m_zigbeeEndpointInput, m_theme.surface(), 0);
    lv_obj_set_style_text_color(m_zigbeeEndpointInput, m_theme.textPrimary(), 0);

    // Keyboard events
    lv_obj_add_event_cb(m_zigbeeAddressInput, [](lv_event_t* e) {
        auto* self = static_cast<AddDeviceScreen*>(lv_event_get_user_data(e));
        lv_keyboard_set_textarea(self->m_keyboard, self->m_zigbeeAddressInput);
        lv_obj_clear_flag(self->m_keyboard, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_FOCUSED, this);

    lv_obj_add_event_cb(m_zigbeeEndpointInput, [](lv_event_t* e) {
        auto* self = static_cast<AddDeviceScreen*>(lv_event_get_user_data(e));
        lv_keyboard_set_textarea(self->m_keyboard, self->m_zigbeeEndpointInput);
        lv_keyboard_set_mode(self->m_keyboard, LV_KEYBOARD_MODE_NUMBER);
        lv_obj_clear_flag(self->m_keyboard, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_FOCUSED, this);

    auto defocusHandler = [](lv_event_t* e) {
        auto* self = static_cast<AddDeviceScreen*>(lv_event_get_user_data(e));
        lv_obj_add_flag(self->m_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_mode(self->m_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    };

    lv_obj_add_event_cb(m_zigbeeAddressInput, defocusHandler, LV_EVENT_DEFOCUSED, this);
    lv_obj_add_event_cb(m_zigbeeEndpointInput, defocusHandler, LV_EVENT_DEFOCUSED, this);
}

void AddDeviceScreen::startZigbeePairing() {
    LOG_INFO("Starting Zigbee pairing mode");
    m_isPairing = true;

    // Update UI
    lv_obj_t* btnLabel = lv_obj_get_child(m_pairBtn, 0);
    lv_label_set_text(btnLabel, LV_SYMBOL_CLOSE " Stop");
    lv_obj_set_style_bg_color(m_pairBtn, m_theme.warning(), 0);
    lv_obj_clear_flag(m_pairSpinner, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(m_pairStatusLabel, "Searching for devices...");

    // Clear previous discovery
    m_discoveredIeeeAddress.clear();
    m_discoveredManufacturer.clear();
    m_discoveredModel.clear();
    m_pendingDevice.reset();
    lv_obj_add_flag(m_discoveredDeviceLabel, LV_OBJ_FLAG_HIDDEN);

    // Subscribe to device detection events for status updates
    m_eventSubscriptionId = m_eventBus.subscribe("device.state",
        [this](const Event& evt) {
            const auto& event = static_cast<const DeviceStateEvent&>(evt);
            if (event.property == "_detecting" && m_isPairing) {
                // Device detected, waiting for details - update UI on main thread
                LOG_INFO("Device detected, retrieving info...");
                lv_async_call([](void* userData) {
                    auto* self = static_cast<AddDeviceScreen*>(userData);
                    if (self && self->m_pairStatusLabel && self->m_isPairing) {
                        lv_label_set_text(self->m_pairStatusLabel, "Device found! Getting details...");
                        lv_obj_set_style_text_color(self->m_pairStatusLabel, self->m_theme.textPrimary(), 0);
                    }
                }, this);
            } else if (event.property == "_pairing_error" && m_isPairing) {
                // Pairing failed - update UI on main thread
                LOG_ERROR("Pairing error received");
                lv_async_call([](void* userData) {
                    auto* self = static_cast<AddDeviceScreen*>(userData);
                    if (self && self->m_isPairing) {
                        self->stopZigbeePairing();
                        if (self->m_pairStatusLabel) {
                            lv_label_set_text(self->m_pairStatusLabel, "Pairing failed. Try again.");
                            lv_obj_set_style_text_color(self->m_pairStatusLabel, self->m_theme.error(), 0);
                        }
                    }
                }, this);
            }
        });

    // Set up pending device callback to receive discovered devices
    // This prevents auto-adding and lets us show device in wizard
    m_deviceManager.setPendingDeviceCallback("zigbee",
        [this](DevicePtr device) {
            LOG_INFO("Pending device callback: received device %s", device->id().c_str());
            onZigbeeDeviceDiscovered(device);
        });

    // Start discovery via device manager
    m_deviceManager.startDiscovery("zigbee");
}

void AddDeviceScreen::stopZigbeePairing() {
    LOG_INFO("Stopping Zigbee pairing mode");
    m_isPairing = false;

    // Unsubscribe from events
    if (m_eventSubscriptionId != 0) {
        m_eventBus.unsubscribe(m_eventSubscriptionId);
        m_eventSubscriptionId = 0;
    }

    // Update UI
    lv_obj_t* btnLabel = lv_obj_get_child(m_pairBtn, 0);
    lv_label_set_text(btnLabel, LV_SYMBOL_WIFI " Start Pairing");
    lv_obj_set_style_bg_color(m_pairBtn, m_theme.primary(), 0);
    lv_obj_add_flag(m_pairSpinner, LV_OBJ_FLAG_HIDDEN);

    if (m_discoveredIeeeAddress.empty()) {
        lv_label_set_text(m_pairStatusLabel, "Pairing stopped. No device found.");
    }

    // Stop discovery
    m_deviceManager.stopDiscovery();
}

void AddDeviceScreen::onZigbeeDeviceDiscovered(DevicePtr device) {
    if (!device || !m_isPairing) return;

    LOG_INFO("Zigbee device discovered: %s", device->id().c_str());

    // Store the pending device - it won't be added until the user completes the wizard
    m_pendingDevice = device;

    // Extract IEEE address from device ID (format: zigbee_XXXXXXXXXXXXXXXX)
    std::string deviceId = device->id();
    if (deviceId.substr(0, 7) == "zigbee_") {
        m_discoveredIeeeAddress = "0x" + deviceId.substr(7);
    }

    // Get device info from properties
    auto manufacturer = device->getProperty("manufacturer");
    if (manufacturer.is_string()) {
        m_discoveredManufacturer = manufacturer.get<std::string>();
    }
    auto model = device->getProperty("model");
    if (model.is_string()) {
        m_discoveredModel = model.get<std::string>();
    }

    // Auto-fill device name if empty (do this before async call)
    if (m_deviceName.empty() && !m_discoveredModel.empty()) {
        m_deviceName = m_discoveredModel;
    }

    // Schedule UI updates on main thread - LVGL is not thread-safe
    lv_async_call([](void* userData) {
        auto* self = static_cast<AddDeviceScreen*>(userData);
        if (!self) return;

        // Stop pairing mode and update UI
        self->stopZigbeePairing();

        // Show discovered device info
        char infoText[128];
        snprintf(infoText, sizeof(infoText), "Found: %s %s",
                 self->m_discoveredManufacturer.c_str(), self->m_discoveredModel.c_str());
        lv_label_set_text(self->m_discoveredDeviceLabel, infoText);
        lv_obj_clear_flag(self->m_discoveredDeviceLabel, LV_OBJ_FLAG_HIDDEN);

        lv_label_set_text(self->m_pairStatusLabel, "Device found! Complete setup to add it.");
        lv_obj_set_style_text_color(self->m_pairStatusLabel, self->m_theme.success(), 0);

        // Auto-fill the address field
        if (self->m_zigbeeAddressInput) {
            lv_textarea_set_text(self->m_zigbeeAddressInput, self->m_discoveredIeeeAddress.c_str());
        }
    }, this);

    // Auto-select device type based on discovered device
    if (m_selectedType == DeviceType::Unknown && m_pendingDevice) {
        m_selectedType = m_pendingDevice->type();
        LOG_INFO("Auto-selected device type: %d", static_cast<int>(m_selectedType));
    }
}

void AddDeviceScreen::createStep4_RoomSelection() {
    lv_obj_t* label = lv_label_create(m_stepContainer);
    lv_label_set_text(label, "Assign to room (optional):");
    lv_obj_set_style_text_color(label, m_theme.textPrimary(), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);

    m_roomDropdown = lv_dropdown_create(m_stepContainer);
    lv_obj_set_size(m_roomDropdown, LV_PCT(100), 50);
    lv_obj_align(m_roomDropdown, LV_ALIGN_TOP_LEFT, 0, 30);

    // Build room options
    std::string options = "(No room)";
    int selectedIdx = 0;

    for (size_t i = 0; i < m_rooms.size(); i++) {
        options += "\n" + m_rooms[i].second;
        if (m_rooms[i].first == m_selectedRoomId) {
            selectedIdx = static_cast<int>(i) + 1;  // +1 for "(No room)"
        }
    }

    lv_dropdown_set_options(m_roomDropdown, options.c_str());
    lv_dropdown_set_selected(m_roomDropdown, selectedIdx);

    lv_obj_add_event_cb(m_roomDropdown, roomDropdownHandler, LV_EVENT_VALUE_CHANGED, this);

    // Summary
    lv_obj_t* summary = lv_obj_create(m_stepContainer);
    lv_obj_set_size(summary, LV_PCT(100), 150);
    lv_obj_align(summary, LV_ALIGN_TOP_MID, 0, 100);
    m_theme.applyCardStyle(summary);
    lv_obj_clear_flag(summary, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* summaryTitle = lv_label_create(summary);
    lv_label_set_text(summaryTitle, "Summary:");
    lv_obj_set_style_text_color(summaryTitle, m_theme.textPrimary(), 0);
    lv_obj_set_style_text_font(summaryTitle, &lv_font_montserrat_16, 0);
    lv_obj_align(summaryTitle, LV_ALIGN_TOP_LEFT, 10, 5);

    // Find type name
    const char* typeName = "Unknown";
    for (const auto& info : DEVICE_TYPES) {
        if (info.type == m_selectedType) {
            typeName = info.name;
            break;
        }
    }

    char summaryText[256];
    snprintf(summaryText, sizeof(summaryText),
             "Name: %s\nType: %s\nProtocol: %s\nAddress: %s",
             m_deviceName.c_str(),
             typeName,
             m_selectedProtocol.c_str(),
             m_protocolAddress.empty() ? "(none)" : m_protocolAddress.c_str());

    lv_obj_t* summaryContent = lv_label_create(summary);
    lv_label_set_text(summaryContent, summaryText);
    lv_obj_set_style_text_color(summaryContent, m_theme.textSecondary(), 0);
    lv_obj_align(summaryContent, LV_ALIGN_TOP_LEFT, 10, 30);
}

void AddDeviceScreen::onBackClicked() {
    if (m_currentStep > 1) {
        prevStep();
    } else {
        m_screenManager.goBack();
    }
}

void AddDeviceScreen::onNextClicked() {
    nextStep();
}

void AddDeviceScreen::onCreateDevice() {
    // Get room selection
    if (m_roomDropdown) {
        int idx = lv_dropdown_get_selected(m_roomDropdown);
        if (idx > 0 && idx <= static_cast<int>(m_rooms.size())) {
            m_selectedRoomId = m_rooms[idx - 1].first;
        } else {
            m_selectedRoomId.clear();
        }
    }

    DevicePtr device;

    // If we have a pending device from Zigbee pairing, use it
    if (m_pendingDevice && m_selectedProtocol == "zigbee") {
        LOG_INFO("Using pending Zigbee device: %s", m_pendingDevice->id().c_str());
        device = m_pendingDevice;

        // Update device name if user changed it
        if (!m_deviceName.empty()) {
            device->setName(m_deviceName);
        }

        // Set room if selected
        if (!m_selectedRoomId.empty()) {
            device->setRoom(m_selectedRoomId);
        }

        LOG_INFO("Adding paired device: %s (name=%s, room=%s)",
                 device->id().c_str(),
                 device->name().c_str(),
                 m_selectedRoomId.c_str());
    } else {
        // Generate unique ID for manually configured device
        std::string id = generateDeviceId(m_deviceName);

        LOG_INFO("Creating device: %s (type=%d, protocol=%s, address=%s, room=%s)",
                 m_deviceName.c_str(),
                 static_cast<int>(m_selectedType),
                 m_selectedProtocol.c_str(),
                 m_protocolAddress.c_str(),
                 m_selectedRoomId.c_str());

        // Create device via registry
        device = DeviceTypeRegistry::instance().create(
            m_selectedType,
            id,
            m_deviceName,
            m_selectedProtocol,
            m_protocolAddress
        );

        if (device && !m_selectedRoomId.empty()) {
            device->setRoom(m_selectedRoomId);
        }
    }

    if (device) {
        // Add to manager
        m_deviceManager.addDevice(device);

        // Clear pending device since it's now added
        m_pendingDevice.reset();
        m_deviceManager.clearPendingDevice("zigbee");

        // Callback
        if (m_onDeviceAdded) {
            m_onDeviceAdded(device);
        }

        LOG_INFO("Device added successfully: %s", device->id().c_str());

        // Go back
        m_screenManager.goBack();
    } else {
        LOG_ERROR("Failed to create device");
    }
}

void AddDeviceScreen::resetForm() {
    m_currentStep = 1;
    m_selectedType = DeviceType::Unknown;
    m_deviceName.clear();
    m_selectedProtocol = "local";
    m_protocolAddress.clear();
    m_selectedRoomId = m_preselectedRoomId;

    // Reset pairing state
    m_isPairing = false;
    m_discoveredIeeeAddress.clear();
    m_discoveredManufacturer.clear();
    m_discoveredModel.clear();
    m_pendingDevice.reset();
}

std::string AddDeviceScreen::generateDeviceId(const std::string& name) {
    std::string id;
    for (char c : name) {
        if (std::isalnum(c)) {
            id += static_cast<char>(std::tolower(c));
        } else if (c == ' ' && !id.empty() && id.back() != '_') {
            id += '_';
        }
    }

    // Remove trailing underscore
    while (!id.empty() && id.back() == '_') {
        id.pop_back();
    }

    // Make unique by appending timestamp if needed
    std::string baseId = id;
    auto existing = m_deviceManager.getDevice(id);
    int suffix = 1;
    while (existing) {
        id = baseId + "_" + std::to_string(suffix++);
        existing = m_deviceManager.getDevice(id);
    }

    return id;
}

void AddDeviceScreen::backButtonHandler(lv_event_t* e) {
    auto* self = static_cast<AddDeviceScreen*>(lv_event_get_user_data(e));
    if (self) {
        self->onBackClicked();
    }
}

void AddDeviceScreen::nextButtonHandler(lv_event_t* e) {
    auto* self = static_cast<AddDeviceScreen*>(lv_event_get_user_data(e));
    if (self) {
        self->onNextClicked();
    }
}

void AddDeviceScreen::typeCardClickHandler(lv_event_t* e) {
    auto* self = static_cast<AddDeviceScreen*>(lv_event_get_user_data(e));
    lv_obj_t* card = lv_event_get_target(e);
    auto type = static_cast<DeviceType>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(card)));
    if (self) {
        self->onDeviceTypeSelected(type);
    }
}

void AddDeviceScreen::protocolDropdownHandler(lv_event_t* e) {
    auto* self = static_cast<AddDeviceScreen*>(lv_event_get_user_data(e));
    lv_obj_t* dropdown = lv_event_get_target(e);
    int idx = lv_dropdown_get_selected(dropdown);
    if (self) {
        self->onProtocolSelected(idx);
    }
}

void AddDeviceScreen::roomDropdownHandler(lv_event_t* e) {
    (void)e;  // Selection is read at create time
}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
