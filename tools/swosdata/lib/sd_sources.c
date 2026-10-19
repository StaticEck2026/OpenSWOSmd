/*
 * sd_sources.c -- the player's own copy of the game, wherever it is.
 */
/* lstat, under -std=c99: a link is never followed when searching a folder. */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "sd_sources.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#define JAG_ROM_SIZE (2u * 1024 * 1024)

static int32_t ends_with_nocase(const char *s, const char *suffix)
{
    size_t ls = strlen(s), lx = strlen(suffix);
    if (lx > ls)
        return 0;
    for (size_t i = 0; i < lx; i++)
        if (tolower((unsigned char)s[ls - lx + i]) != tolower((unsigned char)suffix[i]))
            return 0;
    return 1;
}

static int32_t is_dir(const char *path)
{
#ifdef _WIN32
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

static long file_size(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fclose(f);
    return n;
}

/* SWOS 2020's game folder (<install>/swos): its program and a pitch. */
static int32_t is_s2020_game_dir(const char *dir)
{
    char p[1024];
    return Sg_FindNoCase(dir, "swos-port-Release.exe", p, sizeof p) == 0 &&
           Sg_FindNoCase(dir, "pitch1.dat", p, sizeof p) == 0 &&
           Sg_FindNoCase(dir, "pitch1.blk", p, sizeof p) == 0;
}

/* A PC game folder: SPRITE.DAT and the DATA folder's team files -- and not
 * SWOS 2020's, which has both but its own graphics. */
static int32_t is_dos_dir(const char *dir)
{
    char p[1024];
    return Sg_FindNoCase(dir, "SPRITE.DAT", p, sizeof p) == 0 &&
           Sg_FindNoCase(dir, "DATA/TEAM.000", p, sizeof p) == 0 &&
           !is_s2020_game_dir(dir);
}

/* `dir` is SWOS 2020's install or its game folder: the game folder into out. */
static int32_t find_s2020(const char *dir, char *out, uint32_t cap)
{
    char game[1024];
    if (is_s2020_game_dir(dir)) {
        snprintf(out, cap, "%s", dir);
        return 0;
    }
    if (Sg_FindNoCase(dir, "swos", game, sizeof game) == 0 && is_s2020_game_dir(game)) {
        snprintf(out, cap, "%s", game);
        return 0;
    }
    return -1;
}

/* The cartridge: 2 MB, and the game's name where the Jaguar header keeps
 * nothing useful -- so check for the LZ-packed sheets' tell-tale instead:
 * Sensible Soccer International Edition's own text. */
static int32_t is_jag_rom(const char *path)
{
    if (!(ends_with_nocase(path, ".jag") || ends_with_nocase(path, ".j64") ||
          ends_with_nocase(path, ".rom") || ends_with_nocase(path, ".bin")))
        return 0;
    if (file_size(path) != (long)JAG_ROM_SIZE)
        return 0;
    SgBuf b;
    char err[256];
    if (Sg_ReadFile(path, &b, err) != 0)
        return 0;
    int32_t found = 0;
    static const char tell[] = "SENSIBLE";
    for (uint32_t i = 0; i + sizeof tell - 1 <= b.size && !found; i++)
        if (memcmp(b.data + i, tell, sizeof tell - 1) == 0)
            found = 1;
    Sg_BufFree(&b);
    return found;
}

int32_t SdSources_AddFile(SdSources *s, const char *path)
{
    char err[256];
    if (ends_with_nocase(path, ".adf")) {
        Adf *a = Adf_Open(path, err);
        if (!a)
            return -1;
        int32_t rc = -1;
        char p[1024];
        const char *vol = Adf_VolumeName(a);
        uint32_t n;
        /* Disk 1 holds the game, disk 2 the rest of the data. Volume names
         * first; a renamed disk still has its files. */
        uint8_t *probe = Adf_ReadFile(a, "SWOS2", &n, err);
        int32_t looks1 = probe != NULL || strcmp(vol, "SWOS") == 0;
        free(probe);
        if (looks1 && !s->adf1[0]) {
            snprintf(s->adf1, sizeof s->adf1, "%s", path);
            rc = 0;
        } else if (!looks1 && (strcmp(vol, "SWOS2") == 0) && !s->adf2[0]) {
            snprintf(s->adf2, sizeof s->adf2, "%s", path);
            rc = 0;
        }
        (void)p;
        Adf_Close(a);
        return rc;
    }
    if (is_jag_rom(path)) {
        if (!s->jagRom[0])
            snprintf(s->jagRom, sizeof s->jagRom, "%s", path);
        return 0;
    }
    if (is_dir(path)) {
        if (is_dos_dir(path)) {
            if (!s->dosDir[0])
                snprintf(s->dosDir, sizeof s->dosDir, "%s", path);
            return 0;
        }
        char game[1024];
        if (find_s2020(path, game, sizeof game) == 0) {
            if (!s->s2020Dir[0])
                snprintf(s->s2020Dir, sizeof s->s2020Dir, "%s", game);
            return 0;
        }
        return -1;
    }
    /* A file inside the PC game folder or SWOS 2020's: take its folder. */
    char dir[1024];
    snprintf(dir, sizeof dir, "%s", path);
    char *slash = strrchr(dir, '/');
    char *bslash = strrchr(dir, '\\');
    if (bslash && (!slash || bslash > slash))
        slash = bslash;
    if (slash) {
        *slash = '\0';
        if (is_dos_dir(dir)) {
            if (!s->dosDir[0])
                snprintf(s->dosDir, sizeof s->dosDir, "%s", dir);
            return 0;
        }
        char game[1024];
        if (find_s2020(dir, game, sizeof game) == 0) {
            if (!s->s2020Dir[0])
                snprintf(s->s2020Dir, sizeof s->s2020Dir, "%s", game);
            return 0;
        }
    }
    return -1;
}

static int32_t count_found(const SdSources *s)
{
    return (s->adf1[0] != 0) + (s->adf2[0] != 0) + (s->dosDir[0] != 0) + (s->jagRom[0] != 0) +
           (s->s2020Dir[0] != 0);
}

/* ---- the folder search ---------------------------------------------------- */

#define SCAN_MAX_DEPTH 24       /* deeper than any real install; a guard, not a limit */

typedef struct {
    SdScanProgress progress;
    void          *user;
    int32_t        folders;
    int32_t        stopped;
} ScanCtx;

static int32_t all_found(const SdSources *s)
{
    return s->adf1[0] && s->adf2[0] && s->dosDir[0] && s->jagRom[0] && s->s2020Dir[0];
}

static int32_t contains_nocase(const char *s, const char *word)
{
    size_t lw = strlen(word);
    for (; *s; s++) {
        size_t i = 0;
        while (i < lw && s[i] && tolower((unsigned char)s[i]) == word[i])
            i++;
        if (i == lw)
            return 1;
    }
    return 0;
}

/* Folders no copy of the game is in: hidden ones, and Windows' own. */
static int32_t skip_folder(const char *name)
{
    return name[0] == '.' || name[0] == '$' ||
           contains_nocase(name, "system volume information");
}

/* A file met on the way. An .adf is opened only when its volume name is one
 * of the game's disks, or its file name says it is the game (a renamed disk):
 * a folder of a thousand other Amiga games costs a thousand root blocks. */
static void scan_file(SdSources *s, const char *path, const char *name)
{
    if (ends_with_nocase(name, ".adf")) {
        char vol[32];
        if (s->adf1[0] && s->adf2[0])
            return;
        if (Adf_PeekVolumeName(path, vol) != 0)
            return;
        if (strcmp(vol, "SWOS") != 0 && strcmp(vol, "SWOS2") != 0 &&
            !contains_nocase(name, "swos") && !contains_nocase(name, "sensible"))
            return;
    } else if (!(ends_with_nocase(name, ".jag") || ends_with_nocase(name, ".j64") ||
                 ends_with_nocase(name, ".rom") || ends_with_nocase(name, ".bin")) || s->jagRom[0]) {
        return;
    }
    SdSources_AddFile(s, path);
}

typedef struct {
    char    name[260];
    uint8_t isDir;
} Entry;

/* One folder's entries, links left out (a junction or symbolic link could lead
 * back up, so it is never followed). malloc'd; *count entries. */
static Entry *list_dir(const char *dir, int32_t *count)
{
    Entry *list = NULL;
    int32_t n = 0, cap = 0;
#ifdef _WIN32
    char pattern[1100];
    Sg_PathJoin(pattern, sizeof pattern, dir, "*");
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        *count = 0;
        return NULL;
    }
    do {
        const char *name = fd.cFileName;
        int32_t link = (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
        int32_t isDirectory = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    DIR *d = opendir(dir);
    if (!d) {
        *count = 0;
        return NULL;
    }
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        const char *name = e->d_name;
        char full[1100];
        struct stat st;
        Sg_PathJoin(full, sizeof full, dir, name);
        if (lstat(full, &st) != 0)
            continue;
        int32_t link = S_ISLNK(st.st_mode);
        int32_t isDirectory = S_ISDIR(st.st_mode);
#endif
        if (!link && strcmp(name, ".") != 0 && strcmp(name, "..") != 0 && strlen(name) < sizeof list->name) {
            if (n == cap) {
                cap = cap ? cap * 2 : 64;
                Entry *grown = realloc(list, sizeof *list * (size_t)cap);
                if (!grown)
                    break;
                list = grown;
            }
            snprintf(list[n].name, sizeof list[n].name, "%s", name);
            list[n].isDir = (uint8_t)isDirectory;
            n++;
        }
#ifdef _WIN32
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    }
    closedir(d);
#endif
    *count = n;
    return list;
}

static int32_t has_entry(const Entry *list, int32_t n, const char *name, int32_t isDir)
{
    for (int32_t i = 0; i < n; i++) {
        if (list[i].isDir != isDir)
            continue;
        const char *a = list[i].name, *b = name;
        while (*a && tolower((unsigned char)*a) == tolower((unsigned char)*b))
            a++, b++;
        if (!*a && !*b)
            return 1;
    }
    return 0;
}

static void scan_dir(SdSources *s, const char *dir, int32_t depth, ScanCtx *ctx)
{
    if (ctx->stopped || all_found(s))
        return;
    ctx->folders++;
    if (ctx->progress && ctx->progress(ctx->user, ctx->folders, dir) != 0) {
        ctx->stopped = 1;
        return;
    }
    int32_t n;
    Entry *list = list_dir(dir, &n);

    /* The game's own folders are found whole; nothing else is inside them (and
     * SWOS 2020's holds PC graphics of its own, which are not the PC game). The
     * full checks run only where the listing hints at one, so a folder is
     * listed once, not once a check. */
    int32_t gameDir = 0;
    if (has_entry(list, n, "SPRITE.DAT", 0) && has_entry(list, n, "DATA", 1) && is_dos_dir(dir)) {
        if (!s->dosDir[0])
            snprintf(s->dosDir, sizeof s->dosDir, "%s", dir);
        gameDir = 1;
    } else if (has_entry(list, n, "swos", 1) || has_entry(list, n, "swos-port-Release.exe", 0)) {
        char game[1024];
        if (find_s2020(dir, game, sizeof game) == 0) {
            if (!s->s2020Dir[0])
                snprintf(s->s2020Dir, sizeof s->s2020Dir, "%s", game);
            gameDir = 1;
        }
    }
    for (int32_t i = 0; i < n && !ctx->stopped && !all_found(s); i++) {
        char p[1100];
        Sg_PathJoin(p, sizeof p, dir, list[i].name);
        if (!list[i].isDir)
            scan_file(s, p, list[i].name);
        else if (!gameDir && depth < SCAN_MAX_DEPTH && !skip_folder(list[i].name))
            scan_dir(s, p, depth + 1, ctx);
    }
    free(list);
}

int32_t SdSources_ScanEx(SdSources *s, const char *dir, SdScanProgress progress, void *user)
{
    ScanCtx ctx = { progress, user, 0, 0 };
    scan_dir(s, dir, 0, &ctx);
    return ctx.stopped ? -1 : count_found(s);
}

int32_t SdSources_Scan(SdSources *s, const char *dir)
{
    return SdSources_ScanEx(s, dir, NULL, NULL);
}

int32_t SdSources_HaveAmiga(const SdSources *s) { return s->adf1[0] && s->adf2[0]; }
int32_t SdSources_HaveDos(const SdSources *s) { return s->dosDir[0] != 0; }
int32_t SdSources_HaveJaguar(const SdSources *s) { return s->jagRom[0] != 0; }
int32_t SdSources_HaveSwos2020(const SdSources *s) { return s->s2020Dir[0] != 0; }

int32_t SdSources_Open(SdSources *s, char *err)
{
    if (s->adf1[0] && !s->disk1 && !(s->disk1 = Adf_Open(s->adf1, err)))
        return -1;
    if (s->adf2[0] && !s->disk2 && !(s->disk2 = Adf_Open(s->adf2, err)))
        return -1;
    return 0;
}

void SdSources_Close(SdSources *s)
{
    Adf_Close(s->disk1);
    Adf_Close(s->disk2);
    s->disk1 = s->disk2 = NULL;
}

int32_t SdSources_ReadAmiga(SdSources *s, const char *rel, SgBuf *out, char *err)
{
    Adf *disks[2] = { s->disk1, s->disk2 };
    for (int32_t i = 0; i < 2; i++) {
        if (!disks[i])
            continue;
        uint32_t n;
        char e2[256];
        uint8_t *d = Adf_ReadFile(disks[i], rel, &n, e2);
        if (d) {
            out->data = d;
            out->size = n;
            return 0;
        }
    }
    snprintf(err, 256, "%s is not on the Amiga disks", rel);
    return -1;
}

int32_t SdSources_ReadDos(SdSources *s, const char *rel, SgBuf *out, char *err)
{
    char p[1024];
    if (!s->dosDir[0] || Sg_FindNoCase(s->dosDir, rel, p, sizeof p) != 0) {
        snprintf(err, 256, "%s is not in the PC game folder", rel);
        return -1;
    }
    return Sg_ReadFile(p, out, err);
}
