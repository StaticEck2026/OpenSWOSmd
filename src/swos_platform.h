/*
 * swos_platform.h - Platform compatibility shim for SDL build
 *
 * Force-included via -include in CFLAGS so it precedes every translation unit.
 * Replaces Amiga-specific section attributes with no-ops for x86/x64 Linux/Windows
 * SDL builds.
 */

#ifndef SWOS_PLATFORM_H
#define SWOS_PLATFORM_H

/* When the Amiga m68k toolchain compiles a file that pulls this
 * header in transitively (one of the synced data files does), give
 * it an empty translation unit — the SDL_BUILD-only defines below
 * must NOT be visible on Amiga. The SDL build still injects this
 * header via -include in CFLAGS as before. */
#if defined(__m68k__) || defined(__amigaos__) || defined(__amigaos4__)
/* Amiga build: no platform shim needed. The Amiga m68k-elf toolchain
 * doesn't prefix symbols (ELF on m68k uses bare names), so _ASM_PFX
 * expands to an empty string. The synced data files (swos_globals.c
 * etc.) use this macro inside .globl/.equ inline-asm declarations and
 * need it defined on both ports. */
#define _ASM_PFX ""
#else

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

/* SWOS_PACKED — gated packed-struct attribute.
 *
 * Most SWOS structs faithfully mirror Amiga's 2-byte struct alignment
 * via `__attribute__((packed))`. That works fine for ELF (Linux,
 * Amiga m68k) on every architecture we test, but Mach-O's static
 * linker (ld64) on arm64 / x86_64 rejects packed structs that put
 * pointer fields at non-8-byte-aligned offsets — it can't emit the
 * relocation. Building the macOS universal binary fails with errors
 * like "pointer not aligned in '_euroCup'+0x2E".
 *
 * To unblock macOS, expand SWOS_PACKED to nothing on Apple targets;
 * the compiler then pads to natural alignment, which costs a few
 * bytes per struct but resolves all the pointer-alignment failures.
 * On Amiga / Linux it keeps the original packed layout — those
 * targets continue to assume Amiga-compatible struct sizes.
 *
 * Static size-check asserts that pin specific struct sizes (e.g.
 * Sprite, SpriteGraphics) are gated on __APPLE__ in addition to the
 * existing UINTPTR_MAX bitness check.  */
#if defined(__APPLE__)
#define SWOS_PACKED
#define SWOS_PACKED_ALIGN(n)
/* Same reasoning for `#pragma pack(push, N)` blocks (used in menu_data.h /
 * audio.h to box-pack the menu/variant descriptor layouts) — disabled on
 * Apple so pointer fields get natural alignment for ld64 relocations. */
#define SWOS_PACK_PUSH1
#define SWOS_PACK_PUSH2
#define SWOS_PACK_POP
#else
#define SWOS_PACKED              __attribute__((packed))
#define SWOS_PACKED_ALIGN(n)     __attribute__((packed, aligned(n)))
#define SWOS_PACK_PUSH1          _Pragma("pack(push, 1)")
#define SWOS_PACK_PUSH2          _Pragma("pack(push, 2)")
#define SWOS_PACK_POP            _Pragma("pack(pop)")
#endif

/* Disable all audio output. The SDL audio device is never opened, the
 * RJP1 player is never loaded, and every AudioSDL_* trigger is a no-op.
 * Useful for isolating audio-related issues during testing. */
#define SWOS_AUDIO_DISABLED 0

/* The Amiga code uses its own typedefs (unsigned char uint8_t, etc.).
 * Make sure stdint.h is NOT pulled in indirectly to prevent typedef
 * conflicts. The Amiga code's typedefs match the SysV ABI on x86/x64.
 *
 * The Amiga port doesn't see this file, so this typedef is SDL-only.
 * uintptr_t is needed for the bitness-aware pointer slots
 * (MenuEntry.bg/fg, frontbufferBasePtr, *.fg = (uintptr_t)X writes,
 * etc.) introduced during Phase 6. GCC defines __UINTPTR_TYPE__ to
 * the platform's correct typedef — `unsigned int` on 32-bit ILP32,
 * `unsigned long` on 64-bit LP64, `unsigned long long` on Win64
 * LLP64 — which is exactly what we want. */
#ifndef _UINTPTR_T_DEFINED
typedef __UINTPTR_TYPE__ uintptr_t;
#define _UINTPTR_T_DEFINED 1
#endif

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

#endif /* !__m68k__ */

#endif /* SWOS_PLATFORM_H */
