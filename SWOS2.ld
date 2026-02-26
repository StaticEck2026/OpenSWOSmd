ENTRY(main)

SECTIONS
{
  text 0x00100000 : {
        *(.text.startup.main) *(.text.main) *(.text.startup.*) *(text) *(TEXT) *(.text) *(.text.*) *(code) *(CODE) *(.rodata) *(.rodata.*) *(const)
  }

  data : {
        *(data) *(DATA) *(.data) *(.data.*) *(.rodata) *(.rodata.*) *(const)
  }

  BSSInChip 0x00010000 (NOLOAD) : {
        *(BSSInChip) *(BSSInChip*)
  }

  . = ALIGN(4);
  bss (NOLOAD) : {
        *(bss) *(BSS) *(.bss) *(.bss.*) *(COMMON)
  }
}