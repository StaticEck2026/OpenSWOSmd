/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* swos_io.c — portable I/O + heap shim. See swos_io.h. */
#include "swos_io.h"
#include <stdarg.h>
#include <string.h>

/* ============================================================
 * Minimal libc fallbacks for Amiga.
 *
 * The m68k-amiga-elf toolchain (bartmanabyss build) ships with a
 * partial libc — memcpy/memset link but strlen/strcmp/memcmp/atoi
 * are not provided. Rather than chase the toolchain config, drop in
 * tiny implementations here. SDL builds use the host libc so these
 * are skipped.
 * ============================================================ */
#if defined(__m68k__) || defined(__amigaos__) || defined(__amigaos4__)

size_t strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/* mjson uses strncmp; -Ofast inlined it as a builtin so it never needed a
 * definition, but -O0 (DBG build) emits a real call. */
int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (n == 0) return 0;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    while (n--) {
        if (*pa != *pb) return (int)*pa - (int)*pb;
        pa++; pb++;
    }
    return 0;
}

int atoi(const char *s) {
    int n = 0, neg = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    return neg ? -n : n;
}

#endif

/* ============================================================
 * Allocation. Both backings store the block size in the first 4
 * bytes; the caller receives a pointer to the byte after that.
 * swos_io_free / swos_io_realloc reverse the offset to recover
 * the size that AllocMem / malloc were given.
 * ============================================================ */

#define SWOS_HDR_SIZE 4u

#ifdef SWOS_SDL_BUILD
#include <stdio.h>
#include <stdlib.h>

void *swos_io_alloc(uint32_t sz)
{
    uint8_t *block = (uint8_t *)malloc((size_t)sz + SWOS_HDR_SIZE);
    if (!block) return (void *)0;
    *(uint32_t *)block = sz + SWOS_HDR_SIZE;
    return block + SWOS_HDR_SIZE;
}

void *swos_io_realloc(void *p, uint32_t new_sz)
{
    if (!p) return swos_io_alloc(new_sz);
    uint8_t *block = (uint8_t *)p - SWOS_HDR_SIZE;
    uint8_t *grew = (uint8_t *)realloc(block, (size_t)new_sz + SWOS_HDR_SIZE);
    if (!grew) return (void *)0;
    *(uint32_t *)grew = new_sz + SWOS_HDR_SIZE;
    return grew + SWOS_HDR_SIZE;
}

void swos_io_free(void *p)
{
    if (!p) return;
    uint8_t *block = (uint8_t *)p - SWOS_HDR_SIZE;
    free(block);
}

#else  /* !SWOS_SDL_BUILD — Amiga */

extern uint8_t *AllocMemAny(uint32_t sz);
extern void     FreeMemAny(void *addr, uint32_t sz);

void *swos_io_alloc(uint32_t sz)
{
    uint8_t *block = AllocMemAny(sz + SWOS_HDR_SIZE);
    if (!block) return (void *)0;
    *(uint32_t *)block = sz + SWOS_HDR_SIZE;
    return block + SWOS_HDR_SIZE;
}

void *swos_io_realloc(void *p, uint32_t new_sz)
{
    if (!p) return swos_io_alloc(new_sz);
    uint8_t *block = (uint8_t *)p - SWOS_HDR_SIZE;
    uint32_t old_size = *(uint32_t *)block;        /* includes header */
    uint32_t old_payload = old_size - SWOS_HDR_SIZE;

    void *out = swos_io_alloc(new_sz);
    if (!out) return (void *)0;

    uint32_t copy_n = (old_payload < new_sz) ? old_payload : new_sz;
    if (copy_n) memcpy(out, p, copy_n);

    FreeMemAny(block, old_size);
    return out;
}

void swos_io_free(void *p)
{
    if (!p) return;
    uint8_t *block = (uint8_t *)p - SWOS_HDR_SIZE;
    uint32_t sz = *(uint32_t *)block;
    FreeMemAny(block, sz);
}

#endif

/* ============================================================
 * Whole-file load/save.
 * ============================================================ */

#ifdef SWOS_SDL_BUILD

static void resolve_save_path(const uint8_t *name, char *out, uint32_t out_sz)
{
    const char *src = (const char *)name;
    const char *colon = strchr(src, ':');
    if (colon) src = colon + 1;
    snprintf(out, out_sz, "save/%s", src);
}

int swos_io_load_alloc(const uint8_t *name, uint8_t **out_buf, uint32_t *out_size)
{
    char path[512];
    resolve_save_path(name, path, sizeof(path));

    FILE *f = fopen(path, "rb");
    if (!f) return 1;

    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return 1; }

    uint8_t *buf = (uint8_t *)swos_io_alloc((uint32_t)n + 1u);
    if (!buf) { fclose(f); return 1; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    if (got != (size_t)n) { swos_io_free(buf); return 1; }
    buf[n] = '\0';

    *out_buf = buf;
    *out_size = (uint32_t)n;
    return 0;
}

int swos_io_save(const uint8_t *name, const uint8_t *buf, uint32_t size)
{
    char path[512];
    resolve_save_path(name, path, sizeof(path));

    FILE *f = fopen(path, "wb");
    if (!f) return 1;
    size_t put = fwrite(buf, 1, size, f);
    fclose(f);
    return (put == size) ? 0 : 1;
}

#else  /* Amiga */

extern uint32_t RobNorthenLoader_C(uint32_t cmd, uint32_t arg1,
                                    uint8_t *name, uint8_t *buf);

#define ROBN_LOAD      0u
#define ROBN_SAVE      1u
#define ROBN_GET_SIZE  5u

int swos_io_load_alloc(const uint8_t *name, uint8_t **out_buf, uint32_t *out_size)
{
    uint32_t sz = RobNorthenLoader_C(ROBN_GET_SIZE, 0, (uint8_t *)name, (uint8_t *)0);
    if (sz == 0) return 1;

    uint8_t *buf = (uint8_t *)swos_io_alloc(sz + 1u);
    if (!buf) return 1;

    uint32_t rc = RobNorthenLoader_C(ROBN_LOAD, 0, (uint8_t *)name, buf);
    if (rc != 0) { swos_io_free(buf); return 1; }
    buf[sz] = '\0';

    *out_buf = buf;
    *out_size = sz;
    return 0;
}

int swos_io_save(const uint8_t *name, const uint8_t *buf, uint32_t size)
{
    uint32_t rc = RobNorthenLoader_C(ROBN_SAVE, size, (uint8_t *)name,
                                      (uint8_t *)buf);
    return (rc == 0) ? 0 : 1;
}

#endif

/* ============================================================
 * Minimal snprintf: %s / %d / %u / literal characters.
 *
 * The codebase's format strings are simple — prefix + filename or
 * prefix + integer. A handwritten formatter avoids pulling in stdio
 * on Amiga and is small enough to live here. Output is always
 * NUL-terminated; the return value matches stdio snprintf (number
 * of characters that *would* have been written excluding NUL, so
 * the caller can detect truncation by comparing to bufsz).
 * ============================================================ */

static uint32_t fmt_uint(char *buf, uint32_t bufsz, uint32_t *pos, uint32_t v)
{
    char tmp[16];
    int n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v) {
        tmp[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    /* tmp holds digits in reverse — emit forwards. */
    uint32_t emitted = 0;
    for (int i = n - 1; i >= 0; i--) {
        if (*pos + 1u < bufsz) buf[(*pos)++] = tmp[i];
        emitted++;
    }
    return emitted;
}

int swos_io_snprintf(char *buf, uint32_t bufsz, const char *fmt, ...)
{
    if (bufsz == 0) return 0;
    va_list ap;
    va_start(ap, fmt);
    uint32_t pos = 0;
    uint32_t total = 0;
    const char *p = fmt;
    while (*p) {
        if (*p != '%') {
            if (pos + 1u < bufsz) buf[pos++] = *p;
            total++;
            p++;
            continue;
        }
        p++;  /* skip '%' */
        if (*p == 's') {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            while (*s) {
                if (pos + 1u < bufsz) buf[pos++] = *s;
                total++;
                s++;
            }
        } else if (*p == 'd') {
            int v = va_arg(ap, int);
            uint32_t uv;
            if (v < 0) {
                if (pos + 1u < bufsz) buf[pos++] = '-';
                total++;
                uv = (uint32_t)(-(long)v);
            } else {
                uv = (uint32_t)v;
            }
            total += fmt_uint(buf, bufsz, &pos, uv);
        } else if (*p == 'u') {
            uint32_t v = va_arg(ap, unsigned int);
            total += fmt_uint(buf, bufsz, &pos, v);
        } else if (*p == '%') {
            if (pos + 1u < bufsz) buf[pos++] = '%';
            total++;
        } else {
            /* Unsupported specifier — emit verbatim. */
            if (pos + 1u < bufsz) buf[pos++] = '%';
            total++;
            if (*p && pos + 1u < bufsz) { buf[pos++] = *p; total++; }
        }
        if (*p) p++;
    }
    buf[pos] = '\0';
    va_end(ap);
    return (int)total;
}
