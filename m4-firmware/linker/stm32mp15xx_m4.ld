/**
 * Linker script for STM32MP15xx Cortex-M4
 *
 * Memory map for M4 core:
 * - RETRAM: 64KB @ 0x00000000 (aliased to 0x38000000)
 * - MCUSRAM: 256KB @ 0x10000000 (can be extended to 384KB)
 *
 * Note: Actual memory addresses depend on device tree configuration
 * and resource allocation between A7 and M4 cores.
 */

/* Entry point */
ENTRY(Reset_Handler)

/* Memory regions */
MEMORY
{
    /* M4 RETRAM - typically used for vector table and critical code */
    RETRAM (xrw) : ORIGIN = 0x00000000, LENGTH = 64K

    /* M4 SRAM - main code and data memory */
    MCUSRAM (xrw) : ORIGIN = 0x10000000, LENGTH = 256K

    /* Shared memory for OpenAMP/RPMsg (allocated by Linux) */
    /* VRING (rw) : ORIGIN = 0x10040000, LENGTH = 64K */
}

/* Stack and heap sizes */
_Min_Heap_Size = 0x1000;   /* 4KB heap */
_Min_Stack_Size = 0x1000;  /* 4KB stack */

SECTIONS
{
    /* Vector table - must be at start of memory */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >MCUSRAM

    /* Program code */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)

        KEEP(*(.init))
        KEEP(*(.fini))

        . = ALIGN(4);
        _etext = .;
    } >MCUSRAM

    /* Read-only data */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } >MCUSRAM

    /* ARM exception handling */
    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } >MCUSRAM

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } >MCUSRAM

    /* Constructors */
    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } >MCUSRAM

    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } >MCUSRAM

    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } >MCUSRAM

    /* Used by startup code to initialize data */
    _sidata = LOADADDR(.data);

    /* Initialized data */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >MCUSRAM

    /* Uninitialized data (zeroed by startup) */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } >MCUSRAM

    /* Heap */
    ._user_heap :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        . = . + _Min_Heap_Size;
        . = ALIGN(8);
    } >MCUSRAM

    /* Stack */
    ._user_stack :
    {
        . = ALIGN(8);
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
        _estack = .;
    } >MCUSRAM

    /* Remove debug info from output */
    /DISCARD/ :
    {
        libc.a(*)
        libm.a(*)
        libgcc.a(*)
    }

    /* ARM attributes */
    .ARM.attributes 0 : { *(.ARM.attributes) }
}
