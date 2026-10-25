/* OpenSWOS Mega Drive memory map.
 *
 *   0x000000  ROM: vectors, header (0x100), code, read-only data, the .data image
 *   0xFF0000  work RAM, 64 KB: the debug log ring first (a fixed address the harness
 *             reads, scripts/md/mdrun.c --log 0xFF0000), then .data, .bss, the stack
 *             growing down from the top.
 *
 * Read-only tables the shared engine declares without `const` (animation tables,
 * formations, ...) are moved to ROM by name in the .rodata rule below when the engine
 * never writes them -- see docs/MEGADRIVE.md "RAM". Everything else non-const lands in
 * RAM, as it must.
 */
OUTPUT_FORMAT("elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(_md_start)

MEMORY
{
    rom (rx) : ORIGIN = 0x000000, LENGTH = 4M
    ram (rw) : ORIGIN = 0xFF0000, LENGTH = 64K
}

__stack_top = 0x01000000;   /* first push lands at 0xFFFFFC */

SECTIONS
{
    .vectors 0x000000 : { KEEP(*(.vectors)) } > rom
    .header  0x000100 : { KEEP(*(.header)) } > rom

    .text 0x000200 : {
        *(.text.md_hot .text.md_hot.*)
        *(.text .text.*)
    } > rom

    .rodata : {
        *(.rodata .rodata.*)
        INCLUDE build/md/md_romdata.ld
        . = ALIGN(2);
    } > rom

    .debuglog (NOLOAD) : { *(.md_debuglog) . = ALIGN(4); } > ram

    .data : {
        __data_start = .;
        *(.data .data.*)
        . = ALIGN(4);
        __data_end = .;
    } > ram AT > rom
    __data_load = LOADADDR(.data);

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > ram

    /* Leave room for the stack: the link fails if RAM use leaves less than this. */
    __stack_reserve = 0xC00;
    ASSERT(__bss_end + __stack_reserve <= 0x01000000, "work RAM overflow: .data + .bss leave no room for the stack")

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
