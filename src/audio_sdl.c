/*
 * audio_sdl.c -- SDL audio backend for the SWOS port.
 *
 * The game's music and SFX are RJP1 modules (Richard Joseph Player) —
 * .sng pattern data plus .ins (sample bank), both Amiga big-endian.
 * We host a ported-to-C RJP1 player (src/rjp1.c) and drive it from
 * the main game-loop tick via SDL_QueueAudio.
 *
 * Output format: signed 8-bit stereo at the device's negotiated rate
 * (usually 44.1 kHz, sometimes 48 kHz on WSL2 / Windows). RJP1_MixSamples
 * accepts the rate per call so resampling stays correct regardless.
 *
 * Why queue mode instead of SDL audio callback mode: on WSL2 and some
 * Windows audio drivers, SDL's callback thread is scheduled extremely
 * unreliably — we measured persistent 91 ms gaps and occasional 2+ s
 * stalls, well beyond any reasonable buffer size. In queue mode we
 * produce audio from the main thread at the game's 50 Hz vblank tick
 * and SDL drains it on its own; the game loop is paced by a
 * high-resolution counter and is very stable.
 */

#include "platform_sdl.h"
#include "rjp1.h"
#include "sfx_debug.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>

#define AUDIO_SAMPLE_RATE 44100

static SDL_AudioDeviceID s_audio_dev;
static SDL_AudioSpec     s_audio_spec;
static SDL_mutex        *s_mtx;

static RJP1_Player       s_player;
static int               s_player_active;   /* 1 = RJP1_Load succeeded and a song is selected */

/* Second RJP1 player dedicated to SFX/crowd, so music and SFX can play AT THE
 * SAME TIME (arcade attract "music+sfx": HERO over the reel crowd). When
 * s_sfx_use_second is off, SFX shares s_player exactly as before (music XOR SFX,
 * the normal game) and s_sfx_player stays idle — so the normal game is
 * unaffected. When on, LoadSfxBank/the triggers route here and the tick mixes
 * both players. Toggled via AudioSDL_SetSfxSecondPlayer. */
static RJP1_Player       s_sfx_player;
static int               s_sfx_active;
static int               s_sfx_use_second;

/* Where SFX (bank load + triggers) go right now, and the matching active flag. */
static RJP1_Player *sfx_tgt(void)        { return s_sfx_use_second ? &s_sfx_player : &s_player; }
static int          sfx_act(void)        { return s_sfx_use_second ? s_sfx_active  : s_player_active; }
static void         sfx_set_act(int v)   { if (s_sfx_use_second) s_sfx_active = v; else s_player_active = v; }

static int               s_master_volume;   /* 0..64, scales final output */
/* Down-mix flag. The Amiga's 4 channels are hard-panned (1,2 → left ;
 * 0,3 → right) by Paula and so by the cyxx player ported to rjp1.c. On
 * speakers without strong stereo separation that hard pan sounds
 * unbalanced — set this to 1 (the default) to average L+R back into
 * each lane after the mix. --audio=stereo restores the original Amiga
 * panning. */
static int               s_audio_mono = 1;

/* Song looping. Most SWOS songs (the menus) loop internally, but some — notably
 * the HERO title song used in arcade attract — have no loop-back and just end.
 * When s_loop_song is set, AudioSDL_Tick restarts the song the moment it finishes
 * so it plays continuously. s_loop_subsong remembers which subsong to restart. */
static int               s_loop_song;
static uint8_t           s_loop_subsong;

/* One-shot WAV overlay (e.g. the arcade coin-insert jingle). Loaded once into the
 * device format (S16 stereo at the device rate) and mixed on top of whatever the
 * RJP1 player is producing — completely separate from the player, so it can never
 * disturb the music's channels. s_ovl_pos < 0 means idle. */
static int16_t          *s_ovl_pcm;
static int               s_ovl_len;     /* total int16 samples (interleaved stereo) */
static int               s_ovl_pos = -1;

/* Queue-mode mix scratch. Sized for one game tick (50 Hz) of stereo S8
 * at the highest plausible negotiated rate (48 kHz) plus a little slack. */
static int8_t            s_mix_buf[48000 * 2 / 50 + 128];
/* Same scratch for the SFX player, summed with s_mix_buf at the 16-bit stage. */
static int8_t            s_sfx_mix_buf[48000 * 2 / 50 + 128];
/* 16-bit output buffer: the 8-bit mix is up-converted here with the master volume
 * applied losslessly (see want.format). One int16 per int8 mix sample. */
static int16_t           s_out_buf[48000 * 2 / 50 + 128];

/* Target queue fill we try to maintain ahead of the playback cursor.
 * 80 ms gives the driver enough cushion to ride out ~4 game frames of
 * main-thread stalls without audibly underrunning, without adding
 * noticeable latency between music events and what you hear.
 *
 * WASM runs single-threaded and pumps audio once per emscripten_sleep frame,
 * which jitters more (browser scheduling / GC) than a native timer thread — a
 * deeper queue rides that out. Paired with the real-audio fill loop (below /
 * Platform_RenderFrame) instead of the native silence-pad. */
#ifdef SWOS_WASM_BUILD
#define QUEUE_TARGET_MS  120
#else
#define QUEUE_TARGET_MS  80
#endif

/* Dedicated audio-pump thread. Keeping the SDL queue full is the audio engine's
 * one hard real-time job, but our only pumps were the game loop (Platform_VBlank)
 * and the render timer — both of which stall under render load, draining the
 * 80 ms queue to silence (the grain/dropouts that make us sound worse than the
 * reference player, which has its own audio-callback thread). This thread does
 * nothing but refill the queue on a tight interval, fully decoupled from
 * rendering. The game-loop/timer pumps stay as harmless backups (AudioSDL_Tick
 * is mutex-guarded and no-ops once the queue is at target). */
static SDL_Thread  *s_pump_thread;
static volatile int s_pump_run;
static int audio_pump_thread(void *unused);

int AudioSDL_Init(int mono)
{
    s_audio_mono = mono ? 1 : 0;
#if SWOS_AUDIO_DISABLED
    /* Audio disabled at compile time — return success so platform init
     * continues; s_audio_dev stays 0 and AudioSDL_Tick early-returns. */
    return 0;
#endif
    s_mtx = SDL_CreateMutex();

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq     = AUDIO_SAMPLE_RATE;
    want.format   = AUDIO_S16SYS;  /* 16-bit out: the RJP1 mix is 8-bit, but the
                                    * master-volume / fade scaling is applied in the
                                    * 8->16 conversion so it's lossless (no 8-bit
                                    * quantisation grain — the Amiga did volume in
                                    * analog after the DAC). */
    want.channels = 2;
    want.samples  = 1024;         /* driver hint only; we use QueueAudio */
    want.callback = NULL;         /* queue mode — we push, SDL pulls */
    want.userdata = NULL;

    /* Accept whatever freq the driver wants to give us so we aren't
     * dependent on exactly 44.1 kHz being available. AudioSDL_Tick uses
     * `s_audio_spec.freq` to compute how many bytes to generate per
     * game tick, so a negotiated rate works transparently. */
    s_audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &s_audio_spec,
                                      SDL_AUDIO_ALLOW_FREQUENCY_CHANGE |
                                      SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if (s_audio_dev == 0) {
        fprintf(stderr, "SDL_OpenAudioDevice: %s\n", SDL_GetError());
        return -1;
    }
    s_master_volume = 0;
    SDL_PauseAudioDevice(s_audio_dev, 0);

    /* Start the dedicated pump so the queue stays full regardless of render load.
     * Single-threaded on WASM — AudioSDL_Tick is already called from
     * Platform_RenderFrame every frame, so the pump thread is redundant (and
     * pthreads on wasm need SharedArrayBuffer/COOP+COEP). */
    s_pump_run    = 1;
#ifndef SWOS_WASM_BUILD
    s_pump_thread = SDL_CreateThread(audio_pump_thread, "swos-audio-pump", NULL);
#endif
    return 0;
}

/* Silence the audio device immediately. Use this on SDL_QUIT so that
 * crowd chants / music don't continue playing while the game loop
 * unwinds back to Platform_Shutdown — the SDL audio driver buffers
 * 80–250 ms of samples, which is audible enough to leak past the
 * window close. Pausing + clearing the queue stops sound at the
 * driver instantly. The player state (s_player_active, RJP1 channels)
 * is left intact so any post-close cleanup that wants to interact
 * with audio still has a valid device until AudioSDL_Shutdown
 * actually closes it. */
void AudioSDL_PauseAndClear(void)
{
    if (s_audio_dev) {
        SDL_PauseAudioDevice(s_audio_dev, 1);
        SDL_ClearQueuedAudio(s_audio_dev);
    }
}

void AudioSDL_Shutdown(void)
{
    /* Stop the pump before tearing down the device so it can't queue onto a
     * closed device. */
    if (s_pump_thread) {
        s_pump_run = 0;
        SDL_WaitThread(s_pump_thread, NULL);
        s_pump_thread = NULL;
    }
    if (s_audio_dev) {
        SDL_PauseAudioDevice(s_audio_dev, 1);
        SDL_ClearQueuedAudio(s_audio_dev);
        SDL_CloseAudioDevice(s_audio_dev);
        s_audio_dev = 0;
    }
    if (s_mtx) { SDL_DestroyMutex(s_mtx); s_mtx = NULL; }
}

/* Produce audio and queue it into SDL. Called once per game tick
 * (50 Hz) from BOTH the main thread (via Platform_RenderFrame) and the
 * SDL_AddTimer worker (tick_timer_cb). Either thread is sufficient;
 * having both means audio keeps flowing even when one is blocked
 * (e.g. main is stuck inside SDL_RenderPresent waiting for monitor
 * vblank with --vsync=on, which can occasionally exceed one game frame
 * on a 60 Hz monitor due to 50→60 pulldown).
 *
 * Rationale for queue mode over callback mode: on WSL2 + PulseAudio the
 * SDL audio callback thread gets scheduled extremely unevenly — we saw
 * callback gaps of 91 ms consistently, spiking to 2+ seconds after a
 * couple of minutes of play. That drains the driver buffer faster than
 * we refill it and produces the "note pause note pause" stutter.
 *
 * With queue mode we drive production ourselves at the game's 50 Hz
 * vsync, which is paced by a high-resolution counter (Platform_VBlank)
 * and is extremely reliable. We only enqueue as much as we need to
 * keep roughly QUEUE_TARGET_MS of audio ahead of the playback cursor,
 * so latency stays low and the queue can absorb short main-thread
 * stalls without underrunning.
 *
 * Concurrency: the queue-fill check and the RJP1 song-advance step
 * (RJP1_HandleTick) are wrapped in s_mtx as one critical section.
 * If both threads arrive simultaneously, the loser sees the queue
 * already topped up and bails out — guaranteeing the song state
 * advances at most once per "queue underflow event", not twice. */
void AudioSDL_Tick(void)
{
    if (!s_audio_dev) return;

    const int freq = s_audio_spec.freq ? s_audio_spec.freq : AUDIO_SAMPLE_RATE;
    const int samples_per_tick = (freq * 2) / 50;        /* int8 mix samples (stereo) per 50 Hz tick */
    if (samples_per_tick > (int)sizeof(s_mix_buf)) return;   /* safety */
    const Uint32 out_bytes = (Uint32)samples_per_tick * 2;   /* S16 device bytes to queue */

    /* Queue sizes are device (S16) bytes = 4 per stereo frame. */
    Uint32 target_bytes = (Uint32)freq * 4 * QUEUE_TARGET_MS / 1000;

    SDL_LockMutex(s_mtx);

    /* Re-check the queue inside the lock — if the other thread just
     * topped it up, we're done. This is what prevents two threads from
     * both calling RJP1_HandleTick for the same tick (which would
     * double-step the song state). */
    Uint32 queued_bytes = SDL_GetQueuedAudioSize(s_audio_dev);
    if (queued_bytes >= target_bytes) {
        SDL_UnlockMutex(s_mtx);
        return;
    }

    if (s_player_active) {
        SDL_memset(s_mix_buf, 0, (size_t)samples_per_tick);
        RJP1_HandleTick(&s_player);
        if (s_loop_song && RJP1_IsFinished(&s_player)) {
            RJP1_StartSong(&s_player, s_loop_subsong);  /* loop: restart the song... */
            RJP1_HandleTick(&s_player);                 /* ...and tick its first row now (no gap) */
        }
        RJP1_MixSamples(&s_player, freq, s_mix_buf, samples_per_tick);

        /* Down-mix to mono (default). Amiga channels 1,2 are hard-left
         * and 0,3 are hard-right in the cyxx mapping inside rjp1.c —
         * pleasant on stereo headphones, lopsided on a single speaker.
         * Energy-preserving fold-down: (L+R)/sqrt(2) (≈ *181/256) keeps the
         * overall loudness equal to the stereo mix; the old plain (L+R)/2 lost
         * ~3 dB and halved hard-panned content like the HERO vocal. */
        if (s_audio_mono) {
            for (int i = 0; i + 1 < samples_per_tick; i += 2) {
                int m = ((int)s_mix_buf[i] + (int)s_mix_buf[i + 1]) * 181 / 256;
                if (m < -128) m = -128; else if (m > 127) m = 127;
                s_mix_buf[i]     = (int8_t)m;
                s_mix_buf[i + 1] = (int8_t)m;
            }
        }
    } else {
        SDL_memset(s_mix_buf, 0, (size_t)samples_per_tick);   /* silence */
    }

    /* SFX player — only ticks when music+sfx mixing routed SFX to its own player
     * (s_sfx_active). Idle otherwise, so the normal game (SFX in s_player) is
     * unchanged. No loop handling: the SFX bank is trigger-driven. */
    if (s_sfx_active) {
        SDL_memset(s_sfx_mix_buf, 0, (size_t)samples_per_tick);
        RJP1_HandleTick(&s_sfx_player);
        RJP1_MixSamples(&s_sfx_player, freq, s_sfx_mix_buf, samples_per_tick);
        if (s_audio_mono) {
            for (int i = 0; i + 1 < samples_per_tick; i += 2) {
                int m = ((int)s_sfx_mix_buf[i] + (int)s_sfx_mix_buf[i + 1]) * 181 / 256;
                if (m < -128) m = -128; else if (m > 127) m = 127;
                s_sfx_mix_buf[i]     = (int8_t)m;
                s_sfx_mix_buf[i + 1] = (int8_t)m;
            }
        }
    } else {
        SDL_memset(s_sfx_mix_buf, 0, (size_t)samples_per_tick);
    }

    /* Up-convert the 8-bit mix to 16-bit, applying the master volume (0..64)
     * losslessly in the wider domain — a quiet level no longer collapses the
     * 8-bit range, so there's no quantisation grain (the Amiga applied volume in
     * analog after the DAC). At full (0x40) this is simply *256. The music and SFX
     * players sum here (each ±127); on loud overlaps the clamp catches the peak. */
    {
        const int mv = s_master_volume;
        for (int i = 0; i < samples_per_tick; ++i) {
            int v = ((int)s_mix_buf[i] + (int)s_sfx_mix_buf[i]) * 256 * mv / 64;
            if (v < -32768) v = -32768; else if (v > 32767) v = 32767;
            s_out_buf[i] = (int16_t)v;
        }
    }

    /* Mix the one-shot WAV overlay (coin-insert jingle etc.) on top at full level,
     * independent of the music's master volume. */
    if (s_ovl_pos >= 0 && s_ovl_pcm) {
        for (int i = 0; i < samples_per_tick && s_ovl_pos < s_ovl_len; i++, s_ovl_pos++) {
            int v = (int)s_out_buf[i] + (int)s_ovl_pcm[s_ovl_pos];
            if (v < -32768) v = -32768; else if (v > 32767) v = 32767;
            s_out_buf[i] = (int16_t)v;
        }
        if (s_ovl_pos >= s_ovl_len) s_ovl_pos = -1;   /* finished */
    }

    /* Enqueue inside the lock so a concurrent caller's queue re-check sees the
     * new size. SDL_QueueAudio is internally locked; nesting s_mtx is safe. */
    SDL_QueueAudio(s_audio_dev, s_out_buf, out_bytes);

#ifndef SWOS_WASM_BUILD
    /* If a long main-thread stall left us under target, top up with silence
     * rather than mixing multiple ticks (which would desync the fade tempo).
     * WASM does NOT do this — it would stutter, since the once-per-frame pump
     * regularly leaves us under target. Instead AudioSDL_Tick is called in a
     * loop (Platform_RenderFrame) so the queue fills with REAL ticks; the device
     * drains at real time so the song tempo stays correct. */
    queued_bytes = SDL_GetQueuedAudioSize(s_audio_dev);
    if (queued_bytes + out_bytes < target_bytes) {
        SDL_memset(s_out_buf, 0, (size_t)out_bytes);
        SDL_QueueAudio(s_audio_dev, s_out_buf, out_bytes);
    }
#endif

    SDL_UnlockMutex(s_mtx);
}

/* Dedicated pump loop (see s_pump_thread). Polls faster than the queue drains so
 * an underrun never has time to form; AudioSDL_Tick's own target check stops it
 * from over-filling (it returns early once QUEUE_TARGET_MS is buffered). The 5 ms
 * sleep keeps CPU negligible — most wake-ups just see a full queue and return. */
static int audio_pump_thread(void *unused)
{
    (void)unused;
    while (s_pump_run) {
        AudioSDL_Tick();
        SDL_Delay(5);
    }
    return 0;
}

/* ---- Public API for audio.c --------------------------------------- *
 *
 * `song_data` / `ins_data` point at raw .sng / .ins file contents that
 * the caller has loaded into one of the big BSS banks (menuSongData /
 * menuInsBank etc.). The player references them by pointer — don't
 * free or move them while a song is active. */
int AudioSDL_StartSong(const uint8_t *song_data, int song_size,
                       const uint8_t *ins_data,  int ins_size,
                       int subsong)
{
#if SWOS_AUDIO_DISABLED
    (void)song_data; (void)song_size; (void)ins_data; (void)ins_size; (void)subsong;
    return 0;
#endif
    SDL_LockMutex(s_mtx);

    int ok = RJP1_Load(&s_player, song_data, song_size, ins_data, ins_size);
    if (!ok) {
        fprintf(stderr, "[audio] RJP1_Load failed (song_size=%d ins_size=%d)\n",
                song_size, ins_size);
        s_player_active = 0;
        SDL_UnlockMutex(s_mtx);
        return -1;
    }

    /* Pass the caller's subsong number through verbatim — RJP1_StartSong
     * treats it as a 0-based index into the subsong table (matching the
     * cyxx reference player). The previous "if (subsong <= 0) subsong = 1"
     * remap clobbered the loader-intro's deliberate subsong=0 selection
     * for HERO, picking subsong-1 instead — and HERO's record-1 has the
     * vocal channel sequence byte zeroed, so the female vocals went
     * silent. RJP1_StartSong has its own clamp for out-of-range values. */
    if (subsong < 0) subsong = 0;
    RJP1_StartSong(&s_player, (uint8_t)subsong);
    s_player_active = 1;
    s_loop_subsong  = (uint8_t)subsong;  /* remembered so a looping song can restart */
    s_loop_song     = 0;                 /* off by default; caller opts in via AudioSDL_SetSongLoop */

    /* Drop whatever stale audio was queued so the new song starts
     * immediately instead of after the previous song's residual buffer. */
    if (s_audio_dev)
        SDL_ClearQueuedAudio(s_audio_dev);

    SDL_UnlockMutex(s_mtx);
    return 0;
}

void AudioSDL_StopSong(void)
{
#if SWOS_AUDIO_DISABLED
    return;
#endif
    SDL_LockMutex(s_mtx);
    if (s_player_active)
        RJP1_StopAll(&s_player);
    s_player_active = 0;
    s_loop_song = 0;
    SDL_UnlockMutex(s_mtx);
}

/* Route SFX (bank load + triggers) to a dedicated second player so music can play
 * simultaneously (arcade "music+sfx"). Off = SFX shares the music player exactly as
 * before. Call BEFORE LoadSfxBank/EnsureSfxLoadedAndStart so the bank lands in the
 * right player. */
void AudioSDL_SetSfxSecondPlayer(int on)
{
#if SWOS_AUDIO_DISABLED
    (void)on; return;
#endif
    SDL_LockMutex(s_mtx);
    s_sfx_use_second = on ? 1 : 0;
    SDL_UnlockMutex(s_mtx);
}

/* Stop + deactivate the dedicated SFX player (no effect on the music player). */
void AudioSDL_StopSfx(void)
{
#if SWOS_AUDIO_DISABLED
    return;
#endif
    SDL_LockMutex(s_mtx);
    if (s_sfx_active)
        RJP1_StopAll(&s_sfx_player);
    s_sfx_active = 0;
    SDL_UnlockMutex(s_mtx);
}

/* Enable/disable auto-restart of the current song when it finishes. Used for the
 * arcade HERO title song, which has no internal loop-back. Reset by StartSong. */
void AudioSDL_SetSongLoop(int loop)
{
#if SWOS_AUDIO_DISABLED
    (void)loop;
    return;
#endif
    SDL_LockMutex(s_mtx);
    s_loop_song = loop ? 1 : 0;
    SDL_UnlockMutex(s_mtx);
}

/* Load a PCM .wav into the one-shot overlay slot, converted to the device format
 * (S16 stereo at the device rate). Replaces any previously-loaded overlay. Returns
 * 0 on success. */
int AudioSDL_LoadOverlaySample(const char *path)
{
#if SWOS_AUDIO_DISABLED
    (void)path; return -1;
#endif
    if (!s_audio_dev || !path) return -1;

    SDL_AudioSpec ws;
    Uint8 *wbuf = NULL;
    Uint32 wlen = 0;
    if (!SDL_LoadWAV(path, &ws, &wbuf, &wlen)) {
        fprintf(stderr, "[audio] SDL_LoadWAV(%s): %s\n", path, SDL_GetError());
        return -1;
    }
    SDL_AudioCVT cvt;
    int rc = SDL_BuildAudioCVT(&cvt, ws.format, ws.channels, ws.freq,
                               AUDIO_S16SYS, 2, s_audio_spec.freq);
    if (rc < 0) {
        fprintf(stderr, "[audio] SDL_BuildAudioCVT(%s): %s\n", path, SDL_GetError());
        SDL_FreeWAV(wbuf);
        return -1;
    }
    cvt.len = (int)wlen;
    cvt.buf = (Uint8 *)SDL_malloc((size_t)cvt.len * (cvt.len_mult > 0 ? cvt.len_mult : 1));
    if (!cvt.buf) { SDL_FreeWAV(wbuf); return -1; }
    SDL_memcpy(cvt.buf, wbuf, wlen);
    SDL_FreeWAV(wbuf);
    int conv_bytes = cvt.len;              /* rc==0: already device format */
    if (rc == 1) { SDL_ConvertAudio(&cvt); conv_bytes = cvt.len_cvt; }

    SDL_LockMutex(s_mtx);
    if (s_ovl_pcm) SDL_free(s_ovl_pcm);
    s_ovl_pcm = (int16_t *)cvt.buf;
    s_ovl_len = conv_bytes / (int)sizeof(int16_t);
    s_ovl_pos = -1;
    SDL_UnlockMutex(s_mtx);
    return 0;
}

/* Trigger the loaded overlay sample from the start (mixed over the music). */
void AudioSDL_PlayOverlaySample(void)
{
#if SWOS_AUDIO_DISABLED
    return;
#endif
    SDL_LockMutex(s_mtx);
    if (s_ovl_pcm) s_ovl_pos = 0;
    SDL_UnlockMutex(s_mtx);
}

/* ---- SFX ---- *
 *
 * The SFX bank (sound/sfx.sng + sound/sfx.in1) uses the same RJP1
 * file format as music. SWOS' Amiga m68k audio engine (audio.c +
 * audio_irq.c) is the byte-poking engine that would normally parse
 * the bank section pointers and trigger samples — that engine is
 * stubbed out on SDL because the bank layout is big-endian and we
 * haven't byte-swapped it. Instead of porting all of audio_irq.c
 * we short-circuit the trigger calls into the existing RJP1 player
 * via RJP1_TriggerSample.
 *
 * We re-use the single s_player instance: when LoadSfxBank is called
 * (after sound/sfx.sng/in1 are read into BSS) it overwrites whatever
 * music was loaded. This matches the Amiga, which routes both music
 * and SFX through the same channel state — entering match mode
 * stops the menu music and hands the player to SFX. If we ever want
 * music _during_ a match we'll need a second RJP1_Player. */
int AudioSDL_LoadSfxBank(const uint8_t *song_data, int song_size,
                         const uint8_t *ins_data,  int ins_size)
{
#if SWOS_AUDIO_DISABLED
    (void)song_data; (void)song_size; (void)ins_data; (void)ins_size;
    return 0;
#endif
    SDL_LockMutex(s_mtx);

    int ok = RJP1_Load(sfx_tgt(), song_data, song_size, ins_data, ins_size);
    if (!ok) {
        fprintf(stderr, "[audio] SFX RJP1_Load failed (song_size=%d ins_size=%d)\n",
                song_size, ins_size);
        sfx_set_act(0);
        SDL_UnlockMutex(s_mtx);
        return -1;
    }

    /* SFX bank stays "active" so RJP1_HandleTick runs each frame and
     * advances envelopes / period modulation for any in-flight sample.
     * Without this the player ticks but never plays — even after a
     * trigger sets channel.active=1, play_channel only runs when
     * RJP1_HandleTick is invoked, which is gated on the active flag. */
    sfx_set_act(1);
    if (!s_sfx_use_second) {
        s_loop_song = 0;   /* shared player: SFX is trigger-driven, never auto-restarted.
                            * With the 2nd player s_loop_song belongs to the MUSIC player
                            * (e.g. HERO's loop) — must not clobber it here. */
        /* Drop residual queued audio so the next mix tick starts clean. With the
         * 2nd player the music is mid-flight in the shared output queue — leave it. */
        if (s_audio_dev)
            SDL_ClearQueuedAudio(s_audio_dev);
    }

    SDL_UnlockMutex(s_mtx);
    return 0;
}

/* Arcade attract suppresses SFX triggers so the HERO attract song isn't corrupted
 * by reel crowd/SFX events firing on the shared RJP1 player (they'd retrigger one
 * of HERO's own samples). Defined in arcade.c (SDL-only); 0 in the normal game. */
extern int _gArcadeSfxSuppressed;

/* Trigger an SFX by note id (the Amiga AudioChannelNoteOnOff path). */
void AudioSDL_TriggerSfxByNoteId(int channel, int note_id)
{
#if SWOS_AUDIO_DISABLED
    (void)channel; (void)note_id;
    return;
#endif
    if (_gArcadeSfxSuppressed) return;
    SWOS_SFX_LOG_TRIGGER_NOTE(channel, note_id);
    SDL_LockMutex(s_mtx);
    if (sfx_act())
        RJP1_StartSequenceByNoteId(sfx_tgt(), channel, note_id);
    SDL_UnlockMutex(s_mtx);
}

/* Trigger an SFX by wave id (the Amiga AudioChannelQueueNextWaveIfEnabled
 * path). Skips the note-record indirection. */
void AudioSDL_TriggerSfxByWaveId(int channel, int wave_id)
{
#if SWOS_AUDIO_DISABLED
    (void)channel; (void)wave_id;
    return;
#endif
    if (_gArcadeSfxSuppressed) return;
    SWOS_SFX_LOG_TRIGGER_WAVE(channel, wave_id);
    SDL_LockMutex(s_mtx);
    if (sfx_act())
        RJP1_StartSequenceByWaveId(sfx_tgt(), channel, wave_id);
    SDL_UnlockMutex(s_mtx);
}

/* Trigger an SFX preset (the Amiga AudioApplyPresetToChannels path).
 * Reads the 4-byte preset record (one noteId per channel) and fires
 * each non-zero entry on its corresponding channel via the noteId
 * sequence start. */
void AudioSDL_TriggerSfxPreset(int preset_id)
{
#if SWOS_AUDIO_DISABLED
    (void)preset_id;
    return;
#endif
    if (_gArcadeSfxSuppressed) return;
    SDL_LockMutex(s_mtx);
    if (sfx_act()) {
        RJP1_Player *p = sfx_tgt();
        uint8_t note_ids[4];
        RJP1_PresetEntry(p, preset_id, note_ids);
        SWOS_SFX_LOG_TRIGGER_PRESET(preset_id,
                                    note_ids[0], note_ids[1],
                                    note_ids[2], note_ids[3]);
        for (int ch = 0; ch < 4; ++ch) {
            if (note_ids[ch] != 0)
                RJP1_StartSequenceByNoteId(p, ch, note_ids[ch]);
        }
    }
    SDL_UnlockMutex(s_mtx);
}

void AudioSDL_SetMasterVolume(int vol_0_64)
{
    if (vol_0_64 < 0) vol_0_64 = 0;
    if (vol_0_64 > 64) vol_0_64 = 64;
    /* int write is atomic enough for a volume gate — no need to lock. */
    s_master_volume = vol_0_64;
}

/* Per-channel volume gate (Amiga 0..0x40). Routes to RJP1's per-channel
 * attenuation so the chant fade-in/out (and any other single-channel
 * volume control) actually scales the audible output, instead of
 * silently updating audioState.channelVolume[ch] only — the bug that
 * made SFX_CROWD_CHANT_2 appear to loop forever at full volume. */
void AudioSDL_SetChannelVolume(int channel, int vol_0_64)
{
#if SWOS_AUDIO_DISABLED
    (void)channel; (void)vol_0_64;
    return;
#endif
    /* When attract music (HERO) is playing, the reels are match replays whose
     * crowd-chant code (sfx_crowd.c) fades channels 0-3 in/out. The shared player
     * holds HERO, not the SFX bank, so those per-channel fades would mute HERO's
     * channels — the "samples cut out randomly" bug. The trigger path is already
     * suppressed; suppress this volume path too. (Off during an arcade match,
     * where the chant is wanted.) */
    if (_gArcadeSfxSuppressed) return;
    SDL_LockMutex(s_mtx);
    if (sfx_act())
        RJP1_SetChannelAttenuation(sfx_tgt(), channel, vol_0_64);
    SDL_UnlockMutex(s_mtx);
}
