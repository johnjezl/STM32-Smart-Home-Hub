/**
 * STM32MP1 M4 GPIO Driver
 */
#pragma once

#include <cstdint>
#include "smarthub_m4/stm32mp1xx.h"

namespace smarthub::m4 {

enum class PinMode {
    Input,
    Output,
    AlternateFunction,
    Analog
};

enum class PinPull {
    None,
    PullUp,
    PullDown
};

enum class PinSpeed {
    Low,
    Medium,
    High,
    VeryHigh
};

enum class OutputType {
    PushPull,
    OpenDrain
};

/**
 * GPIO Pin configuration and control
 */
class GPIO {
public:
    /**
     * Configure a GPIO pin
     * @param port GPIO port (GPIOA, GPIOB, etc.)
     * @param pin Pin number (0-15)
     * @param mode Pin mode
     * @param pull Pull-up/pull-down configuration
     * @param speed Output speed
     * @param otype Output type (push-pull or open-drain)
     * @param af Alternate function number (0-15, only if mode is AF)
     */
    static void configure(GPIO_TypeDef* port, uint8_t pin,
                          PinMode mode,
                          PinPull pull = PinPull::None,
                          PinSpeed speed = PinSpeed::Medium,
                          OutputType otype = OutputType::PushPull,
                          uint8_t af = 0);

    /**
     * Set pin high
     */
    static void set(GPIO_TypeDef* port, uint8_t pin);

    /**
     * Set pin low
     */
    static void reset(GPIO_TypeDef* port, uint8_t pin);

    /**
     * Toggle pin state
     */
    static void toggle(GPIO_TypeDef* port, uint8_t pin);

    /**
     * Read pin state
     * @return true if pin is high
     */
    static bool read(GPIO_TypeDef* port, uint8_t pin);

    /**
     * Write pin state
     */
    static void write(GPIO_TypeDef* port, uint8_t pin, bool state);
};

/**
 * LED helper class for STM32MP157F-DK2
 * The board has user LEDs on specific pins
 */
class LED {
public:
    enum Index {
        LED1 = 0,  /* LD7 - PA14 (typically) */
        LED2 = 1,  /* LD8 */
    };

    static void init();
    static void on(Index led);
    static void off(Index led);
    static void toggle(Index led);
};

} // namespace smarthub::m4
