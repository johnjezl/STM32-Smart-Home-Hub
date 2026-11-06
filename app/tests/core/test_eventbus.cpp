/**
 * EventBus Unit Tests
 */

#include <gtest/gtest.h>
#include <smarthub/core/EventBus.hpp>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

class EventBusTest : public ::testing::Test {
protected:
    smarthub::EventBus eventBus;
};

// Test event class
class TestEvent : public smarthub::Event {
public:
    TestEvent(int v) : value(v) {}
    std::string type() const override { return "TestEvent"; }
    int value;
};

class AnotherEvent : public smarthub::Event {
public:
    AnotherEvent(const std::string& m) : message(m) {}
    std::string type() const override { return "AnotherEvent"; }
    std::string message;
};

TEST_F(EventBusTest, Subscribe) {
    bool called = false;
    auto id = eventBus.subscribe("TestEvent", [&](const smarthub::Event& /*e*/) {
        called = true;
    });
    EXPECT_GT(id, 0u);
    EXPECT_FALSE(called);  // Not called until published
}

TEST_F(EventBusTest, PublishTriggersHandler) {
    int receivedValue = 0;
    eventBus.subscribe("TestEvent", [&](const smarthub::Event& e) {
        auto& testEvent = static_cast<const TestEvent&>(e);
        receivedValue = testEvent.value;
    });

    TestEvent event(42);
    eventBus.publish(event);

    EXPECT_EQ(receivedValue, 42);
}

TEST_F(EventBusTest, MultipleSubscribers) {
    int count = 0;
    eventBus.subscribe("TestEvent", [&](const smarthub::Event& /*e*/) { count++; });
    eventBus.subscribe("TestEvent", [&](const smarthub::Event& /*e*/) { count++; });
    eventBus.subscribe("TestEvent", [&](const smarthub::Event& /*e*/) { count++; });

    TestEvent event(1);
    eventBus.publish(event);

    EXPECT_EQ(count, 3);
}

TEST_F(EventBusTest, DifferentEventTypes) {
    int testEventCount = 0;
    int anotherEventCount = 0;

    eventBus.subscribe("TestEvent", [&](const smarthub::Event& /*e*/) {
        testEventCount++;
    });
    eventBus.subscribe("AnotherEvent", [&](const smarthub::Event& /*e*/) {
        anotherEventCount++;
    });

    TestEvent testEvent(1);
    AnotherEvent anotherEvent("hello");

    eventBus.publish(testEvent);
    eventBus.publish(anotherEvent);
    eventBus.publish(testEvent);

    EXPECT_EQ(testEventCount, 2);
    EXPECT_EQ(anotherEventCount, 1);
}

TEST_F(EventBusTest, Unsubscribe) {
    int count = 0;
    auto id = eventBus.subscribe("TestEvent", [&](const smarthub::Event& /*e*/) {
        count++;
    });

    TestEvent event(1);
    eventBus.publish(event);
    EXPECT_EQ(count, 1);

    eventBus.unsubscribe(id);
    eventBus.publish(event);
    EXPECT_EQ(count, 1);  // Should not increase after unsubscribe
}

TEST_F(EventBusTest, AsyncPublish) {
    std::atomic<int> count{0};
    eventBus.subscribe("TestEvent", [&](const smarthub::Event& /*e*/) {
        count++;
    });

    auto event = std::make_unique<TestEvent>(42);
    eventBus.publishAsync(std::move(event));

    // Process the queue
    eventBus.processQueue();

    EXPECT_EQ(count.load(), 1);
}

TEST_F(EventBusTest, EventData) {
    std::string receivedMessage;
    eventBus.subscribe("AnotherEvent", [&](const smarthub::Event& e) {
        auto& anotherEvent = static_cast<const AnotherEvent&>(e);
        receivedMessage = anotherEvent.message;
    });

    AnotherEvent event("Hello, World!");
    eventBus.publish(event);

    EXPECT_EQ(receivedMessage, "Hello, World!");
}

TEST_F(EventBusTest, NoSubscribersDoesNotCrash) {
    TestEvent event(42);
    EXPECT_NO_THROW(eventBus.publish(event));
}

TEST_F(EventBusTest, SubscribeAfterPublish) {
    TestEvent event1(1);
    eventBus.publish(event1);  // Published before subscription

    int count = 0;
    eventBus.subscribe("TestEvent", [&](const smarthub::Event& /*e*/) {
        count++;
    });

    // First event should not be received
    EXPECT_EQ(count, 0);

    TestEvent event2(2);
    eventBus.publish(event2);  // Published after subscription
    EXPECT_EQ(count, 1);
}

TEST_F(EventBusTest, UnsubscribeWaitsForAHandlerRunningOnAnotherThread) {
    std::atomic<bool> started{false};
    std::atomic<bool> finished{false};
    const auto id = eventBus.subscribe("TestEvent", [&](const smarthub::Event&) {
        started = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        finished = true;
    });

    std::thread publisher([&] { eventBus.publish(TestEvent(1)); });
    while (!started) {
        std::this_thread::yield();
    }

    eventBus.unsubscribe(id);
    EXPECT_TRUE(finished);
    publisher.join();
}

TEST_F(EventBusTest, HandlerUnsubscribedByAnEarlierHandlerDoesNotRun) {
    smarthub::SubscriptionId second = 0;
    bool secondRan = false;
    eventBus.subscribe("TestEvent", [&](const smarthub::Event&) { eventBus.unsubscribe(second); });
    second = eventBus.subscribe("TestEvent", [&](const smarthub::Event&) { secondRan = true; });

    eventBus.publish(TestEvent(1));
    EXPECT_FALSE(secondRan);
}

TEST_F(EventBusTest, HandlerCanUnsubscribeItself) {
    smarthub::SubscriptionId id = 0;
    int calls = 0;
    id = eventBus.subscribe("TestEvent", [&](const smarthub::Event&) {
        ++calls;
        eventBus.unsubscribe(id);
    });

    eventBus.publish(TestEvent(1));
    eventBus.publish(TestEvent(1));
    EXPECT_EQ(calls, 1);
}

TEST_F(EventBusTest, ThrowingHandlerStillLetsUnsubscribeFinish) {
    const auto id = eventBus.subscribe("TestEvent", [](const smarthub::Event&) {
        throw std::runtime_error("handler failed");
    });

    EXPECT_THROW(eventBus.publish(TestEvent(1)), std::runtime_error);
    eventBus.unsubscribe(id);
    EXPECT_EQ(eventBus.subscriberCount("TestEvent"), 0u);
}

TEST_F(EventBusTest, UnsubscribingAnEarlierHandlerKeepsTheLaterOnes) {
    int firstCalls = 0;
    int secondCalls = 0;
    int globalCalls = 0;
    const auto first = eventBus.subscribe("TestEvent", [&](const smarthub::Event&) { ++firstCalls; });
    eventBus.subscribe("TestEvent", [&](const smarthub::Event&) { ++secondCalls; });
    const auto global = eventBus.subscribeAll([&](const smarthub::Event&) { ++globalCalls; });
    eventBus.subscribeAll([](const smarthub::Event&) {});

    eventBus.unsubscribe(first);
    eventBus.unsubscribe(global);
    eventBus.publish(TestEvent(1));

    EXPECT_EQ(firstCalls, 0);
    EXPECT_EQ(secondCalls, 1);
    EXPECT_EQ(globalCalls, 0);
}

TEST_F(EventBusTest, UnsubscribingAnEarlierHandlerWaitsForIt) {
    std::atomic<bool> started{false};
    std::atomic<bool> finished{false};
    const auto first = eventBus.subscribe("TestEvent", [&](const smarthub::Event&) {
        started = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        finished = true;
    });
    eventBus.subscribe("TestEvent", [](const smarthub::Event&) {});

    std::thread publisher([&] { eventBus.publish(TestEvent(1)); });
    while (!started) {
        std::this_thread::yield();
    }

    eventBus.unsubscribe(first);
    EXPECT_TRUE(finished);
    publisher.join();
}
