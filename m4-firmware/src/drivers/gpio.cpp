/**
 * STM32MP1 M4 GPIO Driver Implementation
 */

#include "smarthub_m4/drivers/gpio.hpp"

namespace smarthub::m4 {

void GPIO::configure(GPIO_TypeDef* port, uint8_t pin,
                     PinMode mode,
                     PinPull pull,
                     PinSpeed speed,
                     OutputType otype,
                     uint8_t af) {
    if (pin > 15) return;

    uint32_t pos = pin;
    uint32_t pos2 = pin * 2;

    /* Configure mode */
    uint32_t moder = port->MODER;
    moder &= ~(0x3UL << pos2);
    switch (mode) {
        case PinMode::Input:
            moder |= (GPIO_MODE_INPUT << pos2);
            break;
        case PinMode::Output:
            moder |= (GPIO_MODE_OUTPUT << pos2);
            break;
        case PinMode::AlternateFunction:
            moder |= (GPIO_MODE_AF << pos2);
            break;
        case PinMode::Analog:
            moder |= (GPIO_MODE_ANALOG << pos2);
            break;
    }
    port->MODER = moder;

    /* Configure output type */
    uint32_t otyper = port->OTYPER;
    otyper &= ~(1UL << pos);
    if (otype == OutputType::OpenDrain) {
        otyper |= (1UL << pos);
    }
    port->OTYPER = otyper;

    /* Configure speed */
    uint32_t ospeedr = port->OSPEEDR;
    ospeedr &= ~(0x3UL << pos2);
    switch (speed) {
        case PinSpeed::Low:
            ospeedr |= (GPIO_SPEED_LOW << pos2);
            break;
        case PinSpeed::Medium:
            ospeedr |= (GPIO_SPEED_MEDIUM << pos2);
            break;
        case PinSpeed::High:
            ospeedr |= (GPIO_SPEED_HIGH << pos2);
            break;
        case PinSpeed::VeryHigh:
            ospeedr |= (GPIO_SPEED_VHIGH << pos2);
            break;
    }
    port->OSPEEDR = ospeedr;

    /* Configure pull-up/pull-down */
    uint32_t pupdr = port->PUPDR;
    pupdr &= ~(0x3UL << pos2);
    switch (pull) {
        case PinPull::None:
            pupdr |= (GPIO_PUPD_NONE << pos2);
            break;
        case PinPull::PullUp:
            pupdr |= (GPIO_PUPD_UP << pos2);
            break;
        case PinPull::PullDown:
            pupdr |= (GPIO_PUPD_DOWN << pos2);
            break;
    }
    port->PUPDR = pupdr;

    /* Configure alternate function */
    if (mode == PinMode::AlternateFunction) {
        uint32_t afr_idx = (pin < 8) ? 0 : 1;
        uint32_t afr_pos = (pin & 0x7) * 4;
        uint32_t afr = port->AFR[afr_idx];
        afr &= ~(0xFUL << afr_pos);
        afr |= ((uint32_t)af << afr_pos);
        port->AFR[afr_idx] = afr;
    }
}

void GPIO::set(GPIO_TypeDef* port, uint8_t pin) {
    port->BSRR = (1UL << pin);
}

void GPIO::reset(GPIO_TypeDef* port, uint8_t pin) {
    port->BSRR = (1UL << (pin + 16));
}

void GPIO::toggle(GPIO_TypeDef* port, uint8_t pin) {
    if (port->ODR & (1UL << pin)) {
        reset(port, pin);
    } else {
        set(port, pin);
    }
}

bool GPIO::read(GPIO_TypeDef* port, uint8_t pin) {
    return (port->IDR & (1UL << pin)) != 0;
}

void GPIO::write(GPIO_TypeDef* port, uint8_t pin, bool state) {
    if (state) {
        set(port, pin);
    } else {
        reset(port, pin);
    }
}

/* LED implementation - adjust pins based on actual board configuration */
/* STM32MP157F-DK2 has LEDs but M4 access depends on device tree */

void LED::init() {
    /* LEDs are typically configured by Linux device tree
     * This is a placeholder for direct M4 control */
}

void LED::on(Index led) {
    (void)led;
    /* Implementation depends on actual pin assignment */
}

void LED::off(Index led) {
    (void)led;
}

void LED::toggle(Index led) {
    (void)led;
}

} // namespace smarthub::m4
