/*
 * STM32H743xI application image (bootloader occupies bank 1 sector 0).
 * Symbol names match ST's startup_stm32h743xx.s (_sidata/_sdata/_edata/_sbss/_ebss/_estack).
 */
ENTRY(Reset_Handler)

_estack = ORIGIN(DTCMRAM) + LENGTH(DTCMRAM);
_Min_Heap_Size  = 0x1000;  /* newlib malloc: should stay unused */
_Min_Stack_Size = 0x1000;  /* main/ISR stack (MSP) */

MEMORY
{
  ITCMRAM (xrw) : ORIGIN = 0x00000000, LENGTH = 64K
  DTCMRAM (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
  FLASH   (rx)  : ORIGIN = 0x08020000, LENGTH = 1664K   /* B1S1..B2S5; B2S6/S7 = nvstore */
  RAM_D1  (xrw) : ORIGIN = 0x24000000, LENGTH = 512K    /* AXI SRAM */
  RAM_D2  (xrw) : ORIGIN = 0x30000000, LENGTH = 288K    /* SRAM1-3, non-cacheable via MPU */
  RAM_D3  (xrw) : ORIGIN = 0x38000000, LENGTH = 64K     /* SRAM4 */
  SDRAM   (rw)  : ORIGIN = 0xC0200000, LENGTH = 30M     /* first 2 MB = framebuffers */
  QSPI    (r)   : ORIGIN = 0x90000000, LENGTH = 16M     /* GUI asset partition */
}

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text) *(.text*) *(.glue_7) *(.glue_7t) *(.eh_frame)
    KEEP(*(.init)) KEEP(*(.fini))
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata) *(.rodata*)
    /* TouchGFX fonts and texts stay in internal flash: QSPI is not mapped at runtime */
    *(FontFlashSection FontFlashSection.*)
    *(TextFlashSection TextFlashSection.*)
    . = ALIGN(4);
  } >FLASH

  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
  .ARM : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } >FLASH

  .preinit_array : { PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*)) PROVIDE_HIDDEN(__preinit_array_end = .); } >FLASH
  .init_array    : { PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*)) PROVIDE_HIDDEN(__init_array_end = .); } >FLASH
  .fini_array    : { PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*)) PROVIDE_HIDDEN(__fini_array_end = .); } >FLASH

  _sidata = LOADADDR(.data);

  /* CPU-only data in DTCM: zero wait state, not reachable by DMA1/2 */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data) *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >DTCMRAM AT> FLASH

  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss) *(.bss*) *(COMMON) *(.dtcm_bss)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >DTCMRAM

  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >DTCMRAM

  /* FreeRTOS heap (task stacks, queues) in AXI SRAM */
  .freertos_heap (NOLOAD) :
  {
    . = ALIGN(8);
    *(.freertos_heap)
  } >RAM_D1

  /* DMA buffers: D2 SRAM, MPU non-cacheable. Zeroed by the port when needed. */
  .dma_bss (NOLOAD) :
  {
    . = ALIGN(32);
    *(.dma_bss) *(.dma_bss*)
  } >RAM_D2

  /* External SDRAM: NOLOAD, zeroed by pal_sys_init() after the FMC is up */
  .sdram_bss (NOLOAD) :
  {
    . = ALIGN(32);
    _ssdram_bss = .;
    *(.sdram_bss) *(.sdram_bss*)
    . = ALIGN(32);
    _esdram_bss = .;
  } >SDRAM

  /* TouchGFX bitmaps: programmed through the CubeProgrammer external loader */
  ExtFlashSection :
  {
    *(ExtFlashSection ExtFlashSection.*)
  } >QSPI

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
