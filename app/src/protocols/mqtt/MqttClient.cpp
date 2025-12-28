/**
 * SmartHub MQTT Client Implementation
 */

#include <smarthub/protocols/mqtt/MqttClient.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/core/Logger.hpp>

#if __has_include(<mosquitto.h>)
#include <mosquitto.h>
#define HAVE_MOSQUITTO 1
#else
#define HAVE_MOSQUITTO 0
#endif

namespace smarthub {

#if HAVE_MOSQUITTO

static bool s_mosquittoInitialized = false;

MqttClient::MqttClient(EventBus& eventBus, const std::string& broker, int port)
    : m_eventBus(eventBus)
    , m_broker(broker)
    , m_port(port)
{
    if (!s_mosquittoInitialized) {
        mosquitto_lib_init();
        s_mosquittoInitialized = true;
    }
}

MqttClient::~MqttClient() {
    disconnect();
}

bool MqttClient::connect() {
    if (m_mosq) {
        disconnect();
    }

    m_mosq = mosquitto_new(m_clientId.c_str(), true, this);
    if (!m_mosq) {
        LOG_ERROR("Failed to create mosquitto client");
        return false;
    }

    // Set callbacks
    mosquitto_connect_callback_set(m_mosq, onConnect);
    mosquitto_disconnect_callback_set(m_mosq, onDisconnect);
    mosquitto_message_callback_set(m_mosq, onMessage);

    // Set credentials if provided
    if (!m_username.empty()) {
        mosquitto_username_pw_set(m_mosq, m_username.c_str(),
                                   m_password.empty() ? nullptr : m_password.c_str());
    }

    // Connect
    int rc = mosquitto_connect(m_mosq, m_broker.c_str(), m_port, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_ERROR("Failed to connect to MQTT broker: %s", mosquitto_strerror(rc));
        mosquitto_destroy(m_mosq);
        m_mosq = nullptr;
        return false;
    }

    // Start network loop
    rc = mosquitto_loop_start(m_mosq);
    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_ERROR("Failed to start MQTT loop: %s", mosquitto_strerror(rc));
        mosquitto_destroy(m_mosq);
        m_mosq = nullptr;
        return false;
    }

    return true;
}

void MqttClient::disconnect() {
    if (m_mosq) {
        mosquitto_loop_stop(m_mosq, true);
        mosquitto_disconnect(m_mosq);
        mosquitto_destroy(m_mosq);
        m_mosq = nullptr;
    }
    m_connected = false;
}

void MqttClient::poll() {
    // Using threaded loop, so nothing needed here
}

bool MqttClient::subscribe(const std::string& topic, MqttQoS qos) {
    if (!m_mosq || !m_connected) {
        return false;
    }

    int rc = mosquitto_subscribe(m_mosq, nullptr, topic.c_str(), static_cast<int>(qos));
    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_ERROR("Failed to subscribe to %s: %s", topic.c_str(), mosquitto_strerror(rc));
        return false;
    }

    m_subscriptions.push_back(topic);
    LOG_DEBUG("Subscribed to %s", topic.c_str());
    return true;
}

bool MqttClient::unsubscribe(const std::string& topic) {
    if (!m_mosq || !m_connected) {
        return false;
    }

    int rc = mosquitto_unsubscribe(m_mosq, nullptr, topic.c_str());
    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_ERROR("Failed to unsubscribe from %s: %s", topic.c_str(), mosquitto_strerror(rc));
        return false;
    }

    m_subscriptions.erase(
        std::remove(m_subscriptions.begin(), m_subscriptions.end(), topic),
        m_subscriptions.end()
    );
    return true;
}

bool MqttClient::publish(const std::string& topic, const std::string& payload,
                         MqttQoS qos, bool retain) {
    if (!m_mosq || !m_connected) {
        return false;
    }

    int rc = mosquitto_publish(m_mosq, nullptr, topic.c_str(),
                               static_cast<int>(payload.size()),
                               payload.c_str(), static_cast<int>(qos), retain);
    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_ERROR("Failed to publish to %s: %s", topic.c_str(), mosquitto_strerror(rc));
        return false;
    }

    return true;
}

void MqttClient::setCredentials(const std::string& username, const std::string& password) {
    m_username = username;
    m_password = password;
}

void MqttClient::onConnect(struct mosquitto* /*mosq*/, void* obj, int rc) {
    auto* client = static_cast<MqttClient*>(obj);
    client->handleConnect(rc);
}

void MqttClient::onDisconnect(struct mosquitto* /*mosq*/, void* obj, int rc) {
    auto* client = static_cast<MqttClient*>(obj);
    client->handleDisconnect(rc);
}

void MqttClient::onMessage(struct mosquitto* /*mosq*/, void* obj,
                           const ::mosquitto_message* msg) {
    auto* client = static_cast<MqttClient*>(obj);
    std::string topic(msg->topic);
    std::string payload(static_cast<char*>(msg->payload), msg->payloadlen);
    client->handleMessage(topic, payload);
}

void MqttClient::handleConnect(int rc) {
    if (rc == 0) {
        m_connected = true;
        LOG_INFO("Connected to MQTT broker %s:%d", m_broker.c_str(), m_port);

        // Resubscribe to topics
        for (const auto& topic : m_subscriptions) {
            mosquitto_subscribe(m_mosq, nullptr, topic.c_str(), 1);
        }
    } else {
        LOG_ERROR("MQTT connection failed: %s", mosquitto_strerror(rc));
    }
}

void MqttClient::handleDisconnect(int rc) {
    m_connected = false;
    if (rc != 0) {
        LOG_WARN("Disconnected from MQTT broker: %s", mosquitto_strerror(rc));
    }
}

void MqttClient::handleMessage(const std::string& topic, const std::string& payload) {
    LOG_DEBUG("MQTT message: %s = %s", topic.c_str(), payload.c_str());

    // Publish to event bus
    MqttMessageEvent event;
    event.topic = topic;
    event.payload = payload;
    event.retained = false;
    m_eventBus.publish(event);

    // Call user callback if set
    if (m_messageCallback) {
        m_messageCallback(topic, payload);
    }
}

#else // !HAVE_MOSQUITTO

// Stub implementation when mosquitto is not available
MqttClient::MqttClient(EventBus& eventBus, const std::string& broker, int port)
    : m_eventBus(eventBus)
    , m_broker(broker)
    , m_port(port)
{
    LOG_WARN("MQTT support not compiled in");
}

MqttClient::~MqttClient() {}
bool MqttClient::connect() { return false; }
void MqttClient::disconnect() {}
void MqttClient::poll() {}
bool MqttClient::subscribe(const std::string&, MqttQoS) { return false; }
bool MqttClient::unsubscribe(const std::string&) { return false; }
bool MqttClient::publish(const std::string&, const std::string&, MqttQoS, bool) { return false; }
void MqttClient::setCredentials(const std::string&, const std::string&) {}

#endif // HAVE_MOSQUITTO

} // namespace smarthub
