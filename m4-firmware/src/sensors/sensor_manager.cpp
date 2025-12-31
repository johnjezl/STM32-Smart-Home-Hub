/**
 * SmartHub M4 Sensor Manager Implementation
 */

#include "smarthub_m4/sensors/sensor_manager.hpp"
#include "smarthub_m4/drivers/clock.hpp"

namespace smarthub::m4 {

SensorManager::SensorManager(I2C& i2c)
    : m_i2c(i2c)
    , m_sht31(i2c, SHT31::DEFAULT_ADDR) {
}

uint8_t SensorManager::init() {
    m_sensorCount = 0;

    /* Try to initialize SHT31 */
    if (m_sht31.init()) {
        m_sht31Present = true;
        m_sensorCount += 2;  /* Temperature and humidity */
    }

    /* Add initialization for other sensors here */
    /* e.g., BME280, TSL2561, etc. */

    return m_sensorCount;
}

void SensorManager::poll() {
    uint32_t now = Clock::getTicks();

    /* Check if it's time to poll */
    if ((now - m_lastPoll) < m_pollInterval) {
        return;
    }

    m_lastPoll = now;

    /* Poll all sensors */
    if (m_sht31Present) {
        pollSHT31();
    }
}

void SensorManager::forcePoll() {
    m_lastPoll = 0;
    poll();
}

void SensorManager::pollSHT31() {
    if (!m_sht31.measure()) {
        return;
    }

    /* Report temperature */
    SensorReading tempReading;
    tempReading.id = 0;
    tempReading.type = SensorType::Temperature;
    tempReading.value = m_sht31.temperatureFixed();
    tempReading.scale = 100;
    tempReading.timestamp = Clock::getTicks();
    reportReading(tempReading);

    /* Report humidity */
    SensorReading humReading;
    humReading.id = 1;
    humReading.type = SensorType::Humidity;
    humReading.value = m_sht31.humidityFixed();
    humReading.scale = 100;
    humReading.timestamp = Clock::getTicks();
    reportReading(humReading);
}

void SensorManager::reportReading(const SensorReading& reading) {
    /* Send via RPMsg */
    rpmsg().sendSensorData(reading.id, reading.type,
                           reading.value, reading.scale);

    /* Call user callback */
    if (m_callback) {
        m_callback(reading);
    }
}

float SensorManager::getTemperature() const {
    if (m_sht31Present) {
        return m_sht31.temperature();
    }
    return 0.0f;
}

float SensorManager::getHumidity() const {
    if (m_sht31Present) {
        return m_sht31.humidity();
    }
    return 0.0f;
}

} // namespace smarthub::m4
