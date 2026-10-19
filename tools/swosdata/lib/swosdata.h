/*
 * swosdata.h -- build OpenSWOS's game data from the player's own copy of the game.
 *
 * OpenSWOS ships no original game data. The player points this at what they
 * own -- the Amiga floppies, the PC version (GOG), the Jaguar cartridge, SWOS 2020 -- and
 * it writes the trees the game reads into an assets folder, laid out as the
 * game's bin/ folder is: data/, grafs/, grafs_aga/, grafs_dos/, grafs_jag/,
 * grafs_2020/, sound/, sound_dos/, data_json/teams/.
 *
 * Each step needs some of the sources and is skipped, with a note, without
 * them; SwosData_Plan says beforehand what will and will not be made.
 */
#ifndef SWOSDATA_H
#define SWOSDATA_H

#include <stdint.h>
#include "sd_sources.h"

typedef struct {
    SdSources   src;
    char        outDir[1024];   /* the assets folder */
    char        resDir[1024];   /* the game's own files, laid out as bin/ is:
                                 * data/credits_icons.bmp, setup/glyphs.txt, ... */
    /* Called for every line worth showing, and for progress. Either may be NULL. */
    void      (*log)(void *user, const char *line);
    void      (*progress)(void *user, int32_t done, int32_t total);
    void       *user;
} SdJob;

typedef enum {
    SD_STEP_AMIGA_FILES,    /* data/, grafs/, sound/ from the Amiga disks */
    SD_STEP_DOS_FILES,      /* grafs_dos/, sound_dos/ from the PC game */
    SD_STEP_AGA_PC,         /* grafs_aga/: backdrops, pitches, credits from the PC game */
    SD_STEP_AGA_AMIGA,      /* grafs_aga/: the loading and disk screens, from the Amiga */
    SD_STEP_TEAMS,          /* data_json/teams/ (and data/ without the Amiga disks) */
    SD_STEP_JAGUAR,         /* grafs_jag/: the Jaguar's sprites, gaps from the PC game */
    SD_STEP_SWOS2020,       /* grafs_2020/ and grafs_aga/P2020_n.MAP: SWOS 2020's pitches */
    SD_STEP_COUNT
} SdStep;

typedef struct {
    const char *name;       /* what it makes, for people */
    int32_t     ready;      /* its sources are there */
    const char *needs;      /* what it needs, for people */
} SdStepInfo;

/* What each step will do with the sources in `job`. */
void    SwosData_Plan(const SdJob *job, SdStepInfo info[SD_STEP_COUNT]);

/* The assets folder the game reads (src/swos_io.c swos_assets_dir): $SWOS_ASSETS,
 * else "assets" in the per-user game folder -- Documents/SWOS on Windows,
 * ~/Library/Application Support/SWOS on macOS, ~/.swos elsewhere. */
void    SwosData_DefaultAssetsDir(char *out, uint32_t cap);

/* Run every step whose sources are there. 0 when everything that ran
 * succeeded; otherwise `err` (256 bytes) names the first failure. */
int32_t SwosData_Run(SdJob *job, char *err);

/* The Amiga version: copy what the Amiga binary reads (data/, grafs/,
 * grafs_aga/, sound/) from an assets folder SwosData_Run filled into a SWOS
 * drawer unpacked from SWOS-Amiga.zip, which ships the program but no game
 * data. The drawer is recognised by its SWOS2 program. Needs the Amiga disks'
 * files in the assets folder (grafs/, sound/). 0 on success. */
int32_t SwosData_FillAmigaDrawer(const char *assetsDir, const char *drawerDir,
                                 void (*log)(void *user, const char *line), void *user,
                                 char *err);

#endif
