/**
 * SHT31 Temperature/Humidity Sensor Implementation
 */

#include "smarthub_m4/sensors/sht31.hpp"
#include "smarthub_m4/drivers/clock.hpp"

namespace smarthub::m4 {

/* SHT31 Commands */
static constexpr uint16_t CMD_MEAS_HIGH_REP     = 0x2400;  /* High repeatability, no clock stretch */
static constexpr uint16_t CMD_MEAS_MED_REP      = 0x240B;  /* Medium repeatability */
static constexpr uint16_t CMD_MEAS_LOW_REP      = 0x2416;  /* Low repeatability */
static constexpr uint16_t CMD_SOFT_RESET        = 0x30A2;
static constexpr uint16_t CMD_HEATER_ENABLE     = 0x306D;
static constexpr uint16_t CMD_HEATER_DISABLE    = 0x3066;
static constexpr uint16_t CMD_READ_STATUS       = 0xF32D;
static constexpr uint16_t CMD_CLEAR_STATUS      = 0x3041;

/* CRC polynomial for SHT31 */
static constexpr uint8_t CRC_POLYNOMIAL = 0x31;

SHT31::SHT31(I2C& i2c, uint8_t addr)
    : m_i2c(i2c)
    , m_addr(addr) {
}

bool SHT31::init() {
    /* Check if sensor is present */
    m_present = m_i2c.probe(m_addr);
    if (!m_present) {
        return false;
    }

    /* Soft reset */
    if (!reset()) {
        m_present = false;
        return false;
    }

    /* Wait for reset */
    Clock::delayMs(2);

    /* Clear status */
    clearStatus();

    return true;
}

bool SHT31::measure(SHT31_Repeatability rep) {
    if (!m_present) return false;

    /* Select measurement command based on repeatability */
    uint16_t cmd;
    uint32_t delayMs;
    switch (rep) {
        case SHT31_Repeatability::Low:
            cmd = CMD_MEAS_LOW_REP;
            delayMs = 4;
            break;
        case SHT31_Repeatability::Medium:
            cmd = CMD_MEAS_MED_REP;
            delayMs = 6;
            break;
        case SHT31_Repeatability::High:
        default:
            cmd = CMD_MEAS_HIGH_REP;
            delayMs = 15;
            break;
    }

    /* Send measurement command */
    if (!sendCommand(cmd)) {
        return false;
    }

    /* Wait for measurement */
    Clock::delayMs(delayMs);

    /* Read 6 bytes: temp MSB, temp LSB, temp CRC, hum MSB, hum LSB, hum CRC */
    uint8_t data[6];
    if (!readData(data, 6)) {
        return false;
    }

    /* Verify CRCs */
    if (crc8(&data[0], 2) != data[2]) {
        return false;
    }
    if (crc8(&data[3], 2) != data[5]) {
        return false;
    }

    /* Convert temperature */
    uint16_t rawTemp = ((uint16_t)data[0] << 8) | data[1];
    m_temperature = -45.0f + 175.0f * ((float)rawTemp / 65535.0f);

    /* Convert humidity */
    uint16_t rawHum = ((uint16_t)data[3] << 8) | data[4];
    m_humidity = 100.0f * ((float)rawHum / 65535.0f);

    /* Clamp humidity */
    if (m_humidity > 100.0f) m_humidity = 100.0f;
    if (m_humidity < 0.0f) m_humidity = 0.0f;

    return true;
}

bool SHT31::reset() {
    return sendCommand(CMD_SOFT_RESET);
}

bool SHT31::setHeater(bool enable) {
    return sendCommand(enable ? CMD_HEATER_ENABLE : CMD_HEATER_DISABLE);
}

uint16_t SHT31::readStatus() {
    if (!sendCommand(CMD_READ_STATUS)) {
        return 0xFFFF;
    }

    Clock::delayMs(1);

    uint8_t data[3];
    if (!readData(data, 3)) {
        return 0xFFFF;
    }

    /* Verify CRC */
    if (crc8(&data[0], 2) != data[2]) {
        return 0xFFFF;
    }

    return ((uint16_t)data[0] << 8) | data[1];
}

bool SHT31::clearStatus() {
    return sendCommand(CMD_CLEAR_STATUS);
}

bool SHT31::sendCommand(uint16_t cmd) {
    uint8_t data[2] = {
        (uint8_t)(cmd >> 8),
        (uint8_t)(cmd & 0xFF)
    };
    return m_i2c.write(m_addr, data, 2);
}

bool SHT31::readData(uint8_t* data, size_t len) {
    return m_i2c.read(m_addr, data, len);
}

uint8_t SHT31::crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ CRC_POLYNOMIAL;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

} // namespace smarthub::m4
