/**
 * Automation Manager Implementation
 *
 * Handles automation lifecycle, trigger evaluation, and action execution.
 */

#include "smarthub/automation/AutomationManager.hpp"
#include "smarthub/database/Database.hpp"
#include "smarthub/devices/DeviceManager.hpp"
#include "smarthub/core/Logger.hpp"

#include <chrono>
#include <ctime>
#include <random>
#include <thread>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace smarthub {

AutomationManager::AutomationManager(EventBus& eventBus, Database& database,
                                     DeviceManager& deviceManager)
    : m_eventBus(eventBus)
    , m_database(database)
    , m_deviceManager(deviceManager)
{
}

AutomationManager::~AutomationManager() {
    shutdown();
}

bool AutomationManager::initialize() {
    if (m_initialized) return true;

    LOG_INFO("Initializing automation manager");

    loadFromDatabase();
    setupEventHandlers();

    m_initialized = true;

    LOG_INFO("Automation manager initialized with %zu automations",
             m_automations.size());
    return true;
}

void AutomationManager::shutdown() {
    if (!m_initialized) return;

    LOG_INFO("Shutting down automation manager");

    if (m_eventSubscription > 0) {
        m_eventBus.unsubscribe(m_eventSubscription);
        m_eventSubscription = 0;
    }

    m_initialized = false;
}

void AutomationManager::setupEventHandlers() {
    // Subscribe to device state changes
    m_eventSubscription = m_eventBus.subscribe("device.state",
        [this](const Event& event) {
            const auto& stateEvent = static_cast<const DeviceStateEvent&>(event);

            // Convert std::any to nlohmann::json
            nlohmann::json value;
            nlohmann::json prevValue;

            try {
                if (stateEvent.value.has_value()) {
                    if (stateEvent.value.type() == typeid(bool)) {
                        value = std::any_cast<bool>(stateEvent.value);
                    } else if (stateEvent.value.type() == typeid(int)) {
                        value = std::any_cast<int>(stateEvent.value);
                    } else if (stateEvent.value.type() == typeid(double)) {
                        value = std::any_cast<double>(stateEvent.value);
                    } else if (stateEvent.value.type() == typeid(std::string)) {
                        value = std::any_cast<std::string>(stateEvent.value);
                    }
                }

                if (stateEvent.previousValue.has_value()) {
                    if (stateEvent.previousValue.type() == typeid(bool)) {
                        prevValue = std::any_cast<bool>(stateEvent.previousValue);
                    } else if (stateEvent.previousValue.type() == typeid(int)) {
                        prevValue = std::any_cast<int>(stateEvent.previousValue);
                    } else if (stateEvent.previousValue.type() == typeid(double)) {
                        prevValue = std::any_cast<double>(stateEvent.previousValue);
                    } else if (stateEvent.previousValue.type() == typeid(std::string)) {
                        prevValue = std::any_cast<std::string>(stateEvent.previousValue);
                    }
                }
            } catch (const std::bad_any_cast&) {
                // Ignore conversion errors
            }

            onDeviceStateChanged(stateEvent.deviceId, stateEvent.property,
                                value, prevValue);
        });
}

void AutomationManager::poll(uint64_t currentTimeMs) {
    if (!m_initialized) return;

    // Check time triggers once per minute
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&t);
    int currentMinute = tm->tm_min;

    if (currentMinute != m_lastCheckedMinute) {
        m_lastCheckedMinute = currentMinute;
        checkTimeTriggers();
    }

    // Check interval triggers
    checkIntervalTriggers(currentTimeMs);
}

void AutomationManager::onDeviceStateChanged(const std::string& deviceId,
                                              const std::string& property,
                                              const nlohmann::json& value,
                                              const nlohmann::json& previousValue) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& [id, automation] : m_automations) {
        if (!automation->enabled) continue;

        for (const auto& trigger : automation->triggers) {
            if (trigger.type != TriggerType::DeviceState &&
                trigger.type != TriggerType::SensorThreshold) {
                continue;
            }

            if (evaluateTrigger(trigger, deviceId, property, value, previousValue)) {
                std::string triggerInfo = "DeviceState:" + deviceId + "." + property;
                executeAutomation(automation, triggerInfo);
                break;  // Only trigger once per automation
            }
        }
    }
}

void AutomationManager::checkTimeTriggers() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& [id, automation] : m_automations) {
        if (!automation->enabled) continue;

        for (const auto& trigger : automation->triggers) {
            if (trigger.type == TriggerType::Time && evaluateTimeTrigger(trigger)) {
                executeAutomation(automation, "Time");
                break;
            }
        }
    }
}

void AutomationManager::checkIntervalTriggers(uint64_t currentTimeMs) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& [id, automation] : m_automations) {
        if (!automation->enabled) continue;

        for (const auto& trigger : automation->triggers) {
            if (trigger.type == TriggerType::TimeInterval && trigger.intervalMinutes > 0) {
                uint64_t intervalMs = trigger.intervalMinutes * 60 * 1000;
                uint64_t lastCheck = m_lastIntervalCheck[id];

                if (lastCheck == 0) {
                    // First check - initialize
                    m_lastIntervalCheck[id] = currentTimeMs;
                } else if (currentTimeMs - lastCheck >= intervalMs) {
                    m_lastIntervalCheck[id] = currentTimeMs;
                    executeAutomation(automation, "Interval:" +
                                     std::to_string(trigger.intervalMinutes) + "min");
                    break;
                }
            }
        }
    }
}

bool AutomationManager::evaluateTrigger(const Trigger& trigger,
                                         const std::string& eventDeviceId,
                                         const std::string& eventProperty,
                                         const nlohmann::json& eventValue,
                                         const nlohmann::json& eventPreviousValue) const {
    switch (trigger.type) {
        case TriggerType::DeviceState: {
            if (trigger.deviceId != eventDeviceId) return false;
            if (trigger.property != eventProperty) return false;

            // Check value transition if specified
            if (!trigger.toValue.is_null()) {
                if (eventValue != trigger.toValue) return false;
            }
            if (!trigger.fromValue.is_null()) {
                if (eventPreviousValue != trigger.fromValue) return false;
            }
            return true;
        }

        case TriggerType::SensorThreshold: {
            if (trigger.deviceId != eventDeviceId) return false;
            if (trigger.property != eventProperty) return false;

            // Compare sensor value against threshold
            if (!eventValue.is_number()) return false;
            nlohmann::json thresholdJson = trigger.threshold;
            return compareValues(eventValue, trigger.compareOp, thresholdJson);
        }

        default:
            return false;
    }
}

bool AutomationManager::evaluateTimeTrigger(const Trigger& trigger) const {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&t);

    if (trigger.hour >= 0 && tm->tm_hour != trigger.hour) return false;
    if (trigger.minute >= 0 && tm->tm_min != trigger.minute) return false;

    // Check day of week if specified
    if (!trigger.daysOfWeek.empty()) {
        bool dayMatch = false;
        for (int day : trigger.daysOfWeek) {
            if (tm->tm_wday == day) {
                dayMatch = true;
                break;
            }
        }
        if (!dayMatch) return false;
    }

    return true;
}

bool AutomationManager::evaluateCondition(const Condition& condition) const {
    if (!condition.isGroup) {
        return evaluateConditionLeaf(condition.leaf);
    }

    // Evaluate group with short-circuit logic
    if (condition.children.empty()) return true;

    if (condition.groupOp == LogicOp::And) {
        for (const auto& child : condition.children) {
            if (!evaluateCondition(child)) return false;  // Short-circuit
        }
        return true;
    } else {  // LogicOp::Or
        for (const auto& child : condition.children) {
            if (evaluateCondition(child)) return true;  // Short-circuit
        }
        return false;
    }
}

bool AutomationManager::evaluateConditionLeaf(const ConditionLeaf& leaf) const {
    auto device = m_deviceManager.getDevice(leaf.deviceId);
    if (!device) return false;

    nlohmann::json actualValue = device->getProperty(leaf.property);
    return compareValues(actualValue, leaf.op, leaf.value);
}

bool AutomationManager::compareValues(const nlohmann::json& actual,
                                       CompareOp op,
                                       const nlohmann::json& expected) const {
    // Handle null cases
    if (actual.is_null() || expected.is_null()) {
        return (op == CompareOp::Equal) ? (actual == expected)
                                        : (actual != expected);
    }

    // Numeric comparison
    if (actual.is_number() && expected.is_number()) {
        double a = actual.get<double>();
        double e = expected.get<double>();

        switch (op) {
            case CompareOp::Equal:          return std::abs(a - e) < 0.001;
            case CompareOp::NotEqual:       return std::abs(a - e) >= 0.001;
            case CompareOp::GreaterThan:    return a > e;
            case CompareOp::GreaterOrEqual: return a >= e;
            case CompareOp::LessThan:       return a < e;
            case CompareOp::LessOrEqual:    return a <= e;
        }
    }

    // Boolean comparison
    if (actual.is_boolean() && expected.is_boolean()) {
        bool a = actual.get<bool>();
        bool e = expected.get<bool>();
        return (op == CompareOp::Equal) ? (a == e) : (a != e);
    }

    // String comparison
    if (actual.is_string() && expected.is_string()) {
        return (op == CompareOp::Equal) ? (actual == expected)
                                        : (actual != expected);
    }

    // Type mismatch - compare as strings
    return (op == CompareOp::Equal) ? (actual.dump() == expected.dump())
                                    : (actual.dump() != expected.dump());
}

void AutomationManager::executeAutomation(AutomationPtr automation,
                                           const std::string& triggerInfo) {
    if (!automation || !automation->enabled) return;

    // Check condition if present
    if (automation->condition.has_value()) {
        if (!evaluateCondition(*automation->condition)) {
            LOG_DEBUG("Automation '%s' condition not met, skipping",
                     automation->name.c_str());
            return;
        }
    }

    LOG_INFO("Executing automation: %s (trigger: %s)",
             automation->name.c_str(), triggerInfo.c_str());

    // Update last triggered timestamp
    automation->lastTriggeredAt = getCurrentTimestamp();
    saveToDatabase(*automation);

    // Execute actions sequentially
    for (const auto& action : automation->actions) {
        if (action.type == ActionType::Delay) {
            std::this_thread::sleep_for(std::chrono::milliseconds(action.delayMs));
        } else {
            executeAction(action);
        }
    }
}

void AutomationManager::executeAction(const Action& action) {
    switch (action.type) {
        case ActionType::SetDeviceState:
            LOG_DEBUG("Action: Set %s.%s = %s",
                     action.deviceId.c_str(),
                     action.property.c_str(),
                     action.value.dump().c_str());
            m_deviceManager.setDeviceState(action.deviceId, action.property, action.value);
            break;

        case ActionType::Delay:
            // Handled in executeAutomation
            break;
    }
}

// CRUD Operations
bool AutomationManager::addAutomation(const Automation& automation) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (automation.id.empty()) {
        LOG_ERROR("Cannot add automation with empty ID");
        return false;
    }

    if (m_automations.count(automation.id) > 0) {
        LOG_ERROR("Automation with ID '%s' already exists", automation.id.c_str());
        return false;
    }

    auto ptr = std::make_shared<Automation>(automation);
    ptr->createdAt = getCurrentTimestamp();
    ptr->updatedAt = ptr->createdAt;

    if (!saveToDatabase(*ptr)) {
        return false;
    }

    m_automations[automation.id] = ptr;
    LOG_INFO("Added automation: %s", automation.name.c_str());
    return true;
}

bool AutomationManager::updateAutomation(const Automation& automation) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_automations.find(automation.id);
    if (it == m_automations.end()) {
        LOG_ERROR("Automation '%s' not found", automation.id.c_str());
        return false;
    }

    auto ptr = std::make_shared<Automation>(automation);
    ptr->createdAt = it->second->createdAt;
    ptr->updatedAt = getCurrentTimestamp();

    if (!saveToDatabase(*ptr)) {
        return false;
    }

    m_automations[automation.id] = ptr;
    LOG_INFO("Updated automation: %s", automation.name.c_str());
    return true;
}

bool AutomationManager::deleteAutomation(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_automations.find(id);
    if (it == m_automations.end()) {
        LOG_ERROR("Automation '%s' not found", id.c_str());
        return false;
    }

    if (!deleteFromDatabase(id)) {
        return false;
    }

    std::string name = it->second->name;
    m_automations.erase(it);
    m_lastIntervalCheck.erase(id);

    LOG_INFO("Deleted automation: %s", name.c_str());
    return true;
}

AutomationPtr AutomationManager::getAutomation(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_automations.find(id);
    return (it != m_automations.end()) ? it->second : nullptr;
}

std::vector<AutomationPtr> AutomationManager::getAllAutomations() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<AutomationPtr> result;
    result.reserve(m_automations.size());

    for (const auto& [id, automation] : m_automations) {
        result.push_back(automation);
    }

    return result;
}

bool AutomationManager::setEnabled(const std::string& id, bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_automations.find(id);
    if (it == m_automations.end()) {
        return false;
    }

    it->second->enabled = enabled;
    it->second->updatedAt = getCurrentTimestamp();
    return saveToDatabase(*it->second);
}

bool AutomationManager::triggerAutomation(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_automations.find(id);
    if (it == m_automations.end()) {
        return false;
    }

    executeAutomation(it->second, "Manual");
    return true;
}

size_t AutomationManager::automationCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_automations.size();
}

std::string AutomationManager::generateId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";

    std::string id = "auto_";
    for (int i = 0; i < 8; ++i) {
        id += hex[dis(gen)];
    }
    return id;
}

// Persistence
void AutomationManager::loadFromDatabase() {
    auto stmt = m_database.prepare(
        "SELECT id, name, description, enabled, triggers, condition, actions, "
        "created_at, updated_at, last_triggered_at FROM automations");

    if (!stmt || !stmt->isValid()) {
        LOG_ERROR("Failed to prepare automation query");
        return;
    }

    while (stmt->step()) {
        try {
            Automation automation;
            automation.id = stmt->getString(0);
            automation.name = stmt->getString(1);
            automation.description = stmt->isNull(2) ? "" : stmt->getString(2);
            automation.enabled = (stmt->getInt(3) == 1);

            // Parse triggers JSON
            std::string triggersStr = stmt->getString(4);
            if (!triggersStr.empty()) {
                auto triggersJson = nlohmann::json::parse(triggersStr);
                for (const auto& tj : triggersJson) {
                    automation.triggers.push_back(Trigger::fromJson(tj));
                }
            }

            // Parse condition JSON (may be null)
            std::string conditionStr = stmt->isNull(5) ? "" : stmt->getString(5);
            if (!conditionStr.empty()) {
                auto conditionJson = nlohmann::json::parse(conditionStr);
                if (!conditionJson.is_null()) {
                    automation.condition = Condition::fromJson(conditionJson);
                }
            }

            // Parse actions JSON
            std::string actionsStr = stmt->getString(6);
            if (!actionsStr.empty()) {
                auto actionsJson = nlohmann::json::parse(actionsStr);
                for (const auto& aj : actionsJson) {
                    automation.actions.push_back(Action::fromJson(aj));
                }
            }

            automation.createdAt = static_cast<uint64_t>(stmt->getInt64(7));
            automation.updatedAt = static_cast<uint64_t>(stmt->getInt64(8));
            if (!stmt->isNull(9)) {
                automation.lastTriggeredAt = static_cast<uint64_t>(stmt->getInt64(9));
            }

            m_automations[automation.id] = std::make_shared<Automation>(automation);

        } catch (const std::exception& e) {
            LOG_ERROR("Failed to load automation: %s", e.what());
        }
    }
}

bool AutomationManager::saveToDatabase(const Automation& automation) {
    nlohmann::json triggersJson = nlohmann::json::array();
    for (const auto& trigger : automation.triggers) {
        triggersJson.push_back(trigger.toJson());
    }

    nlohmann::json conditionJson = nullptr;
    if (automation.condition.has_value()) {
        conditionJson = automation.condition->toJson();
    }

    nlohmann::json actionsJson = nlohmann::json::array();
    for (const auto& action : automation.actions) {
        actionsJson.push_back(action.toJson());
    }

    auto stmt = m_database.prepare(R"(
        INSERT OR REPLACE INTO automations
        (id, name, description, enabled, triggers, condition, actions,
         created_at, updated_at, last_triggered_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");

    if (!stmt || !stmt->isValid()) {
        LOG_ERROR("Failed to prepare save automation statement");
        return false;
    }

    stmt->bind(1, automation.id);
    stmt->bind(2, automation.name);
    stmt->bind(3, automation.description);
    stmt->bind(4, automation.enabled ? 1 : 0);
    stmt->bind(5, triggersJson.dump());
    if (conditionJson.is_null()) {
        stmt->bindNull(6);
    } else {
        stmt->bind(6, conditionJson.dump());
    }
    stmt->bind(7, actionsJson.dump());
    stmt->bind(8, static_cast<int64_t>(automation.createdAt));
    stmt->bind(9, static_cast<int64_t>(automation.updatedAt));
    stmt->bind(10, static_cast<int64_t>(automation.lastTriggeredAt));

    return stmt->execute();
}

bool AutomationManager::deleteFromDatabase(const std::string& id) {
    auto stmt = m_database.prepare("DELETE FROM automations WHERE id = ?");
    if (!stmt || !stmt->isValid()) {
        LOG_ERROR("Failed to prepare delete automation statement");
        return false;
    }
    stmt->bind(1, id);
    return stmt->execute();
}

uint64_t AutomationManager::getCurrentTimestamp() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace smarthub
