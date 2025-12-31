/**
 * SHT31 Temperature/Humidity Sensor Driver
 *
 * Sensirion SHT31 digital humidity and temperature sensor.
 * I2C address: 0x44 (ADDR pin low) or 0x45 (ADDR pin high)
 */
#pragma once

#include <cstdint>
#include "smarthub_m4/drivers/i2c.hpp"

namespace smarthub::m4 {

/**
 * SHT31 Repeatability modes
 */
enum class SHT31_Repeatability {
    Low,        /* 2.5ms measurement time */
    Medium,     /* 4.5ms measurement time */
    High        /* 12.5ms measurement time */
};

/**
 * SHT31 Temperature/Humidity Sensor
 */
class SHT31 {
public:
    static constexpr uint8_t DEFAULT_ADDR = 0x44;
    static constexpr uint8_t ALT_ADDR = 0x45;

    /**
     * Create SHT31 driver
     * @param i2c I2C bus instance
     * @param addr I2C address (0x44 or 0x45)
     */
    SHT31(I2C& i2c, uint8_t addr = DEFAULT_ADDR);

    /**
     * Initialize sensor
     * @return true if sensor responds
     */
    bool init();

    /**
     * Perform single-shot measurement
     * @param repeatability Measurement repeatability
     * @return true if measurement successful
     */
    bool measure(SHT31_Repeatability rep = SHT31_Repeatability::High);

    /**
     * Get last measured temperature
     * @return Temperature in Celsius
     */
    float temperature() const { return m_temperature; }

    /**
     * Get last measured humidity
     * @return Relative humidity in %
     */
    float humidity() const { return m_humidity; }

    /**
     * Get temperature as fixed-point (x100)
     * @return Temperature * 100
     */
    int32_t temperatureFixed() const { return (int32_t)(m_temperature * 100); }

    /**
     * Get humidity as fixed-point (x100)
     * @return Humidity * 100
     */
    int32_t humidityFixed() const { return (int32_t)(m_humidity * 100); }

    /**
     * Perform soft reset
     */
    bool reset();

    /**
     * Enable internal heater (for sensor testing)
     */
    bool setHeater(bool enable);

    /**
     * Read sensor status register
     */
    uint16_t readStatus();

    /**
     * Clear status register
     */
    bool clearStatus();

    /**
     * Check if sensor is present
     */
    bool isPresent() const { return m_present; }

private:
    bool sendCommand(uint16_t cmd);
    bool readData(uint8_t* data, size_t len);
    static uint8_t crc8(const uint8_t* data, size_t len);

    I2C& m_i2c;
    uint8_t m_addr;
    bool m_present = false;
    float m_temperature = 0.0f;
    float m_humidity = 0.0f;
};

} // namespace smarthub::m4
