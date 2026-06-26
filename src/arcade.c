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
#include "text_render.h"    /* DrawMenuText, DrawMenuTextRightAligned */
#include "display.h"        /* _screen_width, SCREEN_HEIGHT */
#include "platform_sdl.h"   /* PlatformInitOptions — service-menu settings */
#include "swos_settings.h"  /* Settings_Save — service menu writes SWOS.ini */
#include "arcade_svcfont.h" /* SVCFONT glyph LUT (TROGF15 large / TROGF7 small) */

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
#define ARC_Y_BOTTOM      256        /* INSERT COIN / CREDIT NN / player-count row */
#define ARC_Y_PROMPT      240        /* PUSH START BUTTON — just above the bottom row */
#define ARC_ASSET_INIT    2          /* CAssetSys ASSET_OP_INIT_SOURCE0 */
#define ARC_REEL_DIR      "grafs_arcade/attract_mode"
#define ARC_MAX_REELS     64

static char     s_reels[ARC_MAX_REELS][64];
static int      s_reelCount;
static uint32_t s_rngState = 0x1234abcdu;
static int      s_credits;     /* total credits in the cabinet */
static int      s_startGame;   /* pending game: 0 none, 1 one-player, 2 two-player */
static int      s_serviceReq;  /* service key pressed -> open the F2 service menu */
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

/* Cabinet input: coins add credits; start buttons request a game (1P needs 1
 * credit, 2P needs 2). Shared by the attract reels and the idle screen. */
static void arc_drain_input(void)
{
    int c1 = 0, c2 = 0, sv = 0, st1 = 0, st2 = 0;
    Platform_ArcadeInput(&c1, &c2, &sv, &st1, &st2);
    if (c1 + c2 > 0) {
        s_credits += (c1 + c2);
        fprintf(stderr, "[arcade] credit -> %d\n", s_credits);
    }
    if (sv) s_serviceReq = 1;
    if (st1 && s_credits >= 1)      { s_credits -= 1; s_startGame = 1; }
    else if (st2 && s_credits >= 2) { s_credits -= 2; s_startGame = 2; }
    if (s_startGame)
        fprintf(stderr, "[arcade] START %d-player game (credits left %d)\n",
                s_startGame, s_credits);
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
int Arcade_DrawAttractOverlay(void)
{
    arc_drain_input();
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

    if (s_credits <= 0) {
        /* no credits — flashing INSERT COIN in both bottom corners */
        if (flash) {
            DrawMenuText(8, ARC_Y_BOTTOM, ARC_WHITE, (uint8_t *)"INSERT COIN", ARC_CHARS);
            DrawMenuTextRightAligned(328, ARC_Y_BOTTOM, ARC_WHITE, (uint8_t *)"INSERT COIN", ARC_CHARS);
        }
    } else {
        /* credits available — CREDIT count (bottom-right) + flashing PUSH START
         * prompt with the player-count line aligned to the CREDIT row */
        char buf[20];
        snprintf(buf, sizeof buf, "CREDIT %02d", s_credits);
        DrawMenuTextRightAligned(328, ARC_Y_BOTTOM, ARC_WHITE, (uint8_t *)buf, ARC_CHARS);
        if (flash)
            DrawMenuTextCentered(168, ARC_Y_PROMPT, ARC_WHITE, (uint8_t *)"PUSH START BUTTON", ARC_CHARS);
        DrawMenuTextCentered(168, ARC_Y_BOTTOM, ARC_WHITE, (uint8_t *)(
            (s_credits >= 2) ? "ONE OR TWO PLAYERS" : "ONE PLAYER ONLY"), ARC_CHARS);
    }

    _frontbufferBasePtr = saved;
    s_overlayActive = 1;
    return (s_startGame != 0) || s_serviceReq;   /* non-zero -> leave the attract reel */
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
        if (Arcade_DrawAttractOverlay()) break;   /* a game was started */
        WaitForVSync();
        ReadTimerDelta();
    }
}

/* ====================================================================
 * Phase 3b — international flag selection
 * ==================================================================== */
#define FLAG_W         24
#define FLAG_H         16
#define FLAG_COLS      6
#define FLAG_ROWS      8
#define FLAG_COUNT     (FLAG_COLS * FLAG_ROWS)
#define FLAG_CELL_W    28          /* flag 24 + frame room for the selector */
#define FLAG_CELL_H    22          /* flag 16 + frame room */
#define FLAG_SHEET_CAP (352 * 272)

static uint8_t  s_flagSheet[FLAG_SHEET_CAP];
static uint32_t s_flagPal[256];
static int      s_flagSheetW, s_flagSheetH, s_flagsLoaded;
static uint32_t s_flagScreenRGBA[ARC_LOGO_MAXW * SCREEN_HEIGHT];
static int      s_flagScreenActive;

extern int Platform_LoadIndexedBMP(const char *path, uint8_t *out8, int out_cap,
                                   uint32_t *outPal, int *outW, int *outH);

static void arc_load_flags(void)
{
    if (Platform_LoadIndexedBMP("grafs_arcade/FLAGS.bmp", s_flagSheet, FLAG_SHEET_CAP,
                                s_flagPal, &s_flagSheetW, &s_flagSheetH) == 0) {
        s_flagsLoaded = 1;
        fprintf(stderr, "[arcade] FLAGS.bmp %dx%d loaded\n", s_flagSheetW, s_flagSheetH);
    }
}

/* Platform present hook: when a full-screen arcade RGBA frame is active (flag
 * select) copy it over the texture instead of the 8bpp framebuffer path. */
int Arcade_RenderFullScreen(uint32_t *dst, int dst_pitch_pixels, int w, int h)
{
    if (!s_flagScreenActive || !dst) return 0;
    if (h > SCREEN_HEIGHT)  h = SCREEN_HEIGHT;
    if (w > _screen_width)  w = _screen_width;
    for (int y = 0; y < h; y++)
        memcpy(dst + (size_t)y * (size_t)dst_pitch_pixels,
               s_flagScreenRGBA + (size_t)y * (size_t)_screen_width,
               (size_t)w * sizeof(uint32_t));
    return 1;
}

static void arc_flag_blit(int f, int px, int py)
{
    if (!s_flagsLoaded) return;
    int sx = (f % FLAG_COLS) * FLAG_W, sy = (f / FLAG_COLS) * FLAG_H;
    for (int y = 0; y < FLAG_H; y++) {
        int dy = py + y;
        if (dy < 0 || dy >= SCREEN_HEIGHT) continue;
        const uint8_t *src = s_flagSheet + (size_t)(sy + y) * s_flagSheetW + sx;
        uint32_t      *out = s_flagScreenRGBA + (size_t)dy * _screen_width;
        for (int x = 0; x < FLAG_W; x++) {
            int dx = px + x;
            if (dx >= 0 && dx < _screen_width) out[dx] = s_flagPal[src[x]];
        }
    }
}

static void arc_flag_rect(int x0, int y0, int rw, int rh, uint32_t c)
{
    for (int x = x0; x < x0 + rw; x++) {
        if (x < 0 || x >= _screen_width) continue;
        if (y0 >= 0 && y0 < SCREEN_HEIGHT)              s_flagScreenRGBA[(size_t)y0 * _screen_width + x] = c;
        if (y0+rh-1 >= 0 && y0+rh-1 < SCREEN_HEIGHT)    s_flagScreenRGBA[(size_t)(y0+rh-1) * _screen_width + x] = c;
    }
    for (int y = y0; y < y0 + rh; y++) {
        if (y < 0 || y >= SCREEN_HEIGHT) continue;
        if (x0 >= 0 && x0 < _screen_width)              s_flagScreenRGBA[(size_t)y * _screen_width + x0] = c;
        if (x0+rw-1 >= 0 && x0+rw-1 < _screen_width)    s_flagScreenRGBA[(size_t)y * _screen_width + x0+rw-1] = c;
    }
}

/* Render a centred header (8bpp via the menu-draw routines) and overlay it white
 * with a black shadow onto the flag-screen RGBA. */
static void arc_flag_header(const char *header)
{
    size_t n = (size_t)_screen_width * SCREEN_HEIGHT;
    if (n > sizeof s_overlayBuf) n = sizeof s_overlayBuf;
    memset(s_overlayBuf, 0, n);
    uintptr_t saved = _frontbufferBasePtr;
    _frontbufferBasePtr = (uintptr_t)s_overlayBuf;
    DrawMenuTextCentered(168, 16, ARC_WHITE, (uint8_t *)header, ARC_CHARS);
    _frontbufferBasePtr = saved;

    uint8_t shadowBit = (uint8_t)(1u << (NUM_PLANES - 1));
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        const uint8_t *tx  = s_overlayBuf + (size_t)y * _screen_width;
        uint32_t      *out = s_flagScreenRGBA + (size_t)y * _screen_width;
        for (int x = 0; x < _screen_width; x++) {
            uint8_t t = tx[x];
            if (t) out[x] = (t & (uint8_t)~shadowBit) ? 0xFFFFFFFFu : 0xFF000000u;
        }
    }
}

/* The flag-select screen — one shared centred 6x8 grid, one selector for 1P and
 * two colour-coded selectors for 2P. Each player's joystick navigates their own
 * selector; fire confirms. Returns the chosen flag indices (0..47). */
static void arc_flag_select(int numPlayers, int *outP1, int *outP2)
{
    extern uint8_t _pl1Up, _pl1Down, _pl1Left, _pl1Right, _pl1ShortFire;
    extern uint8_t _pl2Up, _pl2Down, _pl2Left, _pl2Right, _pl2ShortFire;
    extern void Player1StatusProc(void);
    extern void Player2StatusProc(void);

    *outP1 = 0; if (outP2) *outP2 = 0;
    if (!s_flagsLoaded) return;

    int sel1 = 0, sel2 = FLAG_COUNT - 1;
    int conf1 = 0, conf2 = (numPlayers < 2);
    int u1=0,d1=0,l1=0,r1=0, u2=0,d2=0,l2=0,r2=0;
    uint32_t frame = 0;
    int gx0 = (_screen_width - FLAG_COLS * FLAG_CELL_W) / 2;
    int gy0 = 44;

    s_flagScreenActive = 1;
    while (_quitToDos == 0 && (!conf1 || !conf2)) {
        frame++;
        Player1StatusProc();
        if (numPlayers >= 2) Player2StatusProc();

        if (!conf1) {
            if (_pl1Up && !u1)    sel1 = (sel1 - FLAG_COLS + FLAG_COUNT) % FLAG_COUNT;
            if (_pl1Down && !d1)  sel1 = (sel1 + FLAG_COLS) % FLAG_COUNT;
            if (_pl1Left && !l1)  sel1 = (sel1 - 1 + FLAG_COUNT) % FLAG_COUNT;
            if (_pl1Right && !r1) sel1 = (sel1 + 1) % FLAG_COUNT;
            if (_pl1ShortFire)    conf1 = 1;
        }
        u1=_pl1Up; d1=_pl1Down; l1=_pl1Left; r1=_pl1Right;
        if (numPlayers >= 2 && !conf2) {
            if (_pl2Up && !u2)    sel2 = (sel2 - FLAG_COLS + FLAG_COUNT) % FLAG_COUNT;
            if (_pl2Down && !d2)  sel2 = (sel2 + FLAG_COLS) % FLAG_COUNT;
            if (_pl2Left && !l2)  sel2 = (sel2 - 1 + FLAG_COUNT) % FLAG_COUNT;
            if (_pl2Right && !r2) sel2 = (sel2 + 1) % FLAG_COUNT;
            if (_pl2ShortFire)    conf2 = 1;
        }
        u2=_pl2Up; d2=_pl2Down; l2=_pl2Left; r2=_pl2Right;

        /* background */
        size_t total = (size_t)_screen_width * SCREEN_HEIGHT;
        for (size_t i = 0; i < total; i++) s_flagScreenRGBA[i] = 0xFF0A280Au;
        /* flags */
        for (int f = 0; f < FLAG_COUNT; f++)
            arc_flag_blit(f, gx0 + (f % FLAG_COLS) * FLAG_CELL_W + 2,
                             gy0 + (f / FLAG_COLS) * FLAG_CELL_H + 3);
        /* selectors (flashing; green once confirmed) */
        int fl = (frame >> 3) & 1;
        { int cx = gx0 + (sel1 % FLAG_COLS) * FLAG_CELL_W, cy = gy0 + (sel1 / FLAG_COLS) * FLAG_CELL_H;
          uint32_t c = conf1 ? 0xFF00FF00u : (fl ? 0xFFFFFFFFu : 0xFF707070u);
          arc_flag_rect(cx, cy, FLAG_CELL_W, FLAG_CELL_H, c); }
        if (numPlayers >= 2) {
          int cx = gx0 + (sel2 % FLAG_COLS) * FLAG_CELL_W, cy = gy0 + (sel2 / FLAG_COLS) * FLAG_CELL_H;
          uint32_t c = conf2 ? 0xFF00FF00u : (fl ? 0xFF40A0FFu : 0xFF204060u);
          arc_flag_rect(cx - 1, cy - 1, FLAG_CELL_W + 2, FLAG_CELL_H + 2, c); }
        /* header */
        arc_flag_header(numPlayers >= 2 ? "SELECT YOUR TEAMS" : "SELECT YOUR TEAM");

        WaitForVSync();
        ReadTimerDelta();
    }
    s_flagScreenActive = 0;
    *outP1 = sel1; if (outP2) *outP2 = sel2;
}

/* Begin the requested arcade game: flag select, then (3c) the arcade-rules match.
 * The match is still a stub — for now it logs the chosen flags and returns. */
static void arc_start_game(void)
{
    int numPlayers = (s_startGame >= 2) ? 2 : 1;
    int p1 = 0, p2 = 0;
    arc_flag_select(numPlayers, &p1, &p2);
    if (numPlayers >= 2)
        fprintf(stderr, "[arcade] flags chosen: P1=%d P2=%d -> START MATCH (3c TODO)\n", p1, p2);
    else
        fprintf(stderr, "[arcade] flag chosen: P1=%d -> START MATCH (3c TODO)\n", p1);
    s_startGame = 0;
}

/* ====================================================================
 * Phase 4 — F2 service menu (SVCFONT, writes game-rule settings to SWOS.ini)
 * ==================================================================== */
PlatformInitOptions *g_arcadeOpts    = NULL;   /* live options, set by main.c */
const char          *g_arcadeIniPath = NULL;   /* SWOS.ini path, set by main.c */

#define SVC_SHEET_CAP (320 * 256)
static uint8_t  s_svcSheet[SVC_SHEET_CAP];
static uint32_t s_svcPal[256];
static int      s_svcW, s_svcH, s_svcLoaded;

static void arc_load_svcfont(void)
{
    if (Platform_LoadIndexedBMP("grafs_arcade/SVCFONT.bmp", s_svcSheet, SVC_SHEET_CAP,
                                s_svcPal, &s_svcW, &s_svcH) == 0)
        s_svcLoaded = 1;
}

static const FontGlyph *arc_svc_glyph(const FontGlyph *glyphs, int count, char c)
{
    for (int i = 0; i < count; i++)
        if (glyphs[i].ch == (unsigned char)c) return &glyphs[i];
    return NULL;
}

/* Draw `str` into the full-screen RGBA at (px,py) with the given font; sheet ink
 * (non-zero index) is painted in `color`. Returns the end x. */
static int arc_svc_text(int px, int py, uint32_t color, const char *str,
                        const FontGlyph *glyphs, int count)
{
    if (!s_svcLoaded) return px;
    for (const char *p = str; *p; p++) {
        const FontGlyph *g = arc_svc_glyph(glyphs, count, *p);
        if (!g) { px += 6; continue; }
        for (int gy = 0; gy < g->h; gy++) {
            int dy = py + g->yoff + gy;
            if (dy < 0 || dy >= SCREEN_HEIGHT) continue;
            const uint8_t *src = s_svcSheet + (size_t)(g->y + gy) * s_svcW + g->x;
            uint32_t      *out = s_flagScreenRGBA + (size_t)dy * _screen_width;
            for (int gx = 0; gx < g->w; gx++) {
                int dx = px + gx;
                if (dx >= 0 && dx < _screen_width && src[gx]) out[dx] = color;
            }
        }
        px += g->w + 1;
    }
    return px;
}

static int arc_svc_textw(const char *str, const FontGlyph *glyphs, int count)
{
    int w = 0;
    for (const char *p = str; *p; p++) {
        const FontGlyph *g = arc_svc_glyph(glyphs, count, *p);
        w += g ? g->w + 1 : 6;
    }
    return w;
}

static void arc_svc_centered(int cx, int py, uint32_t color, const char *str,
                             const FontGlyph *glyphs, int count)
{
    arc_svc_text(cx - arc_svc_textw(str, glyphs, count) / 2, py, color, str, glyphs, count);
}

/* Fill a solid rectangle in the full-screen RGBA. */
static void arc_fill_rect(int x0, int y0, int rw, int rh, uint32_t c)
{
    for (int y = y0; y < y0 + rh; y++) {
        if (y < 0 || y >= SCREEN_HEIGHT) continue;
        uint32_t *out = s_flagScreenRGBA + (size_t)y * _screen_width;
        for (int x = x0; x < x0 + rw; x++)
            if (x >= 0 && x < _screen_width) out[x] = c;
    }
}

/* Smoothly cycle an ARGB palette, blending between consecutive entries; `period`
 * frames per segment, looping back to the start. */
static uint32_t arc_cycle_color(const uint32_t *cols, int n, uint32_t frame, uint32_t period)
{
    uint32_t t   = frame % (period * (uint32_t)n);
    int      seg = (int)(t / period);
    int      f   = (int)(t % period);
    uint32_t a = cols[seg], b = cols[(seg + 1) % n];
    int ar=(a>>16)&0xFF, ag=(a>>8)&0xFF, ab=a&0xFF;
    int br=(b>>16)&0xFF, bg=(b>>8)&0xFF, bb=b&0xFF;
    int r  = ar + (br - ar) * f / (int)period;
    int g  = ag + (bg - ag) * f / (int)period;
    int bl = ab + (bb - ab) * f / (int)period;
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
}

/* The F2 service menu: title + arcade game-rule settings. Joystick up/down picks
 * a row, left/right changes it; fire or the service key exits and writes SWOS.ini. */
static void arc_service_menu(void)
{
    extern uint8_t _pl1Up, _pl1Down, _pl1Left, _pl1Right, _pl1ShortFire;
    extern void Player1StatusProc(void);

    if (!g_arcadeOpts) return;

    /* Service palette = SVCFONT.bmp's own indexed palette (the NBA DIAGP colours):
     * 0=black 2=white 3=red 5=yellow 6=green 7=blue 9=cyan. */
    uint32_t BLACK = s_svcLoaded ? s_svcPal[0] : 0xFF000000u;
    uint32_t WHITE = s_svcLoaded ? s_svcPal[2] : 0xFFFFFFFFu;
    uint32_t YEL   = s_svcLoaded ? s_svcPal[5] : 0xFFFFE000u;
    uint32_t GREEN = s_svcLoaded ? s_svcPal[6] : 0xFF00FF00u;
    const int times[] = { 30, 45, 60, 90, 120 };
    const int NT = (int)(sizeof times / sizeof times[0]);
    const int NUM = 3;

    int sel = 0, u=0, d=0, l=0, r=0, f=1;   /* f=1: ignore a fire still held on entry */
    uint32_t frame = 0;
    int cx = _screen_width / 2;
    s_flagScreenActive = 1;

    while (_quitToDos == 0) {
        Player1StatusProc();
        int sv = 0; Platform_ArcadeInput(NULL, NULL, &sv, NULL, NULL);

        if (_pl1Up && !u)   sel = (sel - 1 + NUM) % NUM;
        if (_pl1Down && !d) sel = (sel + 1) % NUM;
        int adj = ((_pl1Right && !r) ? 1 : 0) - ((_pl1Left && !l) ? 1 : 0);
        if (adj) {
            if (sel == 0) {
                int idx = 1;
                for (int i = 0; i < NT; i++) if (times[i] == g_arcadeOpts->arcade_game_seconds) idx = i;
                g_arcadeOpts->arcade_game_seconds = times[(idx + adj + NT) % NT];
            } else if (sel == 1) g_arcadeOpts->arcade_golden_goal = !g_arcadeOpts->arcade_golden_goal;
            else                 g_arcadeOpts->arcade_penalties   = !g_arcadeOpts->arcade_penalties;
        }
        u=_pl1Up; d=_pl1Down; l=_pl1Left; r=_pl1Right;
        int exitNow = (_pl1ShortFire && !f) || sv;
        f = _pl1ShortFire;
        if (exitNow) break;

        frame++;
        /* background — pure black */
        size_t total = (size_t)_screen_width * SCREEN_HEIGHT;
        for (size_t i = 0; i < total; i++) s_flagScreenRGBA[i] = BLACK;

        /* title (green) + subtitle (yellow), enclosed in a yellow box */
        const char *title = "ARCADE SERVICE";
        const char *sub   = "OPENSWOS ARCADE - VER L1.0 6/26/26";
        int tw = arc_svc_textw(title, TROGF15_glyphs, TROGF15_COUNT);
        int sw = arc_svc_textw(sub,   TROGF7_glyphs,  TROGF7_COUNT);
        int boxW = (tw > sw ? tw : sw) + 28;
        for (int t = 0; t < 3; t++)   /* 3px-thick border */
            arc_flag_rect(cx - boxW / 2 + t, 8 + t, boxW - 2 * t, 50 - 2 * t, YEL);
        arc_svc_centered(cx, 14, GREEN, title, TROGF15_glyphs, TROGF15_COUNT);
        arc_svc_centered(cx, 42, YEL,   sub,   TROGF7_glyphs,  TROGF7_COUNT);

        /* selected row cycles Red->Black->Blue->Cyan->Green (blended) on a white box */
        uint32_t cyc[5] = { s_svcPal[3], s_svcPal[0], s_svcPal[7], s_svcPal[9], s_svcPal[6] };
        uint32_t selCol = arc_cycle_color(cyc, 5, frame, 14);

        char val[24];
        int ly = 92;
        for (int i = 0; i < NUM; i++) {
            const char *label = (i == 0) ? "GAME TIME" : (i == 1) ? "GOLDEN GOAL" : "PENALTIES ON DRAW";
            if      (i == 0) snprintf(val, sizeof val, "%d SEC", g_arcadeOpts->arcade_game_seconds);
            else if (i == 1) snprintf(val, sizeof val, "%s", g_arcadeOpts->arcade_golden_goal ? "ON" : "OFF");
            else             snprintf(val, sizeof val, "%s", g_arcadeOpts->arcade_penalties   ? "ON" : "OFF");
            uint32_t tc = YEL;
            if (i == sel) {
                arc_fill_rect(cx - 140, ly - 2, 280, 16, WHITE);
                tc = selCol;
            }
            arc_svc_text(cx - 130, ly, tc, label, TROGF7_glyphs, TROGF7_COUNT);
            arc_svc_text(cx + 70,  ly, tc, val,   TROGF7_glyphs, TROGF7_COUNT);
            ly += 18;
        }
        arc_svc_centered(cx, 224, WHITE, "JOYSTICK: CHANGE    FIRE / F2: EXIT + SAVE",
                         TROGF7_glyphs, TROGF7_COUNT);

        WaitForVSync();
        ReadTimerDelta();
    }
    s_flagScreenActive = 0;

    if (g_arcadeIniPath) {
        Settings_Save(g_arcadeOpts, g_arcadeIniPath);
        fprintf(stderr, "[arcade] service settings saved to %s\n", g_arcadeIniPath);
    }
}

void ArcadeMainLoop(void)
{
    fprintf(stderr, "[arcade] boot-to-arcade: entering attract loop\n");

    /* The boot loader leaves the copper palette faded to black; restore the menu
     * palette so anything we draw is visible. */
    { extern uint16_t _menuPalette[]; WriteCopperPalette(_menuPalette); }

    arc_load_logo();
    arc_load_flags();
    arc_load_svcfont();
    arc_scan_reels();
    ReadTimerDelta();

    while (_quitToDos == 0) {
        if (s_serviceReq) {              /* F2 pressed during a reel/idle */
            s_serviceReq = 0;
            arc_service_menu();
            continue;
        }
        if (s_startGame) {               /* start pressed during a reel/idle */
            arc_start_game();
            continue;
        }

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
