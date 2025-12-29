/**
 * SmartHub Room Implementation
 */

#include <smarthub/devices/Room.hpp>

namespace smarthub {

Room::Room(const std::string& id, const std::string& name)
    : m_id(id)
    , m_name(name)
{
}

nlohmann::json Room::toJson() const {
    return {
        {"id", m_id},
        {"name", m_name},
        {"icon", m_icon},
        {"sort_order", m_sortOrder},
        {"floor", m_floor}
    };
}

Room Room::fromJson(const nlohmann::json& json) {
    std::string id = json.value("id", "");
    std::string name = json.value("name", "");

    Room room(id, name);
    room.setIcon(json.value("icon", "room"));
    room.setSortOrder(json.value("sort_order", 0));
    room.setFloor(json.value("floor", 0));

    return room;
}

} // namespace smarthub
