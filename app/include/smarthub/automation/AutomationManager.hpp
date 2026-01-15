/**
 * Automation Manager
 *
 * Manages automation rules - CRUD operations, trigger evaluation,
 * condition checking, and action execution.
 */
#pragma once

#include "smarthub/automation/Automation.hpp"
#include "smarthub/core/EventBus.hpp"

#include <map>
#include <mutex>
#include <functional>
#include <cstdint>

namespace smarthub {

class Database;
class DeviceManager;

/**
 * AutomationManager handles the lifecycle and execution of automation rules.
 *
 * Features:
 * - CRUD operations for automations (persisted to database)
 * - Device state change triggers via EventBus
 * - Time-based triggers (polled via poll() method)
 * - Sensor threshold triggers
 * - Condition evaluation with AND/OR logic
 * - Action execution with optional delays
 */
class AutomationManager {
public:
    AutomationManager(EventBus& eventBus, Database& database,
                      DeviceManager& deviceManager);
    ~AutomationManager();

    /**
     * Initialize manager and load automations from database
     */
    bool initialize();

    /**
     * Shutdown and cleanup
     */
    void shutdown();

    /**
     * Poll timer-based triggers (call from main loop)
     * @param currentTimeMs Current time in milliseconds
     */
    void poll(uint64_t currentTimeMs);

    // CRUD Operations
    bool addAutomation(const Automation& automation);
    bool updateAutomation(const Automation& automation);
    bool deleteAutomation(const std::string& id);
    AutomationPtr getAutomation(const std::string& id) const;
    std::vector<AutomationPtr> getAllAutomations() const;

    /**
     * Enable/disable an automation
     */
    bool setEnabled(const std::string& id, bool enabled);

    /**
     * Manually trigger an automation (for testing)
     */
    bool triggerAutomation(const std::string& id);

    /**
     * Get count of automations
     */
    size_t automationCount() const;

    /**
     * Generate a unique automation ID
     */
    std::string generateId();

private:
    // Event handling
    void setupEventHandlers();
    void onDeviceStateChanged(const std::string& deviceId,
                               const std::string& property,
                               const nlohmann::json& value,
                               const nlohmann::json& previousValue);

    // Timer handling
    void checkTimeTriggers();
    void checkIntervalTriggers(uint64_t currentTimeMs);

    // Evaluation engine
    bool evaluateTrigger(const Trigger& trigger,
                         const std::string& eventDeviceId,
                         const std::string& eventProperty,
                         const nlohmann::json& eventValue,
                         const nlohmann::json& eventPreviousValue) const;
    bool evaluateTimeTrigger(const Trigger& trigger) const;
    bool evaluateCondition(const Condition& condition) const;
    bool evaluateConditionLeaf(const ConditionLeaf& leaf) const;
    bool compareValues(const nlohmann::json& actual,
                       CompareOp op,
                       const nlohmann::json& expected) const;

    // Execution engine
    void executeAutomation(AutomationPtr automation,
                           const std::string& triggerInfo);
    void executeAction(const Action& action);

    // Persistence
    void loadFromDatabase();
    bool saveToDatabase(const Automation& automation);
    bool deleteFromDatabase(const std::string& id);

    // Get current timestamp
    uint64_t getCurrentTimestamp() const;

    EventBus& m_eventBus;
    Database& m_database;
    DeviceManager& m_deviceManager;

    std::map<std::string, AutomationPtr> m_automations;
    mutable std::mutex m_mutex;

    uint64_t m_eventSubscription = 0;

    // Timer state
    int m_lastCheckedMinute = -1;
    std::map<std::string, uint64_t> m_lastIntervalCheck;  // automationId -> lastCheck

    bool m_initialized = false;
};

} // namespace smarthub
