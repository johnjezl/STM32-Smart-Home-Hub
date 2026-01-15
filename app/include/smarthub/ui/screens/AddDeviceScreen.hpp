/**
 * Add Device Screen
 *
 * Multi-step wizard for adding new devices to the smart home system.
 */
#pragma once

#include "smarthub/ui/Screen.hpp"
#include "smarthub/devices/IDevice.hpp"
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace smarthub {

class DeviceManager;
class EventBus;

namespace ui {

class ThemeManager;

/**
 * Add device wizard screen
 *
 * Steps:
 * 1. Select device type
 * 2. Enter name and select protocol
 * 3. Configure protocol-specific settings
 * 4. Select room (optional)
 */
class AddDeviceScreen : public Screen {
public:
    using DeviceAddedCallback = std::function<void(DevicePtr)>;

    AddDeviceScreen(ScreenManager& screenManager,
                    ThemeManager& theme,
                    DeviceManager& deviceManager,
                    EventBus& eventBus);
    ~AddDeviceScreen() override;

    /**
     * Set room to pre-select (when adding from RoomDetailScreen)
     */
    void setPreselectedRoom(const std::string& roomId, const std::string& roomName);

    /**
     * Set callback for when device is added
     */
    void onDeviceAdded(DeviceAddedCallback callback);

    void onCreate() override;
    void onShow() override;
    void onHide() override;
    void onUpdate(uint32_t deltaMs) override;
    void onDestroy() override;

private:
#ifdef SMARTHUB_ENABLE_LVGL
    void createHeader();
    void createContent();

    // Step management
    void showStep(int step);
    void nextStep();
    void prevStep();

    // Step 1: Device type selection
    void createStep1_DeviceType();
    void onDeviceTypeSelected(DeviceType type);

    // Step 2: Name and protocol
    void createStep2_NameProtocol();
    void onProtocolSelected(int index);

    // Step 3: Protocol configuration
    void createStep3_ProtocolConfig();
    void createLocalConfig();
    void createMqttConfig();
    void createHttpConfig();
    void createZigbeeConfig();

    // Zigbee pairing
    void startZigbeePairing();
    void stopZigbeePairing();
    void onZigbeeDeviceDiscovered(DevicePtr device);

    // Step 4: Room selection
    void createStep4_RoomSelection();

    // Actions
    void onBackClicked();
    void onNextClicked();
    void onCreateDevice();
    void resetForm();

    std::string generateDeviceId(const std::string& name);

    static void backButtonHandler(lv_event_t* e);
    static void nextButtonHandler(lv_event_t* e);
    static void typeCardClickHandler(lv_event_t* e);
    static void protocolDropdownHandler(lv_event_t* e);
    static void roomDropdownHandler(lv_event_t* e);

    // UI elements
    lv_obj_t* m_header = nullptr;
    lv_obj_t* m_backBtn = nullptr;
    lv_obj_t* m_titleLabel = nullptr;
    lv_obj_t* m_content = nullptr;
    lv_obj_t* m_stepContainer = nullptr;

    // Step 2 elements
    lv_obj_t* m_nameInput = nullptr;
    lv_obj_t* m_protocolDropdown = nullptr;

    // Step 3 elements
    lv_obj_t* m_configContainer = nullptr;
    lv_obj_t* m_mqttTopicInput = nullptr;
    lv_obj_t* m_httpUrlInput = nullptr;
    lv_obj_t* m_zigbeeAddressInput = nullptr;
    lv_obj_t* m_zigbeeEndpointInput = nullptr;

    // Zigbee pairing elements
    lv_obj_t* m_pairBtn = nullptr;
    lv_obj_t* m_pairStatusLabel = nullptr;
    lv_obj_t* m_pairSpinner = nullptr;
    lv_obj_t* m_discoveredDeviceLabel = nullptr;

    // Step 4 elements
    lv_obj_t* m_roomDropdown = nullptr;

    // Navigation buttons
    lv_obj_t* m_prevBtn = nullptr;
    lv_obj_t* m_nextBtn = nullptr;

    // Keyboard
    lv_obj_t* m_keyboard = nullptr;
#endif

    // State
    int m_currentStep = 1;
    DeviceType m_selectedType = DeviceType::Unknown;
    std::string m_deviceName;
    std::string m_selectedProtocol = "local";
    std::string m_protocolAddress;
    std::string m_selectedRoomId;
    std::string m_preselectedRoomId;
    std::string m_preselectedRoomName;

    // Protocol options
    std::vector<std::string> m_protocols = {"local", "mqtt", "http", "zigbee"};
    std::vector<std::pair<std::string, std::string>> m_rooms;  // id, name

    // Zigbee pairing state
    bool m_isPairing = false;
    std::string m_discoveredIeeeAddress;
    std::string m_discoveredManufacturer;
    std::string m_discoveredModel;
    DevicePtr m_pendingDevice;  // Device discovered but not yet added

    ThemeManager& m_theme;
    DeviceManager& m_deviceManager;
    EventBus& m_eventBus;
    DeviceAddedCallback m_onDeviceAdded;
    uint64_t m_eventSubscriptionId = 0;

    static constexpr int TOTAL_STEPS = 4;
};

} // namespace ui
} // namespace smarthub
