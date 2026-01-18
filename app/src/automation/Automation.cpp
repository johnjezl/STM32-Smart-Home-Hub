/**
 * Automation Data Structures Implementation
 *
 * JSON serialization for automation rules.
 */

#include "smarthub/automation/Automation.hpp"

namespace smarthub {

// Enum to string conversions
std::string triggerTypeToString(TriggerType type) {
    switch (type) {
        case TriggerType::DeviceState:    return "device_state";
        case TriggerType::Time:           return "time";
        case TriggerType::TimeInterval:   return "time_interval";
        case TriggerType::SensorThreshold: return "sensor_threshold";
        default:                          return "device_state";
    }
}

TriggerType stringToTriggerType(const std::string& str) {
    if (str == "device_state")     return TriggerType::DeviceState;
    if (str == "time")             return TriggerType::Time;
    if (str == "time_interval")    return TriggerType::TimeInterval;
    if (str == "sensor_threshold") return TriggerType::SensorThreshold;
    return TriggerType::DeviceState;
}

std::string compareOpToString(CompareOp op) {
    switch (op) {
        case CompareOp::Equal:          return "eq";
        case CompareOp::NotEqual:       return "neq";
        case CompareOp::GreaterThan:    return "gt";
        case CompareOp::GreaterOrEqual: return "gte";
        case CompareOp::LessThan:       return "lt";
        case CompareOp::LessOrEqual:    return "lte";
        default:                        return "eq";
    }
}

CompareOp stringToCompareOp(const std::string& str) {
    if (str == "eq")  return CompareOp::Equal;
    if (str == "neq") return CompareOp::NotEqual;
    if (str == "gt")  return CompareOp::GreaterThan;
    if (str == "gte") return CompareOp::GreaterOrEqual;
    if (str == "lt")  return CompareOp::LessThan;
    if (str == "lte") return CompareOp::LessOrEqual;
    return CompareOp::Equal;
}

std::string logicOpToString(LogicOp op) {
    return (op == LogicOp::And) ? "and" : "or";
}

LogicOp stringToLogicOp(const std::string& str) {
    return (str == "or") ? LogicOp::Or : LogicOp::And;
}

std::string actionTypeToString(ActionType type) {
    switch (type) {
        case ActionType::SetDeviceState: return "set_device_state";
        case ActionType::Delay:          return "delay";
        default:                         return "set_device_state";
    }
}

ActionType stringToActionType(const std::string& str) {
    if (str == "set_device_state") return ActionType::SetDeviceState;
    if (str == "delay")            return ActionType::Delay;
    return ActionType::SetDeviceState;
}

// Trigger serialization
nlohmann::json Trigger::toJson() const {
    nlohmann::json j;
    j["type"] = triggerTypeToString(type);

    switch (type) {
        case TriggerType::DeviceState:
            j["device_id"] = deviceId;
            j["property"] = property;
            if (!fromValue.is_null()) j["from_value"] = fromValue;
            if (!toValue.is_null()) j["to_value"] = toValue;
            break;

        case TriggerType::Time:
            j["hour"] = hour;
            j["minute"] = minute;
            if (!daysOfWeek.empty()) j["days_of_week"] = daysOfWeek;
            break;

        case TriggerType::TimeInterval:
            j["interval_minutes"] = intervalMinutes;
            break;

        case TriggerType::SensorThreshold:
            j["device_id"] = deviceId;
            j["property"] = property;
            j["compare_op"] = compareOpToString(compareOp);
            j["threshold"] = threshold;
            break;
    }

    return j;
}

Trigger Trigger::fromJson(const nlohmann::json& j) {
    Trigger t;
    t.type = stringToTriggerType(j.value("type", "device_state"));

    switch (t.type) {
        case TriggerType::DeviceState:
            t.deviceId = j.value("device_id", "");
            t.property = j.value("property", "");
            if (j.contains("from_value")) t.fromValue = j["from_value"];
            if (j.contains("to_value")) t.toValue = j["to_value"];
            break;

        case TriggerType::Time:
            t.hour = j.value("hour", -1);
            t.minute = j.value("minute", -1);
            if (j.contains("days_of_week")) {
                t.daysOfWeek = j["days_of_week"].get<std::vector<int>>();
            }
            break;

        case TriggerType::TimeInterval:
            t.intervalMinutes = j.value("interval_minutes", 0);
            break;

        case TriggerType::SensorThreshold:
            t.deviceId = j.value("device_id", "");
            t.property = j.value("property", "");
            t.compareOp = stringToCompareOp(j.value("compare_op", "eq"));
            t.threshold = j.value("threshold", 0.0);
            break;
    }

    return t;
}

// ConditionLeaf serialization
nlohmann::json ConditionLeaf::toJson() const {
    return {
        {"device_id", deviceId},
        {"property", property},
        {"op", compareOpToString(op)},
        {"value", value}
    };
}

ConditionLeaf ConditionLeaf::fromJson(const nlohmann::json& j) {
    ConditionLeaf c;
    c.deviceId = j.value("device_id", "");
    c.property = j.value("property", "");
    c.op = stringToCompareOp(j.value("op", "eq"));
    if (j.contains("value")) c.value = j["value"];
    return c;
}

// Condition serialization
nlohmann::json Condition::toJson() const {
    nlohmann::json j;
    j["is_group"] = isGroup;

    if (isGroup) {
        j["group_op"] = logicOpToString(groupOp);
        nlohmann::json childArray = nlohmann::json::array();
        for (const auto& child : children) {
            childArray.push_back(child.toJson());
        }
        j["children"] = childArray;
    } else {
        j["leaf"] = leaf.toJson();
    }

    return j;
}

Condition Condition::fromJson(const nlohmann::json& j) {
    Condition c;
    c.isGroup = j.value("is_group", false);

    if (c.isGroup) {
        c.groupOp = stringToLogicOp(j.value("group_op", "and"));
        if (j.contains("children") && j["children"].is_array()) {
            for (const auto& child : j["children"]) {
                c.children.push_back(Condition::fromJson(child));
            }
        }
    } else {
        if (j.contains("leaf")) {
            c.leaf = ConditionLeaf::fromJson(j["leaf"]);
        }
    }

    return c;
}

// Action serialization
nlohmann::json Action::toJson() const {
    nlohmann::json j;

    switch (type) {
        case ActionType::SetDeviceState:
            j["deviceId"] = deviceId;
            j["property"] = property;
            j["value"] = value;
            break;

        case ActionType::Delay:
            j["delayMs"] = delayMs;
            break;
    }

    return j;
}

Action Action::fromJson(const nlohmann::json& j) {
    Action a;
    a.type = stringToActionType(j.value("type", "set_device_state"));

    switch (a.type) {
        case ActionType::SetDeviceState:
            // Accept both camelCase and snake_case
            a.deviceId = j.value("deviceId", j.value("device_id", ""));
            a.property = j.value("property", "");
            if (j.contains("value")) a.value = j["value"];
            break;

        case ActionType::Delay:
            a.delayMs = j.value("delayMs", j.value("delay_ms", 0));
            break;
    }

    return a;
}

// Automation serialization
nlohmann::json Automation::toJson() const {
    nlohmann::json j;
    j["id"] = id;
    j["name"] = name;
    j["description"] = description;
    j["enabled"] = enabled;

    // Output single trigger for API compatibility
    if (!triggers.empty()) {
        j["trigger"] = triggers[0].toJson();
    }

    if (condition.has_value()) {
        j["conditions"] = nlohmann::json::array({condition->toJson()});
    }

    nlohmann::json actionsArray = nlohmann::json::array();
    for (const auto& action : actions) {
        actionsArray.push_back(action.toJson());
    }
    j["actions"] = actionsArray;

    j["lastTriggered"] = lastTriggeredAt;

    return j;
}

Automation Automation::fromJson(const nlohmann::json& j) {
    Automation a;
    a.id = j.value("id", "");
    a.name = j.value("name", "");
    a.description = j.value("description", "");
    a.enabled = j.value("enabled", true);

    // Accept both singular "trigger" and plural "triggers"
    if (j.contains("trigger") && j["trigger"].is_object()) {
        a.triggers.push_back(Trigger::fromJson(j["trigger"]));
    } else if (j.contains("triggers") && j["triggers"].is_array()) {
        for (const auto& trigger : j["triggers"]) {
            a.triggers.push_back(Trigger::fromJson(trigger));
        }
    }

    if (j.contains("condition") && !j["condition"].is_null()) {
        a.condition = Condition::fromJson(j["condition"]);
    }

    if (j.contains("actions") && j["actions"].is_array()) {
        for (const auto& action : j["actions"]) {
            a.actions.push_back(Action::fromJson(action));
        }
    }

    a.createdAt = j.value("created_at", 0ULL);
    a.updatedAt = j.value("updated_at", 0ULL);
    a.lastTriggeredAt = j.value("last_triggered_at", j.value("lastTriggered", 0ULL));

    return a;
}

} // namespace smarthub
