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

/* ---- Save directory -------------------------------------------------------
 * Every save file (careers .CAR/.SEA/.PRE/.DIY, tactics .TAC, highlights .HIL,
 * edited teams .EDT, options .CFG) lives under one base directory. On most
 * platforms that's "save" relative to the game dir (CWD, anchored to the binary
 * at startup) — unchanged.
 *
 * macOS exception: the game is usually launched from a .app bundle, and macOS
 * makes a *downloaded* bundle READ-ONLY — Gatekeeper "App Translocation" runs a
 * quarantined app from a randomized read-only mount, and /Applications needs
 * admin. Writing saves inside the bundle then fails: "saving doesn't work". So
 * on macOS, if the in-bundle "save" dir isn't writable, redirect saves to the
 * per-user Application Support dir (always writable) and seed it once with the
 * bundled defaults (FORMA.TAC, TEST.CAR, …) so they're still there. Both the
 * reader (resolve_save_path here) and the writer (resolve_path in resload.c)
 * consult swos_save_dir(), so load and save always agree. */
static char s_save_dir[1024] = "save";

const char *swos_save_dir(void)
{
    return s_save_dir;
}

#ifdef __APPLE__
#include <SDL.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

static int save_dir_writable(const char *dir)
{
    char probe[1100];
    snprintf(probe, sizeof probe, "%s/.swos_write_test", dir);
    FILE *f = fopen(probe, "wb");     /* also fails if `dir` doesn't exist */
    if (!f) return 0;
    fclose(f);
    remove(probe);
    return 1;
}

static void save_copy_file(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    if (!in) return;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return; }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, in)) > 0)
        fwrite(buf, 1, n, out);
    fclose(out);
    fclose(in);
}

/* Copy every regular file from the bundled `src` dir into the writable `dst`,
 * skipping any that already exist (never clobber the player's saves). */
static void save_seed_defaults(const char *src, const char *dst)
{
    DIR *d = opendir(src);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;            /* . .. and dotfiles */
        char sp[1200], dp[1200];
        snprintf(sp, sizeof sp, "%s/%s", src, e->d_name);
        snprintf(dp, sizeof dp, "%s/%s", dst, e->d_name);
        FILE *exists = fopen(dp, "rb");
        if (exists) { fclose(exists); continue; }     /* keep existing */
        save_copy_file(sp, dp);
    }
    closedir(d);
}
#endif  /* __APPLE__ */

void swos_save_dir_init(void)
{
#ifdef __APPLE__
    /* Keep the in-bundle "save" dir while it's writable (a plain CLI run, or an
     * un-quarantined bundle) so the existing workflow is untouched. Only
     * redirect when it isn't — the downloaded/translocated .app that fails now. */
    if (save_dir_writable("save"))
        return;

    char *pref = SDL_GetPrefPath("baker76", "SWOS");   /* ~/Library/Application Support/baker76/SWOS/ */
    if (!pref)
        return;                                        /* give up; keep "save" */

    char cand[1024];
    snprintf(cand, sizeof cand, "%ssave", pref);       /* pref ends in '/' */
    SDL_free(pref);

    if (mkdir(cand, 0777) != 0 && errno != EEXIST)
        return;                                        /* can't create; keep "save" */

    save_seed_defaults("save", cand);                  /* copy bundled defaults once */
    snprintf(s_save_dir, sizeof s_save_dir, "%s", cand);
#endif
}

static void resolve_save_path(const uint8_t *name, char *out, uint32_t out_sz)
{
    const char *src = (const char *)name;
    const char *colon = strchr(src, ':');
    if (colon) src = colon + 1;
    snprintf(out, out_sz, "%s/%s", swos_save_dir(), src);
}

int swos_io_load_alloc(const uint8_t *name, uint8_t **out_buf, uint32_t *out_size)
{
    char path[1280];   /* s_save_dir (up to 1023) + "/" + filename */
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
    char path[1280];
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
