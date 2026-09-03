/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "math68k.h"

/* Movement speed_factor (dos_gameplay.c): DOS/PC 70fps scales the CalculateDeltaXAndY
 * deltas by 41/64; Amiga 50fps = 1/1 (no-op). See docs/DOS_GAMEPLAY.md §8.3. */
extern int16_t _gMoveDeltaScaleNum;
extern int16_t _gMoveDeltaScaleDen;

/* ================================================================
 * kSineCosineTable — 256 entries of int16_t
 * Indexed by direction (0..511 wrapped to 0..255 via word addressing).
 * Values are sine/cosine * 32767.
 * ================================================================ */
static const int16_t kSineCosineTable[257] = {
    0, 804, 1608, 2410, 3212, 4011, 4808, 5602,
    6393, 7179, 7962, 8739, 9512, 10278, 11039, 11793,
    12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530,
    18204, 18867, 19519, 20159, 20787, 21402, 22005, 22594,
    23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790,
    27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
    30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971,
    32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
    32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285,
    32137, 31971, 31785, 31580, 31356, 31113, 30852, 30571,
    30273, 29956, 29621, 29268, 28898, 28510, 28105, 27683,
    27245, 26790, 26319, 25832, 25329, 24812, 24279, 23731,
    23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868,
    18204, 17530, 16846, 16151, 15446, 14732, 14010, 13279,
    12539, 11793, 11039, 10278, 9512, 8739, 7962, 7179,
    6393, 5602, 4808, 4011, 3212, 2411, 1608, 804,
    0, -804, -1608, -2410, -3212, -4011, -4808, -5602,
    -6393, -7179, -7962, -8739, -9512, -10278, -11039, -11793,
    -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530,
    -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
    -23170, -23731, -24279, -24812, -25329, -25832, -26319, -26790,
    -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
    -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971,
    -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
    -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285,
    -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
    -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683,
    -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
    -23170, -22594, -22005, -21402, -20787, -20159, -19519, -18867,
    -18204, -17530, -16845, -16151, -15446, -14732, -14009, -13278,
    -12539, -11792, -11039, -10278, -9512, -8739, -7961, -7179,
    -6392, -5602, -4808, -4011, -3211, -2410, -1608, -804,
    0x1A00  /* pad/sentinel — matches original asm dc.w $1A00 */
};

/* ================================================================
 * kAngleCoeficients — 32×32 = 1024 entries of int16_t
 * Indexed by (dy_abs<<5 | dx_abs), each entry is an angle coefficient
 * (tangent-like value 0..64, or -1 for zero movement).
 * ================================================================ */
static const int16_t kAngleCoeficients[1025] = {
    -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    64, 32, 16, 11, 8, 6, 5, 5, 4, 4, 3, 3, 3, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    64, 48, 32, 21, 16, 13, 11, 9, 8, 7, 6, 6, 5, 5, 5, 4,
    4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2,
    64, 53, 43, 32, 24, 19, 16, 14, 12, 11, 10, 9, 8, 7, 7, 6,
    6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3,
    64, 56, 48, 40, 32, 26, 21, 18, 16, 14, 13, 12, 11, 10, 9, 9,
    8, 8, 7, 7, 6, 6, 6, 6, 5, 5, 5, 5, 5, 4, 4, 4,
    64, 58, 51, 45, 38, 32, 27, 23, 20, 18, 16, 15, 13, 12, 11, 11,
    10, 9, 9, 8, 8, 8, 7, 7, 7, 6, 6, 6, 6, 6, 5, 5,
    64, 59, 53, 48, 43, 37, 32, 27, 24, 21, 19, 17, 16, 15, 14, 13,
    12, 11, 11, 10, 10, 9, 9, 8, 8, 8, 7, 7, 7, 7, 6, 6,
    64, 59, 55, 50, 46, 41, 37, 32, 28, 25, 22, 20, 19, 17, 16, 15,
    14, 13, 12, 12, 11, 11, 10, 10, 9, 9, 9, 8, 8, 8, 7, 7,
    64, 60, 56, 52, 48, 44, 40, 36, 32, 28, 26, 23, 21, 20, 18, 17,
    16, 15, 14, 13, 13, 12, 12, 11, 11, 10, 10, 9, 9, 9, 9, 8,
    64, 60, 57, 53, 50, 46, 43, 39, 36, 32, 29, 26, 24, 22, 21, 19,
    18, 17, 16, 15, 14, 14, 13, 13, 12, 12, 11, 11, 10, 10, 10, 9,
    64, 61, 58, 54, 51, 48, 45, 42, 38, 35, 32, 29, 27, 25, 23, 21,
    20, 19, 18, 17, 16, 15, 15, 14, 13, 13, 12, 12, 11, 11, 11, 10,
    64, 61, 58, 55, 52, 49, 47, 44, 41, 38, 35, 32, 29, 27, 25, 23,
    22, 21, 20, 19, 18, 17, 16, 15, 15, 14, 14, 13, 13, 12, 12, 11,
    64, 61, 59, 56, 53, 51, 48, 45, 43, 40, 37, 35, 32, 30, 27, 26,
    24, 23, 21, 20, 19, 18, 17, 17, 16, 15, 15, 14, 14, 13, 13, 12,
    64, 62, 59, 57, 54, 52, 49, 47, 44, 42, 39, 37, 34, 32, 30, 28,
    26, 24, 23, 22, 21, 20, 19, 18, 17, 17, 16, 15, 15, 14, 14, 13,
    64, 62, 59, 57, 55, 53, 50, 48, 46, 43, 41, 39, 37, 34, 32, 30,
    28, 26, 25, 24, 22, 21, 20, 19, 19, 18, 17, 17, 16, 15, 15, 14,
    64, 62, 60, 58, 55, 53, 51, 49, 47, 45, 43, 41, 38, 36, 34, 32,
    30, 28, 27, 25, 24, 23, 22, 21, 20, 19, 18, 18, 17, 17, 16, 15,
    64, 62, 60, 58, 56, 54, 52, 50, 48, 46, 44, 42, 40, 38, 36, 34,
    32, 30, 28, 27, 26, 24, 23, 22, 21, 20, 20, 19, 18, 18, 17, 17,
    64, 62, 60, 58, 56, 55, 53, 51, 49, 47, 45, 43, 41, 40, 38, 36,
    34, 32, 30, 29, 27, 26, 25, 24, 23, 22, 21, 20, 19, 19, 18, 18,
    64, 62, 60, 59, 57, 55, 53, 52, 50, 48, 46, 44, 43, 41, 39, 37,
    36, 34, 32, 30, 29, 27, 26, 25, 24, 23, 22, 21, 21, 20, 19, 19,
    64, 62, 61, 59, 57, 56, 54, 52, 51, 49, 47, 45, 44, 42, 40, 39,
    37, 35, 34, 32, 30, 29, 28, 26, 25, 24, 23, 23, 22, 21, 20, 20,
    64, 62, 61, 59, 58, 56, 54, 53, 51, 50, 48, 46, 45, 43, 42, 40,
    38, 37, 35, 34, 32, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 21,
    64, 62, 61, 59, 58, 56, 55, 53, 52, 50, 49, 47, 46, 44, 43, 41,
    40, 38, 37, 35, 34, 32, 31, 29, 28, 27, 26, 25, 24, 23, 22, 22,
    64, 63, 61, 60, 58, 57, 55, 54, 52, 51, 49, 48, 47, 45, 44, 42,
    41, 39, 38, 36, 35, 33, 32, 31, 29, 28, 27, 26, 25, 24, 23, 23,
    64, 63, 61, 60, 58, 57, 56, 54, 53, 51, 50, 49, 47, 46, 45, 43,
    42, 40, 39, 38, 36, 35, 33, 32, 31, 29, 28, 27, 26, 25, 25, 24,
    64, 63, 61, 60, 59, 57, 56, 55, 53, 52, 51, 49, 48, 47, 45, 44,
    43, 41, 40, 39, 37, 36, 35, 33, 32, 31, 30, 28, 27, 26, 26, 25,
    64, 63, 61, 60, 59, 58, 56, 55, 54, 52, 51, 50, 49, 47, 46, 45,
    44, 42, 41, 40, 38, 37, 36, 35, 33, 32, 31, 30, 29, 28, 27, 26,
    64, 63, 62, 60, 59, 58, 57, 55, 54, 53, 52, 50, 49, 48, 47, 46,
    44, 43, 42, 41, 39, 38, 37, 36, 34, 33, 32, 31, 30, 29, 28, 27,
    64, 63, 62, 60, 59, 58, 57, 56, 55, 53, 52, 51, 50, 49, 47, 46,
    45, 44, 43, 41, 40, 39, 38, 37, 36, 34, 33, 32, 31, 30, 29, 28,
    64, 63, 62, 61, 59, 58, 57, 56, 55, 54, 53, 51, 50, 49, 48, 47,
    46, 45, 43, 42, 41, 40, 39, 38, 37, 35, 34, 33, 32, 31, 30, 29,
    64, 63, 62, 61, 60, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 47,
    46, 45, 44, 43, 42, 41, 40, 39, 38, 36, 35, 34, 33, 32, 31, 30,
    64, 63, 62, 61, 60, 59, 58, 57, 55, 54, 53, 52, 51, 50, 49, 48,
    47, 46, 45, 44, 43, 42, 41, 39, 38, 37, 36, 35, 34, 33, 32, 31,
    64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49,
    47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32,
    0x1A00  /* pad/sentinel — matches original asm dc.w $1A00 */
};

/* ================================================================
 * CalculateDeltaXAndY  (SWOS2.s line 9284)
 *
 * Given source (srcX, srcY), destination (destX, destY), and speed,
 * compute:
 *   - direction (0..255 on a circle, or 0xFFFF if no movement)
 *   - deltaX (32-bit signed fixed-point: speed × cosine)
 *   - deltaY (32-bit signed fixed-point: speed × sine)
 *
 * The algorithm:
 *   1. Compute dx = destX - srcX, dy = destY - srcY, record signs
 *   2. Normalize |dx|, |dy| into [0..31] by halving both until both fit
 *   3. Index into kAngleCoeficients[dy*32 + dx] to get a tangent value (0..64)
 *   4. Map tangent + quadrant to a direction (0..511 in half-degrees)
 *   5. Look up sine/cosine from kSineCosineTable, shift right by 8
 *   6. Multiply by speed to get fixed-point deltas
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t CalculateDeltaXAndY(int16_t destX, int16_t destY,
                               int16_t srcX, int16_t srcY,
                               int16_t speed,
                               int32_t *outDeltaX, int32_t *outDeltaY)
{
    int16_t dx = destX - srcX;   /* d1 = destX - srcX */
    int16_t dy = destY - srcY;   /* d2 = destY - srcY */

    /* Record signs: smi sets 0xFF if negative, 0x00 if positive/zero */
    uint8_t xNeg = (dx < 0) ? 0xFF : 0x00;   /* d3 via smi */
    uint8_t yNeg = (dy < 0) ? 0xFF : 0x00;   /* d4 via smi */

    /* Absolute values */
    int16_t absDx = (dx < 0) ? -dx : dx;
    int16_t absDy = (dy < 0) ? -dy : dy;

    /* Normalize: halve both until absDx < 32 */
    while (absDx >= 32) {
        absDx >>= 1;
        absDy >>= 1;
    }
    /* Then halve both until absDy < 32 */
    while (absDy >= 32) {
        absDx >>= 1;
        absDy >>= 1;
    }

    /* Index into angle coefficients table */
    int16_t tableIndex = (absDy << 5) | absDx;
    int16_t tangent = kAngleCoeficients[tableIndex];   /* 0..64 or -1 */

    if (tangent < 0) {
        /* No movement (dx == dy == 0) */
        *outDeltaX = 0;
        *outDeltaY = 0;
        return 0xFFFF;
    }

    /* Map tangent + quadrant to direction */
    tangent <<= 1;   /* asl.w #1,d0 */

    int16_t direction;
    if (xNeg == 0) {
        /* Right half of circle */
        if (yNeg == 0) {
            /* Quadrant 1: x+, y+ */
            direction = 128 - tangent;
        } else {
            /* Quadrant 4: x+, y- */
            direction = 128 + tangent;
        }
    } else {
        /* Left half of circle */
        if (yNeg == 0) {
            /* Quadrant 2: x-, y+ */
            direction = 384 + tangent;
        } else {
            /* Quadrant 3: x-, y- */
            direction = 384 - tangent;
        }
    }

    /* Calculate sine/cosine index */
    direction &= 0x1FF;   /* 0..511 */

    int16_t cosIdx = direction;
    int16_t dir = 512 - direction + 256;
    dir &= 0x1FF;
    dir >>= 1;            /* dir = direction on a circle 0..255 */

    /* Look up cosine */
    /* The table is indexed by word offset — in asm it uses (a1,d1.w) where d1 is byte count.
     * Since d1 is already the word index × 2 (from asl.w #1 earlier for tangent, but
     * direction was not shifted), we need to account for the word indexing.
     * In asm: cosIdx is used directly as byte offset into the word table.
     * The table is word-sized, so byte offset = word_index * 2.
     * cosIdx is already in "word offset" form since direction was computed
     * from values that were already doubled (tangent <<= 1, and 128/384 are even).
     * Actually direction itself is the byte offset (each entry is 2 bytes). */
    int16_t cosVal = kSineCosineTable[cosIdx >> 1];   /* word index */

    int16_t sinIdx = cosIdx + 128;
    sinIdx &= 0x1FF;
    int16_t sinVal = kSineCosineTable[sinIdx >> 1];   /* word index */

    /* Shift right 8 bits (asr.w #8) to get -128..127 range */
    cosVal >>= 8;
    sinVal >>= 8;

    /* Multiply by speed (muls.w: 16×16 → 32) */
    *outDeltaX = (int32_t)cosVal * (int32_t)speed;
    *outDeltaY = (int32_t)sinVal * (int32_t)speed;

    /* speed_factor: scale the movement deltas by the active game style so a higher
     * frame rate doesn't play faster — reproduces swos-2020's CalculateDeltaXAndY_125
     * hook, injected at this routine's tail (symbols.txt:2083 -> render.cpp:4682).
     * DOS/PC 70fps = 41/64; Amiga 50fps = 1/1 (no-op, so the Amiga build and normal
     * 50fps play are byte-for-byte unchanged). Callers reading only `dir` (AI
     * goal-angle, tactics arrows) are unaffected. See docs/DOS_GAMEPLAY.md §8.3.
     *
     * 41/64 is done as a chain of ARITHMETIC SHIFTS, not `* 41 / 64`:
     *   x - x/4 - x/16 - x/32 - x/64  ==  x * (64-16-4-2-1)/64  ==  x * 41/64
     * Each shift floors, while C division truncates toward zero, so the two
     * disagree by one on negative deltas — and that one unit compounds into a
     * different trajectory within a few frames. swos-port hit the same thing
     * (updateSprite.cpp: "shifts have to be used explicitly, divisions end up
     * with slightly different result"), and the replay rig caught ours:
     * delta_x -93531 where the recording had -93530. `--replay-selftest` runs
     * the reference vectors for this routine. */
    if (_gMoveDeltaScaleNum == 41 && _gMoveDeltaScaleDen == 64) {
        int32_t dx = *outDeltaX, dy = *outDeltaY;
        *outDeltaX = dx - (dx >> 2) - (dx >> 4) - (dx >> 5) - (dx >> 6);
        *outDeltaY = dy - (dy >> 2) - (dy >> 4) - (dy >> 5) - (dy >> 6);
    } else if (_gMoveDeltaScaleNum != _gMoveDeltaScaleDen) {
        *outDeltaX = *outDeltaX * _gMoveDeltaScaleNum / _gMoveDeltaScaleDen;
        *outDeltaY = *outDeltaY * _gMoveDeltaScaleNum / _gMoveDeltaScaleDen;
    }

    return (uint16_t)dir;
}
