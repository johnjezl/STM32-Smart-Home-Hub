/**
 * STM32MP1 M4 I2C Driver Implementation
 */

#include "smarthub_m4/drivers/i2c.hpp"
#include "smarthub_m4/drivers/clock.hpp"

namespace smarthub::m4 {

/* I2C timing values for 104.5 MHz APB clock */
/* These values should be calculated based on actual clock frequency */
static constexpr uint32_t I2C_TIMING_100KHZ = 0x10D07DB5;  /* Standard mode */
static constexpr uint32_t I2C_TIMING_400KHZ = 0x00D00E28;  /* Fast mode */
static constexpr uint32_t I2C_TIMING_1MHZ   = 0x00500816;  /* Fast mode plus */

static constexpr uint32_t I2C_TIMEOUT_MS = 100;

I2C::I2C(I2C_TypeDef* instance)
    : m_i2c(instance) {
}

bool I2C::init(Speed speed) {
    if (!m_i2c) return false;

    /* Disable peripheral first */
    m_i2c->CR1 &= ~I2C_CR1_PE;

    /* Configure timing */
    configTiming(speed);

    /* Configure I2C */
    m_i2c->CR1 = 0;  /* Clear control register */
    m_i2c->CR2 = 0;  /* Clear control register 2 */

    /* Enable peripheral */
    m_i2c->CR1 |= I2C_CR1_PE;

    m_initialized = true;
    return true;
}

void I2C::deinit() {
    if (m_i2c) {
        m_i2c->CR1 &= ~I2C_CR1_PE;
    }
    m_initialized = false;
}

void I2C::configTiming(Speed speed) {
    uint32_t timing;
    switch (speed) {
        case Speed::Standard:
            timing = I2C_TIMING_100KHZ;
            break;
        case Speed::Fast:
            timing = I2C_TIMING_400KHZ;
            break;
        case Speed::FastPlus:
            timing = I2C_TIMING_1MHZ;
            break;
        default:
            timing = I2C_TIMING_400KHZ;
            break;
    }
    m_i2c->TIMINGR = timing;
}

bool I2C::waitFlag(uint32_t flag, bool set, uint32_t timeout) {
    uint32_t start = Clock::getTicks();
    while ((Clock::getTicks() - start) < timeout) {
        bool flagSet = (m_i2c->ISR & flag) != 0;
        if (flagSet == set) {
            return true;
        }
        /* Check for errors */
        if (m_i2c->ISR & I2C_ISR_NACKF) {
            m_i2c->ICR = I2C_ICR_NACKCF;
            return false;
        }
    }
    return false;
}

bool I2C::waitBusy(uint32_t timeout) {
    uint32_t start = Clock::getTicks();
    while ((Clock::getTicks() - start) < timeout) {
        if (!(m_i2c->ISR & I2C_ISR_BUSY)) {
            return true;
        }
    }
    return false;
}

bool I2C::probe(uint8_t addr) {
    if (!m_initialized) return false;

    /* Wait for bus to be free */
    if (!waitBusy(I2C_TIMEOUT_MS)) return false;

    /* Configure transfer: 0 bytes, write mode, autoend */
    m_i2c->CR2 = ((uint32_t)addr << 1) |  /* 7-bit address shifted */
                 I2C_CR2_AUTOEND |
                 (0 << I2C_CR2_NBYTES_POS);

    /* Generate START */
    m_i2c->CR2 |= I2C_CR2_START;

    /* Wait for STOP (autoend) or NACK */
    uint32_t start = Clock::getTicks();
    while ((Clock::getTicks() - start) < I2C_TIMEOUT_MS) {
        if (m_i2c->ISR & I2C_ISR_STOPF) {
            m_i2c->ICR = I2C_ICR_STOPCF;
            return true;  /* Device acknowledged */
        }
        if (m_i2c->ISR & I2C_ISR_NACKF) {
            m_i2c->ICR = I2C_ICR_NACKCF;
            return false;  /* No acknowledge */
        }
    }
    return false;
}

bool I2C::write(uint8_t addr, const uint8_t* data, size_t len) {
    if (!m_initialized || !data || len == 0) return false;

    /* Wait for bus to be free */
    if (!waitBusy(I2C_TIMEOUT_MS)) return false;

    /* Configure transfer */
    m_i2c->CR2 = ((uint32_t)addr << 1) |
                 I2C_CR2_AUTOEND |
                 ((uint32_t)len << I2C_CR2_NBYTES_POS);

    /* Generate START */
    m_i2c->CR2 |= I2C_CR2_START;

    /* Send data */
    for (size_t i = 0; i < len; i++) {
        if (!waitFlag(I2C_ISR_TXIS, true, I2C_TIMEOUT_MS)) {
            return false;
        }
        m_i2c->TXDR = data[i];
    }

    /* Wait for transfer complete */
    if (!waitFlag(I2C_ISR_STOPF, true, I2C_TIMEOUT_MS)) {
        return false;
    }

    /* Clear STOP flag */
    m_i2c->ICR = I2C_ICR_STOPCF;

    return true;
}

bool I2C::read(uint8_t addr, uint8_t* data, size_t len) {
    if (!m_initialized || !data || len == 0) return false;

    /* Wait for bus to be free */
    if (!waitBusy(I2C_TIMEOUT_MS)) return false;

    /* Configure transfer */
    m_i2c->CR2 = ((uint32_t)addr << 1) |
                 I2C_CR2_RD_WRN |
                 I2C_CR2_AUTOEND |
                 ((uint32_t)len << I2C_CR2_NBYTES_POS);

    /* Generate START */
    m_i2c->CR2 |= I2C_CR2_START;

    /* Read data */
    for (size_t i = 0; i < len; i++) {
        if (!waitFlag(I2C_ISR_RXNE, true, I2C_TIMEOUT_MS)) {
            return false;
        }
        data[i] = m_i2c->RXDR;
    }

    /* Wait for transfer complete */
    if (!waitFlag(I2C_ISR_STOPF, true, I2C_TIMEOUT_MS)) {
        return false;
    }

    /* Clear STOP flag */
    m_i2c->ICR = I2C_ICR_STOPCF;

    return true;
}

bool I2C::writeReg(uint8_t addr, uint8_t reg, uint8_t value) {
    uint8_t data[2] = { reg, value };
    return write(addr, data, 2);
}

bool I2C::writeRegs(uint8_t addr, uint8_t reg, const uint8_t* data, size_t len) {
    if (!m_initialized || !data) return false;

    /* Wait for bus to be free */
    if (!waitBusy(I2C_TIMEOUT_MS)) return false;

    /* Configure transfer: register + data */
    m_i2c->CR2 = ((uint32_t)addr << 1) |
                 I2C_CR2_AUTOEND |
                 ((uint32_t)(len + 1) << I2C_CR2_NBYTES_POS);

    /* Generate START */
    m_i2c->CR2 |= I2C_CR2_START;

    /* Send register address */
    if (!waitFlag(I2C_ISR_TXIS, true, I2C_TIMEOUT_MS)) {
        return false;
    }
    m_i2c->TXDR = reg;

    /* Send data */
    for (size_t i = 0; i < len; i++) {
        if (!waitFlag(I2C_ISR_TXIS, true, I2C_TIMEOUT_MS)) {
            return false;
        }
        m_i2c->TXDR = data[i];
    }

    /* Wait for transfer complete */
    if (!waitFlag(I2C_ISR_STOPF, true, I2C_TIMEOUT_MS)) {
        return false;
    }

    /* Clear STOP flag */
    m_i2c->ICR = I2C_ICR_STOPCF;

    return true;
}

uint8_t I2C::readReg(uint8_t addr, uint8_t reg) {
    uint8_t value = 0;
    readRegs(addr, reg, &value, 1);
    return value;
}

bool I2C::readRegs(uint8_t addr, uint8_t reg, uint8_t* data, size_t len) {
    if (!m_initialized || !data || len == 0) return false;

    /* Wait for bus to be free */
    if (!waitBusy(I2C_TIMEOUT_MS)) return false;

    /* First: Write register address (no STOP) */
    m_i2c->CR2 = ((uint32_t)addr << 1) |
                 (1 << I2C_CR2_NBYTES_POS);

    /* Generate START */
    m_i2c->CR2 |= I2C_CR2_START;

    /* Send register address */
    if (!waitFlag(I2C_ISR_TXIS, true, I2C_TIMEOUT_MS)) {
        return false;
    }
    m_i2c->TXDR = reg;

    /* Wait for transfer complete (TC, not STOP since no autoend) */
    if (!waitFlag(I2C_ISR_TC, true, I2C_TIMEOUT_MS)) {
        return false;
    }

    /* Second: Read data with repeated START */
    m_i2c->CR2 = ((uint32_t)addr << 1) |
                 I2C_CR2_RD_WRN |
                 I2C_CR2_AUTOEND |
                 ((uint32_t)len << I2C_CR2_NBYTES_POS);

    /* Generate repeated START */
    m_i2c->CR2 |= I2C_CR2_START;

    /* Read data */
    for (size_t i = 0; i < len; i++) {
        if (!waitFlag(I2C_ISR_RXNE, true, I2C_TIMEOUT_MS)) {
            return false;
        }
        data[i] = m_i2c->RXDR;
    }

    /* Wait for STOP */
    if (!waitFlag(I2C_ISR_STOPF, true, I2C_TIMEOUT_MS)) {
        return false;
    }

    /* Clear STOP flag */
    m_i2c->ICR = I2C_ICR_STOPCF;

    return true;
}

uint16_t I2C::readReg16BE(uint8_t addr, uint8_t reg) {
    uint8_t data[2] = {0, 0};
    if (readRegs(addr, reg, data, 2)) {
        return ((uint16_t)data[0] << 8) | data[1];
    }
    return 0;
}

} // namespace smarthub::m4
