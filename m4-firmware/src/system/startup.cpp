/**
 * STM32MP1 Cortex-M4 Startup Code
 *
 * Vector table and Reset_Handler implementation.
 */

#include <cstdint>
#include "smarthub_m4/stm32mp1xx.h"

/* External symbols from linker script */
extern uint32_t _sidata;    /* Start of .data in flash */
extern uint32_t _sdata;     /* Start of .data in RAM */
extern uint32_t _edata;     /* End of .data in RAM */
extern uint32_t _sbss;      /* Start of .bss */
extern uint32_t _ebss;      /* End of .bss */
extern uint32_t _estack;    /* Top of stack */

/* Main function */
extern int main(void);

/* C++ static constructors */
extern void (*__preinit_array_start[])(void);
extern void (*__preinit_array_end[])(void);
extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);

/* C++ ABI support for bare metal */
extern "C" void* __dso_handle = nullptr;

/* C++ pure virtual call handler */
extern "C" void __cxa_pure_virtual() {
    while (1) { __WFI(); }
}

/* C++ atexit (not used in bare metal) */
extern "C" int __cxa_atexit(void (*)(void*), void*, void*) {
    return 0;
}

/* Default interrupt handler */
extern "C" void Default_Handler(void) {
    while (1) {
        __WFI();
    }
}

/* Exception and interrupt handlers - weak aliases to Default_Handler */
extern "C" {
    void NMI_Handler(void)              __attribute__((weak, alias("Default_Handler")));
    void HardFault_Handler(void)        __attribute__((weak));
    void MemManage_Handler(void)        __attribute__((weak, alias("Default_Handler")));
    void BusFault_Handler(void)         __attribute__((weak, alias("Default_Handler")));
    void UsageFault_Handler(void)       __attribute__((weak, alias("Default_Handler")));
    void SVC_Handler(void)              __attribute__((weak, alias("Default_Handler")));
    void DebugMon_Handler(void)         __attribute__((weak, alias("Default_Handler")));
    void PendSV_Handler(void)           __attribute__((weak, alias("Default_Handler")));
    void SysTick_Handler(void)          __attribute__((weak, alias("Default_Handler")));

    /* STM32MP1 specific interrupts - define as weak aliases */
    void WWDG1_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
    void EXTI0_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
    void EXTI1_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
    void EXTI2_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
    void EXTI3_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
    void EXTI4_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
    void DMA1_Stream0_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
    void DMA1_Stream1_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
    void DMA1_Stream2_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
    void DMA1_Stream3_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
    void DMA1_Stream4_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
    void DMA1_Stream5_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
    void DMA1_Stream6_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
    void I2C1_EV_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
    void I2C1_ER_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
    void I2C2_EV_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
    void I2C2_ER_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
    void I2C3_EV_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
    void I2C3_ER_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
    void I2C5_EV_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
    void I2C5_ER_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
    void IPCC_RX0_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
    void IPCC_TX0_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
}

/* Hard Fault Handler with trace logging */
extern "C" void HardFault_Handler(void) {
    /* Write to trace buffer to indicate fault */
    volatile char* trace = (volatile char*)0x10049200;  /* Offset to not overwrite main trace */
    trace[0] = 'H'; trace[1] = 'A'; trace[2] = 'R'; trace[3] = 'D';
    trace[4] = 'F'; trace[5] = 'A'; trace[6] = 'U'; trace[7] = 'L';
    trace[8] = 'T'; trace[9] = ':';

    /* Capture and log CFSR (Configurable Fault Status Register) */
    volatile uint32_t cfsr = SCB->CFSR;
    volatile uint32_t bfar = SCB->BFAR;

    /* Write CFSR as hex to trace */
    const char hex[] = "0123456789ABCDEF";
    trace[10] = 'C'; trace[11] = 'F'; trace[12] = 'S'; trace[13] = 'R'; trace[14] = '=';
    for (int i = 0; i < 8; i++) {
        trace[15 + i] = hex[(cfsr >> (28 - i*4)) & 0xF];
    }
    trace[23] = ' ';
    trace[24] = 'B'; trace[25] = 'F'; trace[26] = 'A'; trace[27] = 'R'; trace[28] = '=';
    for (int i = 0; i < 8; i++) {
        trace[29 + i] = hex[(bfar >> (28 - i*4)) & 0xF];
    }
    trace[37] = '\n';

    /* Infinite loop */
    while (1) {
        __asm volatile ("nop");
    }
}

/**
 * Reset Handler - Entry point after reset
 */
extern "C" void Reset_Handler(void) {
    /* Very early trace - write magic bytes before any init */
    volatile char* trace = (volatile char*)0x10049000;
    trace[0] = 'R';
    trace[1] = 'S';
    trace[2] = 'T';
    trace[3] = '!';

    /* Copy initialized data from flash to RAM */
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    /* Zero-fill BSS section */
    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0;
    }

    /* Call C++ static constructors (preinit) */
    for (void (**p)(void) = __preinit_array_start; p < __preinit_array_end; ++p) {
        (*p)();
    }

    /* Call C++ static constructors (init) */
    for (void (**p)(void) = __init_array_start; p < __init_array_end; ++p) {
        (*p)();
    }

    /* Enable FPU (CP10 and CP11) */
    SCB->CPACR |= ((3UL << 10*2) | (3UL << 11*2));
    __DSB();
    __ISB();

    /* Call main */
    main();

    /* Should never return, but if it does, loop forever */
    while (1) {
        __WFI();
    }
}

/**
 * Vector Table
 *
 * Must be placed in RETRAM at 0x00000000 for M4 boot on STM32MP1.
 * The M4 always fetches the initial SP and reset vector from 0x00000000.
 * Code remains in MCUSRAM - only the vector table is in RETRAM.
 */
__attribute__((section(".isr_vector")))
const void* g_pfnVectors[] = {
    /* Initial stack pointer */
    (void*)&_estack,

    /* Cortex-M4 Exception Handlers */
    (void*)Reset_Handler,           /* Reset Handler */
    (void*)NMI_Handler,             /* NMI Handler */
    (void*)HardFault_Handler,       /* Hard Fault Handler */
    (void*)MemManage_Handler,       /* MPU Fault Handler */
    (void*)BusFault_Handler,        /* Bus Fault Handler */
    (void*)UsageFault_Handler,      /* Usage Fault Handler */
    (void*)0,                       /* Reserved */
    (void*)0,                       /* Reserved */
    (void*)0,                       /* Reserved */
    (void*)0,                       /* Reserved */
    (void*)SVC_Handler,             /* SVCall Handler */
    (void*)DebugMon_Handler,        /* Debug Monitor Handler */
    (void*)0,                       /* Reserved */
    (void*)PendSV_Handler,          /* PendSV Handler */
    (void*)SysTick_Handler,         /* SysTick Handler */

    /* STM32MP1 External Interrupts */
    (void*)WWDG1_IRQHandler,        /* 0: Window WatchDog */
    (void*)Default_Handler,         /* 1: Reserved */
    (void*)Default_Handler,         /* 2: Reserved */
    (void*)Default_Handler,         /* 3: Reserved */
    (void*)Default_Handler,         /* 4: Reserved */
    (void*)Default_Handler,         /* 5: Reserved */
    (void*)EXTI0_IRQHandler,        /* 6: EXTI Line0 */
    (void*)EXTI1_IRQHandler,        /* 7: EXTI Line1 */
    (void*)EXTI2_IRQHandler,        /* 8: EXTI Line2 */
    (void*)EXTI3_IRQHandler,        /* 9: EXTI Line3 */
    (void*)EXTI4_IRQHandler,        /* 10: EXTI Line4 */
    (void*)DMA1_Stream0_IRQHandler, /* 11: DMA1 Stream 0 */
    (void*)DMA1_Stream1_IRQHandler, /* 12: DMA1 Stream 1 */
    (void*)DMA1_Stream2_IRQHandler, /* 13: DMA1 Stream 2 */
    (void*)DMA1_Stream3_IRQHandler, /* 14: DMA1 Stream 3 */
    (void*)DMA1_Stream4_IRQHandler, /* 15: DMA1 Stream 4 */
    (void*)DMA1_Stream5_IRQHandler, /* 16: DMA1 Stream 5 */
    (void*)DMA1_Stream6_IRQHandler, /* 17: DMA1 Stream 6 */
    (void*)Default_Handler,         /* 18-30: Reserved/Others */
    (void*)Default_Handler,
    (void*)Default_Handler,
    (void*)Default_Handler,
    (void*)Default_Handler,
    (void*)Default_Handler,
    (void*)Default_Handler,
    (void*)Default_Handler,
    (void*)Default_Handler,
    (void*)Default_Handler,
    (void*)Default_Handler,
    (void*)Default_Handler,
    (void*)Default_Handler,
    (void*)I2C1_EV_IRQHandler,      /* 31: I2C1 Event */
    (void*)I2C1_ER_IRQHandler,      /* 32: I2C1 Error */
    (void*)I2C2_EV_IRQHandler,      /* 33: I2C2 Event */
    (void*)I2C2_ER_IRQHandler,      /* 34: I2C2 Error */
    /* ... Additional interrupts as needed ... */
    /* Fill remaining with Default_Handler up to 150 */
};

/* SCB CPACR register address - not defined in minimal header */
#ifndef SCB_CPACR
#define SCB_CPACR (*(volatile uint32_t*)(SCB_BASE + 0x88))
#endif

/* Override SCB structure access for CPACR */
namespace {
    volatile uint32_t& CPACR = *(volatile uint32_t*)(0xE000ED88UL);
}

/* Re-implement Reset_Handler to properly set FPU */
__attribute__((naked)) void Reset_Handler_ASM(void) {
    __asm volatile (
        /* Set stack pointer */
        "ldr r0, =_estack\n"
        "mov sp, r0\n"

        /* Enable FPU */
        "ldr r0, =0xE000ED88\n"     /* CPACR address */
        "ldr r1, [r0]\n"
        "orr r1, r1, #(0xF << 20)\n" /* Enable CP10 and CP11 */
        "str r1, [r0]\n"
        "dsb\n"
        "isb\n"

        /* Jump to C Reset_Handler */
        "b Reset_Handler\n"
    );
}
