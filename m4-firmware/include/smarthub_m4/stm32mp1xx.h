/**
 * STM32MP1xx Cortex-M4 Register Definitions
 *
 * Minimal register definitions for SmartHub M4 firmware.
 * Based on STM32MP157 Reference Manual (RM0436).
 */
#ifndef STM32MP1XX_H
#define STM32MP1XX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Core Peripheral Base Addresses
 * ============================================================================ */

#define PERIPH_BASE         0x40000000UL
#define APB1_PERIPH_BASE    PERIPH_BASE
#define APB2_PERIPH_BASE    (PERIPH_BASE + 0x00010000UL)
#define AHB2_PERIPH_BASE    (PERIPH_BASE + 0x08000000UL)
#define AHB4_PERIPH_BASE    (PERIPH_BASE + 0x10000000UL)

/* GPIO */
#define GPIOA_BASE          (AHB4_PERIPH_BASE + 0x2000UL)
#define GPIOB_BASE          (AHB4_PERIPH_BASE + 0x3000UL)
#define GPIOC_BASE          (AHB4_PERIPH_BASE + 0x4000UL)
#define GPIOD_BASE          (AHB4_PERIPH_BASE + 0x5000UL)
#define GPIOE_BASE          (AHB4_PERIPH_BASE + 0x6000UL)
#define GPIOF_BASE          (AHB4_PERIPH_BASE + 0x7000UL)
#define GPIOG_BASE          (AHB4_PERIPH_BASE + 0x8000UL)
#define GPIOH_BASE          (AHB4_PERIPH_BASE + 0x9000UL)
#define GPIOI_BASE          (AHB4_PERIPH_BASE + 0xA000UL)

/* I2C */
#define I2C1_BASE           (APB1_PERIPH_BASE + 0x5400UL)
#define I2C2_BASE           (APB1_PERIPH_BASE + 0x5800UL)
#define I2C3_BASE           (APB1_PERIPH_BASE + 0x5C00UL)
#define I2C4_BASE           (APB1_PERIPH_BASE + 0x8400UL)
#define I2C5_BASE           (APB1_PERIPH_BASE + 0xD000UL)

/* USART/UART */
#define USART2_BASE         (APB1_PERIPH_BASE + 0x4400UL)
#define USART3_BASE         (APB1_PERIPH_BASE + 0x4800UL)
#define UART4_BASE          (APB1_PERIPH_BASE + 0x4C00UL)
#define UART5_BASE          (APB1_PERIPH_BASE + 0x5000UL)
#define UART7_BASE          (APB1_PERIPH_BASE + 0x8000UL)
#define UART8_BASE          (APB1_PERIPH_BASE + 0x8400UL)

/* RCC */
#define RCC_BASE            (AHB4_PERIPH_BASE + 0x0000UL)

/* EXTI */
#define EXTI_BASE           (APB2_PERIPH_BASE + 0x0400UL)

/* IPCC (Inter-Processor Communication Controller) - at 0x4C001000 */
#define IPCC_BASE           0x4C001000UL

/* HSEM (Hardware Semaphore) */
#define HSEM_BASE           (AHB4_PERIPH_BASE + 0x0B000UL)

/* ============================================================================
 * GPIO Register Structure
 * ============================================================================ */

typedef struct {
    volatile uint32_t MODER;    /* Mode register */
    volatile uint32_t OTYPER;   /* Output type register */
    volatile uint32_t OSPEEDR;  /* Output speed register */
    volatile uint32_t PUPDR;    /* Pull-up/pull-down register */
    volatile uint32_t IDR;      /* Input data register */
    volatile uint32_t ODR;      /* Output data register */
    volatile uint32_t BSRR;     /* Bit set/reset register */
    volatile uint32_t LCKR;     /* Lock register */
    volatile uint32_t AFR[2];   /* Alternate function registers */
    volatile uint32_t BRR;      /* Bit reset register */
} GPIO_TypeDef;

#define GPIOA   ((GPIO_TypeDef *)GPIOA_BASE)
#define GPIOB   ((GPIO_TypeDef *)GPIOB_BASE)
#define GPIOC   ((GPIO_TypeDef *)GPIOC_BASE)
#define GPIOD   ((GPIO_TypeDef *)GPIOD_BASE)
#define GPIOE   ((GPIO_TypeDef *)GPIOE_BASE)
#define GPIOF   ((GPIO_TypeDef *)GPIOF_BASE)
#define GPIOG   ((GPIO_TypeDef *)GPIOG_BASE)
#define GPIOH   ((GPIO_TypeDef *)GPIOH_BASE)
#define GPIOI   ((GPIO_TypeDef *)GPIOI_BASE)

/* GPIO Mode */
#define GPIO_MODE_INPUT     0x00
#define GPIO_MODE_OUTPUT    0x01
#define GPIO_MODE_AF        0x02
#define GPIO_MODE_ANALOG    0x03

/* GPIO Output Type */
#define GPIO_OTYPE_PP       0x00  /* Push-pull */
#define GPIO_OTYPE_OD       0x01  /* Open-drain */

/* GPIO Speed */
#define GPIO_SPEED_LOW      0x00
#define GPIO_SPEED_MEDIUM   0x01
#define GPIO_SPEED_HIGH     0x02
#define GPIO_SPEED_VHIGH    0x03

/* GPIO Pull */
#define GPIO_PUPD_NONE      0x00
#define GPIO_PUPD_UP        0x01
#define GPIO_PUPD_DOWN      0x02

/* ============================================================================
 * I2C Register Structure
 * ============================================================================ */

typedef struct {
    volatile uint32_t CR1;      /* Control register 1 */
    volatile uint32_t CR2;      /* Control register 2 */
    volatile uint32_t OAR1;     /* Own address register 1 */
    volatile uint32_t OAR2;     /* Own address register 2 */
    volatile uint32_t TIMINGR;  /* Timing register */
    volatile uint32_t TIMEOUTR; /* Timeout register */
    volatile uint32_t ISR;      /* Interrupt and status register */
    volatile uint32_t ICR;      /* Interrupt clear register */
    volatile uint32_t PECR;     /* PEC register */
    volatile uint32_t RXDR;     /* Receive data register */
    volatile uint32_t TXDR;     /* Transmit data register */
} I2C_TypeDef;

#define I2C1    ((I2C_TypeDef *)I2C1_BASE)
#define I2C2    ((I2C_TypeDef *)I2C2_BASE)
#define I2C3    ((I2C_TypeDef *)I2C3_BASE)
#define I2C4    ((I2C_TypeDef *)I2C4_BASE)
#define I2C5    ((I2C_TypeDef *)I2C5_BASE)

/* I2C CR1 bits */
#define I2C_CR1_PE          (1 << 0)    /* Peripheral enable */
#define I2C_CR1_TXIE        (1 << 1)    /* TX interrupt enable */
#define I2C_CR1_RXIE        (1 << 2)    /* RX interrupt enable */
#define I2C_CR1_STOPIE      (1 << 5)    /* STOP interrupt enable */
#define I2C_CR1_NACKIE      (1 << 4)    /* NACK interrupt enable */

/* I2C CR2 bits */
#define I2C_CR2_START       (1 << 13)   /* Start generation */
#define I2C_CR2_STOP        (1 << 14)   /* Stop generation */
#define I2C_CR2_NACK        (1 << 15)   /* NACK generation */
#define I2C_CR2_NBYTES_POS  16          /* Number of bytes position */
#define I2C_CR2_RELOAD      (1 << 24)   /* NBYTES reload mode */
#define I2C_CR2_AUTOEND     (1 << 25)   /* Automatic end mode */
#define I2C_CR2_RD_WRN      (1 << 10)   /* Transfer direction (0=write, 1=read) */

/* I2C ISR bits */
#define I2C_ISR_TXE         (1 << 0)    /* Transmit data register empty */
#define I2C_ISR_TXIS        (1 << 1)    /* Transmit interrupt status */
#define I2C_ISR_RXNE        (1 << 2)    /* Receive data register not empty */
#define I2C_ISR_STOPF       (1 << 5)    /* Stop detection flag */
#define I2C_ISR_TC          (1 << 6)    /* Transfer complete */
#define I2C_ISR_TCR         (1 << 7)    /* Transfer complete reload */
#define I2C_ISR_NACKF       (1 << 4)    /* Not acknowledge received */
#define I2C_ISR_BUSY        (1 << 15)   /* Bus busy */

/* I2C ICR bits */
#define I2C_ICR_STOPCF      (1 << 5)    /* Stop detection flag clear */
#define I2C_ICR_NACKCF      (1 << 4)    /* Not acknowledge flag clear */

/* ============================================================================
 * IPCC Register Structure (Inter-Processor Communication)
 * ============================================================================ */

typedef struct {
    volatile uint32_t C1CR;     /* CPU1 control register */
    volatile uint32_t C1MR;     /* CPU1 mask register */
    volatile uint32_t C1SCR;    /* CPU1 status set/clear register */
    volatile uint32_t C1TOC2SR; /* CPU1 to CPU2 status register */
    volatile uint32_t C2CR;     /* CPU2 control register */
    volatile uint32_t C2MR;     /* CPU2 mask register */
    volatile uint32_t C2SCR;    /* CPU2 status set/clear register */
    volatile uint32_t C2TOC1SR; /* CPU2 to CPU1 status register */
} IPCC_TypeDef;

#define IPCC    ((IPCC_TypeDef *)IPCC_BASE)

/* IPCC bits */
#define IPCC_C1CR_RXOIE     (1 << 0)    /* Receive occupied interrupt enable */
#define IPCC_C1CR_TXFIE     (1 << 16)   /* Transmit free interrupt enable */
#define IPCC_C2CR_RXOIE     (1 << 0)
#define IPCC_C2CR_TXFIE     (1 << 16)

/* ============================================================================
 * RCC Register Structure (Reset and Clock Control)
 * ============================================================================ */

typedef struct {
    volatile uint32_t RESERVED0[220];   /* Skip to M4 relevant registers */
    volatile uint32_t MC_AHB4ENSETR;    /* AHB4 peripheral clock enable set */
    volatile uint32_t MC_AHB4ENCLRR;    /* AHB4 peripheral clock enable clear */
    volatile uint32_t RESERVED1[2];
    volatile uint32_t MC_APB1ENSETR;    /* APB1 peripheral clock enable set */
    volatile uint32_t MC_APB1ENCLRR;    /* APB1 peripheral clock enable clear */
    volatile uint32_t RESERVED2[2];
    volatile uint32_t MC_APB2ENSETR;    /* APB2 peripheral clock enable set */
    volatile uint32_t MC_APB2ENCLRR;    /* APB2 peripheral clock enable clear */
} RCC_TypeDef;

#define RCC     ((RCC_TypeDef *)RCC_BASE)

/* RCC AHB4 enable bits */
#define RCC_AHB4_GPIOAEN    (1 << 0)
#define RCC_AHB4_GPIOBEN    (1 << 1)
#define RCC_AHB4_GPIOCEN    (1 << 2)
#define RCC_AHB4_GPIODEN    (1 << 3)
#define RCC_AHB4_GPIOEEN    (1 << 4)
#define RCC_AHB4_GPIOFEN    (1 << 5)
#define RCC_AHB4_GPIOGEN    (1 << 6)
#define RCC_AHB4_GPIOHEN    (1 << 7)
#define RCC_AHB4_GPIOIEN    (1 << 8)

/* RCC APB1 enable bits */
#define RCC_APB1_I2C1EN     (1 << 21)
#define RCC_APB1_I2C2EN     (1 << 22)
#define RCC_APB1_I2C3EN     (1 << 23)
#define RCC_APB1_I2C5EN     (1 << 24)

/* ============================================================================
 * Cortex-M4 Core Registers
 * ============================================================================ */

#define SCS_BASE            (0xE000E000UL)
#define NVIC_BASE           (SCS_BASE + 0x100UL)
#define SCB_BASE            (SCS_BASE + 0xD00UL)
#define SYSTICK_BASE        (SCS_BASE + 0x010UL)

typedef struct {
    volatile uint32_t ISER[8];      /* Interrupt Set Enable */
    uint32_t RESERVED0[24];
    volatile uint32_t ICER[8];      /* Interrupt Clear Enable */
    uint32_t RESERVED1[24];
    volatile uint32_t ISPR[8];      /* Interrupt Set Pending */
    uint32_t RESERVED2[24];
    volatile uint32_t ICPR[8];      /* Interrupt Clear Pending */
    uint32_t RESERVED3[24];
    volatile uint32_t IABR[8];      /* Interrupt Active Bit */
    uint32_t RESERVED4[56];
    volatile uint8_t IP[240];       /* Interrupt Priority */
    uint32_t RESERVED5[644];
    volatile uint32_t STIR;         /* Software Trigger Interrupt */
} NVIC_TypeDef;

typedef struct {
    volatile uint32_t CTRL;         /* SysTick Control and Status */
    volatile uint32_t LOAD;         /* SysTick Reload Value */
    volatile uint32_t VAL;          /* SysTick Current Value */
    volatile uint32_t CALIB;        /* SysTick Calibration */
} SysTick_TypeDef;

typedef struct {
    volatile uint32_t CPUID;        /* CPUID Base Register */
    volatile uint32_t ICSR;         /* Interrupt Control and State */
    volatile uint32_t VTOR;         /* Vector Table Offset */
    volatile uint32_t AIRCR;        /* Application Interrupt and Reset Control */
    volatile uint32_t SCR;          /* System Control Register */
    volatile uint32_t CCR;          /* Configuration Control Register */
    volatile uint8_t SHP[12];       /* System Handlers Priority */
    volatile uint32_t SHCSR;        /* System Handler Control and State */
    volatile uint32_t CFSR;         /* Configurable Fault Status */
    volatile uint32_t HFSR;         /* HardFault Status */
    volatile uint32_t DFSR;         /* Debug Fault Status */
    volatile uint32_t MMFAR;        /* MemManage Fault Address */
    volatile uint32_t BFAR;         /* BusFault Address */
    volatile uint32_t AFSR;         /* Auxiliary Fault Status */
    uint32_t RESERVED0[18];
    volatile uint32_t CPACR;        /* Coprocessor Access Control */
} SCB_TypeDef;

#define NVIC    ((NVIC_TypeDef *)NVIC_BASE)
#define SCB     ((SCB_TypeDef *)SCB_BASE)
#define SysTick ((SysTick_TypeDef *)SYSTICK_BASE)

/* SysTick Control bits */
#define SYSTICK_CTRL_ENABLE     (1 << 0)
#define SYSTICK_CTRL_TICKINT    (1 << 1)
#define SYSTICK_CTRL_CLKSOURCE  (1 << 2)
#define SYSTICK_CTRL_COUNTFLAG  (1 << 16)

/* ============================================================================
 * Interrupt Numbers
 * ============================================================================ */

typedef enum {
    /* Cortex-M4 Processor Exceptions */
    NonMaskableInt_IRQn     = -14,
    HardFault_IRQn          = -13,
    MemoryManagement_IRQn   = -12,
    BusFault_IRQn           = -11,
    UsageFault_IRQn         = -10,
    SVCall_IRQn             = -5,
    DebugMonitor_IRQn       = -4,
    PendSV_IRQn             = -2,
    SysTick_IRQn            = -1,

    /* STM32MP1 Specific Interrupts */
    WWDG1_IRQn              = 0,
    EXTI0_IRQn              = 6,
    EXTI1_IRQn              = 7,
    EXTI2_IRQn              = 8,
    EXTI3_IRQn              = 9,
    EXTI4_IRQn              = 10,
    I2C1_EV_IRQn            = 31,
    I2C1_ER_IRQn            = 32,
    I2C2_EV_IRQn            = 33,
    I2C2_ER_IRQn            = 34,
    I2C3_EV_IRQn            = 72,
    I2C3_ER_IRQn            = 73,
    I2C5_EV_IRQn            = 107,
    I2C5_ER_IRQn            = 108,
    IPCC_RX0_IRQn           = 103,
    IPCC_TX0_IRQn           = 104,
} IRQn_Type;

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static inline void __enable_irq(void) {
    __asm volatile ("cpsie i" ::: "memory");
}

static inline void __disable_irq(void) {
    __asm volatile ("cpsid i" ::: "memory");
}

static inline void __WFI(void) {
    __asm volatile ("wfi");
}

static inline void __DSB(void) {
    __asm volatile ("dsb 0xF" ::: "memory");
}

static inline void __ISB(void) {
    __asm volatile ("isb 0xF" ::: "memory");
}

static inline void NVIC_EnableIRQ(IRQn_Type IRQn) {
    if ((int32_t)IRQn >= 0) {
        NVIC->ISER[(uint32_t)IRQn >> 5] = (1UL << ((uint32_t)IRQn & 0x1FUL));
    }
}

static inline void NVIC_DisableIRQ(IRQn_Type IRQn) {
    if ((int32_t)IRQn >= 0) {
        NVIC->ICER[(uint32_t)IRQn >> 5] = (1UL << ((uint32_t)IRQn & 0x1FUL));
    }
}

static inline void NVIC_SetPriority(IRQn_Type IRQn, uint32_t priority) {
    if ((int32_t)IRQn >= 0) {
        NVIC->IP[(uint32_t)IRQn] = (uint8_t)((priority << 4) & 0xFFUL);
    } else {
        SCB->SHP[(((uint32_t)IRQn) & 0xFUL) - 4UL] = (uint8_t)((priority << 4) & 0xFFUL);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* STM32MP1XX_H */
