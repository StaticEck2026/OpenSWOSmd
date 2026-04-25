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

#include <SDL.h>
#include <stdio.h>
#include <string.h>

#define AUDIO_SAMPLE_RATE 44100

static SDL_AudioDeviceID s_audio_dev;
static SDL_AudioSpec     s_audio_spec;
static SDL_mutex        *s_mtx;

static RJP1_Player       s_player;
static int               s_player_active;   /* 1 = RJP1_Load succeeded and a song is selected */
static int               s_master_volume;   /* 0..64, scales final output */

/* Queue-mode mix scratch. Sized for one game tick (50 Hz) of stereo S8
 * at the highest plausible negotiated rate (48 kHz) plus a little slack. */
static int8_t            s_mix_buf[48000 * 2 / 50 + 128];

/* Target queue fill we try to maintain ahead of the playback cursor.
 * 80 ms gives the driver enough cushion to ride out ~4 game frames of
 * main-thread stalls without audibly underrunning, without adding
 * noticeable latency between music events and what you hear. */
#define QUEUE_TARGET_MS  80

int AudioSDL_Init(void)
{
    s_mtx = SDL_CreateMutex();

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq     = AUDIO_SAMPLE_RATE;
    want.format   = AUDIO_S8;
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
    return 0;
}

void AudioSDL_Shutdown(void)
{
    if (s_audio_dev) {
        SDL_CloseAudioDevice(s_audio_dev);
        s_audio_dev = 0;
    }
    if (s_mtx) { SDL_DestroyMutex(s_mtx); s_mtx = NULL; }
}

/* Produce audio from the main thread and queue it into SDL. Called once
 * per rendered frame (50 Hz) from Platform_RenderFrame.
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
 * stalls without underrunning. */
void AudioSDL_Tick(void)
{
    if (!s_audio_dev) return;

    const int freq = s_audio_spec.freq ? s_audio_spec.freq : AUDIO_SAMPLE_RATE;

    /* How much audio is already queued, in bytes? */
    Uint32 queued_bytes  = SDL_GetQueuedAudioSize(s_audio_dev);
    Uint32 target_bytes  = (Uint32)freq * 2 * QUEUE_TARGET_MS / 1000;

    if (queued_bytes >= target_bytes) return;   /* already buffered enough */

    /* Generate one game tick of audio (50 Hz == one RJP1 pattern step). */
    const int bytes_per_tick = (freq * 2) / 50;
    if (bytes_per_tick > (int)sizeof(s_mix_buf)) return;   /* safety */

    SDL_LockMutex(s_mtx);
    if (!s_player_active) {
        SDL_UnlockMutex(s_mtx);
        /* Still queue silence so the driver keeps ticking forward and
         * SDL_GetQueuedAudioSize returns sensible values when music
         * starts. */
        SDL_memset(s_mix_buf, 0, (size_t)bytes_per_tick);
    } else {
        SDL_memset(s_mix_buf, 0, (size_t)bytes_per_tick);
        RJP1_HandleTick(&s_player);
        RJP1_MixSamples(&s_player, freq, s_mix_buf, bytes_per_tick);

        /* Apply master volume (0..64) in place. */
        if (s_master_volume < 64) {
            int scale = s_master_volume;
            for (int i = 0; i < bytes_per_tick; ++i) {
                s_mix_buf[i] = (int8_t)(((int)s_mix_buf[i] * scale) / 64);
            }
        }
        SDL_UnlockMutex(s_mtx);
    }

    SDL_QueueAudio(s_audio_dev, s_mix_buf, (Uint32)bytes_per_tick);

    /* If we'd gotten WAY behind (e.g. a 2-second main-thread stall), we
     * may still be under target after just one tick's worth. Top up
     * with silence instead of trying to mix multiple game ticks in one
     * frame, so music stays time-aligned with the game state (a long
     * catch-up would desynchronise the menu volume fade tick). */
    queued_bytes = SDL_GetQueuedAudioSize(s_audio_dev);
    if (queued_bytes + bytes_per_tick < target_bytes) {
        SDL_memset(s_mix_buf, 0, (size_t)bytes_per_tick);
        SDL_QueueAudio(s_audio_dev, s_mix_buf, (Uint32)bytes_per_tick);
    }
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
    SDL_LockMutex(s_mtx);

    int ok = RJP1_Load(&s_player, song_data, song_size, ins_data, ins_size);
    if (!ok) {
        fprintf(stderr, "[audio] RJP1_Load failed (song_size=%d ins_size=%d)\n",
                song_size, ins_size);
        s_player_active = 0;
        SDL_UnlockMutex(s_mtx);
        return -1;
    }

    if (subsong <= 0) subsong = 1;
    RJP1_StartSong(&s_player, (uint8_t)subsong);
    s_player_active = 1;

    /* Drop whatever stale audio was queued so the new song starts
     * immediately instead of after the previous song's residual buffer. */
    if (s_audio_dev)
        SDL_ClearQueuedAudio(s_audio_dev);

    SDL_UnlockMutex(s_mtx);
    return 0;
}

void AudioSDL_StopSong(void)
{
    SDL_LockMutex(s_mtx);
    if (s_player_active)
        RJP1_StopAll(&s_player);
    s_player_active = 0;
    SDL_UnlockMutex(s_mtx);
}

void AudioSDL_SetMasterVolume(int vol_0_64)
{
    if (vol_0_64 < 0) vol_0_64 = 0;
    if (vol_0_64 > 64) vol_0_64 = 64;
    /* int write is atomic enough for a volume gate — no need to lock. */
    s_master_volume = vol_0_64;
}
