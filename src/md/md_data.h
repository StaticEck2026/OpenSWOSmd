/* md_data.h -- the game data in ROM (made by scripts/md/make_md_data.py).
 *
 * MdImage: one sprite image as VDP sprites --
 *   u16 nTiles, u8 nPieces, u8 0, nPieces x MdPiece, then nTiles x 32 bytes of 4bpp
 *   tiles (each piece's tiles column-major, as the VDP reads a multi-tile sprite).
 * The engine's SpriteGraphics.sprData points at one.
 */
#ifndef MD_DATA_H
#define MD_DATA_H

#include <stdint.h>
#include "game_types.h"

typedef struct { int8_t dx, dy; uint8_t size; uint8_t tile; } MdPiece;
typedef struct { uint16_t nTiles; uint8_t nPieces, pad; MdPiece pieces[]; } MdImage;
#define MD_IMAGE_TILES(img) ((const uint8_t *)(img) + 4 + 4 * (img)->nPieces)

typedef struct {
    const uint8_t  *tiles;     /* nVdpTiles x 32 bytes, for VRAM from TILE_PITCH on */
    const uint16_t *cells;     /* per 16x16 tile: four nametable entries (TL, TR, BL, BR) */
    const uint16_t *map;       /* 42 x 55 16x16-tile indices */
    uint16_t nVdpTiles, nTiles16;
} MdPitch;

#define MD_NUM_IMAGES   1310
#define MD_KIT_FIRST    227
#define MD_KIT_LAST     833
#define MD_ADVERT_FIRST 113
#define MD_ADVERT_COUNT 24
#define MD_ADVERT_LINES 13
#define MD_SG_RAM_COUNT 25      /* make_md_data.py MUTABLE: 113..137 */

extern const SpriteGraphics md_sgRom[MD_NUM_IMAGES];
extern SpriteGraphics md_sgRam[];
extern const uint16_t md_sgRamIndex[];
extern const SpriteGraphics *const md_spriteTable[MD_NUM_IMAGES];
extern const uint8_t *const md_kitImage[3][MD_KIT_LAST - MD_KIT_FIRST];
extern const uint8_t *const md_advertImage[MD_ADVERT_COUNT][MD_ADVERT_LINES];
extern const MdPitch md_pitches[6];
extern const uint8_t md_teamsData[];
extern const uint16_t md_numTeams;

#endif
