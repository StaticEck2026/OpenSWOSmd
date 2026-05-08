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
#if SWOS_AUDIO_DISABLED
    /* Audio disabled at compile time — return success so platform init
     * continues; s_audio_dev stays 0 and AudioSDL_Tick early-returns. */
    return 0;
#endif
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
    const int bytes_per_tick = (freq * 2) / 50;
    if (bytes_per_tick > (int)sizeof(s_mix_buf)) return;   /* safety */

    Uint32 target_bytes = (Uint32)freq * 2 * QUEUE_TARGET_MS / 1000;
    int    have_audio   = 0;     /* 1 if we filled s_mix_buf with music */

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
        have_audio = 1;
    } else {
        /* Queue silence so the driver keeps ticking forward and
         * SDL_GetQueuedAudioSize returns sensible values when music
         * starts. */
        SDL_memset(s_mix_buf, 0, (size_t)bytes_per_tick);
    }

    /* Enqueue inside the lock so a concurrent caller's queue re-check
     * sees the new size. SDL_QueueAudio is internally locked; nesting
     * our s_mtx around it is safe. */
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

    SDL_UnlockMutex(s_mtx);
    (void)have_audio;
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

    int ok = RJP1_Load(&s_player, song_data, song_size, ins_data, ins_size);
    if (!ok) {
        fprintf(stderr, "[audio] SFX RJP1_Load failed (song_size=%d ins_size=%d)\n",
                song_size, ins_size);
        s_player_active = 0;
        SDL_UnlockMutex(s_mtx);
        return -1;
    }

    /* SFX bank stays "active" so RJP1_HandleTick runs each frame and
     * advances envelopes / period modulation for any in-flight sample.
     * Without this the player ticks but never plays — even after a
     * trigger sets channel.active=1, play_channel only runs when
     * RJP1_HandleTick is invoked, which is gated on s_player_active. */
    s_player_active = 1;

    /* Drop residual queued audio from any prior song so the next mix
     * tick starts clean (otherwise the first SFX is preceded by the
     * tail of whatever music was buffered). */
    if (s_audio_dev)
        SDL_ClearQueuedAudio(s_audio_dev);

    SDL_UnlockMutex(s_mtx);
    return 0;
}

/* Trigger an SFX by note id (the Amiga _AudioChannelNoteOnOff path). */
void AudioSDL_TriggerSfxByNoteId(int channel, int note_id)
{
#if SWOS_AUDIO_DISABLED
    (void)channel; (void)note_id;
    return;
#endif
    SWOS_SFX_LOG_TRIGGER_NOTE(channel, note_id);
    SDL_LockMutex(s_mtx);
    if (s_player_active)
        RJP1_StartSequenceByNoteId(&s_player, channel, note_id);
    SDL_UnlockMutex(s_mtx);
}

/* Trigger an SFX by wave id (the Amiga _AudioChannelQueueNextWaveIfEnabled
 * path). Skips the note-record indirection. */
void AudioSDL_TriggerSfxByWaveId(int channel, int wave_id)
{
#if SWOS_AUDIO_DISABLED
    (void)channel; (void)wave_id;
    return;
#endif
    SWOS_SFX_LOG_TRIGGER_WAVE(channel, wave_id);
    SDL_LockMutex(s_mtx);
    if (s_player_active)
        RJP1_StartSequenceByWaveId(&s_player, channel, wave_id);
    SDL_UnlockMutex(s_mtx);
}

/* Trigger an SFX preset (the Amiga _AudioApplyPresetToChannels path).
 * Reads the 4-byte preset record (one noteId per channel) and fires
 * each non-zero entry on its corresponding channel via the noteId
 * sequence start. */
void AudioSDL_TriggerSfxPreset(int preset_id)
{
#if SWOS_AUDIO_DISABLED
    (void)preset_id;
    return;
#endif
    SDL_LockMutex(s_mtx);
    if (s_player_active) {
        uint8_t note_ids[4];
        RJP1_PresetEntry(&s_player, preset_id, note_ids);
        SWOS_SFX_LOG_TRIGGER_PRESET(preset_id,
                                    note_ids[0], note_ids[1],
                                    note_ids[2], note_ids[3]);
        for (int ch = 0; ch < 4; ++ch) {
            if (note_ids[ch] != 0)
                RJP1_StartSequenceByNoteId(&s_player, ch, note_ids[ch]);
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
    SDL_LockMutex(s_mtx);
    if (s_player_active)
        RJP1_SetChannelAttenuation(&s_player, channel, vol_0_64);
    SDL_UnlockMutex(s_mtx);
}
