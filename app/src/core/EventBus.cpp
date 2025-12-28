/**
 * SmartHub EventBus Implementation
 */

#include <smarthub/core/EventBus.hpp>
#include <chrono>

namespace smarthub {

Event::Event() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    m_timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

SubscriptionId EventBus::subscribe(const std::string& eventType, EventHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);

    SubscriptionId id = m_nextId++;
    m_subscribers[eventType].push_back({id, std::move(handler)});

    return id;
}

SubscriptionId EventBus::subscribeAll(EventHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);

    SubscriptionId id = m_nextId++;
    m_globalSubscribers.push_back({id, std::move(handler)});

    return id;
}

void EventBus::unsubscribe(SubscriptionId id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Remove from type-specific subscribers
    for (auto& [type, subs] : m_subscribers) {
        subs.erase(
            std::remove_if(subs.begin(), subs.end(),
                [id](const Subscription& s) { return s.id == id; }),
            subs.end()
        );
    }

    // Remove from global subscribers
    m_globalSubscribers.erase(
        std::remove_if(m_globalSubscribers.begin(), m_globalSubscribers.end(),
            [id](const Subscription& s) { return s.id == id; }),
        m_globalSubscribers.end()
    );
}

void EventBus::publish(const Event& event) {
    std::vector<EventHandler> handlers;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Collect handlers for this event type
        auto it = m_subscribers.find(event.type());
        if (it != m_subscribers.end()) {
            for (const auto& sub : it->second) {
                handlers.push_back(sub.handler);
            }
        }

        // Collect global handlers
        for (const auto& sub : m_globalSubscribers) {
            handlers.push_back(sub.handler);
        }
    }

    // Call handlers outside the lock to prevent deadlocks
    for (const auto& handler : handlers) {
        handler(event);
    }
}

void EventBus::publishAsync(std::unique_ptr<Event> event) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_eventQueue.push_back(std::move(event));
}

void EventBus::processQueue() {
    std::vector<std::unique_ptr<Event>> events;

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        events.swap(m_eventQueue);
    }

    for (const auto& event : events) {
        publish(*event);
    }
}

size_t EventBus::subscriberCount(const std::string& eventType) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_subscribers.find(eventType);
    if (it != m_subscribers.end()) {
        return it->second.size();
    }
    return 0;
}

} // namespace smarthub
