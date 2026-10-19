/*
 * jag2amiga.c -- the Jaguar sprite tier from the cartridge (see jag2amiga.h).
 *
 * Ported from SwosGfx (github.com/benbaker76/SwosGfx) JagToAmiga.cs -- the bank
 * table, the placement walk, JagRom's depacker and CRY reader -- with its
 * Jag/ patch files replaced by the code that derives them, and from OpenSWOS's
 * scripts/make_jag_graphics.py (the planar banks, JAG.PAL, JAGKITS.BIN).
 */
#include "jag2amiga.h"

#include "amiga_raw.h"
#include "dos2amiga.h"
#include "dos_sprite.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const Sd_JagBankNames[SD_JAG_BANK_COUNT] = {
    "CJCTEAM1", "CJCTEAM2", "CJCTEAM3", "CJCTEAMG", "CJCBITS", "CJCBENCH",
};

#define CART_BASE          0x800000u
#define GAME_PALETTE_OFS   0x6FA42u
#define SHEET_SIZE         (SD_BANK_W * SD_BANK_H)
#define STREAM_CJCGRAFS    0x8947C0u

/* ---- CRY chroma tables (Atari's, as JagToAmiga.cs / jagfx carry them) --- */

static const uint8_t kCryR[256] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x13, 0x00,
    0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x40, 0x2B, 0x15, 0x00,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x5F, 0x47, 0x2F, 0x17, 0x00,
    0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x68, 0x4E, 0x34, 0x1A, 0x00,
    0xA9, 0xA9, 0xA9, 0xA9, 0xA9, 0xA9, 0xA9, 0xA9, 0xA9, 0xAA, 0x8D, 0x71, 0x55, 0x38, 0x1C, 0x00,
    0xCB, 0xCB, 0xCB, 0xCB, 0xCB, 0xCB, 0xCB, 0xCB, 0xCB, 0xCB, 0x99, 0x7A, 0x5B, 0x3D, 0x1E, 0x00,
    0xED, 0xED, 0xED, 0xED, 0xED, 0xED, 0xED, 0xED, 0xE6, 0xC5, 0xA4, 0x83, 0x62, 0x41, 0x20, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF7, 0xD6, 0xB5, 0x94, 0x73, 0x52, 0x31, 0x11,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEB, 0xCC, 0xAD, 0x8F, 0x70, 0x51, 0x33,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE3, 0xC6, 0xAA, 0x8D, 0x71, 0x55,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF9, 0xDF, 0xC5, 0xAB, 0x91, 0x77,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8, 0xE0, 0xC8, 0xB1, 0x99,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC, 0xE6, 0xD0, 0xBB,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xDD,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

static const uint8_t kCryG[256] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
    0x00, 0x13, 0x26, 0x39, 0x4D, 0x60, 0x73, 0x86, 0x9A, 0xAD, 0xC0, 0xD3, 0xE7, 0xFA, 0xFF, 0xFF,
    0x00, 0x15, 0x2B, 0x40, 0x56, 0x6B, 0x81, 0x96, 0xAC, 0xC1, 0xD7, 0xEC, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x17, 0x2F, 0x47, 0x5F, 0x77, 0x8E, 0xA6, 0xBE, 0xD6, 0xEE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x1A, 0x34, 0x4E, 0x68, 0x82, 0x9C, 0xB6, 0xD0, 0xEA, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x1C, 0x38, 0x55, 0x71, 0x8D, 0xAA, 0xC6, 0xE2, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x1E, 0x3D, 0x5B, 0x85, 0x99, 0xB7, 0xD6, 0xF4, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x20, 0x41, 0x62, 0x83, 0xA4, 0xC5, 0xE6, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x20, 0x41, 0x62, 0x83, 0xA4, 0xC5, 0xE6, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x1E, 0x3D, 0x5B, 0x85, 0x99, 0xB7, 0xD6, 0xF4, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x1C, 0x38, 0x55, 0x71, 0x8D, 0xAA, 0xC6, 0xE2, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x1A, 0x34, 0x4E, 0x68, 0x82, 0x9C, 0xB6, 0xD0, 0xEA, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x17, 0x2F, 0x47, 0x5F, 0x77, 0x8E, 0xA6, 0xBE, 0xD6, 0xEE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x15, 0x2B, 0x40, 0x56, 0x6B, 0x81, 0x96, 0xAC, 0xC1, 0xD7, 0xEC, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x13, 0x26, 0x39, 0x4D, 0x60, 0x73, 0x86, 0x9A, 0xAD, 0xC0, 0xD3, 0xE7, 0xFA, 0xFF, 0xFF,
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
};

static const uint8_t kCryB[256] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xDD,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC, 0xE6, 0xD0, 0xBB,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8, 0xE0, 0xC8, 0xB1, 0x99,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF9, 0xDF, 0xC5, 0xAB, 0x91, 0x77,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE3, 0xC6, 0xAA, 0x8D, 0x71, 0x55,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEB, 0xCC, 0xAD, 0x8F, 0x70, 0x51, 0x33,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF7, 0xD6, 0xB5, 0x94, 0x73, 0x52, 0x31, 0x11,
    0xED, 0xED, 0xED, 0xED, 0xED, 0xED, 0xED, 0xED, 0xE6, 0xC5, 0xA4, 0x83, 0x62, 0x41, 0x20, 0x00,
    0xCB, 0xCB, 0xCB, 0xCB, 0xCB, 0xCB, 0xCB, 0xCB, 0xCB, 0xCB, 0x99, 0x7A, 0x5B, 0x3D, 0x1E, 0x00,
    0xA9, 0xA9, 0xA9, 0xA9, 0xA9, 0xA9, 0xA9, 0xA9, 0xA9, 0xAA, 0x8D, 0x71, 0x55, 0x38, 0x1C, 0x00,
    0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x68, 0x4E, 0x34, 0x1A, 0x00,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x5F, 0x47, 0x2F, 0x17, 0x00,
    0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x40, 0x2B, 0x15, 0x00,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x13, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* ---- the cartridge ------------------------------------------------------ */

static int32_t ToFileOffset(const SdBuf *rom, uint32_t address, uint32_t *ofs, char *err)
{
    uint32_t o = address >= CART_BASE ? address - CART_BASE : address;

    if (o >= rom->size) {
        snprintf(err, 256, "address $%06X is outside the %u byte cartridge image",
                 (unsigned)address, (unsigned)rom->size);
        return -1;
    }
    *ofs = o;
    return 0;
}

/* JagRom.LzBits: stop at `size` bytes, at the end-of-stream code, or when the
 * image runs out -- the last two are short and an error for us, as they are
 * for SwosGfx's UnpackSheet. */
int32_t Sd_JagUnpack(const SdBuf *rom, uint32_t address, uint32_t size, SdBuf *out, char *err)
{
    enum { WINDOW = 8192, MASK = WINDOW - 1, OFFSET_BITS = 13, LENGTH_BITS = 4, MIN_MATCH = 3 };
    uint8_t ring[WINDOW];
    uint32_t start, outLen = 0, wpos = 1;
    uint64_t bitPos, bitEnd;
    const uint8_t *src = rom->data;

    out->data = NULL;
    out->size = 0;
    if (ToFileOffset(rom, address, &start, err) != 0)
        return -1;
    out->data = malloc(size ? size : 1);
    if (!out->data) {
        snprintf(err, 256, "out of memory");
        return -1;
    }
    memset(ring, 0, sizeof ring);
    bitPos = (uint64_t)start * 8;
    bitEnd = (uint64_t)rom->size * 8;

#define READ_BITS(n, v)                                                       \
    do {                                                                      \
        (v) = 0;                                                              \
        for (int32_t i_ = 0; i_ < (n); i_++, bitPos++)                        \
            (v) = ((v) << 1) | ((src[bitPos >> 3] >> (7 - (bitPos & 7))) & 1); \
    } while (0)

    while (outLen < size && bitPos + 1 + 8 <= bitEnd) {
        uint32_t flag, offset, length;

        READ_BITS(1, flag);
        if (flag) {
            uint32_t lit;
            READ_BITS(8, lit);
            out->data[outLen++] = (uint8_t)lit;
            ring[wpos] = (uint8_t)lit;
            wpos = (wpos + 1) & MASK;
            continue;
        }
        if (bitPos + OFFSET_BITS + LENGTH_BITS > bitEnd)
            break;
        READ_BITS(OFFSET_BITS, offset);
        READ_BITS(LENGTH_BITS, length);
        length += MIN_MATCH;
        if (offset == 0)
            break;                                  /* end of stream */
        for (uint32_t i = 0; i < length && outLen < size; i++) {
            uint8_t c = ring[offset];
            out->data[outLen++] = c;
            ring[wpos] = c;
            wpos = (wpos + 1) & MASK;
            offset = (offset + 1) & MASK;
        }
    }
#undef READ_BITS

    if (outLen != size) {
        snprintf(err, 256, "Jaguar stream at $%06X unpacked to %u bytes, expected %u",
                 (unsigned)address, (unsigned)outLen, (unsigned)size);
        Sd_BufFree(out);
        return -1;
    }
    out->size = size;
    return 0;
}

int32_t Sd_JagGamePalette(const SdBuf *rom, uint32_t pal[256], char *err)
{
    if (rom->size < GAME_PALETTE_OFS + 512) {
        snprintf(err, 256, "the cartridge image is too short for its palette");
        return -1;
    }
    for (int32_t i = 0; i < 256; i++) {
        uint32_t chroma = rom->data[GAME_PALETTE_OFS + i * 2];
        uint32_t intensity = rom->data[GAME_PALETTE_OFS + i * 2 + 1];
        uint32_t r = kCryR[chroma] * intensity / 255;
        uint32_t g = kCryG[chroma] * intensity / 255;
        uint32_t b = kCryB[chroma] * intensity / 255;
        pal[i] = (r << 16) | (g << 8) | b;
    }
    return 0;
}

/* ---- colour matching ---------------------------------------------------- */

/* JagToAmiga.BuildRemap: every index but 0 to the nearest colour of `to`
 * (AmigaPalette.GetNearestColor: Euclidean RGB, the first of equals; the
 * squared distance orders as its sqrt does). */
static void BuildRemap(const uint32_t *from, const uint32_t *to, uint8_t map[256])
{
    map[0] = 0;
    for (int32_t i = 1; i < 256; i++) {
        int32_t r = (int32_t)(from[i] >> 16 & 0xFF), g = (int32_t)(from[i] >> 8 & 0xFF), b = (int32_t)(from[i] & 0xFF);
        int32_t best = 0;
        int64_t bestD = -1;
        for (int32_t j = 0; j < 256; j++) {
            int32_t dr = r - (int32_t)(to[j] >> 16 & 0xFF);
            int32_t dg = g - (int32_t)(to[j] >> 8 & 0xFF);
            int32_t db = b - (int32_t)(to[j] & 0xFF);
            int64_t d = (int64_t)dr * dr + (int64_t)dg * dg + (int64_t)db * db;
            if (bestD < 0 || d < bestD) {
                bestD = d;
                best = j;
            }
        }
        map[i] = (uint8_t)best;
    }
}

static void ImagePalette(const SdImage *img, uint32_t pal[256])
{
    for (int32_t i = 0; i < 256; i++)
        pal[i] = ((uint32_t)img->pal[i][0] << 16) | ((uint32_t)img->pal[i][1] << 8) | img->pal[i][2];
}

/* ---- which sprites each Jaguar sheet holds ------------------------------ */

typedef struct { int32_t lo, hi; } Range;

typedef struct {
    SdJagBank    bank;
    int32_t      loadOffset;        /* word index of the load opcode in menuColorAndTextScript */
    uint32_t     stream;            /* cartridge address of the packed sheet */
    const char  *teamFile;          /* kit banks: the PC container, slot 227+n = sprite n */
    const Range *except;            /* Amiga slots the Jaguar sheet uses for something else */
    int32_t      numExcept;
} JagBankSpec;

/* JagToAmiga.BankTable, found by scoring every placement's pixel mask in the
 * Jaguar sheet against the original Amiga bank. */
static const Range kBitsExcept[] = {
    { 1090, 1090 },     /* the Jaguar has a 3-pixel smudge where the Amiga has 13 */
    { 1093, 1093 },     /* blank on the Amiga; unrelated pixels on the Jaguar */
};
static const Range kBenchExcept[] = {
    { 1171, 1173 },     /* blank on the Amiga; the Jaguar parks two advertising posts there */
    { 1188, 1195 },     /* the Jaguar repeats its one (brown) manager over COACH2's cells */
};

static const JagBankSpec kBanks[SD_JAG_BANK_COUNT] = {
    { SD_JAG_CJCTEAM1,  405, 0x8AC828u, "TEAM1.DAT", NULL, 0 },
    { SD_JAG_CJCTEAM2,  405, 0x8AFB58u, "TEAM2.DAT", NULL, 0 },
    { SD_JAG_CJCTEAM3,  405, 0x8B2FE0u, "TEAM3.DAT", NULL, 0 },
    { SD_JAG_CJCTEAMG, 3418, 0x8A02E8u, NULL, NULL, 0 },
    { SD_JAG_CJCBITS,  4547, 0x8993D8u, NULL, kBitsExcept, 2 },
    { SD_JAG_CJCBENCH, 4889, 0x89D0B0u, NULL, kBenchExcept, 2 },
};

#define DOS_GAMEPLAY_OFFSET 114     /* PC index = Amiga index + 114 */
#define TEAM_BLOCK_BASE     227     /* first Amiga slot of the kit block */

static int32_t UsesJaguar(const JagBankSpec *spec, int32_t amigaIndex)
{
    for (int32_t i = 0; i < spec->numExcept; i++)
        if (amigaIndex >= spec->except[i].lo && amigaIndex <= spec->except[i].hi)
            return 0;
    return 1;
}

static const SdDosSprite *SpriteFor(const JagBankSpec *spec, const SdDosSprites *model,
                                    const SdDosDat *team, int32_t amigaIndex)
{
    int32_t i;

    if (spec->teamFile) {
        i = amigaIndex - TEAM_BLOCK_BASE;
        return (i >= 0 && i < team->count) ? &team->spr[i] : NULL;
    }
    i = amigaIndex + DOS_GAMEPLAY_OFFSET;
    return (i >= 0 && i < SD_DOS_NUM_SPRITES) ? model->spr[i] : NULL;
}

/* ---- patches over the sheet --------------------------------------------- */

/* The celebration block: 3x2 cells of 16x16 at (144,64), blank in every
 * Jaguar kit sheet. Stamped whole; a drawing in another palette is matched to
 * the nearest Jaguar colours (JagToAmiga.ApplyCelebrationPatch). 1 when
 * stamped, 0 when there is no file, -1 on a bad one. */
#define CELEB_X 144
#define CELEB_Y 64
#define CELEB_W 48
#define CELEB_H 32

static int32_t ApplyCelebration(uint8_t *sheet, const char *setupDir, const char *bankName,
                                const uint32_t *jagPal, char *err)
{
    char rel[64], path[1024];
    SdImage art;
    uint32_t pal[256];
    uint8_t map[256];
    int32_t same = 1;

    if (!setupDir || !setupDir[0])
        return 0;
    snprintf(rel, sizeof rel, "%s_CELEB.bmp", bankName);
    if (Sd_FindNoCase(setupDir, rel, path, sizeof path) != 0)
        return 0;
    memset(&art, 0, sizeof art);
    if (SdImage_ReadBmp(&art, path, err) != 0)
        return -1;
    if (art.w != CELEB_W || art.h != CELEB_H) {
        snprintf(err, 256, "%s is %dx%d, expected %dx%d (the bank block at (%d,%d))",
                 rel, (int)art.w, (int)art.h, CELEB_W, CELEB_H, CELEB_X, CELEB_Y);
        SdImage_Free(&art);
        return -1;
    }
    ImagePalette(&art, pal);
    for (int32_t i = 0; i < 256; i++)
        if (pal[i] != jagPal[i])
            same = 0;
    if (!same)
        BuildRemap(pal, jagPal, map);
    for (int32_t y = 0; y < CELEB_H; y++)
        for (int32_t x = 0; x < CELEB_W; x++) {
            uint8_t v = art.px[y * CELEB_W + x];
            sheet[(CELEB_Y + y) * SD_BANK_W + CELEB_X + x] = same ? v : map[v];
        }
    SdImage_Free(&art);
    return 1;
}

/* Lay a crop over the sheet: it REPLACES its rectangle, zeros included, and
 * marks it so the walk takes those cells from the sheet (ApplyBankPatch). */
static void ApplyCrop(uint8_t *sheet, uint8_t *mask, const uint8_t *crop,
                      int32_t x0, int32_t y0, int32_t w, int32_t h)
{
    for (int32_t y = 0; y < h; y++)
        for (int32_t x = 0; x < w; x++) {
            int32_t o = (y0 + y) * SD_BANK_W + x0 + x;
            sheet[o] = crop[y * w + x];
            mask[o] = 1;
        }
}

/* The manager crops: the PC game's COACH2 frames (which are the Amiga's) in
 * the Jaguar palette, its shadow colour 8 on the Jaguar's shadow index. `tmpl`
 * is the bench bank with every placement drawn from the PC sprites that way. */
#define DOS_SHADOW_COLOUR 8
#define JAG_SHADOW_INDEX  30

static void CutCrop(const uint8_t *bank, int32_t x0, int32_t y0, int32_t w, int32_t h, uint8_t *crop)
{
    for (int32_t y = 0; y < h; y++)
        memcpy(crop + y * w, bank + (y0 + y) * SD_BANK_W + x0, (size_t)w);
}

/* The SELECT SUBSTITUTE arrow (sprite 1170, 9x8 at (272,120)). The Jaguar draws
 * it pointing left; SWOS puts it at the top of the bench pointing right. Its
 * drawing is mirrored inside the window and its drop shadow -- indices 60-62,
 * one pixel down-right of the art -- is cast again from the mirrored art, each
 * row in the shade the Jaguar's own shadow has in that row (62 where it has none). */
#define ARROW_X 272
#define ARROW_Y 120
#define ARROW_W 9
#define ARROW_H 8

static int32_t IsJagArrowShadow(uint8_t v) { return v >= 60 && v <= 62; }

static void MakeArrow(const uint8_t *sheet, uint8_t crop[ARROW_W * ARROW_H])
{
    uint8_t shade[ARROW_H];
    uint8_t art[ARROW_W * ARROW_H];

    memset(crop, 0, ARROW_W * ARROW_H);
    memset(art, 0, sizeof art);
    for (int32_t y = 0; y < ARROW_H; y++) {
        shade[y] = 62;
        for (int32_t x = 0; x < ARROW_W; x++) {
            uint8_t v = sheet[(ARROW_Y + y) * SD_BANK_W + ARROW_X + x];
            if (IsJagArrowShadow(v)) {
                if (shade[y] == 62)
                    shade[y] = v;
            } else if (v) {
                crop[y * ARROW_W + (ARROW_W - 1 - x)] = v;
                art[y * ARROW_W + (ARROW_W - 1 - x)] = 1;
            }
        }
    }
    for (int32_t y = 0; y + 1 < ARROW_H; y++)
        for (int32_t x = 0; x + 1 < ARROW_W; x++)
            if (art[y * ARROW_W + x] && !art[(y + 1) * ARROW_W + x + 1])
                crop[(y + 1) * ARROW_W + x + 1] = shade[y + 1];
}

/* ---- the walk ----------------------------------------------------------- */

/* Copy a window out of the sheet, 0 transparent. 1 when it drew anything. */
static int32_t CopyFromSheet(uint8_t *dst, const uint8_t *sheet, int32_t x0, int32_t y0, int32_t w, int32_t h)
{
    int32_t written = 0;

    for (int32_t y = 0; y < h; y++) {
        int32_t by = y0 + y;
        if (by < 0 || by >= SD_BANK_H)
            continue;
        for (int32_t x = 0; x < w; x++) {
            int32_t bx = x0 + x;
            uint8_t v;
            if (bx < 0 || bx >= SD_BANK_W)
                continue;
            v = sheet[by * SD_BANK_W + bx];
            if (!v)
                continue;
            dst[by * SD_BANK_W + bx] = v;
            written++;
        }
    }
    return written > 0;
}

static int32_t MaskCovers(const uint8_t *mask, int32_t x0, int32_t y0, int32_t w, int32_t h)
{
    if (!mask)
        return 0;
    for (int32_t y = y0 < 0 ? 0 : y0; y < y0 + h && y < SD_BANK_H; y++)
        for (int32_t x = x0 < 0 ? 0 : x0; x < x0 + w && x < SD_BANK_W; x++)
            if (mask[y * SD_BANK_W + x])
                return 1;
    return 0;
}

/* A decoded PC sprite: its own indices, 0..15, 0 transparent. */
typedef struct {
    uint8_t *px;
    int32_t  w, h;
} Decoded;

static int32_t Decode(const SdDosSprite *spr, Decoded *d, char *err)
{
    d->px = NULL;
    d->w = d->h = 0;
    if (!spr || spr->width <= 0 || spr->height <= 0)
        return 0;
    d->px = malloc((size_t)spr->width * (size_t)spr->height);
    if (!d->px) {
        snprintf(err, 256, "out of memory");
        return -1;
    }
    if (Sd_DosSpriteDecode(spr, d->px) != 0) {
        snprintf(err, 256, "a PC sprite's record does not fit its data");
        free(d->px);
        d->px = NULL;
        return -1;
    }
    d->w = spr->width;
    d->h = spr->height;
    return 0;
}

/* JagToAmiga.BlitRemapped: transparency is decided on the PC index, so a colour
 * whose nearest Jaguar match is index 0 still writes its 0. */
static void BlitRemapped(uint8_t *bank, const Decoded *d, const SdPlacement *p, const uint8_t map[256])
{
    int32_t limW = d->w < p->winW ? d->w : p->winW;
    int32_t limH = d->h < p->winH ? d->h : p->winH;

    for (int32_t y = 0; y < limH; y++) {
        int32_t by = p->y0 + y;
        if (by < 0 || by >= SD_BANK_H)
            continue;
        for (int32_t x = 0; x < limW; x++) {
            uint8_t v = d->px[y * d->w + x];
            int32_t bx = p->x0 + x;
            if (v == 0 || bx < 0 || bx >= SD_BANK_W)
                continue;
            bank[by * SD_BANK_W + bx] = map[v];
        }
    }
}

/* The bench bank drawn wholly from the PC sprites with `map` (the template
 * the manager crops are cut from). */
static int32_t RenderDosTemplate(const JagBankSpec *spec, const SdPlacementList *places,
                                 const SdDosSprites *model, const uint8_t map[256],
                                 uint8_t *out, char *err)
{
    memset(out, 0, SHEET_SIZE);
    for (int32_t k = 0; k < places->count; k++) {
        Decoded d;
        if (Decode(SpriteFor(spec, model, NULL, places->items[k].amigaIndex), &d, err) != 0)
            return -1;
        if (d.px)
            BlitRemapped(out, &d, &places->items[k], map);
        free(d.px);
    }
    return 0;
}

static int32_t BuildBank(const JagBankSpec *spec, const SdBuf *rom, const SdPlacementList *places,
                         const SdDosSprites *model, const char *dosDir, const char *setupDir,
                         const uint32_t *jagPal, const uint8_t *dosToJag,
                         SdImage *bank, int32_t *celebrated, char *err)
{
    const char *name = Sd_JagBankNames[spec->bank];
    SdDosDat team;
    SdBuf sheet;
    uint8_t *mask = NULL, *tmpl = NULL;
    int32_t rc = -1;

    memset(&team, 0, sizeof team);
    sheet.data = NULL;
    *celebrated = 0;
    if (SdImage_Init(bank, SD_BANK_W, SD_BANK_H) != 0) {
        snprintf(err, 256, "out of memory");
        return -1;
    }
    SdImage_SetPaletteArgb(bank, jagPal, 256);
    if (Sd_JagUnpack(rom, spec->stream, SHEET_SIZE, &sheet, err) != 0)
        goto out;
    if (spec->teamFile && Sd_DosDatLoad(&team, dosDir, spec->teamFile, SD_DOS_TEAM_SPRITES, err) != 0)
        goto out;

    if (spec->teamFile) {
        int32_t c = ApplyCelebration(sheet.data, setupDir, name, jagPal, err);
        if (c < 0)
            goto out;
        *celebrated = c;
    }
    if (spec->bank == SD_JAG_CJCBENCH) {
        uint8_t shadowMap[256], coach[48 * 24], arrow[ARROW_W * ARROW_H];

        mask = calloc(1, SHEET_SIZE);
        tmpl = malloc(SHEET_SIZE);
        if (!mask || !tmpl) {
            snprintf(err, 256, "out of memory");
            goto out;
        }
        memcpy(shadowMap, dosToJag, 256);
        shadowMap[DOS_SHADOW_COLOUR] = JAG_SHADOW_INDEX;
        if (RenderDosTemplate(spec, places, model, shadowMap, tmpl, err) != 0)
            goto out;
        /* SwosGfx applied its patch files in name order: 16_24, 256_24, 272_120. */
        CutCrop(tmpl, 16, 24, 48, 24, coach);       /* sitting trio, 1189-1191 */
        ApplyCrop(sheet.data, mask, coach, 16, 24, 48, 24);
        CutCrop(tmpl, 256, 24, 48, 24, coach);      /* standing trio, 1192-1194 */
        ApplyCrop(sheet.data, mask, coach, 256, 24, 48, 24);
        MakeArrow(sheet.data, arrow);
        ApplyCrop(sheet.data, mask, arrow, ARROW_X, ARROW_Y, ARROW_W, ARROW_H);
    }

    for (int32_t k = 0; k < places->count; k++) {
        const SdPlacement *p = &places->items[k];
        Decoded d;
        int32_t w, h, took;

        if (Decode(SpriteFor(spec, model, &team, p->amigaIndex), &d, err) != 0)
            goto out;
        /* The sprite table is shared across releases, so the PC sprite's size is
         * the window lifted from the sheet; without one, the script's window. */
        w = d.px && d.w < p->winW ? d.w : p->winW;
        h = d.px && d.h < p->winH ? d.h : p->winH;
        took = (UsesJaguar(spec, p->amigaIndex) || MaskCovers(mask, p->x0, p->y0, w, h)) &&
               CopyFromSheet(bank->px, sheet.data, p->x0, p->y0, w, h);
        if (!took && d.px)
            BlitRemapped(bank->px, &d, p, dosToJag);
        free(d.px);
    }
    rc = 0;
out:
    free(mask);
    free(tmpl);
    Sd_BufFree(&sheet);
    Sd_DosDatFree(&team);
    if (rc != 0)
        SdImage_Free(bank);
    return rc;
}

int32_t Sd_JagBanks(const SdBuf *rom, const char *dosDir, const char *setupDir,
                    SdImage banks[SD_JAG_BANK_COUNT], SdImage *grafsSheet,
                    uint32_t *celebrations, char *err)
{
    SdScriptPlacements script;
    SdDosSprites *model = NULL;
    uint32_t jagPal[256];
    uint8_t dosToJag[256];
    const uint16_t *words;
    int32_t n, done = 0, rc = -1;
    SdBuf sheet;

    memset(&script, 0, sizeof script);
    for (int32_t b = 0; b < SD_JAG_BANK_COUNT; b++)
        memset(&banks[b], 0, sizeof banks[b]);
    if (grafsSheet)
        memset(grafsSheet, 0, sizeof *grafsSheet);
    if (celebrations)
        *celebrations = 0;

    if (Sd_JagGamePalette(rom, jagPal, err) != 0)
        return -1;
    BuildRemap(Sd_DosSpritePalGame, jagPal, dosToJag);
    words = Sd_ScriptWords(SD_SCRIPT_MENU_COLOR_TEXT, &n);
    if (Sd_ScriptInterpret(words, n, &script, err) != 0)
        return -1;
    model = calloc(1, sizeof *model);
    if (!model) {
        snprintf(err, 256, "out of memory");
        goto out;
    }
    if (Sd_DosSpritesLoad(model, dosDir, err) != 0)
        goto out;

    for (done = 0; done < SD_JAG_BANK_COUNT; done++) {
        const JagBankSpec *spec = &kBanks[done];
        const SdPlacementList *places = Sd_ScriptFindLoad(&script, spec->loadOffset);
        int32_t celebrated;

        if (!places) {
            snprintf(err, 256, "%s: no bank load at word %d of its script",
                     Sd_JagBankNames[done], (int)spec->loadOffset);
            goto out;
        }
        if (BuildBank(spec, rom, places, model, dosDir, setupDir, jagPal, dosToJag,
                      &banks[done], &celebrated, err) != 0)
            goto out;
        if (celebrated && celebrations)
            *celebrations |= 1u << done;
    }

    if (grafsSheet) {
        if (Sd_JagUnpack(rom, STREAM_CJCGRAFS, SHEET_SIZE, &sheet, err) != 0)
            goto out;
        if (SdImage_Init(grafsSheet, SD_BANK_W, SD_BANK_H) != 0) {
            Sd_BufFree(&sheet);
            snprintf(err, 256, "out of memory");
            goto out;
        }
        memcpy(grafsSheet->px, sheet.data, SHEET_SIZE);
        SdImage_SetPaletteArgb(grafsSheet, jagPal, 256);
        Sd_BufFree(&sheet);
    }
    rc = 0;
out:
    if (rc != 0)
        for (int32_t b = 0; b < done && b < SD_JAG_BANK_COUNT; b++)
            SdImage_Free(&banks[b]);
    if (model)
        Sd_DosSpritesFree(model);
    free(model);
    Sd_ScriptPlacementsFree(&script);
    return rc;
}

/* ---- the files ---------------------------------------------------------- */

/* make_jag_graphics.py KIT_BOXES: (x, y, w, h) in the CJCGRAFS sheet, drop
 * shadow included. A shirt and an icon per shirt type (0 ordinary, 1 coloured
 * sleeves, 2 vertical stripes, 3 hoops), the shorts, the two socks. */
static const int32_t kKitBoxes[][4] = {
    { 209, 63, 44, 40 }, { 257,  1, 44, 40 }, { 161,  1, 44, 41 }, { 209,  1, 44, 40 },
    { 161, 42, 21, 20 }, { 257, 42, 21, 20 }, { 193, 42, 21, 20 }, { 225, 42, 21, 20 },
    { 289, 42, 28, 22 }, { 161, 63, 14, 25 }, { 183, 63, 15, 25 },
};
#define NUM_KIT_BOXES ((int32_t)(sizeof kKitBoxes / sizeof kKitBoxes[0]))

static int32_t MakeKits(const SdImage *sheet, SdBuf *out, char *err)
{
    uint32_t size = 4 + 768 + 1, o;

    for (int32_t i = 0; i < NUM_KIT_BOXES; i++)
        size += 2 + (uint32_t)(kKitBoxes[i][2] * kKitBoxes[i][3]);
    out->data = malloc(size);
    if (!out->data) {
        snprintf(err, 256, "out of memory");
        return -1;
    }
    out->size = size;
    memcpy(out->data, "JGK1", 4);
    memcpy(out->data + 4, sheet->pal, 768);
    out->data[772] = (uint8_t)NUM_KIT_BOXES;
    o = 773;
    for (int32_t i = 0; i < NUM_KIT_BOXES; i++) {
        int32_t x = kKitBoxes[i][0], y = kKitBoxes[i][1], w = kKitBoxes[i][2], h = kKitBoxes[i][3];
        out->data[o++] = (uint8_t)w;
        out->data[o++] = (uint8_t)h;
        for (int32_t r = 0; r < h; r++, o += (uint32_t)w)
            memcpy(out->data + o, sheet->px + (y + r) * sheet->w + x, (size_t)w);
    }
    return 0;
}

void Sd_JagFilesFree(SdJagFile out[SD_JAG_FILE_COUNT])
{
    for (int32_t i = 0; i < SD_JAG_FILE_COUNT; i++)
        Sd_BufFree(&out[i].data);
}

int32_t Sd_JagBuild(const char *romPath, const char *dosDir, const char *setupDir,
                    SdJagFile out[SD_JAG_FILE_COUNT], uint32_t *celebrations, char *err)
{
    SdImage banks[SD_JAG_BANK_COUNT], grafs;
    SdBuf rom;
    uint8_t used[256];
    int32_t rc = -1;

    memset(out, 0, sizeof(SdJagFile) * SD_JAG_FILE_COUNT);
    if (Sd_ReadFile(romPath, &rom, err) != 0)
        return -1;
    if (rom.size != SD_JAG_ROM_SIZE) {
        snprintf(err, 256, "%s is %u bytes; the cartridge image is %u",
                 romPath, (unsigned)rom.size, (unsigned)SD_JAG_ROM_SIZE);
        Sd_BufFree(&rom);
        return -1;
    }
    if (Sd_JagBanks(&rom, dosDir, setupDir, banks, &grafs, celebrations, err) != 0) {
        Sd_BufFree(&rom);
        return -1;
    }
    Sd_BufFree(&rom);

    memset(used, 0, sizeof used);
    for (int32_t b = 0; b < SD_JAG_BANK_COUNT; b++) {
        snprintf(out[b].name, sizeof out[b].name, "%s.RAW", Sd_JagBankNames[b]);
        for (int32_t i = 0; i < SHEET_SIZE; i++)
            used[banks[b].px[i]] = 1;
        if (Sd_AmigaRawEncode(&banks[b], 8, &out[b].data, err) != 0)
            goto done;
    }

    /* JAG.PAL: the palette and which of it the art uses (all six sheets share it). */
    snprintf(out[6].name, sizeof out[6].name, "JAG.PAL");
    out[6].data.size = 4 + 768 + 256;
    out[6].data.data = malloc(out[6].data.size);
    if (!out[6].data.data) {
        snprintf(err, 256, "out of memory");
        goto done;
    }
    memcpy(out[6].data.data, "JGP1", 4);
    memcpy(out[6].data.data + 4, banks[0].pal, 768);
    memcpy(out[6].data.data + 772, used, 256);

    snprintf(out[7].name, sizeof out[7].name, "JAGKITS.BIN");
    if (MakeKits(&grafs, &out[7].data, err) != 0)
        goto done;
    rc = 0;
done:
    for (int32_t b = 0; b < SD_JAG_BANK_COUNT; b++)
        SdImage_Free(&banks[b]);
    SdImage_Free(&grafs);
    if (rc != 0)
        Sd_JagFilesFree(out);
    return rc;
}
