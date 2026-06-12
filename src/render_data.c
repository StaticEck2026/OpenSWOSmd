/*
 * render_data.c — Blitter/graphics working variables extracted from SWOS2.s
 *
 * NOT part of the build yet — lives in data/ for preparation.
 *
 * All values and sizes cross-checked against SWOS2.s and data/Data.c.
 */

#include "render_data.h"

#if RENDER_DATA_C

/* --- Text blit state (SWOS2.s:3312-3322) --- */
uint16_t  bltconShiftBase        = 0;
uint16_t  textBlitPlaneMask      = 0;
uint16_t  textBlitHeightLines    = 0;
uint32_t *blitSrcA_MaskPtr       = 0;    /* pointer to blitter source A mask */
uint32_t *blitSrcB_GlyphPtr      = 0;    /* pointer to blitter source B glyph */

/* --- Glyph blit state (SWOS2.s:3324-3332) --- */
uint16_t  bltconShiftBase_glyph  = 0;
uint16_t  glyphBlitPlaneMask     = 0;
uint16_t  glyphHeightLinesCache  = 0;
uint32_t  blitSrcA_MaskPtr_glyph = 0;    /* raw 32-bit address */
uint32_t  blitSrcB_GlyphPtr_glyph = 0;   /* raw 32-bit address */

/* --- Destination row info (SWOS2.s:3334-3337) --- */
uint16_t  dstRowBytesPerPlane    = 0;
uint8_t   dstRowAdvanceBytes[56] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

/* --- Hex conversion buffers (SWOS2.s:9179-9182) --- */
uint8_t   hexLongStringBuffer[10] = { 0 };  /* 8 hex digits + null + pad */
uint8_t   hexWordStringBuffer[6]  = { 0 };  /* 4 hex digits + null + pad */

/* The little-player MENU_CMD_SPRITE_COPY descriptor (was stadiumMenuLittlePlayer
 * + littlePlayerSpriteGeneric + littlePlayersColorTable adjacency aliases) is now
 * the EditTeamsSpriteCopy `stadiumMenuLittlePlayerSpriteCopy` struct in
 * swos_globals.c — no more adjacency requirement. */

/* --- Scorer padding (SWOS2.s:4632-4645) — 194 zero bytes after team2Scorers --- */
uint8_t   scorer_padding[194] = { 0 };

/*
 * corePrecacheTable (SWOS2.s:4558-4561)
 *
 * Assembly defines this as a 3-entry pointer table:
 *   dc.l aSoundSfxIn2        ; "sound/sfx.in2"
 *   dc.l aGrafsSwcpich2M     ; "grafs/swcpich2.map"
 *   dc.l -1                  ; terminator
 *
 * Data.c declares it as uint32_t with value 1054812 (the resolved address of the
 * first string pointer).  We mirror that declaration for now.
 */
uint32_t  corePrecacheTable = 1054812;

#endif /* RENDER_DATA_C */
