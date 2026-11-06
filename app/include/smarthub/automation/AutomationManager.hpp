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
#include <chrono>
#include <condition_variable>
#include <thread>
#include <utility>
#include <mutex>
#include <vector>
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
     * Stop the worker thread, dropping any runs still queued or in a delay
     */
    void shutdown();

    /**
     * Check time-based triggers (call from main loop). Actions run on a worker thread, as they
     * do for every trigger, so this and publishing a device change don't wait for them.
     * @param currentTimeMs Current time in milliseconds
     */
    void poll(uint64_t currentTimeMs);

    /**
     * Check an automation before adding or updating it
     * @return A description of the problem, or an empty string if it can run
     */
    std::string problemWith(const Automation& automation) const;

    // CRUD Operations
    bool addAutomation(const Automation& automation);
    bool updateAutomation(const Automation& automation);
    bool deleteAutomation(const std::string& id);
    // These return copies: other threads run and change the stored automations
    AutomationPtr getAutomation(const std::string& id) const;
    std::vector<AutomationPtr> getAllAutomations() const;

    /**
     * Enable/disable an automation
     */
    bool setEnabled(const std::string& id, bool enabled);

    /**
     * Manually trigger an automation. It runs on the worker thread.
     * @return Whether it was queued, which needs it to be enabled and its condition to hold
     */
    bool triggerAutomation(const std::string& id);

    /**
     * Wait until no runs are queued or in a delay (for tests)
     */
    void waitForRuns();

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

    // Execution engine. A run is a copy of the automation, taken when it was triggered, which
    // the worker thread steps through. A delay puts it back in the queue for later.
    struct Run {
        Automation automation;
        std::string triggerInfo;
        size_t nextAction = 0;
        int64_t delayBudgetMs = Automation::kMaxTotalDelayMs;
        int depth = 0;  // How many automations in a row led to this one
    };
    using RunQueue = std::multimap<std::chrono::steady_clock::time_point, Run>;

    bool enqueueRuns(std::vector<std::pair<Automation, std::string>> runs);
    void workerLoop();
    void continueRun(Run run);
    void executeAction(const Action& action);

    // Persistence
    void loadFromDatabase();
    bool saveToDatabase(const Automation& automation);
    bool deleteFromDatabase(const std::string& id);

    void clearIntervalTimers(const std::string& id);

    // Get current timestamp
    uint64_t getCurrentTimestamp() const;

    EventBus& m_eventBus;
    Database& m_database;
    DeviceManager& m_deviceManager;

    std::map<std::string, AutomationPtr> m_automations;
    mutable std::mutex m_mutex;

    uint64_t m_eventSubscription = 0;

    // Worker thread and its queue, ordered by when each run is next due
    std::thread m_worker;
    std::mutex m_queueMutex;
    std::condition_variable m_queueChanged;
    RunQueue m_runQueue;
    struct RunRate {
        std::chrono::steady_clock::time_point windowStart;
        int runs = 0;
    };
    std::map<std::string, RunRate> m_runRates;
    bool m_stopping = false;
    bool m_workerBusy = false;

    // Timer state
    int m_lastCheckedMinute = -1;
    std::map<std::pair<std::string, size_t>, uint64_t> m_lastIntervalCheck;  // (id, trigger index)

    bool m_initialized = false;
};

} // namespace smarthub
