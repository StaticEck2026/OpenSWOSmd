/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "pitch.h"
#include "game_types.h"
#include "display.h"

/* ---- External globals (all live in SWOS2.s) ---- */
extern uint32_t swcPitchMapFiles[];     /* char *[6] -- pitch map filenames */
extern uint8_t pitchDatBuffer[];        /* 9240 bytes (2310 longs) */
extern uint8_t currentMenu[];           /* BSS base for pointer relocation */
extern uint16_t panicReasonCode;
extern int16_t cameraX;
extern int16_t cameraY;
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
    uint32_t baseAddr = (uint32_t)currentMenu;
    uint32_t *fanPtrsA;  /* first group of fan sprite pointers */
    uint32_t *fanPtrsB;  /* second group */
    TeamGame *teamData;
    uint16_t kitPrimary, kitSecondary, kitTertiary;

    /* Step 1: Relocate all pointers in pitchDatBuffer */
    pitchPtrs = (uint32_t *)pitchDatBuffer;
    for (i = PITCH_POINTER_COUNT - 1; i >= 0; i--)
    {
        pitchPtrs[i] += baseAddr;
    }

    /* Step 2: Reset camera */
    cameraX = 0;
    cameraY = 0;
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
