/*
 * arcade.c — SWOS arcade / coin-op mode (SDL-only). See docs/ARCADE.md.
 *
 * Phase 1: boot gate + INSERT COIN skeleton (committed).
 * Phase 2 (this file): attract mode plays the HIL/RPL reels from
 *   bin/grafs_arcade/attract_mode/ (random, free-running, camera-follow as the
 *   replay viewer does), looping. Coin keys 5/6 still register. The logo +
 *   INSERT-COIN overlay drawn *over* the reel, and coin->match, come next.
 */
#ifdef SWOS_SDL_BUILD

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "arcade.h"
#include "text_render.h"   /* DrawMenuText, DrawMenuTextRightAligned */
#include "display.h"       /* _screen_width, SCREEN_HEIGHT */

int _gArcadeMode = 0;

extern void WaitForVSync(void);
extern void ReadTimerDelta(void);
extern volatile uint16_t _quitToDos;
extern volatile uintptr_t _frontbufferBasePtr;
extern uint8_t _bigCharsTable[];

/* replay engine + reel loaders (highlight.c / dos_replay_load.c / game_loop.c) */
extern uint8_t  _hilFileBuffer[];
extern uint16_t _hilNumGoals;
extern uint16_t _hilStarted;
extern uint16_t _hilMaxSubstitutes;
extern uint16_t _gameMaxSubstitutes;
extern uint16_t _teamsLoaded;
extern uint16_t _poolplyrLoaded;
extern void ViewHighlights(void);
extern int16_t CAssetSys_DispatchGetSize(uint16_t opcode, uint16_t param,
                                          void *name, void *dest, uint32_t *outFileSize);
extern int  DosReplay_IsDosHil(const uint8_t *buf, uint32_t size);
extern int  DosReplay_ConvertHilInPlace(uint32_t fileSize);
extern int  DosReplay_LoadRpl(uint8_t *filename);
extern void DosReplay_ClearRpl(void);
extern uint32_t _kHilFileSig;

#define ARC_WHITE         2          /* menu-palette white (0x0FFF) for the UI text */
#define ARC_CHARS         ((uint8_t *)_bigCharsTable)
#define ARC_ASSET_INIT    2          /* CAssetSys ASSET_OP_INIT_SOURCE0 */
#define ARC_REEL_DIR      "grafs_arcade/attract_mode"
#define ARC_MAX_REELS     64

static char     s_reels[ARC_MAX_REELS][64];
static int      s_reelCount;
static uint32_t s_rngState = 0x1234abcdu;
static int      s_credits1, s_credits2;   /* inserted credits (shared attract/match) */
static uint32_t s_overlayFrame;           /* blink phase for the attract overlay */

#define ARC_LOGO_MAXW  1024
static uint8_t  s_logoBuf[ARC_LOGO_MAXW * SCREEN_HEIGHT];    /* LOADER1 logo, 16-col chunky */
static int      s_logoLoaded;
static uint8_t  s_overlayBuf[ARC_LOGO_MAXW * SCREEN_HEIGHT]; /* composited UI, menu-palette idx */
static int      s_overlayActive;

/* Tiny LCG — Date/rand are off-limits in workflows but this is the game; still
 * keep it self-contained + seed-stirred so the rotation isn't fixed. */
static uint32_t arc_rand(void)
{
    s_rngState = s_rngState * 1103515245u + 12345u;
    return (s_rngState >> 16) & 0x7fff;
}

static int arc_ends_with(const char *s, const char *suf)
{
    size_t ls = strlen(s), lf = strlen(suf);
    return ls >= lf && strcasecmp(s + ls - lf, suf) == 0;
}

/* Scan attract_mode/ for .HIL/.RPL reels. Returns the count. */
static int arc_scan_reels(void)
{
    s_reelCount = 0;
    DIR *d = opendir(ARC_REEL_DIR);
    if (!d) {
        fprintf(stderr, "[arcade] no attract reels: cannot open %s\n", ARC_REEL_DIR);
        return 0;
    }
    struct dirent *e;
    while ((e = readdir(d)) != NULL && s_reelCount < ARC_MAX_REELS) {
        if (e->d_name[0] == '.') continue;
        if (arc_ends_with(e->d_name, ".HIL") || arc_ends_with(e->d_name, ".RPL")) {
            /* The .HIL converter reads the (larger) DOS source into _hilFileBuffer
             * (192254 B), so skip reels too big for it; RPLs use the 2.76 MB buffer. */
            char p[160];
            snprintf(p, sizeof p, "%s/%s", ARC_REEL_DIR, e->d_name);
            struct stat st;
            uint32_t limit = arc_ends_with(e->d_name, ".RPL") ? (2700u * 1024u) : 192254u;
            if (stat(p, &st) == 0 && (uint32_t)st.st_size > limit) {
                fprintf(stderr, "[arcade] skip oversized reel %s (%ld B)\n",
                        e->d_name, (long)st.st_size);
                continue;
            }
            snprintf(s_reels[s_reelCount], sizeof s_reels[0], "%s", e->d_name);
            s_reelCount++;
        }
    }
    closedir(d);
    fprintf(stderr, "[arcade] %d attract reel(s) in %s\n", s_reelCount, ARC_REEL_DIR);
    return s_reelCount;
}

/* Load reel `name` (relative to attract_mode/) into the replay engine. The
 * direct asset dispatch resolves paths relative to bin/ and has no ~91 KB
 * frontbuffer-stage cap, so it handles the multi-MB RPLs. Returns 0 on success. */
static int arc_load_reel(const char *name)
{
    char path[128];
    snprintf(path, sizeof path, "%s/%s", ARC_REEL_DIR, name);
    DosReplay_ClearRpl();

    if (arc_ends_with(name, ".RPL"))
        return DosReplay_LoadRpl((uint8_t *)path);

    /* .HIL: read into the HIL buffer, then convert if it's the DOS/2020 layout */
    uint32_t size = 0;
    CAssetSys_DispatchGetSize(ARC_ASSET_INIT, 0, path, _hilFileBuffer, &size);
    if (size == 0)
        return 1;
    if (DosReplay_IsDosHil(_hilFileBuffer, size))
        return DosReplay_ConvertHilInPlace(size);
    /* native Amiga HIL: already in our layout — accept if the signature matches */
    return 0;
}

static void arc_clear(void)
{
    memset((void *)_frontbufferBasePtr, 0,
           (size_t)_screen_width * (size_t)SCREEN_HEIGHT);
}

/* Coin/service input — accumulate credits (shared by attract + idle). */
static void arc_drain_coins(void)
{
    int c1 = 0, c2 = 0, sv = 0;
    Platform_ArcadeInput(&c1, &c2, &sv);
    if (c1) { s_credits1 += c1; fprintf(stderr, "[arcade] coin P1 (credits=%d)\n", s_credits1); }
    if (c2) { s_credits2 += c2; fprintf(stderr, "[arcade] coin P2 (credits=%d)\n", s_credits2); }
    if (sv)  fprintf(stderr, "[arcade] F2 service button — service menu TODO\n");
}

/* Decode the LOADER1 intro logo once into s_logoBuf (8bpp chunky, frontbuffer
 * layout). LoadOverlayToFrontbuffer writes the decode into _frontbufferMaskBuffer
 * (a staging buffer, not the visible one), so this doesn't disturb the screen. */
static void arc_load_logo(void)
{
    extern void    LoadOverlayToFrontbuffer(const uint8_t *path);
    extern uint8_t _frontbufferMaskBuffer[];
    size_t n = (size_t)_screen_width * (size_t)SCREEN_HEIGHT;
    if (n > sizeof s_logoBuf) n = sizeof s_logoBuf;
    LoadOverlayToFrontbuffer((const uint8_t *)"grafs/loader1.raw");
    memcpy(s_logoBuf, _frontbufferMaskBuffer, n);
    s_logoLoaded = 1;
}

/* The attract overlay: full-screen SWOS logo (index-0 transparent) + flashing
 * INSERT COIN / credit state, rendered into a private 16-colour buffer using
 * MENU-palette indices. The reel underneath uses its own (DOS) palette, so the
 * platform layer composites this buffer with the menu palette — see
 * Arcade_CompositeOverlay. Drawn over the playing reel (hook in PlayHighlightsLoop)
 * and on the idle screen. */
void Arcade_DrawAttractOverlay(void)
{
    arc_drain_coins();
    s_overlayFrame++;
    int flash = (int)((s_overlayFrame >> 4) & 1);

    size_t n = (size_t)_screen_width * (size_t)SCREEN_HEIGHT;
    if (n > sizeof s_overlayBuf) n = sizeof s_overlayBuf;
    memset(s_overlayBuf, 0, n);

    /* Briefly retarget the menu-draw routines at the overlay buffer so the
     * INSERT-COIN text lands there (index 0 transparent) instead of the reel
     * frontbuffer; the logo is composited separately with its own palette. */
    uintptr_t saved = _frontbufferBasePtr;
    _frontbufferBasePtr = (uintptr_t)s_overlayBuf;

    if (s_credits1 == 0) { if (flash) DrawMenuText(8, 248, ARC_WHITE, (uint8_t *)"INSERT COIN", ARC_CHARS); }
    else                            DrawMenuText(8, 248, ARC_WHITE, (uint8_t *)"PLAYER 1 READY", ARC_CHARS);
    if (s_credits2 == 0) { if (flash) DrawMenuTextRightAligned(328, 248, ARC_WHITE, (uint8_t *)"INSERT COIN", ARC_CHARS); }
    else                            DrawMenuTextRightAligned(328, 248, ARC_WHITE, (uint8_t *)"PLAYER 2 READY", ARC_CHARS);

    _frontbufferBasePtr = saved;
    s_overlayActive = 1;
}

/* Composite the attract overlay onto the final ARGB8888 frame using the MENU
 * palette (16-col), index 0 transparent. Called by the platform present path
 * after chunky_to_rgba, so the logo + text show their menu colours over a reel
 * that is using a different (DOS) palette. */
void Arcade_CompositeOverlay(uint32_t *dst, int dst_pitch_pixels, int w, int h)
{
    if (!s_overlayActive || !dst)
        return;

    /* LOADER1 is authored for the title-screen palette (the intro shows it with
     * _paletteTable[3]); the reel underneath uses its own palette, so map the
     * logo's indices through _titleScreenPalette here. The INSERT-COIN text is a
     * flat white drawn separately into s_overlayBuf. */
    extern uint16_t _titleScreenPalette[];
    uint32_t pal[16];
    for (int i = 0; i < 16; i++) {
        uint16_t v = _titleScreenPalette[i];          /* 0x0RGB Amiga 12-bit */
        uint32_t r = (v >> 8) & 0xF, g = (v >> 4) & 0xF, b = v & 0xF;
        r = (r << 4) | r; g = (g << 4) | g; b = (b << 4) | b;
        pal[i] = ((uint32_t)0xFFu << 24) | (r << 16) | (g << 8) | b;
    }
    const uint32_t white = 0xFFFFFFFFu;
    const uint32_t black = 0xFF000000u;
    const uint8_t  shadowBit = (uint8_t)(1u << (NUM_PLANES - 1)); /* glyph shadow plane */

    if (h > SCREEN_HEIGHT)  h = SCREEN_HEIGHT;
    if (w > _screen_width)  w = _screen_width;
    for (int y = 0; y < h; y++) {
        const uint8_t *lg  = s_logoBuf    + (size_t)y * (size_t)_screen_width;
        const uint8_t *tx  = s_overlayBuf + (size_t)y * (size_t)_screen_width;
        uint32_t      *out = dst + (size_t)y * (size_t)dst_pitch_pixels;
        for (int x = 0; x < w; x++) {
            if (lg[x]) out[x] = pal[lg[x] & 15];          /* logo (title-screen palette) */
            uint8_t t = tx[x];
            if (t)                                        /* text: body->white ink, */
                out[x] = (t & (uint8_t)~shadowBit) ? white : black; /* shadow plane->black */
        }
    }
}

/* No reels available: hold the attract overlay on a black screen for a bit, then
 * the caller re-scans (so files dropped into attract_mode/ at runtime appear). */
static void arc_idle_insert_coin(int frames)
{
    for (int f = 0; f < frames && _quitToDos == 0; f++) {
        arc_clear();
        Arcade_DrawAttractOverlay();
        WaitForVSync();
        ReadTimerDelta();
    }
}

void ArcadeMainLoop(void)
{
    fprintf(stderr, "[arcade] boot-to-arcade: entering attract loop\n");

    /* The boot loader leaves the copper palette faded to black; restore the menu
     * palette so anything we draw is visible. */
    { extern uint16_t _menuPalette[]; WriteCopperPalette(_menuPalette); }

    arc_load_logo();
    arc_scan_reels();
    ReadTimerDelta();

    while (_quitToDos == 0) {
        if (s_reelCount == 0) {
            arc_idle_insert_coin(250);   /* ~5 s, then re-scan */
            arc_scan_reels();
            continue;
        }

        /* play a random reel through the normal replay path (camera follows as
         * usual). ViewHighlights does CopyTeamsAndHeader + InitGameSaveTeams +
         * PlayHighlightsLoop and returns when the reel finishes. */
        int idx = (int)(arc_rand() % (uint32_t)s_reelCount);
        s_rngState ^= (uint32_t)_quitToDos << 13;   /* stir */
        fprintf(stderr, "[arcade] attract reel: %s\n", s_reels[idx]);

        if (arc_load_reel(s_reels[idx]) != 0 || _hilNumGoals == 0) {
            fprintf(stderr, "[arcade] reel load failed/empty: %s\n", s_reels[idx]);
            arc_idle_insert_coin(100);
            continue;
        }

        _hilStarted        = 1;
        _gameMaxSubstitutes = _hilMaxSubstitutes;
        _teamsLoaded       = 0;
        _poolplyrLoaded    = 0;
        ViewHighlights();

        /* between reels: restore the menu palette (the reel set its own) and a
         * brief INSERT COIN beat. */
        { extern uint16_t _menuPalette[]; WriteCopperPalette(_menuPalette); }
        arc_idle_insert_coin(60);
    }
}

#endif /* SWOS_SDL_BUILD */
