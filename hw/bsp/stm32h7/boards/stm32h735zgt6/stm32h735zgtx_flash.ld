/* SPDX-License-Identifier: MIT */

/* STM32H735ZGT6: 1 MiB flash, 560 KiB normal SRAM, 4 KiB backup SRAM. */

ENTRY(Reset_Handler)

_Min_Heap_Size = 0x200;
_Min_Stack_Size = 0x400;

MEMORY
{
  /*
   * Every TCM/AXI option-byte split provides at least 64 KiB ITCM and
   * 128 KiB AXI SRAM. Keep all automatic placement inside that guaranteed
   * AXI window. AXIRAM_EXT describes the additional 192 KiB present with the
   * default 64 KiB ITCM / 320 KiB AXI split, but no section uses it.
   */
  ITCMRAM   (xrw) : ORIGIN = 0x00000000, LENGTH = 64K
  DTCMRAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
  AXIRAM    (xrw) : ORIGIN = 0x24000000, LENGTH = 128K
  AXIRAM_EXT(xrw) : ORIGIN = 0x24020000, LENGTH = 192K
  SRAM1     (xrw) : ORIGIN = 0x30000000, LENGTH = 16K
  SRAM2     (xrw) : ORIGIN = 0x30004000, LENGTH = 16K
  SRAM4     (xrw) : ORIGIN = 0x38000000, LENGTH = 16K
  FLASH     (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
}

_estack = ORIGIN(AXIRAM) + LENGTH(AXIRAM);

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
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  .ARM.extab :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } >FLASH

  .ARM :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .preinit_array :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } >FLASH

  .init_array :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } >FLASH

  .fini_array :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } >FLASH

  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    *(.RamFunc)
    *(.RamFunc*)
    . = ALIGN(4);
    _edata = .;
  } >AXIRAM AT>FLASH

  .bss (NOLOAD) :
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
  } >AXIRAM

  /* SuperDFU reset handoff state must survive C runtime initialization. */
  .dfuram (NOLOAD) :
  {
    . = ALIGN(4);
    *(.dfuram)
    . = ALIGN(4);
  } >AXIRAM

  ._user_heap_stack (NOLOAD) :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >AXIRAM

  /DISCARD/ :
  {
    libc.a(*)
    libm.a(*)
    libgcc.a(*)
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
