/**
 * SmartHub MQTT Client
 *
 * MQTT client for communication with external devices and services.
 */
#pragma once

#include <string>
#include <functional>
#include <vector>
#include <mutex>

// Forward declarations - these are from mosquitto.h
extern "C" {
    struct mosquitto;
    struct mosquitto_message;
}

namespace smarthub {

class EventBus;

/**
 * MQTT Quality of Service levels
 */
enum class MqttQoS {
    AtMostOnce = 0,
    AtLeastOnce = 1,
    ExactlyOnce = 2
};

/**
 * MQTT client wrapper
 */
class MqttClient {
public:
    using MessageCallback = std::function<void(const std::string& topic, const std::string& payload)>;

    MqttClient(EventBus& eventBus, const std::string& broker, int port);
    ~MqttClient();

    // Non-copyable
    MqttClient(const MqttClient&) = delete;
    MqttClient& operator=(const MqttClient&) = delete;

    /**
     * Connect to MQTT broker
     */
    bool connect();

    /**
     * Disconnect from broker
     */
    void disconnect();

    /**
     * Check if connected
     */
    bool isConnected() const { return m_connected; }

    /**
     * Poll for messages (call from main loop)
     */
    void poll();

    /**
     * Subscribe to a topic
     */
    bool subscribe(const std::string& topic, MqttQoS qos = MqttQoS::AtLeastOnce);

    /**
     * Unsubscribe from a topic
     */
    bool unsubscribe(const std::string& topic);

    /**
     * Publish a message
     */
    bool publish(const std::string& topic, const std::string& payload,
                 MqttQoS qos = MqttQoS::AtLeastOnce, bool retain = false);

    /**
     * Set client ID
     */
    void setClientId(const std::string& clientId) { m_clientId = clientId; }

    /**
     * Set credentials
     */
    void setCredentials(const std::string& username, const std::string& password);

    /**
     * Set message callback (in addition to EventBus)
     */
    void setMessageCallback(MessageCallback callback) { m_messageCallback = callback; }

private:
    static void onConnect(struct mosquitto* mosq, void* obj, int rc);
    static void onDisconnect(struct mosquitto* mosq, void* obj, int rc);
    static void onMessage(struct mosquitto* mosq, void* obj, const ::mosquitto_message* msg);

    void handleConnect(int rc);
    void handleDisconnect(int rc);
    void handleMessage(const std::string& topic, const std::string& payload);

    EventBus& m_eventBus;
    std::string m_broker;
    int m_port;
    std::string m_clientId = "smarthub";
    std::string m_username;
    std::string m_password;

    struct mosquitto* m_mosq = nullptr;
    bool m_connected = false;
    std::vector<std::string> m_subscriptions;
    MessageCallback m_messageCallback;
    std::mutex m_mutex;
};

} // namespace smarthub
