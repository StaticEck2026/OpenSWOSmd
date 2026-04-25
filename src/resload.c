/*
 * resload.c -- SDL/POSIX replacement for the Amiga Resload.s + Resload C port.
 *
 * The original Amiga `resload` module was a thin wrapper over the
 * AmigaOS DOS library (Open/Read/Write/Close/Lock/Examine) plus a
 * RobNorthenLoader-style command dispatcher.
 *
 * On a hosted SDL build we throw all of that away and use stdio +
 * dirent.h. The dispatch surface (`_RobNorthenLoader_C`) and the
 * helper symbols (`_AllocMemAny`, `_FixSnoopBug`, `_KBHook`, ...) keep
 * their original signatures so the rest of the C code links unchanged.
 *
 * Save files go to "save/<NAME>", data files to whatever path the
 * caller passes in ("data/...", "grafs/..."). All paths are relative
 * to the working directory; main() chdirs to the binary's own
 * directory at startup so these resolve under the game's bin/ folder
 * regardless of where the user launched SWOS from. We do not
 * implement RNC decompression here -- that lives in rnc.c and the
 * asset loader calls it explicitly via _RncDecompress*.
 */

#include "resload.h"
#include "display.h"
#include "rnc_magic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>

extern void _RncDecompressV1(uint8_t *src, uint8_t *dst);
extern void _RncDecompressV2(uint8_t *src, uint8_t *dst);

/* ---- Globals shared with the rest of the C code -------------------- */
/* `size` was defined in Resload.s; the rob_* helpers stash the byte
 * count of the most-recent operation into its low 16 bits. */
uint32_t size;

/* Return value the asm bridge stashed in d1. We keep it for any C code
 * that still reads it. */
uint32_t _rnl_d1_return;

/* Quit-on-keystroke scancode (DEL). KBHook still updates g_quitToDos. */
uint8_t _keyexit = 0x46;

/* ---- Path resolver ------------------------------------------------- */
/* Strip an Amiga drive prefix ("DF0:", "PROGDIR:", ...) and pick the
 * right local subdirectory: save files go under "save/", everything
 * else is treated as a path relative to "bin/" (where the Amiga data
 * lives in this project). */
static const char *known_save_exts[] = {
    ".DIY", ".PRE", ".SEA", ".CAR", ".TAC", ".HIL", ".EDT", NULL
};

static int has_save_ext(const char *name)
{
    size_t len = strlen(name);
    if (len < 4) return 0;
    const char *tail = name + len - 4;
    for (const char **e = known_save_exts; *e; ++e)
        if (strcasecmp(*e, tail) == 0) return 1;
    return 0;
}

static void resolve_path(const uint8_t *amiga_name, char *out, size_t out_sz)
{
    const char *src = (const char *)amiga_name;
    /* Skip Amiga "VOLUME:" prefix if any. */
    const char *colon = strchr(src, ':');
    if (colon) src = colon + 1;

    if (has_save_ext(src)) {
        snprintf(out, out_sz, "save/%s", src);
    } else {
        /* Asset paths come in like "grafs/foo.raw" or "data/bar.dat" --
         * the caller already includes the subdirectory; pass through. */
        snprintf(out, out_sz, "%s", src);
    }
}

/* ---- Low-level file ops ------------------------------------------- */
/* Try to open `path`, falling back to a case-insensitive search of the
 * containing directory if the literal path doesn't exist. The original
 * Amiga DOS was case-insensitive; the data files shipped with the game
 * are uppercase ("HERO.INS") but the C code asks for lowercase
 * ("sound/hero.ins"). */
static FILE *open_case_insensitive(const char *path, const char *mode)
{
    FILE *f = fopen(path, mode);
    if (f) return f;

    /* Split into dir + base */
    char dir[512];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (!slash) return NULL;
    *slash = 0;
    const char *base = slash + 1;

    DIR *d = opendir(dir);
    if (!d) return NULL;
    struct dirent *de;
    char alt[512];
    while ((de = readdir(d)) != NULL) {
        if (strcasecmp(de->d_name, base) == 0) {
            snprintf(alt, sizeof(alt), "%s/%s", dir, de->d_name);
            closedir(d);
            return fopen(alt, mode);
        }
    }
    closedir(d);
    return NULL;
}

static uint32_t fs_get_size(const char *path)
{
    FILE *f = open_case_insensitive(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fclose(f);
    return n > 0 ? (uint32_t)n : 0;
}

static uint32_t fs_load(const char *path, uint8_t *dest)
{
    FILE *f = open_case_insensitive(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return 0; }
    size_t got = fread(dest, 1, (size_t)n, f);
    fclose(f);
    return (uint32_t)got;
}

static uint32_t fs_save(const char *path, const uint8_t *src, uint32_t n)
{
    /* Make sure parent dir exists (best-effort -- ignore failure). */
    char buf[512];
    snprintf(buf, sizeof(buf), "%s", path);
    char *slash = strrchr(buf, '/');
    if (slash) {
        *slash = 0;
        mkdir(buf, 0755);
    }

    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    size_t put = fwrite(src, 1, n, f);
    fclose(f);
    return (uint32_t)put;
}

static uint32_t fs_delete(const char *path)
{
    return (remove(path) == 0) ? 1 : 0;
}

/* List filenames in `dirpath` into a flat double-NUL-terminated buffer.
 * Each entry is "name\0", terminated overall by an extra "\0". Returns
 * the number of entries. */
static uint32_t fs_listdir(const char *dirpath, uint8_t *out, uint32_t out_sz)
{
    fprintf(stderr, "[fs_listdir] enter dirpath=%s\n", dirpath);
    DIR *d = opendir(dirpath);
    if (!d) {
        fprintf(stderr, "[fs_listdir] opendir(%s) failed: errno=%d (%s)\n",
                dirpath, errno, strerror(errno));
        return 0;
    }
    fprintf(stderr, "[fs_listdir] opendir ok\n");

    struct dirent *de;
    uint8_t *p = out;
    uint8_t *end = out + out_sz - 2;
    uint32_t n = 0;
    int seen = 0;

    while ((errno = 0, de = readdir(d)) != NULL) {
        seen++;
        fprintf(stderr, "[fs_listdir]   readdir: '%s'\n", de->d_name);
        if (de->d_name[0] == '.') continue;
        size_t len = strlen(de->d_name);
        if (p + len + 1 >= end) break;
        memcpy(p, de->d_name, len + 1);
        p += len + 1;
        ++n;
    }
    fprintf(stderr, "[fs_listdir] done seen=%d kept=%u readdir-errno=%d (%s)\n",
            seen, (unsigned)n, errno, errno ? strerror(errno) : "");
    *p = 0;
    closedir(d);
    return n;
}

/* ---- RobNorthenLoader command handlers ---------------------------- */
static uint8_t s_listdir_buf[2048];

/* AmigaDOS convention: 0 = success, non-zero = error code. The C
 * call sites in asset_sys.c / asset_loader.c check `result == 0` for
 * success, so we must return 0 on a successful load/save. The buffer
 * we return is RAW disk content -- callers like _LoadRNCToBufferNoBlit
 * expect to see the RNC header and decompress themselves. */
static uint32_t rob_load(uint8_t *name, uint8_t *dest)
{
    char path[512];
    resolve_path(name, path, sizeof(path));
    uint32_t got = fs_load(path, dest);
    size = got;
    _rnl_d1_return = got;
    return got ? 0 : 1;
}

static uint32_t rob_save(uint8_t *name, uint8_t *src, uint32_t fileSize)
{
    char path[512];
    resolve_path(name, path, sizeof(path));
    uint32_t put = fs_save(path, src, fileSize);
    size = put;
    _rnl_d1_return = put;
    return put ? 0 : 1;
}

static uint32_t rob_delete(uint8_t *name)
{
    char path[512];
    resolve_path(name, path, sizeof(path));
    fs_delete(path);
    _rnl_d1_return = 0;
    return 0;
}

static uint32_t rob_get_file_size(uint8_t *name)
{
    char path[512];
    resolve_path(name, path, sizeof(path));
    uint32_t n = fs_get_size(path);
    size = n;
    _rnl_d1_return = n;
    return n;
}

static uint32_t rob_list_directory(uint8_t *name, uint8_t *callerBuf)
{
    (void)name;
    /* The Amiga implementation always listed "save/". Match that. */
    uint32_t count = fs_listdir("save", s_listdir_buf, sizeof(s_listdir_buf));
    {
        char cwd[512] = {0};
        if (!getcwd(cwd, sizeof(cwd))) cwd[0] = 0;
        fprintf(stderr, "[listdir] cwd=%s count=%u callerBuf=%p\n", cwd, (unsigned)count, (void*)callerBuf);
        uint8_t *p = s_listdir_buf;
        for (uint32_t i = 0; i < count && i < 8; i++) {
            fprintf(stderr, "[listdir]   '%s'\n", (char*)p);
            p += strlen((char*)p) + 1;
        }
    }
    if (count == 0) {
        _rnl_d1_return = 0;
        size = 0;
        return 0;
    }

    /* Repackage into the AmigaDOS Examine()-derived 32-byte slot
     * layout that AssetSys_ResolveAssetList consumes. callerBuf points
     * at &numberOfFiles + 2 (the asm uses `lea 2(a4),a1` before the
     * dispatch call), so each 32-byte floppy slot is offset by 2 from
     * the reader's "logical entry" view:
     *
     *     reader entry N at &numberOfFiles + N*32:
     *       byte 0..1  = unused (entry 0 has the count here)
     *       byte 2     = type   (2 = source-path drop, 0 = file)
     *       byte 3     = name length (signed)
     *       byte 4..   = NUL-terminated filename (up to 28 chars)
     *
     *     floppy slot N at callerBuf + N*32:
     *       byte 0     = type
     *       byte 1     = name length
     *       byte 2..29 = name + NUL
     *       byte 30..31 = padding (overlaps next entry's "unused" head)
     *
     * `_GetFilenameAndExtension` later iterates `foundFilenamesBuffer`
     * (aliased to &numberOfFiles + 4 — i.e. the start of entry 0's
     * name field) with a 32-byte stride and scans each slot for '.'. */
    uint8_t *src = s_listdir_buf;
    uint8_t *dst = callerBuf;
    for (uint32_t i = 0; i < count; ++i) {
        size_t len = strlen((char *)src);
        memset(dst, 0, 0x20);
        dst[0] = 0;                /* type: regular file */
        if (len > 0x1B) len = 0x1B; /* clamp to 28-char name slot */
        dst[1] = (uint8_t)len;     /* name length */
        memcpy(dst + 2, src, len);
        dst[2 + len] = 0;
        src += strlen((char *)src) + 1;
        dst += 0x20;
    }

    *(uint16_t *)&size = (uint16_t)count;
    _rnl_d1_return = count;
    return 0;  /* AmigaDOS convention: 0 = success; the count is in _rnl_d1_return */
}

uint32_t _RobNorthenLoader_C(uint32_t cmd, uint32_t arg1, uint8_t *name, uint8_t *buf)
{
    switch (cmd) {
    case 0: return rob_load(name, buf);
    case 1: return rob_save(name, buf, arg1);
    case 2: return rob_delete(name);
    case 3: return rob_list_directory(name, buf);
    case 4: _rnl_d1_return = 0; return 0;       /* format disk -- no-op */
    case 5: return rob_get_file_size(name);
    case 6:
        /* "Finalise after cmd 5": `_Op5ThenMaybe6` calls this when cmd 5
         * reported a non-zero file size, and `_AssetCacheLoadFromAssetSys`
         * then reads `_rnl_d1_return` to decide how big a cache node to
         * allocate. Crucially we must NOT zero d1_return here -- doing
         * so produced a zero-byte node whose split-header shared the
         * payload address, causing the next file load to corrupt the
         * cache free list. */
        return 0;
    default:
        /* Unknown cmd: leave d1_return alone; the caller may still be
         * reading a value from an earlier cmd 5. */
        return 0;
    }
}

/* ---- Hardware-side helpers (no-ops on SDL) ------------------------ */
void _CloseDosLibrary(void) {}
void _FixSnoopBug(void)     {}
void _FixMemoryRoutine(void) {}

/* KBHook was the Amiga keyboard CIA poller. On SDL we feed input via
 * input_sdl.c, so just return 0 here. */
uint8_t _KBHook(void) { return 0; }
