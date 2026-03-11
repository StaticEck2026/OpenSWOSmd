/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef SWOS_SECTIONS_H
#define SWOS_SECTIONS_H

#define SWOS_CHIP_BSS __attribute__((section("BSSInChip.MEMF_CHIP"), aligned(4)))
#define SWOS_FAST_BSS __attribute__((section("bss"), aligned(4)))

#endif
