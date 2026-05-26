/*
 * rnc_magic.h -- Endian-aware RNC v1/v2 magic constants for direct
 * `*(uint32_t *)buf` comparisons.
 *
 * Files start with the 4 bytes 'R','N','C',0x01 (or 0x02). On the
 * Amiga (m68k, big-endian) a native uint32_t load gives 0x524E4301.
 * On x86 (little-endian) the same four bytes load as 0x01434E52.
 *
 * Code that builds the signature byte-by-byte (rnc.c::v1ReadU32)
 * does NOT need this header -- it always assembles the big-endian
 * value regardless of host order.
 */

#ifndef RNC_MAGIC_H
#define RNC_MAGIC_H

/* Use the standard GCC/Clang predefined byte-order macros instead of
 * the ad-hoc x86/MSVC list — that list missed ARM Linux (which is
 * little-endian by default but defines neither __i386__ nor
 * __x86_64__), so on the armhf cross-build the LE constant fell
 * through to the BE branch and `_LoadRNCToBufferNoBlit` panicked
 * loading sound/hero.ins on every Pi 3 run.
 *
 * MSVC doesn't define __BYTE_ORDER__, so keep an explicit fallback
 * for it. */
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#  define RNC_MAGIC_V1 0x01434E52u
#  define RNC_MAGIC_V2 0x02434E52u
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#  define RNC_MAGIC_V1 0x524E4301u
#  define RNC_MAGIC_V2 0x524E4302u
#elif defined(_MSC_VER) || defined(_M_IX86) || defined(_M_X64)
#  define RNC_MAGIC_V1 0x01434E52u
#  define RNC_MAGIC_V2 0x02434E52u
#else
#  error "rnc_magic.h: cannot determine host byte order; please add a case for your toolchain"
#endif

#endif /* RNC_MAGIC_H */
