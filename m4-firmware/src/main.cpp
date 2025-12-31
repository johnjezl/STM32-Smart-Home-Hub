/**
 * SmartHub M4 Firmware - Main Entry Point
 *
 * STM32MP157F-DK2 Smart Home Hub
 * Runs on Cortex-M4 bare-metal
 *
 * Features:
 * - Sensor data acquisition (I2C sensors)
 * - Real-time GPIO control
 * - Inter-processor communication with A7 (Linux)
 */

#include <cstdint>
#include "smarthub_m4/stm32mp1xx.h"
#include "smarthub_m4/drivers/clock.hpp"
#include "smarthub_m4/drivers/gpio.hpp"
#include "smarthub_m4/drivers/i2c.hpp"
#include "smarthub_m4/rpmsg/rpmsg.hpp"
#include "smarthub_m4/sensors/sensor_manager.hpp"

using namespace smarthub::m4;

/* Debug trace buffer */
static volatile char* trace_buf = (volatile char*)0x10049000;
static size_t trace_pos = 0;

static void trace_main(const char* msg) {
    while (*msg && trace_pos < 0x1000) {
        trace_buf[trace_pos++] = *msg++;
    }
    if (trace_pos < 0x1000) trace_buf[trace_pos++] = '\n';
}

/* Global objects */
static I2C* g_i2c = nullptr;
static SensorManager* g_sensors = nullptr;

/**
 * Handle commands from A7
 */
static void handleCommand(const MsgHeader* hdr, const void* payload, size_t len) {
    switch ((MsgType)hdr->type) {
        case MsgType::CMD_SET_INTERVAL: {
            if (len >= sizeof(uint16_t) && g_sensors) {
                uint16_t interval = *(const uint16_t*)payload;
                g_sensors->setPollingInterval(interval);
            }
            break;
        }

        case MsgType::CMD_GET_SENSOR_DATA: {
            if (g_sensors) {
                g_sensors->forcePoll();
            }
            break;
        }

        case MsgType::CMD_SET_GPIO: {
            if (len >= sizeof(GPIOPayload)) {
                const GPIOPayload* gpio = (const GPIOPayload*)payload;
                GPIO_TypeDef* port = nullptr;
                switch (gpio->port) {
                    case 0: port = GPIOA; break;
                    case 1: port = GPIOB; break;
                    case 2: port = GPIOC; break;
                    case 3: port = GPIOD; break;
                    case 4: port = GPIOE; break;
                    case 5: port = GPIOF; break;
                    case 6: port = GPIOG; break;
                    case 7: port = GPIOH; break;
                    case 8: port = GPIOI; break;
                }
                if (port && gpio->pin < 16) {
                    switch (gpio->state) {
                        case 0: GPIO::reset(port, gpio->pin); break;
                        case 1: GPIO::set(port, gpio->pin); break;
                        case 2: GPIO::toggle(port, gpio->pin); break;
                    }
                }
            }
            break;
        }

        case MsgType::CMD_GET_GPIO: {
            if (len >= sizeof(GPIOPayload)) {
                const GPIOPayload* gpio = (const GPIOPayload*)payload;
                GPIO_TypeDef* port = nullptr;
                switch (gpio->port) {
                    case 0: port = GPIOA; break;
                    case 1: port = GPIOB; break;
                    case 2: port = GPIOC; break;
                    case 3: port = GPIOD; break;
                    case 4: port = GPIOE; break;
                    case 5: port = GPIOF; break;
                    case 6: port = GPIOG; break;
                    case 7: port = GPIOH; break;
                    case 8: port = GPIOI; break;
                }
                if (port && gpio->pin < 16) {
                    GPIOPayload response;
                    response.port = gpio->port;
                    response.pin = gpio->pin;
                    response.state = GPIO::read(port, gpio->pin) ? 1 : 0;
                    response.mode = 0;
                    rpmsg().send(MsgType::RSP_GPIO_STATE, &response, sizeof(response));
                }
            }
            break;
        }

        default:
            break;
    }
}

/**
 * System initialization
 */
static bool system_init() {
    trace_main("INIT:clock");
    /* Initialize system clock and SysTick */
    Clock::init();

    trace_main("INIT:delay");
    /* Small delay for stability - use busy wait to avoid SysTick dependency */
    for (volatile int i = 0; i < 100000; i++) { __asm volatile("nop"); }

    /* Skip I2C and sensors for now - focus on RPMsg */
    trace_main("INIT:skip i2c/sensors");

    trace_main("INIT:rpmsg");
    /* Initialize RPMsg for A7 communication */
    if (!rpmsg().init()) {
        trace_main("RPMSG:fail");
        return false;
    }
    trace_main("RPMSG:OK");

    /* Set command handler */
    rpmsg().setCallback(handleCommand);

    return true;
}

/**
 * Main loop - sensor polling and communication
 */
static void main_loop() {
    /* Poll RPMsg for incoming commands */
    rpmsg().poll();

    /* Poll sensors */
    if (g_sensors) {
        g_sensors->poll();
    }
}

/**
 * Error handler
 */
extern "C" void Error_Handler() {
    __disable_irq();
    while (true) {
        __WFI();
    }
}

/**
 * Main entry point
 */
int main() {
    trace_main("MAIN:start");

    /* Initialize hardware and peripherals */
    if (!system_init()) {
        trace_main("MAIN:init FAIL");
        Error_Handler();
    }
    trace_main("MAIN:init OK");

    /* Send initial status */
    rpmsg().sendStatus(Clock::getTicks(),
                       g_sensors ? g_sensors->getSensorCount() : 0,
                       g_sensors ? g_sensors->getPollingInterval() : 0);

    /* Main processing loop */
    while (true) {
        main_loop();

        /* Wait for interrupt to save power */
        __WFI();
    }

    return 0;
}
