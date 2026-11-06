/**
 * SmartHub EventBus Implementation
 */

#include <smarthub/core/EventBus.hpp>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace smarthub {

struct EventBus::Handler {
    explicit Handler(EventHandler function) : call(std::move(function)) {}

    const EventHandler call;
    std::mutex mutex;
    std::condition_variable finished;
    int running = 0;
    bool active = true;
};

namespace {

// The handlers this thread is inside, so unsubscribing from one doesn't wait for itself
thread_local std::vector<const void*> t_runningHandlers;

}  // namespace

Event::Event() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    m_timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

SubscriptionId EventBus::subscribe(const std::string& eventType, EventHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);

    SubscriptionId id = m_nextId++;
    m_subscribers[eventType].push_back({id, std::make_shared<Handler>(std::move(handler))});

    return id;
}

SubscriptionId EventBus::subscribeAll(EventHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);

    SubscriptionId id = m_nextId++;
    m_globalSubscribers.push_back({id, std::make_shared<Handler>(std::move(handler))});

    return id;
}

void EventBus::unsubscribe(SubscriptionId id) {
    std::vector<std::shared_ptr<Handler>> removed;

    const auto removeFrom = [&](std::vector<Subscription>& subscriptions) {
        const auto first = std::remove_if(subscriptions.begin(), subscriptions.end(),
                                          [id](const Subscription& s) { return s.id == id; });
        for (auto it = first; it != subscriptions.end(); ++it) {
            removed.push_back(it->handler);
        }
        subscriptions.erase(first, subscriptions.end());
    };

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& [type, subs] : m_subscribers) {
            removeFrom(subs);
        }
        removeFrom(m_globalSubscribers);
    }

    for (const auto& handler : removed) {
        const auto onThisThread = std::count(t_runningHandlers.begin(), t_runningHandlers.end(),
                                             static_cast<const void*>(handler.get()));
        std::unique_lock<std::mutex> lock(handler->mutex);
        handler->active = false;
        handler->finished.wait(lock, [&] { return handler->running <= onThisThread; });
    }
}

void EventBus::publish(const Event& event) {
    std::vector<std::shared_ptr<Handler>> handlers;

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
        {
            std::lock_guard<std::mutex> lock(handler->mutex);
            if (!handler->active) {
                continue;  // Unsubscribed since the list was collected
            }
            ++handler->running;
        }

        // Also runs if the handler throws
        struct Finish {
            Handler& handler;
            ~Finish() {
                t_runningHandlers.pop_back();
                std::lock_guard<std::mutex> lock(handler.mutex);
                --handler.running;
                handler.finished.notify_all();
            }
        };
        t_runningHandlers.push_back(handler.get());
        Finish finish{*handler};
        handler->call(event);
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
