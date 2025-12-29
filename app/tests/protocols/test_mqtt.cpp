/**
 * MqttClient Unit Tests
 *
 * Tests MQTT client functionality. Live broker tests are skipped
 * when no broker is available.
 */

#include <gtest/gtest.h>
#include <smarthub/protocols/mqtt/MqttClient.hpp>
#include <smarthub/core/EventBus.hpp>
#include <atomic>
#include <thread>
#include <chrono>

class MqttClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        eventBus = std::make_unique<smarthub::EventBus>();
    }

    void TearDown() override {
        eventBus.reset();
    }

    std::unique_ptr<smarthub::EventBus> eventBus;
};

TEST_F(MqttClientTest, Construction) {
    smarthub::MqttClient client(*eventBus, "127.0.0.1", 1883);
    EXPECT_FALSE(client.isConnected());
}

TEST_F(MqttClientTest, SetClientId) {
    smarthub::MqttClient client(*eventBus, "127.0.0.1", 1883);
    client.setClientId("test_client");
    // No crash, setter works
    EXPECT_FALSE(client.isConnected());
}

TEST_F(MqttClientTest, SetCredentials) {
    smarthub::MqttClient client(*eventBus, "127.0.0.1", 1883);
    client.setCredentials("user", "password");
    // No crash, setter works
    EXPECT_FALSE(client.isConnected());
}

TEST_F(MqttClientTest, SetMessageCallback) {
    smarthub::MqttClient client(*eventBus, "127.0.0.1", 1883);

    bool callbackSet = false;
    client.setMessageCallback([&](const std::string& /*topic*/, const std::string& /*payload*/) {
        callbackSet = true;
    });

    // Callback is set but won't be called until we receive a message
    EXPECT_FALSE(client.isConnected());
}

TEST_F(MqttClientTest, ConnectFailsWithNoBroker) {
    smarthub::MqttClient client(*eventBus, "127.0.0.1", 1883);

    // Connection should fail when no broker is running
    // Either fails immediately or eventually, depending on mosquitto availability
    // If mosquitto library is not compiled in, connect() returns false
    // If mosquitto is available but no broker, connect() may fail or succeed then disconnect
    (void)client.connect();
    EXPECT_FALSE(client.isConnected());
}

TEST_F(MqttClientTest, DisconnectWhenNotConnected) {
    smarthub::MqttClient client(*eventBus, "127.0.0.1", 1883);
    // Should not crash
    client.disconnect();
    EXPECT_FALSE(client.isConnected());
}

TEST_F(MqttClientTest, SubscribeFailsWhenNotConnected) {
    smarthub::MqttClient client(*eventBus, "127.0.0.1", 1883);
    EXPECT_FALSE(client.subscribe("test/topic"));
}

TEST_F(MqttClientTest, UnsubscribeFailsWhenNotConnected) {
    smarthub::MqttClient client(*eventBus, "127.0.0.1", 1883);
    EXPECT_FALSE(client.unsubscribe("test/topic"));
}

TEST_F(MqttClientTest, PublishFailsWhenNotConnected) {
    smarthub::MqttClient client(*eventBus, "127.0.0.1", 1883);
    EXPECT_FALSE(client.publish("test/topic", "payload"));
}

TEST_F(MqttClientTest, PollWhenNotConnected) {
    smarthub::MqttClient client(*eventBus, "127.0.0.1", 1883);
    // Should not crash
    client.poll();
    EXPECT_FALSE(client.isConnected());
}

TEST_F(MqttClientTest, QoSLevels) {
    smarthub::MqttClient client(*eventBus, "127.0.0.1", 1883);

    // Test all QoS levels compile and work
    EXPECT_FALSE(client.subscribe("test/topic", smarthub::MqttQoS::AtMostOnce));
    EXPECT_FALSE(client.subscribe("test/topic", smarthub::MqttQoS::AtLeastOnce));
    EXPECT_FALSE(client.subscribe("test/topic", smarthub::MqttQoS::ExactlyOnce));

    EXPECT_FALSE(client.publish("test/topic", "data", smarthub::MqttQoS::AtMostOnce));
    EXPECT_FALSE(client.publish("test/topic", "data", smarthub::MqttQoS::AtLeastOnce, true));
}

TEST_F(MqttClientTest, MultipleDisconnects) {
    smarthub::MqttClient client(*eventBus, "127.0.0.1", 1883);
    client.disconnect();
    client.disconnect();
    client.disconnect();
    // Should not crash
    EXPECT_FALSE(client.isConnected());
}

// Live broker tests - only run if MQTT_BROKER_TEST env var is set
class MqttClientLiveTest : public MqttClientTest {
protected:
    void SetUp() override {
        MqttClientTest::SetUp();

        // Skip live tests unless explicitly enabled
        const char* brokerTest = std::getenv("MQTT_BROKER_TEST");
        if (!brokerTest || std::string(brokerTest) != "1") {
            GTEST_SKIP() << "Set MQTT_BROKER_TEST=1 to run live broker tests";
        }
    }
};

TEST_F(MqttClientLiveTest, ConnectToLocalBroker) {
    smarthub::MqttClient client(*eventBus, "127.0.0.1", 1883);
    ASSERT_TRUE(client.connect());
    EXPECT_TRUE(client.isConnected());

    // Wait for connection to establish
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    client.disconnect();
    EXPECT_FALSE(client.isConnected());
}

TEST_F(MqttClientLiveTest, SubscribeAndPublish) {
    smarthub::MqttClient client(*eventBus, "127.0.0.1", 1883);
    ASSERT_TRUE(client.connect());

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::atomic<bool> messageReceived{false};
    std::string receivedTopic;
    std::string receivedPayload;

    client.setMessageCallback([&](const std::string& topic, const std::string& payload) {
        receivedTopic = topic;
        receivedPayload = payload;
        messageReceived = true;
    });

    EXPECT_TRUE(client.subscribe("smarthub/test"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(client.publish("smarthub/test", "hello world"));

    // Wait for message
    for (int i = 0; i < 20 && !messageReceived; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    EXPECT_TRUE(messageReceived);
    EXPECT_EQ(receivedTopic, "smarthub/test");
    EXPECT_EQ(receivedPayload, "hello world");

    client.disconnect();
}

TEST_F(MqttClientLiveTest, PublishesEventBusMessage) {
    smarthub::MqttClient client(*eventBus, "127.0.0.1", 1883);
    ASSERT_TRUE(client.connect());

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::atomic<int> eventCount{0};
    eventBus->subscribe("mqtt.message", [&](const smarthub::Event& /*e*/) {
        eventCount++;
    });

    EXPECT_TRUE(client.subscribe("smarthub/eventbus_test"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(client.publish("smarthub/eventbus_test", "test payload"));

    // Wait for event
    for (int i = 0; i < 20 && eventCount == 0; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    EXPECT_GE(eventCount.load(), 1);

    client.disconnect();
}
