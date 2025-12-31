/**
 * SmartHub M4 Sensor Manager
 *
 * Manages all connected sensors and periodic data acquisition.
 */
#pragma once

#include <cstdint>
#include <functional>
#include "smarthub_m4/sensors/sht31.hpp"
#include "smarthub_m4/rpmsg/rpmsg.hpp"

namespace smarthub::m4 {

/**
 * Sensor reading structure
 */
struct SensorReading {
    uint8_t id;             /* Sensor ID */
    SensorType type;        /* Sensor type */
    int32_t value;          /* Fixed-point value */
    int32_t scale;          /* Scale factor */
    uint32_t timestamp;     /* Timestamp in ms */
};

/**
 * Callback for sensor readings
 */
using SensorCallback = std::function<void(const SensorReading&)>;

/**
 * Sensor Manager
 *
 * Handles sensor initialization, polling, and data reporting.
 */
class SensorManager {
public:
    static constexpr uint32_t DEFAULT_POLL_INTERVAL = 1000;  /* 1 second */
    static constexpr uint8_t MAX_SENSORS = 8;

    /**
     * Create sensor manager
     * @param i2c I2C bus for sensor communication
     */
    explicit SensorManager(I2C& i2c);

    /**
     * Initialize all sensors
     * @return Number of sensors found
     */
    uint8_t init();

    /**
     * Poll sensors and send data (call from main loop)
     */
    void poll();

    /**
     * Set polling interval
     * @param ms Interval in milliseconds
     */
    void setPollingInterval(uint32_t ms) { m_pollInterval = ms; }

    /**
     * Get polling interval
     */
    uint32_t getPollingInterval() const { return m_pollInterval; }

    /**
     * Get number of active sensors
     */
    uint8_t getSensorCount() const { return m_sensorCount; }

    /**
     * Set callback for sensor readings
     */
    void setCallback(SensorCallback cb) { m_callback = cb; }

    /**
     * Force immediate poll
     */
    void forcePoll();

    /**
     * Get last temperature reading (from SHT31)
     */
    float getTemperature() const;

    /**
     * Get last humidity reading (from SHT31)
     */
    float getHumidity() const;

private:
    void pollSHT31();
    void reportReading(const SensorReading& reading);

    I2C& m_i2c;
    uint32_t m_pollInterval = DEFAULT_POLL_INTERVAL;
    uint32_t m_lastPoll = 0;
    uint8_t m_sensorCount = 0;

    /* Sensors */
    SHT31 m_sht31;
    bool m_sht31Present = false;

    /* Callback */
    SensorCallback m_callback;
};

} // namespace smarthub::m4
