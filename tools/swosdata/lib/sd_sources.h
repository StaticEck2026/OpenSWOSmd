/*
 * sd_sources.h -- the player's own copy of the game, wherever it is.
 *
 * Three kinds of source, any combination:
 *   - the Amiga version: its two floppy images (.adf). Told apart by their
 *     volume names, "SWOS" (disk 1) and "SWOS2" (disk 2), never by file name.
 *   - the PC version: the installed game folder (GOG's, or any copy of one) --
 *     found by SPRITE.DAT, any case.
 *   - the Atari Jaguar version: the cartridge image (.jag, .j64, .rom or .bin, 2 MB).
 *
 * The caller may name each one, or name folders to search (the setup app's
 * "browse to a folder" does that); SdSources_Scan fills in what it finds.
 */
#ifndef SWOSDATA_SOURCES_H
#define SWOSDATA_SOURCES_H

#include <stdint.h>
#include "adf.h"
#include "sd_util.h"

typedef struct {
    char  adf1[1024];       /* Amiga disk 1 (volume "SWOS"), "" when not found */
    char  adf2[1024];       /* Amiga disk 2 (volume "SWOS2") */
    char  dosDir[1024];     /* the PC game folder */
    char  jagRom[1024];     /* the Jaguar cartridge image */
    Adf  *disk1, *disk2;    /* open while SdSources_Open .. Close */
} SdSources;

/* Look in `dir` (and one level of folders below it) for any of the three. A
 * source already set is left as it is. Returns how many were found in all. */
int32_t SdSources_Scan(SdSources *s, const char *dir);

/* Check one file the user picked: an .adf (either disk), the cartridge, or a
 * file inside the PC game folder (its folder is taken). 0 when recognised. */
int32_t SdSources_AddFile(SdSources *s, const char *path);

int32_t SdSources_HaveAmiga(const SdSources *s);   /* both disks */
int32_t SdSources_HaveDos(const SdSources *s);
int32_t SdSources_HaveJaguar(const SdSources *s);

/* Open the disks for reading. 0 on success. */
int32_t SdSources_Open(SdSources *s, char *err);
void    SdSources_Close(SdSources *s);

/* Read a file from the Amiga disks: `rel` as on the disk, e.g. "grafs/MENUS.RAW";
 * disk 1 is tried first, then disk 2. */
int32_t SdSources_ReadAmiga(SdSources *s, const char *rel, SdBuf *out, char *err);

/* Read a file from the PC game folder, any case. */
int32_t SdSources_ReadDos(SdSources *s, const char *rel, SdBuf *out, char *err);

#endif
