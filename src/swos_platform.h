/*
 * swos_platform.h - Platform compatibility shim for SDL build
 *
 * Force-included via -include in CFLAGS so it precedes every translation unit.
 * Replaces Amiga-specific section attributes with no-ops for x86/x64 Linux/Windows
 * SDL builds.
 */

#ifndef SWOS_PLATFORM_H
#define SWOS_PLATFORM_H

/* The SDL build is hosted on x86/x64 with little-endian ordering. */
#define SWOS_SDL_BUILD 1

/* On Win32 PE-COFF the C compiler prepends '_' to every C global symbol
 * at the assembler level (so a C `foo` becomes `_foo` in the .o file).
 * ELF (Linux/macOS) does not. Many files alias buffers via inline asm
 * (`.globl name; .equ name, base + offset`) which by default emits the
 * unprefixed name — that links on Linux but not on Win32. Use _ASM_PFX
 * inside __asm__ string literals to prefix the symbol name correctly.
 *
 *   __asm__(".globl " _ASM_PFX "foo\n"
 *           ".equ "  _ASM_PFX "foo, " _ASM_PFX "base + 42\n");
 *
 * GCC's __USER_LABEL_PREFIX__ is "_" on platforms that prefix and empty
 * elsewhere — stringify it via two-step macro expansion. */
#define _SWOS_ASM_STR1(x) #x
#define _SWOS_ASM_STR(x)  _SWOS_ASM_STR1(x)
#define _ASM_PFX          _SWOS_ASM_STR(__USER_LABEL_PREFIX__)

/* Disable all audio output. The SDL audio device is never opened, the
 * RJP1 player is never loaded, and every AudioSDL_* trigger is a no-op.
 * Useful for isolating audio-related issues during testing. */
#define SWOS_AUDIO_DISABLED 0

/* The Amiga code uses its own typedefs (unsigned char uint8_t, etc.).
 * Make sure stdint.h is NOT pulled in indirectly to prevent typedef
 * conflicts. The Amiga code's typedefs match the SysV ABI on x86/x64. */

/* Override Amiga section macros — section attributes don't make sense on PC. */
#ifdef SWOS_CHIP_BSS
#undef SWOS_CHIP_BSS
#endif
#ifdef SWOS_FAST_BSS
#undef SWOS_FAST_BSS
#endif
#define SWOS_CHIP_BSS
#define SWOS_FAST_BSS

/* Pre-empt swos_sections.h from re-defining them */
#define SWOS_SECTIONS_H

/* The Amiga code's FixedPoint union is { int16_t i; uint16_t frac } which
 * works on big-endian. On little-endian we need to swap the order so that
 * a raw int32_t value of 0x00010000 still maps to i=1, frac=0. */
#define AMIGA_FIXEDPOINT_LE 1

/* ---- Chip-register base redirect -----------------------------------
 * The Amiga code pokes `*(volatile uint16_t *)(CUSTOM_BASE + OFFSET) = x;`
 * for registers that don't have named struct fields (audio regs, etc.).
 * On the Amiga, CUSTOM_BASE = 0x00DFF000 (memory-mapped hardware).
 * On the SDL host that address is unmapped and any poke segfaults.
 *
 * Redirect CUSTOM_BASE to the address of our stub `custom` Custom
 * struct, which has a pad_0A0[112] array that covers the audio
 * register window (0xA0..0x17F). All offset-relative writes now land
 * harmlessly in that buffer.
 *
 * `struct Custom` itself is fully defined in display.h. We only need
 * a forward declaration here so the extern compiles in translation
 * units that include this header before display.h. */
struct Custom;
extern volatile struct Custom *custom;
#define CUSTOM_BASE ((unsigned long)(custom))

#endif /* SWOS_PLATFORM_H */
