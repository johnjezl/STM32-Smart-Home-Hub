/**
 * Automation Data Structures
 *
 * Defines triggers, conditions, actions, and automation rules
 * for the SmartHub automation system.
 */
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace smarthub {

/**
 * Trigger types for automation rules
 */
enum class TriggerType {
    DeviceState,      // Device property changed
    Time,             // At specific time
    TimeInterval,     // Every N minutes/hours
    SensorThreshold   // Sensor value crosses threshold
};

/**
 * Comparison operators for conditions and thresholds
 */
enum class CompareOp {
    Equal,
    NotEqual,
    GreaterThan,
    GreaterOrEqual,
    LessThan,
    LessOrEqual
};

/**
 * Logical operators for combining conditions
 */
enum class LogicOp {
    And,
    Or
};

/**
 * Action types
 */
enum class ActionType {
    SetDeviceState,   // Set a device property
    Delay             // Wait before next action
};

/**
 * A single trigger definition
 */
struct Trigger {
    TriggerType type = TriggerType::DeviceState;

    // For DeviceState and SensorThreshold
    std::string deviceId;
    std::string property;
    nlohmann::json fromValue;    // Optional: trigger on specific transition
    nlohmann::json toValue;      // Optional: trigger when reaches this value

    // For Time triggers
    int hour = -1;               // 0-23, -1 = any
    int minute = -1;             // 0-59, -1 = any
    std::vector<int> daysOfWeek; // 0=Sun, 6=Sat, empty = any

    // For TimeInterval triggers
    int intervalMinutes = 0;     // Repeat every N minutes

    // For SensorThreshold triggers
    CompareOp compareOp = CompareOp::Equal;
    double threshold = 0.0;

    nlohmann::json toJson() const;
    static Trigger fromJson(const nlohmann::json& j);
};

/**
 * A single condition (leaf node in condition tree)
 */
struct ConditionLeaf {
    std::string deviceId;
    std::string property;
    CompareOp op = CompareOp::Equal;
    nlohmann::json value;

    nlohmann::json toJson() const;
    static ConditionLeaf fromJson(const nlohmann::json& j);
};

/**
 * Condition tree node - can be leaf or group
 */
struct Condition {
    bool isGroup = false;

    // Leaf condition (when isGroup = false)
    ConditionLeaf leaf;

    // Group condition (when isGroup = true)
    LogicOp groupOp = LogicOp::And;
    std::vector<Condition> children;

    nlohmann::json toJson() const;
    static Condition fromJson(const nlohmann::json& j);
};

/**
 * A single action to execute
 */
struct Action {
    ActionType type = ActionType::SetDeviceState;
    std::string deviceId;
    std::string property;
    nlohmann::json value;
    uint32_t delayMs = 0;        // For Delay actions

    nlohmann::json toJson() const;
    static Action fromJson(const nlohmann::json& j);
};

/**
 * Complete automation rule
 */
struct Automation {
    std::string id;
    std::string name;
    std::string description;
    bool enabled = true;

    std::vector<Trigger> triggers;        // Any trigger can activate
    std::optional<Condition> condition;   // Optional condition tree
    std::vector<Action> actions;          // Sequential actions

    uint64_t createdAt = 0;
    uint64_t updatedAt = 0;
    uint64_t lastTriggeredAt = 0;

    nlohmann::json toJson() const;
    static Automation fromJson(const nlohmann::json& j);
};

using AutomationPtr = std::shared_ptr<Automation>;

// Helper functions for enum conversion
std::string triggerTypeToString(TriggerType type);
TriggerType stringToTriggerType(const std::string& str);

std::string compareOpToString(CompareOp op);
CompareOp stringToCompareOp(const std::string& str);

std::string logicOpToString(LogicOp op);
LogicOp stringToLogicOp(const std::string& str);

std::string actionTypeToString(ActionType type);
ActionType stringToActionType(const std::string& str);

} // namespace smarthub
