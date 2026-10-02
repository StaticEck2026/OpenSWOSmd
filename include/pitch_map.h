/* pitch_map.h -- the pitch's PICTURE from a Tile Agent map (docs/PITCH_MAPS.md).
 *
 * `[graphics] pitch_maps = true` draws a DOS-tier pitch from
 * <graphics dir>/pitch<N>.tmj where one exists. The match is not played from it:
 * the crowd's slot table, its random picks and everything else simulated stay on
 * the game's own PITCHn.DAT/.BLK, so a match, a replay and a netplay peer are
 * bit for bit what they are with the setting off. The map decides what each
 * cell LOOKS like. SDL only. */
#ifndef PITCH_MAP_H
#define PITCH_MAP_H

#include <stdint.h>

void    PitchMap_SetEnabled(int32_t on);
int32_t PitchMap_Enabled(void);

/* Load (or reuse) the map for this pitch from `dir`; 1 when there is one. Called
 * by the DOS pitch loader after it has read the game's own files. */
int32_t PitchMap_Load(const char *dir, int32_t pitchNumber);
/* Bake the map's cells into tiles and give the fans their kits. Called at the
 * end of FixupAndColorPitchFile, when the engine has recoloured its own. */
void    PitchMap_Bake(void);
/* 1 while a baked map is what the pitch is drawn from. */
int32_t PitchMap_Active(void);
void    PitchMap_Drop(void);            /* this pitch is not drawn from a map */

/* The tile to draw for a cell, given the tile the ENGINE holds there (which says
 * whether a fan is sitting or standing). NULL = nothing there: the surround. */
const uint8_t *PitchMap_Tile(int32_t cell, const uint8_t *engineTile);

/* The pitch condition's palette entries from the map's `pitch-<condition>`
 * event: fills entry[] and rgb[] (8-bit) and answers how many, 0 when the map
 * says nothing about this condition. */
int32_t PitchMap_Condition(int32_t pitchType, uint8_t *entry, uint8_t (*rgb)[3], int32_t max);

/* pitch.c: the kit colours the engine gave a tile of its own bank, 0 if none. */
int32_t Pitch_FanTileKit(const uint8_t *tile, uint8_t *slot10, uint8_t *slot11);

#endif
