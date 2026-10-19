/*
 * sd_sources.h -- the player's own copy of the game, wherever it is.
 *
 * Four kinds of source, any combination:
 *   - the Amiga version: its two floppy images (.adf). Told apart by their
 *     volume names, "SWOS" (disk 1) and "SWOS2" (disk 2), never by file name.
 *   - the PC version: the installed game folder (GOG's, or any copy of one) --
 *     found by SPRITE.DAT, any case.
 *   - the Atari Jaguar version: the cartridge image (.jag, .j64, .rom or .bin, 2 MB).
 *   - SWOS 2020: its install folder, for the pitches its launcher has put in the
 *     game's own folder (swos/) -- whichever set the player picked. Found by the
 *     program in that folder, swos-port-Release.exe; that folder also looks like
 *     the PC version's (SPRITE.DAT, DATA/TEAM.000), and must never be taken for it.
 *
 * The caller names folders to search (the setup app's "Choose a folder"),
 * or a file or folder for one source; SdSources_Scan fills in what it finds.
 */
#ifndef SWOSDATA_SOURCES_H
#define SWOSDATA_SOURCES_H

#include <stdint.h>
#include "adf.h"
#include "sg_util.h"

typedef struct {
    char  adf1[1024];       /* Amiga disk 1 (volume "SWOS"), "" when not found */
    char  adf2[1024];       /* Amiga disk 2 (volume "SWOS2") */
    char  dosDir[1024];     /* the PC game folder */
    char  jagRom[1024];     /* the Jaguar cartridge image */
    char  s2020Dir[1024];   /* SWOS 2020's game folder: <install>/swos */
    Adf  *disk1, *disk2;    /* open while SdSources_Open .. Close */
} SdSources;

/* Look through `dir` and every folder inside it, however deep, for any of the
 * four. A source already set is left as it is. Links are not followed, hidden
 * and system folders are skipped, a game folder once recognised is not looked
 * inside, and the search ends when all four are found. Returns how many are
 * known in all. */
int32_t SdSources_Scan(SdSources *s, const char *dir);

/* The same, telling `progress` (may be NULL) about every folder as it goes:
 * how many so far and which. A non-zero return stops the search; then -1. */
typedef int32_t (*SdScanProgress)(void *user, int32_t folders, const char *dir);
int32_t SdSources_ScanEx(SdSources *s, const char *dir, SdScanProgress progress, void *user);

/* Check one file the user picked: an .adf (either disk), the cartridge, a file
 * inside the PC game folder (its folder is taken), or a SWOS 2020 install (its
 * folder, its swos/ folder or a file in either). 0 when recognised. */
int32_t SdSources_AddFile(SdSources *s, const char *path);

int32_t SdSources_HaveAmiga(const SdSources *s);   /* both disks */
int32_t SdSources_HaveDos(const SdSources *s);
int32_t SdSources_HaveJaguar(const SdSources *s);
int32_t SdSources_HaveSwos2020(const SdSources *s);

/* Open the disks for reading. 0 on success. */
int32_t SdSources_Open(SdSources *s, char *err);
void    SdSources_Close(SdSources *s);

/* Read a file from the Amiga disks: `rel` as on the disk, e.g. "grafs/MENUS.RAW";
 * disk 1 is tried first, then disk 2. */
int32_t SdSources_ReadAmiga(SdSources *s, const char *rel, SgBuf *out, char *err);

/* Read a file from the PC game folder, any case. */
int32_t SdSources_ReadDos(SdSources *s, const char *rel, SgBuf *out, char *err);

#endif
