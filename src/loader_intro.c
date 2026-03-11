#include "loader_intro.h"
#include "fade.h"
#include "display.h"
#include "input.h"
#include "audio.h"

/* ---- External globals (all live in SWOS2.s) ---- */
extern uint16_t paletteIndex;
extern uint16_t g_quitToDos;
extern volatile uint16_t currentGameTick;
extern uint8_t pl1Fire;
extern volatile uint32_t frontbufferBaseAltPtr;
extern uint32_t memBoundaryMarker;
extern uint16_t audioBankLoaded;
extern uint16_t audioActive;
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
    if (audioBankLoaded == 0)
    {
        _StopAllAudioChannels();
        _InitAudioState(diyFileBufferCopy, audioInstrumentBank);
        audioBankLoaded = 1;
    }
    _AudioApplyPresetToChannels(0);
    audioActive = 1;
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
 * ================================================================ */
static void loaderIntroLoadFrameToFrontbufferAlt(const uint8_t *filename)
{
    _LoadFileEx((uint8_t *)filename, (uint8_t *)frontbufferBaseAltPtr, (uint32_t *)0);
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

        /* Wait 1 game tick (d1=0 → dbf loops once) */
        tick = currentGameTick;
        while (currentGameTick == tick)
        {
            /* spin */
        }

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
    loaderIntroLoadFrameToFrontbufferAlt(filename);
#ifndef AGA
    _FadeIn();
#endif

    if (loaderIntroWaitForFireOrTimeout(timeout) != 0)
        return 1;

#ifndef AGA
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
    loaderIntroLoadFrameToFrontbufferAlt(filename);
    loaderIntroEnsureMenuMusicPlaying();
#ifndef AGA
    _FadeIn();
#endif

    if (loaderIntroWaitForFireOrTimeout(timeout) != 0)
        return 1;

#ifndef AGA
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
    _FadeOutToBlack();
#ifndef AGA
    paletteIndex = 3;
#endif
    loaderIntroLoadFrameToFrontbufferAlt((const uint8_t *)GRAFS "loader1.raw");
    _FadeIn();
    goto exit_cleanup;

load_fail_early:
    _FadeOutToBlack();
#ifndef AGA
    paletteIndex = 3;
#endif
    loaderIntroLoadFrameToFrontbufferAlt((const uint8_t *)GRAFS "loader1.raw");
    _FadeIn();
    return;
}
