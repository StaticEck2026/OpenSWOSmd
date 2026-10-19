/*
 * swosdata.c -- build OpenSWOS's game data from the player's own copy of the game.
 */
#include "swosdata.h"
#include "sd_manifest.h"
#include "amiga_raw.h"
#include "credits_aga.h"
#include "dos_picture.h"
#include "dos_pitch.h"
#include "team_json.h"
#include "dos2amiga.h"
#include "font_extend.h"
#include "jag2amiga.h"
#include "rnc_pack.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

static void logf_(SdJob *job, const char *fmt, ...)
{
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (job->log)
        job->log(job->user, line);
}

static int32_t count_amiga(void)
{
    int32_t n = 0;
    while (g_sdAmigaCopy[n])
        n++;
    return n;
}

static int32_t count_dos(void)
{
    int32_t n = 0;
    while (g_sdDosCopy[n].dest)
        n++;
    return n;
}

void SwosData_Plan(const SdJob *job, SdStepInfo info[SD_STEP_COUNT])
{
    info[SD_STEP_AMIGA_FILES].name = "Amiga data, graphics and music";
    info[SD_STEP_AMIGA_FILES].needs = "both Amiga disks";
    info[SD_STEP_AMIGA_FILES].ready = SdSources_HaveAmiga(&job->src);

    info[SD_STEP_DOS_FILES].name = "PC graphics, sound and commentary";
    info[SD_STEP_DOS_FILES].needs = "the PC game";
    info[SD_STEP_DOS_FILES].ready = SdSources_HaveDos(&job->src);

    info[SD_STEP_AGA_PC].name = "AGA menus, pitches and credits";
    info[SD_STEP_AGA_PC].needs = "the PC game";
    info[SD_STEP_AGA_PC].ready = SdSources_HaveDos(&job->src);

    info[SD_STEP_AGA_AMIGA].name = "AGA loading screens";
    info[SD_STEP_AGA_AMIGA].needs = "both Amiga disks";
    info[SD_STEP_AGA_AMIGA].ready = SdSources_HaveAmiga(&job->src);

    info[SD_STEP_TEAMS].name = "Teams and players";
    info[SD_STEP_TEAMS].needs = "the Amiga disks or the PC game";
    info[SD_STEP_TEAMS].ready = SdSources_HaveAmiga(&job->src) || SdSources_HaveDos(&job->src);

    info[SD_STEP_JAGUAR].name = "Atari Jaguar sprites";
    info[SD_STEP_JAGUAR].needs = "the Jaguar cartridge and the PC game";
    info[SD_STEP_JAGUAR].ready = SdSources_HaveJaguar(&job->src) && SdSources_HaveDos(&job->src);

    info[SD_STEP_SWOS2020].name = "SWOS 2020 pitches";
    info[SD_STEP_SWOS2020].needs = "a SWOS 2020 install";
    info[SD_STEP_SWOS2020].ready = SdSources_HaveSwos2020(&job->src);
}

static int32_t write_out(SdJob *job, const char *rel, const SdBuf *b, char *err)
{
    char path[1100];
    Sd_PathJoin(path, sizeof path, job->outDir, rel);
    return Sd_WriteFile(path, b->data, b->size, err);
}

static void tick(SdJob *job, int32_t *done, int32_t total)
{
    if (job->progress)
        job->progress(job->user, ++*done, total);
}

static void glyphs_path(const SdJob *job, char *out, uint32_t cap)
{
    Sd_PathJoin(out, cap, job->resDir, "setup/glyphs.txt");
}

static int32_t write_packed(SdJob *job, const char *rel, const SdBuf *b, char *err);

static int32_t step_amiga_files(SdJob *job, int32_t *done, int32_t total, char *err)
{
    for (int32_t i = 0; g_sdAmigaCopy[i]; i++) {
        SdBuf b;
        if (SdSources_ReadAmiga(&job->src, g_sdAmigaCopy[i], &b, err) != 0)
            return -1;
        int32_t rc = write_out(job, g_sdAmigaCopy[i], &b, err);
        Sd_BufFree(&b);
        if (rc != 0)
            return -1;
        if (job->progress)
            job->progress(job->user, ++*done, total);
    }
    /* The font, with the extra letters drawn in (written over the disk's own). */
    {
        SdBuf raw, un, out;
        SdImage img;
        char glyphs[1100];
        glyphs_path(job, glyphs, sizeof glyphs);
        if (SdSources_ReadAmiga(&job->src, "grafs/CHARSET.RAW", &raw, err) != 0)
            return -1;
        int32_t rc = Sd_RncUnpack(&raw, &un, err);
        Sd_BufFree(&raw);
        if (rc == 0 && (rc = Sd_AmigaRawDecode(&un, 320, 256, 4, &img, err)) == 0) {
            if ((rc = Sd_FontExtend(&img, glyphs, err)) == 0 &&
                (rc = Sd_AmigaRawEncode(&img, 4, &out, err)) == 0) {
                rc = write_packed(job, "grafs/CHARSET.RAW", &out, err);
                Sd_BufFree(&out);
            }
            SdImage_Free(&img);
        }
        Sd_BufFree(&un);
        if (rc != 0)
            return -1;
        tick(job, done, total);
    }
    logf_(job, "Amiga: %d files", count_amiga() + 1);
    return 0;
}

static int32_t count_2020(void)
{
    int32_t n = 0;
    while (g_sd2020Copy[n].dest)
        n++;
    return n;
}

static int32_t step_dos_files(SdJob *job, int32_t *done, int32_t total, char *err)
{
    for (int32_t i = 0; g_sdDosCopy[i].dest; i++) {
        SdBuf b;
        if (SdSources_ReadDos(&job->src, g_sdDosCopy[i].src, &b, err) != 0)
            return -1;
        int32_t rc = write_out(job, g_sdDosCopy[i].dest, &b, err);
        Sd_BufFree(&b);
        if (rc != 0)
            return -1;
        if (job->progress)
            job->progress(job->user, ++*done, total);
    }
    logf_(job, "PC: %d files", count_dos());
    return 0;
}

/* The full-screen pictures the PC game has no version of: the Amiga's own,
 * remapped to the AGA build's 256 colours (SwosGfx !DOS2AGA.bat step 1). */
static const char *const kAmigaPictures[] = {
    "LOADER00", "LOADER01", "LOADER1", "LOADER10", "LOADER2", "LOADER3", "LOADER4",
    "LOADER5", "LOADER6", "LOADER7", "LOADER8A", "LOADER8B", "LOADER9", "DISK22", "OLDDISK1",
};
#define AGA_PC_COUNT (SD_BANK_COUNT + 2 + 6 + 1)   /* banks, MENUBG, MENUBG2, SWCPICH1-6, CREDITS */

/* Packed as SwosGfx packed them (RNC method 2). Not optional: the game reads a
 * file whole into its screen buffer before unpacking it, so a 352x272 8-plane
 * picture (95,744 bytes) only fits packed. */
static int32_t write_packed(SdJob *job, const char *rel, const SdBuf *b, char *err)
{
    SdBuf packed;
    if (Sd_RncPackLikeSwosGfx(b, &packed, err) != 0)
        return -1;
    int32_t rc = write_out(job, rel, &packed, err);
    Sd_BufFree(&packed);
    return rc;
}

static int32_t write_raw(SdJob *job, const char *rel, const SdImage *img, char *err)
{
    SdBuf raw;
    if (Sd_AmigaRawEncode(img, 0, &raw, err) != 0)
        return -1;
    int32_t rc = write_packed(job, rel, &raw, err);
    Sd_BufFree(&raw);
    return rc;
}

/* A PC picture, enlarged to the Amiga screen (SWTITLE -> MENUBG, STAD -> MENUBG2). */
static int32_t make_backdrop(SdJob *job, const char *dosName, SdImage *out, char *err)
{
    char path[1100];
    SdImage pic;
    if (Sd_FindNoCase(job->src.dosDir, dosName, path, sizeof path) != 0) {
        snprintf(err, 256, "%s is not in the PC game folder", dosName);
        return -1;
    }
    if (Sd_DosPictureLoad(path, &pic, err) != 0)
        return -1;
    int32_t rc = Sd_DosPictureScaleToAmiga(&pic, out, err);
    SdImage_Free(&pic);
    return rc;
}

/* The sprite, menu and font banks: the PC game's sprites laid out where the
 * Amiga's banks keep them, and the extra letters drawn into the font. */
static int32_t make_banks(SdJob *job, int32_t *done, int32_t total, char *err)
{
    SdImage banks[SD_BANK_COUNT];
    char glyphs[1100];
    if (Sd_DosToAmigaBanks(job->src.dosDir, banks, err) != 0)
        return -1;
    glyphs_path(job, glyphs, sizeof glyphs);
    int32_t rc = Sd_FontExtend(&banks[SD_BANK_CHARSET], glyphs, err);
    for (int32_t b = 0; b < SD_BANK_COUNT; b++) {
        char rel[64];
        if (rc == 0) {
            snprintf(rel, sizeof rel, "grafs_aga/%s.RAW", Sd_BankNames[b]);
            rc = write_raw(job, rel, &banks[b], err);
            tick(job, done, total);
        }
        SdImage_Free(&banks[b]);
    }
    return rc;
}

static int32_t step_aga_pc(SdJob *job, int32_t *done, int32_t total, char *err)
{
    SdImage menubg, menubg2, icons;
    if (make_banks(job, done, total, err) != 0)
        return -1;
    if (make_backdrop(job, "SWTITLE.256", &menubg, err) != 0)
        return -1;
    int32_t rc = write_raw(job, "grafs_aga/MENUBG.RAW", &menubg, err);
    tick(job, done, total);
    if (rc == 0 && (rc = make_backdrop(job, "STAD.256", &menubg2, err)) == 0) {
        rc = write_raw(job, "grafs_aga/MENUBG2.RAW", &menubg2, err);
        SdImage_Free(&menubg2);
        tick(job, done, total);
    }
    for (int32_t n = 1; n <= 6 && rc == 0; n++) {
        SdBuf map;
        char rel[64];
        if ((rc = Sd_DosPitchToAgaMap(job->src.dosDir, n, &map, err)) != 0)
            break;
        snprintf(rel, sizeof rel, "grafs_aga/SWCPICH%d.MAP", n);
        rc = write_packed(job, rel, &map, err);
        Sd_BufFree(&map);
        tick(job, done, total);
    }
    /* The credits backdrop: MENUBG with the project's own photos on it. */
    if (rc == 0) {
        char path[1100];
        Sd_PathJoin(path, sizeof path, job->resDir, "data/credits_icons.bmp");
        if ((rc = SdImage_ReadBmp(&icons, path, err)) == 0) {
            SdBuf raw;
            if ((rc = Sd_MakeCreditsAga(&menubg, &icons, &raw, err)) == 0) {
                rc = write_packed(job, "grafs_aga/CREDITS.RAW", &raw, err);
                Sd_BufFree(&raw);
            }
            SdImage_Free(&icons);
        }
        tick(job, done, total);
    }
    SdImage_Free(&menubg);
    if (rc == 0)
        logf_(job, "AGA from the PC game: %d files", AGA_PC_COUNT);
    return rc;
}

static int32_t step_aga_amiga(SdJob *job, int32_t *done, int32_t total, char *err)
{
    int32_t count = (int32_t)(sizeof kAmigaPictures / sizeof kAmigaPictures[0]);
    for (int32_t i = 0; i < count; i++) {
        char rel[64];
        SdBuf raw;
        SdImage img;
        snprintf(rel, sizeof rel, "grafs/%s.RAW", kAmigaPictures[i]);
        if (SdSources_ReadAmiga(&job->src, rel, &raw, err) != 0)
            return -1;
        int32_t rc = Sd_AmigaPictureTo256(kAmigaPictures[i], &raw, &img, err);
        Sd_BufFree(&raw);
        if (rc != 0)
            return -1;
        snprintf(rel, sizeof rel, "grafs_aga/%s.RAW", kAmigaPictures[i]);
        rc = write_raw(job, rel, &img, err);
        SdImage_Free(&img);
        if (rc != 0)
            return -1;
        tick(job, done, total);
    }
    logf_(job, "AGA from the Amiga: %d files", count);
    return 0;
}

/* The team files: the "data/" entries of the Amiga list. The PC game holds the
 * same files, unpacked, plus a few countries the Amiga's never had -- left out,
 * or the game would load them as extra countries. */
static int32_t count_teams(void)
{
    int32_t n = 0;
    for (int32_t i = 0; g_sdAmigaCopy[i]; i++)
        n += strncmp(g_sdAmigaCopy[i], "data/", 5) == 0;
    return n;
}

static int32_t step_teams(SdJob *job, int32_t *done, int32_t total, char *err)
{
    int32_t fromAmiga = SdSources_HaveAmiga(&job->src);
    for (int32_t i = 0; g_sdAmigaCopy[i]; i++) {
        const char *rel = g_sdAmigaCopy[i];
        if (strncmp(rel, "data/", 5) != 0)
            continue;
        SdBuf raw, json;
        char dosRel[64], name[64], out[96];
        snprintf(dosRel, sizeof dosRel, "DATA/%s", rel + 5);
        int32_t rc = fromAmiga ? SdSources_ReadAmiga(&job->src, rel, &raw, err)
                               : SdSources_ReadDos(&job->src, dosRel, &raw, err);
        if (rc != 0)
            return -1;
        /* Without the disks the game's own data/ comes from the PC files too,
         * packed the way the Amiga's are -- which gives the Amiga's files back
         * byte for byte: team files method 1, the cup pools and the player pool
         * method 2. */
        if (!fromAmiga) {
            SdBuf packed = { NULL, 0 };
            int32_t method = strncmp(rel + 5, "TEAM.", 5) == 0 ? 1 : 2;
            rc = Sd_RncPack(&raw, method, &packed, err);
            if (rc == 0)
                rc = write_out(job, rel, &packed, err);
            Sd_BufFree(&packed);
            if (rc != 0) {
                Sd_BufFree(&raw);
                return -1;
            }
        }
        rc = Sd_TeamFileToJson(rel + 5, &raw, &json, err);
        Sd_BufFree(&raw);
        if (rc != 0)
            return -1;
        Sd_TeamJsonName(rel + 5, name, sizeof name);
        snprintf(out, sizeof out, "data_json/teams/%s", name);
        rc = write_out(job, out, &json, err);
        Sd_BufFree(&json);
        if (rc != 0)
            return -1;
        tick(job, done, total);
    }
    logf_(job, "Teams: %d files, from the %s", count_teams(), fromAmiga ? "Amiga disks" : "PC game");
    return 0;
}

/* The Jaguar redrew the gameplay sprites at 256 colours in the Amiga's bank
 * layout; what it lacks comes from the PC game, and the goal celebration from
 * the project's own drawings in setup/. */
static int32_t step_jaguar(SdJob *job, int32_t *done, int32_t total, char *err)
{
    SdJagFile files[SD_JAG_FILE_COUNT];
    uint32_t celebrations = 0;
    char setup[1100];
    Sd_PathJoin(setup, sizeof setup, job->resDir, "setup");
    if (Sd_JagBuild(job->src.jagRom, job->src.dosDir, setup, files, &celebrations, err) != 0)
        return -1;
    int32_t rc = 0;
    for (int32_t i = 0; i < SD_JAG_FILE_COUNT && rc == 0; i++) {
        char rel[96];
        snprintf(rel, sizeof rel, "grafs_jag/%.40s", files[i].name);
        rc = write_out(job, rel, &files[i].data, err);
        tick(job, done, total);
    }
    Sd_JagFilesFree(files);
    if (rc == 0)
        logf_(job, "Jaguar: %d files", SD_JAG_FILE_COUNT);
    return rc;
}

/* SWOS 2020's pitches as its launcher left them in its game folder -- the set the
 * player picked there -- and the AGA Amiga's maps of them, P2020_n.MAP. Those are
 * written unpacked: the game reads them straight into the pitch buffer, as they
 * do not fit the screen buffer even packed (src/pitch.c loadPitchMap2020). */
static int32_t step_swos2020(SdJob *job, int32_t *done, int32_t total, char *err)
{
    for (int32_t i = 0; g_sd2020Copy[i].dest; i++) {
        char path[1100];
        SdBuf b;
        if (Sd_FindNoCase(job->src.s2020Dir, g_sd2020Copy[i].src, path, sizeof path) != 0) {
            snprintf(err, 256, "%s is not in SWOS 2020's game folder", g_sd2020Copy[i].src);
            return -1;
        }
        if (Sd_ReadFile(path, &b, err) != 0)
            return -1;
        int32_t rc = write_out(job, g_sd2020Copy[i].dest, &b, err);
        Sd_BufFree(&b);
        if (rc != 0)
            return -1;
        tick(job, done, total);
    }
    for (int32_t n = 1; n <= 6; n++) {
        SdBuf map;
        char rel[64];
        if (Sd_DosPitchToAgaMap(job->src.s2020Dir, n, &map, err) != 0)
            return -1;
        snprintf(rel, sizeof rel, "grafs_aga/P2020_%d.MAP", n);
        int32_t rc = write_out(job, rel, &map, err);
        Sd_BufFree(&map);
        if (rc != 0)
            return -1;
        tick(job, done, total);
    }
    logf_(job, "SWOS 2020: %d files", count_2020() + 6);
    return 0;
}

int32_t SwosData_Run(SdJob *job, char *err)
{
    SdStepInfo info[SD_STEP_COUNT];
    SwosData_Plan(job, info);

    int32_t total = 0, done = 0;
    if (info[SD_STEP_AMIGA_FILES].ready)
        total += count_amiga() + 1;
    if (info[SD_STEP_DOS_FILES].ready)
        total += count_dos();
    if (info[SD_STEP_AGA_PC].ready)
        total += AGA_PC_COUNT;
    if (info[SD_STEP_AGA_AMIGA].ready)
        total += (int32_t)(sizeof kAmigaPictures / sizeof kAmigaPictures[0]);
    if (info[SD_STEP_TEAMS].ready)
        total += count_teams();
    if (info[SD_STEP_JAGUAR].ready)
        total += SD_JAG_FILE_COUNT;
    if (info[SD_STEP_SWOS2020].ready)
        total += count_2020() + 6;

    if (SdSources_Open(&job->src, err) != 0)
        return -1;
    int32_t rc = 0;
    for (int32_t s = 0; s < SD_STEP_COUNT && rc == 0; s++) {
        if (!info[s].ready) {
            logf_(job, "Skipped: %s (needs %s)", info[s].name, info[s].needs);
            continue;
        }
        switch ((SdStep)s) {
        case SD_STEP_AMIGA_FILES: rc = step_amiga_files(job, &done, total, err); break;
        case SD_STEP_DOS_FILES:   rc = step_dos_files(job, &done, total, err); break;
        case SD_STEP_AGA_PC:      rc = step_aga_pc(job, &done, total, err); break;
        case SD_STEP_AGA_AMIGA:   rc = step_aga_amiga(job, &done, total, err); break;
        case SD_STEP_TEAMS:       rc = step_teams(job, &done, total, err); break;
        case SD_STEP_JAGUAR:      rc = step_jaguar(job, &done, total, err); break;
        case SD_STEP_SWOS2020:    rc = step_swos2020(job, &done, total, err); break;
        default: break;
        }
    }
    SdSources_Close(&job->src);
    return rc;
}

void SwosData_DefaultAssetsDir(char *out, uint32_t cap)
{
    const char *env = getenv("SWOS_ASSETS");
    if (env && *env) {
        snprintf(out, cap, "%s", env);
        return;
    }
#ifdef _WIN32
    const char *profile = getenv("USERPROFILE");
    snprintf(out, cap, "%s/Documents/SWOS/assets", profile ? profile : ".");
    for (char *p = out; *p; p++)
        if (*p == '\\')
            *p = '/';
#elif defined(__APPLE__)
    const char *home = getenv("HOME");
    snprintf(out, cap, "%s/Library/Application Support/SWOS/assets", home ? home : ".");
#else
    const char *home = getenv("HOME");
    snprintf(out, cap, "%s/.swos/assets", home ? home : ".");
#endif
}

/* Copy every file under src/rel to dst/rel. Returns how many, or -1. */
static int32_t copy_tree(const char *src, const char *dst, const char *rel, char *err)
{
    char dir[1100];
    int32_t count = 0;
    Sd_PathJoin(dir, sizeof dir, src, rel);
#ifdef _WIN32
    char pattern[1100];
    Sd_PathJoin(pattern, sizeof pattern, dir, "*");
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    do {
        const char *name = fd.cFileName;
        int32_t isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    DIR *d = opendir(dir);
    if (!d)
        return 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        const char *name = e->d_name;
        char full[1100];
        struct stat st;
        Sd_PathJoin(full, sizeof full, dir, name);
        int32_t isDir = stat(full, &st) == 0 && S_ISDIR(st.st_mode);
#endif
        if (name[0] == '.')
            continue;
        char sub[1100];
        Sd_PathJoin(sub, sizeof sub, rel, name);
        if (isDir) {
            int32_t n = copy_tree(src, dst, sub, err);
            if (n < 0) {
                count = -1;
                break;
            }
            count += n;
        } else {
            char from[1100], to[1100];
            SdBuf b;
            Sd_PathJoin(from, sizeof from, src, sub);
            Sd_PathJoin(to, sizeof to, dst, sub);
            if (Sd_ReadFile(from, &b, err) != 0 || Sd_WriteFile(to, b.data, b.size, err) != 0) {
                Sd_BufFree(&b);
                count = -1;
                break;
            }
            Sd_BufFree(&b);
            count++;
        }
#ifdef _WIN32
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    }
    closedir(d);
#endif
    return count;
}

int32_t SwosData_FillAmigaDrawer(const char *assetsDir, const char *drawerDir,
                                 void (*log)(void *user, const char *line), void *user,
                                 char *err)
{
    static const char *const kTrees[] = { "data", "grafs", "grafs_aga", "sound" };
    char probe[1100];
    if (Sd_FindNoCase(drawerDir, "SWOS2", probe, sizeof probe) != 0) {
        snprintf(err, 256, "%s is not a SWOS drawer (no SWOS2 in it): unpack SWOS-Amiga.zip first",
                 drawerDir);
        return -1;
    }
    if (Sd_FindNoCase(assetsDir, "grafs/MENUS.RAW", probe, sizeof probe) != 0 ||
        Sd_FindNoCase(assetsDir, "grafs_aga/MENUS.RAW", probe, sizeof probe) != 0) {
        snprintf(err, 256, "the game data is not complete: the Amiga version needs both the "
                           "Amiga disks and the PC game");
        return -1;
    }
    for (uint32_t i = 0; i < sizeof kTrees / sizeof kTrees[0]; i++) {
        int32_t n = copy_tree(assetsDir, drawerDir, kTrees[i], err);
        if (n < 0)
            return -1;
        if (log) {
            char line[256];
            snprintf(line, sizeof line, "Amiga drawer: %s/ (%d files)", kTrees[i], n);
            log(user, line);
        }
    }
    /* The program writes saves there but will not create the folder. */
    Sd_PathJoin(probe, sizeof probe, drawerDir, "save");
    Sd_MakeDirs(probe);
    return 0;
}
