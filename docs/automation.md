# SmartHub Automation System

This document describes the automation system implemented in Phase 8.

## Overview

The automation system allows users to create rules that automatically control devices based on triggers, conditions, and actions. It provides a flexible, powerful way to automate smart home behavior.

**Key Features:**
- Multiple trigger types (device state, time-based, sensor thresholds)
- Conditional logic (AND/OR) for complex rules
- Sequential action execution with delays
- Persistence to SQLite database
- EventBus integration for real-time device state triggers

## Architecture

```
Automation System
    |
    +-- Automation (data structure)
    |       |
    |       +-- Triggers (what starts the automation)
    |       +-- Conditions (optional, when to execute)
    |       +-- Actions (what to do)
    |
    +-- AutomationManager (CRUD, evaluation, execution)
    |       |
    |       +-- EventBus subscription (device state triggers)
    |       +-- Timer polling (time-based triggers)
    |       +-- Condition evaluation engine
    |       +-- Action execution engine
    |
    +-- UI Screens
            |
            +-- AutomationListScreen
            +-- AddAutomationScreen (4-step wizard)
            +-- EditAutomationScreen
```

## Data Model

### Automation Structure

```cpp
struct Automation {
    std::string id;              // Unique identifier (e.g., "auto_123456")
    std::string name;            // User-friendly name
    std::string description;     // Optional description
    bool enabled;                // Whether automation is active
    std::vector<Trigger> triggers;
    std::optional<Condition> condition;
    std::vector<Action> actions;
    uint64_t lastTriggeredAt;    // Timestamp of last execution
};
```

### Trigger Types

| Type | Description | Configuration |
|------|-------------|---------------|
| DeviceState | Device property changes | deviceId, property, toValue (optional) |
| Time | Specific time of day | hour (0-23), minute (0-59) |
| TimeInterval | Every N minutes | intervalMinutes |
| SensorThreshold | Sensor crosses threshold | deviceId, property, compareOp, threshold |

```cpp
enum class TriggerType {
    DeviceState,      // Device property changed
    Time,             // At specific time (e.g., 6:00 PM)
    TimeInterval,     // Every N minutes
    SensorThreshold   // Sensor crosses threshold
};
```

### Compare Operators

Used for sensor thresholds and conditions:

```cpp
enum class CompareOp {
    Equal,            // ==
    NotEqual,         // !=
    GreaterThan,      // >
    LessThan,         // <
    GreaterOrEqual,   // >=
    LessOrEqual       // <=
};
```

### Conditions

Conditions use a tree structure supporting AND/OR logic:

```cpp
struct Condition {
    bool isGroup;                    // true = group node, false = leaf node
    ConditionLeaf leaf;              // When isGroup=false
    LogicOp groupOp;                 // AND or OR
    std::vector<Condition> children; // When isGroup=true
};

struct ConditionLeaf {
    std::string deviceId;
    std::string property;
    CompareOp op;
    nlohmann::json value;
};
```

Example: "Only if living room light is off AND time is after 6 PM":

```cpp
Condition cond;
cond.isGroup = true;
cond.groupOp = LogicOp::And;

Condition c1, c2;
c1.isGroup = false;
c1.leaf = {"living_room_light", "on", CompareOp::Equal, false};

c2.isGroup = false;
c2.leaf = {"time", "hour", CompareOp::GreaterOrEqual, 18};

cond.children = {c1, c2};
```

### Action Types

| Type | Description | Configuration |
|------|-------------|---------------|
| SetDeviceState | Set device property | deviceId, property, value |
| Delay | Wait before next action | delayMs |
| Notify | Send notification | message |

```cpp
enum class ActionType {
    SetDeviceState,   // Set device property to value
    Delay,            // Wait N milliseconds
    Notify            // Send notification (future)
};
```

## AutomationManager

### Initialization

```cpp
#include <smarthub/automation/AutomationManager.hpp>

AutomationManager manager(eventBus, database, deviceManager);
manager.initialize();  // Loads automations from database
```

### CRUD Operations

```cpp
// Create
Automation automation;
automation.id = manager.generateId();
automation.name = "Motion Light";
automation.enabled = true;
// ... configure triggers, actions
manager.addAutomation(automation);

// Read
auto all = manager.getAllAutomations();
auto one = manager.getAutomation("auto_123");

// Update
automation.name = "Updated Name";
manager.updateAutomation(automation);

// Delete
manager.deleteAutomation("auto_123");
```

### Enable/Disable

```cpp
manager.setEnabled("auto_123", false);  // Disable
manager.setEnabled("auto_123", true);   // Enable
```

### Manual Trigger

```cpp
// For testing
manager.triggerAutomation("auto_123");
```

### Polling

Call from main loop for time-based triggers:

```cpp
while (running) {
    uint64_t now = getCurrentTimeMs();
    manager.poll(now);
    // ... other updates
}
```

## Execution Flow

1. **Trigger Fires**
   - DeviceState: EventBus callback when device property changes
   - Time: poll() checks current time against trigger times
   - TimeInterval: poll() checks elapsed time since last execution
   - SensorThreshold: EventBus callback checks threshold crossing

2. **Find Matching Automations**
   - Iterate automations with matching trigger type
   - Check if trigger conditions match event

3. **Evaluate Conditions**
   - If automation has condition, evaluate recursively
   - AND groups short-circuit on first false
   - OR groups short-circuit on first true

4. **Execute Actions**
   - Execute actions sequentially
   - Delay actions pause execution
   - Device state actions call DeviceManager

5. **Update Last Triggered**
   - Record execution timestamp
   - Persist to database

## Database Schema

```sql
CREATE TABLE IF NOT EXISTS automations (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT,
    enabled INTEGER DEFAULT 1,
    triggers TEXT NOT NULL,      -- JSON array
    condition TEXT,              -- JSON tree (nullable)
    actions TEXT NOT NULL,       -- JSON array
    created_at INTEGER,
    updated_at INTEGER,
    last_triggered_at INTEGER
);
```

## UI Screens

### AutomationListScreen

Displays all automations with:
- Enable/disable toggle per automation
- Automation name and trigger summary
- Last triggered timestamp
- "Add Automation" card at bottom

Navigation:
- Tap automation → EditAutomationScreen
- Tap Add → AddAutomationScreen

### AddAutomationScreen (4-Step Wizard)

**Step 1: Name & Description**
- Name input (required)
- Description input (optional)

**Step 2: Trigger Configuration**
- Trigger type dropdown
- Dynamic configuration based on type:
  - DeviceState: Device, Property, Value
  - Time: Hour, Minute
  - TimeInterval: Minutes
  - SensorThreshold: Device, Operator, Value

**Step 3: Conditions (Optional)**
- Add device state conditions
- AND/OR logic toggle

**Step 4: Actions**
- Device dropdown
- Property dropdown
- Value input
- Add multiple actions with delays

### EditAutomationScreen

Edit existing automation:
- Name/description editing
- Enable/disable toggle
- View triggers, conditions, actions
- Delete confirmation dialog

## Examples

### Example 1: Motion Light

Turn on living room light when motion is detected:

```cpp
Automation auto;
auto.id = "motion_light";
auto.name = "Motion Light";
auto.enabled = true;

Trigger trigger;
trigger.type = TriggerType::DeviceState;
trigger.deviceId = "motion_sensor_1";
trigger.property = "motion";
trigger.toValue = true;
auto.triggers.push_back(trigger);

Action action;
action.type = ActionType::SetDeviceState;
action.deviceId = "living_room_light";
action.property = "on";
action.value = true;
auto.actions.push_back(action);
```

### Example 2: Evening Lights

Turn on lights at 6:00 PM:

```cpp
Automation auto;
auto.id = "evening_lights";
auto.name = "Evening Lights";
auto.enabled = true;

Trigger trigger;
trigger.type = TriggerType::Time;
trigger.hour = 18;
trigger.minute = 0;
auto.triggers.push_back(trigger);

Action action1, action2;
action1.type = ActionType::SetDeviceState;
action1.deviceId = "living_room_light";
action1.property = "on";
action1.value = true;
auto.actions.push_back(action1);

action2.type = ActionType::SetDeviceState;
action2.deviceId = "kitchen_light";
action2.property = "on";
action2.value = true;
auto.actions.push_back(action2);
```

### Example 3: Temperature Alert

Notify when temperature exceeds 80°F:

```cpp
Automation auto;
auto.id = "temp_alert";
auto.name = "Temperature Alert";
auto.enabled = true;

Trigger trigger;
trigger.type = TriggerType::SensorThreshold;
trigger.deviceId = "temp_sensor_1";
trigger.property = "temperature";
trigger.compareOp = CompareOp::GreaterThan;
trigger.threshold = 80.0;
auto.triggers.push_back(trigger);

Action action;
action.type = ActionType::Notify;
action.message = "Temperature exceeds 80°F!";
auto.actions.push_back(action);
```

### Example 4: Conditional Automation

Turn on light when motion detected, but only if it's after 6 PM:

```cpp
Automation auto;
auto.id = "conditional_motion";
auto.name = "Evening Motion Light";
auto.enabled = true;

// Trigger: motion detected
Trigger trigger;
trigger.type = TriggerType::DeviceState;
trigger.deviceId = "motion_sensor_1";
trigger.property = "motion";
trigger.toValue = true;
auto.triggers.push_back(trigger);

// Condition: after 6 PM
Condition cond;
cond.isGroup = false;
cond.leaf.deviceId = "system";
cond.leaf.property = "hour";
cond.leaf.op = CompareOp::GreaterOrEqual;
cond.leaf.value = 18;
auto.condition = cond;

// Action: turn on light
Action action;
action.type = ActionType::SetDeviceState;
action.deviceId = "living_room_light";
action.property = "on";
action.value = true;
auto.actions.push_back(action);
```

## Testing

Unit tests are in `app/tests/automation/`:
- `test_automation.cpp` - Data structures, CRUD, triggers
- `test_automation_screens.cpp` - UI screen registration and navigation

Run tests:
```bash
cd app/build-test
ninja test_automation
./test_automation
```

## Future Enhancements

- Scene support (group multiple device actions)
- Sunrise/sunset triggers
- Notification integration (push, email)
- Automation templates
- Import/export automations
- Voice assistant integration
