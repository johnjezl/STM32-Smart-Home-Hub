/**
 * RoomCard Widget Implementation
 */

#include "smarthub/ui/widgets/RoomCard.hpp"
#include "smarthub/ui/ThemeManager.hpp"

#include <cstdio>

namespace smarthub {
namespace ui {

#ifdef SMARTHUB_ENABLE_LVGL

RoomCard::RoomCard(lv_obj_t* parent, const ThemeManager& theme)
    : m_theme(theme)
{
    // Create card container
    m_card = lv_obj_create(parent);
    lv_obj_set_size(m_card, WIDTH, HEIGHT);
    lv_obj_add_flag(m_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(m_card, LV_OBJ_FLAG_SCROLLABLE);

    // Apply card style
    m_theme.applyCardStyle(m_card);

    // Add click handler
    lv_obj_add_event_cb(m_card, clickHandler, LV_EVENT_CLICKED, this);

    createLayout();
}

RoomCard::~RoomCard() {
    // LVGL objects are deleted when parent is deleted
}

void RoomCard::createLayout() {
    // Room name (top)
    m_nameLabel = lv_label_create(m_card);
    lv_label_set_text(m_nameLabel, "Room");
    lv_obj_set_style_text_color(m_nameLabel, m_theme.textPrimary(), 0);
    lv_obj_align(m_nameLabel, LV_ALIGN_TOP_LEFT, 0, 0);

    // Temperature (bottom left)
    m_tempLabel = lv_label_create(m_card);
    lv_label_set_text(m_tempLabel, "--°F");
    lv_obj_set_style_text_color(m_tempLabel, m_theme.textSecondary(), 0);
    lv_obj_align(m_tempLabel, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // Device count with icon (bottom right)
    m_devicesLabel = lv_label_create(m_card);
    lv_label_set_text(m_devicesLabel, LV_SYMBOL_LIGHT_BULB " 0");
    lv_obj_set_style_text_color(m_devicesLabel, m_theme.textSecondary(), 0);
    lv_obj_align(m_devicesLabel, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

void RoomCard::setRoomData(const RoomData& data) {
    m_roomId = data.id;

    if (m_nameLabel) {
        lv_label_set_text(m_nameLabel, data.name.c_str());
    }

    if (m_tempLabel) {
        char buf[16];
        if (data.hasTemperature) {
            snprintf(buf, sizeof(buf), "%.0f°F", data.temperature);
        } else {
            snprintf(buf, sizeof(buf), "--°F");
        }
        lv_label_set_text(m_tempLabel, buf);
    }

    if (m_devicesLabel) {
        char buf[16];
        snprintf(buf, sizeof(buf), LV_SYMBOL_LIGHT_BULB " %d", data.activeDevices);
        lv_label_set_text(m_devicesLabel, buf);

        // Highlight if devices are on
        if (data.activeDevices > 0) {
            lv_obj_set_style_text_color(m_devicesLabel, m_theme.warning(), 0);
        } else {
            lv_obj_set_style_text_color(m_devicesLabel, m_theme.textSecondary(), 0);
        }
    }
}

void RoomCard::onClick(ClickCallback callback) {
    m_onClick = std::move(callback);
}

void RoomCard::clickHandler(lv_event_t* e) {
    RoomCard* self = static_cast<RoomCard*>(lv_event_get_user_data(e));
    if (self && self->m_onClick) {
        self->m_onClick(self->m_roomId);
    }
}

#else // !SMARTHUB_ENABLE_LVGL

RoomCard::RoomCard() {}
RoomCard::~RoomCard() {}
void RoomCard::setRoomData(const RoomData&) {}
void RoomCard::onClick(ClickCallback) {}

#endif // SMARTHUB_ENABLE_LVGL

} // namespace ui
} // namespace smarthub
