/**
 * RoomCard Widget
 *
 * Card showing room summary: name, temperature, active device count.
 */
#pragma once

#include <string>
#include <functional>
#include <cstdint>

#ifdef SMARTHUB_ENABLE_LVGL
#include <lvgl.h>
#endif

namespace smarthub {
namespace ui {

class ThemeManager;

/**
 * Room data for display
 */
struct RoomData {
    std::string id;
    std::string name;
    std::string climate;          // Such as "72°F / 45%"; empty if the room has no such sensors
    int activeDevices = 0;        // Number of active lights/switches
    bool motionDetected = false;  // Outlines the card

    bool operator==(const RoomData& other) const {
        return id == other.id && name == other.name && climate == other.climate &&
               activeDevices == other.activeDevices && motionDetected == other.motionDetected;
    }
};

/**
 * RoomCard widget for dashboard
 *
 * Layout (~180x100):
 * +----------------+
 * | Living Rm      |
 * | 72°F / 45%  💡3|
 * +----------------+
 * Outlined while motion is detected
 */
class RoomCard {
public:
    using ClickCallback = std::function<void(const std::string& roomId)>;

#ifdef SMARTHUB_ENABLE_LVGL
    /**
     * Create room card widget
     * @param parent Parent LVGL object
     * @param theme Theme manager for styling
     */
    RoomCard(lv_obj_t* parent, const ThemeManager& theme);
#else
    RoomCard();
#endif
    ~RoomCard();

    /**
     * Set the room data to display
     */
    void setRoomData(const RoomData& data);

    /**
     * Get the room ID
     */
    const std::string& roomId() const { return m_roomId; }

    /**
     * Set callback for card click
     */
    void onClick(ClickCallback callback);

#ifdef SMARTHUB_ENABLE_LVGL
    /**
     * Get the root object
     */
    lv_obj_t* obj() const { return m_card; }
#endif

    static constexpr int WIDTH = 180;
    static constexpr int HEIGHT = 100;

private:
#ifdef SMARTHUB_ENABLE_LVGL
    void createLayout();
    static void clickHandler(lv_event_t* e);

    lv_obj_t* m_card = nullptr;
    lv_obj_t* m_nameLabel = nullptr;
    lv_obj_t* m_tempLabel = nullptr;
    lv_obj_t* m_devicesLabel = nullptr;

    const ThemeManager& m_theme;
#endif

    std::string m_roomId;
    RoomData m_shown;
    bool m_hasShown = false;
    ClickCallback m_onClick;
};

} // namespace ui
} // namespace smarthub
