/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "adverts.h"
#include "game_types.h"

/* Sprite image indices */
#define SPR_ADVERT_1_PART_1     113
#define SPR_ADVERT_2_PART_1     121
#define SPR_ADVERT_3_PART_1     129
#define SPR_CORNER_FLAG_FRAME_0 1070
#define BIG_S_FRAME_00          1127

/* Game state constants */
#define ST_KEEPER_HOLDS_BALL        3
#define ST_RESULT_AFTER_THE_GAME    26
#define ST_GAME_IN_PROGRESS         100

/* ---- External globals (BSS / data section) ---- */
extern uint16_t trainingGame;
extern uint16_t gameState;
extern FixedPoint cameraX;
extern FixedPoint cameraY;
extern uint16_t showingUserStats;
extern uint16_t statsTimer;
extern uint16_t inSubstitutesMenu;
extern uint16_t subsState;
extern uint16_t currentPlayerIndicatorPosition;
extern uint8_t *spriteGraphicsPtr;
extern uint16_t frameCount;
extern uint16_t lastFrameTicks;

/* ---- Data labels in SWOS2.s code section ---- */
extern uint16_t advertIndex;
extern uint16_t advertSlideTimer;
extern uint16_t currentAnimatedAdLine;
extern uint32_t advert1Pointers[8];
extern uint32_t advert2Pointers[8];
extern uint32_t advert3Pointers[8];
extern uint16_t blockAdvertisements;
extern uint16_t gameStatePl;
extern volatile uint16_t goalCameraMode;
extern uint16_t foulXCoordinate;
extern uint16_t spinBigS;
extern uint16_t bigSIndex;

/* Sprite arrays in data section */
extern Sprite   advertInGameSprites[8]; /* 8 * SPRITE_SIZE bytes */
extern Sprite   cornerFlagSprite;       /* SPRITE_SIZE bytes */
extern Sprite   big_S_Sprite;           /* SPRITE_SIZE bytes */
extern uint8_t  cornerFlagPictureIndices[32];

/* ---- External C/asm functions ---- */
extern void     _InitDisplaySprites(void);
extern uint16_t _Rand(void);

/* ================================================================
 * CopyAdvertisementsData  (SWOS2.s line 11822)
 *
 * Copies sprite graphics data pointers for the 3 advertisement
 * strips from the sprite graphics bank.
 * Offsets 0x1C4, 0x1E4, 0x204 into spriteGraphicsPtr are pointer
 * tables; each entry points to a long that we dereference.
 * ================================================================ */
__attribute__((used, externally_visible))
void _CopyAdvertisementsData(void)
{
    uint32_t *base = (uint32_t *)spriteGraphicsPtr;
    uint32_t *src;
    int i;

    /* advert1: offset 0x1C4 = 452 bytes = 113 longs */
    src = (uint32_t *)((uint8_t *)base + 0x1C4);
    for (i = 0; i < 8; i++) {
        uint32_t *ptr = (uint32_t *)src[i];
        advert1Pointers[i] = *ptr;
    }

    /* advert2: offset 0x1E4 = 484 bytes = 121 longs */
    src = (uint32_t *)((uint8_t *)base + 0x1E4);
    for (i = 0; i < 8; i++) {
        uint32_t *ptr = (uint32_t *)src[i];
        advert2Pointers[i] = *ptr;
    }

    /* advert3: offset 0x204 = 516 bytes = 129 longs */
    src = (uint32_t *)((uint8_t *)base + 0x204);
    for (i = 0; i < 8; i++) {
        uint32_t *ptr = (uint32_t *)src[i];
        advert3Pointers[i] = *ptr;
    }
}

/* ================================================================
 * InitAdvertisements  (SWOS2.s line 11849)
 *
 * Clears ad state and hides all advert sprites.
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitAdvertisements(void)
{
    advertIndex = 0;
    advertSlideTimer = 0;
    currentAnimatedAdLine = 0;
    _SetAdvertisementsInvisible();
}

/* ================================================================
 * SetAdvertisementsVisible  (SWOS2.s line 11858)
 *
 * Sets visible=1 on all 8 advert in-game sprites, then calls
 * InitDisplaySprites to register them.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SetAdvertisementsVisible(void)
{
    int i;
    for (i = 0; i < 8; i++) {
        advertInGameSprites[i].visible = 1;
    }
    _InitDisplaySprites();
}

/* ================================================================
 * SetAdvertisementsInvisible  (SWOS2.s line 11870)
 *
 * Clears visible on all 8 advert in-game sprites, then calls
 * InitDisplaySprites.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SetAdvertisementsInvisible(void)
{
    int i;
    for (i = 0; i < 8; i++) {
        advertInGameSprites[i].visible = 0;
    }
    _InitDisplaySprites();
}

/* ================================================================
 * ManageAdvertisements  (SWOS2.s line 11882)
 *
 * State machine for animated pitch-side advertisements during
 * gameplay. Picks a random advert strip, animates it line by line,
 * pauses, then tears it down.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ManageAdvertisements(void)
{
    uint16_t d0, d1, d2;
    int16_t d7;
    uint32_t *a6;   /* advert pointers */
    Sprite *spr;
    uint32_t *base;

    /* Bail out conditions */
    if (trainingGame != 0)
        return;
    if (gameStatePl == ST_GAME_IN_PROGRESS)
        goto reset_ads_for_gameplay_state;
    if (cameraY.s.i > 84)
        goto reset_ads_for_gameplay_state;
    if (blockAdvertisements != 0)
        return;
    if (goalCameraMode != 0)
        return;
    if ((int16_t)advertIndex < 0)
        return;
    if (gameState == ST_KEEPER_HOLDS_BALL)
        return;
    if (gameState == ST_RESULT_AFTER_THE_GAME)
        return;

    /* If advertIndex == 0, try to start a new ad cycle */
    if (advertIndex == 0) {
        d0 = _Rand();
        if (d0 > 128)
            goto stop_ad_cycle;

        /* If advert sprites not yet visible, make them visible */
        if ((uint16_t)advertInGameSprites[0].visible == 0)
            _SetAdvertisementsVisible();

        /* Pick which advert strip to show based on foulXCoordinate */
        d0 = _Rand();
        d0 = (d0 & 1) + 1;   /* d0 = 1..2 */
        if (foulXCoordinate >= 326) {
            d0 += 1;           /* d0 = 2..3 */
            if (foulXCoordinate <= 346) {
                d0 = _Rand();
                d0 = (uint16_t)((uint16_t)(d0 * 3) >> 8) + 1;  /* d0 = 1..3 */
            }
        }
        advertIndex = d0;
    }

    /* Route to selected ad setup */
    if (advertIndex == 2) {
        d7 = SPR_ADVERT_2_PART_1;
        a6 = advert2Pointers;
        d1 = 0x111;
    } else if (advertIndex == 3) {
        d7 = SPR_ADVERT_3_PART_1;
        a6 = advert3Pointers;
        d1 = 0x1B0;
    } else {
        /* advertIndex == 1 (default) */
        d7 = SPR_ADVERT_1_PART_1;
        a6 = advert1Pointers;
        d1 = 112;
    }

    /* Check slide timer or advance animation */
    if (advertSlideTimer != 0) {
        advertSlideTimer -= 1;
    } else {
        /* Only advance on every 8th frame */
        d0 = frameCount & 7;
        if (d0 == 0) {
            currentAnimatedAdLine += 1;
            if (currentAnimatedAdLine == 6) {
                advertSlideTimer = 0x28;    /* pause for 40 frames */
            } else if (currentAnimatedAdLine == 0x0D) {
                goto stop_ad_cycle;
            }
        }
    }

    /* Build ad strip: update 8 sprite tiles */
    base = (uint32_t *)spriteGraphicsPtr;
    {
        uint32_t *sprGfxTable = (uint32_t *)((uint8_t *)base + (uint16_t)(d7 << 2));
        int16_t imgIdx = d7;
        for (d2 = 0; d2 < 8; d2++) {
            SpriteGraphics *gfx = (SpriteGraphics *)sprGfxTable[d2];
            spr = &advertInGameSprites[d2];
            spr->imageIndex = (int16_t)imgIdx;
            spr->x.s.i = (int16_t)d1;
            spr->y.s.i = 0x5A;
            spr->z.s.i = 0;
            /* Set nlines to 6 */
            gfx->nlines = 6;
            /* Compute data offset: scroll the source image down by one
             * scanline per `currentAnimatedAdLine` step. The byte stride
             * per scanline differs by format:
             *   Amiga bitplane: wquads * 8  (4 planes * 2 bytes/word)
             *   SDL    chunky:  wquads * 16 (1 byte/pixel * 16 px/wquad)
             * Using the Amiga formula on chunky data shifts only half a
             * scanline per increment, producing visible diagonal motion
             * (8 px right + half a row down) instead of vertical scroll. */
            {
                uint16_t wquads = (uint16_t)gfx->wquads;
#ifdef SWOS_SDL_BUILD
                uint32_t lineBytes = (uint32_t)wquads * 16;
#else
                uint32_t lineBytes = (uint32_t)wquads * 8;
#endif
                uint32_t offset = (uint32_t)currentAnimatedAdLine * lineBytes;
                offset += a6[d2];
                gfx->sprData = offset;
            }
            d1 += 0x10;
            imgIdx += 1;
        }
    }
    return;

stop_ad_cycle:
    advertIndex = 0xFFFF;
    goto teardown_ads_and_reset_counters;

reset_ads_for_gameplay_state:
    advertIndex = 0;
    /* fall through */

teardown_ads_and_reset_counters:
    if ((uint16_t)advertInGameSprites[0].visible != 0)
        _SetAdvertisementsInvisible();
    advertSlideTimer = 0;
    currentAnimatedAdLine = 0;
}

/* ================================================================
 * UpdateCornerFlagSprite  (SWOS2.s line 12012)
 *
 * Updates the corner flag sprite position based on camera and
 * animates it using a frame-based lookup table.
 * ================================================================ */
__attribute__((used, externally_visible))
void _UpdateCornerFlagSprite(void)
{
    int16_t d0;
    Sprite *spr = &cornerFlagSprite;

    /* X position: if cameraX.s.i + 168 >= 336, hide far right; else show at x=81 */
    d0 = cameraX.s.i + 168;
    if ((uint16_t)d0 >= 336)
        spr->x.s.i = 590;
    else
        spr->x.s.i = 81;

    /* Y position: if cameraY.s.i + 136 >= 449, hide far bottom; else show at y=129 */
    d0 = cameraY.s.i + 136;
    if ((uint16_t)d0 >= 449)
        spr->y.s.i = 769;
    else
        spr->y.s.i = 129;

    /* Animation: 32-entry lookup table indexed by (frameCount >> 1) & 0x1F */
    d0 = (frameCount >> 1) & 0x1F;
    d0 = (uint16_t)cornerFlagPictureIndices[d0] + SPR_CORNER_FLAG_FRAME_0;
    spr->imageIndex = (int16_t)d0;
}

/* ================================================================
 * SpinBig_S_Sprite  (SWOS2.s line 12046)
 *
 * Updates the spinning "S" indicator sprite in the upper-right
 * corner during gameplay.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SpinBig_S_Sprite(void)
{
    Sprite *spr = &big_S_Sprite;
    int16_t d0;

    /* Default: set imageIndex to -1 (invisible) */
    spr->imageIndex = -1;

    if (spinBigS == 0)
        return;

    /* Advance spin index unless in substitutes menu or showing stats */
    if (inSubstitutesMenu != 0) {
        if (subsState != 0)
            goto skip_advance;
    }
    if (showingUserStats != 0)
        goto skip_advance;
    if (statsTimer != 0)
        goto skip_advance;

    /* Advance by lastFrameTicks, wrap at 64 */
    bigSIndex = (bigSIndex + lastFrameTicks) & 0x3F;

skip_advance:
    d0 = bigSIndex >> 1;
    d0 += BIG_S_FRAME_00;
    spr->imageIndex = (int16_t)d0;

    /* Position relative to camera */
    spr->x.s.i = cameraX.s.i + 0x12D;
    spr->y.s.i = cameraY.s.i + 0x271E;

    if (currentPlayerIndicatorPosition == 3)
        spr->y.s.i += 0x0A;

    spr->z.s.i = 10000;
}
