/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "loader_intro.h"
#include "fade.h"
#include "display.h"
#include "input.h"
#include "audio.h"
#ifdef SWOS_SDL_BUILD
#include <string.h>
extern uint8_t rawAssetLoadBuffer[];
extern int  AudioSDL_StartSong(const uint8_t *song_data, int song_size,
                               const uint8_t *ins_data,  int ins_size,
                               int subsong);
extern void _AudioSetChannelVolume(uint16_t mask, uint16_t vol);
/* Sizes of the buffers used to stage hero.sng / hero.ins. The actual
 * file payload is smaller; RJP1_Load parses headers internally and
 * uses only what it needs. */
#define HERO_SONG_BUFFER_BYTES   (4 * 1024)        /* diyFileBufferCopyBuf */
#define HERO_INS_BANK_BYTES      (512 * 1024)      /* audioInstrumentBank */
#endif

/* ---- External globals (all live in SWOS2.s) ---- */
extern uint16_t paletteIndex;
extern uint16_t g_quitToDos;
extern volatile uint16_t currentGameTick;
extern void _WaitForVSync(void);   /* game_utils.c — pumps audio + events on SDL */
extern uint8_t pl1Fire;
extern volatile uint32_t frontbufferBaseAltPtr;
extern volatile uint32_t frontbufferBasePtr;
extern uint32_t memBoundaryMarker;
extern uint16_t audioBankLoaded;
extern volatile uint16_t audioActive;
extern uint8_t diyFileBufferCopy[];
extern uint8_t audioInstrumentBank[];
extern uint8_t aSoundHeroSng[];
extern uint8_t aSoundHeroIns[];

/* ---- External asm functions (C-callable bridges in swos2_unit.s) ---- */
/* _Player1StatusProc declared in input.h */
extern int16_t _LoadFileEx(uint8_t *filename, uint8_t *dest, uint32_t *outFileSize);
extern int16_t _AssetEnsureReadyWithRetry(uint8_t *name, uint8_t *dest);  /* asset_loader.c */
extern int16_t _LoadRNCToBufferNoBlit(uint8_t *name, uint8_t *dest);     /* asset_loader.c */

/* ================================================================
 * LoaderIntroEnsureMenuMusicPlaying  (SWOS2.s line 21035)
 *
 * If audio bank not yet loaded, stops audio and starts menu music
 * from the hero audio bank data. Then applies preset 0 and marks
 * audio as active.
 * ================================================================ */
static void loaderIntroEnsureMenuMusicPlaying(void)
{
#ifdef SWOS_SDL_BUILD
    /* SDL build: route through the RJP1 player + SDL audio callback
     * (same mechanism _TryPlayMusic uses for the menu song). The Amiga
     * `_InitAudioState` is stubbed on SDL because it parses a 68k-BE
     * bank header we don't byte-swap, so calling it here would leave
     * the hero music silent. Hand the loaded hero.sng / hero.ins to
     * the player instead, then ramp the master volume in. */
    if (audioBankLoaded == 0) {
        _StopAllAudioChannels();
        /* HERO has 5 subsongs; the reference rjp1player plays subsong 0
         * (the full mix with vocals). Subsong 1 has the singing channel
         * sequence byte zeroed, so passing 1 here was the cause of the
         * "music plays but no singing" symptom. */
        if (AudioSDL_StartSong(diyFileBufferCopy, HERO_SONG_BUFFER_BYTES,
                               audioInstrumentBank, HERO_INS_BANK_BYTES, 0) != 0) {
            /* Load failed — keep audio silent but mark "loaded" so we
             * don't hammer the player every vblank. */
        }
        audioBankLoaded = 1;
    }

    _AudioSetChannelVolume(0xFFFF, 0);
    audioActive = 1;
    /* Ramp the master volume from 0 to 0x23 over a few vblanks so the
     * song fades in instead of starting at full volume mid-note. Mirror
     * of _TryPlayMusic's ramp on the menu song. */
    for (uint16_t vol = 0; vol <= 0x22; vol += 2) {
        _AudioSetChannelVolume(0xFFFF, vol);
        _WaitForVSync();
    }
    _AudioSetChannelVolume(0xFFFF, 0x23);
#else
    if (audioBankLoaded == 0)
    {
        _StopAllAudioChannels();
        _InitAudioState(diyFileBufferCopy, audioInstrumentBank);
        audioBankLoaded = 1;
    }
    _AudioApplyPresetToChannels(0);
    audioActive = 1;
#endif
}

/* ================================================================
 * LoaderIntroStopMusicAndResetAudio  (SWOS2.s line 21051)
 *
 * Beeps all channels down to silence, stops audio, and clears
 * the active/loaded flags.
 * ================================================================ */
static void loaderIntroStopMusicAndResetAudio(void)
{
    _AudioBeepAllShort();
    _StopAllAudioChannels();
    audioActive = 0;
    audioBankLoaded = 0;
}

/* ================================================================
 * LoaderIntroLoadHeroAudioBank  (SWOS2.s line 21021)
 *
 * Loads hero song ("sound/hero.sng") into diyFileBufferCopy and
 * hero instruments ("sound/hero.ins") into audioInstrumentBank.
 * Retries the entire sequence from scratch on any failure.
 * ================================================================ */
static void loaderIntroLoadHeroAudioBank(void)
{
retry:
    audioBankLoaded = 0;
    if (_AssetEnsureReadyWithRetry(aSoundHeroSng, diyFileBufferCopy) != 0)
        goto retry;
    if (_LoadRNCToBufferNoBlit(aSoundHeroIns, audioInstrumentBank) != 0)
        goto retry;
}

/* ================================================================
 * LoaderIntroLoadFrameToFrontbufferAlt  (SWOS2.s line 21365)
 *
 * Loads a raw image file into the alternate frontbuffer.
 *
 * Amiga: bitplane data goes straight into the alt frontbuffer
 *   (which is itself bitplane-format) — display picks it up as-is.
 *
 * SDL:   the alt frontbuffer is 8bpp chunky (palette index per
 *   byte) so writing raw bitplane bytes there makes
 *   `chunky_to_rgba` paint garbage. Stage the file through
 *   `rawAssetLoadBuffer` and decode NUM_PLANES interleaved planes
 *   into chunky bytes — same loop _LoadOverlayToFrontbuffer uses
 *   for menubg.raw. Frame layout matches the menu BG: PLANE_STRIDE
 *   bytes/plane row, NUM_PLANES planes, decode SCREEN_WIDTH pixels
 *   per scanline.
 * ================================================================ */
static void loaderIntroLoadFrameToFrontbufferAlt(const uint8_t *filename)
{
#ifdef SWOS_SDL_BUILD
    _LoadFileEx((uint8_t *)filename, rawAssetLoadBuffer, (uint32_t *)0);

    uint8_t *dstAlt  = (uint8_t *)frontbufferBaseAltPtr;
    uint8_t *dstBase = (uint8_t *)frontbufferBasePtr;
    if (!dstAlt) return;

    memset(dstAlt, 0, FRONTBUFFER_SIZE_TOTAL_BYTES);
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        const uint8_t *rowSrc = rawAssetLoadBuffer + (size_t)y * ROW_STRIDE_BYTES;
        uint8_t       *rowDst = dstAlt + (size_t)y * FRONTBUFFER_ROW_STRIDE_BYTES;
        for (int px = 0; px < SCREEN_WIDTH; px++) {
            int bit  = 7 - (px & 7);
            int byte = px >> 3;
            uint8_t idx = 0;
            for (int p = 0; p < NUM_PLANES; p++) {
                uint8_t planeByte = rowSrc[p * PLANE_STRIDE + byte];
                idx |= (uint8_t)(((planeByte >> bit) & 1u) << p);
            }
            rowDst[px] = idx;
        }
    }

    /* Mirror alt → base so the post-swap frame the renderer reads is
     * always the decoded image, not whatever stale chunky data was in
     * the other buffer. _LoadFileEx does the same mirror when dest is
     * alt; we have to do it ourselves here because we staged through
     * rawAssetLoadBuffer to run the bitplane decode. Without this the
     * intro frame appears for ~1/50 s then gets replaced by the prior
     * buffer contents on the next vsync swap → visible flicker. */
    if (dstBase && dstBase != dstAlt) {
        memcpy(dstBase, dstAlt, FRONTBUFFER_SIZE_TOTAL_BYTES);
    }
#else
    _LoadFileEx((uint8_t *)filename, (uint8_t *)frontbufferBaseAltPtr, (uint32_t *)0);
#endif
}

/* ================================================================
 * LoaderIntroWaitForFireOrTimeout  (SWOS2.s line 21337)
 *
 * Polls player 1 input each frame. Returns non-zero if fire was
 * pressed or g_quitToDos set; returns 0 if the timeout (d7 frames)
 * expired without interruption.
 *
 * Original asm: d7 = timeout count (dbf-style, so iterates d7+1 times).
 * Each iteration: poll input, wait 1 tick.
 * ================================================================ */
static int16_t loaderIntroWaitForFireOrTimeout(int16_t timeout)
{
    int16_t remaining = timeout;

    while (remaining >= 0)
    {
        uint16_t tick;

        _Player1StatusProc();

        if (pl1Fire != 0)
            return 1;
        if (g_quitToDos != 0)
            return 1;

        _WaitForVSync();

        remaining--;
    }

    return 0;
}

/* ================================================================
 * LoaderIntroShowFrameFadeInOut  (SWOS2.s line 21287)
 *
 * Loads a frame to the alt frontbuffer, fades in, waits for
 * fire/timeout, then fades out (unless interrupted).
 * Returns non-zero if fire/quit interrupted, 0 if timeout.
 * ================================================================ */
static int16_t loaderIntroShowFrameFadeInOut(const uint8_t *filename, int16_t timeout)
{
    if (g_quitToDos != 0) return 1;

    loaderIntroLoadFrameToFrontbufferAlt(filename);
#ifndef AGA
    _FadeIn();
#endif

    if (loaderIntroWaitForFireOrTimeout(timeout) != 0)
        return 1;

#ifndef AGA
    /* Skip the fade-out if the user closed the window — it just adds
     * ~1 s of unresponsive black-screen between the click and exit. */
    if (g_quitToDos == 0)
        _FadeOutToBlack();
#endif
    return 0;
}

/* ================================================================
 * LoaderIntroShowFrameWithMusic  (SWOS2.s line 21305)
 *
 * Same as ShowFrameFadeInOut but also ensures menu music is playing
 * before the fade-in.
 * ================================================================ */
static int16_t loaderIntroShowFrameWithMusic(const uint8_t *filename, int16_t timeout)
{
    if (g_quitToDos != 0) return 1;

    loaderIntroLoadFrameToFrontbufferAlt(filename);
    loaderIntroEnsureMenuMusicPlaying();
#ifndef AGA
    _FadeIn();
#endif

    if (loaderIntroWaitForFireOrTimeout(timeout) != 0)
        return 1;

#ifndef AGA
    if (g_quitToDos == 0)
        _FadeOutToBlack();
#endif
    return 0;
}

/* ================================================================
 * ShowLoaderIntroSequence  (SWOS2.s line 21170)
 *
 * Displays the loader intro animation sequence: 13 frames with
 * fade in/out effects, music, and fire-button skip support.
 *
 * Flow:
 *   1. Sanity-check memBoundaryMarker address
 *   2. Initialize camera and display
 *   3. Poll fire button (skip intro if pressed)
 *   4. Load hero audio bank
 *   5. Show frames loader00..loader10 with fade in/out
 *   6. Show final loader1.raw frame with extended timeout
 *   7. Stop music and reset audio
 *
 * Early exit paths:
 *   - Fire pressed before frames → jump to loader1.raw, fade in, return
 *   - Load fail on first 2 frames → fade out, show loader1.raw, fade in
 *   - Load fail on later frames → fade out, show loader1.raw, fade in,
 *     then stop music
 * ================================================================ */
__attribute__((used, externally_visible))
void _ShowLoaderIntroSequence(void)
{
    /* Sanity check: memBoundaryMarker address must be <= 1MB */
    if ((uint32_t)&memBoundaryMarker > 0x100000u)
    {
        /* Original asm had a commented-out infinite color-cycle loop here.
         * In practice this path is dead code in a working build. */
    }

    /* Initialize camera at (0,0) and set up display */
    _SetCameraCoordinates(0, 0);
    _SwapBuffersAndCameraViewState();
    _UpdatePlanePtrsAndBlitParams();
    _SwapBuffersAndCameraViewState();
    _UpdatePlanePtrsAndBlitParams();

    /* Check if fire is already pressed → skip intro */
    _Player1StatusProc();
    if (pl1Fire != 0)
    {
        /* Fire pressed path: just show loader1.raw and fade in */
#ifndef AGA
        paletteIndex = 3;
#endif
        loaderIntroLoadFrameToFrontbufferAlt((const uint8_t *)GRAFS "loader1.raw");
        _FadeIn();
        return;
    }

    /* Load hero audio bank (retries until success) */
    loaderIntroLoadHeroAudioBank();

    /* Show frame sequence */
#ifndef AGA
    paletteIndex = 6;
#endif

    /* Frame 0: loader00.raw */
    if (loaderIntroShowFrameFadeInOut((const uint8_t *)GRAFS "loader00.raw", 0xFA) != 0)
        goto load_fail_early;

    /* Frame 1: loader01.raw */
    if (loaderIntroShowFrameFadeInOut((const uint8_t *)GRAFS "loader01.raw", 0xFA) != 0)
        goto load_fail_early;

    /* Frame 2: loader1.raw (with music) */
#ifndef AGA
    paletteIndex = 3;
#endif
    if (loaderIntroShowFrameWithMusic((const uint8_t *)GRAFS "loader1.raw", 0xFA) != 0)
        goto exit_cleanup;

    /* Frame 3: loader2.raw */
#ifndef AGA
    paletteIndex = 5;
#endif
    if (loaderIntroShowFrameFadeInOut((const uint8_t *)GRAFS "loader2.raw", 0xFA) != 0)
        goto load_fail_mid;

    /* Frames 4-12: loader3..loader10 */
    if (loaderIntroShowFrameFadeInOut((const uint8_t *)GRAFS "loader3.raw", 0xFA) != 0)
        goto load_fail_mid;
    if (loaderIntroShowFrameFadeInOut((const uint8_t *)GRAFS "loader4.raw", 0xFA) != 0)
        goto load_fail_mid;
    if (loaderIntroShowFrameFadeInOut((const uint8_t *)GRAFS "loader5.raw", 0xFA) != 0)
        goto load_fail_mid;
    if (loaderIntroShowFrameFadeInOut((const uint8_t *)GRAFS "loader6.raw", 0xFA) != 0)
        goto load_fail_mid;
    if (loaderIntroShowFrameFadeInOut((const uint8_t *)GRAFS "loader7.raw", 0xFA) != 0)
        goto load_fail_mid;
    if (loaderIntroShowFrameFadeInOut((const uint8_t *)GRAFS "loader8a.raw", 0xFA) != 0)
        goto load_fail_mid;
    if (loaderIntroShowFrameFadeInOut((const uint8_t *)GRAFS "loader8b.raw", 0xFA) != 0)
        goto load_fail_mid;
    if (loaderIntroShowFrameFadeInOut((const uint8_t *)GRAFS "loader9.raw", 0xFA) != 0)
        goto load_fail_mid;
    if (loaderIntroShowFrameFadeInOut((const uint8_t *)GRAFS "loader10.raw", 0xFA) != 0)
        goto load_fail_mid;

    /* Final frame: loader1.raw with extended timeout (0x6D6 = 1750 frames) */
#ifndef AGA
    paletteIndex = 3;
#endif
    loaderIntroShowFrameFadeInOut((const uint8_t *)GRAFS "loader1.raw", 0x6D6);
    /* Fall through to exit_cleanup (ignore return value) */

exit_cleanup:
    loaderIntroStopMusicAndResetAudio();
    return;

load_fail_mid:
    /* Fire pressed → show loader1 splash and continue.
     * Window-X pressed → bypass the splash and exit fast. */
    if (g_quitToDos == 0) {
        _FadeOutToBlack();
#ifndef AGA
        paletteIndex = 3;
#endif
        loaderIntroLoadFrameToFrontbufferAlt((const uint8_t *)GRAFS "loader1.raw");
        _FadeIn();
    }
    goto exit_cleanup;

load_fail_early:
    if (g_quitToDos == 0) {
        _FadeOutToBlack();
#ifndef AGA
        paletteIndex = 3;
#endif
        loaderIntroLoadFrameToFrontbufferAlt((const uint8_t *)GRAFS "loader1.raw");
        _FadeIn();
    }
    return;
}
