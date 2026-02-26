#include "random.h"

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
