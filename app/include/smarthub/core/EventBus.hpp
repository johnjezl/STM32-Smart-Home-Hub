/**
 * SmartHub Event Bus
 *
 * Publish-subscribe event system for decoupled communication
 * between application components.
 */
#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <any>
#include <memory>
#include <cstdint>

namespace smarthub {

/**
 * Base class for all events
 */
class Event {
public:
    virtual ~Event() = default;
    virtual std::string type() const = 0;

    uint64_t timestamp() const { return m_timestamp; }

protected:
    Event();
    uint64_t m_timestamp;
};

/**
 * Device state changed event
 */
class DeviceStateEvent : public Event {
public:
    std::string deviceId;
    std::string property;
    std::any value;
    std::any previousValue;

    std::string type() const override { return "device.state"; }
};

/**
 * Sensor data received event (from M4 or external)
 */
class SensorDataEvent : public Event {
public:
    std::string sensorId;
    std::string sensorType;
    double value;
    std::string unit;

    std::string type() const override { return "sensor.data"; }
};

/**
 * MQTT message received event
 */
class MqttMessageEvent : public Event {
public:
    std::string topic;
    std::string payload;
    bool retained;

    std::string type() const override { return "mqtt.message"; }
};

/**
 * RPMsg message received event (from M4)
 */
class RpmsgMessageEvent : public Event {
public:
    std::vector<uint8_t> data;

    std::string type() const override { return "rpmsg.message"; }
};

/**
 * System status event
 */
class SystemEvent : public Event {
public:
    enum class Status {
        Starting,
        Ready,
        ShuttingDown,
        Error
    };

    Status status;
    std::string message;

    std::string type() const override { return "system.status"; }
};

using EventHandler = std::function<void(const Event&)>;
using SubscriptionId = uint64_t;

/**
 * Event bus for publish-subscribe messaging
 */
class EventBus {
public:
    EventBus() = default;
    ~EventBus() = default;

    // Non-copyable
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    /**
     * Subscribe to events of a specific type
     * @param eventType Event type string (e.g., "device.state")
     * @param handler Callback function
     * @return Subscription ID for unsubscribing
     */
    SubscriptionId subscribe(const std::string& eventType, EventHandler handler);

    /**
     * Subscribe to all events
     * @param handler Callback function
     * @return Subscription ID
     */
    SubscriptionId subscribeAll(EventHandler handler);

    /**
     * Unsubscribe from events
     * @param id Subscription ID returned from subscribe()
     */
    void unsubscribe(SubscriptionId id);

    /**
     * Publish an event synchronously
     * @param event Event to publish
     */
    void publish(const Event& event);

    /**
     * Queue an event for async processing
     * @param event Event to queue (ownership transferred)
     */
    void publishAsync(std::unique_ptr<Event> event);

    /**
     * Process queued async events (call from main loop)
     */
    void processQueue();

    /**
     * Get number of subscribers for an event type
     */
    size_t subscriberCount(const std::string& eventType) const;

private:
    struct Subscription {
        SubscriptionId id;
        EventHandler handler;
    };

    std::unordered_map<std::string, std::vector<Subscription>> m_subscribers;
    std::vector<Subscription> m_globalSubscribers;
    std::vector<std::unique_ptr<Event>> m_eventQueue;
    mutable std::mutex m_mutex;
    std::mutex m_queueMutex;
    SubscriptionId m_nextId = 1;
};

} // namespace smarthub
