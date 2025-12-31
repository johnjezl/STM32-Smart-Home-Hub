/**
 * STM32MP1 M4 Clock Driver
 */
#pragma once

#include <cstdint>

namespace smarthub::m4 {

enum class GPIO_Port {
    A, B, C, D, E, F, G, H, I
};

enum class I2C_Instance {
    Instance1, Instance2, Instance3, Instance5
};

/**
 * Clock configuration and timing utilities
 */
class Clock {
public:
    /**
     * Initialize system tick and clock configuration
     */
    static void init();

    /**
     * Enable GPIO port clock
     */
    static void enableGPIO(GPIO_Port port);

    /**
     * Enable I2C peripheral clock
     */
    static void enableI2C(I2C_Instance instance);

    /**
     * Get CPU frequency in Hz
     */
    static uint32_t getCpuFreq();

    /**
     * Get APB1 frequency in Hz (for I2C, UART, etc.)
     */
    static uint32_t getAPB1Freq();

    /**
     * Get APB2 frequency in Hz
     */
    static uint32_t getAPB2Freq();

    /**
     * Get system tick count (milliseconds since init)
     */
    static uint32_t getTicks();

    /**
     * Delay for specified milliseconds
     */
    static void delayMs(uint32_t ms);

    /**
     * Delay for specified microseconds (busy wait)
     */
    static void delayUs(uint32_t us);
};

} // namespace smarthub::m4
