/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#include "random.h"
#include "deterministic.h"
#include "game_types.h"

/* ---- External globals (all live in SWOS2.s) ---- */
extern uint8_t kRandomTable[];        /* 257-byte lookup table */
extern uint8_t seed;                  /* primary PRNG seed/counter (in code section) */
extern uint8_t randXorIndex;          /* primary XOR key index (adjacent to seed) */
extern uint8_t randXorKey;            /* primary XOR key */
extern uint8_t seed2;                 /* secondary PRNG seed/counter (BSS) */
extern uint8_t randXorIndex2;         /* secondary XOR key index (BSS) */
extern uint8_t randXorKey2;           /* secondary XOR key (BSS) */
extern uint8_t *savedRandStatePtr;    /* pointer into savedRandState buffer */
extern uint16_t panicReasonCode;
extern void _PanicHaltLoop(void) __attribute__((noreturn));

/* ================================================================
 * Rand  (SWOS2.s line 18116)
 *
 * Primary PRNG used during gameplay.
 * Returns 0-255 in d0.
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _Rand(void)
{
    uint16_t d0 = seed & 0xFFu;

    if (d0 == 0)
    {
        randXorIndex++;
        randXorKey = kRandomTable[randXorIndex & 0xFF];
    }

    d0 = (uint16_t)((kRandomTable[d0] ^ randXorKey) & 0xFFu);
    seed++;
    return d0;
}

/* ================================================================
 * Rand2  (SWOS2.s line 18067)
 *
 * Secondary PRNG (independent state from Rand).
 * Returns 0-255 in d0.
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _Rand2(void)
{
    uint16_t d0 = seed2 & 0xFFu;

    if (d0 == 0)
    {
        randXorIndex2++;
        randXorKey2 = kRandomTable[randXorIndex2 & 0xFF];
    }

    d0 = (uint16_t)((kRandomTable[d0] ^ randXorKey2) & 0xFFu);
    seed2++;
    return d0;
}

/* ================================================================
 * GetRandomWord  (SWOS2.s line 18053)
 *
 * Combines two Rand2 calls to produce a full 16-bit random value.
 * Low byte from first call, high byte from second call.
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _GetRandomWord(void)
{
    uint16_t lo = _Rand2();
    uint16_t hi = _Rand2();
    return (uint16_t)((hi << 8) | lo);
}

/* ================================================================
 * SaveRand2State  (SWOS2.s line 18008)
 *
 * Pushes secondary PRNG state (3 bytes: seed2, randXorIndex2,
 * randXorKey2) onto the savedRandState stack-style buffer.
 * Panics if the buffer overflows into kRandomTable.
 * ================================================================ */
__attribute__((used, externally_visible))
void _SaveRand2State(void)
{
    uint8_t *ptr = savedRandStatePtr;

    *ptr++ = seed2;
    *ptr++ = randXorIndex2;
    *ptr++ = randXorKey2;
    savedRandStatePtr = ptr;

    /* Original asm: cmpa.l #kRandomTable,a0 / bcs.s (skip panic) */
    if ((uint32_t)ptr >= (uint32_t)kRandomTable)
    {
        panicReasonCode = 0x9793;
        _PanicHaltLoop();
    }
}

/* ================================================================
 * RestoreRand2State  (SWOS2.s line 18025)
 *
 * Pops secondary PRNG state (3 bytes) from the savedRandState
 * stack-style buffer, restoring seed2/randXorIndex2/randXorKey2.
 * ================================================================ */
__attribute__((used, externally_visible))
void _RestoreRand2State(void)
{
    uint8_t *ptr = savedRandStatePtr;

    ptr--;
    randXorKey2 = *ptr;
    ptr--;
    randXorIndex2 = *ptr;
    ptr--;
    seed2 = *ptr;
    savedRandStatePtr = ptr;
}

/* ================================================================
 * Deterministic Mode Initialization
 *
 * Seeds both PRNGs from DETERMINISTIC_SEED so every game sequence
 * is reproducible. Called from _start() before any game logic.
 * ================================================================ */
#ifdef DETERMINISTIC_MODE
extern uint16_t randomSeed;
extern uint8_t  newRandomSeedCopy;
extern uint8_t  savedRandState[];
uint16_t deterministicTick = 0;

__attribute__((used, externally_visible))
void _InitDeterministicMode(void)
{
    /* Primary PRNG */
    seed = (uint8_t)(DETERMINISTIC_SEED & 0xFF);
    randXorIndex = (uint8_t)((DETERMINISTIC_SEED >> 8) & 0xFF);
    randXorKey = kRandomTable[randXorIndex];

    /* Secondary PRNG */
    seed2 = (uint8_t)((DETERMINISTIC_SEED >> 4) & 0xFF);
    randXorIndex2 = (uint8_t)((DETERMINISTIC_SEED >> 12) & 0xFF);
    randXorKey2 = kRandomTable[randXorIndex2];

    /* Seeding helpers */
    randomSeed = DETERMINISTIC_SEED;
    newRandomSeedCopy = (uint8_t)(DETERMINISTIC_SEED & 0xFF);

    /* Reset save stack */
    savedRandStatePtr = savedRandState;

    deterministicTick = 0;

#ifdef DETERMINISTIC_DUMP_FRAME
    /* Reset frame log on each init — armed from InitGameRestoreTeams */
    rngLogCount = 0;
    rngDumpFrameCounter = 0;
    rngDumpActive = 1;
#endif
}
#endif

/* ================================================================
 * RNG Dump — writes log to DH0:rng_dump.bin after N frames
 * ================================================================ */
#ifdef DETERMINISTIC_DUMP_FRAME
uint16_t rngLogCount = 0;
uint8_t  rngLogRng[RNG_LOG_MAX];
uint8_t  rngLogVal[RNG_LOG_MAX];
uint16_t rngDumpFrameCounter = 0;
uint16_t rngDumpActive = 0;

/* Per-frame state log: 8 bytes per frame (tick, ball x/y/z integer parts) */
#define FRAME_LOG_MAX DETERMINISTIC_DUMP_FRAME
static int16_t frameLog[FRAME_LOG_MAX * 4];  /* 4 int16_t per frame */
static uint16_t frameLogCount = 0;

extern void _PanicHaltLoop(void) __attribute__((noreturn));

/* Inline DOS calls for file dump (dosBase from Resload) */
extern uint32_t S_DosBase;
extern uint32_t S_VBR;
extern uint32_t S_SavedInt2;
extern void Int2KeyboardHandler(void);
extern Sprite ballSprite;
extern uint16_t currentGameTick;
#define LEVEL_2_VEC 0x68
#define MODE_NEWFILE 1006

static inline uint32_t dumpDosOpen(const uint8_t *name, uint32_t mode) {
    register void *_a6 __asm("a6") = (void *)S_DosBase;
    register const uint8_t *_d1 __asm("d1") = name;
    register uint32_t _d2 __asm("d2") = mode;
    register uint32_t _d0 __asm("d0");
    __asm volatile ("jsr -30(%%a6)" : "=r"(_d0), "+r"(_d1), "+r"(_d2) : "r"(_a6) : "a0","a1","memory");
    return _d0;
}
static inline int32_t dumpDosWrite(uint32_t handle, const void *buf, uint32_t len) {
    register void *_a6 __asm("a6") = (void *)S_DosBase;
    register uint32_t _d1 __asm("d1") = handle;
    register const void *_d2 __asm("d2") = buf;
    register uint32_t _d3 __asm("d3") = len;
    register int32_t _d0 __asm("d0");
    __asm volatile ("jsr -48(%%a6)" : "=r"(_d0), "+r"(_d1), "+r"(_d2), "+r"(_d3) : "r"(_a6) : "a0","a1","memory");
    return _d0;
}
static inline void dumpDosClose(uint32_t handle) {
    register void *_a6 __asm("a6") = (void *)S_DosBase;
    register uint32_t _d1 __asm("d1") = handle;
    register uint32_t _d0 __asm("d0");
    __asm volatile ("jsr -36(%%a6)" : "=r"(_d0), "+r"(_d1) : "r"(_a6) : "a0","a1","memory");
}

__attribute__((used, externally_visible))
void _RngDumpCheck(void)
{
    if (!rngDumpActive)
        return;

    /* Log per-frame state: tick, ball_x, sprite_count, ball_y */
    if (frameLogCount < FRAME_LOG_MAX) {
        extern uint16_t numSpritesToRender;
        uint16_t idx = frameLogCount * 4;
        frameLog[idx + 0] = (int16_t)currentGameTick;
        frameLog[idx + 1] = ballSprite.x.s.i;
        frameLog[idx + 2] = (int16_t)numSpritesToRender;
        frameLog[idx + 3] = ballSprite.y.s.i;
        frameLogCount++;
    }

    rngDumpFrameCounter++;
    if (rngDumpFrameCounter < DETERMINISTIC_DUMP_FRAME)
        return;

    /* Swap int2 to OS handler before DOS calls */
    volatile uint32_t *vec = (volatile uint32_t *)(S_VBR + LEVEL_2_VEC);
    *vec = S_SavedInt2;

    /* Write frame state dump */
    uint32_t fh = dumpDosOpen((const uint8_t *)"PROGDIR:state_dump.bin", MODE_NEWFILE);
    if (fh != 0) {
        dumpDosWrite(fh, frameLog, frameLogCount * 8);
        dumpDosClose(fh);
    }

    /* Restore game int2 handler */
    *vec = (uint32_t)Int2KeyboardHandler;

    /* Halt so user can collect the file */
    panicReasonCode = 0xE000 | (frameLogCount & 0xFFF);
    _PanicHaltLoop();
}
#endif
