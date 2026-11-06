/**
 * Automation Data Structures Implementation
 *
 * JSON serialization for automation rules.
 */

#include "smarthub/automation/Automation.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

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
            t.deviceId = j.value("deviceId", j.value("device_id", ""));
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
            t.deviceId = j.value("deviceId", j.value("device_id", ""));
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
    j["type"] = actionTypeToString(type);

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
    // Saved automations didn't always record the type, and only delays have this field
    const bool savedDelay = !j.contains("type") && !j.contains("deviceId") &&
                            !j.contains("device_id") &&
                            (j.contains("delayMs") || j.contains("delay_ms"));
    a.type = savedDelay ? ActionType::Delay
                        : stringToActionType(j.value("type", "set_device_state"));

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

    // "trigger" for the usual single trigger, and "triggers" when there are several
    if (triggers.size() == 1) {
        j["trigger"] = triggers[0].toJson();
    } else if (triggers.size() > 1) {
        nlohmann::json triggersArray = nlohmann::json::array();
        for (const auto& trigger : triggers) {
            triggersArray.push_back(trigger.toJson());
        }
        j["triggers"] = triggersArray;
    }

    if (condition.has_value()) {
        j["condition"] = condition->toJson();
    }

    nlohmann::json actionsArray = nlohmann::json::array();
    for (const auto& action : actions) {
        actionsArray.push_back(action.toJson());
    }
    j["actions"] = actionsArray;

    j["lastTriggered"] = lastTriggeredAt;

    return j;
}

namespace {

constexpr int kMaxTotalDelayMs = Automation::kMaxTotalDelayMs;

// About a year, and small enough to convert to milliseconds safely
constexpr int kMaxIntervalMinutes = 525600;

// A whole number field, or the fallback if it's absent. Reading other JSON numbers as an int
// would round or wrap them, which could turn a bad value into a good one, so they read as -1000.
int64_t wholeNumber(const nlohmann::json& j, const char* key, int64_t fallback) {
    if (!j.contains(key)) return fallback;
    const auto& value = j[key];
    if (!value.is_number_integer()) return -1000;
    // Values too big for the signed type would wrap, possibly to the -1 wildcard
    if (value.is_number_unsigned() && value.get<uint64_t>() > INT64_MAX) return -1000;
    return value.get<int64_t>();
}

// JSON values are copied and written out recursively, so a request can't be allowed to nest
// them deeply. This has to be checked without recursing.
bool nestedTooDeeply(const nlohmann::json& root) {
    constexpr int kMaxDepth = 32;
    std::vector<std::pair<const nlohmann::json*, int>> pending = {{&root, 0}};
    while (!pending.empty()) {
        const auto [value, depth] = pending.back();
        pending.pop_back();
        if (depth > kMaxDepth) return true;
        if (value->is_structured()) {
            for (const auto& child : *value) {
                pending.emplace_back(&child, depth + 1);
            }
        }
    }
    return false;
}

bool isCompareOp(const std::string& op) {
    return op == "eq" || op == "neq" || op == "gt" || op == "gte" || op == "lt" || op == "lte";
}

std::string validateTrigger(const nlohmann::json& trigger) {
    if (!trigger.is_object()) return "a trigger must be an object";

    const std::string type = trigger.value("type", "device_state");
    if (type == "device_state" || type == "sensor_threshold") {
        if (trigger.contains("value")) {
            return "a trigger compares with to_value, from_value or threshold, not value";
        }
        if (trigger.value("deviceId", trigger.value("device_id", "")).empty()) {
            return type + " trigger needs a device_id";
        }
        const std::string property = trigger.value("property", "");
        if (property.empty()) return type + " trigger needs a property";
        // DeviceManager publishes events like _removed while holding its lock
        if (property[0] == '_') return "a trigger can't use the internal property " + property;
        if (type == "sensor_threshold") {
            const std::string op = trigger.value("compare_op", "");
            if (!isCompareOp(op)) return "unknown compare_op: " + op;
            if (!trigger.contains("threshold") || !trigger["threshold"].is_number() ||
                !std::isfinite(trigger["threshold"].get<double>())) {
                return "sensor_threshold trigger needs a numeric threshold";
            }
        }
    } else if (type == "time") {
        // -1 means any hour or any minute, and has to be asked for
        const int64_t hour = wholeNumber(trigger, "hour", -2);
        const int64_t minute = wholeNumber(trigger, "minute", -2);
        if (hour < -1 || hour > 23) return "time trigger needs an hour from 0 to 23, or -1";
        if (minute < -1 || minute > 59) return "time trigger needs a minute from 0 to 59, or -1";
        if (trigger.contains("days_of_week")) {
            if (!trigger["days_of_week"].is_array()) return "days_of_week must be a list";
            for (const auto& day : trigger["days_of_week"]) {
                if (!day.is_number_integer() || day.get<int64_t>() < 0 || day.get<int64_t>() > 6) {
                    return "days_of_week values must be from 0 to 6";
                }
            }
        }
    } else if (type == "time_interval") {
        const int64_t minutes = wholeNumber(trigger, "interval_minutes", 0);
        if (minutes <= 0 || minutes > kMaxIntervalMinutes) {
            return "time_interval trigger needs an interval_minutes from 1 to " +
                   std::to_string(kMaxIntervalMinutes);
        }
    } else {
        return "unknown trigger type: " + type;
    }
    return "";
}

std::string validateAction(const nlohmann::json& action) {
    if (!action.is_object()) return "an action must be an object";

    const std::string type = action.value("type", "set_device_state");
    if (type == "set_device_state") {
        if (action.value("deviceId", action.value("device_id", "")).empty()) {
            return "set_device_state action needs a deviceId";
        }
        if (action.value("property", "").empty()) {
            return "set_device_state action needs a property";
        }
        if (!action.contains("value") || action["value"].is_null()) {
            return "set_device_state action needs a value";
        }
    } else if (type == "delay") {
        const int64_t delayMs = wholeNumber(action, "delayMs", wholeNumber(action, "delay_ms", 0));
        if (delayMs <= 0 || delayMs > kMaxTotalDelayMs) {
            return "delay action needs a delayMs from 1 to " + std::to_string(kMaxTotalDelayMs);
        }
    } else {
        return "unknown action type: " + type;
    }
    return "";
}

std::string validateCondition(const nlohmann::json& condition, int depth = 0) {
    // Conditions are handled recursively, so a request can't be allowed to nest them deeply
    constexpr int kMaxConditionDepth = 8;
    if (depth > kMaxConditionDepth) return "conditions are nested too deeply";
    if (!condition.is_object()) return "a condition must be an object";

    if (condition.value("is_group", false)) {
        const std::string op = condition.value("group_op", "and");
        if (op != "and" && op != "or") return "unknown group_op: " + op;
        if (!condition.contains("children") || !condition["children"].is_array() ||
            condition["children"].empty()) {
            return "a condition group needs children";
        }
        for (const auto& child : condition["children"]) {
            const std::string problem = validateCondition(child, depth + 1);
            if (!problem.empty()) return problem;
        }
        return "";
    }

    if (!condition.contains("leaf") || !condition["leaf"].is_object()) {
        return "a condition needs a leaf, or is_group and children";
    }
    const auto& leaf = condition["leaf"];
    if (leaf.value("device_id", "").empty()) return "a condition needs a device_id";
    if (leaf.value("property", "").empty()) return "a condition needs a property";
    const std::string op = leaf.value("op", "");
    if (!isCompareOp(op)) return "unknown condition op: " + op;
    if (!leaf.contains("value")) return "a condition needs a value";
    if (op != "eq" && op != "neq" && !leaf["value"].is_number()) {
        return "a condition using " + op + " needs a numeric value";
    }
    return "";
}

// "trigger" for a single trigger, or "triggers" for several
nlohmann::json triggerList(const nlohmann::json& j) {
    if (j.contains("triggers") && j["triggers"].is_array()) {
        return j["triggers"];
    }
    nlohmann::json triggers = nlohmann::json::array();
    if (j.contains("trigger")) {
        triggers.push_back(j["trigger"]);
    }
    return triggers;
}

std::string validateFields(const nlohmann::json& j) {
    if (nestedTooDeeply(j)) return "the request is nested too deeply";
    if (j.value("name", "").empty()) return "name is required";
    if (j.contains("enabled") && !j["enabled"].is_boolean()) return "enabled must be true or false";
    if (j.contains("description") && !j["description"].is_string()) {
        return "description must be text";
    }

    // The API addresses automations by id in the URL path
    const std::string id = j.value("id", "");
    constexpr size_t kMaxIdLength = 64;
    if (id.size() > kMaxIdLength ||
        !std::all_of(id.begin(), id.end(), [](unsigned char ch) {
            return std::isalnum(ch) || ch == '_' || ch == '-';
        })) {
        return "id can only use letters, digits, _ and -, up to 64 characters";
    }

    // Previous responses used this name, and quietly dropping a condition is dangerous
    if (j.contains("conditions")) return "use condition rather than conditions";

    if (j.contains("trigger") && j.contains("triggers")) {
        return "send either trigger or triggers, not both";
    }
    const nlohmann::json triggers = triggerList(j);
    if (triggers.empty()) return "at least one trigger is required";
    for (const auto& trigger : triggers) {
        const std::string problem = validateTrigger(trigger);
        if (!problem.empty()) return problem;
    }

    if (!j.contains("actions") || !j["actions"].is_array() || j["actions"].empty()) {
        return "at least one action is required";
    }
    int64_t totalDelayMs = 0;
    for (const auto& action : j["actions"]) {
        const std::string problem = validateAction(action);
        if (!problem.empty()) return problem;
        if (action.value("type", "") == "delay") {
            totalDelayMs += wholeNumber(action, "delayMs", wholeNumber(action, "delay_ms", 0));
            if (totalDelayMs > kMaxTotalDelayMs) {
                return "delays can total at most " + std::to_string(kMaxTotalDelayMs) + " ms";
            }
        }
    }

    if (j.contains("condition") && !j["condition"].is_null()) {
        const std::string problem = validateCondition(j["condition"]);
        if (!problem.empty()) return problem;
    }
    return "";
}

}  // namespace

std::string Automation::validateJson(const nlohmann::json& j) {
    try {
        return validateFields(j);
    } catch (const nlohmann::json::type_error&) {
        return "a field has the wrong type";
    }
}

Automation Automation::fromJson(const nlohmann::json& j) {
    Automation a;
    a.id = j.value("id", "");
    a.name = j.value("name", "");
    a.description = j.value("description", "");
    a.enabled = j.value("enabled", true);

    for (const auto& trigger : triggerList(j)) {
        a.triggers.push_back(Trigger::fromJson(trigger));
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
