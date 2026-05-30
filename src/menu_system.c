/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "menu_system.h"
#include "display.h"
#include "game_utils.h"
#include "fade.h"
#include "text_render.h"
#include "sprites.h"
#include "blitter.h"

/* ---- Callback function pointer type ---- */
typedef void (*VoidFunc)(void);
/* VoidFuncD1D2 removed: FUNC1/FUNC2 callbacks now use _CallMenuFuncWithXY */


/* ---- External globals (all live in SWOS2.s) ---- */
extern uint8_t currentMenu[];
extern MenuEntry menuEntries[];
extern uintptr_t currentMenuPtr;
extern volatile uint16_t menuStatus;
extern uint16_t menuFade;
extern volatile uint16_t exitMenu;
extern volatile uint16_t seasonUpdateInProgress;

#include "swos_types.h"
#include "auto_skip_debug.h"
#include "input.h"  /* pauseSkipRequested -- SPACE / second controller button */
#include <string.h>  /* memcpy in _UnpackMenu — Amiga string.h is partial,
                      * include early so the prototype is in scope before
                      * the first call site. */

/* ---- Blitter register write macros ----
 * GCC m68k 15.1.0 decomposes `volatile uint16_t = N` struct-member
 * stores into byte-pair `move.b` instructions in some contexts. For
 * hardware-trigger registers (writing `bltsize` starts the blitter),
 * the decomposed write triggers the blit mid-update with a half-formed
 * size — fill/copy operations either do nothing or write garbage.
 * Same macros as in blitter.c / sprites.c / scroll_blit.c.  */
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

/* Season-update menu hold: on Amiga, the slow CPU + blitter naturally
 * stretched each cup/league result menu render to ~0.5-1 s. On x86 the
 * controlMask=0xFFFF auto-fire dispatches in the first MenuProc iteration,
 * so the season-transition chain visually skips in milliseconds. Hold each
 * menu for SEASON_UPDATE_MENU_HOLD_FRAMES iterations (50 Hz pacing) so the
 * user can read each result like on the original.
 *
 * Counter resets in _ActivateMenu (per-menu boundary); _MenuProc gates the
 * auto-fire dispatch behind it when seasonUpdateInProgress is set. Only
 * applies to controlMask==0xFFFF entries — every other dispatch path
 * (normal fire-only, navigation, etc.) is unaffected. */
#define SEASON_UPDATE_MENU_HOLD_FRAMES 50
static uint16_t s_seasonUpdateHoldCounter;

/* SDL-only: latched once SPACE / second controller button is pressed
 * during a season update, so the user can press once and let go — the
 * skip persists across every remaining menu in the chain. Auto-clears
 * when seasonUpdateInProgress drops back to 0. */
static uint16_t s_seasonSkipLatched;
extern uint16_t menuCursorFrame;
extern uint16_t menuTextBlinkCounter;
extern uint16_t controlMask;
extern volatile uint16_t fire;
extern volatile uint16_t shortFire;
extern uint8_t left;
extern uint8_t right;
extern uint8_t up;
extern uint8_t down;
extern int16_t menuControlsDirection;
extern int16_t menuControlsDirection2;
extern uint16_t kColorTableShine[];
extern uint16_t nextEntryDirectionOffsets[];
extern uint32_t magicValue;
extern uint16_t panicReasonCode;
extern uint32_t irqCookieObfuscated;
extern char *errorMessagePtr;
extern uint16_t forceDrawTransparentMenuItems;

/* _MenuCheckControls globals */
extern volatile uint16_t currentKey;
extern uint16_t lastFrameTicks;
extern int16_t fireCounter;
extern uint16_t player1ClearFlag;
extern uint16_t player2ClearFlag;
extern uint16_t twoPlayers;
extern uint16_t fireResetFlag;
extern uint8_t pl1Fire;
extern uint8_t pl1ShortFire;
extern int16_t pl1FireCounter;
extern uint16_t pl1SkipFireCounter;
extern int16_t pl1Direction;
extern uint8_t pl1Left;
extern uint8_t pl1Right;
extern uint8_t pl1Up;
extern uint8_t pl1Down;
extern uint8_t pl2Fire;
extern uint8_t pl2ShortFire;
extern int16_t pl2FireCounter;
extern uint16_t pl2SkipFireCounter;
extern int16_t pl2Direction;
extern uint8_t pl2Left;
extern uint8_t pl2Right;
extern uint8_t pl2Up;
extern uint8_t pl2Down;
extern int16_t lastControls;
extern uint16_t controlsHeldTimer;
extern uint16_t longFireTime;
extern uint16_t longFireFlag;
extern uint32_t allowShorterMenuItemsWithFrames;

/* UnpackMenu globals */
extern uint16_t menuX;
extern uint16_t menuY;
extern uint8_t templateMenuEntry[];
extern uint32_t menuFreeSpace;
extern uint32_t menuDataOffset;
extern uint16_t deadVar03;
extern uint8_t *spriteGraphicsPtr;

/* Platform quit flag: set by Platform_RequestQuit on SDL_QUIT / ESC /
 * X button click. _ShowMenu loops check it to break out of nested menus. */
extern uint16_t g_quitToDos;

#ifdef SWOS_DEBUG_AUTOSKIP
/* Map a packed-menu pointer to a human name for the autoskip log. */
extern uint8_t diyCupMenu[], diyLeagueTournamentMenu[], chairmanMemosMenu[];
extern uint8_t careerMenu[], contestMenu[], careerOverMenu[], continueMenu[];
extern uint8_t continueAbortMenu[], chooseTeamsMenu[], selectFilesMenu[];
extern uint8_t designDiyCupMenu[], designDiyLeagueMenu[], clubBusinessMenu[];
extern uint8_t topGoalScorersMenu[], competitionTopGoalScorersMenu[];
extern uint8_t squadMenu[], competitionStatsMenu[], viewCompetitionsMenu[];
extern uint8_t managementRecordMenu[];
extern uint8_t offersMenu[];
extern uint8_t swosMainMenu[];
extern uint8_t gotJobOfferMenu[];
static const char *swos_autoskip_menu_name(const void *p)
{
    if (p == diyCupMenu) return "diyCupMenu";
    if (p == diyLeagueTournamentMenu) return "diyLeagueTournamentMenu";
    if (p == chairmanMemosMenu) return "chairmanMemosMenu";
    if (p == careerMenu) return "careerMenu";
    if (p == contestMenu) return "contestMenu";
    if (p == careerOverMenu) return "careerOverMenu";
    if (p == continueMenu) return "continueMenu";
    if (p == continueAbortMenu) return "continueAbortMenu";
    if (p == chooseTeamsMenu) return "chooseTeamsMenu";
    if (p == selectFilesMenu) return "selectFilesMenu";
    if (p == designDiyCupMenu) return "designDiyCupMenu";
    if (p == designDiyLeagueMenu) return "designDiyLeagueMenu";
    if (p == clubBusinessMenu) return "clubBusinessMenu";
    if (p == topGoalScorersMenu) return "topGoalScorersMenu";
    if (p == competitionTopGoalScorersMenu) return "competitionTopGoalScorersMenu";
    if (p == squadMenu) return "squadMenu";
    if (p == competitionStatsMenu) return "competitionStatsMenu";
    if (p == viewCompetitionsMenu) return "viewCompetitionsMenu";
    if (p == managementRecordMenu) return "managementRecordMenu";
    if (p == offersMenu) return "offersMenu";
    if (p == swosMainMenu) return "swosMainMenu";
    if (p == gotJobOfferMenu) return "gotJobOfferMenu";
    return "unknown";
}
#else
static inline const char *swos_autoskip_menu_name(const void *p) { (void)p; return ""; }
#endif

/* GetKey globals */
extern volatile uint16_t keyCount;
extern volatile uint8_t keyBuffer[];
extern uint8_t keycodeLut[];

/* BlitterCopyOverlayToScreen globals */
extern volatile uintptr_t frontbufferBasePtr;
extern uint8_t frontbufferBase[];
extern uint8_t frontbufferBaseAlt[];
extern uint8_t frontbufferMaskBuffer[];

/* DrawMenuItemOuterFrame data */
extern uint8_t outerFrameColorsTable[];

/* DrawMenuItem data */
extern void *charTables[];
extern uint8_t numbersBuffer[];
extern uint32_t smallCharsTable[];

/* diyLeagueScratchBuffer boundary marker */
extern volatile uint8_t diyLeagueScratchBuffer[];

/* loc_1215BC label (anti-tamper jump target in RestorePreviousMenu stub) */
extern void loc_1215BC(void);

/* ---- External asm functions (C-to-asm bridges still needed) ---- */
/* _DrawSprite16Pixels declared in sprites.h */

/* External C functions (already ported) */
extern void _WaitForVSync(void);
extern void _FadeIn(void);
extern void _BlitterCopyPanelToScreen2(void);
extern void _Player1StatusProc(void);
extern void _Player2StatusProc(void);
extern void _PanicHaltLoop(void);

/* ---- String constant for stack overflow ---- */
extern uint8_t aStackOverflow[];


/* ================================================================
 * CalcMenuEntryAddress
 * ================================================================ */
__attribute__((used, externally_visible))
MenuEntry *_CalcMenuEntryAddress(uint16_t ordinal)
{
    return (MenuEntry *)(currentMenu + MENU_HEADER_SIZE + (uint32_t)ordinal * MENUENTRY_STRIDE);
}

/* ================================================================
 * SetExitMenuFlag
 * ================================================================ */
__attribute__((used, externally_visible))
void _SetExitMenuFlag(void)
{
    SWOS_AUTOSKIP_LOG("SetExitMenuFlag: caller=%p exitMenu now 1",
                      __builtin_return_address(0));
    exitMenu = 1;
}

/* ================================================================
 * SetCurrentEntry
 * ================================================================ */
__attribute__((used, externally_visible))
void _SetCurrentEntry(uint16_t index)
{
    MenuEntry *entry = _CalcMenuEntryAddress(index);
    Menu *menu = (Menu *)currentMenu;
    menu->selectedEntry = entry;
    SWOS_AUTOSKIP_LOG("SetCurrentEntry(%u) caller=%p ctrlMask=0x%04x onSelect=%p",
                      (unsigned)index, __builtin_return_address(0),
                      (unsigned)entry->controlMask,
                      (void *)(uintptr_t)entry->onSelect);
}

/* ================================================================
 * Int2Ascii — convert uint16 to decimal ASCII string
 * ================================================================ */
__attribute__((used, externally_visible))
void _Int2Ascii(uint16_t value, uint8_t *out)
{
    uint16_t d0 = value;
    uint16_t q;

    if (d0 >= 1000) {
        q = d0 / 1000;
        *out++ = (uint8_t)(q + '0');
        d0 = d0 % 1000;
    }
    if (value >= 100) {
        q = d0 / 100;
        *out++ = (uint8_t)(q + '0');
        d0 = d0 % 100;
    }
    if (value >= 10) {
        q = d0 / 10;
        *out++ = (uint8_t)(q + '0');
        d0 = d0 % 10;
    }
    *out++ = (uint8_t)(d0 + '0');
    *out = 0;
}

/* ================================================================
 * GetKey — read from keyboard buffer, lookup via keycodeLut
 *
 * PC-relative data (keyCount, keyBuffer, keycodeLut) accessed as
 * globals since they are now exported via xdef.
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _GetKey(void)
{
    uint16_t d0;

    d0 = 0;

    currentKey = keyCount;
    if (keyCount == 0)
        return 0;

read_next_key:
    /* Disable keyboard interrupt */
    custom->intena = 8;

    d0 = (uint16_t)keyBuffer[0];

    /* Shift buffer down (9 bytes) */
    keyBuffer[0] = keyBuffer[1];
    keyBuffer[1] = keyBuffer[2];
    keyBuffer[2] = keyBuffer[3];
    keyBuffer[3] = keyBuffer[4];
    keyBuffer[4] = keyBuffer[5];
    keyBuffer[5] = keyBuffer[6];
    keyBuffer[6] = keyBuffer[7];
    keyBuffer[7] = keyBuffer[8];
    keyBuffer[8] = keyBuffer[9];

    keyCount--;

    /* Re-enable keyboard interrupt */
    custom->intena = 0x8008;

    d0 = (uint16_t)keycodeLut[d0];
    if (d0 == 0x3F) {
        /* Invalid key — skip and try next */
        if (keyCount != 0)
            goto read_next_key;
        d0 = 0;
    }

    currentKey = d0;
    return d0;
}

/* ================================================================
 * BlitterCopyOverlayToScreen — hardware blitter copy
 * ================================================================ */
#ifdef SWOS_SDL_BUILD
/* SDL build: the Amiga blitter is a no-op (writes go to a stub
 * Custom chip struct), so the original asm-style routine below would
 * silently do nothing. Replace with a plain memcpy of the full
 * interleaved frontbuffer. The source-select logic mirrors the asm:
 * if `frontbufferBasePtr` is currently the primary buffer, the source
 * is the alt buffer; otherwise source = primary. Either way we copy
 * onto whatever `frontbufferBasePtr` currently points at. */
#include <string.h>
__attribute__((used, externally_visible))
void _BlitterCopyOverlayToScreen(void)
{
    /* Called at the end of _DrawMenu (and at the end of _LoadFileEx).
     * By that point _WaitForVSync has already swapped buffers, so
     * `frontbufferBasePtr` points at the POST-swap buffer (the one the
     * SDL renderer will now read from), while the OTHER buffer still
     * holds the freshly-drawn menu. We propagate the drawing forward
     * so the newly-active buffer shows the same content on the next
     * render tick. Full-frame copy matches the Amiga
     * BLTSIZE_OVERLAY_TO_SCREEN variant used inside LoadFile; the
     * BLTSIZE_OVERLAY_PANEL (30-row) variant Amiga-DrawMenu uses
     * works there because the bitplane pointers still point at the
     * just-drawn buffer post-swap -- on our SDL renderer that
     * isn't true, so we need the full sync. */
    uint8_t *src = (frontbufferBasePtr != (uintptr_t)frontbufferBaseAlt)
                       ? frontbufferBaseAlt
                       : frontbufferBase;
    uint8_t *dst = (uint8_t *)frontbufferBasePtr;
    if (src && dst && src != dst)
        memcpy(dst, src, FRONTBUFFER_SIZE_TOTAL_BYTES);
}
#else
__attribute__((used, externally_visible))
void _BlitterCopyOverlayToScreen(void)
{
    uint32_t srcAddr;

    /* Enable blitter DMA */
    custom->dmacon = (uint16_t)DMACON_BLT_ON;

    /* Wait for blitter idle (dummy read + busy-wait) */
    (void)(custom->dmaconr & (1u << 14));
    while (custom->dmaconr & (1u << 14))
        ;

    /* Disable blitter DMA for setup */
    custom->dmacon = (uint16_t)DMACON_BLT_OFF;

    /* Setup blitter registers */
    BLT_W(custom->bltalwm, BLT_FWM_ALL);
    BLT_W(custom->bltafwm, BLT_FWM_ALL);
    BLT_W(custom->bltcon1, 0);
    BLT_W(custom->bltcon0, BLTCON0_USEA_USED_LF_F0);
    /* Select source: if frontbuffer is Alt, use Base; else use Alt */
    srcAddr = (uintptr_t)frontbufferBase;
    if (frontbufferBasePtr != (uintptr_t)frontbufferBaseAlt)
        srcAddr = (uintptr_t)frontbufferBaseAlt;

#ifdef AGA
    /* AGA: per-plane blit with interleaved stride */
    BLT_W(custom->bltamod, (uint16_t)(ROW_STRIDE_BYTES - PLANE_STRIDE));
    BLT_W(custom->bltdmod, (uint16_t)(ROW_STRIDE_BYTES - PLANE_STRIDE));
    {
        uint8_t *srcPlane = (uint8_t *)srcAddr;
        uint8_t *dstPlane = (uint8_t *)frontbufferBasePtr;
        int16_t plane;
        for (plane = NUM_PLANES_MINUS1; plane >= 0; plane--)
        {
            custom->dmacon = (uint16_t)DMACON_BLT_ON;
            while (custom->dmaconr & (1u << 14)) {}
            custom->dmacon = (uint16_t)DMACON_BLT_OFF;

            BLT_L(custom->bltapt, (uint32_t)srcPlane);
            BLT_L(custom->bltdpt, (uint32_t)dstPlane);
            BLT_W(custom->bltsize, (uint16_t)BLTSIZE_OVERLAY_TO_SCREEN);
            srcPlane += PLANE_STRIDE;
            dstPlane += PLANE_STRIDE;
        }
    }
    /* Wait for last blit */
    custom->dmacon = (uint16_t)DMACON_BLT_ON;
    while (custom->dmaconr & (1u << 14)) {}
    custom->dmacon = (uint16_t)DMACON_BLT_OFF;
#else
    BLT_W(custom->bltamod, 0);
    BLT_W(custom->bltdmod, 0);
    /* Set destination to current frontbuffer */
    BLT_L(custom->bltdpt, frontbufferBasePtr);
    BLT_L(custom->bltapt, srcAddr);
    /* Start blit */
    BLT_W(custom->bltsize, BLTSIZE_OVERLAY_PANEL);
    /* Enable blitter DMA */
    custom->dmacon = (uint16_t)DMACON_BLT_ON;

    /* Wait for blit to finish (dummy read + busy-wait) */
    (void)(custom->dmaconr & (1u << 14));
    while (custom->dmaconr & (1u << 14))
        ;

    /* Disable blitter DMA */
    custom->dmacon = (uint16_t)DMACON_BLT_OFF;

    /* Reset blit size */
    BLT_W(custom->bltsize, 0x16);
#endif
}
#endif /* SWOS_SDL_BUILD */

/* ================================================================
 * DrawMenuItemBackground — fills rectangle via FillRectangle
 *
 * Input: d0=color, d1=x, d2=y, d3=width, d4=height
 * Reorders to: x, y, w, h, color, buffer
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawMenuItemBackground(uint16_t color, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    _FillRectangle(x, y, w, h, color);
}

/* ================================================================
 * DrawMenuItemBackgroundMasked — masked rectangle
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawMenuItemBackgroundMasked(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    _RectRowMaskedApplyScreenOffset(x, y, w, h, frontbufferMaskBuffer, (uint8_t *)frontbufferBasePtr);
}

/* ================================================================
 * DrawMenuItemInnerFrame — 1-pixel inner frame (4 border strips)
 *
 * d0=color, d1=x, d2=y, d3=width, d4=height
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawMenuItemInnerFrame(uint16_t color, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    /* Top border (full width, height=1) */
    _DrawMenuItemBackground(color, x, y, w, 1);
    /* Left border (width=1, full height) */
    _DrawMenuItemBackground(color, x, y, 1, h);
    /* Bottom border (full width, height=1) */
    _DrawMenuItemBackground(color, x, y + h - 1, w, 1);
    /* Right border (width=1, full height) */
    _DrawMenuItemBackground(color, x + w - 1, y, 1, h);
}

/* ================================================================
 * DrawMenuItemOuterFrame — 4-pixel colored frame using color table
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawMenuItemOuterFrame(uint16_t color, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    uint8_t topLeftColor, bottomRightColor;

    topLeftColor = outerFrameColorsTable[color * 2];
    bottomRightColor = outerFrameColorsTable[color * 2 + 1];

    /* Top border (full width, height=1) */
    _DrawMenuItemBackground((uint16_t)(int8_t)topLeftColor, x, y, w, 1);
    /* Left border (width=1, full height) */
    _DrawMenuItemBackground((uint16_t)(int8_t)topLeftColor, x, y, 1, h);
    /* Bottom border (full width, height=1) */
    _DrawMenuItemBackground((uint16_t)(int8_t)bottomRightColor, x, y + h - 1, w, 1);
    /* Right border (width=1, full height) */
    _DrawMenuItemBackground((uint16_t)(int8_t)bottomRightColor, x + w - 1, y, 1, h);
}

/* ================================================================
 * DrawMenuBackWithCheck — conditional background erase
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawMenuBackWithCheck(MenuEntry *entry)
{
    if (entry->isInvisible == 0)
        return;
    if (entry->alreadyDrawn == 0)
        return;

    entry->alreadyDrawn = entry->alreadyDrawn << 8;

    if (entry->dwWidth == 0)
        return;
    if (entry->dwHeight == 0)
        return;

    if (forceDrawTransparentMenuItems) {
        _DrawMenuItemBackground(15, entry->x, entry->y, entry->dwWidth, entry->dwHeight);
    } else {
        _DrawMenuItemBackgroundMasked(entry->x, entry->y, entry->dwWidth, entry->dwHeight);
    }
}

/* ================================================================
 * DrawBackAllItems — draw background for invisible items
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawBackAllItems(uint16_t maxItems, MenuEntry *firstEntry)
{
    int16_t i;
    MenuEntry *entry = firstEntry;

    for (i = (int16_t)maxItems - 1; i >= 0; i--) {
        if (entry->isInvisible != 0) {
            _DrawMenuBackWithCheck(entry);
        }
        entry = (MenuEntry *)((uint8_t *)entry + MENUENTRY_STRIDE);
    }
}

/* ================================================================
 * CallDrawMenuItem — double-buffered draw with vsync
 * ================================================================ */
__attribute__((used, externally_visible))
void _CallDrawMenuItem(MenuEntry *entry)
{
#ifdef SWOS_SDL_BUILD
    /* The Amiga "double-flush" pattern (draw, vsync, draw, vsync) is
     * how the original code keeps both buffers in sync after a partial
     * redraw — each draw goes to one buffer; the swap inside vsync
     * makes the OTHER buffer the new drawing target, so a second draw
     * lands in it too.
     *
     * On SDL each `_WaitForVSync` is a visible frame present. Doing
     * the double-flush emits TWO present cycles per partial redraw,
     * and a transition like _PlayMatchChangeTactics chains 5+ of
     * these — every present shows slightly different intermediate
     * state and the user sees flicker.
     *
     * Same end-state, fewer visible frames: draw once, vsync (which
     * presents the new state), then memcpy the displayed buffer to
     * the now-drawing-target buffer so both are in sync. No second
     * present required. */
    _DrawMenuBackWithCheck(entry);
    _DrawMenuItem(entry);
    _WaitForVSync();
    _BlitterCopyOverlayToScreen();
#else
    _DrawMenuBackWithCheck(entry);
    _DrawMenuItem(entry);
    _WaitForVSync();
    _DrawMenuBackWithCheck(entry);
    _DrawMenuItem(entry);
    _WaitForVSync();
#endif
}

/* ================================================================
 * DrawMenuItem — main menu item renderer
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawMenuItem(MenuEntry *entry)
{
    uint16_t bg, content;
    uint16_t sprIdx;
    uint8_t *textPtr;
    uint8_t *charTable;
    uint16_t flags, flagsMask, textY, textX, textColor;
    uint16_t charHeight, entryHeight;

    if (entry->isInvisible)
        return;

    /* Mark as drawn */
    entry->alreadyDrawn = 1;

    /* Call beforeDraw callback — a5 = entry in original asm */
    if (entry->beforeDraw != 0) {
        _CallMenuOnSelect((void *)entry->beforeDraw, entry);
        if (entry->isInvisible)
            return;
    }

    /* deadVar03 check — if set, skip to afterDraw */
    if (deadVar03 != 0)
        goto post_draw_callback;

    /* ---- Background rendering ---- */
    bg = entry->background;

    if (bg != NO_BACKGROUND) {
        if (bg == ENTRY_FRAME_AND_BACK_COLOR) {
            uint16_t x = entry->x;
            uint16_t y = entry->y;
            uint16_t w = entry->dwWidth;
            uint16_t h = entry->dwHeight;
            uint16_t bgColor = entry->bgColor & 0xF;

            if (bgColor == 0) {
                if (forceDrawTransparentMenuItems) {
                    _DrawMenuItemBackground(YELLOW_GREEN_BLACK_15, x, y, w, h);
                } else {
                    _DrawMenuItemBackgroundMasked(x, y, w, h);
                }
            } else {
                if (h <= 8) {
                    _DrawMenuItemBackground(bgColor, x, y, w, h);
                } else {
                    _DrawMenuItemBackground(bgColor, x, y, w, h);
                    _DrawMenuItemOuterFrame(bgColor, x, y, w, h);
                }

                /* Inner frame if high nibble is non-zero */
                {
                    uint16_t frameColor = (entry->bgColor & 0xF0) >> 4;
                    if (frameColor != 0) {
                        _DrawMenuItemInnerFrame(frameColor, x + 1, y + 1, w - 2, h - 2);
                    }
                }
            }
        } else if (bg == ENTRY_SPRITE1) {
            uint16_t sprIdx = entry->bgColor;
            _DrawSprite16Pixels(sprIdx, entry->x, entry->y);
        } else if (bg == ENTRY_FUNC1) {
            /* Original asm passes x in d1, y in d2 as register params.
             * The unpacker stored the function pointer as a 32-bit value
             * in the bg slot; reading the field directly is portable. */
            _CallMenuFuncWithXY((void *)entry->bg, entry->x, entry->y);
        } else {
            panicReasonCode = 0x1209;
            _PanicHaltLoop();
            return;
        }
    }

    /* ---- Foreground/content rendering ---- */
    content = entry->content;

    if (content == NO_FOREGROUND)
        goto post_draw_callback;

    if (content == ENTRY_STRING) {
        /* 0xFFFFFFFF is the sentinel set by MENU_CMD_TEXT_NO_PTR (opcode
         * 22). Original flow: the game runtime stores the real pointer
         * (e.g. the selected team's name for the START button) before
         * drawing; until something overwrites the sentinel we must not
         * dereference it. */
        if (entry->fg == 0 || entry->fg == 0xFFFFFFFF)
            goto post_draw_callback;
        textPtr = (uint8_t *)entry->fg;
        goto fg_text_common;
    }

    if (content == ENTRY_SPRITE2) {
        sprIdx = entry->fgWord;
        goto fg_sprite_draw;
    }

    if (content == ENTRY_STRING_TABLE) {
        /* Layout: { void *indexPtr; uint16_t base; void *entries[N]; }
         * (packed). On 32-bit indexPtr at 0, base at 4, entries at 6.
         * On 64-bit indexPtr at 0 (8 bytes), base at 8, entries at 10.
         * Use sizeof() of the header so the offsets stay correct on
         * both targets. The auto-generated tables in
         * swos_amiga_ptr_imports.c declare matching packed struct
         * types — see e.g. diyPitchTypeOrSeasonStrTable. */
        struct __attribute__((packed)) MenuStringTableHeader {
            void    *indexPtr;
            uint16_t base;
        };
        struct MenuStringTableHeader *hdr =
            (struct MenuStringTableHeader *)entry->fg;
        uint16_t idx = *(uint16_t *)hdr->indexPtr;
        uint16_t base = hdr->base;
        void **entries = (void **)((uint8_t *)hdr + sizeof(*hdr));

        idx -= base;
        textPtr = (uint8_t *)entries[idx];
        goto fg_text_common;
    }

    if (content == ENTRY_MULTI_LINE_TEXT) {
        uint8_t *multiData;
        int16_t numLines, lineSpacing;
        uint16_t mlY;

        if (entry->fg == 0)
            goto post_draw_callback;

        flags = entry->stringFlags;
        flagsMask = (flags & STR_FLAGS_MASK) >> 2;
        charTable = (uint8_t *)charTables[flagsMask / 4];
        charHeight = *(uint16_t *)(charTable + CHARTABLE_CHARHEIGHT);

        multiData = (uint8_t *)entry->fg;
        numLines = (int16_t)(int8_t)*multiData++;
        if (numLines == 0)
            goto post_draw_callback;

        /* Calculate vertical spacing */
        {
            int32_t freeSpace = (int32_t)entry->dwHeight - (int32_t)charHeight * numLines;
            lineSpacing = (int16_t)(freeSpace / (int16_t)(numLines + 1));
        }

        textX = (entry->dwWidth >> 1) + entry->x;
        mlY = entry->y + (uint16_t)lineSpacing;
        lineSpacing += *(uint16_t *)(charTable + CHARTABLE_CHARHEIGHT);

        numLines -= 1; /* dbf-style: asm does addq #1 (for div), then subq #2, so counter = numLines-1 */

        {
            int16_t i;
            for (i = numLines; i >= 0; i--) {
                textColor = flags & STR_COLOR_MASK;
                if (textColor == 0)
                    textColor = 2;
                _DrawMenuTextCentered(textX, mlY, textColor, multiData, charTable);
                mlY += (uint16_t)lineSpacing;
                /* Skip to next null-terminated string */
                while (*multiData != 0)
                    multiData++;
                multiData++;
            }
        }
        goto post_draw_callback;
    }

    if (content == ENTRY_NUMBER) {
        _Int2Ascii(entry->fgWord, numbersBuffer);
        textPtr = numbersBuffer;
        goto fg_text_common;
    }

    if (content == ENTRY_FUNC2) {
        /* Original asm passes x in d1, y in d2 as register params */
        _CallMenuFuncWithXY((void *)entry->fg, entry->x, entry->y);
        goto post_draw_callback;
    }

    if (content == ENTRY_SPRITE_COPY) {
        uint8_t *copyData = (uint8_t *)entry->fg;
        uint16_t srcIdx = *(uint16_t *)copyData;
        uint16_t dstIdx = *(uint16_t *)(copyData + 2);

        _CopySprite(srcIdx, dstIdx, 0, 0);
        _ConvertSpriteColors(dstIdx, (uint16_t *)(copyData + 4));

        sprIdx = dstIdx;
        goto fg_sprite_draw;
    }

    /* Unknown content type */
    panicReasonCode = 0x1209;
    _PanicHaltLoop();
    return;

fg_sprite_draw:
    {
        SpriteGraphics **sprGfxTable = (SpriteGraphics **)spriteGraphicsPtr;
        SpriteGraphics *sprEntry;
        uint16_t cx, cy;

#ifdef SWOS_SDL_BUILD
        /* The sprite graphics table (`spriteGraphics`) is populated
         * from menus.raw etc. during the Amiga bootstrap; we haven't
         * wired up that pipeline, so the table is a zero-filled stub.
         * Skip the sprite draw rather than dereferencing a null
         * entry pointer. */
        if (!sprGfxTable) goto post_draw_callback;
        sprEntry = sprGfxTable[sprIdx];
        if (!sprEntry) goto post_draw_callback;
#else
        sprEntry = sprGfxTable[sprIdx];
#endif
        cx = (entry->dwWidth >> 1) + entry->x - ((uint16_t)sprEntry->pixWidth >> 1);
        cy = (entry->dwHeight >> 1) + entry->y - ((uint16_t)sprEntry->nlines >> 1);

        _DrawSprite16Pixels(sprIdx, cx, cy);
        goto post_draw_callback;
    }

fg_text_common:
    /* Common text rendering path */
    flags = entry->stringFlags;
    flagsMask = (flags & STR_FLAGS_MASK) >> 2;
    charTable = (uint8_t *)charTables[flagsMask / 4];
#ifdef SWOS_SDL_BUILD
    if (!charTable) goto post_draw_callback;
#endif
    charHeight = *(uint16_t *)(charTable + CHARTABLE_CHARHEIGHT);
    entryHeight = entry->dwHeight;

    /* Vertical centering */
    {
        int16_t diff = (int16_t)entryHeight - (int16_t)charHeight;
        if (diff == 1) {
            textY = entry->y + 1;
        } else if (diff == 3) {
            textY = entry->y + 2;
        } else {
            textY = entry->y + (uint16_t)((int16_t)diff >> 1);
        }
    }

    textColor = flags & STR_COLOR_MASK;

    /* Check alignment bits — asm: btst #15 → left, btst #14 → right */
    if (flags & (1 << 15)) {
        /* Bit 15 set: left-aligned */
        textX = entry->x;
        if (entry->background != NO_BACKGROUND)
            textX += 2;
        if (textColor == 0)
            textColor = 2;
        _DrawMenuText(textX, textY, textColor, textPtr, charTable);
    } else if (flags & (1 << 14)) {
        /* Bit 14 set: right-aligned */
        textX = entry->x + entry->dwWidth - 1;
        if (entry->background != NO_BACKGROUND)
            textX -= 2;
        if (textColor == 0)
            textColor = 2;
        _DrawMenuTextRightAligned(textX, textY, textColor, textPtr, charTable);
    } else {
        /* Default: centered */
        textX = (entry->dwWidth >> 1) + entry->x;
        if (textColor == 0)
            textColor = 2;
        _DrawMenuTextCentered(textX, textY, textColor, textPtr, charTable);
    }

post_draw_callback:
    if (entry->afterDrawCb != 0) {
        _CallMenuOnSelect((void *)entry->afterDrawCb, entry);
    }
}

/* ================================================================
 * initMenuEntry — initialize template entry to defaults
 * ================================================================ */
static void initMenuEntry(MenuEntry *tmpl)
{
    tmpl->background = 0;
    tmpl->bg = 0;
    tmpl->content = 0;
    tmpl->fg = 0;
    tmpl->isInvisible = 0;
    tmpl->isDisabled = 0;
    tmpl->leftEntry = -1;
    tmpl->rightEntry = -1;
    tmpl->upEntry = -1;
    tmpl->downEntry = -1;
    tmpl->directionLeft = -1;
    tmpl->directionRight = -1;
    tmpl->directionUp = -1;
    tmpl->directionDown = -1;
    tmpl->skipLeftEntry = -1;
    tmpl->skipRightEntry = -1;
    tmpl->skipUpEntry = -1;
    tmpl->skipDownEntry = -1;
    tmpl->onSelect = 0;
    tmpl->controlMask = 0;
    tmpl->beforeDraw = 0;
    tmpl->afterDrawCb = 0;
}

/* Read a native pointer-width value from the packed stream and advance
 * the cursor. On 32-bit reads 4 bytes (matching the Amiga layout); on
 * 64-bit reads 8 (matching the C `void *` slot in MenuCmd_OnSelect,
 * MenuCmd_TextPtr, etc.). Returning uintptr_t lets the caller assign
 * directly to MenuEntry.bg/fg (uintptr_t) or cast to a function-pointer
 * field. */
static inline uintptr_t unpackReadPtr(uint8_t **pp)
{
    uintptr_t v;
    __builtin_memcpy(&v, *pp, sizeof(v));
    *pp += sizeof(v);
    return v;
}

/* ================================================================
 * UnpackMenu — unpack packed menu data into MenuEntry array
 *
 * packed: source packed menu data
 * dest: destination buffer (currentMenu)
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _UnpackMenu(uint8_t *packed, uint8_t *dest)
{
    uint8_t *a0 = packed;  /* source pointer */
    uint8_t *a1 = dest;    /* destination / current entry write pointer */
    MenuEntry *tmpl = (MenuEntry *)templateMenuEntry;
    MenuEntry *savedA1 = (MenuEntry *)0;  /* for compact-mode restore (a5) */
    uint8_t *menuBase = dest;  /* a6 = menu base */
    uint16_t compactMode = 0;  /* d2 */
    uint16_t entryCount = 0;   /* d7 */
    uint16_t opcode;
    MenuEntry *curEntry;

    menuX = 0;
    menuY = 0;
    initMenuEntry(tmpl);

    /* Copy menu header.
     *
     * Source layout (MenuHeader): 3 void* + 1 uint32_t defaultEntry.
     * Dest layout   (Menu)      : 3 fn-ptr + 1 MenuEntry* selectedEntry
     *                              + uint16_t numEntries + uint32_t endOfMenuPtr.
     *
     * On 32-bit the pointer slots are 4 bytes; on 64-bit they grow to 8.
     * The 4th source field is a uint32_t ordinal that ActivateMenu later
     * resolves to a real MenuEntry* via _CalcMenuEntryAddress, so we copy
     * just the 4 ordinal bytes into the low half of selectedEntry. Use
     * field-typed access so the pointer width round-trips for both
     * targets without a per-bitness branch. */
    {
        struct __attribute__((packed)) PackedHdr {
            void    *onInitPtr;
            void    *onReturnPtr;
            void    *onDrawPtr;
            uint32_t defaultEntry;
        } *srcHdr = (struct PackedHdr *)a0;
        Menu *dstMenu = (Menu *)a1;
        dstMenu->onInit        = (MenuVoidFn)srcHdr->onInitPtr;
        dstMenu->afterDraw     = (MenuVoidFn)srcHdr->onReturnPtr;
        dstMenu->onDraw        = (MenuVoidFn)srcHdr->onDrawPtr;
        dstMenu->selectedEntry = (MenuEntry *)(uintptr_t)srcHdr->defaultEntry;
        a0 += sizeof(*srcHdr);
    }

    a1 += MENU_HEADER_SIZE;

parse_loop:
    /* Check for end marker (64537 = 0xFC19) */
    if (*(uint16_t *)a0 == 64537)
        goto finish_menu;

    /* Check for set_xy command (64538 = 0xFC1A) */
    if (*(uint16_t *)a0 == 64538) {
        a0 += 2;
        menuX = *(uint16_t *)a0; a0 += 2;
        menuY = *(uint16_t *)a0; a0 += 2;
        goto parse_loop;
    }

    /* Check for save-to-template command (64539 = 0xFC1B) */
    if (*(uint16_t *)a0 == 64539) {
        savedA1 = (MenuEntry *)a1;
        a1 = (uint8_t *)tmpl;
        a0 += 2;
        goto apply_template_and_parse_type;
    }

    /* Check for reset-template command (64540 = 0xFC1C) */
    if (*(uint16_t *)a0 == 64540) {
        initMenuEntry(tmpl);
        a0 += 2;
        goto parse_loop;
    }

apply_template_and_parse_type:
    /* Copy template fields to current entry */
    curEntry = (MenuEntry *)a1;
    curEntry->background = tmpl->background;
    curEntry->bg = tmpl->bg;
    curEntry->content = tmpl->content;
    curEntry->stringFlags = tmpl->stringFlags;
    curEntry->fg = tmpl->fg;
    curEntry->isInvisible = tmpl->isInvisible;
    curEntry->isDisabled = tmpl->isDisabled;
    curEntry->leftEntry = tmpl->leftEntry;
    curEntry->rightEntry = tmpl->rightEntry;
    curEntry->upEntry = tmpl->upEntry;
    curEntry->downEntry = tmpl->downEntry;
    *(uint32_t *)((uint8_t *)curEntry + 12) = *(uint32_t *)((uint8_t *)tmpl + 12);
    *(uint32_t *)((uint8_t *)curEntry + 16) = *(uint32_t *)((uint8_t *)tmpl + 16);
    curEntry->onSelect = tmpl->onSelect;
    curEntry->controlMask = tmpl->controlMask;
    curEntry->beforeDraw = tmpl->beforeDraw;
    curEntry->afterDrawCb = tmpl->afterDrawCb;

    /* Clear alreadyDrawn */
    curEntry->alreadyDrawn = 0;

    /* Read position based on compact mode */
    {
        uint16_t word0 = *(uint16_t *)a0;
        if (word0 == 0xFFFF) {
            /* Switch to compact mode */
            compactMode = 1;
            a0 += 2;
        } else if (word0 == 0xFFFE) {
            /* Switch to normal mode */
            compactMode = 0;
            a0 += 2;
        }
    }

    if (compactMode) {
        /* Compact: 4 bytes (x, y, w, h as uint8) */
        uint16_t val;
        val = (uint16_t)*a0++; val += 24; val += menuX;
        curEntry->x = val;
        val = (uint16_t)*a0++; val += 36; val += menuY;
        curEntry->y = val;
        val = (uint16_t)*a0++;
        curEntry->dwWidth = val;
        val = (uint16_t)*a0++;
        curEntry->dwHeight = val;
    } else {
        /* Normal: 8 bytes (x, y, w, h as uint16) */
        curEntry->x = *(uint16_t *)a0 + 24 + menuX; a0 += 2;
        curEntry->y = *(uint16_t *)a0 + 36 + menuY; a0 += 2;
        curEntry->dwWidth = *(uint16_t *)a0; a0 += 2;
        curEntry->dwHeight = *(uint16_t *)a0; a0 += 2;
    }

    /* Parse type opcodes in a loop */
convert_menu_switch:
    opcode = *(uint16_t *)a0; a0 += 2;

    switch (opcode) {
    case 0: /* MENU_CMD_END — next entry */
        if (a1 == (uint8_t *)tmpl) {
            /* Was writing to template — restore */
            a1 = (uint8_t *)savedA1;
            goto parse_loop;
        }
        curEntry->ordinal = entryCount;
        a1 += MENUENTRY_STRIDE;
        entryCount++;
        goto parse_loop;

    case 1: /* invisible */
        curEntry->isInvisible = 1;
        goto convert_menu_switch;

    case 2: /* set_positions (nav links) */
        curEntry->leftEntry = (int8_t)*a0++;
        curEntry->rightEntry = (int8_t)*a0++;
        curEntry->upEntry = (int8_t)*a0++;
        curEntry->downEntry = (int8_t)*a0++;
        goto convert_menu_switch;

    case 3: /* func1 (background function) */
        curEntry->background = ENTRY_FUNC1;
        curEntry->bg = unpackReadPtr(&a0);
        goto convert_menu_switch;

    case 4: /* entry_frame (background color + frame) */
        curEntry->background = ENTRY_FRAME_AND_BACK_COLOR;
        /* BE-correctness: bg is a uintptr_t union'd with bgColor (uint16
         * alias of the FIRST 2 BYTES in memory order). Writing the full
         * `bg = (uintptr_t)16bit_value` zero-extends to 32 bits, which
         * on BE puts the meaningful 16 bits in the LAST 2 bytes — and
         * bgColor (first 2 bytes) then reads 0. Write to bgColor
         * directly so only the high-byte slot in memory gets the value.
         * Reader (menu_system.c:608) uses `entry->bgColor`. */
        curEntry->bgColor = *(uint16_t *)a0; a0 += 2;
        goto convert_menu_switch;

    case 5: /* sprite1 (background sprite) — reader uses entry->bgColor */
        curEntry->background = ENTRY_SPRITE1;
        curEntry->bgColor = *(uint16_t *)a0; a0 += 2;
        goto convert_menu_switch;

    case 6: /* invalid_type */
        curEntry->background = 4;
        curEntry->bgColor = *(uint16_t *)a0; a0 += 2;
        goto convert_menu_switch;

    case 7: /* func2 (foreground function) */
        curEntry->content = ENTRY_FUNC2;
        curEntry->stringFlags = 0;
        curEntry->fg = unpackReadPtr(&a0);
        goto convert_menu_switch;

    case 8: /* string_entry */
        curEntry->content = ENTRY_STRING;
        curEntry->stringFlags = *(uint16_t *)a0; a0 += 2;
        curEntry->fg = unpackReadPtr(&a0);
        /* Check if fg is a valid pointer (>= 256) or NULL */
        if (curEntry->fg != 0 && curEntry->fg < 256) {
            curEntry->fg = 0xFFFFFFFF;
        }
        goto convert_menu_switch;

    case 9: /* sprite2 (foreground sprite) — reader uses entry->fgWord */
        curEntry->content = ENTRY_SPRITE2;
        curEntry->stringFlags = 0;
        /* Same union/BE issue as case 4-6 — write to the uint16 alias. */
        curEntry->fgSpriteIdx = *(uint16_t *)a0; a0 += 2;
        goto convert_menu_switch;

    case 10: /* string_table */
        curEntry->content = ENTRY_STRING_TABLE;
        curEntry->stringFlags = *(uint16_t *)a0; a0 += 2;
        curEntry->fg = unpackReadPtr(&a0);
        goto convert_menu_switch;

    case 11: /* multiLine_text */
        curEntry->content = ENTRY_MULTI_LINE_TEXT;
        curEntry->stringFlags = *(uint16_t *)a0; a0 += 2;
        curEntry->fg = unpackReadPtr(&a0);
        goto convert_menu_switch;

    case 12: /* number — reader uses entry->fgNumber */
        curEntry->content = ENTRY_NUMBER;
        curEntry->stringFlags = *(uint16_t *)a0; a0 += 2;
        curEntry->fgNumber = *(uint16_t *)a0; a0 += 2;
        goto convert_menu_switch;

    case 13: /* on_select_fire_only */
        curEntry->controlMask = 0x20;
        curEntry->onSelect = (MenuEntryFn)unpackReadPtr(&a0);
        goto convert_menu_switch;

    case 14: /* on_select */
        curEntry->controlMask = *(uint16_t *)a0; a0 += 2;
        curEntry->onSelect = (MenuEntryFn)unpackReadPtr(&a0);
        goto convert_menu_switch;

    case 15: /* before_draw_func */
        curEntry->beforeDraw = (MenuEntryFn)unpackReadPtr(&a0);
        goto convert_menu_switch;

    case 16: /* after_draw_func */
        curEntry->afterDrawCb = (MenuEntryFn)unpackReadPtr(&a0);
        goto convert_menu_switch;

    case 17: /* left_skip */
        *((uint8_t *)curEntry + 16 + 0) = *a0++;
        *((uint8_t *)curEntry + 12 + 0) = *a0++;
        goto convert_menu_switch;

    case 18: /* right_skip */
        *((uint8_t *)curEntry + 16 + 1) = *a0++;
        *((uint8_t *)curEntry + 12 + 1) = *a0++;
        goto convert_menu_switch;

    case 19: /* up_skip */
        *((uint8_t *)curEntry + 16 + 2) = *a0++;
        *((uint8_t *)curEntry + 12 + 2) = *a0++;
        goto convert_menu_switch;

    case 20: /* down_skip */
        *((uint8_t *)curEntry + 16 + 3) = *a0++;
        *((uint8_t *)curEntry + 12 + 3) = *a0++;
        goto convert_menu_switch;

    case 21: /* sprite_copy */
        curEntry->content = ENTRY_SPRITE_COPY;
        curEntry->stringFlags = 0;
        curEntry->fg = unpackReadPtr(&a0);
        goto convert_menu_switch;

    case 22: /* string_entry2 (deferred text — marker for dynamic string) */
        curEntry->content = ENTRY_STRING;
        curEntry->stringFlags = *(uint16_t *)a0; a0 += 2;
        curEntry->fg = 0xFFFFFFFF;
        /* Same validation as case 8 */
        if (curEntry->fg != 0 && curEntry->fg < 256) {
            curEntry->fg = 0xFFFFFFFF;
        }
        goto convert_menu_switch;

    default:
        /* Should not happen — fall through */
        goto convert_menu_switch;
    }

finish_menu:
    /* Store numEntries */
    {
        Menu *menu = (Menu *)menuBase;
        MenuEntry *entryIter;
        int16_t i;

        menu->numEntries = entryCount;

        /* Post-process: resolve deferred string entries (fg == 0xFFFFFFFF) */
        entryIter = (MenuEntry *)(menuBase + MENU_HEADER_SIZE);
        for (i = (int16_t)entryCount - 1; i >= 0; i--) {
            if (entryIter->content == ENTRY_STRING && entryIter->fg == 0xFFFFFFFF) {
                /* Allocate buffer space for dynamic menu text */
                entryIter->fg = (uintptr_t)a1;
                /* Initialize buffer head to 0 (empty string). The original
                 * Amiga asm wrote a literal "STDMENUTEXT\0" debug marker
                 * here (four `move.l #'STDM',(a0)+` instructions). Faithful
                 * port reproduced that, but in scenarios where downstream
                 * init code doesn't populate the buffer (e.g. fresh career
                 * with no completed-season data in the management record
                 * menu), the placeholder text shows through to the user as
                 * "STDMENUTEXT" — not a bug-class we want surfaced as UI.
                 * Empty-init removes the placeholder while keeping the
                 * buffer reserved for downstream writers. */
                a1[0] = 0;
                a1 += 0x46;
            }
            entryIter = (MenuEntry *)((uint8_t *)entryIter + MENUENTRY_STRIDE);
        }

        menu->endOfMenuPtr = (uintptr_t)a1;
        menuFreeSpace = (uintptr_t)diyLeagueScratchBuffer - (uintptr_t)a1;
        menuDataOffset = (uintptr_t)a1 - (uintptr_t)currentMenu;

        /* Anti-tamper check */
        {
            uint32_t cookie = irqCookieObfuscated ^ 0xCFD1F8C5;
            if (cookie != 0x1722) {
                //loc_1215BC();
                return;
            }
        }
    }
}

/* ================================================================
 * DrawMenuPanelBackground
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawMenuPanelBackground(void)
{
    if (forceDrawTransparentMenuItems != 0)
        _DrawMenuItemBackground(7, 24, 36, 304, 200);
}

/* ================================================================
 * DrawSelectedMenuItemFrame
 * ================================================================ */
__attribute__((used, externally_visible, noinline))
void _DrawSelectedMenuItemFrame(void)
{
    Menu *menu = (Menu *)currentMenu;
    MenuEntry *sel = menu->selectedEntry;
    uint16_t w, h, color;

    if (sel == (MenuEntry *)0)
        return;

    w = sel->dwWidth;
    h = sel->dwHeight;
    if (h == 0 && w == 0)
        return;

    color = kColorTableShine[(menuCursorFrame & 14) >> 1];
    _DrawMenuItemInnerFrame(color, sel->x, sel->y, w, h);
}

/* ================================================================
 * DrawMenu
 * ================================================================ */
__attribute__((used, externally_visible, noinline))
void _DrawMenu(void)
{
    Menu *menu = (Menu *)currentMenu;
    MenuEntry *entry;
    int16_t i, count;
    MenuVoidFn onDrawPtr;

    menuStatus = 0;
    menuTextBlinkCounter = 0;

    _BlitterCopyPanelToScreen2();
    _DrawMenuPanelBackground();

    /* Clear all alreadyDrawn flags */
    count = (int16_t)menu->numEntries;
    entry = (MenuEntry *)menuEntries;
    for (i = 0; i < count; i++) {
        entry->alreadyDrawn = 0;
        entry = (MenuEntry *)((uint8_t *)entry + MENUENTRY_STRIDE);
    }

    /* Call onDraw callback if present */
    onDrawPtr = menu->onDraw;
    if (onDrawPtr != 0) {
        _CallMenuCallback((void *)onDrawPtr);
    }

    /* Draw all entries */
    entry = (MenuEntry *)menuEntries;
    count = (int16_t)menu->numEntries;
    for (i = 0; i < count; i++) {
        _DrawMenuItem(entry);
        entry = (MenuEntry *)((uint8_t *)entry + MENUENTRY_STRIDE);
    }
    _WaitForVSync();
    _BlitterCopyOverlayToScreen();
}

/* ================================================================
 * UpdateBlinkingMenuText
 * ================================================================ */
__attribute__((used, externally_visible))
void _UpdateBlinkingMenuText(void)
{
    Menu *menu = (Menu *)currentMenu;
    int16_t d6, d5, d7, i;
    MenuEntry *entry;
    uint16_t counter;

    menuTextBlinkCounter = (menuTextBlinkCounter + 1) & 0x1F;
    counter = menuTextBlinkCounter;

    for (d6 = 1; d6 >= 0; d6--) {
        d5 = 0;
        d7 = (int16_t)menu->numEntries;
        entry = (MenuEntry *)menuEntries;

        for (i = 0; i < d7; i++) {
            if (entry->content != NO_FOREGROUND) {
                uint16_t flags = entry->stringFlags;
                if (flags & (1 << 13)) {
                    if (counter == 0) {
                        _DrawMenuBackWithCheck(entry);
                        _DrawMenuItem(entry);
                        d5++;
                    } else if (counter == 24) {
                        uint16_t savedContent = entry->content;
                        entry->content = 0;
                        _DrawMenuBackWithCheck(entry);
                        _DrawMenuItem(entry);
                        d5++;
                        entry->content = savedContent;
                    }
                }
            }
            entry = (MenuEntry *)((uint8_t *)entry + MENUENTRY_STRIDE);
        }

        if (d5 == 0)
            return;
        _WaitForVSync();
    }
}

/* ================================================================
 * _MenuCheckControls
 * ================================================================ */
__attribute__((used, externally_visible))
void _MenuCheckControls(void)
{
    uint16_t d2;

    _GetKey();

    d2 = lastFrameTicks;

    if (fireCounter > 0)
        fireCounter = 0;

    if (player1ClearFlag) {
        pl1Fire = 0;
        shortFire = 0;
        fire = 0;
        left = 0;
        right = 0;
        up = 0;
        down = 0;
        menuControlsDirection = -1;
        menuControlsDirection2 = -1;
    } else {
        _Player1StatusProc();

        if (pl1FireCounter != 0) {
            fireCounter = pl1FireCounter;
            if (pl1FireCounter > 0)
                pl1FireCounter = 0;
        }

        if (pl1Fire == 0) {
            if ((allowShorterMenuItemsWithFrames & 0xFFFF) != 0) {
                pl1SkipFireCounter++;
                pl1Fire = ((uint8_t)pl1SkipFireCounter) & 1;
            }
            if ((allowShorterMenuItemsWithFrames >> 16) != 0) {
                pl1Fire = 1;
            }
        }

        shortFire = (uint16_t)pl1ShortFire;
        fire = (uint16_t)pl1Fire;
        left = pl1Left;
        right = pl1Right;
        up = pl1Up;
        down = pl1Down;
        menuControlsDirection = pl1Direction;
        menuControlsDirection2 = pl1Direction;
    }

    pl2Fire = 0;

    if (!player2ClearFlag && twoPlayers) {
        _Player2StatusProc();

        if (pl2FireCounter != 0) {
            fireCounter = pl2FireCounter;
            if (pl2FireCounter > 0)
                pl2FireCounter = 0;
        }

        if (pl2Fire == 0 && pl2SkipFireCounter != 0) {
            pl2Fire = 1;
        }

        shortFire |= (uint16_t)pl2ShortFire;
        fire |= (uint16_t)pl2Fire;
        left |= pl2Left;
        right |= pl2Right;
        up |= pl2Up;
        down |= pl2Down;

        if (pl1Direction < 0) {
            menuControlsDirection = pl2Direction;
            menuControlsDirection2 = pl2Direction;
        }
    }

    if (fireResetFlag) {
        if (fire == 0)
            fireResetFlag = 0;
        else {
            fire = 0;
            shortFire = 0;
        }
    }

    if (shortFire != 0) {
        longFireTime = 0;
        longFireFlag = 0;
    } else if (fire != 0) {
        longFireTime += d2;
        if (longFireTime >= 0x18) {
            longFireTime = 0x10;
            longFireFlag++;
            shortFire = 1;
        }
    }

    {
        int16_t curDir = menuControlsDirection;
        uint16_t threshold = (fire != 0) ? 0x18 : 0x0C;
        uint16_t resetVal = (fire != 0) ? 0x10 : 8;

        if (curDir == lastControls) {
            controlsHeldTimer += d2;
            if (controlsHeldTimer >= threshold) {
                controlsHeldTimer = resetVal;
                menuControlsDirection = curDir;
            } else {
                menuControlsDirection = -1;
            }
        } else {
            controlsHeldTimer = 0;
            lastControls = curDir;
        }
    }

    if (fire == 0 && menuControlsDirection < 0) {
        uint16_t key = currentKey;
        if (key == 0x10) {
            menuControlsDirection = 0;
        } else if (key == 0x12) {
            menuControlsDirection = 2;
        } else if (key == 0x11) {
            menuControlsDirection = 4;
        } else if (key == 0x13) {
            menuControlsDirection = 6;
        } else if (key == 0x0D) {
            shortFire = 1;
            fire = 1;
        }
    }
}

/* ================================================================
 * MenuProc
 * ================================================================ */
__attribute__((used, externally_visible, noinline))
void _MenuProc(void)
{
    Menu *menu;
    MenuEntry *selectedEntry;
    MenuEntryFn onSelectPtr;
    uint16_t d7;
    int16_t dir;

#ifdef SWOS_SDL_BUILD
    /* If Platform_RequestQuit flagged a quit from the SDL event pump,
     * longjmp back to _FrontEndMainLoop now — short-circuits the entire
     * nested menu state machine so we don't flicker between submenus
     * as they each return and re-activate on the way out. */
    extern void Platform_CheckQuitRequest(void);
    Platform_CheckQuitRequest();
#endif

    /* Drop the season-end skip latch the moment the chain ends, so the
     * next chain (e.g. start of the next season-end review) gets the
     * full per-screen dwell unless the user presses SPACE / button 2
     * again. */
    if (seasonUpdateInProgress == 0)
        s_seasonSkipLatched = 0;

    _ReadTimerDelta();

    if (magicValue != 0x12345678) {
        panicReasonCode = 0x9999;
        errorMessagePtr = (char *)aStackOverflow;
        _PanicHaltLoop();
        return;
    }

    if ((irqCookieObfuscated ^ 0xCFD1F8B3) != 0x1754)
        return;

    if (menuStatus == 0) {
        _DrawSelectedMenuItemFrame();
        _WaitForVSync();
    }

    menuCursorFrame++;

    if (menuStatus != 0)
        _DrawMenu();

    if (menuFade != 0) {
        _FadeIn();
        menuFade = 0;
    }

    _UpdateBlinkingMenuText();
    _MenuCheckControls();

    menu = (Menu *)currentMenu;
    selectedEntry = menu->selectedEntry;
    controlMask = 0;

    onSelectPtr = selectedEntry->onSelect;
    if (onSelectPtr == 0)
        goto no_event_or_done;

    d7 = 0;
    if (shortFire) d7 |= (1 << 5);
    if (fire)      d7 |= (1 << 0);
    if (left)      d7 |= (1 << 1);
    if (right)     d7 |= (1 << 2);
    if (up)        d7 |= (1 << 3);
    if (down)      d7 |= (1 << 4);

    dir = menuControlsDirection;
    if (dir >= 0) {
        uint16_t diagIdx = (uint16_t)dir >> 1;
        switch (diagIdx) {
        case 0: d7 |= (1 << 8); break;
        case 1: d7 |= (1 << 7); break;
        case 2: d7 |= (1 << 9); break;
        case 3: d7 |= (1 << 6); break;
        }
    }

    {
        int ord = swos_autoskip_entry_ordinal((void *)selectedEntry, menuEntries);
        SWOS_AUTOSKIP_LOG("MenuProc: dispatch entry=%d ctrlMask=0x%04x d7=0x%04x onSelect=%p "
                          "(short-circuit=%s)",
                          ord,
                          (unsigned)selectedEntry->controlMask,
                          (unsigned)d7,
                          (void *)(uintptr_t)onSelectPtr,
                          selectedEntry->controlMask == 0xFFFF ? "yes" : "no");
    }

    if (selectedEntry->controlMask != 0xFFFF) {
        d7 &= selectedEntry->controlMask;
        if (d7 == 0) {
            SWOS_AUTOSKIP_LOG("MenuProc: skipping onSelect (d7 & mask == 0)");
            goto no_event_or_done;
        }
    } else if (seasonUpdateInProgress != 0) {
        /* Hold the menu visible for ~1 s so the user can read each
         * cup/league result during a season transition. Without this the
         * controlMask=0xFFFF entry fires on iteration 1 and the chain
         * flashes by in milliseconds — see Amiga visual reference.
         *
         * SDL-only: pauseSkipRequested (SPACE key / second controller
         * button) latches s_seasonSkipLatched, draining the hold counter
         * for THIS menu AND every remaining menu in the chain. The latch
         * clears the next time seasonUpdateInProgress drops to 0 (see
         * top of MenuProc), so a single press skips the whole sequence
         * without the user having to hold the button. */
#ifdef SWOS_SDL_BUILD
        if (pauseSkipRequested)
            s_seasonSkipLatched = 1;
#else
        /* Amiga: pauseSkipRequested is SDL-only, so latch the skip from
         * the inputs the Amiga path already populates — SPACE or the
         * second fire button (pl2Fire = CIAA_PRA bit 6). NB: _GetKey
         * translates the raw SPACE scancode (0x40) through keycodeLut
         * before storing currentKey, so SPACE arrives as ASCII 0x20, not
         * the raw 0x40. Mirrors the SDL "SPACE / 2nd button" behaviour
         * without new input plumbing. */
        if (currentKey == 0x20 || pl2Fire != 0)
            s_seasonSkipLatched = 1;
#endif
        if (s_seasonSkipLatched) {
            s_seasonUpdateHoldCounter = SEASON_UPDATE_MENU_HOLD_FRAMES;
        } else if (s_seasonUpdateHoldCounter < SEASON_UPDATE_MENU_HOLD_FRAMES) {
            s_seasonUpdateHoldCounter++;
            SWOS_AUTOSKIP_LOG("MenuProc: holding (counter=%u/%u)",
                              (unsigned)s_seasonUpdateHoldCounter,
                              (unsigned)SEASON_UPDATE_MENU_HOLD_FRAMES);
            goto no_event_or_done;
        }
    }

    controlMask = d7;
    SWOS_AUTOSKIP_LOG("MenuProc: -> _CallMenuOnSelect (exitMenu before=%u)", (unsigned)exitMenu);
    /* Original asm MenuProc kept a5 = selectedEntry when calling onSelect.
       Many callbacks (SelectTeamsCommon, SelectLeaguesCommon etc.) read a5
       to get the entry's ordinal and string pointer. */
    _CallMenuOnSelect((void *)onSelectPtr, (void *)selectedEntry);
    SWOS_AUTOSKIP_LOG("MenuProc: <- _CallMenuOnSelect (exitMenu after=%u)", (unsigned)exitMenu);

no_event_or_done:
    if (fire != 0)
        return;

    dir = menuControlsDirection;
    if (dir < 0)
        return;

    {
        uint16_t dirIdx = (uint16_t)dir & 6;
        uint16_t fieldOff = nextEntryDirectionOffsets[dirIdx >> 1];
        uint8_t *navBase = (uint8_t *)selectedEntry;
        int8_t navOrd;

        navOrd = *((int8_t *)(navBase + 8 + fieldOff));
        if (navOrd == -1)
            return;

        for (;;) {
            MenuEntry *cand = _CalcMenuEntryAddress((uint16_t)(uint8_t)navOrd);

            if (cand->isDisabled == 0 && cand->isInvisible == 0) {
                menu->selectedEntry = cand;
                if (selectedEntry->background == NO_BACKGROUND)
                    _DrawMenu();
                else
                    _CallDrawMenuItem(selectedEntry);
                return;
            }

            {
                int8_t dirField = *((int8_t *)((uint8_t *)cand + 12 + fieldOff));
                if (dirField == -1) {
                    navOrd = *((int8_t *)((uint8_t *)cand + 8 + fieldOff));
                    if (navOrd == -1)
                        return;
                    continue;
                }
                navOrd = *((int8_t *)((uint8_t *)cand + 16 + fieldOff));
                fieldOff = (uint16_t)(uint8_t)dirField;
            }
        }
    }
}

/* ================================================================
 * ActivateMenu
 * ================================================================ */
__attribute__((used, externally_visible, noinline))
void _ActivateMenu(uint8_t *packedMenu)
{
    Menu *menu;
    uintptr_t selEntryVal;
    MenuVoidFn onInitPtr;

    currentMenuPtr = (uintptr_t)packedMenu;
    s_seasonUpdateHoldCounter = 0;

    _UnpackMenu(packedMenu, currentMenu);

    menu = (Menu *)currentMenu;
    /* The unpacker may write either an ordinal (0..65535) or a real
     * pointer into this slot; resolve ordinals via _CalcMenuEntryAddress
     * before any consumer reads it as a MenuEntry *. */
    selEntryVal = (uintptr_t)menu->selectedEntry;

    if (selEntryVal <= 0xFFFF) {
        menu->selectedEntry = _CalcMenuEntryAddress((uint16_t)selEntryVal);
    }

    onInitPtr = menu->onInit;
    if (onInitPtr != 0) {
        _CallMenuCallback((void *)onInitPtr);
    }
    if (exitMenu != 0)
        return;

    _DrawMenu();
}

/* ================================================================
 * ShowMenu
 * ================================================================ */
__attribute__((used, externally_visible))
void _ShowMenu(uint8_t *packedMenu)
{
    /* If a quit was already requested, skip rendering this menu entirely
     * and propagate exitMenu = 1 to any outer ShowMenu still on the
     * stack. Without this, a click on the SDL window's X (or any path
     * that sets g_quitToDos) only exits the innermost menu — the outer
     * loop then re-enters its own MenuProc and the user sees every
     * pending menu in the nested chain flash by one at a time before
     * the program actually exits. The same cascade is also visible on
     * the season-transition path where multiple cup result menus
     * stack up. */
    if (g_quitToDos) {
        SWOS_AUTOSKIP_LOG("ShowMenu: SKIPPED packedMenu=%p (%s) — g_quitToDos set",
                          packedMenu, swos_autoskip_menu_name(packedMenu));
        exitMenu = 1;
        return;
    }

    Menu *menu = (Menu *)currentMenu;
    /* volatile as safety: _CallMenuCallback saves/restores all regs around asm
       callbacks, but keep volatile in case any callback path is missed. */
    /* Save+restore as MenuEntry * (the new struct field type). */
    MenuEntry * volatile oldSelectedEntry = menu->selectedEntry;
    volatile uintptr_t oldPackedMenu = currentMenuPtr;

    menuStatus = 0;
    exitMenu = 0;

    SWOS_AUTOSKIP_LOG("ShowMenu: ENTER packedMenu=%p (%s)", packedMenu,
                      swos_autoskip_menu_name(packedMenu));
    _ActivateMenu(packedMenu);
    SWOS_AUTOSKIP_LOG("ShowMenu: post-Activate exitMenu=%u", (unsigned)exitMenu);

    if (exitMenu == 0) {
#ifdef SWOS_DEBUG_AUTOSKIP
        unsigned proc_iters = 0;
#endif
        do {
            _MenuProc();
#ifdef SWOS_DEBUG_AUTOSKIP
            proc_iters++;
            if (proc_iters <= 3 || proc_iters == 50 || proc_iters == 200 || proc_iters % 1000 == 0) {
                Menu *m = (Menu *)currentMenu;
                int ord = swos_autoskip_entry_ordinal((void *)m->selectedEntry, menuEntries);
                SWOS_AUTOSKIP_LOG("  proc iter=%u exitMenu=%u sel=%d fire=%u shortFire=%u",
                                  proc_iters, (unsigned)exitMenu, ord,
                                  (unsigned)fire, (unsigned)shortFire);
            }
#endif
        } while (exitMenu == 0);
    }
    SWOS_AUTOSKIP_LOG("ShowMenu: loop exited exitMenu=%u", (unsigned)exitMenu);

    menuStatus = 1;
    /* Don't clear exitMenu if quit propagation is in flight — leave it
     * set so the outer ShowMenu's `while (exitMenu == 0)` exits too. */
    if (!g_quitToDos)
        exitMenu = 0;

    _RestorePreviousMenu(oldSelectedEntry, (uint8_t *)oldPackedMenu);
}

/* ================================================================
 * RestorePreviousMenu
 * ================================================================ */
__attribute__((used, externally_visible))
void _RestorePreviousMenu(MenuEntry *oldSelectedEntry, uint8_t *oldPackedMenu)
{
    Menu *menu;
    MenuVoidFn afterDrawPtr;

    currentMenuPtr = (uintptr_t)oldPackedMenu;
    _UnpackMenu(oldPackedMenu, currentMenu);

    menu = (Menu *)currentMenu;
    menu->selectedEntry = oldSelectedEntry;

    afterDrawPtr = menu->afterDraw;
    if (afterDrawPtr != 0) {
        _CallMenuCallback((void *)afterDrawPtr);
    }
}

/* ================================================================
 * BlitMaskedRectAltToFront  (SWOS2.s line 40901)
 *
 * Copies a rectangle from the alt frontbuffer to the main frontbuffer
 * using masked row operations. Just shuffles registers and calls
 * RectRowMaskedApplyScreenOffset.
 *
 * Asm register convention:
 *   in:  d1=x, d2=y, d3=w, d4=h
 *   uses: frontbufferBaseAltPtr (src), frontbufferBasePtr (dst)
 * ================================================================ */
__attribute__((used, externally_visible))
void _BlitMaskedRectAltToFront(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    extern volatile uintptr_t frontbufferBaseAltPtr;
    _RectRowMaskedApplyScreenOffset(x, y, w, h, (uint8_t *)frontbufferBaseAltPtr, (uint8_t *)frontbufferBasePtr);
}

/* ================================================================
 * DrawMenuItemsFromList  (SWOS2.s line 41359)
 *
 * Two-pass draw of menu items from a list of ordinals:
 * Pass 1: draw backgrounds for visible entries
 * Pass 2: draw full items
 *
 * Asm register convention:
 *   in:  a0 = pointer to int16_t list terminated by negative value
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawMenuItemsFromList(const int16_t *list)
{
    const int16_t *iter;
    int16_t ordinal;
    MenuEntry *entry;

    /* Pass 1: draw backgrounds */
    iter = list;
    for (;;) {
        ordinal = *iter++;
        if (ordinal < 0)
            break;
        entry = _CalcMenuEntryAddress((uint16_t)ordinal);
        if (entry->isInvisible != 0) {
            _DrawMenuBackWithCheck(entry);
        }
    }

    /* Pass 2: draw items */
    iter = list;
    for (;;) {
        ordinal = *iter++;
        if (ordinal < 0)
            break;
        entry = _CalcMenuEntryAddress((uint16_t)ordinal);
        _DrawMenuItem(entry);
    }
}

/* ================================================================
 * DrawMultipleItems  (SWOS2.s line 41421)
 *
 * Draws multiple consecutive menu entries: first all backgrounds
 * via DrawBackAllItems, then each item via DrawMenuItem.
 *
 * Asm register convention:
 *   in:  d0 = number of items, a5 = first MenuEntry pointer
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawMultipleItems(uint16_t maxItems, MenuEntry *firstEntry)
{
    int16_t i;
    MenuEntry *entry;

    _DrawBackAllItems(maxItems, firstEntry);

    entry = firstEntry;
    for (i = (int16_t)maxItems - 1; i >= 0; i--) {
        _DrawMenuItem(entry);
        entry = (MenuEntry *)((uint8_t *)entry + MENUENTRY_STRIDE);
    }
}

/* ================================================================
 * DrawMenuItemsWaitForVSync  (SWOS2.s line 41347)
 *
 * Double-buffered list draw: draw items, vsync, draw again, vsync.
 *
 * Asm register convention:
 *   in:  a0 = pointer to int16_t list terminated by negative value
 * ================================================================ */
__attribute__((used, externally_visible))
void _DrawMenuItemsWaitForVSync(const int16_t *list)
{
#ifdef SWOS_SDL_BUILD
    /* See _CallDrawMenuItem comment — collapse the Amiga double-flush
     * to single vsync + memcpy sync on SDL so we present once instead
     * of twice per partial redraw. */
    _DrawMenuItemsFromList(list);
    _WaitForVSync();
    _BlitterCopyOverlayToScreen();
#else
    _DrawMenuItemsFromList(list);
    _WaitForVSync();
    _DrawMenuItemsFromList(list);
    _WaitForVSync();
#endif
}

/* ================================================================
 * CallDrawMultipleItems  (SWOS2.s line 41408)
 *
 * Double-buffered multi-item draw: draw, vsync, draw, vsync.
 *
 * Asm register convention:
 *   in:  d0 = number of items, a5 = first MenuEntry pointer
 * ================================================================ */
__attribute__((used, externally_visible))
void _CallDrawMultipleItems(uint16_t maxItems, MenuEntry *firstEntry)
{
#ifdef SWOS_SDL_BUILD
    /* See _CallDrawMenuItem comment — collapse double-flush. */
    _DrawMultipleItems(maxItems, firstEntry);
    _WaitForVSync();
    _BlitterCopyOverlayToScreen();
#else
    _DrawMultipleItems(maxItems, firstEntry);
    _WaitForVSync();
    _DrawMultipleItems(maxItems, firstEntry);
    _WaitForVSync();
#endif
}

/* ================================================================
 * CallDrawMultipleItemsRect  (SWOS2.s line 41389)
 *
 * Draws multiple items, waits for vsync, then blits a masked
 * rectangle from alt buffer to front buffer using the entry's
 * bounding box, then waits for vsync again.
 *
 * Note: d0 (maxItems) is implicitly passed from the caller.
 *
 * Asm register convention:
 *   in:  d0 = number of items (implicit), a5 = first MenuEntry ptr
 * ================================================================ */
__attribute__((used, externally_visible))
void _CallDrawMultipleItemsRect(uint16_t maxItems, MenuEntry *firstEntry)
{
    _DrawMultipleItems(maxItems, firstEntry);
    _WaitForVSync();
    _BlitMaskedRectAltToFront(firstEntry->x, firstEntry->y, firstEntry->dwWidth, firstEntry->dwHeight);
    _WaitForVSync();
}
