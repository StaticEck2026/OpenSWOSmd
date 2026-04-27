/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "pitch.h"
#include "game_types.h"
#include "display.h"
#include "endian_helpers.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* ---- External globals (all live in SWOS2.s) ---- */
extern uint32_t swcPitchMapFiles[];     /* char *[6] -- pitch map filenames */
extern uint8_t pitchDatBuffer[];        /* 9240 bytes (2310 longs) */
extern uint8_t currentMenu[];           /* BSS base for pointer relocation */
extern uint16_t panicReasonCode;
extern FixedPoint cameraX;
extern FixedPoint cameraY;
extern uint16_t cameraXMod320;
extern uint16_t cameraYMod320;
extern TeamGame topTeamInGame;
extern TeamGame bottomTeamInGame;
extern uint16_t fansSidesSwapped;

/* ---- External functions ---- */
extern int16_t _LoadFileEx(uint8_t *filename, uint8_t *dest, uint32_t *outFileSize);
extern void _PanicHaltLoop(void) __attribute__((noreturn));

/* ---- Constants ---- */
#define PITCH_POINTER_COUNT     2310    /* 9240 / 4 */
#define FAN_PTR_TABLE_SIZE      21      /* 21 pointers per fan group */
#define FAN_PTR_TABLE_OFFSET    0x54    /* second group starts here */

/* pitchFansConvertTable: color remap table for fan sprites.
 * Indices 10,11 get replaced with team kit colors. */
static uint16_t pitchFansConvertTable[16] = {
    0, 1, 2, 3, 4, 5, 6, 7,
    8, 9, 0xFFFF, 0xFFFF, 12, 13, 14, 15
};

/* ================================================================
 * AdjustFanColor  (SWOS2.s line 7737)
 *
 * Recolors a 4-plane interleaved sprite (16 pixels wide, 16 rows)
 * by replacing each pixel's 4-bit color index via convTable.
 *
 * Sprite layout: NUM_PLANES planes interleaved, each plane is 1 word per row.
 * Row stride = NUM_PLANES * 2 bytes.
 *
 * Parameters (original asm swaps a0/a1):
 *   destPattern = pointer to planar sprite data
 *   convTable   = color remap table: old_index → new_index
 * ================================================================ */
__attribute__((used, externally_visible))
void _AdjustFanColor(uint8_t *destPattern, const uint16_t *convTable)
{
#ifdef SWOS_SDL_BUILD
    /* Chunky form: destPattern is a 16×16 8bpp tile (256 bytes) — our
     * load-time decode in _FixupAndColorPitchFile converts fan tiles
     * to the same format as pitch tiles. Color remap is a trivial LUT;
     * skip transparent pixels (palette index 0) to preserve see-through. */
    for (int i = 0; i < 256; i++) {
        uint8_t c = destPattern[i];
        if (c == 0) continue;
        destPattern[i] = (uint8_t)convTable[c];
    }
#else
    int16_t row, pixel, pl;
    uint16_t *plane0 = (uint16_t *)destPattern;

    for (row = 15; row >= 0; row--)
    {
        uint16_t w[NUM_PLANES];
        for (pl = 0; pl < NUM_PLANES; pl++)
            w[pl] = plane0[pl];

        for (pixel = 15; pixel >= 0; pixel--)
        {
            uint16_t colorIdx = 0;

            /* Extract N-bit color from MSBs of each plane */
            for (pl = NUM_PLANES - 1; pl >= 0; pl--) {
                colorIdx = (uint16_t)((colorIdx << 1) | ((w[pl] >> 15) & 1));
                w[pl] <<= 1;
            }

            /* Look up new color */
            uint16_t newColor = convTable[colorIdx];

            /* Insert new color bits into LSBs */
            for (pl = 0; pl < NUM_PLANES; pl++) {
                if (newColor & (1 << pl))
                    w[pl] |= 1;
            }
        }

        for (pl = 0; pl < NUM_PLANES; pl++)
            plane0[pl] = w[pl];
        plane0 += NUM_PLANES;
    }
#endif
}

/* ================================================================
 * LoadPitchMap  (SWOS2.s line 5557)
 *
 * Loads a pitch map file by index into pitchDatBuffer.
 * Panics if file exceeds maximum size.
 * ================================================================ */
__attribute__((used, externally_visible))
void _LoadPitchMap(uint16_t mapIndex)
{
    uint8_t *filename = (uint8_t *)swcPitchMapFiles[mapIndex];
    uint32_t fileSize = 0;
    int16_t result;

    result = _LoadFileEx(filename, pitchDatBuffer, &fileSize);
    (void)result;

    if (fileSize > PITCHFILE_MAX_SIZE)
    {
        panicReasonCode = 3;
        _PanicHaltLoop();
    }
}

/* ================================================================
 * FixupAndColorPitchFile  (SWOS2.s line 5572)
 *
 * 1. Relocates all pitch pointer entries by adding BSS base address.
 * 2. Resets camera position.
 * 3. Recolors fan sprites for both teams using kit colors.
 * ================================================================ */
__attribute__((used, externally_visible))
void _FixupAndColorPitchFile(void)
{
    uint32_t *pitchPtrs;
    int16_t i;
#ifdef SWOS_SDL_BUILD
    /* SDL: the entire pitch file is loaded into pitchDatBuffer. The
     * pointer-table entries are byte offsets from the start of the
     * tile-data region, which begins right after the 9240-byte
     * pointer table. */
    uint32_t baseAddr = (uint32_t)(uintptr_t)(pitchDatBuffer + PITCH_POINTER_COUNT * 4);
#else
    uint32_t baseAddr = (uint32_t)currentMenu;
#endif
    uint32_t *fanPtrsA;  /* first group of fan sprite pointers */
    uint32_t *fanPtrsB;  /* second group */
    TeamGame *teamData;
    uint16_t kitPrimary, kitSecondary, kitTertiary;

    /* Step 1: Relocate all pointers in pitchDatBuffer.
     * SWOS Amiga pitch files store each mapping entry as a 32-bit
     * big-endian byte offset into the tile-data region. On x86 LE
     * a native uint32 load would byte-swap the value (offset 0x80
     * becomes 0x80000000), so read each entry via BE_READ_U32. */
    pitchPtrs = (uint32_t *)pitchDatBuffer;
    for (i = PITCH_POINTER_COUNT - 1; i >= 0; i--)
    {
#ifdef SWOS_SDL_BUILD
        uint32_t offsetBE = BE_READ_U32((const uint8_t *)&pitchPtrs[i]);
        pitchPtrs[i] = offsetBE + baseAddr;
#else
        pitchPtrs[i] += baseAddr;
#endif
    }

#ifdef SWOS_SDL_BUILD
    /* Step 1b (SDL only): decode each 128-byte 4-bitplane tile to a
     * 256-byte 8bpp chunky tile. Chunky tiles live in pitchChunkyBuffer,
     * indexed by pointer slot. After this pass, pitchPtrs[i] points at
     * chunky bytes so the renderer is a plain memcpy.
     *
     * Bitplane tile layout: 16 scanlines × (4 planes × 2 bytes) = 128 bytes.
     * Each scanline is `plane0_word, plane1_word, plane2_word, plane3_word`
     * packing 16 horizontal pixels as bit N of each plane word. */
    {
        extern uint8_t pitchChunkyBuffer[];
        uint8_t *bufStart = pitchDatBuffer;
        uint8_t *bufEnd   = pitchDatBuffer + 64 * 1024;
        for (i = 0; i < PITCH_POINTER_COUNT; i++) {
            uint8_t *bitplane = (uint8_t *)(uintptr_t)pitchPtrs[i];
            uint8_t *chunky   = pitchChunkyBuffer + (size_t)i * 256;
            /* Guard against bogus pointers (shouldn't happen, but the
             * file could be malformed or the stub still indexed). */
            if (bitplane < bufStart || bitplane + 128 > bufEnd) {
                for (int k = 0; k < 256; k++) chunky[k] = 0;
            } else {
                for (int row = 0; row < 16; row++) {
                    const uint8_t *rowSrc = bitplane + row * 8;
                    /* Read 4 plane words (big-endian from file) */
                    uint16_t p0 = ((uint16_t)rowSrc[0] << 8) | rowSrc[1];
                    uint16_t p1 = ((uint16_t)rowSrc[2] << 8) | rowSrc[3];
                    uint16_t p2 = ((uint16_t)rowSrc[4] << 8) | rowSrc[5];
                    uint16_t p3 = ((uint16_t)rowSrc[6] << 8) | rowSrc[7];
                    uint8_t *dst = chunky + row * 16;
                    for (int px = 0; px < 16; px++) {
                        int bit = 15 - px;
                        uint8_t idx = (uint8_t)(((p0 >> bit) & 1)      |
                                                (((p1 >> bit) & 1) << 1) |
                                                (((p2 >> bit) & 1) << 2) |
                                                (((p3 >> bit) & 1) << 3));
                        dst[px] = idx;
                    }
                }
            }
            pitchPtrs[i] = (uint32_t)(uintptr_t)chunky;
        }
    }
#endif

    /* Step 2: Reset camera */
    cameraX.s.i = 0;
    cameraY.s.i = 0;
    cameraXMod320 = 0;
    cameraYMod320 = 0;

    /* Step 3: Set up fan pointer groups */
    fanPtrsA = (uint32_t *)pitchDatBuffer;                          /* offset 0 */
    fanPtrsB = (uint32_t *)(pitchDatBuffer + FAN_PTR_TABLE_OFFSET); /* offset 0x54 */

    fansSidesSwapped = 0;
    /* If topTeamInGame's team name first byte is odd, swap groups */
    if (topTeamInGame.teamName[0] & 1)
    {
        uint32_t *tmp = fanPtrsA;
        fanPtrsA = fanPtrsB;
        fanPtrsB = tmp;
        fansSidesSwapped = 1;
    }

    /* Step 4: Recolor top team fans (21 sprites) */
    teamData = &topTeamInGame;
    kitPrimary = (uint16_t)teamData->secShirtCol;
    kitSecondary = (uint16_t)teamData->secStripesCol;
    kitTertiary = (uint16_t)teamData->secShortsCol;

    pitchFansConvertTable[11] = kitPrimary;   /* offset 0x16 */
    if (kitSecondary != kitPrimary)
        pitchFansConvertTable[10] = kitSecondary; /* offset 0x14 */
    else
        pitchFansConvertTable[10] = kitTertiary;

    for (i = FAN_PTR_TABLE_SIZE - 1; i >= 0; i--)
    {
        _AdjustFanColor((uint8_t *)fanPtrsB[i], pitchFansConvertTable);
    }

    /* Step 5: Recolor bottom team fans (21 sprites) */
    teamData = &bottomTeamInGame;
    kitPrimary = (uint16_t)teamData->secShirtCol;
    kitSecondary = (uint16_t)teamData->secStripesCol;
    kitTertiary = (uint16_t)teamData->secShortsCol;

    pitchFansConvertTable[11] = kitPrimary;
    if (kitSecondary != kitPrimary)
        pitchFansConvertTable[10] = kitSecondary;
    else
        pitchFansConvertTable[10] = kitTertiary;

    for (i = 20; i >= 0; i--)
    {
        _AdjustFanColor((uint8_t *)fanPtrsA[i], pitchFansConvertTable);
    }
}

#ifdef SWOS_SDL_BUILD
/* ================================================================
 * _DrawChunkyPitch (SDL only)
 *
 * Whole-visible-pitch redraw from the chunky tile bank. Replaces the
 * Amiga incremental scroll blits (_Scroll{X,Y}UpdateRowsAndBlit, now
 * no-op on SDL).
 *
 * Layout (see AmigaTools.cs ReadPitchMap):
 *   - 42 cols × 55 rows = 2310 tile mappings indexed from pointer 0
 *   - Each tile is 16×16 palette-index bytes (256 bytes chunky)
 *
 * Camera position is in pixels (cameraX.s.i / cameraY.s.i). We pad by one
 * tile in each direction so tiles partially on-screen render fully.
 * ================================================================ */
/* Layout (per AmigaTools.cs): the pitch map is a flat 42-col × 55-row
 * grid of 16×16 tile mappings — every pointer in pitchDatBuffer is a
 * real tile entry, indexed from row 0. The first 42 happen to reference
 * fan-sprite tiles that _AdjustFanColor recolors at load, but that
 * shares tile *data* across mapping entries, not a separate pointer
 * prefix. Rendering must start at index 0 so pitch world Y=0 lines up
 * with overlay sprites that use the same coordinate space. */
#define PITCH_PTR_START     0
#define PITCH_GRID_COLS     42
#define PITCH_GRID_ROWS     55
#define PITCH_TILE_SIZE     16

extern uint8_t pitchDatBuffer[];
extern volatile uint32_t frontbufferBasePtr;

__attribute__((used, externally_visible))
void _DrawChunkyPitch(void)
{
    uint32_t *pitchPtrs = (uint32_t *)pitchDatBuffer;
    uint8_t  *fb        = (uint8_t *)(uintptr_t)frontbufferBasePtr;
    if (!fb) return;

    const int fbW = FRONTBUFFER_ROW_STRIDE_BYTES;   /* SDL = SCREEN_WIDTH = 320 */
    const int fbH = SCREEN_HEIGHT;                  /* 200 */

    int startTileX = (int)cameraX.s.i / PITCH_TILE_SIZE;
    int startTileY = (int)cameraY.s.i / PITCH_TILE_SIZE;
    int offsetX    = (int)cameraX.s.i % PITCH_TILE_SIZE;
    int offsetY    = (int)cameraY.s.i % PITCH_TILE_SIZE;
    if (offsetX < 0) { offsetX += PITCH_TILE_SIZE; startTileX--; }
    if (offsetY < 0) { offsetY += PITCH_TILE_SIZE; startTileY--; }

    int tilesAcross = fbW / PITCH_TILE_SIZE + 2;
    int tilesDown   = fbH / PITCH_TILE_SIZE + 2;

    for (int ty = 0; ty < tilesDown; ty++) {
        int gridY = startTileY + ty;
        int screenY = ty * PITCH_TILE_SIZE - offsetY;
        for (int tx = 0; tx < tilesAcross; tx++) {
            int gridX = startTileX + tx;
            int screenX = tx * PITCH_TILE_SIZE - offsetX;

            /* Off-pitch area: paint the surround color (palette 0). */
            uint8_t *tile = (uint8_t *)0;
            if (gridX >= 0 && gridX < PITCH_GRID_COLS &&
                gridY >= 0 && gridY < PITCH_GRID_ROWS) {
                int ptrIdx = PITCH_PTR_START + gridY * PITCH_GRID_COLS + gridX;
                tile = (uint8_t *)(uintptr_t)pitchPtrs[ptrIdx];
            }

            for (int row = 0; row < PITCH_TILE_SIZE; row++) {
                int py = screenY + row;
                if (py < 0 || py >= fbH) continue;
                uint8_t *dstRow = fb + py * fbW;
                if (tile) {
                    const uint8_t *srcRow = tile + row * PITCH_TILE_SIZE;
                    for (int col = 0; col < PITCH_TILE_SIZE; col++) {
                        int px = screenX + col;
                        if (px < 0 || px >= fbW) continue;
                        dstRow[px] = srcRow[col];
                    }
                } else {
                    for (int col = 0; col < PITCH_TILE_SIZE; col++) {
                        int px = screenX + col;
                        if (px < 0 || px >= fbW) continue;
                        dstRow[px] = 0;
                    }
                }
            }
        }
    }
}
#endif
