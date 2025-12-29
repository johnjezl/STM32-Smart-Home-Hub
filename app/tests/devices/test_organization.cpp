/**
 * Room and DeviceGroup Unit Tests - Phase 3
 *
 * Tests for organizational structures: rooms and device groups.
 */

#include <gtest/gtest.h>
#include <smarthub/devices/Room.hpp>
#include <smarthub/devices/DeviceGroup.hpp>

using namespace smarthub;

// ============================================================================
// Room Tests
// ============================================================================

class RoomTest : public ::testing::Test {
protected:
};

TEST_F(RoomTest, Construction) {
    Room room("living_room", "Living Room");

    EXPECT_EQ(room.id(), "living_room");
    EXPECT_EQ(room.name(), "Living Room");
    EXPECT_EQ(room.icon(), "room");  // Default icon
    EXPECT_EQ(room.sortOrder(), 0);
    EXPECT_EQ(room.floor(), 0);
}

TEST_F(RoomTest, SetName) {
    Room room("room1", "Original Name");
    room.setName("New Name");

    EXPECT_EQ(room.name(), "New Name");
}

TEST_F(RoomTest, SetIcon) {
    Room room("room1", "Room");

    EXPECT_EQ(room.icon(), "room");  // Default icon

    room.setIcon("mdi:sofa");
    EXPECT_EQ(room.icon(), "mdi:sofa");

    room.setIcon("mdi:bed");
    EXPECT_EQ(room.icon(), "mdi:bed");
}

TEST_F(RoomTest, SetSortOrder) {
    Room room("room1", "Room");

    EXPECT_EQ(room.sortOrder(), 0);

    room.setSortOrder(5);
    EXPECT_EQ(room.sortOrder(), 5);

    room.setSortOrder(-1);
    EXPECT_EQ(room.sortOrder(), -1);
}

TEST_F(RoomTest, ToJson) {
    Room room("bedroom", "Master Bedroom");
    room.setIcon("mdi:bed");
    room.setSortOrder(2);

    auto json = room.toJson();

    EXPECT_EQ(json["id"], "bedroom");
    EXPECT_EQ(json["name"], "Master Bedroom");
    EXPECT_EQ(json["icon"], "mdi:bed");
    EXPECT_EQ(json["sort_order"], 2);
}

TEST_F(RoomTest, FromJson) {
    nlohmann::json json = {
        {"id", "kitchen"},
        {"name", "Kitchen"},
        {"icon", "mdi:stove"},
        {"sort_order", 3},
        {"floor", 1}
    };

    auto room = Room::fromJson(json);

    EXPECT_EQ(room.id(), "kitchen");
    EXPECT_EQ(room.name(), "Kitchen");
    EXPECT_EQ(room.icon(), "mdi:stove");
    EXPECT_EQ(room.sortOrder(), 3);
    EXPECT_EQ(room.floor(), 1);
}

TEST_F(RoomTest, FromJsonMinimal) {
    nlohmann::json json = {
        {"id", "room1"},
        {"name", "Room"}
    };

    auto room = Room::fromJson(json);

    EXPECT_EQ(room.id(), "room1");
    EXPECT_EQ(room.name(), "Room");
    EXPECT_EQ(room.icon(), "room");  // Default icon when not specified
    EXPECT_EQ(room.sortOrder(), 0);
    EXPECT_EQ(room.floor(), 0);
}

TEST_F(RoomTest, RoundTrip) {
    Room original("office", "Home Office");
    original.setIcon("mdi:desk");
    original.setSortOrder(4);
    original.setFloor(2);

    auto json = original.toJson();
    auto restored = Room::fromJson(json);

    EXPECT_EQ(restored.id(), original.id());
    EXPECT_EQ(restored.name(), original.name());
    EXPECT_EQ(restored.icon(), original.icon());
    EXPECT_EQ(restored.sortOrder(), original.sortOrder());
    EXPECT_EQ(restored.floor(), original.floor());
}

TEST_F(RoomTest, SetFloor) {
    Room room("room1", "Room");

    EXPECT_EQ(room.floor(), 0);

    room.setFloor(2);
    EXPECT_EQ(room.floor(), 2);

    room.setFloor(-1);  // Basement
    EXPECT_EQ(room.floor(), -1);
}

// ============================================================================
// DeviceGroup Tests
// ============================================================================

class DeviceGroupTest : public ::testing::Test {
protected:
};

TEST_F(DeviceGroupTest, Construction) {
    DeviceGroup group("all_lights", "All Lights");

    EXPECT_EQ(group.id(), "all_lights");
    EXPECT_EQ(group.name(), "All Lights");
    EXPECT_TRUE(group.deviceIds().empty());
}

TEST_F(DeviceGroupTest, AddDevice) {
    DeviceGroup group("group1", "Test Group");

    group.addDevice("light1");
    EXPECT_EQ(group.deviceIds().size(), 1u);
    EXPECT_EQ(group.deviceIds()[0], "light1");

    group.addDevice("light2");
    EXPECT_EQ(group.deviceIds().size(), 2u);
}

TEST_F(DeviceGroupTest, AddDuplicateDevice) {
    DeviceGroup group("group1", "Test Group");

    group.addDevice("light1");
    group.addDevice("light1");  // Duplicate

    // Implementation may or may not allow duplicates
    // Test that at least one exists
    EXPECT_GE(group.deviceIds().size(), 1u);
}

TEST_F(DeviceGroupTest, RemoveDevice) {
    DeviceGroup group("group1", "Test Group");

    group.addDevice("light1");
    group.addDevice("light2");
    group.addDevice("light3");

    group.removeDevice("light2");

    auto ids = group.deviceIds();
    EXPECT_EQ(ids.size(), 2u);
    EXPECT_TRUE(std::find(ids.begin(), ids.end(), "light1") != ids.end());
    EXPECT_TRUE(std::find(ids.begin(), ids.end(), "light3") != ids.end());
    EXPECT_TRUE(std::find(ids.begin(), ids.end(), "light2") == ids.end());
}

TEST_F(DeviceGroupTest, RemoveNonexistentDevice) {
    DeviceGroup group("group1", "Test Group");

    group.addDevice("light1");
    group.removeDevice("nonexistent");  // Should not crash

    EXPECT_EQ(group.deviceIds().size(), 1u);
}

TEST_F(DeviceGroupTest, RemoveFromEmptyGroup) {
    DeviceGroup group("group1", "Test Group");

    group.removeDevice("light1");  // Should not crash

    EXPECT_TRUE(group.deviceIds().empty());
}

TEST_F(DeviceGroupTest, ClearDevices) {
    DeviceGroup group("group1", "Test Group");

    group.addDevice("light1");
    group.addDevice("light2");
    group.addDevice("light3");

    // Remove all
    group.removeDevice("light1");
    group.removeDevice("light2");
    group.removeDevice("light3");

    EXPECT_TRUE(group.deviceIds().empty());
}

TEST_F(DeviceGroupTest, ToJson) {
    DeviceGroup group("downstairs", "Downstairs Lights");
    group.addDevice("light1");
    group.addDevice("light2");

    auto json = group.toJson();

    EXPECT_EQ(json["id"], "downstairs");
    EXPECT_EQ(json["name"], "Downstairs Lights");
    EXPECT_TRUE(json["devices"].is_array());
    EXPECT_EQ(json["devices"].size(), 2u);
}

TEST_F(DeviceGroupTest, ManyDevices) {
    DeviceGroup group("large_group", "Large Group");

    for (int i = 0; i < 100; i++) {
        group.addDevice("device" + std::to_string(i));
    }

    EXPECT_EQ(group.deviceIds().size(), 100u);

    // Remove half
    for (int i = 0; i < 50; i++) {
        group.removeDevice("device" + std::to_string(i * 2));
    }

    EXPECT_EQ(group.deviceIds().size(), 50u);
}

TEST_F(DeviceGroupTest, DeviceOrder) {
    DeviceGroup group("group1", "Test Group");

    group.addDevice("c_device");
    group.addDevice("a_device");
    group.addDevice("b_device");

    auto ids = group.deviceIds();
    // Order should be preserved as added
    EXPECT_EQ(ids[0], "c_device");
    EXPECT_EQ(ids[1], "a_device");
    EXPECT_EQ(ids[2], "b_device");
}
