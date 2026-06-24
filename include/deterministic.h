/*
 * Deterministic Gameplay Mode
 *
 * When DETERMINISTIC_MODE is defined, all RNG is seeded from a fixed
 * value instead of hardware timers. This makes every game sequence
 * reproducible given the same seed, enabling comparison between the
 * original ASM and C port versions.
 *
 * Usage:
 *   - Uncomment DETERMINISTIC_MODE to enable
 *   - Change DETERMINISTIC_SEED to get different (but reproducible) sequences
 *   - Both C and ASM versions must use the same seed value
 *   - Compare by running VIEW RESULT (simulated matches need no input)
 *
 * RNG Dump:
 *   - Define DETERMINISTIC_DUMP_FRAME to dump RNG state after N frames
 *   - Both versions write to DH0:rng_dump.bin (4 bytes per call: 2-byte counter + 1-byte result + 1-byte which_rng)
 *   - Compare the binary dumps between ASM and C to find first divergence
 */

#ifndef DETERMINISTIC_H
#define DETERMINISTIC_H

/* Uncomment to enable deterministic mode */
//#define DETERMINISTIC_MODE

/* Seed value — change for different reproducible sequences */
#define DETERMINISTIC_SEED 0x1234

/* Uncomment and set frame count to dump RNG calls after N game frames.
 * After DETERMINISTIC_DUMP_FRAME frames, the game halts and writes
 * DH0:rng_dump.bin containing all RNG calls made during those frames. */
//#define DETERMINISTIC_DUMP_FRAME 1500

/* Maximum number of sprites enqueued to render stacks per frame.
 * Caps blitter work so C and ASM versions have matching frame timing.
 * Set to 0 to disable the limit. */
#define DETERMINISTIC_SPRITE_LIMIT 0

#ifdef DETERMINISTIC_MODE
void InitDeterministicMode(void);
#endif

#ifdef DETERMINISTIC_DUMP_FRAME
/* RNG log entry: which RNG (0=Rand, 1=Rand2), result value */
#define RNG_LOG_MAX 32767
extern uint16_t rngLogCount;
extern uint8_t  rngLogRng[RNG_LOG_MAX];
extern uint8_t  rngLogVal[RNG_LOG_MAX];
extern uint16_t rngDumpFrameCounter;
extern uint16_t rngDumpActive;

#define RNG_LOG(which, val) do { \
    if (rngLogCount < RNG_LOG_MAX) { \
        rngLogRng[rngLogCount] = (which); \
        rngLogVal[rngLogCount] = (val); \
        rngLogCount++; \
    } \
} while(0)

void RngDumpCheck(void);
#else
#define RNG_LOG(which, val)
#endif

#endif /* DETERMINISTIC_H */
