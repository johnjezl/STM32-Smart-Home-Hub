/**
 * Automation Manager Implementation
 *
 * Handles automation lifecycle, trigger evaluation, and action execution.
 */

#include "smarthub/automation/AutomationManager.hpp"
#include "smarthub/database/Database.hpp"
#include "smarthub/devices/DeviceManager.hpp"
#include "smarthub/core/Logger.hpp"

#include <algorithm>
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

    // Start from this minute, so a restart doesn't run a time trigger that already ran in it
    const std::time_t startTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm startTm{};
    localtime_r(&startTime, &startTm);
    m_lastCheckedMinute = startTm.tm_min;

    m_stopping = false;
    m_worker = std::thread(&AutomationManager::workerLoop, this);

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

    RunQueue dropped;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_stopping = true;
        dropped.swap(m_runQueue);
    }
    m_queueChanged.notify_all();
    if (m_worker.joinable()) {
        m_worker.join();
    }
    for (const auto& [due, run] : dropped) {
        LOG_INFO("Automation %s stopped by shutdown", run.automation.name.c_str());
    }

    m_initialized = false;
}

namespace {

// DeviceManager publishes JSON values, and other publishers use plain types
nlohmann::json eventValueToJson(const std::any& value) {
    if (!value.has_value()) return nullptr;
    if (const auto* json = std::any_cast<nlohmann::json>(&value)) return *json;
    if (const auto* flag = std::any_cast<bool>(&value)) return *flag;
    if (const auto* number = std::any_cast<int>(&value)) return *number;
    if (const auto* number = std::any_cast<double>(&value)) return *number;
    if (const auto* text = std::any_cast<std::string>(&value)) return *text;
    if (const auto* text = std::any_cast<const char*>(&value)) return std::string(*text);
    return nullptr;
}

} // namespace

void AutomationManager::setupEventHandlers() {
    // Subscribe to device state changes
    m_eventSubscription = m_eventBus.subscribe("device.state",
        [this](const Event& event) {
            const auto& stateEvent = static_cast<const DeviceStateEvent&>(event);

            onDeviceStateChanged(stateEvent.deviceId, stateEvent.property,
                                 eventValueToJson(stateEvent.value),
                                 eventValueToJson(stateEvent.previousValue));
        });
}

void AutomationManager::poll(uint64_t currentTimeMs) {
    if (!m_initialized) return;

    // Check time triggers once per minute
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    int currentMinute = tm.tm_min;

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
    // Internal events such as _removed aren't changes to a device
    if (!property.empty() && property[0] == '_') return;

    std::vector<std::pair<Automation, std::string>> runs;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        for (const auto& [id, automation] : m_automations) {
            if (!automation->enabled) continue;

            for (const auto& trigger : automation->triggers) {
                if (trigger.type != TriggerType::DeviceState &&
                    trigger.type != TriggerType::SensorThreshold) {
                    continue;
                }

                if (evaluateTrigger(trigger, deviceId, property, value, previousValue)) {
                    runs.emplace_back(*automation, "DeviceState:" + deviceId + "." + property);
                    break;  // Only trigger once per automation
                }
            }
        }
    }
    enqueueRuns(std::move(runs));
}

void AutomationManager::checkTimeTriggers() {
    std::vector<std::pair<Automation, std::string>> runs;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        for (const auto& [id, automation] : m_automations) {
            if (!automation->enabled) continue;

            for (const auto& trigger : automation->triggers) {
                if (trigger.type == TriggerType::Time && evaluateTimeTrigger(trigger)) {
                    runs.emplace_back(*automation, "Time");
                    break;
                }
            }
        }
    }
    enqueueRuns(std::move(runs));
}

void AutomationManager::checkIntervalTriggers(uint64_t currentTimeMs) {
    std::vector<std::pair<Automation, std::string>> runs;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        for (const auto& [id, automation] : m_automations) {
            if (!automation->enabled) continue;

            bool due = false;
            for (size_t index = 0; index < automation->triggers.size(); ++index) {
                const auto& trigger = automation->triggers[index];
                if (trigger.type == TriggerType::TimeInterval && trigger.intervalMinutes > 0) {
                    const auto timer = std::make_pair(id, index);
                    uint64_t intervalMs = static_cast<uint64_t>(trigger.intervalMinutes) * 60 * 1000;
                    uint64_t lastCheck = m_lastIntervalCheck[timer];

                    if (lastCheck == 0) {
                        // First check - initialize
                        m_lastIntervalCheck[timer] = currentTimeMs;
                    } else if (currentTimeMs - lastCheck >= intervalMs) {
                        m_lastIntervalCheck[timer] = currentTimeMs;
                        // Triggers due together run the automation once
                        if (!due) {
                            due = true;
                            runs.emplace_back(*automation, "Interval:" + std::to_string(trigger.intervalMinutes) + "min");
                        }
                    }
                }
            }
        }
    }
    enqueueRuns(std::move(runs));
}

namespace {

// How many automations in a row led to what this thread is running. Actions can trigger other
// automations, and opposing ones would otherwise go on forever.
thread_local int t_runDepth = 0;
constexpr int kMaxRunDepth = 8;

// More than this waiting means automations are firing far faster than they can run
constexpr size_t kMaxQueuedRuns = 256;

}  // namespace

void AutomationManager::enqueueRuns(std::vector<std::pair<Automation, std::string>> runs) {
    if (runs.empty()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        for (auto& [automation, triggerInfo] : runs) {
            if (m_stopping || !m_worker.joinable()) {
                return;
            }
            if (t_runDepth >= kMaxRunDepth) {
                LOG_ERROR("Automation '%s' not run: automations are triggering each other in a loop",
                          automation.name.c_str());
                continue;
            }
            if (m_runQueue.size() >= kMaxQueuedRuns) {
                LOG_ERROR("Automation '%s' not run: too many runs are waiting",
                          automation.name.c_str());
                continue;
            }

            Run run;
            run.automation = std::move(automation);
            run.triggerInfo = std::move(triggerInfo);
            run.depth = t_runDepth;
            m_runQueue.emplace(std::chrono::steady_clock::now(), std::move(run));
        }
    }
    m_queueChanged.notify_all();
}

void AutomationManager::workerLoop() {
    std::unique_lock<std::mutex> lock(m_queueMutex);
    while (!m_stopping) {
        if (m_runQueue.empty()) {
            m_queueChanged.wait(lock);
            continue;
        }

        const auto next = m_runQueue.begin();
        if (next->first > std::chrono::steady_clock::now()) {
            m_queueChanged.wait_until(lock, next->first);
            continue;
        }

        Run run = std::move(next->second);
        m_runQueue.erase(next);
        m_workerBusy = true;

        // Unlocked: actions can wait on devices, and changing one publishes an event that comes
        // straight back to onDeviceStateChanged on this thread.
        lock.unlock();
        continueRun(std::move(run));
        lock.lock();

        m_workerBusy = false;
        m_queueChanged.notify_all();
    }
}

void AutomationManager::waitForRuns() {
    std::unique_lock<std::mutex> lock(m_queueMutex);
    m_queueChanged.wait(lock, [this] { return m_stopping || (m_runQueue.empty() && !m_workerBusy); });
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
    std::tm tm{};
    localtime_r(&t, &tm);

    if (trigger.hour >= 0 && tm.tm_hour != trigger.hour) return false;
    if (trigger.minute >= 0 && tm.tm_min != trigger.minute) return false;

    // Check day of week if specified
    if (!trigger.daysOfWeek.empty()) {
        bool dayMatch = false;
        for (int day : trigger.daysOfWeek) {
            if (tm.tm_wday == day) {
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
    // Only numbers can be ordered, so an ordering test on anything else hasn't held
    const bool ordering = op != CompareOp::Equal && op != CompareOp::NotEqual;
    if (ordering && !(actual.is_number() && expected.is_number())) {
        return false;
    }

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

void AutomationManager::continueRun(Run run) {
    const Automation& automation = run.automation;

    // Whatever this run's actions trigger counts as one further along
    t_runDepth = run.depth + 1;
    struct ResetDepth {
        ~ResetDepth() { t_runDepth = 0; }
    } resetDepth;

    if (run.nextAction == 0) {
        if (!automation.enabled) return;

        if (automation.condition.has_value() && !evaluateCondition(*automation.condition)) {
            LOG_DEBUG("Automation '%s' condition not met, skipping", automation.name.c_str());
            return;
        }

        LOG_INFO("Executing automation: %s (trigger: %s)",
                 automation.name.c_str(), run.triggerInfo.c_str());

        std::lock_guard<std::mutex> lock(m_mutex);
        auto stored = m_automations.find(automation.id);
        if (stored != m_automations.end()) {
            stored->second->lastTriggeredAt = getCurrentTimestamp();
            saveToDatabase(*stored->second);
        }
    } else {
        // It could have been turned off or removed during a delay. An edit doesn't stop this
        // run, which finishes the actions it started with.
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto stored = m_automations.find(automation.id);
        if (stored == m_automations.end() || !stored->second->enabled) {
            LOG_INFO("Automation %s stopped during a delay", automation.name.c_str());
            return;
        }
    }

    while (run.nextAction < automation.actions.size()) {
        const Action& action = automation.actions[run.nextAction];
        ++run.nextAction;

        if (action.type != ActionType::Delay) {
            executeAction(action);
            continue;
        }

        // Older saved automations could have longer delays than validation allows
        const int64_t delayMs = std::min<int64_t>(action.delayMs, run.delayBudgetMs);
        run.delayBudgetMs -= delayMs;
        const auto due = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs);
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (!m_stopping) {
                m_runQueue.emplace(due, std::move(run));
            }
        }
        return;
    }
}

void AutomationManager::executeAction(const Action& action) {
    switch (action.type) {
        case ActionType::SetDeviceState: {
            LOG_DEBUG("Action: Set %s.%s = %s",
                     action.deviceId.c_str(),
                     action.property.c_str(),
                     action.value.dump().c_str());
            // Devices can change after an automation is saved, and the LCD wizard takes free text
            const auto device = m_deviceManager.getDevice(action.deviceId);
            if (!device ||
                !DeviceManager::isControllableProperty(*device, action.property, action.value)) {
                LOG_WARN("Skipping action: can't set %s.%s to %s", action.deviceId.c_str(),
                         action.property.c_str(), action.value.dump().c_str());
                break;
            }
            if (!m_deviceManager.setDeviceState(action.deviceId, action.property, action.value)) {
                LOG_WARN("Action failed: set %s.%s to %s", action.deviceId.c_str(),
                         action.property.c_str(), action.value.dump().c_str());
            }
            break;
        }

        case ActionType::Delay:
            // Handled in executeAutomation
            break;
    }
}

std::string AutomationManager::problemWith(const Automation& automation) const {
    const std::string problem = Automation::validateJson(automation.toJson());
    if (!problem.empty()) return problem;

    // Devices might be added later, so only the ones that exist can be checked.
    // executeAction checks again when the action runs.
    for (const auto& action : automation.actions) {
        if (action.type != ActionType::SetDeviceState) continue;

        const auto device = m_deviceManager.getDevice(action.deviceId);
        if (device &&
            !DeviceManager::isControllableProperty(*device, action.property, action.value)) {
            return "action can't set " + action.property + " to that value on " + device->id();
        }
    }
    return "";
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
    clearIntervalTimers(automation.id);
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
    clearIntervalTimers(id);

    LOG_INFO("Deleted automation: %s", name.c_str());
    return true;
}

void AutomationManager::clearIntervalTimers(const std::string& id) {
    auto it = m_lastIntervalCheck.lower_bound(std::make_pair(id, size_t{0}));
    while (it != m_lastIntervalCheck.end() && it->first.first == id) {
        it = m_lastIntervalCheck.erase(it);
    }
}

AutomationPtr AutomationManager::getAutomation(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_automations.find(id);
    return (it != m_automations.end()) ? std::make_shared<Automation>(*it->second) : nullptr;
}

std::vector<AutomationPtr> AutomationManager::getAllAutomations() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<AutomationPtr> result;
    result.reserve(m_automations.size());

    for (const auto& [id, automation] : m_automations) {
        result.push_back(std::make_shared<Automation>(*automation));
    }

    return result;
}

bool AutomationManager::setEnabled(const std::string& id, bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_automations.find(id);
    if (it == m_automations.end()) {
        return false;
    }

    if (enabled && !it->second->enabled) {
        clearIntervalTimers(id);  // It wasn't counting while it was off
    }
    it->second->enabled = enabled;
    it->second->updatedAt = getCurrentTimestamp();
    return saveToDatabase(*it->second);
}

bool AutomationManager::triggerAutomation(const std::string& id) {
    std::vector<std::pair<Automation, std::string>> runs;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_automations.find(id);
        if (it == m_automations.end()) {
            return false;
        }
        runs.emplace_back(*it->second, "Manual");
    }

    enqueueRuns(std::move(runs));
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

            // Older versions could save automations that can't run. Say so, and keep them.
            const std::string problem = problemWith(automation);
            if (!problem.empty()) {
                LOG_WARN("Automation '%s' may not run: %s", automation.name.c_str(), problem.c_str());
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
