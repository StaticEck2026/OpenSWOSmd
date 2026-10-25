/* md_libc.c -- the C library functions the Mega Drive build links against.
 *
 * The cartridge has no OS and no libc; the shared engine needs the string and memory
 * functions, and a few hosted ones the parts linked here reach (snprintf for the HUD).
 * Nothing allocates: there is no heap on the Mega Drive build.
 */
#include <stdarg.h>
#include <stdint.h>
#include "md_hw.h"

typedef unsigned long md_size_t;

void *memcpy(void *dst, const void *src, md_size_t n)
{
    uint8_t *d = dst; const uint8_t *s = src;
    if ((((uint32_t)d | (uint32_t)s) & 1) == 0) {
        uint32_t *dl = (uint32_t *)d; const uint32_t *sl = (const uint32_t *)s;
        while (n >= 16) { dl[0] = sl[0]; dl[1] = sl[1]; dl[2] = sl[2]; dl[3] = sl[3]; dl += 4; sl += 4; n -= 16; }
        while (n >= 4) { *dl++ = *sl++; n -= 4; }
        d = (uint8_t *)dl; s = (const uint8_t *)sl;
    }
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, md_size_t n)
{
    uint8_t *d = dst; const uint8_t *s = src;
    if (d == s || !n) return dst;
    if (d < s || d >= s + n) return memcpy(dst, src, n);
    d += n; s += n;
    while (n--) *--d = *--s;
    return dst;
}

void *memset(void *dst, int c, md_size_t n)
{
    uint8_t *d = dst;
    uint8_t v = (uint8_t)c;
    if (!((uint32_t)d & 1) && n >= 8) {
        uint32_t w = v * 0x01010101u;
        uint32_t *dl = (uint32_t *)d;
        while (n >= 4) { *dl++ = w; n -= 4; }
        d = (uint8_t *)dl;
    }
    while (n--) *d++ = v;
    return dst;
}

int memcmp(const void *a, const void *b, md_size_t n)
{
    const uint8_t *p = a, *q = b;
    for (; n; n--, p++, q++) if (*p != *q) return *p - *q;
    return 0;
}

md_size_t strlen(const char *s) { const char *p = s; while (*p) p++; return (md_size_t)(p - s); }
int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}
int strncmp(const char *a, const char *b, md_size_t n)
{
    for (; n; n--, a++, b++) { if (*a != *b) return (uint8_t)*a - (uint8_t)*b; if (!*a) return 0; }
    return 0;
}
char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)) {} return r; }
char *strncpy(char *d, const char *s, md_size_t n)
{
    char *r = d;
    for (; n && *s; n--) *d++ = *s++;
    for (; n; n--) *d++ = 0;
    return r;
}
char *strcat(char *d, const char *s) { strcpy(d + strlen(d), s); return d; }
char *strchr(const char *s, int c) { for (;; s++) { if (*s == (char)c) return (char *)s; if (!*s) return 0; } }
char *strrchr(const char *s, int c) { const char *r = 0; for (;; s++) { if (*s == (char)c) r = s; if (!*s) return (char *)r; } }
int abs(int v) { return v < 0 ? -v : v; }
int toupper(int c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }
int tolower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

int vsnprintf(char *buf, md_size_t n, const char *fmt, va_list ap) { return md_vsnprintf(buf, (int)n, fmt, ap); }
int snprintf(char *buf, md_size_t n, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    int r = md_vsnprintf(buf, (int)n, fmt, ap);
    va_end(ap);
    return r;
}
int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    int r = md_vsnprintf(buf, 256, fmt, ap);
    va_end(ap);
    return r;
}
