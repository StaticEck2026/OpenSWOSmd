/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/*
 * scroll_blit.c — Scroll blitter system
 *
 * Handles updating the frontbuffer when the camera scrolls:
 *   - ScrollXUpdateColumnsAndBlit: blits pitch tile columns on X scroll
 *   - ScrollYUpdateRowsAndBlit: CPU row copy + blitter mask copy on Y scroll
 *
 * Ported from SWOS2.s lines 5504-5884.
 */

#include "swos_types.h"   /* uint*_t/uintptr_t — platform-aware; never <stdint.h> directly (m68k toolchain's chains to a missing libc stdint.h) */
#include "scroll_blit.h"
#include "game_types.h"
#include "display.h"

/* ---- Blitter register write macros ----
 * GCC m68k 15.1.0 decomposes `volatile uint16_t = N` struct-member
 * stores into byte-pair `move.b` instructions in some contexts. For
 * hardware-trigger registers (writing `bltsize` starts the blitter),
 * the decomposed write triggers the blit mid-update with a half-formed
 * size — fill/copy operations either do nothing or write garbage. The
 * macros wrap each write in inline `move.w` / `move.l` asm so the
 * single-instruction store is guaranteed on m68k. Defined in blitter.c
 * too — kept local here to avoid an extra header just for these.  */
#if defined(__m68k__) || defined(__amigaos__) || defined(__amigaos4__)
#define BLT_W(reg, val)                                                     \
    do {                                                                    \
        uint16_t _v = (uint16_t)(val);                                      \
        volatile uint16_t *_p = &(reg);                                     \
        __asm__ volatile ("move.w %0,(%1)" : : "d"(_v), "a"(_p) : "memory");\
    } while (0)
#define BLT_L(reg, val)                                                     \
    do {                                                                    \
        uint32_t _v = (uint32_t)(val);                                      \
        volatile uint32_t *_p = &(reg);                                     \
        __asm__ volatile ("move.l %0,(%1)" : : "d"(_v), "a"(_p) : "memory");\
    } while (0)
#else
#define BLT_W(reg, val) ((void)((reg) = (uint16_t)(val)))
#define BLT_L(reg, val) ((void)((reg) = (uint32_t)(val)))
#endif

/* ---- External globals ---- */
extern FixedPoint _cameraX;
extern FixedPoint _cameraY;
extern int16_t  _oldCameraX;
extern int16_t  _oldCameraY;
extern uint16_t _cameraXMod320;
extern uint16_t _cameraYMod320;
extern uint16_t _cameraCoordinatesValid;
extern volatile uintptr_t _frontbufferBasePtr;
extern uint8_t  _frontbufferMaskBuffer[];
extern uint8_t  _pitchDatBuffer[];

/* ---- Scroll-blit-specific constants ---- */
#define SCROLL_RIGHT_EDGE_X         335

/* ---- Inline data for ScrollYUpdateRowsAndBlit ---- */
static uint16_t blitWordCount;
static uint32_t maskBlitDestPtr;
static uint16_t maskBlitPendingBytesA;
static uint32_t maskBlitSecondStreamPtr;
static uint16_t maskBlitPendingBytesB;

/* ---- Wait for blitter idle ---- */
static inline __attribute__((always_inline)) void waitBlit(void)
{
    _custom->dmacon = (uint16_t)DMACON_BLT_ON;
    (void)(_custom->dmaconr & (1u << 14));
    while (_custom->dmaconr & (1u << 14))
        ;
    _custom->dmacon = (uint16_t)DMACON_BLT_OFF;
}

#if defined(AGA) && !defined(SWOS_SDL_BUILD)
/* ================================================================
 * Deferred ScrollX mirror maintenance (AGA block-B elimination)
 *
 * The 2x-tall frontbuffer needs rows [SCREEN_HEIGHT, 2*SCREEN_HEIGHT) to mirror
 * [0, SCREEN_HEIGHT) so the copper's single block A can display the whole screen
 * contiguously (no mid-screen 8-plane pointer reload). On a tile cross ScrollX
 * draws a fresh 16px column into [0, SH); its mirror is a full-height A->D copy
 * of SCREEN_HEIGHT*NUM_PLANES one-word rows.
 *
 * Doing that copy on the SAME frame as the column-tile blits OVERRUNS vblank:
 * the ~18 tile blits alone already fill the ~36-line vblank window (measured:
 * L4..L33, display starts vpos 36), so the ~2176-row mirror runs on into the
 * visible display lines and starves the 8-plane bitplane fetch -> the moving
 * garbage column (proven cycle-exact, dma_profile.py).
 *
 * FIX: DEFER the post-column maintenance to NON-CROSS frames of the same buffer.
 * At normal scroll speed a buffer crosses a tile only every ~8 frames but is the
 * draw buffer every 2nd frame, so 3 idle-for-this-buffer frames sit between two
 * of its crosses -- their empty vblank absorbs the copies, and the flush never
 * shares a frame with the column-tile blits. Two slots, one per physical
 * double-buffer, so the alternating frames don't clobber each other's job.
 *
 * BOTH the mirror AND the frontbuffer->mask column copy are deferred: on a cross
 * frame the ~2176-row column blit alone fills vblank, so either copy tacked on
 * after it also spills into the display (the original SWOS2.s Phase 3 mask copy
 * did exactly that). Each copy is itself a full ~2176-row column, so the two run
 * on TWO successive non-cross frames (mirror, then mask) rather than one. The
 * column stays valid in its buffer until it rolls off (~192 frames), and the
 * fresh column is off-screen for several frames (the ~1-coarse-quantum lead
 * SWOS draws each column with), so both copies are done before it is displayed.
 * ================================================================ */
static uint32_t s_pendingSrc[2];    /* frontbuffer src addr of the column; 0 = empty */
static uint32_t s_pendingBuf[2];    /* draw-buffer base the slot belongs to */
static uint8_t  s_pendingPhase[2];  /* 0 = mirror pending, 1 = mask pending */

/* Batched A->D column copy: 1 word wide, `rows` tall (rows may exceed the
 * blitter's 1024-row-per-blit max), plane-step modulo on both channels. A batch
 * of 0x400 encodes as bltsize height-field 0, which the blitter reads as 1024. */
static void columnBlitCopy(uint32_t src, uint32_t dst, uint16_t rows, uint16_t widthWords)
{
    uint16_t planeMod = (uint16_t)(PLANE_STRIDE - widthWords * 2u);  /* skip to next plane */
    waitBlit();
    BLT_L(_custom->bltapt, src);
    BLT_L(_custom->bltdpt, dst);
    BLT_W(_custom->bltamod, planeMod);
    BLT_W(_custom->bltdmod, planeMod);
    BLT_W(_custom->bltafwm, BLT_FWM_ALL);
    BLT_W(_custom->bltalwm, BLT_FWM_ALL);
    BLT_W(_custom->bltcon1, 0);
    BLT_W(_custom->bltcon0, BLTCON0_USEA_USED_LF_F0);
    while (rows) {
        uint16_t batch = rows > 0x400u ? 0x400u : rows;
        rows -= batch;
        waitBlit();
        BLT_W(_custom->bltsize, (uint16_t)((batch << 6) | widthWords));
    }
}

/* Execute the next deferred phase for the current draw buffer. Mirror and mask
 * are each a FULL-column (SCREEN_HEIGHT*NUM_PLANES-row) copy -- doing both on one
 * frame would overrun vblank, so they run on TWO successive non-cross frames
 * (mirror first, then mask). Both are done well before the freshly-drawn column
 * scrolls into view (the ~1-coarse-quantum lead SWOS draws each column with). */
static void flushPendingColumn(void)
{
    int i;
    for (i = 0; i < 2; i++) {
        if (!s_pendingSrc[i] || s_pendingBuf[i] != (uint32_t)_frontbufferBasePtr)
            continue;
        if (s_pendingPhase[i] == 0) {
            /* Mirror the 32px (2-word) column into the +SCREEN_HEIGHT half. */
            columnBlitCopy(s_pendingSrc[i], s_pendingSrc[i] + FB_MIRROR_OFFSET_BYTES,
                           (uint16_t)(SCREEN_HEIGHT * NUM_PLANES), 2u);
            s_pendingPhase[i] = 1;
        } else {
            /* Copy the 32px (2-word) column into the mask buffer (Phase 3, occlusion). */
            uint32_t xByteOff = s_pendingSrc[i] - s_pendingBuf[i];
            columnBlitCopy(s_pendingSrc[i], xByteOff + (uint32_t)_frontbufferMaskBuffer,
                           (uint16_t)(SCREEN_HEIGHT * NUM_PLANES), 2u);
            s_pendingSrc[i] = 0;    /* slot done */
        }
    }
}

/* Queue the freshly-drawn column (mirror + mask) for the current draw buffer. */
static void recordPendingColumn(uint32_t mSrc)
{
    int i, slot = -1;
    for (i = 0; i < 2; i++)                          /* reuse this buffer's slot */
        if (s_pendingBuf[i] == (uint32_t)_frontbufferBasePtr) { slot = i; break; }
    if (slot < 0)
        for (i = 0; i < 2; i++)                      /* else a free slot */
            if (!s_pendingSrc[i]) { slot = i; break; }
    if (slot < 0) slot = 0;                          /* else clobber (fast-scroll fallback) */
    s_pendingBuf[slot] = (uint32_t)_frontbufferBasePtr;
    s_pendingSrc[slot] = mSrc;
    s_pendingPhase[slot] = 0;
}
#endif /* AGA && !SWOS_SDL_BUILD */

#ifndef SWOS_SDL_BUILD
/* ----------------------------------------------------------------
 * scrollXBlitColumn — blit ONE 16px (1-word) pitch-tile column into the
 * frontbuffer at destXByteOff, full height, wrapping the circular Y buffer at
 * cameraYMod320 (SWOS2.s ScrollX phases 1+2). Extracted so the AGA path can
 * write BOTH halves of a 32px FMODE coarse block (two calls); OCS calls it once.
 * `a1` is the pitch-pointer-table cursor for the tile column at the current
 * cameraY row (advanced internally down the column).
 * ---------------------------------------------------------------- */
static void scrollXBlitColumn(uint8_t *a1, uint32_t destXByteOff)
{
    uint16_t d1, d2, d3;
    uint8_t *a0;

    {
        uint32_t yByteOff = (uint32_t)_cameraYMod320 * ROW_STRIDE_BYTES;
        a0 = (uint8_t *)_frontbufferBasePtr + destXByteOff + yByteOff;
    }

    /* Set up blitter for column copy (A -> D, no shift) */
    waitBlit();
    BLT_L(_custom->bltdpt, (uint32_t)a0);
    BLT_W(_custom->bltamod, 0);
    BLT_W(_custom->bltdmod, BLTDMOD_PLANE_STEP);
    BLT_W(_custom->bltafwm, BLT_FWM_ALL);
    BLT_W(_custom->bltalwm, BLT_FWM_ALL);
    BLT_W(_custom->bltcon1, 0);
    BLT_W(_custom->bltcon0, BLTCON0_USEA_USED_LF_F0);

    /* ---- Phase 1: top of frontbuffer (cameraYMod320 to SCREEN_HEIGHT) ---- */
    d2 = _cameraYMod320;
    d1 = d2 & TILE_MASK;
    if (d1 != 0) {
        d3 = TILE_HEIGHT - d1;
        d2 += d3;
        uint16_t srcOff = d1 << NUM_PLANES_SHIFT_PLUS1;
        uint16_t bltsize = ((d3 << NUM_PLANES_SHIFT) << 6) | 1;
        a0 = *(uint8_t **)a1 + srcOff;
        a1 += PITCH_PTR_ROW_BYTES;
        waitBlit();
        BLT_L(_custom->bltapt, (uint32_t)a0);
        BLT_W(_custom->bltsize, bltsize);
    }
    while (d2 < SCREEN_HEIGHT) {
        a0 = *(uint8_t **)a1;
        a1 += PITCH_PTR_ROW_BYTES;
        waitBlit();
        BLT_L(_custom->bltapt, (uint32_t)a0);
        BLT_W(_custom->bltsize, BLTSIZE_TILE_COL);
        d2 += TILE_HEIGHT;
    }

    /* ---- Phase 2: wrap to top of frontbuffer ---- */
    a0 = (uint8_t *)_frontbufferBasePtr + destXByteOff;
    waitBlit();
    BLT_L(_custom->bltdpt, (uint32_t)a0);
    d2 = _cameraYMod320;
    if (d2 >= TILE_HEIGHT) {
        do {
            a0 = *(uint8_t **)a1;
            a1 += PITCH_PTR_ROW_BYTES;
            waitBlit();
            BLT_L(_custom->bltapt, (uint32_t)a0);
            BLT_W(_custom->bltsize, BLTSIZE_TILE_COL);
            d2 -= TILE_HEIGHT;
        } while (d2 >= TILE_HEIGHT);
    }
    if (d2 != 0) {
        a0 = *(uint8_t **)a1;
        uint16_t bltsize = ((d2 << NUM_PLANES_SHIFT) << 6) | 1;
        waitBlit();
        BLT_L(_custom->bltapt, (uint32_t)a0);
        BLT_W(_custom->bltsize, bltsize);
    }
}
#endif /* !SWOS_SDL_BUILD */

#if defined(AGA) && !defined(SWOS_SDL_BUILD)
void ScrollYVergeRefresh(void);   /* verge-only margin refresh; defined after ScrollYUpdateRowsAndBlit */
#endif

/* ================================================================
 * ScrollXUpdateColumnsAndBlit  (SWOS2.s lines 5504-5678)
 *
 * Compares cameraX.s.i vs oldCameraX to detect horizontal scroll.
 * If a tile boundary is crossed, blits a column of pitch tiles
 * into the frontbuffer, then copies that column to the mask buffer.
 * ================================================================ */
__attribute__((used, externally_visible))
void ScrollXUpdateColumnsAndBlit(void)
{
#ifdef SWOS_SDL_BUILD
    /* Amiga's incremental scroll updates (blit one new column of pitch
     * tiles when the camera crosses a tile boundary) use 4-bitplane
     * interleaved pitch-tile data + Amiga blitter word writes. On SDL
     * the pitch data is being migrated to 8bpp chunky, and the whole
     * visible pitch gets redrawn each frame (matching swos-port-c's
     * DrawSprites approach). No-op this on SDL; the chunky redraw
     * handles scroll transparently. */
    return;
#else
    int16_t d0, d1;
    int16_t d5;   /* signed: right lead = +SCROLL_RIGHT_EDGE_X, AGA left lead = -32 */

    /* Check if camera moved horizontally */
    d0 = _cameraX.s.i;
    if (d0 == _oldCameraX) {
#if defined(AGA) && !defined(SWOS_SDL_BUILD)
        /* No X scroll this frame -> flush any column maintenance deferred from an
         * earlier tile-cross of this buffer (non-cross frame, empty vblank). */
        flushPendingColumn();
#endif
        return;
    }

#if defined(AGA) && !defined(SWOS_SDL_BUILD)
    /* Left-touchline verge refresh (see ScrollYVergeRefresh). ScrollY only refreshes the FMODE
     * verge margin (tile 0 -> byte 0) for the rows it copies, and is skipped on a horizontal-only
     * frame, so rows first exposed at camXMod>=32 keep a stale byte-0 that the display's -4
     * pull-back reads as the left "column strip". Refresh the whole-screen margin EVERY frame
     * while at the touchline -- this is what actually kills the strip (a once-per-transition
     * variant regressed it: the verge needs continuous refresh, e.g. across the double-buffers /
     * as the DATA columns roll). It is a full-screen CPU write, so at ~1 iteration/frame it's
     * ~1/10th of a full ScrollY refill; if the loop is ever unthrottled it multiplies (see the
     * 2026-07 rig-throttle notes) -- fix the loop rate, not this. */
    if (_cameraXMod320 < 32u)
        ScrollYVergeRefresh();
#endif

    if (d0 >= _oldCameraX) {
        /* Moved right */
        d5 = SCROLL_RIGHT_EDGE_X;
        d0 = _oldCameraX + SCROLL_RIGHT_EDGE_X;
        d0 >>= TILE_SHIFT_X;  /* asr.w */
        d1 = _cameraX.s.i + SCROLL_RIGHT_EDGE_X;
        d1 >>= TILE_SHIFT_X;  /* asr.w */
    } else {
        /* Moved left */
#if defined(AGA) && !defined(SWOS_SDL_BUILD)
        /* FMODE=1: the display reads a 32px LEFT margin (coarse-4) for its fine
         * scroll (the empty-column pull-back), applied only at cameraX >= 32. The
         * leftward-ENTERING column must land in that margin or it shows the white
         * strip -> write at the left leading edge (d5 = -32), mirroring the
         * rightward +335 lead. Below the guard (pitch left edge) the display does
         * not pull back, so keep 0 and never read pitch-x < 0. */
        d5 = (_cameraX.s.i >= 32) ? (int16_t)-32 : (int16_t)0;
#else
        d5 = 0;
#endif
        d0 = (int16_t)(_oldCameraX + d5) >> TILE_SHIFT_X;   /* asr.w (signed) */
        d1 = (int16_t)(_cameraX.s.i + d5) >> TILE_SHIFT_X;  /* asr.w (signed) */
    }

    /* Did we cross a tile boundary? */
    if (d1 == d0) {
        /* No tile boundary crossed — just update oldCameraX */
        _oldCameraX = _cameraX.s.i;
        _cameraCoordinatesValid = 0;
#if defined(AGA) && !defined(SWOS_SDL_BUILD)
        /* Non-cross frame: empty vblank -> flush this buffer's deferred column
         * maintenance (mirror + mask). Confining the flush to the two non-cross
         * early-returns guarantees it never shares a frame with the column-tile
         * blits (which alone fill the ~36-line vblank), so nothing overruns. */
        flushPendingColumn();
#endif
        return;
    }

    /* Tile boundary crossed — do the column blit */
    _cameraCoordinatesValid = 0;
    _oldCameraX = _cameraX.s.i;

    /* Pitch tile-row base + leading tile column (in tiles) */
    uint16_t tileRow = (uint16_t)_cameraY.s.i >> TILE_SHIFT_X;  /* lsr.w */
    uint32_t pitchRowOff = (uint32_t)tileRow * PITCH_PTR_ROW_BYTES;
    uint16_t leadTileUnits = ((uint16_t)(_cameraX.s.i + d5)) >> TILE_SHIFT_X;  /* lsr.w */

    /* Rolling leading-edge byte within the circular line buffer — faithful to
     * SWOS2.s (d0 = (cameraXMod320 + d5) >> 3, andi.l #WORD_ALIGN_EVEN_MASK).
     * NOT clamped to the plane row: the frontbuffer line is circular in X and the
     * display over-reads past it in lock-step; the write is one column ahead of
     * the visible right edge, exactly where the display reads it as it scrolls. */
    uint32_t destXByteOff =
        (uint32_t)(((uint16_t)(_cameraXMod320 + d5) >> 3) & (uint16_t)WORD_ALIGN_EVEN_MASK);
    /* Skip the per-plane verge margin: pitch DATA lives at byte
     * SCROLL_LEFT_MARGIN_BYTES.. of each plane row (matches the display + ScrollY). */
    destXByteOff += SCROLL_LEFT_MARGIN_BYTES;

#ifdef AGA
    /* FMODE=1: the display fetches on a 32px (4-byte) coarse grid, so a single
     * 16px column write leaves the OTHER half of that 32px block unwritten -> the
     * display reads a stale byte -> jump + white strip during incremental scroll.
     * ScrollY already snaps its fill to 32px (fmodeExtraTiles there); mirror that
     * here: write the FULL 32px block by blitting BOTH 16px tile columns (the one
     * now on the block's left + the leading one) at the 32px-aligned dest. */
    {
        uint32_t dest32 = destXByteOff & (uint32_t)(uint16_t)SCROLL_X_COARSE_ALIGN_MASK;
        uint16_t leftHalf = (uint16_t)(((uint16_t)(_cameraXMod320 + d5) >> 4) & 1u);
        uint16_t leftTileUnits = (uint16_t)(leadTileUnits - leftHalf);
        uint8_t *aL = _pitchDatBuffer + pitchRowOff + ((uint32_t)(uint16_t)(leftTileUnits << 2));
        uint8_t *aR = _pitchDatBuffer + pitchRowOff + ((uint32_t)(uint16_t)((leftTileUnits + 1u) << 2));
        scrollXBlitColumn(aL, dest32);
        scrollXBlitColumn(aR, dest32 + 2u);

        /* DEFER the 32px-wide (2-word) mirror (block-B elimination) + mask copy to
         * the next two non-cross frames of this buffer (they'd overrun vblank here). */
        recordPendingColumn(_frontbufferBasePtr + dest32);
    }
#else
    {
        uint8_t *a1 = _pitchDatBuffer + pitchRowOff + ((uint32_t)(uint16_t)(leadTileUnits << 2));
        scrollXBlitColumn(a1, destXByteOff);

        /* OCS: the 4-plane column is half as tall and fits vblank together with the
         * mask copy, so copy the column to the mask buffer inline (SWOS2.s Phase 3). */
        uint32_t srcAddr = _frontbufferBasePtr + destXByteOff;
        uint32_t dstAddr = srcAddr - _frontbufferBasePtr + (uint32_t)_frontbufferMaskBuffer;

        waitBlit();
        BLT_L(_custom->bltapt, srcAddr);
        BLT_L(_custom->bltdpt, dstAddr);
        BLT_W(_custom->bltamod, BLTDMOD_PLANE_STEP);
        BLT_W(_custom->bltsize, BLTSIZE_TILE_COL);

        /* ---- Final: 1-word dummy blit ---- */
        waitBlit();
        BLT_W(_custom->bltsize, 1);
    }
#endif /* AGA */
#endif /* !SWOS_SDL_BUILD */
}

/* ================================================================
 * ScrollYUpdateRowsAndBlit  (SWOS2.s lines 5682-5872)
 *
 * Compares cameraY.s.i vs oldCameraY to detect vertical scroll.
 * CPU-based row copy from pitch tile data into frontbuffer.
 * Blitter-based mask copy from frontbuffer to mask buffer.
 * ================================================================ */
__attribute__((used, externally_visible))
void ScrollYUpdateRowsAndBlit(void)
{
#ifdef SWOS_SDL_BUILD
    /* See ScrollXUpdateColumnsAndBlit above — same rationale. */
    return;
#else
    int16_t deltaY;
    uint16_t d4, d5, d6;
    uint16_t fmodeExtraTiles = 0;  /* AGA: extra left tile pulled in to 32px-align the fill */
    int32_t d3;       /* src scanline offset within tile (ext.l) */
    uint32_t d7;      /* dest stride per row */
    uint32_t xByteOff = 0;  /* frontbuffer X byte offset of the fill start */
    int16_t rowCount;
    uint8_t *a1;      /* pitch pointer table cursor */
    uint8_t *a3;      /* dest plane 0 */
#if defined(AGA) && !defined(SWOS_SDL_BUILD)
    uint8_t *a1_verge; /* pitch pointer table cursor for tile column 0 (verge margin) */
    uint8_t *a1_pb;    /* pitch pointer cursor for the -4 pull-back long's first tile */
    uint16_t pbDup;    /* 1 = pull-back is off-pitch -> repeat tile 0 (the old verge case) */
#endif
#ifndef AGA
    uint8_t *a4;      /* dest plane 1 */
    uint8_t *a5;      /* dest plane 2 */
    uint8_t *a6_ptr;  /* dest plane 3 */
#endif

    /* Check if camera moved vertically */
    deltaY = _cameraY.s.i - _oldCameraY;
    if (deltaY == 0)
        return;

    if (deltaY > 0) {
        /* Scrolled down: d5 = SCREEN_HEIGHT - deltaY (first dest row) */
        d5 = SCREEN_HEIGHT - deltaY;
    } else {
        /* Scrolled up: d5 = 0 (first dest row), deltaY = abs */
        d5 = 0;
        deltaY = -deltaY;
    }

    _oldCameraY = _cameraY.s.i;
    _cameraCoordinatesValid = 0;

    /* ---- Compute pitch data pointer ---- */
    a1 = _pitchDatBuffer;
    {
        uint16_t tileRow = (uint16_t)(_cameraY.s.i + d5) >> TILE_SHIFT_X;
        uint32_t pitchRowOff = (uint32_t)tileRow * PITCH_PTR_ROW_BYTES;
        a1 += pitchRowOff;
#if defined(AGA) && !defined(SWOS_SDL_BUILD)
        a1_verge = a1;   /* tile column 0 = the verge (pitch-x 0) */
#endif
        uint16_t tileCol = (uint16_t)_cameraX.s.i >> TILE_SHIFT_X;
        tileCol <<= 2;
        int32_t colOff = (int32_t)(int16_t)tileCol;  /* ext.l d0 */
        a1 += colOff;
    }

    /* ---- Compute frontbuffer dest base ---- */
    a3 = (uint8_t *)_frontbufferBasePtr;
    {
        xByteOff = (uint16_t)_cameraXMod320 >> 3;
        xByteOff &= WORD_ALIGN_EVEN_MASK;
#ifdef AGA
        /* FMODE=1 display fetches from a 32px (4-byte) aligned plane pointer, but
         * this fill is 16px (2-byte) aligned. When the leftmost visible tile sits
         * in the SECOND 16px of a 32px block, the display's coarse start is 16px
         * LEFT of where we fill -> it reads an unwritten byte -> the "vertical
         * strip". Snap the fill start DOWN to the same 32px coarse and pull in the
         * tile now on its left (source back one column + one extra word below) so
         * the full [coarse32, ...) the display reads is always written. */
        fmodeExtraTiles = ((uint16_t)_cameraXMod320 >> 4) & 1u;
        xByteOff &= (uint32_t)(uint16_t)SCROLL_X_COARSE_ALIGN_MASK;
        a1 -= (uint32_t)fmodeExtraTiles * 4u;
#endif
        /* Skip the per-plane verge margin: DATA at byte SCROLL_LEFT_MARGIN_BYTES.. */
        xByteOff += SCROLL_LEFT_MARGIN_BYTES;
        a3 += xByteOff;
        uint16_t yWrapped = _cameraYMod320 + d5;
        if (yWrapped >= SCREEN_HEIGHT)
            yWrapped -= SCREEN_HEIGHT;
        uint32_t yByteOff = (uint32_t)yWrapped * ROW_STRIDE_BYTES;
        a3 += yByteOff;
    }

#if defined(AGA) && !defined(SWOS_SDL_BUILD)
    /* Source for the FMODE -4 pull-back long (see the per-row write in the CPU loop
     * below): the two 16px pitch tiles immediately LEFT of the 32px-aligned fill
     * start. `a1 - a1_verge` is the fill's start tile column x4 (always a multiple
     * of 8 = 2 tiles, because the fill start is 32px-aligned), so < 8 means the
     * pull-back reads off the left edge of the pitch -> repeat tile column 0, which
     * is exactly what the old verge-margin write did. */
    if ((uint32_t)(a1 - a1_verge) >= 8u) {
        a1_pb = a1 - 8;
        pbDup = 0;
    } else {
        a1_pb = a1_verge;
        pbDup = 1;
    }
#endif

    /* ---- Determine word count and dest stride per row ---- */
    if (((uint16_t)_cameraX.s.i & 0xF) != 0) {
        d6 = 0x15;  /* 21 words (unaligned) */
        d7 = PITCH_DEST_MOD_UNALIGNED;
    } else {
        d6 = 0x14;  /* 20 words (aligned) */
        d7 = PITCH_DEST_MOD_ALIGNED;
    }
#ifdef AGA
    /* FMODE=1 wide buffer: fill the FULL 48-byte plane row (24 words) from the 32px
     * coarse, so the fine-scroll fetch margin (past the 336px visible window) is
     * never stale. Overrides the 16px aligned/unaligned word counts — those only
     * covered the visible width, leaving the FMODE margin unwritten. Modulo brings
     * each row back to the same coarse start (fill + modulo = ROW_STRIDE). */
    (void)fmodeExtraTiles;
    /* Fill the 48-byte DATA region (24 words), NOT the 52-byte row: the row now
     * carries a 4-byte verge margin (SCROLL_LEFT_MARGIN_BYTES) the DATA fill skips
     * (a3 already advanced past it via xByteOff). Modulo steps to the next row's
     * DATA start (fill + modulo = ROW_STRIDE). */
    d6 = (uint16_t)(FMODE_FETCH_BYTES / 2 - 1);            /* 24 words = 48 data bytes */
    d7 = (uint32_t)(ROW_STRIDE_BYTES - FMODE_FETCH_BYTES); /* 416 - 48 = 368 */
#endif

    /* ---- Init plane pointers ---- */
#ifndef AGA
    a4 = a3 + PLANE_STRIDE;
    a5 = a3 + PLANE_STRIDE_TIMES2;
    a6_ptr = a3 + PITCH_DEST_MOD_UNALIGNED;  /* plane 3 = a3 + 132 */
#endif

    /* ---- Compute wrapped dest Y position ---- */
    d4 = _cameraYMod320 + d5;
    if (d4 >= SCREEN_HEIGHT)
        d4 -= SCREEN_HEIGHT;

    /* ---- Init mask blit accumulators ---- */
    maskBlitDestPtr = (uint32_t)a3;
    blitWordCount = d6 + 1;
    maskBlitPendingBytesA = 0;
    maskBlitPendingBytesB = 0;
    maskBlitSecondStreamPtr = 0;

    /* ---- Compute source scanline offset within tile ---- */
    d3 = (int32_t)(uint16_t)(((uint16_t)(_cameraY.s.i + d5)) & TILE_MASK);
    d3 <<= NUM_PLANES_SHIFT_PLUS1;  /* * 8 bytes per scanline */

    /* ---- CPU row copy loop ---- */
    rowCount = deltaY - 1;  /* dbf counts down to -1 */
    do {
        int16_t wordIdx = d6;
        uint8_t *a1_saved = a1;

#if defined(AGA) && !defined(SWOS_SDL_BUILD)
        /* FMODE -4 pull-back long (32px / 2 tiles, at byte xByteOff-4 = a3-4).
         *
         * UpdatePlanePtrsAndBlitParams points the bitplane fetch one long EARLIER
         * than the 32px coarse so the extra left long sits under the BPLCON1
         * fine-scroll delay. This fill, though, starts AT the coarse — so those 4
         * bytes are written by nobody: plane p-1's 48-byte fill spills into plane p
         * only up to byte xByteOff-4, and plane p's own fill starts at xByteOff.
         * On a vertical scroll the newly-exposed rows therefore kept whatever was in
         * the circular buffer 272 rows earlier, which the pull-back then displays as
         * a stale column in the leftmost ~32px — the "vertical line down the pitch"
         * (most visibly the perimeter-hoarding tile left behind by a previous visit
         * to the left touchline, since that content is constant in y).
         *
         * Fill it per-row here (CPU, like the rest of this loop — never a blit, so it
         * cannot overrun vblank), from the two pitch tiles left of the fill start.
         * At the pitch's extreme-left edge there is no pitch further left, so pbDup
         * repeats tile column 0 — byte-for-byte the old verge-margin write this
         * replaces (there xByteOff == 4, so a3-4 IS the verge margin at byte 0).
         * Mirror-written too, like the main fill (2x-tall frontbuffer). */
#ifdef PITCH_SCROLL_TEST
        /* rig A/B: 0 = pre-fix behaviour (write only the left-edge verge margin). */
        extern int32_t g_pstPullbackFill;
        if (g_pstPullbackFill || pbDup)
#endif
        {
            uint8_t *psrc0 = *(uint8_t **)a1_pb + d3;
            uint8_t *psrc1 = pbDup ? psrc0 : (*(uint8_t **)(a1_pb + 4) + d3);
            uint8_t *m = a3 - 4;   /* this row's pull-back long */
            int16_t vp;
            for (vp = 0; vp < NUM_PLANES; vp++)
            {
                uint16_t v0 = *(uint16_t *)psrc0;
                uint16_t v1 = *(uint16_t *)psrc1;
                *(uint16_t *)(m + vp * PLANE_STRIDE)                              = v0;
                *(uint16_t *)(m + vp * PLANE_STRIDE + 2)                          = v1;
                *(uint16_t *)(m + vp * PLANE_STRIDE + FB_MIRROR_OFFSET_BYTES)     = v0;
                *(uint16_t *)(m + vp * PLANE_STRIDE + 2 + FB_MIRROR_OFFSET_BYTES) = v1;
                psrc0 += 2;
                psrc1 += 2;
            }
        }
#endif

        /* Inner loop: copy one row of words across all tiles */
        do {
            uint8_t *tilePtr = *(uint8_t **)a1 + d3;
            a1 += 4;  /* next tile pointer */

#ifdef AGA
            /* Copy one word per plane (8 planes, offset from a3). Double-write
             * into the mirror row (+FB_MIRROR_OFFSET_BYTES) so the 2x-tall
             * frontbuffer stays mirrored — lets the copper's block A display the
             * whole screen contiguously with no block-B reload (see display.h). */
            {
                int16_t p;
                for (p = 0; p < NUM_PLANES; p++)
                {
                    uint16_t v = *(uint16_t *)tilePtr;
                    *(uint16_t *)(a3 + p * PLANE_STRIDE) = v;
                    *(uint16_t *)(a3 + p * PLANE_STRIDE + FB_MIRROR_OFFSET_BYTES) = v;
                    tilePtr += 2;
                }
            }
            a3 += 2;
#else
            /* Copy one word per plane (4 planes interleaved) */
            *(uint16_t *)a3 = *(uint16_t *)tilePtr; tilePtr += 2;
            a3 += 2;
            *(uint16_t *)a4 = *(uint16_t *)tilePtr; tilePtr += 2;
            a4 += 2;
            *(uint16_t *)a5 = *(uint16_t *)tilePtr; tilePtr += 2;
            a5 += 2;
            *(uint16_t *)a6_ptr = *(uint16_t *)tilePtr;
            a6_ptr += 2;
#endif

            wordIdx--;
        } while (wordIdx >= 0);

        a1 = a1_saved;

        maskBlitPendingBytesA += NUM_PLANES;
        maskBlitPendingBytesB += NUM_PLANES;

        /* Advance source scanline within tile */
        d3 += SRC_SCANLINE_BYTES;
        if (d3 == TILE_SIZE) {
            /* Crossed tile boundary — advance pitch pointer row */
            a1 += PITCH_PTR_ROW_BYTES;
#if defined(AGA) && !defined(SWOS_SDL_BUILD)
            a1_verge += PITCH_PTR_ROW_BYTES;   /* keep the verge (tile 0) in step */
            a1_pb    += PITCH_PTR_ROW_BYTES;   /* and the pull-back source tiles */
#endif
            d3 = 0;
        }

        /* Advance dest pointers by row stride */
        a3 += d7;
#ifndef AGA
        a4 += d7;
        a5 += d7;
        a6_ptr += d7;
#endif

        /* Handle frontbuffer Y-wrap */
        d4 += 1;
        if (d4 == SCREEN_HEIGHT) {
            a3 -= FRONTBUFFER_SIZE_VISIBLE_BYTES;
#ifndef AGA
            a4 -= FRONTBUFFER_SIZE_VISIBLE_BYTES;
            a5 -= FRONTBUFFER_SIZE_VISIBLE_BYTES;
            a6_ptr -= FRONTBUFFER_SIZE_VISIBLE_BYTES;
#endif
            d4 = 0;
            maskBlitSecondStreamPtr = (uint32_t)a3;
            maskBlitPendingBytesB = 0;
        }

        rowCount--;
    } while (rowCount >= 0);

    /* ---- Blitter mask copy: frontbuffer → mask buffer ---- */
    waitBlit();
    BLT_W(_custom->bltafwm, BLT_FWM_ALL);
    BLT_W(_custom->bltalwm, BLT_FWM_ALL);
    BLT_W(_custom->bltcon1, 0);
    BLT_W(_custom->bltcon0, BLTCON0_USEA_USED_LF_F0);
    {
        uint16_t modVal = (PLANE_STRIDE_DIV2 - blitWordCount) * 2;
        BLT_W(_custom->bltamod, modVal);
        BLT_W(_custom->bltdmod, modVal);
    }

    /* If there's a second stream, subtract its bytes from stream A */
    if (maskBlitSecondStreamPtr != 0) {
        maskBlitPendingBytesA -= maskBlitPendingBytesB;
    }

    /* Set up source and dest for stream A */
    {
        uint32_t srcAddr = maskBlitDestPtr;
        uint32_t dstAddr = srcAddr - _frontbufferBasePtr + (uint32_t)_frontbufferMaskBuffer;

        waitBlit();
        BLT_L(_custom->bltapt, srcAddr);
        BLT_L(_custom->bltdpt, dstAddr);
    }

    /* ---- Stream A: blit in batches of max 0x400 rows ---- */
    for (;;) {
        uint16_t batch = maskBlitPendingBytesA;
        if (batch == 0)
            break;
        if (batch > 0x400)
            batch = 0x400;
        maskBlitPendingBytesA -= batch;

        uint16_t bltsize = (batch << 6) | blitWordCount;

        waitBlit();
        BLT_W(_custom->bltsize, bltsize);
    }

    /* ---- Stream B (if Y-wrap occurred) ---- */
    if (maskBlitSecondStreamPtr == 0)
        return;
    if (maskBlitPendingBytesB == 0)
        return;

    {
        uint32_t srcAddr = maskBlitSecondStreamPtr;
        uint32_t dstAddr = srcAddr - _frontbufferBasePtr + (uint32_t)_frontbufferMaskBuffer;

        waitBlit();
        BLT_L(_custom->bltapt, srcAddr);
        BLT_L(_custom->bltdpt, dstAddr);
    }

    /* ---- Stream B batches ---- */
    for (;;) {
        uint16_t batch = maskBlitPendingBytesB;
        if (batch == 0)
            return;
        if (batch > 0x400)
            batch = 0x400;
        maskBlitPendingBytesB -= batch;

        uint16_t bltsize = (batch << 6) | blitWordCount;

        waitBlit();
        BLT_W(_custom->bltsize, bltsize);
    }
#endif /* !SWOS_SDL_BUILD */
}

#if defined(AGA) && !defined(SWOS_SDL_BUILD)
/* ================================================================
 * ScrollYVergeRefresh
 *
 * Rewrites ONLY the FMODE left-verge margin -- pitch tile column 0 -> byte 0 of every visible
 * row (+ its 2x-tall mirror) -- for the WHOLE screen, WITHOUT the full-row data copy or mask
 * blit that ScrollYUpdateRowsAndBlit does. ScrollY refreshes the verge per-row but only for the
 * deltaY rows it copies, and early-returns on a horizontal-only frame (deltaY==0). So a
 * left-scroll into the touchline (camXMod<32) leaves rows first exposed at camXMod>=32 with a
 * stale byte-0 that the display's FMODE -4 pull-back reads as the left "column strip". This
 * refresh costs ~1/10th of the full-screen ScrollY refill the earlier camera.c fix used, and
 * (unlike that fix) never touches _oldCameraX/_oldCameraY. Setup mirrors ScrollY at d5==0.
 * ================================================================ */
void ScrollYVergeRefresh(void)
{
    /* pitch tile-0 pointer for the top visible tile row (the verge = pitch-x 0) */
    uint8_t *a1_verge = _pitchDatBuffer +
        (uint32_t)((uint16_t)_cameraY.s.i >> TILE_SHIFT_X) * PITCH_PTR_ROW_BYTES;

    /* frontbuffer dest: same coarse-32 align + margin skip ScrollY uses, so (a3 - xByteOff)
     * lands on byte 0 (the verge) of each plane row. */
    uint32_t xByteOff = (uint16_t)_cameraXMod320 >> 3;
    xByteOff &= WORD_ALIGN_EVEN_MASK;
    xByteOff &= (uint32_t)(uint16_t)SCROLL_X_COARSE_ALIGN_MASK;
    xByteOff += SCROLL_LEFT_MARGIN_BYTES;
    uint8_t *a3 = (uint8_t *)_frontbufferBasePtr + xByteOff
                + (uint32_t)_cameraYMod320 * ROW_STRIDE_BYTES;

    /* source scanline offset within the tile (8 bytes/scanline on AGA) */
    int32_t d3 = (int32_t)(uint16_t)((uint16_t)_cameraY.s.i & TILE_MASK) << NUM_PLANES_SHIFT_PLUS1;

    uint16_t d4 = _cameraYMod320;
    int16_t rowCount = SCREEN_HEIGHT - 1;
    do {
        uint8_t *vsrc = *(uint8_t **)a1_verge + d3;
        uint8_t *m = a3 - xByteOff;   /* byte 0 of this row = the verge margin */
        int16_t vp;
        for (vp = 0; vp < NUM_PLANES; vp++) {
            uint16_t vv = *(uint16_t *)vsrc;
            *(uint16_t *)(m + vp * PLANE_STRIDE)                              = vv;
            *(uint16_t *)(m + vp * PLANE_STRIDE + 2)                          = vv;
            *(uint16_t *)(m + vp * PLANE_STRIDE + FB_MIRROR_OFFSET_BYTES)     = vv;
            *(uint16_t *)(m + vp * PLANE_STRIDE + 2 + FB_MIRROR_OFFSET_BYTES) = vv;
            vsrc += 2;
        }
        /* advance source scanline; cross a tile -> next pitch-pointer row */
        d3 += SRC_SCANLINE_BYTES;
        if (d3 == TILE_SIZE) {
            a1_verge += PITCH_PTR_ROW_BYTES;
            d3 = 0;
        }
        a3 += ROW_STRIDE_BYTES;
        if (++d4 == SCREEN_HEIGHT) {
            a3 -= FRONTBUFFER_SIZE_VISIBLE_BYTES;
            d4 = 0;
        }
    } while (--rowCount >= 0);
}
#endif /* AGA && !SWOS_SDL_BUILD */
