/**
 * SmartHub M4 RPMsg Interface
 *
 * Communication between M4 and A7 cores via shared memory.
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>

namespace smarthub::m4 {

/**
 * Message types for M4 <-> A7 communication
 */
enum class MsgType : uint8_t {
    /* A7 -> M4 Commands */
    CMD_PING            = 0x01,
    CMD_GET_SENSOR_DATA = 0x10,
    CMD_SET_INTERVAL    = 0x11,
    CMD_GET_STATUS      = 0x20,
    CMD_SET_GPIO        = 0x30,
    CMD_GET_GPIO        = 0x31,

    /* M4 -> A7 Responses */
    RSP_PONG            = 0x81,
    RSP_SENSOR_DATA     = 0x90,
    RSP_STATUS          = 0xA0,
    RSP_GPIO_STATE      = 0xB1,

    /* M4 -> A7 Events (unsolicited) */
    EVT_SENSOR_UPDATE   = 0xC0,
    EVT_GPIO_CHANGE     = 0xC1,
    EVT_ERROR           = 0xE0,
};

/**
 * Sensor types
 */
enum class SensorType : uint8_t {
    Unknown         = 0,
    Temperature     = 1,
    Humidity        = 2,
    Pressure        = 3,
    Light           = 4,
    Motion          = 5,
    Proximity       = 6,
    CO2             = 7,
    VOC             = 8,
};

/**
 * Message header - common to all messages
 */
struct MsgHeader {
    uint8_t type;       /* MsgType */
    uint8_t flags;      /* Message flags */
    uint16_t seq;       /* Sequence number */
    uint16_t len;       /* Payload length */
    uint16_t reserved;
} __attribute__((packed));

/**
 * Sensor data message payload
 */
struct SensorDataPayload {
    uint8_t sensorId;       /* Sensor ID (0-255) */
    uint8_t sensorType;     /* SensorType */
    uint16_t reserved;
    int32_t value;          /* Fixed-point value (multiply by scale) */
    int32_t scale;          /* Scale factor (e.g., 100 for 2 decimal places) */
    uint32_t timestamp;     /* Timestamp in ms */
} __attribute__((packed));

/**
 * Status response payload
 */
struct StatusPayload {
    uint32_t uptime;        /* Uptime in ms */
    uint8_t sensorCount;    /* Number of active sensors */
    uint8_t errorCount;     /* Error counter */
    uint16_t pollInterval;  /* Current polling interval in ms */
    uint32_t freeMemory;    /* Free memory bytes */
} __attribute__((packed));

/**
 * GPIO command payload
 */
struct GPIOPayload {
    uint8_t port;           /* GPIO port (0=A, 1=B, etc.) */
    uint8_t pin;            /* Pin number (0-15) */
    uint8_t state;          /* Pin state (0=low, 1=high, 2=toggle) */
    uint8_t mode;           /* Pin mode (for SET_GPIO) */
} __attribute__((packed));

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
    static constexpr size_t MAX_MSG_SIZE = 512;
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
    void notifyHost();
    bool announceEndpoint();
    bool sendRaw(uint32_t src, uint32_t dst, const void* data, size_t len);

    bool m_ready = false;
    uint16_t m_seqNum = 0;
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
