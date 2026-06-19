/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "color_sprites.h"
#include "game_types.h"
#include "dos_mode.h"
#ifdef SWOS_SDL_BUILD
#include "dos_graphics.h"   /* DosGfx_DecodePlanarSprite, DOS_TO_AMIGA_OFFSET */
#include <stdio.h>          /* snprintf */
#include <stdlib.h>         /* malloc / calloc */
#endif

/* ---- Sprite indices ---- */
#define SPR_BIG_DASH            195
#define SPR_BIG_0               173
#define SPR_RESULT_BIG_0        183
#define SPR_TEAM1_NAME          193
#define SPR_TEAM2_NAME          194

/* ---- External functions (already ported) ---- */
extern void _ProcessImageCommandStream(const uint16_t *cmdBuffer, uint8_t *workBuffer);
extern void _CopyAdvertisementsData(void);
extern void _MakeSpritesFromPlayerNames(TeamGame *leftTeam, TeamGame *rightTeam);
extern void _HWSprite_LoadFromSpriteGraphics(uint16_t sprIdx);
extern uint16_t _Text2Sprite(uint16_t x, uint16_t y, uint16_t sprIdx,
                             uint8_t *text, uint8_t *charTable,
                             uint16_t planeMask, uint16_t *outTotalWidth);
extern void _ConvertSpriteColors(uint16_t sprIdx, uint16_t *colorTable);
extern void _CountFaces(TeamGame *team);
extern void _ConvertColorTableToSkinColor(uint16_t color, uint16_t *destTable);
extern void _DetermineGoalieFaces(TeamGame *team, uint8_t *gkFaceToIndex, uint8_t *gkFaceIndices);

/* ---- External globals ---- */
extern void *teamFilenamesTable[];
extern uintptr_t team1Filename;
extern uintptr_t team2Filename;
extern uint16_t menuColorAndTextScript[];
extern uint8_t *menuImageStreamBaseEnd;
extern uint8_t bigCharsTable[];
extern uint16_t team1NameLength;
extern uint16_t team2NameLength;
extern uint16_t team1PlayersColorTable[];
extern uint16_t team2PlayersColorTable[];
extern uint16_t colorTableReserves[];
extern int16_t bigNumbersColorConversionTable[];
extern uint16_t useIndividualPlayerSkinColor;
extern uint16_t team1MostFrequentFace;
extern uint16_t team2MostFrequentFace;
extern uint8_t facesFrequency[];
extern void *skinAndHairColorConversionTable[];
extern GoalieFaceState goalieFaces;     /* GK face/skin state (was 4+2 adjacency-aliased globals) */

#ifdef SWOS_SDL_BUILD
/* ================================================================
 * DOS player/keeper/bench sprite overlay (PC-only, gDosGraphics)
 *
 * Loads the DOS game-sprite `.dat` files from bin/grafs_dos and stores
 * them in our sprite-graphics table at `amigaIndex = dosIndex - 114`
 * (DOS_TO_AMIGA_OFFSET), so the unchanged renderer draws DOS players.
 * Each `.dat` is a run of `[24-byte header][4-bitplane planar]` records;
 * we decode the planar pixels to 8bpp chunky (DosGfx_DecodePlanarSprite)
 * and fill a SpriteGraphics the same way _ProcessImageCommandStream does
 * for the Amiga path (type=0, bltsize=h*64+wq*2, maskPtr=0).
 *
 * Scope: players/keepers/bench only (DOS team1/team3/goal1×2/bench →
 * amiga 227..1219). The score/number/team-name/player-name sprites
 * (amiga 113..226, partly runtime-generated) and the charset/fonts stay
 * Amiga. Per-team kit-colour tinting of DOS players is a follow-up — the
 * Amiga 16-entry colour-conversion LUTs can't recolour 256-colour DOS
 * sprites — so both teams currently render the DOS default kit.
 *
 * Built once (file load + decode + records persist); the table slots are
 * re-pointed on every call because the Amiga load+convert path overwrote
 * them earlier in _ConvertColorsAndTextSprites. The blitter resolves
 * spriteGraphicsPtr[idx] live each frame, so re-pointing takes effect.
 * ================================================================ */
extern uint8_t *spriteGraphicsPtr;
extern int16_t  _LoadFileEx(uint8_t *filename, uint8_t *dest, uint32_t *outFileSize);

#define DOS_SPRITE_TABLE_SLOTS  1220   /* spriteGraphics[] backing = 1220 ptrs */

static SpriteGraphics *s_dosSprite[DOS_SPRITE_TABLE_SLOTS];   /* NULL = keep Amiga */
static int s_dosSpritesBuilt;

static void dos_build_sprite_dat(const char *name, int dosStart, int count)
{
    char path[64];
    static uint8_t staging[64 * 1024];   /* >= largest game .dat (team*=45648) */
    uint32_t fileSize = 0;

    snprintf(path, sizeof path, "grafs_dos/%s", name);
    _LoadFileEx((uint8_t *)path, staging, &fileSize);
    if (fileSize == 0 || fileSize > sizeof staging)
        return;

    /* Chunky output is 2 bytes/pixel-byte vs planar's 1; total < 2*fileSize. */
    uint8_t *chunky = (uint8_t *)malloc((size_t)fileSize * 2);
    SpriteGraphics *recs = (SpriteGraphics *)calloc((size_t)count, sizeof(SpriteGraphics));
    if (!chunky || !recs) { free(chunky); free(recs); return; }

    uint32_t inOff = 0, outOff = 0;
    for (int i = 0; i < count; i++) {
        if (inOff + 24 > fileSize) break;
        const uint8_t *h = staging + inOff;
        /* DOS header fields are little-endian (host is LE; DOS path only). */
        #define DOS_LE16(o) (int16_t)((uint16_t)h[(o)] | ((uint16_t)h[(o) + 1] << 8))
        int16_t pixW   = DOS_LE16(10);
        int16_t nlines = DOS_LE16(12);
        int16_t wquads = DOS_LE16(14);
        int16_t cX     = DOS_LE16(16);
        int16_t cY     = DOS_LE16(18);
        #undef DOS_LE16
        inOff += 24;

        int planarBytes = (int)nlines * (int)wquads * 8;
        if (nlines <= 0 || wquads <= 0 || planarBytes <= 0 ||
            inOff + (uint32_t)planarBytes > fileSize)
            break;

        uint8_t *dst = chunky + outOff;
        DosGfx_DecodePlanarSprite(staging + inOff, nlines, wquads, dst);
        inOff  += (uint32_t)planarBytes;
        outOff += (uint32_t)((int)nlines * (int)wquads * 16);

        int amigaIdx = dosStart + i - DOS_TO_AMIGA_OFFSET;

        SpriteGraphics *s = &recs[i];
        s->sprData  = dst;
        s->maskPtr  = 0;
        s->type     = 0;                                   /* 8bpp chunky */
        s->pixWidth = pixW;
        s->nlines   = nlines;
        s->wquads   = wquads;
        s->centerX  = cX;
        s->centerY  = cY;
        s->bltsize  = (uint16_t)(((int)nlines << 6) + wquads + wquads);
        s->ordinal  = (int16_t)amigaIdx;

        if (amigaIdx >= 0 && amigaIdx < DOS_SPRITE_TABLE_SLOTS)
            s_dosSprite[amigaIdx] = s;
    }
}

static void dos_build_all_sprites(void)
{
    /* (file, count, DOS start index) — from the reference loadAllGameSprites.
     * goal1.dat is loaded twice (two keeper copies), team3.dat is the
     * second team's players. score.dat/charset deliberately omitted. */
    dos_build_sprite_dat("team1.dat", 341, 303);   /* amiga 227..529 */
    dos_build_sprite_dat("team3.dat", 644, 303);   /* amiga 530..832 */
    dos_build_sprite_dat("goal1.dat", 947, 116);   /* amiga 833..948 */
    dos_build_sprite_dat("goal1.dat", 1063, 116);  /* amiga 949..1064 */
    dos_build_sprite_dat("bench.dat", 1179, 155);  /* amiga 1065..1219 */
    s_dosSpritesBuilt = 1;
}

void _DosOverlaySprites(void)
{
    SpriteGraphics **table = (SpriteGraphics **)spriteGraphicsPtr;
    if (!table) return;
    if (!s_dosSpritesBuilt) dos_build_all_sprites();
    for (int i = 0; i < DOS_SPRITE_TABLE_SLOTS; i++)
        if (s_dosSprite[i]) table[i] = s_dosSprite[i];
}
#endif /* SWOS_SDL_BUILD */

/* ---- Helper: convert a range of sprites with a color table ---- */
static void convertSpriteRange(uint16_t start, uint16_t end, uint16_t *colorTable)
{
    uint16_t d0;
    for (d0 = start; d0 != end; d0++) {
        _ConvertSpriteColors(d0, colorTable);
    }
}

/* ---- Helper: find most frequent face and apply skin/hair LUT ---- */
static uint16_t findMostFrequentFaceAndApplyLUT(TeamGame *team, uint16_t *colorTable)
{
    uint16_t d0;
    int16_t *lutPtr;

    _CountFaces(team);

    /* Find face with highest frequency */
    d0 = 0;
    {
        uint8_t best = facesFrequency[0];
        if (facesFrequency[1] > best) {
            best = facesFrequency[1];
            d0 = 1;
        }
        if (facesFrequency[2] > best) {
            d0 = 2;
        }
    }

    /* Apply skin/hair color conversion from LUT */
    lutPtr = (int16_t *)skinAndHairColorConversionTable[d0];
    for (;;) {
        int16_t idx = *lutPtr++;
        if (idx < 0)
            break;
        colorTable[idx] = (uint16_t)*lutPtr++;
    }

    return d0;
}

/* ================================================================
 * ConvertColorsAndTextSprites  (SWOS2.s line 12121)
 *
 * Master sprite color initialization function.
 * Sets team filenames, runs image command stream, builds team name
 * sprites, and converts all player/number/result sprites with
 * team-specific and skin-specific color tables.
 *
 * team1 and team2 are pointers to TeamGame structs (A5/A6 in asm).
 * ================================================================ */
__attribute__((used, externally_visible))
void _ConvertColorsAndTextSprites(TeamGame *team1, TeamGame *team2)
{
    uint16_t d0;
    uint16_t totalWidth;

    /* Set team filenames from shirt type index */
    d0 = (uint16_t)team1->prShirtType;
    team1Filename = (uintptr_t)teamFilenamesTable[d0];
    d0 = (uint16_t)team2->prShirtType;
    team2Filename = (uintptr_t)teamFilenamesTable[d0];

    /* Process image command stream (colors menu graphics) */
    /* Force read from memory — LTO must not use a stale cached value */
    _ProcessImageCommandStream(menuColorAndTextScript, *(uint8_t * volatile *)&menuImageStreamBaseEnd);

    /* Copy advertisement sprite data pointers */
    _CopyAdvertisementsData();

    /* Build player name sprites for both teams */
    _MakeSpritesFromPlayerNames(team1, team2);

    /* --- Big number / result sprites --- */
    /* Set bigNumbersColorConversionTable indices 10 and 11 to 2 */
    bigNumbersColorConversionTable[11] = 2;
    bigNumbersColorConversionTable[10] = 2;

    /* Convert big dash sprite */
    _ConvertSpriteColors(SPR_BIG_DASH, (uint16_t *)bigNumbersColorConversionTable);

    /* Convert big digit sprites (SPR_BIG_0 through SPR_RESULT_BIG_0-1) */
    convertSpriteRange(SPR_BIG_0, SPR_RESULT_BIG_0, (uint16_t *)bigNumbersColorConversionTable);

    /* Convert result digit sprites (SPR_RESULT_BIG_0 through SPR_TEAM1_NAME-1) */
    convertSpriteRange(SPR_RESULT_BIG_0, SPR_TEAM1_NAME, (uint16_t *)bigNumbersColorConversionTable);

    /* --- Team 1 name sprite --- */
    _HWSprite_LoadFromSpriteGraphics(SPR_TEAM1_NAME);
    _Text2Sprite(0, 0, SPR_TEAM1_NAME, (uint8_t *)team1->teamName,
                 bigCharsTable, 2, &totalWidth);
    team1NameLength = totalWidth;

    /* --- Team 2 name sprite --- */
    _HWSprite_LoadFromSpriteGraphics(SPR_TEAM2_NAME);
    _Text2Sprite(0, 0, SPR_TEAM2_NAME, (uint8_t *)team2->teamName,
                 bigCharsTable, 2, &totalWidth);
    team2NameLength = totalWidth;

    /* --- Reserve player sprites for team 1 --- */
    colorTableReserves[15] = (uint16_t)team1->prShirtCol;
    {
        uint16_t stripesCol = (uint16_t)team1->prStripesCol;
        if (stripesCol == (uint16_t)team1->prShirtCol)
            stripesCol = (uint16_t)team1->prShortsCol;
        colorTableReserves[11] = stripesCol;
    }
    convertSpriteRange(1196, 1208, colorTableReserves);

    /* --- Reserve player sprites for team 2 --- */
    colorTableReserves[15] = (uint16_t)team2->prShirtCol;
    {
        uint16_t stripesCol = (uint16_t)team2->prStripesCol;
        if (stripesCol == (uint16_t)team2->prShirtCol)
            stripesCol = (uint16_t)team2->prShortsCol;
        colorTableReserves[11] = stripesCol;
    }
    convertSpriteRange(1208, 1220, colorTableReserves);

    /* --- Determine goalkeeper faces for both teams --- */
    _DetermineGoalieFaces(team1, goalieFaces.team1FaceToPosition, goalieFaces.team1Faces);
    _DetermineGoalieFaces(team2, goalieFaces.team2FaceToPosition, goalieFaces.team2Faces);

    /* --- Team color tables: skin/hair from face frequency --- */
    if (useIndividualPlayerSkinColor == 0) {
        /* Use most frequent face for both teams */
        team1MostFrequentFace = findMostFrequentFaceAndApplyLUT(team1, team1PlayersColorTable);
        team2MostFrequentFace = findMostFrequentFaceAndApplyLUT(team2, team2PlayersColorTable);
    }

    /* --- Team 1 shirt/stripes/shorts/socks colors --- */
    {
        uint16_t d1 = (uint16_t)team1->prShirtCol;
        uint16_t d2 = (uint16_t)team1->prStripesCol;
        /* If shirt type == 2 (striped), swap shirt and stripes colors */
        if ((uint16_t)team1->prShirtType == 2) {
            uint16_t tmp = d1; d1 = d2; d2 = tmp;
        }
        team1PlayersColorTable[11] = d1;
        team1PlayersColorTable[10] = d2;
        team1PlayersColorTable[14] = (uint16_t)team1->prShortsCol;
        team1PlayersColorTable[15] = (uint16_t)team1->prSocksCol;
    }

    /* Team 1 skin 0: convert sprites 227-327 */
    if (useIndividualPlayerSkinColor != 0)
        _ConvertColorTableToSkinColor(0, team1PlayersColorTable);
    convertSpriteRange(227, 328, team1PlayersColorTable);

    /* Team 1 skin 1: convert sprites 328-428 */
    if (useIndividualPlayerSkinColor != 0) {
        _ConvertColorTableToSkinColor(1, team1PlayersColorTable);
        convertSpriteRange(328, 429, team1PlayersColorTable);
    }

    /* Team 1 skin 2: convert sprites 429-529 */
    if (useIndividualPlayerSkinColor != 0) {
        _ConvertColorTableToSkinColor(2, team1PlayersColorTable);
        convertSpriteRange(429, 530, team1PlayersColorTable);
    }

    /* Team 1 goalkeeper: convert sprites 833-890.
     * goalieFaces.team1Faces[0] == 0xFF means _DetermineGoalieFaces didn't assign
     * a face slot. Mirroring the reserve-GK guard below, skip the conversion
     * — otherwise _ConvertColorTableToSkinColor sign-extends 0xFF to -1 and
     * indexes the 3-entry skinAndHairColorConversionTable[] out-of-bounds,
     * walking random memory writing (idx,value) pairs and corrupting
     * unrelated globals (we hit `bigCharsTable + 64`, which silently broke
     * the 'R' and 'S' glyph lookups). */
    if (useIndividualPlayerSkinColor != 0) {
        int8_t face = (int8_t)goalieFaces.team1Faces[0];
        if (face >= 0)
            _ConvertColorTableToSkinColor((uint16_t)(uint8_t)face, team1PlayersColorTable);
    }
    convertSpriteRange(833, 891, team1PlayersColorTable);

    /* Team 1 goalkeeper reserve: sprites 891-948
     * Only converted when useIndividualPlayerSkinColor != 0 AND face >= 0.
     * Note: this is team 1's reserve GK face (team1Faces[1]). */
    if (useIndividualPlayerSkinColor != 0) {
        int8_t face2 = (int8_t)goalieFaces.team1Faces[1];
        if (face2 >= 0) {
            _ConvertColorTableToSkinColor((uint16_t)(uint8_t)face2, team1PlayersColorTable);
            convertSpriteRange(891, 949, team1PlayersColorTable);
        }
    }

    /* --- Team 2 shirt/stripes/shorts/socks colors --- */
    {
        uint16_t d1 = (uint16_t)team2->prShirtCol;
        uint16_t d2 = (uint16_t)team2->prStripesCol;
        if ((uint16_t)team2->prShirtType == 2) {
            uint16_t tmp = d1; d1 = d2; d2 = tmp;
        }
        team2PlayersColorTable[11] = d1;
        team2PlayersColorTable[10] = d2;
        team2PlayersColorTable[14] = (uint16_t)team2->prShortsCol;
        team2PlayersColorTable[15] = (uint16_t)team2->prSocksCol;
    }

    /* Team 2 skin 0: convert sprites 530-630 */
    if (useIndividualPlayerSkinColor != 0)
        _ConvertColorTableToSkinColor(0, team2PlayersColorTable);
    convertSpriteRange(0x212, 0x277, team2PlayersColorTable);

    /* Team 2 skin 1: convert sprites 631-731 */
    if (useIndividualPlayerSkinColor != 0) {
        _ConvertColorTableToSkinColor(1, team2PlayersColorTable);
        convertSpriteRange(0x277, 0x2DC, team2PlayersColorTable);
    }

    /* Team 2 skin 2: convert sprites 732-832 */
    if (useIndividualPlayerSkinColor != 0) {
        _ConvertColorTableToSkinColor(2, team2PlayersColorTable);
        convertSpriteRange(0x2DC, 833, team2PlayersColorTable);
    }

    /* Team 2 goalkeeper: convert sprites 949-1006.
     * Same 0xFF-sentinel guard as team 1's main goalkeeper above. */
    if (useIndividualPlayerSkinColor != 0) {
        int8_t face = (int8_t)goalieFaces.team2Faces[0];
        if (face >= 0)
            _ConvertColorTableToSkinColor((uint16_t)(uint8_t)face, team2PlayersColorTable);
    }
    convertSpriteRange(949, 1007, team2PlayersColorTable);

    /* Team 2 goalkeeper reserve: sprites 1007-1064
     * Only converted when useIndividualPlayerSkinColor != 0 AND face >= 0. */
    if (useIndividualPlayerSkinColor != 0) {
        int8_t reserveFace = (int8_t)goalieFaces.team2Faces[1];
        if (reserveFace >= 0) {
            _ConvertColorTableToSkinColor((uint16_t)(uint8_t)reserveFace, team2PlayersColorTable);
            convertSpriteRange(1007, 1065, team2PlayersColorTable);
        }
    }

#ifdef SWOS_SDL_BUILD
    /* PC-only: replace the player/keeper/bench slots with DOS graphics AFTER
     * all the Amiga load+convert work, so the DOS chunky pixels are final and
     * the 16-entry kit/skin LUTs above don't corrupt them. */
    if (gDosGraphics)
        _DosOverlaySprites();
#endif
}
