/**
 * SmartHub Room
 *
 * Represents a room containing devices.
 */
#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace smarthub {

/**
 * Room - represents a physical location containing devices
 */
class Room {
public:
    /**
     * Constructor
     * @param id Unique room identifier
     * @param name Human-readable room name
     */
    Room(const std::string& id, const std::string& name);

    /**
     * Get room ID
     */
    std::string id() const { return m_id; }

    /**
     * Get room name
     */
    std::string name() const { return m_name; }

    /**
     * Set room name
     */
    void setName(const std::string& name) { m_name = name; }

    /**
     * Get room icon (Material Design icon name)
     */
    std::string icon() const { return m_icon; }

    /**
     * Set room icon
     */
    void setIcon(const std::string& icon) { m_icon = icon; }

    /**
     * Get sort order for display
     */
    int sortOrder() const { return m_sortOrder; }

    /**
     * Set sort order
     */
    void setSortOrder(int order) { m_sortOrder = order; }

    /**
     * Get floor number (0 = ground floor)
     */
    int floor() const { return m_floor; }

    /**
     * Set floor number
     */
    void setFloor(int floor) { m_floor = floor; }

    /**
     * Serialize to JSON
     */
    nlohmann::json toJson() const;

    /**
     * Deserialize from JSON
     */
    static Room fromJson(const nlohmann::json& json);

private:
    std::string m_id;
    std::string m_name;
    std::string m_icon = "room";
    int m_sortOrder = 0;
    int m_floor = 0;
};

} // namespace smarthub
