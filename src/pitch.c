/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "pitch.h"
#include "game_types.h"
#include "display.h"
#include "endian_helpers.h"
#include "swos_types.h"
#include "dos_mode.h"
#ifdef SWOS_SDL_BUILD
#include "dos_graphics.h"   /* DosGfx_ParsePitchMap, DOS_TILE_BYTES */
#include <stdio.h>          /* snprintf for the DOS pitch filenames */
#endif

/* ---- External globals (all live in SWOS2.s) ---- */
extern void *swcPitchMapFiles[];     /* char *[6] -- pitch map filenames */
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
extern int16_t LoadFileEx(uint8_t *filename, uint8_t *dest, uint32_t *outFileSize);
extern void PanicHaltLoop(void) __attribute__((noreturn));

/* ---- Constants ---- */
#define PITCH_POINTER_COUNT     2310    /* 9240 / 4 */
#define FAN_PTR_TABLE_SIZE      21      /* 21 pointers per fan group */
#define FAN_PTR_TABLE_OFFSET    0x54    /* second group starts here */

/* Per-slot full-width pointer to the slot's chunky tile buffer. The
 * 32-bit pitchDatBuffer slot can only hold the low 32 of a 64-bit
 * heap/BSS address; that's still usable for the ANIM offset math
 * (`*a0 - baseAddr` cancels the truncated-vs-truncated subtraction),
 * but any *dereference* of the slot value on 64-bit hands the blitter
 * an unmapped low-32 address. We populate this side table at the same
 * time we fill the slot, and read through it whenever we actually need
 * to fetch tile bytes. */
/* Non-static so animated_patterns.c can read it directly (declared
 * extern there). Same shape: index = slot, value = full chunky-tile
 * pointer (NULL if not yet decoded). */
uint8_t *s_pitchFullPtrs[PITCH_POINTER_COUNT];

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
void AdjustFanColor(uint8_t *destPattern, const uint16_t *convTable)
{
#ifdef SWOS_SDL_BUILD
    /* Chunky form: destPattern is a 16×16 8bpp tile (256 bytes) — our
     * load-time decode in FixupAndColorPitchFile converts fan tiles
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

#ifdef SWOS_SDL_BUILD
/* ================================================================
 * dos_load_pitch  (SDL + gDosGraphics only)
 *
 * Loads a DOS pitch from bin/grafs_dos. Two files:
 *   PITCHn.BLK — the tile bank: raw N × 256-byte 16×16 8bpp chunky
 *                tiles. This is already our internal format, so we
 *                load it straight into pitchChunkyBuffer (tile t at
 *                +t*256, matching the Amiga decode path and the
 *                addressing animated_patterns.c expects).
 *   PITCHn.DAT — the tile map: an array of little-endian u32 byte
 *                offsets into the bank (tileIndex = offset / 256).
 *
 * We synthesise the per-slot pointer table s_pitchFullPtrs[] that
 * DrawChunkyPitch consumes — exactly what FixupAndColorPitchFile's
 * Amiga bitplane relocate/decode would produce — then short-circuit
 * that fixup (see FixupAndColorPitchFile). DOS files are numbered
 * PITCH1..PITCH6; mapIndex is 0-based (swcPitchMapFiles order), so the
 * DOS number is mapIndex + 1.
 * ================================================================ */
extern uint8_t pitchChunkyBuffer[];
/* Direct asset dispatch (writes straight to dest, no frontbuffer staging).
 * ASSET_OP_INIT_SOURCE0 == 2 (see file_io.c / asset_loader.c). */
extern int16_t CAssetSys_DispatchGetSize(uint16_t opcode, uint16_t param,
                                          void *name, void *dest,
                                          uint32_t *outFileSize);
#define DOS_ASSET_OP_INIT_SOURCE0   2
/* pitchChunkyBuffer capacity in whole 16x16 tiles. */
#define DOS_PITCH_BANK_MAX_TILES    PITCH_POINTER_COUNT   /* 2310 → 591360 B */

static void dos_load_pitch(uint16_t mapIndex)
{
    char name[320];
    uint32_t blkSize = 0, datSize = 0;
    int pitchNum = (int)mapIndex + 1;          /* DOS PITCH1..PITCH6 */
    if (pitchNum < 1) pitchNum = 1;
    if (pitchNum > 6) pitchNum = 6;

    /* Tile bank — already 256-byte chunky tiles. Load via the DIRECT asset
     * dispatch, NOT LoadFileEx: LoadFileEx stages through the ~91 KB
     * frontbuffer and panics (0xE000) on anything larger. The SWOS-2020 .blk
     * banks are 190–545 KB, so they MUST bypass that staging. The direct
     * dispatch writes straight into pitchChunkyBuffer (591360 B = 2310 tiles);
     * the 2020 set's largest is pitch6 at 545536 B (2131 tiles), which fits. */
    snprintf(name, sizeof name, "%s/PITCH%d.BLK", gDosGraphicsDir, pitchNum);
    CAssetSys_DispatchGetSize(DOS_ASSET_OP_INIT_SOURCE0, 0, name,
                               pitchChunkyBuffer, &blkSize);

    /* Tile map — stage into pitchDatBuffer, then parse the LE offsets. */
    snprintf(name, sizeof name, "%s/PITCH%d.DAT", gDosGraphicsDir, pitchNum);
    LoadFileEx((uint8_t *)name, pitchDatBuffer, &datSize);

    static uint16_t tileMap[PITCH_POINTER_COUNT];
    int n = DosGfx_ParsePitchMap(pitchDatBuffer, datSize, tileMap,
                                 PITCH_POINTER_COUNT);

    uint32_t tileCount = blkSize / DOS_TILE_BYTES;
    /* Never point past the end of pitchChunkyBuffer, even if a future bank has
     * more tiles than the buffer holds (the 2020 set stays under this). */
    if (tileCount > DOS_PITCH_BANK_MAX_TILES) tileCount = DOS_PITCH_BANK_MAX_TILES;
    for (int i = 0; i < PITCH_POINTER_COUNT; i++) {
        if (i < n && (uint32_t)tileMap[i] < tileCount)
            s_pitchFullPtrs[i] = pitchChunkyBuffer +
                                 (size_t)tileMap[i] * DOS_TILE_BYTES;
        else
            s_pitchFullPtrs[i] = (uint8_t *)0;   /* unmapped → surround */
    }

    /* Mirror the Amiga SDL pitch fixup (Step 1b in FixupAndColorPitchFile):
     * also store each slot's chunky-tile pointer (truncated to 32 bits) in the
     * pitchDatBuffer slot itself. The crowd animation (DrawAnimatedPatterns /
     * ResetAnimatedPatternsForBothTeams) reads these slots as tile offsets
     * from pitchChunkyBuffer (`*slot - pitchChunkyBuffer == tileIdx*256`) to
     * toggle fan tiles between their static and animated (waving) variants. The
     * DOS tile bank stores those pairs at adjacent indices just like the Amiga,
     * so the same toggle works. Without this the slots would hold raw .DAT
     * bytes and the toggle math would read garbage. */
    uint32_t *slots = (uint32_t *)pitchDatBuffer;
    for (int i = 0; i < PITCH_POINTER_COUNT; i++)
        slots[i] = (uint32_t)(uintptr_t)s_pitchFullPtrs[i];
}
#endif /* SWOS_SDL_BUILD */

/* ================================================================
 * LoadPitchMap  (SWOS2.s line 5557)
 *
 * Loads a pitch map file by index into pitchDatBuffer.
 * Panics if file exceeds maximum size.
 * ================================================================ */
__attribute__((used, externally_visible))
void LoadPitchMap(uint16_t mapIndex)
{
#ifdef SWOS_SDL_BUILD
    if (gDosGraphics) {
        dos_load_pitch(mapIndex);
        return;
    }
#endif
    uint8_t *filename = (uint8_t *)swcPitchMapFiles[mapIndex];
    uint32_t fileSize = 0;
    int16_t result;

    result = LoadFileEx(filename, pitchDatBuffer, &fileSize);
    (void)result;

    if (fileSize > PITCHFILE_MAX_SIZE)
    {
        panicReasonCode = 3;
        PanicHaltLoop();
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
void FixupAndColorPitchFile(void)
{
    uint32_t *pitchPtrs;
    int16_t i;
    /* Pitch-file pointer-table entries are byte offsets from the start
     * of the tile-data region, which begins right after the 9240-byte
     * pointer table. The original Amiga used `currentMenu` here because
     * it aliased the same RAM block as pitchDatBuffer; our C build keeps
     * them separate, so we must compute the tile-data base from the
     * actual pitch buffer for both targets. */
    uint32_t baseAddr = (uint32_t)(uintptr_t)(pitchDatBuffer + PITCH_POINTER_COUNT * 4);
    TeamGame *teamData;
    uint16_t kitPrimary, kitSecondary, kitTertiary;

#ifdef SWOS_SDL_BUILD
    if (gDosGraphics) {
        /* DOS pitch: s_pitchFullPtrs was already built in LoadPitchMap
         * from the BLK tile bank + DAT tile map. There is no bitplane
         * pointer table to relocate or decode, and the Amiga fan-kit
         * recolor (AdjustFanColor) doesn't apply to 256-colour DOS tiles
         * — it indexes a 16-entry LUT with pixel values that can be 0..255.
         * Just reset the camera and skip the rest. */
        cameraX.s.i = 0;
        cameraY.s.i = 0;
        cameraXMod320 = 0;
        cameraYMod320 = 0;
        fansSidesSwapped = 0;
        return;
    }
#endif

    /* Step 1: Relocate all pointers in pitchDatBuffer.
     * SWOS Amiga pitch files store each mapping entry as a 32-bit
     * big-endian byte offset into the tile-data region. On x86 LE
     * a native uint32 load would byte-swap the value (offset 0x80
     * becomes 0x80000000), so read each entry via BE_READ_U32. */
    pitchPtrs = (uint32_t *)pitchDatBuffer;
    /* Count UP, not down. The backward loop made GCC use
     * `pitchDatBuffer - 4` (a negative-addend symbol reference) as the
     * loop terminator. On the m68k-amiga-elf toolchain that produces a
     * mis-relocated immediate (the loader applies a delta that's off
     * by 0x200), and the loop ends early — leaving slots 0..127 still
     * holding their raw file offsets and the renderer dereferences
     * them as pointers into low memory. Iterating forward terminates
     * at `pitchDatBuffer + PITCH_POINTER_COUNT*4` (positive addend),
     * which gets relocated correctly. */
    for (int32_t k = 0; k < (int32_t)PITCH_POINTER_COUNT; k++)
    {
#ifdef SWOS_SDL_BUILD
        uint32_t offsetBE = BE_READ_U32((const uint8_t *)&pitchPtrs[k]);
        pitchPtrs[k] = offsetBE + baseAddr;
#else
        pitchPtrs[k] += baseAddr;
#endif
    }
    (void)i;

#ifdef SWOS_SDL_BUILD
    /* Step 1b (SDL only): decode each 128-byte 4-bitplane tile to a
     * 256-byte 8bpp chunky tile. Chunky tiles live in pitchChunkyBuffer,
     * indexed by **original Amiga tile index** (offset / 128) — NOT by
     * pointer slot. Indexing by tile-type means many pitch entries that
     * reference the same tile share one chunky copy, AND the
     * `(entry - chunkyBase) / 256` math used by DrawAnimatedPatterns
     * recovers the same tile index the Amiga path computed via
     * `(entry - currentMenu) / 128`. That keeps the fan-tile animation
     * (toggling tile pairs (1,2),(3,4),...,(21,22) between static and
     * animated variants) functional.
     *
     * Bitplane tile layout: 16 scanlines × (4 planes × 2 bytes) = 128 bytes.
     * Each scanline is `plane0_word, plane1_word, plane2_word, plane3_word`
     * packing 16 horizontal pixels as bit N of each plane word. */
    {
        extern uint8_t pitchChunkyBuffer[];
        uint8_t *bufStart    = pitchDatBuffer;
        uint8_t *bufEnd      = pitchDatBuffer + 64 * 1024;
        uint8_t *tileDataBase = pitchDatBuffer + PITCH_POINTER_COUNT * 4;
        /* Track which tile-type indices have been decoded already. */
        static uint8_t tileDecoded[PITCH_POINTER_COUNT];
        for (i = 0; i < PITCH_POINTER_COUNT; i++)
            tileDecoded[i] = 0;

        /* Reconstruct each slot's real (64-bit) tile pointer.
         *
         * Step 1 wrote `pitchPtrs[i] = offsetBE + baseAddr` where
         * baseAddr is `(uint32_t)(uintptr_t)tileDataBase` — truncated
         * to 32 bits. On 32-bit hosts that's lossless; on 64-bit the
         * slot now holds (low-32 of tileDataBase) + offsetBE, which
         * looks like a valid pointer in the low 4 GB but isn't mapped.
         *
         * The original file offset is recovered by subtracting the
         * truncated base from the truncated slot — both halves cancel
         * to give the same offset Step 1 read out of the file. Add it
         * back to the *real* tileDataBase to get the real pointer. */
        uint32_t baseLow = (uint32_t)(uintptr_t)tileDataBase;
        for (i = 0; i < PITCH_POINTER_COUNT; i++) {
            uint32_t slotLow    = pitchPtrs[i];
            uint32_t fileOffset = slotLow - baseLow;
            uint8_t *bitplane   = tileDataBase + fileOffset;
            /* Guard against bogus pointers (shouldn't happen, but the
             * file could be malformed or the stub still indexed). */
            if (bitplane < tileDataBase || bitplane + 128 > bufEnd) {
                /* Fall back to per-entry indexing for invalid pointers
                 * so we at least don't share garbage between tiles. */
                uint32_t safeIdx = (uint32_t)i;
                uint8_t *chunky = pitchChunkyBuffer + safeIdx * 256;
                for (int k = 0; k < 256; k++) chunky[k] = 0;
                pitchPtrs[i] = (uint32_t)(uintptr_t)chunky;
                s_pitchFullPtrs[i] = chunky;
                continue;
            }

            uint32_t tileIdx = (uint32_t)((bitplane - tileDataBase) / 128);
            if (tileIdx >= PITCH_POINTER_COUNT) {
                /* offset out of bounds → safe fallback */
                uint8_t *chunky = pitchChunkyBuffer + (size_t)i * 256;
                for (int k = 0; k < 256; k++) chunky[k] = 0;
                pitchPtrs[i] = (uint32_t)(uintptr_t)chunky;
                s_pitchFullPtrs[i] = chunky;
                continue;
            }

            uint8_t *chunky = pitchChunkyBuffer + (size_t)tileIdx * 256;

            if (!tileDecoded[tileIdx]) {
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
                tileDecoded[tileIdx] = 1;
            }
            pitchPtrs[i] = (uint32_t)(uintptr_t)chunky;
            s_pitchFullPtrs[i] = chunky;
        }
    }
#endif

    /* Step 2: Reset camera */
    cameraX.s.i = 0;
    cameraY.s.i = 0;
    cameraXMod320 = 0;
    cameraYMod320 = 0;

    /* Step 3: Set up fan pointer groups.
     *
     * The Amiga code aliased the in-buffer 32-bit slot table as two
     * fan-group pointers (group A at slot 0, group B at slot
     * FAN_PTR_TABLE_OFFSET/4 = 21). On 64-bit the slot's truncated
     * pointer can't be dereferenced, so the SDL path resolves to the
     * full-width pointers via the side table populated in Step 1b.
     * On Amiga, s_pitchFullPtrs is NOT populated (Step 1b is gated
     * out) — use the in-buffer slot table directly, matching the
     * original asm. */
#ifdef SWOS_SDL_BUILD
    uint8_t **fanGroupA = &s_pitchFullPtrs[0];
    uint8_t **fanGroupB = &s_pitchFullPtrs[FAN_PTR_TABLE_OFFSET / 4];
#else
    uint8_t **fanGroupA = (uint8_t **)pitchDatBuffer;
    uint8_t **fanGroupB = (uint8_t **)(pitchDatBuffer + FAN_PTR_TABLE_OFFSET);
#endif

    fansSidesSwapped = 0;
    /* If topTeamInGame's team name first byte is odd, swap groups */
    if (topTeamInGame.teamName[0] & 1)
    {
        uint8_t **tmp = fanGroupA;
        fanGroupA = fanGroupB;
        fanGroupB = tmp;
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
        AdjustFanColor(fanGroupB[i], pitchFansConvertTable);
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
        AdjustFanColor(fanGroupA[i], pitchFansConvertTable);
    }
}

#ifdef SWOS_SDL_BUILD
/* ================================================================
 * DrawChunkyPitch (SDL only)
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
 * fan-sprite tiles that AdjustFanColor recolors at load, but that
 * shares tile *data* across mapping entries, not a separate pointer
 * prefix. Rendering must start at index 0 so pitch world Y=0 lines up
 * with overlay sprites that use the same coordinate space. */
#define PITCH_PTR_START     0
#define PITCH_GRID_COLS     42
#define PITCH_GRID_ROWS     55
#define PITCH_TILE_SIZE     16

extern uint8_t pitchDatBuffer[];
extern volatile uintptr_t frontbufferBasePtr;

__attribute__((used, externally_visible))
void DrawChunkyPitch(void)
{
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
                tile = (ptrIdx >= 0 && ptrIdx < PITCH_POINTER_COUNT)
                         ? s_pitchFullPtrs[ptrIdx] : (uint8_t *)0;
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
