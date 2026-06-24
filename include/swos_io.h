/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* swos_io.h — portable I/O + heap shim for save_format_json.c (and any
 * other code that needs whole-file load/save or growable buffers on
 * both ports).
 *
 * The Amiga m68k toolchain has no <stdio.h> / <stdlib.h>; the SDL
 * (hosted) build has both but reads through stdio rather than the
 * asset/disk pipeline. This header gives us a single API:
 *
 *   SDL backing:   fopen/fread (load), fwrite (save), malloc/realloc/
 *                  free, snprintf.
 *   Amiga backing: RobNorthenLoader_C dispatch + AllocMem/FreeMem
 *                  (via AllocMemAny / FreeMemAny in resload.c),
 *                  plus a minimal %s/%d snprintf implementation.
 *
 * All functions return 0 / non-NULL on success.
 */
#ifndef SWOS_IO_H
#define SWOS_IO_H

#include "swos_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Whole-file I/O ---- */

/* Load `name` into a fresh buffer. *out_buf gets a heap allocation
 * holding `*out_size` bytes plus a trailing NUL. Free with swos_io_free.
 * Returns 0 on success, non-zero on failure (file not found, OOM,
 * short read). On failure *out_buf is not modified. */
int  swos_io_load_alloc(const uint8_t *name, uint8_t **out_buf, uint32_t *out_size);

/* Save `size` bytes from `buf` to file `name`. Overwrites if it
 * exists. Returns 0 on success, non-zero on failure. */
int  swos_io_save(const uint8_t *name, const uint8_t *buf, uint32_t size);

/* ---- Heap (size-tagged allocations) ----
 *
 * swos_io_alloc / swos_io_realloc / swos_io_free are a portable
 * malloc/realloc/free triple. The first 4 bytes of each block store
 * the allocation size so free/realloc work without the caller
 * tracking it — same as malloc. The buffer returned by
 * swos_io_load_alloc uses this same scheme, so swos_io_free is the
 * universal release call. */

void *swos_io_alloc(uint32_t sz);
void *swos_io_realloc(void *p, uint32_t new_sz);
void  swos_io_free(void *p);

/* ---- Minimal portable formatter ----
 *
 * Supports `%s` (NUL-terminated string), `%d` (signed int, base 10),
 * `%u` (unsigned int, base 10), and literal characters. Output is
 * NUL-terminated and clamped to `bufsz - 1` characters of content.
 * Returns the number of characters that would have been written
 * (excluding the NUL) — i.e. matches snprintf's return contract for
 * truncation detection. */
int swos_io_snprintf(char *buf, uint32_t bufsz, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* SWOS_IO_H */
