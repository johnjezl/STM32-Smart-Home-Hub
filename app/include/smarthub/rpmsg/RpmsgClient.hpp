/**
 * SmartHub RPMsg Client
 *
 * Communication with Cortex-M4 via RPMsg/OpenAMP.
 */
#pragma once

#include <smarthub_protocol/rpmsg_protocol.hpp>

#include <chrono>
#include <string>
#include <utility>
#include <vector>
#include <functional>
#include <cstdint>

namespace smarthub {

class EventBus;

/**
 * RPMsg client for M4 communication
 */
class RpmsgClient {
public:
    using MessageCallback = std::function<void(const std::vector<uint8_t>&)>;

    RpmsgClient(EventBus& eventBus, const std::string& device = "/dev/ttyRPMSG0");
    ~RpmsgClient();

    // Non-copyable
    RpmsgClient(const RpmsgClient&) = delete;
    RpmsgClient& operator=(const RpmsgClient&) = delete;

    /**
     * Open the device and ping the M4. False if the device can't be opened, or the M4 doesn't
     * answer within pingTimeoutMs. Either way poll() keeps trying, in case it starts later.
     */
    bool initialize(int pingTimeoutMs = 250);

    /**
     * Shutdown and cleanup
     */
    void shutdown();

    /**
     * Check if the device is open. See m4Responded for whether anything is answering. It closes
     * if the M4 stops, as Linux takes the device away, and poll() opens it again when it's back.
     */
    bool isConnected() const { return m_connected; }

    /**
     * Whether the M4 has answered a ping since the device was last opened. It doesn't go false
     * if the M4 hangs without Linux noticing.
     */
    bool m4Responded() const { return m_m4Responded; }

    /**
     * Poll for messages, and for the device if it isn't open (call from main loop)
     */
    void poll();

    /**
     * Send raw message to M4
     */
    bool send(const std::vector<uint8_t>& data);

    /**
     * Send typed message
     */
    bool sendMessage(protocol::MsgType type, const std::vector<uint8_t>& payload);

    /**
     * Ask the M4 to read its sensors now. The readings arrive as SensorDataEvents.
     */
    bool requestSensorData();

    /**
     * Set how often the M4 reads its sensors
     */
    bool setSensorInterval(uint16_t intervalMs);

    /**
     * Set GPIO pin state
     * @param port 0=A, 1=B, and so on
     * @param pin 0-15
     */
    bool setGpio(uint8_t port, uint8_t pin, bool state);

    /**
     * Send ping to verify M4 is responsive
     */
    bool ping();

    /**
     * Set message callback
     */
    void setMessageCallback(MessageCallback callback) { m_callback = std::move(callback); }

    /**
     * How long poll() waits between pings or tries at opening the device
     */
    void setRetryInterval(std::chrono::milliseconds interval) { m_retryInterval = interval; }

private:
    bool openDevice();
    void closeDevice();
    void handleReceived(const uint8_t* data, size_t size);
    void handleMessage(const protocol::DecodedMessage& message);

    EventBus& m_eventBus;
    std::string m_device;
    int m_fd = -1;
    bool m_connected = false;
    bool m_m4Responded = false;
    bool m_wanted = false;  // Between initialize and shutdown, when poll should get the device back
    std::chrono::milliseconds m_retryInterval{5000};
    uint16_t m_nextSeq = 0;
    std::vector<uint8_t> m_received;  // What has arrived of a message not complete yet
    std::chrono::steady_clock::time_point m_lastReceived{};
    std::chrono::steady_clock::time_point m_lastPing{};
    MessageCallback m_callback;
};

} // namespace smarthub
