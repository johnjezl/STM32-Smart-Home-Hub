/**
 * STM32MP1 M4 I2C Driver
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include "smarthub_m4/stm32mp1xx.h"

namespace smarthub::m4 {

/**
 * I2C Master Driver
 *
 * Provides basic I2C master functionality for sensor communication.
 * Supports standard (100 kHz) and fast (400 kHz) modes.
 */
class I2C {
public:
    enum class Speed {
        Standard,  /* 100 kHz */
        Fast,      /* 400 kHz */
        FastPlus   /* 1 MHz */
    };

    /**
     * Create I2C driver for specified peripheral
     * @param instance I2C peripheral (I2C1, I2C2, etc.)
     */
    explicit I2C(I2C_TypeDef* instance);

    /**
     * Initialize I2C peripheral
     * @param speed Bus speed
     * @return true if successful
     */
    bool init(Speed speed = Speed::Fast);

    /**
     * Deinitialize I2C peripheral
     */
    void deinit();

    /**
     * Check if device responds at address
     * @param addr 7-bit I2C address
     * @return true if device acknowledges
     */
    bool probe(uint8_t addr);

    /**
     * Write data to device
     * @param addr 7-bit I2C address
     * @param data Data buffer
     * @param len Data length
     * @return true if successful
     */
    bool write(uint8_t addr, const uint8_t* data, size_t len);

    /**
     * Read data from device
     * @param addr 7-bit I2C address
     * @param data Buffer to store data
     * @param len Number of bytes to read
     * @return true if successful
     */
    bool read(uint8_t addr, uint8_t* data, size_t len);

    /**
     * Write register value
     * @param addr 7-bit I2C address
     * @param reg Register address
     * @param value Value to write
     * @return true if successful
     */
    bool writeReg(uint8_t addr, uint8_t reg, uint8_t value);

    /**
     * Write multiple bytes to register
     * @param addr 7-bit I2C address
     * @param reg Register address
     * @param data Data buffer
     * @param len Data length
     * @return true if successful
     */
    bool writeRegs(uint8_t addr, uint8_t reg, const uint8_t* data, size_t len);

    /**
     * Read register value
     * @param addr 7-bit I2C address
     * @param reg Register address
     * @return Register value (0 on error)
     */
    uint8_t readReg(uint8_t addr, uint8_t reg);

    /**
     * Read multiple bytes from register
     * @param addr 7-bit I2C address
     * @param reg Register address
     * @param data Buffer to store data
     * @param len Number of bytes to read
     * @return true if successful
     */
    bool readRegs(uint8_t addr, uint8_t reg, uint8_t* data, size_t len);

    /**
     * Read 16-bit register (big-endian)
     */
    uint16_t readReg16BE(uint8_t addr, uint8_t reg);

private:
    bool waitFlag(uint32_t flag, bool set, uint32_t timeout);
    bool waitBusy(uint32_t timeout);
    void configTiming(Speed speed);

    I2C_TypeDef* m_i2c;
    bool m_initialized = false;
};

} // namespace smarthub::m4
