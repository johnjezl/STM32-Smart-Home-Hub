/**
 * STM32MP1 Cortex-M4 Clock Configuration
 *
 * On STM32MP1, the A7 (Linux) typically configures the main clocks.
 * The M4 core uses these clocks but may need to enable specific
 * peripheral clocks for GPIO, I2C, etc.
 */

#include "smarthub_m4/stm32mp1xx.h"
#include "smarthub_m4/drivers/clock.hpp"

namespace smarthub::m4 {

/* System tick counter - incremented by SysTick_Handler */
static volatile uint32_t g_systick = 0;

/* CPU frequency (configured by A7, typically 209 MHz for M4) */
static constexpr uint32_t CPU_FREQ_HZ = 209000000UL;
static constexpr uint32_t APB1_FREQ_HZ = 104500000UL;
static constexpr uint32_t APB2_FREQ_HZ = 104500000UL;

void Clock::init() {
    /* Configure SysTick for 1ms interrupts */
    uint32_t ticks = CPU_FREQ_HZ / 1000;

    SysTick->LOAD = ticks - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = SYSTICK_CTRL_CLKSOURCE | SYSTICK_CTRL_TICKINT | SYSTICK_CTRL_ENABLE;

    /* Set SysTick interrupt priority (low priority) */
    NVIC_SetPriority(SysTick_IRQn, 15);
}

void Clock::enableGPIO(GPIO_Port port) {
    uint32_t bit = 0;
    switch (port) {
        case GPIO_Port::A: bit = RCC_AHB4_GPIOAEN; break;
        case GPIO_Port::B: bit = RCC_AHB4_GPIOBEN; break;
        case GPIO_Port::C: bit = RCC_AHB4_GPIOCEN; break;
        case GPIO_Port::D: bit = RCC_AHB4_GPIODEN; break;
        case GPIO_Port::E: bit = RCC_AHB4_GPIOEEN; break;
        case GPIO_Port::F: bit = RCC_AHB4_GPIOFEN; break;
        case GPIO_Port::G: bit = RCC_AHB4_GPIOGEN; break;
        case GPIO_Port::H: bit = RCC_AHB4_GPIOHEN; break;
        case GPIO_Port::I: bit = RCC_AHB4_GPIOIEN; break;
    }

    /* Note: On STM32MP1, GPIO clocks are typically managed by A7/Linux
     * through device tree. The M4 may not have direct access to RCC
     * unless configured. This is a placeholder for when direct access
     * is available or for standalone M4 operation. */

    /* RCC->MC_AHB4ENSETR = bit; */
    (void)bit;
}

void Clock::enableI2C(I2C_Instance instance) {
    uint32_t bit = 0;
    switch (instance) {
        case I2C_Instance::Instance1: bit = RCC_APB1_I2C1EN; break;
        case I2C_Instance::Instance2: bit = RCC_APB1_I2C2EN; break;
        case I2C_Instance::Instance3: bit = RCC_APB1_I2C3EN; break;
        case I2C_Instance::Instance5: bit = RCC_APB1_I2C5EN; break;
    }

    /* RCC->MC_APB1ENSETR = bit; */
    (void)bit;
}

uint32_t Clock::getCpuFreq() {
    return CPU_FREQ_HZ;
}

uint32_t Clock::getAPB1Freq() {
    return APB1_FREQ_HZ;
}

uint32_t Clock::getAPB2Freq() {
    return APB2_FREQ_HZ;
}

uint32_t Clock::getTicks() {
    return g_systick;
}

void Clock::delayMs(uint32_t ms) {
    uint32_t start = g_systick;
    while ((g_systick - start) < ms) {
        __WFI();
    }
}

void Clock::delayUs(uint32_t us) {
    /* Busy-wait delay for microseconds
     * Note: This is approximate and depends on CPU frequency */
    uint32_t cycles = (CPU_FREQ_HZ / 1000000) * us;
    while (cycles--) {
        __asm volatile ("nop");
    }
}

} // namespace smarthub::m4

/* SysTick Handler - called every 1ms */
extern "C" void SysTick_Handler(void) {
    smarthub::m4::g_systick++;
}

/* Make g_systick accessible from namespace */
namespace smarthub::m4 {
    extern volatile uint32_t g_systick;
}
