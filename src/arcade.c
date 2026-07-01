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
#include "game_types.h"     /* TeamFileEx — arcade match teams */

int _gArcadeMode = 0;

/* When set, the SDL SFX trigger path is suppressed (AudioSDL_TriggerSfx*). The
 * attract reels are match replays whose recorded crowd/SFX events would otherwise
 * fire on the SAME single RJP1 player that's playing the HERO attract song,
 * corrupting it (post-match, once the SFX bank is loaded — at boot it isn't, which
 * is why it only broke after a match). On during attract (HERO only), off for the
 * actual match (real crowd SFX wanted). SDL-only. */
int _gArcadeSfxSuppressed = 0;

/* When set, the arcade themed pitch (a diced SWCPICH BMP) is active for the match:
 * LoadPitchMap dices the BMP into the tile bank instead of loading PITCH*.BLK, and
 * refresh_palette_from_copper builds the pitch palette (16..255) from the BMP. On
 * only during the arcade MATCH (attract reels use the normal DOS pitch). SDL-only. */
int _gArcadePitchActive = 0;

/* Match-title shown on the pre-match stadium/line-up screen in arcade mode, in
 * place of "FRIENDLY" (set into _gameName by InitAndPlayGame). */
uint8_t _aArcadeMatchName[] = "GOLDEN GOAL";

/* Arcade game modes — shown in the F2 service menu, persisted as [arcade] mode in
 * SWOS.ini, documented in docs/ARCADE.md. Index 0 is the default. To add a mode:
 * add its name here and wire its rules into the match setup. */
static const char *const s_arcadeModeNames[] = {
    "GOLDEN GOAL",
};
int Arcade_ModeCount(void) { return (int)(sizeof s_arcadeModeNames / sizeof s_arcadeModeNames[0]); }
const char *Arcade_ModeName(int idx)
{
    if (idx < 0 || idx >= Arcade_ModeCount()) idx = 0;
    return s_arcadeModeNames[idx];
}
int Arcade_ModeFromName(const char *name)
{
    if (!name) return -1;
    for (int i = 0; i < Arcade_ModeCount(); i++)
        if (strcasecmp(name, s_arcadeModeNames[i]) == 0) return i;
    return -1;
}

extern void WaitForVSync(void);
extern uint16_t ReadTimerDelta(void);
extern volatile uint16_t _quitToDos;
extern volatile uintptr_t _frontbufferBasePtr;
extern volatile uintptr_t _frontbufferBaseAltPtr;
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

/* match launch (mirrors PlayFriendly: match_sim.c) + national-team loading */
extern TeamFileEx _selectedTeams[];
extern uint16_t   _numSelectedTeams;
extern uint8_t    _teamFileBuffer[];
extern int16_t    LoadTeamFile(uint16_t extNumber);
extern void       InitTeams(void);
extern void       InitAndPlayGame(TeamFileEx *team1, TeamFileEx *team2);

/* music: HERO (title) in attract, MENU on entering country (flag) select */
extern void     Arcade_PlayHeroMusic(void);   /* loader_intro.c (SDL-only) */
extern void     InitMenuMusic(void);          /* audio.c — load MENU.SNG/.INS */
extern void     TryPlayMusic(void);           /* audio.c — start menu song */
extern void     StopMusic(void);              /* audio.c — fade out + stop */
extern void     LoadSfxResources(void);       /* audio.c — load the in-game SFX bank */
extern void     EnsureSfxLoadedAndStart(void);/* audio.c — activate SFX playback */

extern PlatformInitOptions *g_arcadeOpts;     /* live arcade options (defined below) */

/* Menu sprite graphics (little-player figures the stadium/line-up screen draws).
 * The front-end normally extracts these from menus.raw via the menu-BG image
 * script (PrepareMenuBackground); arcade skips the front-end, so we run the
 * extraction ourselves before a match. */
extern uint16_t _menuBGImageCmdStream[];
extern uint8_t *_menuImageStreamBaseEnd;
extern void     ProcessImageCommandStream(const uint16_t *cmdBuf, uint8_t *workBuf);

#define ARC_WHITE         2          /* menu-palette white (0x0FFF) for the UI text */
#define ARC_CHARS         ((uint8_t *)_bigCharsTable)
#define ARC_Y_BOTTOM      256        /* INSERT COIN / CREDIT NN — machine status, screen bottom */
#define ARC_Y_PROMPT      224        /* PUSH START BUTTON */
#define ARC_Y_PLAYERS     240        /* ONE PLAYER ONLY / ONE OR TWO PLAYERS — under PUSH START */
#define ARC_ASSET_INIT    2          /* CAssetSys ASSET_OP_INIT_SOURCE0 */
#define ARC_REEL_DIR      "grafs_arcade/attract_mode"
#define ARC_MAX_REELS     64

static char     s_reels[ARC_MAX_REELS][64];
static int      s_reelCount;
static char     s_reelDir[160] = ARC_REEL_DIR;  /* resolved per gameplay mode (see below) */
static uint32_t s_rngState = 0x1234abcdu;
static int      s_credits;     /* total credits in the cabinet */
static int      s_startGame;   /* pending game: 0 none, 1 one-player, 2 two-player */
static int      s_serviceReq;  /* service key pressed -> open the F2 service menu */
static int      s_arcModalRunOver; /* GAME OVER / name-entry / result modal: coins accumulate but
                                    * start/service do NOT fire (so the flow can't be interrupted) */
static uint32_t s_overlayFrame;           /* blink phase for the attract overlay */

#define ARC_LOGO_MAXW  1024
#define ARC_LOGO_SCALE 1     /* DOS LOADER1 LOGO.bmp is already full-size (320x200) */
#define ARC_LOGO_TRANSPARENT 0   /* DOS LOADER1 uses index 0 as the transparent background */
static uint8_t  s_logoBuf[ARC_LOGO_MAXW * SCREEN_HEIGHT];    /* LOGO.bmp pixels (indexed) */
static uint32_t s_logoPal[256];                             /* LOGO.bmp 256-colour palette (ARGB) */
static int      s_logoW, s_logoH;                           /* LOGO.bmp dimensions */
static int      s_logoLoaded;
static uint8_t  s_overlayBuf[ARC_LOGO_MAXW * SCREEN_HEIGHT]; /* INSERT COIN / prompts, menu-palette idx */
static int      s_overlayActive;
static int      s_arcInMatch;   /* 1 while InitAndPlayGame runs — suppresses the attract overlay
                                 * during an in-match GOAL replay (PlayInstantReplayLoop shares the
                                 * Arcade_DrawAttractOverlay hook with the attract reel loop) */

/* Attract FOREGROUND cross-fade: the reel ALWAYS plays in the background; the
 * foreground cycles logo -> (fade) -> high-score table -> (fade) -> logo, composited
 * over the live reel. The two INSERT COIN prompts at the bottom never fade. */
static uint8_t  s_tableBuf[ARC_LOGO_MAXW * SCREEN_HEIGHT];  /* rendered table text, menu-font idx */
static int      s_fgLogoAlpha  = 255;   /* 0..255 logo opacity   */
static int      s_fgTableAlpha = 0;     /* 0..255 table opacity  */
static uint32_t s_fgTimer;              /* frames elapsed in the current fg phase */
static int      s_fgPhase;              /* 0=logo hold 1=logo-out 2=table-in 3=table hold 4=table-out 5=logo-in */
static int      s_fgTableMode;          /* game mode whose table the table phase shows */
static void     arc_render_table_to_buf(int mode);   /* defined in the high-score section */
static void     arc_draw_table_flags(int mode, uint32_t *dst, int pitch, int alpha);
static void     arc_build_menu_argb(uint32_t pal[16]);

/* High-score table colours — indices into _menuPalette (already loaded): 14=green,
 * 6=orange, 10=red, 2=white. The menu-font glyph blit stores the colour index in the
 * pixel, so both composite paths map it back through the menu palette. */
#define HS_COL_TITLE   14    /* GOLDEN GOAL + HIGH SCORES -> green  */
#define HS_COL_HEADER  6     /* RANK / NAME / WINS / TIME -> orange */
#define HS_COL_TOP     10    /* the #1 row               -> red    */
#define HS_COL_ROW     2     /* the other rows           -> white  */

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

/* Pick the reel subdir for the current gameplay mode: attract_mode/dos (DOS
 * gameplay) or attract_mode/amiga (Amiga). Reels are recorded at the gameplay FPS
 * (DOS 70 / Amiga 50), so they must match _gDosGameplay. Falls back to
 * attract_mode/ itself if the mode subdir doesn't exist. */
static void arc_resolve_reel_dir(void)
{
    extern int _gDosGameplay;
    char cand[160];
    snprintf(cand, sizeof cand, "%s/%s", ARC_REEL_DIR, _gDosGameplay ? "dos" : "amiga");
    DIR *d = opendir(cand);
    if (d) { closedir(d); snprintf(s_reelDir, sizeof s_reelDir, "%s", cand); }
    else   { snprintf(s_reelDir, sizeof s_reelDir, "%s", ARC_REEL_DIR); }
}

/* Scan the resolved reel dir for .HIL/.RPL reels. Returns the count. */
static int arc_scan_reels(void)
{
    arc_resolve_reel_dir();
    s_reelCount = 0;
    DIR *d = opendir(s_reelDir);
    if (!d) {
        fprintf(stderr, "[arcade] no attract reels: cannot open %s\n", s_reelDir);
        return 0;
    }
    struct dirent *e;
    while ((e = readdir(d)) != NULL && s_reelCount < ARC_MAX_REELS) {
        if (e->d_name[0] == '.') continue;
        if (arc_ends_with(e->d_name, ".HIL") || arc_ends_with(e->d_name, ".RPL")) {
            /* The .HIL converter reads the (larger) DOS source into _hilFileBuffer
             * (192254 B), so skip reels too big for it; RPLs use the 2.76 MB buffer. */
            char p[224];
            snprintf(p, sizeof p, "%s/%s", s_reelDir, e->d_name);
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
    fprintf(stderr, "[arcade] %d attract reel(s) in %s\n", s_reelCount, s_reelDir);
    return s_reelCount;
}

/* Load reel `name` (relative to attract_mode/) into the replay engine. The
 * direct asset dispatch resolves paths relative to bin/ and has no ~91 KB
 * frontbuffer-stage cap, so it handles the multi-MB RPLs. Returns 0 on success. */
static int arc_load_reel(const char *name)
{
    char path[224];
    snprintf(path, sizeof path, "%s/%s", s_reelDir, name);
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
        { extern void AudioSDL_PlayOverlaySample(void); AudioSDL_PlayOverlaySample(); }
    }
    if (s_arcModalRunOver) return;   /* run-over / name-entry: accept coins only */
    if (sv) s_serviceReq = 1;
    if (st1 && s_credits >= 1)      { s_credits -= 1; s_startGame = 1; }
    else if (st2 && s_credits >= 2) { s_credits -= 2; s_startGame = 2; }
    if (s_startGame)
        fprintf(stderr, "[arcade] START %d-player game (credits left %d)\n",
                s_startGame, s_credits);
}

/* Load the arcade LOGO.bmp once: indexed pixels into s_logoBuf + its own 256-colour
 * palette into s_logoPal. Composited (scaled up, centred) over the attract reels by
 * Arcade_CompositeOverlay — index 0 is transparent. */
static void arc_load_logo(void)
{
    extern int Platform_LoadIndexedBMP(const char *path, uint8_t *out8, int out_cap,
                                       uint32_t *outPal, int *outW, int *outH);
    if (s_logoLoaded) return;
    if (Platform_LoadIndexedBMP("grafs_arcade/LOGO.bmp", s_logoBuf, (int)sizeof s_logoBuf,
                                s_logoPal, &s_logoW, &s_logoH) == 0) {
        s_logoLoaded = 1;
        fprintf(stderr, "[arcade] LOGO.bmp %dx%d loaded\n", s_logoW, s_logoH);
    } else {
        fprintf(stderr, "[arcade] LOGO.bmp failed to load\n");
    }
}

/* Foreground cross-fade phase durations (frames). The reel runs ~50/70 fps under it. */
#define FG_LOGO_HOLD   (6 * 50)
#define FG_TABLE_HOLD  (6 * 50)
#define FG_FADE        (40)        /* fade time each direction (~0.8 s) */

/* Alpha-blend `over` onto `base` (both 0xAARRGGBB) by a (0..255). */
static uint32_t arc_blend(uint32_t base, uint32_t over, int a)
{
    if (a >= 255) return over;
    if (a <= 0)   return base;
    int br = (base >> 16) & 0xFF, bg = (base >> 8) & 0xFF, bb = base & 0xFF;
    int orr = (over >> 16) & 0xFF, og = (over >> 8) & 0xFF, ob = over & 0xFF;
    int r = (orr * a + br * (255 - a)) / 255;
    int g = (og  * a + bg * (255 - a)) / 255;
    int b = (ob  * a + bb * (255 - a)) / 255;
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* Advance the logo<->table foreground cross-fade by one frame (called once per
 * attract frame from Arcade_DrawAttractOverlay). Sequence: hold logo -> fade logo
 * out -> fade table in -> hold table -> fade table out -> fade logo in -> repeat.
 * Each table phase shows the next game mode's table. */
static void arc_fg_advance(void)
{
    s_fgTimer++;
    switch (s_fgPhase) {
    case 0:                                   /* logo hold */
        s_fgLogoAlpha = 255; s_fgTableAlpha = 0;
        if (s_fgTimer >= FG_LOGO_HOLD) { s_fgPhase = 1; s_fgTimer = 0; }
        break;
    case 1:                                   /* logo fade out */
        s_fgLogoAlpha = 255 - (int)(255 * s_fgTimer / FG_FADE);
        if (s_fgTimer >= FG_FADE) { s_fgLogoAlpha = 0;
            arc_render_table_to_buf(s_fgTableMode);   /* build the table about to fade in */
            s_fgPhase = 2; s_fgTimer = 0; }
        break;
    case 2:                                   /* table fade in */
        s_fgTableAlpha = (int)(255 * s_fgTimer / FG_FADE);
        if (s_fgTimer >= FG_FADE) { s_fgTableAlpha = 255; s_fgPhase = 3; s_fgTimer = 0; }
        break;
    case 3:                                   /* table hold */
        s_fgTableAlpha = 255; s_fgLogoAlpha = 0;
        if (s_fgTimer >= FG_TABLE_HOLD) { s_fgPhase = 4; s_fgTimer = 0; }
        break;
    case 4:                                   /* table fade out */
        s_fgTableAlpha = 255 - (int)(255 * s_fgTimer / FG_FADE);
        if (s_fgTimer >= FG_FADE) { s_fgTableAlpha = 0;
            { int mc = Arcade_ModeCount(); s_fgTableMode = (s_fgTableMode + 1) % (mc > 0 ? mc : 1); }
            s_fgPhase = 5; s_fgTimer = 0; }
        break;
    default:                                  /* 5: logo fade in */
        s_fgLogoAlpha = (int)(255 * s_fgTimer / FG_FADE);
        if (s_fgTimer >= FG_FADE) { s_fgLogoAlpha = 255; s_fgPhase = 0; s_fgTimer = 0; }
        break;
    }
}

/* The attract overlay foreground: a SWOS logo <-> high-score table cross-fade plus
 * the flashing INSERT COIN / credit prompts, all composited OVER the continuously
 * playing reel by Arcade_CompositeOverlay (the reel is never interrupted). The
 * logo/table use their own palettes; the prompts are menu-font white+shadow. Drawn
 * each reel frame (hook in PlayHighlightsLoop) and on the idle screen. */
int Arcade_DrawAttractOverlay(void)
{
    /* The in-match goal replay (PlayInstantReplayLoop) shares this hook with the
     * attract reel loop. During a real match don't draw the attract overlay or take
     * attract input — leave s_overlayActive alone (0) so Arcade_CompositeOverlay is a
     * no-op, and return 0 so the replay isn't aborted. */
    if (s_arcInMatch)
        return 0;

    arc_drain_input();
    s_overlayFrame++;
    arc_fg_advance();
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
        DrawMenuTextCentered(168, ARC_Y_PLAYERS, ARC_WHITE, (uint8_t *)(
            (s_credits >= 2) ? "ONE OR TWO PLAYERS" : "ONE PLAYER ONLY"), ARC_CHARS);
    }

    _frontbufferBasePtr = saved;
    s_overlayActive = 1;
    return (s_startGame != 0) || s_serviceReq;   /* non-zero -> leave the attract reel */
}

/* Composite the attract foreground onto the final ARGB8888 frame, over the live reel
 * (called by the platform present path after chunky_to_rgba). Three layers:
 *   1. the SWOS logo, blended at s_fgLogoAlpha (its own palette, index 0 transparent)
 *   2. the high-score table text, blended at s_fgTableAlpha (menu-font white+shadow)
 *   3. the INSERT COIN / credit prompts, always opaque (they never fade)
 * The logo and table cross-fade through the reel (arc_fg_advance drives the alphas);
 * the reel itself is untouched, so it keeps playing in the background. */
void Arcade_CompositeOverlay(uint32_t *dst, int dst_pitch_pixels, int w, int h)
{
    if (!s_overlayActive || !dst)
        return;

    const uint32_t white = 0xFFFFFFFFu;
    const uint32_t black = 0xFF000000u;
    const uint8_t  shadowBit = (uint8_t)(1u << (NUM_PLANES - 1)); /* glyph shadow plane */
    const int la = s_fgLogoAlpha, ta = s_fgTableAlpha;
    uint32_t mpal[16]; arc_build_menu_argb(mpal);   /* table text -> menu palette colours */

    if (h > SCREEN_HEIGHT)  h = SCREEN_HEIGHT;
    if (w > _screen_width)  w = _screen_width;

    /* LOGO.bmp, centred horizontally in the area above the prompt row. */
    const int lscale = ARC_LOGO_SCALE;
    const int lw = s_logoW * lscale, lh = s_logoH * lscale;
    const int lx0 = (_screen_width - lw) / 2;
    const int ly0 = (ARC_Y_PROMPT - lh) / 2;

    for (int y = 0; y < h; y++) {
        const uint8_t *tx  = s_overlayBuf + (size_t)y * (size_t)_screen_width;  /* prompts */
        const uint8_t *tb  = s_tableBuf   + (size_t)y * (size_t)_screen_width;  /* table   */
        uint32_t      *out = dst + (size_t)y * (size_t)dst_pitch_pixels;

        int ly = y - ly0;
        const uint8_t *lgrow = (s_logoLoaded && la > 0 && ly >= 0 && ly < lh)
            ? s_logoBuf + (size_t)(ly / lscale) * (size_t)s_logoW : NULL;

        for (int x = 0; x < w; x++) {
            uint32_t px = out[x];                         /* the reel pixel underneath */

            if (lgrow) {                                  /* 1. logo (faded) */
                int lx = x - lx0;
                if (lx >= 0 && lx < lw) {
                    uint8_t idx = lgrow[lx / lscale];
                    if (idx != ARC_LOGO_TRANSPARENT)
                        px = arc_blend(px, s_logoPal[idx], la);
                }
            }
            if (ta > 0) {                                 /* 2. table text (faded, coloured) */
                uint8_t t = tb[x];
                if (t) px = arc_blend(px, (t & (uint8_t)~shadowBit) ? mpal[t & 0x0F] : black, ta);
            }
            uint8_t t2 = tx[x];                           /* 3. prompts (opaque, white) */
            if (t2) px = (t2 & (uint8_t)~shadowBit) ? white : black;

            out[x] = px;
        }
    }

    /* Country flags next to each table row, faded in/out with the table text. */
    if (ta > 0)
        arc_draw_table_flags(s_fgTableMode, dst, dst_pitch_pixels, ta);
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
static int      s_arcadeFade = 256;   /* full-screen brightness 0..256 (256 = full) */
static uint8_t  s_menubgBuf[ARC_LOGO_MAXW * SCREEN_HEIGHT];  /* MENUBG.RAW, menu-palette idx */
static int      s_menubgLoaded;

extern int Platform_LoadIndexedBMP(const char *path, uint8_t *out8, int out_cap,
                                   uint32_t *outPal, int *outW, int *outH);

/* Decode MENUBG.RAW (the SWOS menu background) into s_menubgBuf the same way as
 * the logo, for use behind the flag/pitch selectors. */
static void arc_load_menubg(void)
{
    extern void    LoadOverlayToFrontbuffer(const uint8_t *path);
    extern uint8_t _frontbufferMaskBuffer[];
    size_t n = (size_t)_screen_width * (size_t)SCREEN_HEIGHT;
    if (n > sizeof s_menubgBuf) n = sizeof s_menubgBuf;
    LoadOverlayToFrontbuffer((const uint8_t *)"grafs/menubg.raw");
    memcpy(s_menubgBuf, _frontbufferMaskBuffer, n);
    s_menubgLoaded = 1;
}

/* Paint the menu background (menu-palette indices -> ARGB) into the full-screen
 * RGBA. The decode is centred in the 336 region; margins are near-black. */
static void arc_fill_menubg(void)
{
    extern uint16_t _menuPalette[];
    uint32_t pal[16];
    for (int i = 0; i < 16; i++) {
        uint16_t v = _menuPalette[i];
        uint32_t r = (v >> 8) & 0xF, g = (v >> 4) & 0xF, b = v & 0xF;
        r = (r << 4) | r; g = (g << 4) | g; b = (b << 4) | b;
        pal[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
    size_t n = (size_t)_screen_width * (size_t)SCREEN_HEIGHT;
    if (n > sizeof s_menubgBuf) n = sizeof s_menubgBuf;
    for (size_t i = 0; i < n; i++)
        s_flagScreenRGBA[i] = s_menubgLoaded ? pal[s_menubgBuf[i] & 15] : 0xFF000000u;
}

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
    int fade = s_arcadeFade;
    for (int y = 0; y < h; y++) {
        const uint32_t *src = s_flagScreenRGBA + (size_t)y * (size_t)_screen_width;
        uint32_t       *out = dst + (size_t)y * (size_t)dst_pitch_pixels;
        if (fade >= 256) {
            memcpy(out, src, (size_t)w * sizeof(uint32_t));
        } else {
            for (int x = 0; x < w; x++) {
                uint32_t c = src[x];
                uint32_t r = (((c >> 16) & 0xFFu) * (uint32_t)fade) >> 8;
                uint32_t g = (((c >> 8)  & 0xFFu) * (uint32_t)fade) >> 8;
                uint32_t b = (( c        & 0xFFu) * (uint32_t)fade) >> 8;
                out[x] = 0xFF000000u | (r << 16) | (g << 8) | b;
            }
        }
    }
    return 1;
}

/* Ramp the arcade full-screen brightness from its current value to `target`
 * (0..256) over `frames`, re-presenting each frame so the flag/pitch screens
 * fade smoothly. The caller must keep s_flagScreenActive set and the last frame
 * drawn into s_flagScreenRGBA (held static for a fade-out). */
static void arc_fade_ramp(int target, int frames)
{
    if (frames < 1) frames = 1;
    int start = s_arcadeFade;
    for (int f = 1; f <= frames && _quitToDos == 0; f++) {
        s_arcadeFade = start + (target - start) * f / frames;
        WaitForVSync();
        ReadTimerDelta();
    }
    s_arcadeFade = target;
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
    s_arcadeFade = 0;                       /* fade in from black */
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

        /* menu background */
        arc_fill_menubg();
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

        if (s_arcadeFade < 256) { s_arcadeFade += 24; if (s_arcadeFade > 256) s_arcadeFade = 256; }
        WaitForVSync();
        ReadTimerDelta();
    }
    arc_fade_ramp(0, 10);                   /* fade out to black for the pitch screen */
    s_flagScreenActive = 0;
    *outP1 = sel1; if (outP2) *outP2 = sel2;
}

static int arc_pitch_select(void);   /* defined after the SVCFONT helpers */
static int arc_load_pitch(int idx);  /* defined in the pitch-select section */

/* High-score session screens (defined in the high-score section, before ArcadeMainLoop). */
static void arc_run_over(int mode, uint32_t wins, uint32_t totalSecs, int flag);
static int  arc_continue_screen(uint32_t wins);

/* ====================================================================
 * Phase 3c — the match (mirror PlayFriendly): country/flag select picks two
 * national teams, loaded into _selectedTeams; InitAndPlayGame runs the match.
 * ==================================================================== */

/* ATTRACT AUDIO setting values (SWOS.ini [arcade] attract_audio). */
#define ARC_ATTRACT_AUDIO_MUSIC      0   /* HERO only, reel SFX suppressed */
#define ARC_ATTRACT_AUDIO_SFX        1   /* reel crowd/SFX only, no music */
#define ARC_ATTRACT_AUDIO_MUSIC_SFX  2   /* HERO + reel crowd/SFX mixed (default) */

/* Attract uses HERO (title) music; entering country/pitch select switches to the
 * MENU music. */
static void arc_music_attract(void)
{
    extern void AudioSDL_SetSongLoop(int loop);
    extern void AudioSDL_SetSfxSecondPlayer(int on);
    /* MUSIC = HERO only; SFX = reel crowd/SFX only; MUSIC+SFX = both, with HERO on
     * the music player and the reel SFX on a dedicated second RJP1 player so they
     * mix (no Amiga 4-channel limit on the SDL host). */
    int mode = g_arcadeOpts ? g_arcadeOpts->arcade_attract_audio : ARC_ATTRACT_AUDIO_MUSIC_SFX;
    if (mode == ARC_ATTRACT_AUDIO_SFX) {
        AudioSDL_SetSfxSecondPlayer(0);   /* SFX on the shared player */
        _gArcadeSfxSuppressed = 0;        /* let reel crowd/SFX through */
        LoadSfxResources();
        EnsureSfxLoadedAndStart();
    } else if (mode == ARC_ATTRACT_AUDIO_MUSIC_SFX) {
        AudioSDL_SetSfxSecondPlayer(1);   /* SFX -> 2nd player, mixed with HERO */
        _gArcadeSfxSuppressed = 0;
        Arcade_PlayHeroMusic();           /* HERO -> music player (clears the queue) */
        AudioSDL_SetSongLoop(1);          /* HERO has no loop-back; loop it */
        LoadSfxResources();
        EnsureSfxLoadedAndStart();        /* SFX bank -> 2nd player (leaves HERO's queue) */
    } else { /* ARC_ATTRACT_AUDIO_MUSIC */
        AudioSDL_SetSfxSecondPlayer(0);
        _gArcadeSfxSuppressed = 1;        /* HERO only; ignore reel SFX triggers */
        Arcade_PlayHeroMusic();
        AudioSDL_SetSongLoop(1);
    }
}
static void arc_music_menu(void)
{
    extern void AudioSetChannelVolume(uint16_t, uint16_t);
    extern void AudioSDL_StopSong(void);
    extern void AudioSDL_StopSfx(void);
    extern void AudioSDL_SetSfxSecondPlayer(int on);
    extern uint16_t _audioActive, _audioBankLoaded;
    /* Stop any attract SFX running on the 2nd player + revert to the shared player
     * (in-game/menu SFX use the shared player). */
    AudioSDL_StopSfx();
    AudioSDL_SetSfxSecondPlayer(0);
    /* Quick-stop HERO (skip StopMusic's ~1.4 s fade — the screen is black during
     * the swap), then load + start MENU (TryPlayMusic ramps it in). */
    AudioSetChannelVolume(0xFFFF, 0);
    AudioSDL_StopSong();
    _audioActive = 0;
    _audioBankLoaded = 0;
    InitMenuMusic();    /* load MENU.SNG / MENU.INS */
    TryPlayMusic();     /* start it (faded in) */
}

/* flag cell -> {national-team file byte, team ordinal, ISO label}. file ==
 * ARC_NO_TEAM => SWOS has no national team for that flag; load the generic
 * placeholder squad. National-team files: 0x50 Europe / 0x51 Africa /
 * 0x52 S.America / 0x53 N&C.America / 0x54 Asia / 0x55 Oceania. Full table +
 * rationale (incl. the 5 generic cells) in docs/ARCADE.md. */
#define ARC_NO_TEAM       0xFF
#define ARC_GENERIC_FILE  0x50   /* placeholder squad template = team.080 ... */
#define ARC_GENERIC_ORD   0x0B   /* ...ordinal 0x0B (ENGLAND), relabelled per cell */
#define ARC_CTRL_HUMAN    0
#define ARC_CTRL_COMPUTER 1      /* TEAM_COMPUTER */

static const struct { uint8_t file, ord; char code[4]; } s_flagTeam[FLAG_COUNT] = {
    {0x53,0x08,"MEX"}, {0x53,0x01,"CAN"}, {0x52,0x02,"BRA"}, {0x53,0x0E,"USA"}, {0x50,0x11,"GER"}, {0x50,0x13,"NED"},
    {0x54,0x18,"KOR"}, {0x50,0x2C,"SUI"}, {0x51,0x13,"MAR"}, {0x55,0x00,"AUS"}, {0x52,0x05,"ECU"}, {0x54,0x09,"JPN"},
    {0x51,0x19,"RSA"}, {0x54,0x15,"QAT"}, {0x50,0x27,"SCO"}, {0x52,0x06,"PAR"}, {0x51,0x0D,"CIV"}, {0x51,0x1E,"TUN"},
    {0x50,0x09,"CZE"}, {ARC_NO_TEAM,0,"BIH"}, {ARC_NO_TEAM,0,"HAI"}, {0x50,0x2D,"TUR"}, {ARC_NO_TEAM,0,"CUW"}, {0x50,0x2B,"SWE"},
    {0x50,0x05,"BEL"}, {0x50,0x2A,"ESP"}, {0x50,0x0F,"FRA"}, {0x52,0x00,"ARG"}, {0x50,0x22,"POR"}, {0x50,0x0B,"ENG"},
    {0x54,0x07,"IRN"}, {0x52,0x08,"URU"}, {0x51,0x17,"SEN"}, {0x50,0x02,"AUT"}, {0x52,0x04,"COL"}, {0x50,0x07,"CRO"},
    {0x51,0x08,"EGY"}, {0x54,0x16,"KSA"}, {0x50,0x20,"NOR"}, {0x51,0x00,"ALG"}, {ARC_NO_TEAM,0,"UZB"}, {0x53,0x0A,"PAN"},
    {0x55,0x02,"NZL"}, {ARC_NO_TEAM,0,"CPV"}, {0x54,0x08,"IRQ"}, {0x54,0x0A,"JOR"}, {0x51,0x07,"COD"}, {0x51,0x0B,"GHA"},
};

/* Load the national team for flag cell `flag` into _selectedTeams[slot] and set
 * who controls it. Returns 0 on success. LoadTeamFile caches by file number, so
 * loading P1 then P2 each does its own load + find + copy atomically. */
static int arc_load_team(int flag, int slot, uint8_t controls)
{
    if (flag < 0 || flag >= FLAG_COUNT) return -1;
    uint8_t file = s_flagTeam[flag].file, ord = s_flagTeam[flag].ord;
    const char *label = s_flagTeam[flag].code;
    int generic = (file == ARC_NO_TEAM);
    if (generic) { file = ARC_GENERIC_FILE; ord = ARC_GENERIC_ORD; }

    if (LoadTeamFile(file) != 0) {
        fprintf(stderr, "[arcade] LoadTeamFile(0x%02X) failed for %s\n", file, label);
        return -1;
    }
    /* _teamFileBuffer: [u16 host-native count][count * TeamFileEx(684B)] */
    uint16_t count = *(uint16_t *)_teamFileBuffer;
    TeamFileEx *rec = (TeamFileEx *)(_teamFileBuffer + 2), *found = (TeamFileEx *)0;
    for (uint16_t i = 0; i < count; i++) {
        if (rec[i].team.countryNumber == file && rec[i].team.teamOrdinal == ord) {
            found = &rec[i];
            break;
        }
    }
    if (!found) {
        fprintf(stderr, "[arcade] team 0x%02X.%02X (%s) not found in file\n", file, ord, label);
        return -1;
    }

    memcpy(&_selectedTeams[slot], found, sizeof(TeamFileEx));
    _selectedTeams[slot].team.teamControls = controls;
    if (generic) {
        /* Placeholder squad: a real (valid) team relabelled to the cell's code so
         * the flag still reads correctly. TODO: replace with a proper generic team
         * when the arcade team/animation system lands. */
        int8_t *nm = _selectedTeams[slot].team.teamName;
        memset(nm, 0, sizeof(_selectedTeams[slot].team.teamName));
        snprintf((char *)nm, sizeof(_selectedTeams[slot].team.teamName), "%s", label);
    }
    fprintf(stderr, "[arcade] P%d team: %s (file 0x%02X ord 0x%02X, ctrl %u)\n",
            slot + 1, label, file, ord, controls);
    return 0;
}

/* ====================================================================
 * Arcade match rules (Parts 2-3) — DETERMINISTIC, driven by the game-frame
 * step (one tick per game-loop iteration), never a wall clock, so the sim
 * stays deterministic for future lockstep multiplayer.
 *
 * - Length: end after game_seconds * ARC_FRAMES_PER_SEC frames of play.
 * - Golden goal: the first goal ends the match (the scorer is ahead).
 * - Penalties: a level score at the time limit goes to a shootout.
 * The match's own clock is run slow (_gameLength = 3 in arc_start_game) so the
 * engine's half-time/full-time never fire within the arcade limit — no edits to
 * the shared match-clock code needed.
 * ==================================================================== */
#define ARC_FRAMES_PER_SEC 50   /* nominal sim frames per arcade second (Amiga PAL cadence) */

extern PlatformInitOptions *g_arcadeOpts;   /* live options (defined below) */

static int  s_arcGoldenGoal;    /* end on first goal */
static int  s_arcPenalties;     /* draw -> penalty shootout */
static int  s_arcMatchActive;   /* gates the per-frame logic for the current match */
static int  s_arcTimerStarted;  /* the frame count begins at kickoff (first live frame) */
static long s_arcFramesPlayed;  /* deterministic count of frames since kickoff */
static long s_arcLimitFrames;   /* end the match at this many frames */
static int  s_arcGoalPendingEnd;      /* golden goal scored: end once the celebration + replay resolve */
static int  s_arcGoalCelebrationSeen; /* the goal-camera celebration has been observed running */
static int  s_arcDebugEndRun;         /* DEBUG instant-win key pressed: end the run -> name entry */
static int  s_arcDebugForceNameEntry; /* DEBUG: show name entry even if the run wouldn't qualify */

/* Latch the rule settings for the match about to start (called from arc_start_game). */
static void arc_begin_match_rules(void)
{
    extern uint16_t _autoReplays;   /* game_timers.c goal-replay gate */
    int secs = (g_arcadeOpts && g_arcadeOpts->arcade_game_seconds > 0)
               ? g_arcadeOpts->arcade_game_seconds : 45;
    s_arcGoldenGoal  = g_arcadeOpts ? g_arcadeOpts->arcade_golden_goal : 1;
    s_arcPenalties   = g_arcadeOpts ? g_arcadeOpts->arcade_penalties  : 1;
    s_arcLimitFrames = (long)secs * ARC_FRAMES_PER_SEC;
    s_arcFramesPlayed = 0;
    s_arcTimerStarted = 0;
    s_arcMatchActive  = 1;
    s_arcGoalPendingEnd = 0;
    s_arcGoalCelebrationSeen = 0;
    s_arcDebugEndRun = 0;
    /* GOAL REPLAY setting -> the engine's goal auto-replay. */
    _autoReplays = (g_arcadeOpts && g_arcadeOpts->arcade_goal_replay) ? 1 : 0;
    _gArcadeSfxSuppressed = 0;   /* the match wants real crowd / in-game SFX */
}

void Arcade_MatchTick(void)
{
    extern uint16_t _gameStatePl;
    extern uint16_t _playingPenalties;
    extern uint16_t _team1TotalGoals, _team2TotalGoals, _playGame;
    extern volatile uint16_t _goalCameraMode;
    extern uint16_t _replayState;
    extern void StartPenalties(void);

    extern int Platform_ArcadeDebugWin(void);

    if (!_gArcadeMode || !s_arcMatchActive) return;
    if (_playingPenalties) return;                  /* the shootout runs + ends itself */

    /* DEBUG instant-win key (F10): make P1/champion the winner, end the match, and
     * flag the run to stop -> GAME OVER -> name entry. For testing the high-score flow. */
    if (Platform_ArcadeDebugWin()) {
        if (_team1TotalGoals <= _team2TotalGoals)
            _team1TotalGoals = (uint16_t)(_team2TotalGoals + 1);
        s_arcDebugEndRun = 1;
        s_arcMatchActive = 0;
        _playGame = 0;
        return;
    }

    /* Golden goal scored: do NOT cut the match on a fixed timer. The old 2-second
     * shrink raced the goal celebration + AUTO REPLAYS and ended the match before
     * the replay could play ("a goal immediately exits"). Instead let the whole
     * goal sequence resolve — the engine runs the celebration (_goalCameraMode != 0)
     * and, if AUTO REPLAYS is on, the instant replay (_replayState != 0, with
     * _goalCameraMode forced to 0 on the replay frame) — then end once everything has
     * settled. The scorer is ahead, so the result picks a winner, not penalties. */
    if (s_arcGoalPendingEnd) {
        if (_goalCameraMode != 0) s_arcGoalCelebrationSeen = 1;   /* celebration began */
        if (s_arcGoalCelebrationSeen && _goalCameraMode == 0 && _replayState == 0) {
            s_arcMatchActive = 0;
            _playGame = 0;
        }
        return;   /* the frame timer is paused while a golden goal resolves */
    }

    if (!s_arcTimerStarted) {
        if (_gameStatePl != ST_GAME_IN_PROGRESS) return;  /* wait for kickoff */
        s_arcTimerStarted = 1;
        s_arcFramesPlayed = 0;
    }
    if (++s_arcFramesPlayed < s_arcLimitFrames) return;

    /* Time up with no golden goal: a level score goes to penalties (if enabled),
     * otherwise the match just ends on the current score. */
    s_arcMatchActive = 0;
    if (_team1TotalGoals == _team2TotalGoals && s_arcPenalties)
        StartPenalties();
    else
        _playGame = 0;
}

void Arcade_GoalScored(void)
{
    if (!_gArcadeMode || !s_arcMatchActive) return;
    if (s_arcGoldenGoal) {
        /* First goal wins. Flag the match to end once the goal celebration AND the
         * auto-replay (if AUTO REPLAYS is on) have finished — see Arcade_MatchTick.
         * Don't end on a fixed delay here: that raced the replay and exited first. */
        s_arcGoalPendingEnd = 1;
    }
}

/* Who won the match that just finished? 1 = top (P1/champion), 2 = bottom (the
 * opponent), 0 = draw. Reads the final goal counters (penalty goals break a draw
 * decided by a shootout). The arcade champion always occupies the top/team-1 slot. */
static int arc_match_winner(void)
{
    extern uint16_t _team1TotalGoals, _team2TotalGoals;
    extern uint16_t _team1PenaltyGoals, _team2PenaltyGoals;
    if (_team1TotalGoals != _team2TotalGoals)
        return _team1TotalGoals > _team2TotalGoals ? 1 : 2;
    if (_team1PenaltyGoals != _team2PenaltyGoals)
        return _team1PenaltyGoals > _team2PenaltyGoals ? 1 : 2;
    return 0;
}

/* Begin the requested arcade game: country (flag) select -> pitch select -> a
 * winner-stays-on run of matches (mirrors PlayFriendly per match). The champion
 * (top/team-1 slot, human) keeps playing fresh opponents; SCORE = consecutive wins,
 * TIME = total in-game time over the run. The run ends on the champion's first
 * non-win (loss or lost shootout) -> GAME OVER -> high-score name entry. */
static void arc_start_game(void)
{
    extern uint16_t _gameType;
    /* Globals the friendly menu normally initialises before InitAndPlayGame. */
    extern uint16_t _friendlyPitchType;   /* pitch CONDITION 0..6 (Part 4 replaces the field) */
    extern uint16_t _pitchTypeOrSeason;   /* 0 = fixed pitch type, not seasonal */
    extern uint16_t _maxSubstitutes;
    extern uint16_t _minSubstitutes;
    extern uint16_t _gameLength;          /* clock-speed index (0..3); 3 = slowest */
    int numPlayers = (s_startGame >= 2) ? 2 : 1;
    int p1 = 0, p2 = 0;
    s_startGame = 0;   /* consume the trigger now: during the run the modal screens
                        * test s_startGame, so it must read 0 until a NEW request. */

    /* Cut to black before swapping music + entering country select, so leaving the
     * attract reel isn't an abrupt jump / frozen frame. s_arcadeFade=0 makes
     * Arcade_RenderFullScreen output opaque black regardless of buffer contents;
     * the selectors fade back in from there. Also kill the attract logo overlay
     * (s_overlayActive is set by Arcade_DrawAttractOverlay and never otherwise
     * cleared, so the logo + stale pitch-menu scratch in s_overlayBuf would draw
     * over the selectors/match). The attract loop re-enables it when we return. */
    s_overlayActive = 0;
    s_arcadeFade = 0;
    s_flagScreenActive = 1;
    WaitForVSync();                         /* show the black frame */
    arc_music_menu();                       /* HERO -> MENU (behind the black screen) */
    arc_flag_select(numPlayers, &p1, &p2);
    if (numPlayers < 2) {                    /* 1P: opponent isn't user-picked -> random CPU */
        do { p2 = (int)(arc_rand() % FLAG_COUNT); } while (p2 == p1);
    }
    int pitch = arc_pitch_select();         /* 0..6 -> SWCPICH1..7 (RANDOM already resolved) */

    /* Selectors done (each faded out to black on exit); make sure no arcade frame
     * composites over the match. */
    s_overlayActive = 0;
    s_flagScreenActive = 0;

    /* Match-engine globals the friendly menu normally sets up — fixed for the whole
     * run (mirrors GoGetTeamsForPlay + PlayFriendly, minus the team-select UI). */
    _gameType = 0;                          /* non-career friendly path */
    _pitchTypeOrSeason = 0;                 /* fixed pitch condition, not seasonal */
    _friendlyPitchType = 4;                 /* a normal pitch (themed pitch overrides at dice time) */
    _maxSubstitutes = 0;                    /* arcade: no subs */
    _minSubstitutes = 0;
    _gameLength = 3;                        /* slowest match clock: engine half/full-time
                                             * never fire within the arcade frame limit */

    /* The run. champFlag holds the seat (top/team-1 slot, human, joystick 1); the
     * opponent slot is replaced each match. wins = streak, totalSecs = total play time.
     *   1P: champion vs a CPU opponent, winner-stays-on — beat the CPU and keep going
     *       on joystick 1; the run ends on the champion's first non-win.
     *   2P: a single head-to-head match (champion = joy1, opponent = joy2); the winner
     *       posts a score (wins = 1). (Mid-streak P2-challenges-P1 cabinet flow is a
     *       larger feature — TODO; this keeps each player on their own stick.) */
    int      champFlag = p1, oppFlag = p2;
    int      runFlag = champFlag;           /* country recorded on the high-score table */
    uint32_t wins = 0, totalSecs = 0;
    int      aborted = 0;

    /* The whole run (and the run-over screens) is modal: coins still accumulate for
     * the NEXT game, but start/service can't interrupt a run in progress — a free
     * mid-streak continuation must not consume a credit, and name entry must finish. */
    s_arcModalRunOver = 1;

    for (;;) {
        uint8_t oppCtrl = (numPlayers >= 2) ? ARC_CTRL_HUMAN : ARC_CTRL_COMPUTER;
        _numSelectedTeams = 0;
        int ok = (arc_load_team(champFlag, 0, ARC_CTRL_HUMAN) == 0);
        ok = ok && (arc_load_team(oppFlag, 1, oppCtrl) == 0);
        if (!ok) {
            fprintf(stderr, "[arcade] team load failed; ending run\n");
            aborted = 1;
            break;
        }

        _numSelectedTeams = 2;
        /* Arcade skipped the front-end, so the menu sprite graphics (the little
         * player figures the stadium/line-up screen draws, sprites 210-213, etc.)
         * were never extracted from menus.raw. Run the menu-BG image script's
         * sprite extraction now — NOT PrepareMenuBackground, whose second step
         * loads the menu BG into the frontbuffer — so the line-up shows real
         * player sprites instead of garbage. */
        ProcessImageCommandStream(_menuBGImageCmdStream, _menuImageStreamBaseEnd);
        InitTeams();
        /* Clear both display buffers to black before StopMusic. StopMusic fades the
         * menu music over ~2.5s while presenting frames, but the RGBA path is now off
         * (pitch select faded to black + cleared s_flagScreenActive), so those frames
         * would show the STALE 8bpp frontbuffer — the attract reel's last pitch frame —
         * mapped through the menu palette (grey garble) instead of the pitch palette.
         * Blanking both buffers keeps the transition black until the stadium draws. */
        { size_t fbn = (size_t)_screen_width * (size_t)SCREEN_HEIGHT;
          if (_frontbufferBasePtr)    memset((void *)_frontbufferBasePtr, 0, fbn);
          if (_frontbufferBaseAltPtr) memset((void *)_frontbufferBaseAltPtr, 0, fbn); }
        StopMusic();                        /* silence menu music before kickoff */
        arc_begin_match_rules();            /* arm golden-goal / time-limit / penalties */
        arc_load_pitch(pitch);              /* chosen pitch -> s_pitchSheet / s_pitchPal */
        _gArcadePitchActive = 1;            /* themed pitch (DOS pipeline) for this match */
        s_arcInMatch = 1;                   /* suppress the attract overlay during goal replays */
        InitAndPlayGame(&_selectedTeams[0], &_selectedTeams[1]);
        s_arcInMatch = 0;
        _gArcadePitchActive = 0;

        if (_quitToDos) { aborted = 1; break; }

        totalSecs += (uint32_t)(s_arcFramesPlayed / ARC_FRAMES_PER_SEC);

        int winner = arc_match_winner();
        if (s_arcDebugEndRun) {              /* DEBUG (F10): end the run with a RANDOM score so the
                                             * name-entry screen can be tested with varied values */
            s_arcDebugEndRun = 0;
            wins      = 1 + (arc_rand() % 20);          /* 1..20 wins */
            totalSecs = 30 + (arc_rand() % (15 * 60));  /* ~0:30 .. 15:00 */
            s_arcDebugForceNameEntry = 1;    /* always show name entry (force last place if needed) */
            break;
        }
        if (numPlayers >= 2) {               /* 2P: one head-to-head match, then done */
            wins = (winner != 0) ? 1u : 0u;
            runFlag = (winner == 2) ? oppFlag : champFlag;   /* record the winner's country */
            break;
        }
        if (winner == 1) {                   /* 1P champion wins -> stays on, free continue */
            wins++;
            do { oppFlag = (int)(arc_rand() % FLAG_COUNT); } while (oppFlag == champFlag);
            pitch = (int)(arc_rand() % 7);   /* a fresh themed pitch each round */
            arc_continue_screen(wins);       /* "WINNER / STREAK N", skippable */
            continue;
        }
        break;                               /* 1P champion lost or drew -> run over */
    }

    /* No arcade frame should composite over the run-over screens. */
    s_overlayActive = 0;
    s_flagScreenActive = 0;

    if (!aborted)
        arc_run_over(g_arcadeOpts ? g_arcadeOpts->arcade_mode : 0, wins, totalSecs, runFlag);

    s_arcModalRunOver = 0;                   /* attract input is live again */
    arc_music_attract();                    /* resume HERO for the attract loop */
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
    const int NUM = 7;

    int sel = 0, u=0, d=0, l=0, r=0, f=1;   /* f=1: ignore a fire still held on entry */
    uint32_t frame = 0;
    int cx = _screen_width / 2;
    s_flagScreenActive = 1;
    s_arcadeFade = 256;                     /* full-bright; the menu doesn't fade in
                                             * (else a leftover fade=0 renders it black) */

    while (_quitToDos == 0) {
        Player1StatusProc();
        int sv = 0; Platform_ArcadeInput(NULL, NULL, &sv, NULL, NULL);

        if (_pl1Up && !u)   sel = (sel - 1 + NUM) % NUM;
        if (_pl1Down && !d) sel = (sel + 1) % NUM;
        int adj = ((_pl1Right && !r) ? 1 : 0) - ((_pl1Left && !l) ? 1 : 0);
        if (adj) {
            if (sel == 0) {
                int mc = Arcade_ModeCount();
                g_arcadeOpts->arcade_mode = (g_arcadeOpts->arcade_mode + adj + mc) % mc;
            } else if (sel == 1) {
                int idx = 1;
                for (int i = 0; i < NT; i++) if (times[i] == g_arcadeOpts->arcade_game_seconds) idx = i;
                g_arcadeOpts->arcade_game_seconds = times[(idx + adj + NT) % NT];
            } else if (sel == 2) g_arcadeOpts->arcade_golden_goal = !g_arcadeOpts->arcade_golden_goal;
            else if (sel == 3)   g_arcadeOpts->arcade_penalties   = !g_arcadeOpts->arcade_penalties;
            else if (sel == 4)   g_arcadeOpts->arcade_goal_replay = !g_arcadeOpts->arcade_goal_replay;
            else if (sel == 5)   g_arcadeOpts->arcade_attract_audio = !g_arcadeOpts->arcade_attract_audio;
            else                 g_arcadeOpts->arcade_gameplay = !g_arcadeOpts->arcade_gameplay;
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
        const char *title = "SERVICE MENU";
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
            const char *label = (i == 0) ? "MODE" : (i == 1) ? "GAME TIME"
                              : (i == 2) ? "GOLDEN GOAL" : (i == 3) ? "PENALTIES ON DRAW"
                              : (i == 4) ? "GOAL REPLAY" : (i == 5) ? "ATTRACT AUDIO" : "GAMEPLAY";
            if      (i == 0) snprintf(val, sizeof val, "%s", Arcade_ModeName(g_arcadeOpts->arcade_mode));
            else if (i == 1) snprintf(val, sizeof val, "%d SEC", g_arcadeOpts->arcade_game_seconds);
            else if (i == 2) snprintf(val, sizeof val, "%s", g_arcadeOpts->arcade_golden_goal ? "ON" : "OFF");
            else if (i == 3) snprintf(val, sizeof val, "%s", g_arcadeOpts->arcade_penalties   ? "ON" : "OFF");
            else if (i == 4) snprintf(val, sizeof val, "%s", g_arcadeOpts->arcade_goal_replay ? "ON" : "OFF");
            else if (i == 5) snprintf(val, sizeof val, "%s", g_arcadeOpts->arcade_attract_audio ? "SFX" : "MUSIC");
            else             snprintf(val, sizeof val, "%s", g_arcadeOpts->arcade_gameplay ? "DOS" : "AMIGA");
            uint32_t tc = YEL;
            /* Keep the rows within the title box (boxW, computed above) — never
             * wider. Label left-aligned at the box's left; value left-aligned in a
             * reserved right column (~90px, wide enough for "GOLDEN GOAL"). */
            int bl = cx - boxW / 2, br = cx + boxW / 2;
            if (i == sel) {
                arc_fill_rect(bl, ly - 2, boxW, 16, WHITE);
                tc = selCol;
            }
            arc_svc_text(bl + 12, ly, tc, label, TROGF7_glyphs, TROGF7_COUNT);
            arc_svc_text(br - 90, ly, tc, val,   TROGF7_glyphs, TROGF7_COUNT);
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

/* ====================================================================
 * Phase 3c (part 1) — pitch select (themed arcade pitches + RANDOM)
 * ==================================================================== */
#define PITCH_OPTIONS 8   /* BEACH..STADIUM (SWCPICH1..7) + RANDOM (SWCPICHR) */
static const char *const s_pitchNames[PITCH_OPTIONS] = {
    "BEACH", "PARK", "YARD", "SKATE PARK", "PRISON", "CITY", "STADIUM", "RANDOM"
};
static uint8_t  s_pitchSheet[672 * 880];
static uint32_t s_pitchPal[256];
static int      s_pitchW, s_pitchH;

static int arc_load_pitch(int idx)   /* 0..6 = SWCPICH1..7, 7 = SWCPICHR */
{
    char path[64];
    if (idx >= PITCH_OPTIONS - 1) snprintf(path, sizeof path, "grafs_arcade/SWCPICHR.bmp");
    else                          snprintf(path, sizeof path, "grafs_arcade/SWCPICH%d.bmp", idx + 1);
    return Platform_LoadIndexedBMP(path, s_pitchSheet, (int)sizeof s_pitchSheet,
                                   s_pitchPal, &s_pitchW, &s_pitchH);
}

/* Dice the loaded arcade pitch (s_pitchSheet, 672x880 chunky 8bpp) into the DOS
 * chunky tile bank: 42x55 = 2310 row-major 16x16 tiles, identity-mapped — exactly
 * what dos_load_pitch produces from PITCH*.BLK, so DrawChunkyPitch + the DOS pitch
 * pipeline render it. Called from LoadPitchMap when _gArcadePitchActive. */
void Arcade_DicePitchTiles(void)
{
    extern uint8_t  _pitchChunkyBuffer[];
    extern uint8_t *_s_pitchFullPtrs[];
    extern uint8_t  _pitchDatBuffer[];
    enum { COLS = 42, ROWS = 55, TILE = 16, SRCW = COLS * TILE /*672*/ };

    if (s_pitchW < SRCW || s_pitchH < ROWS * TILE) {
        fprintf(stderr, "[arcade] pitch BMP %dx%d, expected %dx%d — skipping dice\n",
                s_pitchW, s_pitchH, SRCW, ROWS * TILE);
        return;
    }
    for (int gridY = 0; gridY < ROWS; gridY++) {
        for (int gridX = 0; gridX < COLS; gridX++) {
            int slot = gridY * COLS + gridX;
            uint8_t *tile = _pitchChunkyBuffer + (size_t)slot * (TILE * TILE);
            for (int row = 0; row < TILE; row++)
                memcpy(tile + row * TILE,
                       s_pitchSheet + (size_t)(gridY * TILE + row) * SRCW + gridX * TILE,
                       TILE);
            _s_pitchFullPtrs[slot] = tile;
        }
    }
    /* Mirror each slot's tile pointer into pitchDatBuffer (the animated-pattern /
     * surround code reads these), same as dos_load_pitch. */
    uint32_t *slots = (uint32_t *)_pitchDatBuffer;
    for (int i = 0; i < COLS * ROWS; i++)
        slots[i] = (uint32_t)(uintptr_t)_s_pitchFullPtrs[i];
}

/* The active arcade pitch's 256-entry palette (0xAARRGGBB), used by the palette
 * hook to fill pitch slots 16..255. Slots 0..15 hold the DOS sprite palette. */
const uint32_t *Arcade_PitchPaletteArgb(void) { return s_pitchPal; }

/* Nearest-neighbour scale the loaded pitch into the preview rect (RGBA). */
static void arc_pitch_preview(int px, int py, int pw, int ph)
{
    if (s_pitchW <= 0 || s_pitchH <= 0) return;
    for (int y = 0; y < ph; y++) {
        int dy = py + y;
        if (dy < 0 || dy >= SCREEN_HEIGHT) continue;
        const uint8_t *src = s_pitchSheet + (size_t)(y * s_pitchH / ph) * s_pitchW;
        uint32_t      *out = s_flagScreenRGBA + (size_t)dy * _screen_width;
        for (int x = 0; x < pw; x++) {
            int dx = px + x;
            if (dx >= 0 && dx < _screen_width) out[dx] = s_pitchPal[src[x * s_pitchW / pw]];
        }
    }
}

/* Themed-pitch selector: a column of buttons (green; RANDOM in red) with the
 * selected pitch previewed on the right. Joystick up/down, fire confirms.
 * Returns 0..6 (SWCPICH1..7); RANDOM resolves to a random 0..6. */
static int arc_pitch_select(void)
{
    extern uint8_t _pl1Up, _pl1Down, _pl1ShortFire;
    extern void Player1StatusProc(void);
    extern void DrawMenuItemBackground(uint16_t color, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    extern void DrawMenuItemOuterFrame(uint16_t color, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    extern void DrawMenuItemInnerFrame(uint16_t color, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    extern uint16_t _kColorTableShine[];
    extern int      _menu_draw_x_offset;
    extern uint16_t _cameraXMod320;
    extern uint16_t _menuPalette[];

    /* menu palette -> ARGB (the buttons/text composite through this) */
    uint32_t mpal[16];
    for (int i = 0; i < 16; i++) {
        uint16_t v = _menuPalette[i];
        uint32_t r = (v >> 8) & 0xF, g = (v >> 4) & 0xF, b = v & 0xF;
        r = (r << 4) | r; g = (g << 4) | g; b = (b << 4) | b;
        mpal[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }

    int sel = 0, u = 0, d = 0, f = 1, loaded = -1;
    uint32_t frame = 0;
    /* Lay out within the central 336-wide 4:3 region. Buttons/text draw via the
     * original menu routines into s_overlayBuf (menu-palette indices) with
     * _menu_draw_x_offset = WIDE_MARGIN, then composite through the menu palette. */
    int bw = 152, bh = 24, gap = 2, by0 = 50, bx336 = 8;
    int pvw = 156, pvh = pvw * 880 / 672, pvx336 = 168, pvy = 38;
    int pvx = WIDE_MARGIN + pvx336;
    int labelCx = bx336 + bw / 2;

    s_flagScreenActive = 1;
    s_arcadeFade = 0;                       /* fade in from black */
    while (_quitToDos == 0) {
        frame++;
        Player1StatusProc();
        if (_pl1Up && !u)   sel = (sel - 1 + PITCH_OPTIONS) % PITCH_OPTIONS;
        if (_pl1Down && !d) sel = (sel + 1) % PITCH_OPTIONS;
        u = _pl1Up; d = _pl1Down;
        int pick = (_pl1ShortFire && !f); f = _pl1ShortFire;
        if (loaded != sel) { arc_load_pitch(sel); loaded = sel; }
        if (pick) break;

        arc_fill_menubg();

        /* buttons + flashing selector + labels via the original menu drawing code */
        size_t total = (size_t)_screen_width * SCREEN_HEIGHT;
        size_t n = total > sizeof s_overlayBuf ? sizeof s_overlayBuf : total;
        memset(s_overlayBuf, 0, n);
        uintptr_t savedFb  = _frontbufferBasePtr;
        int       savedOff = _menu_draw_x_offset;
        uint16_t  savedCam = _cameraXMod320;
        _frontbufferBasePtr = (uintptr_t)s_overlayBuf;
        _menu_draw_x_offset = WIDE_MARGIN;
        _cameraXMod320 = 0;

        for (int i = 0; i < PITCH_OPTIONS; i++) {
            int yy = by0 + i * (bh + gap);
            uint16_t col = (i == PITCH_OPTIONS - 1) ? 10 : 14;  /* RED_10 / YELLOW_GREEN_BLACK_14 */
            DrawMenuItemBackground(col, (uint16_t)bx336, (uint16_t)yy, (uint16_t)bw, (uint16_t)bh);
            DrawMenuItemOuterFrame(col, (uint16_t)bx336, (uint16_t)yy, (uint16_t)bw, (uint16_t)bh);
            DrawMenuTextCentered((uint16_t)labelCx, (uint16_t)(yy + 7), 2,
                                 (uint8_t *)s_pitchNames[i], ARC_CHARS);
        }
        { int yy = by0 + sel * (bh + gap);
          uint16_t shine = _kColorTableShine[(frame & 14) >> 1];   /* menu selector flash */
          DrawMenuItemInnerFrame(shine, (uint16_t)bx336, (uint16_t)yy, (uint16_t)bw, (uint16_t)bh);
          DrawMenuItemInnerFrame(shine, (uint16_t)(bx336+1), (uint16_t)(yy+1),
                                 (uint16_t)(bw-2), (uint16_t)(bh-2)); }
        DrawMenuTextCentered((uint16_t)labelCx, 20, 2, (uint8_t *)"SELECT PITCH", ARC_CHARS);

        _frontbufferBasePtr = savedFb;
        _menu_draw_x_offset = savedOff;
        _cameraXMod320 = savedCam;

        for (size_t i = 0; i < n; i++)
            if (s_overlayBuf[i]) s_flagScreenRGBA[i] = mpal[s_overlayBuf[i] & 15];

        /* the pitch preview (its own palette, drawn directly in RGBA) */
        arc_pitch_preview(pvx, pvy, pvw, pvh);
        arc_flag_rect(pvx - 1, pvy - 1, pvw + 2, pvh + 2, 0xFFFFFFFFu);

        if (s_arcadeFade < 256) { s_arcadeFade += 24; if (s_arcadeFade > 256) s_arcadeFade = 256; }
        WaitForVSync();
        ReadTimerDelta();
    }
    arc_fade_ramp(0, 10);                   /* fade out to black before kickoff */
    s_flagScreenActive = 0;

    if (sel >= PITCH_OPTIONS - 1) sel = (int)(arc_rand() % 7);   /* RANDOM -> 0..6 */
    return sel;
}

/* ====================================================================
 * Phase 5 — arcade high-score tables, name entry, and the attract display
 *
 * SCORE = consecutive wins of a winner-stays-on run; TIME = total in-game time
 * over the run. One table per game mode, persisted to ARCADE.HI (next to SWOS.ini).
 * Every screen uses the in-game MENU font (NOT the service font) drawn through the
 * menu palette over the menu background — the same compositing the flag / pitch
 * selectors use. SDL-only.
 * ==================================================================== */
extern int      _menu_draw_x_offset;
extern uint16_t _cameraXMod320;
extern uint16_t _menuPalette[];

#define ARC_HS_ENTRIES   10        /* rows per table */
#define ARC_HS_NAME_MAX  3         /* initials */
#define ARC_HS_MAX_MODES 8         /* upper bound on Arcade_ModeCount() */

typedef struct {
    char     name[ARC_HS_NAME_MAX + 1];   /* initials, uppercase; ' ' allowed */
    uint32_t score;                        /* wins (the streak length) */
    uint32_t timeSecs;                     /* total in-game time over the run */
    uint8_t  flag;                         /* FLAGS.bmp cell (country played) 0..47; 0xFF = none */
} ArcHiEntry;

static ArcHiEntry s_hiTable[ARC_HS_MAX_MODES][ARC_HS_ENTRIES];
static int        s_hiLoaded;

/* ARCADE.HI lives next to SWOS.ini (g_arcadeIniPath's directory) when known, else
 * in the working directory. */
static const char *arc_hs_path(void)
{
    static char path[256];
    if (g_arcadeIniPath && g_arcadeIniPath[0]) {
        const char *slash = strrchr(g_arcadeIniPath, '/');
        const char *bs    = strrchr(g_arcadeIniPath, '\\');
        if (bs && (!slash || bs > slash)) slash = bs;
        if (slash) {
            size_t n = (size_t)(slash - g_arcadeIniPath) + 1;
            if (n > sizeof path - 16) n = sizeof path - 16;
            memcpy(path, g_arcadeIniPath, n);
            snprintf(path + n, sizeof path - n, "ARCADE.HI");
            return path;
        }
    }
    snprintf(path, sizeof path, "ARCADE.HI");
    return path;
}

/* Seed every table with descending placeholder rows so a fresh cabinet still has
 * something to show in attract. */
static void arc_hs_seed_defaults(void)
{
    static const struct { const char *nm; uint32_t sc, t; uint8_t fl; } seed[ARC_HS_ENTRIES] = {
        {"SWS",10,540,29}, {"BEN",8,420,4},  {"AMI",6,330,26}, {"DOS",5,260,2}, {"ARC",4,210,27},
        {"ACE",3,170,25},  {"PRO",3,150,28}, {"FAB",2,110,5},  {"NEW",1,60,23}, {"AAA",1,45,0},
    };
    for (int m = 0; m < ARC_HS_MAX_MODES; m++)
        for (int i = 0; i < ARC_HS_ENTRIES; i++) {
            snprintf(s_hiTable[m][i].name, sizeof s_hiTable[m][i].name, "%s", seed[i].nm);
            s_hiTable[m][i].score    = seed[i].sc;
            s_hiTable[m][i].timeSecs = seed[i].t;
            s_hiTable[m][i].flag     = seed[i].fl;
        }
}

static void arc_hs_load(void)
{
    if (s_hiLoaded) return;
    s_hiLoaded = 1;
    arc_hs_seed_defaults();

    FILE *f = fopen(arc_hs_path(), "r");
    if (!f) {
        fprintf(stderr, "[arcade] no high-score file (%s) — using defaults\n", arc_hs_path());
        return;
    }
    char line[160];
    int mode = -1, row = 0;
    while (fgets(line, sizeof line, f)) {
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '#' || *s == '\r' || *s == '\n' || *s == 0) continue;
        if (*s == '[') {                       /* [MODE NAME] section header */
            char *end = strchr(s, ']');
            if (!end) continue;
            *end = 0;
            mode = Arcade_ModeFromName(s + 1);
            row  = 0;
            continue;
        }
        if (mode < 0 || mode >= ARC_HS_MAX_MODES || row >= ARC_HS_ENTRIES) continue;
        char nm[32]; unsigned sc = 0, t = 0, fl = 0xFF;
        int got = sscanf(s, "%31s %u %u %u", nm, &sc, &t, &fl);   /* flag field is optional */
        if (got >= 1) {
            if (got < 4) fl = 0xFF;
            for (char *p = nm; *p; p++) if (*p == '_') *p = ' ';
            nm[ARC_HS_NAME_MAX] = 0;                          /* keep the first ARC_HS_NAME_MAX chars */
            memcpy(s_hiTable[mode][row].name, nm, ARC_HS_NAME_MAX + 1);
            s_hiTable[mode][row].score    = sc;
            s_hiTable[mode][row].timeSecs = t;
            s_hiTable[mode][row].flag     = (uint8_t)fl;
            row++;
        }
    }
    fclose(f);
    fprintf(stderr, "[arcade] high scores loaded from %s\n", arc_hs_path());
}

static void arc_hs_save(void)
{
    FILE *f = fopen(arc_hs_path(), "w");
    if (!f) {
        fprintf(stderr, "[arcade] cannot write high-score file %s\n", arc_hs_path());
        return;
    }
    fprintf(f, "# OpenSWOS Arcade high scores\n");
    fprintf(f, "# <name> <wins> <time-seconds> <flag>   ('_' = space in name; flag = FLAGS cell, 255 = none)\n");
    int modes = Arcade_ModeCount();
    for (int m = 0; m < modes && m < ARC_HS_MAX_MODES; m++) {
        fprintf(f, "[%s]\n", Arcade_ModeName(m));
        for (int i = 0; i < ARC_HS_ENTRIES; i++) {
            char nm[ARC_HS_NAME_MAX + 1];
            memcpy(nm, s_hiTable[m][i].name, sizeof nm);   /* both fixed at 4 bytes */
            nm[ARC_HS_NAME_MAX] = 0;
            if (!nm[0]) { nm[0] = 'A'; nm[1] = 'A'; nm[2] = 'A'; nm[3] = 0; }
            for (char *p = nm; *p; p++) if (*p == ' ') *p = '_';
            fprintf(f, "%s %u %u %u\n", nm, s_hiTable[m][i].score, s_hiTable[m][i].timeSecs,
                    (unsigned)s_hiTable[m][i].flag);
        }
    }
    fclose(f);
    fprintf(stderr, "[arcade] high scores saved to %s\n", arc_hs_path());
}

/* Rank a (score,timeSecs) would take in mode's table: 0 = best .. ARC_HS_ENTRIES-1;
 * -1 if it doesn't make the table. Higher score wins; a faster run breaks a tie.
 * A 0-win run never qualifies. */
static int arc_hs_rank(int mode, uint32_t score, uint32_t timeSecs)
{
    if (mode < 0 || mode >= ARC_HS_MAX_MODES || score == 0) return -1;
    for (int i = 0; i < ARC_HS_ENTRIES; i++) {
        const ArcHiEntry *e = &s_hiTable[mode][i];
        if (score > e->score) return i;
        if (score == e->score && timeSecs < e->timeSecs) return i;
    }
    return -1;
}

static void arc_hs_insert(int mode, int row, const char *name, uint32_t score,
                          uint32_t timeSecs, uint8_t flag)
{
    if (mode < 0 || mode >= ARC_HS_MAX_MODES || row < 0 || row >= ARC_HS_ENTRIES) return;
    for (int i = ARC_HS_ENTRIES - 1; i > row; i--)
        s_hiTable[mode][i] = s_hiTable[mode][i - 1];
    ArcHiEntry *e = &s_hiTable[mode][row];
    snprintf(e->name, sizeof e->name, "%s", name);
    e->score    = score;
    e->timeSecs = timeSecs;
    e->flag     = flag;
}

/* ---- shared rendering helpers (menu palette over the menu background) ---- */
static void arc_build_menu_argb(uint32_t pal[16])
{
    for (int i = 0; i < 16; i++) {
        uint16_t v = _menuPalette[i];
        uint32_t r = (v >> 8) & 0xF, g = (v >> 4) & 0xF, b = v & 0xF;
        r = (r << 4) | r; g = (g << 4) | g; b = (b << 4) | b;
        pal[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
}

static void arc_composite_menu_overlay(const uint32_t pal[16])
{
    size_t total = (size_t)_screen_width * SCREEN_HEIGHT;
    size_t n = total > sizeof s_overlayBuf ? sizeof s_overlayBuf : total;
    for (size_t i = 0; i < n; i++)
        if (s_overlayBuf[i]) s_flagScreenRGBA[i] = pal[s_overlayBuf[i] & 15];
}


/* INSERT COIN / CREDIT-PUSH-START prompt into the current menu-draw target (the
 * caller has redirected _frontbufferBasePtr to s_overlayBuf, offset = WIDE_MARGIN). */
static void arc_draw_bottom_prompt(uint32_t frame)
{
    int flash = (int)((frame >> 4) & 1);
    if (s_credits <= 0) {
        if (flash)
            DrawMenuTextCentered(168, ARC_Y_BOTTOM, ARC_WHITE, (uint8_t *)"INSERT COIN", ARC_CHARS);
    } else {
        char buf[24];
        snprintf(buf, sizeof buf, "CREDIT %02d", s_credits);
        DrawMenuTextCentered(168, ARC_Y_BOTTOM, ARC_WHITE, (uint8_t *)buf, ARC_CHARS);
        if (flash)
            DrawMenuTextCentered(168, ARC_Y_PROMPT, ARC_WHITE, (uint8_t *)"PUSH START BUTTON", ARC_CHARS);
    }
}

/* ---- high-score table geometry (336-space X, screen-space Y) ---- */
#define HS_TITLE_Y   14
#define HS_SUB_Y     30
#define HS_HEAD_Y    48
#define HS_ROW0_Y    62
#define HS_ROW_DY    15
#define HS_X_RANK    36
#define HS_X_FLAG    74      /* country flag (24px sprite) — 336-space left edge */
#define HS_X_NAME    104
#define HS_X_SCORE   190
#define HS_X_TIME_R  302     /* right edge for the right-aligned TIME column */
#define HS_HILITE_X  30
#define HS_HILITE_W  282     /* 30..312 */

static void arc_draw_table_text(int mode)
{
    DrawMenuTextCentered(168, HS_TITLE_Y, HS_COL_TITLE, (uint8_t *)Arcade_ModeName(mode), ARC_CHARS);
    DrawMenuTextCentered(168, HS_SUB_Y,   HS_COL_TITLE, (uint8_t *)"HIGH SCORES", ARC_CHARS);
    DrawMenuText(HS_X_RANK,  HS_HEAD_Y, HS_COL_HEADER, (uint8_t *)"RANK", ARC_CHARS);
    DrawMenuText(HS_X_NAME,  HS_HEAD_Y, HS_COL_HEADER, (uint8_t *)"NAME", ARC_CHARS);
    DrawMenuText(HS_X_SCORE, HS_HEAD_Y, HS_COL_HEADER, (uint8_t *)"WINS", ARC_CHARS);
    DrawMenuTextRightAligned(HS_X_TIME_R, HS_HEAD_Y, HS_COL_HEADER, (uint8_t *)"TIME", ARC_CHARS);
    for (int i = 0; i < ARC_HS_ENTRIES; i++) {
        const ArcHiEntry *e = &s_hiTable[mode][i];
        int y = HS_ROW0_Y + i * HS_ROW_DY;
        uint16_t col = (i == 0) ? HS_COL_TOP : HS_COL_ROW;   /* #1 row red, the rest white */
        char b[24];
        snprintf(b, sizeof b, "%2d", i + 1);
        DrawMenuText(HS_X_RANK, y, col, (uint8_t *)b, ARC_CHARS);
        DrawMenuText(HS_X_NAME, y, col, (uint8_t *)(e->name[0] ? e->name : "---"), ARC_CHARS);
        snprintf(b, sizeof b, "%u", e->score);
        DrawMenuText(HS_X_SCORE, y, col, (uint8_t *)b, ARC_CHARS);
        snprintf(b, sizeof b, "%u:%02u", e->timeSecs / 60, e->timeSecs % 60);
        DrawMenuTextRightAligned(HS_X_TIME_R, y, col, (uint8_t *)b, ARC_CHARS);
    }
}

/* Blit flag `f` (24x16 from the FLAGS sheet) into an RGBA target at screen (px,py),
 * scaled to 50% (12x8, nearest-neighbour), blended onto whatever is there by `alpha`. */
static void arc_blit_flag_blend(int f, int px, int py, uint32_t *dst, int pitch, int alpha)
{
    if (!s_flagsLoaded || f < 0 || f >= FLAG_COUNT || alpha <= 0) return;
    int sx = (f % FLAG_COLS) * FLAG_W, sy = (f / FLAG_COLS) * FLAG_H;
    int dw = FLAG_W / 2, dh = FLAG_H / 2;            /* 12 x 8 */
    for (int y = 0; y < dh; y++) {
        int dy = py + y;
        if (dy < 0 || dy >= SCREEN_HEIGHT) continue;
        const uint8_t *src = s_flagSheet + (size_t)(sy + y * 2) * s_flagSheetW + sx;
        uint32_t      *out = dst + (size_t)dy * pitch;
        for (int x = 0; x < dw; x++) {
            int dx = px + x;
            if (dx < 0 || dx >= _screen_width) continue;
            uint32_t c = s_flagPal[src[x * 2]];
            out[dx] = (alpha >= 255) ? c : arc_blend(out[dx], c, alpha);
        }
    }
}

/* Draw each high-score entry's country flag next to its row, into an RGBA target,
 * faded by `alpha`. Shared by the run-over result table and the attract overlay. */
static void arc_draw_table_flags(int mode, uint32_t *dst, int pitch, int alpha)
{
    if (mode < 0 || mode >= ARC_HS_MAX_MODES || alpha <= 0) return;
    for (int i = 0; i < ARC_HS_ENTRIES; i++) {
        uint8_t f = s_hiTable[mode][i].flag;
        if (f >= FLAG_COUNT) continue;                  /* entry has no flag */
        int py = HS_ROW0_Y + i * HS_ROW_DY + 1;         /* centre the 8px flag on the text row */
        arc_blit_flag_blend(f, WIDE_MARGIN + HS_X_FLAG, py, dst, pitch, alpha);
    }
}

/* Render mode's table text (menu font, white+shadow) into s_tableBuf, for the attract
 * foreground cross-fade (Arcade_CompositeOverlay blends it over the live reel). */
static void arc_render_table_to_buf(int mode)
{
    size_t total = (size_t)_screen_width * SCREEN_HEIGHT;
    size_t n = total > sizeof s_tableBuf ? sizeof s_tableBuf : total;
    memset(s_tableBuf, 0, n);
    uintptr_t sFb = _frontbufferBasePtr; int sOff = _menu_draw_x_offset; uint16_t sCam = _cameraXMod320;
    _frontbufferBasePtr = (uintptr_t)s_tableBuf; _menu_draw_x_offset = WIDE_MARGIN; _cameraXMod320 = 0;
    arc_draw_table_text(mode);
    _frontbufferBasePtr = sFb; _menu_draw_x_offset = sOff; _cameraXMod320 = sCam;
}

/* Present mode's high-score table over the menu background, fading in then holding
 * for holdFrames. highlightRow >= 0 draws a flashing bar on that row (the entry
 * just added). Returns 1 if interrupted by a start/service request. */
static int arc_present_table(int mode, int holdFrames, int highlightRow)
{
    uint32_t mpal[16]; arc_build_menu_argb(mpal);
    s_flagScreenActive = 1;
    s_arcadeFade = 0;
    uint32_t frame = 0;
    int held = 0;
    while (_quitToDos == 0) {
        frame++;
        arc_drain_input();
        if (s_startGame || s_serviceReq) { s_flagScreenActive = 0; return 1; }

        arc_fill_menubg();

        size_t total = (size_t)_screen_width * SCREEN_HEIGHT;
        size_t n = total > sizeof s_overlayBuf ? sizeof s_overlayBuf : total;
        memset(s_overlayBuf, 0, n);
        uintptr_t sFb = _frontbufferBasePtr; int sOff = _menu_draw_x_offset; uint16_t sCam = _cameraXMod320;
        _frontbufferBasePtr = (uintptr_t)s_overlayBuf; _menu_draw_x_offset = WIDE_MARGIN; _cameraXMod320 = 0;
        arc_draw_table_text(mode);
        arc_draw_bottom_prompt(frame);
        _frontbufferBasePtr = sFb; _menu_draw_x_offset = sOff; _cameraXMod320 = sCam;
        arc_composite_menu_overlay(mpal);
        arc_draw_table_flags(mode, s_flagScreenRGBA, _screen_width, 255);  /* flags (frame dims via s_arcadeFade) */

        if (highlightRow >= 0 && highlightRow < ARC_HS_ENTRIES) {
            int ry = HS_ROW0_Y + highlightRow * HS_ROW_DY - 3;
            int rx = WIDE_MARGIN + HS_HILITE_X;
            uint32_t c = ((frame >> 3) & 1) ? 0xFFFFFF00u : 0xFF806000u;   /* flashing yellow */
            /* 2px-thick box (matches the name-entry selector) */
            arc_flag_rect(rx,     ry,     HS_HILITE_W,     HS_ROW_DY + 2, c);
            arc_flag_rect(rx + 1, ry + 1, HS_HILITE_W - 2, HS_ROW_DY,     c);
        }

        if (s_arcadeFade < 256) { s_arcadeFade += 16; if (s_arcadeFade > 256) s_arcadeFade = 256; }
        else if (++held >= holdFrames) break;

        WaitForVSync();
        ReadTimerDelta();
    }
    arc_fade_ramp(0, 10);
    s_flagScreenActive = 0;
    return (s_startGame || s_serviceReq);
}

/* A simple up-to-3-line centred message over the menu background (GAME OVER /
 * WINNER, etc.). Fades in, holds; if `skippable`, a fire press ends it early.
 * Returns 1 if interrupted by a start/service request (ignored while modal). */
static int arc_message_screen(const char *l1, const char *l2, const char *l3,
                              int holdFrames, int skippable)
{
    extern uint8_t _pl1ShortFire, _pl2ShortFire;
    extern void Player1StatusProc(void);
    extern void Player2StatusProc(void);
    uint32_t mpal[16]; arc_build_menu_argb(mpal);
    s_flagScreenActive = 1;
    s_arcadeFade = 0;
    uint32_t frame = 0;
    int held = 0, f = 1;
    while (_quitToDos == 0) {
        frame++;
        Player1StatusProc(); Player2StatusProc();
        int fire = _pl1ShortFire || _pl2ShortFire;
        arc_drain_input();
        if (s_startGame || s_serviceReq) { s_flagScreenActive = 0; return 1; }

        arc_fill_menubg();
        size_t total = (size_t)_screen_width * SCREEN_HEIGHT;
        size_t n = total > sizeof s_overlayBuf ? sizeof s_overlayBuf : total;
        memset(s_overlayBuf, 0, n);
        uintptr_t sFb = _frontbufferBasePtr; int sOff = _menu_draw_x_offset; uint16_t sCam = _cameraXMod320;
        _frontbufferBasePtr = (uintptr_t)s_overlayBuf; _menu_draw_x_offset = WIDE_MARGIN; _cameraXMod320 = 0;
        if (l1) DrawMenuTextCentered(168,  92, ARC_WHITE, (uint8_t *)l1, ARC_CHARS);
        if (l2) DrawMenuTextCentered(168, 120, ARC_WHITE, (uint8_t *)l2, ARC_CHARS);
        if (l3) DrawMenuTextCentered(168, 144, ARC_WHITE, (uint8_t *)l3, ARC_CHARS);
        arc_draw_bottom_prompt(frame);
        _frontbufferBasePtr = sFb; _menu_draw_x_offset = sOff; _cameraXMod320 = sCam;
        arc_composite_menu_overlay(mpal);

        if (s_arcadeFade < 256) { s_arcadeFade += 16; if (s_arcadeFade > 256) s_arcadeFade = 256; }
        else {
            held++;
            if (skippable && fire && !f && held > 8) break;
            if (held >= holdFrames) break;
        }
        f = fire;
        WaitForVSync();
        ReadTimerDelta();
    }
    arc_fade_ramp(0, 10);
    s_flagScreenActive = 0;
    return (s_startGame || s_serviceReq);
}

static int arc_continue_screen(uint32_t wins)
{
    char l2[32];
    snprintf(l2, sizeof l2, "WIN STREAK: %u", wins);
    return arc_message_screen("WINNER!", l2, "GET READY...", 110, 1);
}

/* ---- name (initials) entry: 4x10 character grid + DEL / END action row ---- */
#define NAME_COLS     10
#define NAME_CHARROWS 4
#define NAME_ROWS     (NAME_CHARROWS + 1)
/* Only glyphs the menu font actually has (A-Z, 0-9, '.', '-', space — '!' and '_'
 * map to glyph 0 in _bigCharsTable, so they're left out). The trailing blanks are
 * selectable space cells. */
static const char s_nameGrid[NAME_CHARROWS * NAME_COLS + 1] =
    "ABCDEFGHIJ"
    "KLMNOPQRST"
    "UVWXYZ0123"
    "456789.-  ";
#define NAME_CELL_W  28
#define NAME_GW      (NAME_COLS * NAME_CELL_W)   /* 280 */
#define NAME_GX336   ((336 - NAME_GW) / 2)       /* 28 (336-space left edge) */
#define NAME_GY      108
#define NAME_CELL_H  20

/* Either player's stick navigates (the champion may be P1 or P2); fire picks the
 * highlighted cell. Writes up to ARC_HS_NAME_MAX chars into out (NUL-terminated). */
static void arc_name_entry(int rank, char *out)
{
    extern uint8_t _pl1Up,_pl1Down,_pl1Left,_pl1Right,_pl1ShortFire;
    extern uint8_t _pl2Up,_pl2Down,_pl2Left,_pl2Right,_pl2ShortFire;
    extern void Player1StatusProc(void);
    extern void Player2StatusProc(void);

    uint32_t mpal[16]; arc_build_menu_argb(mpal);
    char entered[ARC_HS_NAME_MAX + 1] = {0};
    int  nch = 0, col = 0, row = 0;
    int  u = 0, d = 0, l = 0, r = 0, f = 1;
    uint32_t frame = 0;
    int done = 0;

    out[0] = 0;
    s_flagScreenActive = 1;
    s_arcadeFade = 0;
    while (_quitToDos == 0 && !done) {
        frame++;
        Player1StatusProc(); Player2StatusProc();
        int up = _pl1Up || _pl2Up,   dn = _pl1Down || _pl2Down;
        int lf = _pl1Left || _pl2Left, rt = _pl1Right || _pl2Right;
        int fire = _pl1ShortFire || _pl2ShortFire;

        if (up && !u) row = (row - 1 + NAME_ROWS) % NAME_ROWS;
        if (dn && !d) row = (row + 1) % NAME_ROWS;
        if (lf && !l) col = (col - 1 + NAME_COLS) % NAME_COLS;
        if (rt && !r) col = (col + 1) % NAME_COLS;
        int pick = (fire && !f);
        u = up; d = dn; l = lf; r = rt; f = fire;

        if (pick) {
            if (row < NAME_CHARROWS) {
                char c = s_nameGrid[row * NAME_COLS + col];
                if (nch < ARC_HS_NAME_MAX) { entered[nch++] = c; entered[nch] = 0; }
                if (nch >= ARC_HS_NAME_MAX) done = 1;
            } else if (col < NAME_COLS / 2) {        /* DEL */
                if (nch > 0) entered[--nch] = 0;
            } else {                                 /* END */
                done = 1;
            }
        }

        arc_drain_input();   /* accumulate coins; modal flag blocks start/service */

        /* ---- draw ---- */
        arc_fill_menubg();
        size_t total = (size_t)_screen_width * SCREEN_HEIGHT;
        size_t n = total > sizeof s_overlayBuf ? sizeof s_overlayBuf : total;
        memset(s_overlayBuf, 0, n);
        uintptr_t sFb = _frontbufferBasePtr; int sOff = _menu_draw_x_offset; uint16_t sCam = _cameraXMod320;
        _frontbufferBasePtr = (uintptr_t)s_overlayBuf; _menu_draw_x_offset = WIDE_MARGIN; _cameraXMod320 = 0;

        DrawMenuTextCentered(168, 22, ARC_WHITE, (uint8_t *)"NEW HIGH SCORE!", ARC_CHARS);
        { char hh[40]; snprintf(hh, sizeof hh, "RANK %d - ENTER INITIALS", rank + 1);
          DrawMenuTextCentered(168, 42, ARC_WHITE, (uint8_t *)hh, ARC_CHARS); }
        { char shown[ARC_HS_NAME_MAX * 2 + 1]; int k = 0;
          for (int i = 0; i < ARC_HS_NAME_MAX; i++) {
              char c = (i < nch) ? entered[i] : '.';   /* '.' marks an empty slot */
              shown[k++] = (c == ' ') ? '.' : c;       /* a chosen space also shows as '.' */
              shown[k++] = ' ';
          }
          shown[k] = 0;
          DrawMenuTextCentered(168, 70, ARC_WHITE, (uint8_t *)shown, ARC_CHARS); }

        for (int rr = 0; rr < NAME_CHARROWS; rr++)
            for (int cc = 0; cc < NAME_COLS; cc++) {
                char cs[2] = { s_nameGrid[rr * NAME_COLS + cc], 0 };
                int cx = NAME_GX336 + cc * NAME_CELL_W + NAME_CELL_W / 2;
                int cy = NAME_GY + rr * NAME_CELL_H;
                DrawMenuTextCentered((uint16_t)cx, (uint16_t)cy, ARC_WHITE, (uint8_t *)cs, ARC_CHARS);
            }
        { int ay = NAME_GY + NAME_CHARROWS * NAME_CELL_H;
          DrawMenuTextCentered(NAME_GX336 + NAME_GW / 4,     ay, ARC_WHITE, (uint8_t *)"DEL", ARC_CHARS);
          DrawMenuTextCentered(NAME_GX336 + 3 * NAME_GW / 4, ay, ARC_WHITE, (uint8_t *)"END", ARC_CHARS); }

        arc_draw_bottom_prompt(frame);
        _frontbufferBasePtr = sFb; _menu_draw_x_offset = sOff; _cameraXMod320 = sCam;
        arc_composite_menu_overlay(mpal);

        /* flashing selector box over the highlighted cell (RGBA, screen-space) */
        uint32_t selc = ((frame >> 3) & 1) ? 0xFFFFFF00u : 0xFFFF8000u;
        int rx, ry, rw;
        if (row < NAME_CHARROWS) {
            rx = WIDE_MARGIN + NAME_GX336 + col * NAME_CELL_W;
            ry = NAME_GY + row * NAME_CELL_H - 6;
            rw = NAME_CELL_W;
        } else {
            int leftHalf = (col < NAME_COLS / 2);
            rx = WIDE_MARGIN + NAME_GX336 + (leftHalf ? 0 : NAME_GW / 2);
            ry = NAME_GY + NAME_CHARROWS * NAME_CELL_H - 6;
            rw = NAME_GW / 2;
        }
        arc_flag_rect(rx, ry, rw, NAME_CELL_H, selc);
        arc_flag_rect(rx + 1, ry + 1, rw - 2, NAME_CELL_H - 2, selc);

        if (s_arcadeFade < 256) { s_arcadeFade += 24; if (s_arcadeFade > 256) s_arcadeFade = 256; }
        WaitForVSync();
        ReadTimerDelta();
    }
    arc_fade_ramp(0, 10);
    s_flagScreenActive = 0;
    snprintf(out, ARC_HS_NAME_MAX + 1, "%s", entered);
}

/* End of a winner-stays-on run: GAME OVER beat, then (if it qualifies) name entry
 * and a flash of the updated table. Modal — coins accumulate but the flow can't be
 * interrupted into a new game until it returns to the attract loop. */
static void arc_run_over(int mode, uint32_t wins, uint32_t totalSecs, int flag)
{
    char l2[32], l3[32];
    arc_hs_load();   /* s_arcModalRunOver is held by the caller for the whole run */

    snprintf(l2, sizeof l2, "WINS: %u", wins);
    snprintf(l3, sizeof l3, "TIME: %u:%02u", totalSecs / 60, totalSecs % 60);
    arc_message_screen("GAME OVER", l2, l3, 150, 1);

    int rank = arc_hs_rank(mode, wins, totalSecs);
    if (rank < 0 && s_arcDebugForceNameEntry)    /* DEBUG: force the name-entry screen */
        rank = ARC_HS_ENTRIES - 1;               /* (last place) even if it wouldn't qualify */
    s_arcDebugForceNameEntry = 0;
    if (rank >= 0) {
        char name[ARC_HS_NAME_MAX + 1] = "";
        arc_name_entry(rank, name);
        if (!name[0]) snprintf(name, sizeof name, "AAA");
        arc_hs_insert(mode, rank, name, wins, totalSecs,
                      (flag >= 0 && flag < FLAG_COUNT) ? (uint8_t)flag : 0xFF);
        arc_hs_save();
        arc_present_table(mode, 4 * 50, rank);   /* show the table, new row flashing */
    } else {
        arc_present_table(mode, 3 * 50, -1);     /* no high score — still show the table */
    }
}

void ArcadeMainLoop(void)
{
    fprintf(stderr, "[arcade] boot-to-arcade: entering attract loop\n");

    /* The whole arcade renders in DOS graphics (256-colour) so the themed pitches
     * work. The DOS palette only applies during a match/reel (refresh_palette_from_copper
     * gates it on _playGame/_playHighlightFlag), so the menus/selectors/line-up keep
     * their normal palettes. Gameplay feel (dos_gameplay) is a separate setting. */
    { extern int _gDosGraphics, _gDosGameplay;
      _gDosGraphics = 1;
      if (g_arcadeOpts) _gDosGameplay = g_arcadeOpts->arcade_gameplay; }

    /* No animated advertising hoardings in the arcade — the themed pitches carry
     * their own static perimeter art, and the sliding ad sprites would draw over it.
     * _blockAdvertisements makes UpdateAdvertisements keep them invisible; nothing
     * else writes it, so set it once for the whole session. */
    { extern uint16_t _blockAdvertisements; _blockAdvertisements = 1; }

    /* The boot loader leaves the copper palette faded to black; restore the menu
     * palette so anything we draw is visible. */
    { extern uint16_t _menuPalette[]; WriteCopperPalette(_menuPalette); }

    arc_load_logo();
    arc_load_flags();
    arc_load_menubg();
    arc_load_svcfont();
    arc_hs_load();          /* high-score tables (ARCADE.HI; seeds defaults if absent) */
    arc_scan_reels();
    /* Coin-insert jingle (mixed over the attract audio). */
    { extern int AudioSDL_LoadOverlaySample(const char *); AudioSDL_LoadOverlaySample("sound/1UP.WAV"); }
    ReadTimerDelta();

    arc_music_attract();   /* HERO (title) song plays over the attract loop */

    while (_quitToDos == 0) {
        if (s_serviceReq) {              /* F2 pressed during a reel/idle */
            s_serviceReq = 0;
            arc_service_menu();
            /* GAMEPLAY may have changed: re-apply it and re-pick the reel subdir. */
            { extern int _gDosGameplay; if (g_arcadeOpts) _gDosGameplay = g_arcadeOpts->arcade_gameplay; }
            arc_scan_reels();
            arc_music_attract();         /* re-apply ATTRACT AUDIO (may have changed) */
            continue;
        }
        if (s_startGame) {               /* start pressed during a reel/idle */
            arc_start_game();
            continue;
        }

        if (s_reelCount == 0) {
            /* No reels: run the attract foreground (logo<->table cross-fade + INSERT
             * COIN) over a black screen, then re-scan for newly-dropped reels. */
            arc_idle_insert_coin(300);
            arc_scan_reels();
            continue;
        }

        /* Play a random reel right through (the foreground logo<->high-score cross-fade
         * runs over it via the Arcade_DrawAttractOverlay hook — the reel is NEVER cut
         * for the foreground). When it finishes, loop straight into the next reel so a
         * reel is always playing in the background. ViewHighlights does
         * CopyTeamsAndHeader + InitGameSaveTeams + PlayHighlightsLoop and returns when
         * the reel ends (or a coin/start/service aborts it). */
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

        /* Reel finished — restore the menu palette and loop to the next reel. */
        { extern uint16_t _menuPalette[]; WriteCopperPalette(_menuPalette); }
    }
}

#endif /* SWOS_SDL_BUILD */
