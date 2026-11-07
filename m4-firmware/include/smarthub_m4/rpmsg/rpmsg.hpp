/**
 * SmartHub M4 RPMsg Interface
 *
 * Communication between M4 and A7 cores via shared memory.
 */
#pragma once

#include <smarthub_protocol/rpmsg_protocol.hpp>

#include <cstdint>
#include <cstddef>
#include <functional>

namespace smarthub::m4 {

/* The messages are defined once, for both sides, in shared/ */
using protocol::GPIOPayload;
using protocol::IntervalPayload;
using protocol::MsgHeader;
using protocol::MsgType;
using protocol::SensorDataPayload;
using protocol::SensorType;
using protocol::StatusPayload;

/**
 * RPMsg endpoint callback type
 */
using RpmsgCallback = std::function<void(const MsgHeader*, const void*, size_t)>;

/**
 * RPMsg Communication Interface
 *
 * Handles bidirectional communication with A7 core via shared memory
 * and IPCC (Inter-Processor Communication Controller).
 */
class Rpmsg {
public:
    static constexpr size_t MAX_MSG_SIZE = protocol::MAX_MESSAGE_SIZE;
    static constexpr const char* CHANNEL_NAME = "smarthub-m4";

    /**
     * Get singleton instance
     */
    static Rpmsg& instance();

    /**
     * Initialize RPMsg interface
     * @return true if successful
     */
    bool init();

    /**
     * Shutdown RPMsg interface
     */
    void shutdown();

    /**
     * Check if RPMsg is ready
     */
    bool isReady() const { return m_ready; }

    /**
     * Process incoming messages (call from main loop)
     */
    void poll();

    /**
     * Send message to A7
     * @param type Message type
     * @param payload Payload data (can be null)
     * @param len Payload length
     * @return true if sent successfully
     */
    bool send(MsgType type, const void* payload = nullptr, size_t len = 0);

    /**
     * Send sensor data event
     */
    bool sendSensorData(uint8_t sensorId, SensorType type,
                        int32_t value, int32_t scale = 100);

    /**
     * Send status response
     */
    bool sendStatus(uint32_t uptime, uint8_t sensorCount,
                    uint16_t pollInterval);

    /**
     * Send pong response
     */
    bool sendPong();

    /**
     * Set message handler callback
     */
    void setCallback(RpmsgCallback cb) { m_callback = cb; }

    /**
     * Get last error message
     */
    const char* lastError() const { return m_lastError; }

private:
    Rpmsg() = default;
    ~Rpmsg() = default;

    void handleIncomingMessage(const void* rhdr, const void* payload, size_t len);
    void notifyHost(uint32_t channel);
    void ringPending();
    bool announceEndpoint();
    bool sendRaw(uint32_t src, uint32_t dst, const void* data, size_t len);

    bool m_ready = false;
    uint16_t m_seqNum = 0;
    uint32_t m_hostAddr = 0;  /* Who to reply to. 0 until something has written to us. */
    bool m_announced = false;
    uint32_t m_pendingNotify = 0;  /* IPCC channels still to ring */
    RpmsgCallback m_callback;
    const char* m_lastError = nullptr;

    /* Shared memory pointers (set during init) */
    volatile uint8_t* m_txBuffer = nullptr;
    volatile uint8_t* m_rxBuffer = nullptr;
    volatile uint32_t* m_txHead = nullptr;
    volatile uint32_t* m_txTail = nullptr;
    volatile uint32_t* m_rxHead = nullptr;
    volatile uint32_t* m_rxTail = nullptr;
};

/**
 * Convenience function to get RPMsg instance
 */
inline Rpmsg& rpmsg() {
    return Rpmsg::instance();
}

} // namespace smarthub::m4
