/*
 * render_data.h — Blitter/graphics working variables extracted from SWOS2.s
 *
 * NOT part of the build yet — lives in data/ for preparation.
 *
 * Sources: SWOS2.s lines ~3312-3340, ~4558-4561, ~4632-4645, ~9179-9182,
 *          ~14147-14148; cross-checked against data/Data.c and data/Data.h.
 */

#ifndef RENDER_DATA_H
#define RENDER_DATA_H

#include <stdint.h>

/* --- Text blit state (SWOS2.s:3312) --- */
extern uint16_t  bltconShiftBase;           /* dc.w 0 */
extern uint16_t  textBlitPlaneMask;         /* dc.w 0 */
extern uint16_t  textBlitHeightLines;       /* dc.w 0 */
extern uint32_t *blitSrcA_MaskPtr;          /* dc.l 0 — pointer to blitter source A mask */
extern uint32_t *blitSrcB_GlyphPtr;         /* dc.l 0 — pointer to blitter source B glyph */

/* --- Glyph blit state (SWOS2.s:3324) --- */
extern uint16_t  bltconShiftBase_glyph;     /* dc.w 0 */
extern uint16_t  glyphBlitPlaneMask;        /* dc.w 0 */
extern uint16_t  glyphHeightLinesCache;     /* dc.w 0 */
extern uint32_t  blitSrcA_MaskPtr_glyph;    /* dc.l 0 — raw 32-bit address */
extern uint32_t  blitSrcB_GlyphPtr_glyph;   /* dc.l 0 — raw 32-bit address */

/* --- Destination row info (SWOS2.s:3334) --- */
extern uint16_t  dstRowBytesPerPlane;       /* dc.w 0 */
extern uint8_t   dstRowAdvanceBytes[56];    /* 56 zero bytes */

/* --- Hex conversion buffers (SWOS2.s:9179) --- */
extern uint8_t   hexLongStringBuffer[10];   /* 8 hex digits + null + pad */
extern uint8_t   hexWordStringBuffer[6];    /* 4 hex digits + null + pad */

/* --- Little-player sprite generic value (SWOS2.s:14147) --- */
extern uint16_t  littlePlayerSpriteGeneric; /* dc.w 0x00D6 (214) */

/* --- Scorer padding (SWOS2.s:4632) --- */
extern uint8_t   scorer_padding[194];       /* 194 zero bytes after team2Scorers */

/*
 * corePrecacheTable (SWOS2.s:4558)
 *
 * In assembly this is a pointer table: { &"sound/sfx.in2", &"grafs/swcpich2.map", -1 }.
 * Data.c declares it as a plain uint32_t holding the address of the first string.
 * We mirror the Data.c/Data.h declaration for compatibility.
 */
extern uint32_t  corePrecacheTable;

#endif /* RENDER_DATA_H */
