/*
 * sd_sources.c -- the player's own copy of the game, wherever it is.
 */
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
    return Sd_FindNoCase(dir, "swos-port-Release.exe", p, sizeof p) == 0 &&
           Sd_FindNoCase(dir, "pitch1.dat", p, sizeof p) == 0 &&
           Sd_FindNoCase(dir, "pitch1.blk", p, sizeof p) == 0;
}

/* A PC game folder: SPRITE.DAT and the DATA folder's team files -- and not
 * SWOS 2020's, which has both but its own graphics. */
static int32_t is_dos_dir(const char *dir)
{
    char p[1024];
    return Sd_FindNoCase(dir, "SPRITE.DAT", p, sizeof p) == 0 &&
           Sd_FindNoCase(dir, "DATA/TEAM.000", p, sizeof p) == 0 &&
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
    if (Sd_FindNoCase(dir, "swos", game, sizeof game) == 0 && is_s2020_game_dir(game)) {
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
    SdBuf b;
    char err[256];
    if (Sd_ReadFile(path, &b, err) != 0)
        return 0;
    int32_t found = 0;
    static const char tell[] = "SENSIBLE";
    for (uint32_t i = 0; i + sizeof tell - 1 <= b.size && !found; i++)
        if (memcmp(b.data + i, tell, sizeof tell - 1) == 0)
            found = 1;
    Sd_BufFree(&b);
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

static void scan_dir(SdSources *s, const char *dir, int32_t depth)
{
    if (is_dos_dir(dir) && !s->dosDir[0])
        snprintf(s->dosDir, sizeof s->dosDir, "%s", dir);
    if (!s->s2020Dir[0])
        find_s2020(dir, s->s2020Dir, sizeof s->s2020Dir);
#ifdef _WIN32
    char pattern[1100];
    Sd_PathJoin(pattern, sizeof pattern, dir, "*");
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        const char *name = fd.cFileName;
#else
    DIR *d = opendir(dir);
    if (!d)
        return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        const char *name = e->d_name;
#endif
        if (name[0] == '.')
            continue;
        char p[1100];
        Sd_PathJoin(p, sizeof p, dir, name);
        if (is_dir(p)) {
            if (depth > 0)
                scan_dir(s, p, depth - 1);
        } else {
            SdSources_AddFile(s, p);
        }
#ifdef _WIN32
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    }
    closedir(d);
#endif
}

int32_t SdSources_Scan(SdSources *s, const char *dir)
{
    scan_dir(s, dir, 1);
    return count_found(s);
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

int32_t SdSources_ReadAmiga(SdSources *s, const char *rel, SdBuf *out, char *err)
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

int32_t SdSources_ReadDos(SdSources *s, const char *rel, SdBuf *out, char *err)
{
    char p[1024];
    if (!s->dosDir[0] || Sd_FindNoCase(s->dosDir, rel, p, sizeof p) != 0) {
        snprintf(err, 256, "%s is not in the PC game folder", rel);
        return -1;
    }
    return Sd_ReadFile(p, out, err);
}
