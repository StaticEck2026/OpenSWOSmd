#ifndef SWOS_SECTIONS_H
#define SWOS_SECTIONS_H

#define SWOS_CHIP_BSS __attribute__((section("BSSInChip.MEMF_CHIP"), aligned(4)))
#define SWOS_FAST_BSS __attribute__((section("bss"), aligned(4)))

#endif
