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

#if defined(__i386__) || defined(__x86_64__) || defined(__LITTLE_ENDIAN__) || \
    defined(_M_IX86) || defined(_M_X64) || defined(_MSC_VER)
#  define RNC_MAGIC_V1 0x01434E52u
#  define RNC_MAGIC_V2 0x02434E52u
#else
#  define RNC_MAGIC_V1 0x524E4301u
#  define RNC_MAGIC_V2 0x524E4302u
#endif

#endif /* RNC_MAGIC_H */
