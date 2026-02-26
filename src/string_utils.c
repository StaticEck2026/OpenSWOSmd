#include "string_utils.h"

/* ================================================================
 * GetStringLength  (SWOS2.s line 40972)
 *
 * Returns the length of a null-terminated string.
 *
 * Asm register convention:
 *   in:  a0 = string pointer
 *   out: d0.w = length, a0 = pointer to null terminator
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _GetStringLength(const uint8_t *str)
{
    uint16_t len = 0;
    while (str[len] != 0)
        len++;
    return len;
}

/* ================================================================
 * StringEquals  (SWOS2.s line 40997)
 *
 * Compares two null-terminated strings.
 * Returns 0 if equal (matching asm Z flag set convention),
 * non-zero if different.
 *
 * Asm register convention:
 *   in:  a0 = s1, a1 = s2
 *   out: Z flag (set if equal)
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _StringEquals(const uint8_t *s1, const uint8_t *s2)
{
    uint8_t c;
    for (;;) {
        c = *s1++;
        if (c == 0) {
            /* s1 ended — equal only if s2 also at null */
            return (int16_t)*s2;
        }
        if (c != *s2++)
            return 1; /* not equal */
    }
}

/* ================================================================
 * StringCopy  (SWOS2.s line 41015)
 *
 * Copies src to dst including null terminator.
 * Returns pointer to the null terminator in dst (allows chaining).
 *
 * Asm register convention:
 *   in:  a0 = src, a1 = dst
 *   out: a1 = pointer to null terminator in dst
 *        a0 = one byte past the null terminator in src
 * ================================================================ */
__attribute__((used, externally_visible))
uint8_t *_StringCopy(const uint8_t *src, uint8_t *dst)
{
    while ((*dst = *src) != 0) {
        src++;
        dst++;
    }
    return dst; /* points to null terminator */
}

/* ================================================================
 * StringCopyUntilChar  (SWOS2.s line 41028)
 *
 * Copies src to dst until stopChar or end of string.
 * Returns pointer to null terminator in dst.
 *
 * Asm register convention:
 *   in:  a0 = src, a1 = dst, d0.b = stop char
 *   out: a1 = pointer to null terminator in dst
 *        a0 = if stop char found: one before the stop char
 *             if null found: one past the null
 *
 * Note: the asm stub handles the a0 update via the second
 * return value in d1 (srcEnd position).
 * ================================================================ */
__attribute__((used, externally_visible))
uint8_t *_StringCopyUntilChar(const uint8_t *src, uint8_t *dst, uint8_t stopChar)
{
    for (;;) {
        if (*src == stopChar) {
            *dst = 0;
            return dst;
        }
        *dst = *src;
        if (*src == 0) {
            return dst;  /* dst points to the null written */
        }
        src++;
        dst++;
    }
}
