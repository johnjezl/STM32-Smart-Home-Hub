/**
 * SmartHub RPMsg Client
 *
 * Communication with Cortex-M4 via RPMsg/OpenAMP.
 */
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace smarthub {

class EventBus;

/**
 * RPMsg message types (shared with M4 firmware)
 */
enum class RpmsgMessageType : uint8_t {
    Ping = 0x00,
    Pong = 0x01,
    SensorData = 0x10,
    GpioCommand = 0x20,
    GpioState = 0x21,
    AdcRequest = 0x30,
    AdcResponse = 0x31,
    PwmCommand = 0x40,
    Config = 0x50,
    Error = 0xFF
};

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
     * Initialize RPMsg communication
     */
    bool initialize();

    /**
     * Shutdown and cleanup
     */
    void shutdown();

    /**
     * Check if connected to M4
     */
    bool isConnected() const { return m_connected; }

    /**
     * Poll for messages (call from main loop)
     */
    void poll();

    /**
     * Send raw message to M4
     */
    bool send(const std::vector<uint8_t>& data);

    /**
     * Send typed message
     */
    bool sendMessage(RpmsgMessageType type, const std::vector<uint8_t>& payload);

    /**
     * Request sensor data from M4
     */
    bool requestSensorData(uint8_t sensorId);

    /**
     * Set GPIO pin state
     */
    bool setGpio(uint8_t pin, bool state);

    /**
     * Set PWM duty cycle
     */
    bool setPwm(uint8_t channel, uint16_t dutyCycle);

    /**
     * Send ping to verify M4 is responsive
     */
    bool ping();

    /**
     * Set message callback
     */
    void setMessageCallback(MessageCallback callback) { m_callback = callback; }

private:
    void handleMessage(const std::vector<uint8_t>& data);

    EventBus& m_eventBus;
    std::string m_device;
    int m_fd = -1;
    bool m_connected = false;
    MessageCallback m_callback;
};

} // namespace smarthub
