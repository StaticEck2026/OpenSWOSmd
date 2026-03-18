# Deterministic Gameplay with Seeded RNG

## Goal

Make every game run deterministically using a seed, so the same seed always produces the same game. This enables comparing a game sequence from the original ASM version with the C port to verify identical behavior.

## RNG Architecture

SWOS uses two independent 8-bit PRNGs, both based on a 257-byte XOR lookup table (`kRandomTable`):

### Primary PRNG: `_Rand()`
- **State:** `seed` (uint8_t counter), `randXorIndex` (uint8_t), `randXorKey` (uint8_t)
- **Location:** `seed`, `randXorIndex`, `randXorKey` in DATA section (SWOS2.s line 21643-21648)
- **Usage:** Core gameplay physics, ball movement, player AI during matches
- **Returns:** 0-255

### Secondary PRNG: `_Rand2()`
- **State:** `seed2` (uint8_t), `randXorIndex2` (uint8_t), `randXorKey2` (uint8_t)
- **Location:** BSS section (SWOS2.s line 129247-129252)
- **Usage:** Career mode events, team selection, match setup, skill adjustments
- **Returns:** 0-255
- **Has save/restore:** `_SaveRand2State()` / `_RestoreRand2State()` for nested randomization

### Seeding Function: `_Randomize2(uint16_t val)`
```c
void _Randomize2(uint16_t val) {
    uint16_t result = val ^ randomSeed;
    seed2 = (uint8_t)(result >> 8);
    randXorIndex2 = (uint8_t)(result & 0xFF);
    randXorKey2 = kRandomTable[randXorIndex2];
}
```

### Per-Match Random: `gameRandValue`
- Set once at match start: `gameRandValue = _Rand();`
- XOR'd with per-player random values for skill variation
- Location: `match_setup.c` line 762

## Deterministic Mode Configuration

### C Version (`include/deterministic.h`)
```c
#define DETERMINISTIC_MODE
#define DETERMINISTIC_SEED 0x1234
#define DETERMINISTIC_DUMP_FRAME 1500    /* ~30 sec at 50fps */
```

### ASM Version (top of `SWOS2.s`)
```asm
DETERMINISTIC_MODE = 1
DETERMINISTIC_SEED = $1234
DETERMINISTIC_DUMP_FRAME = 1500
```

Both must use identical values.

## RNG Dump System

After `DETERMINISTIC_DUMP_FRAME` game frames, both versions write `rng_dump.bin` containing all logged RNG calls as 2-byte pairs: `(rng_id, value)` where `rng_id` 0 = Rand, 1 = Rand2.

### How it works
1. `rngDumpActive` flag is armed in `InitGameRestoreTeams` (match start)
2. `rngLogCount`, `rngDumpFrameCounter` are reset at the same point
3. Every `Rand()`/`Rand2()` call logs `(rng_id, result)` into `rngLogRng[]`/`rngLogVal[]` arrays
4. Each game frame, `RngDumpCheck` increments the frame counter
5. After N frames, the log is written to disk and the game halts with a panic code

### File paths
- **ASM version:** `DH0:data/rng_dump.bin`
- **C version:** `PROGDIR:rng_dump.bin` (resolves to `dh1/data/rng_dump.bin` with the Amiga C/C++ extension)

### Log buffers
- `rngLogRng[32767]` and `rngLogVal[32767]` — in Resload.s BSS (ASM version) or C globals (C version)
- Placed outside the 0x5555 BSS fill range to avoid corruption

## Issues Found and Fixed

### Issue 1: Keyboard Interrupt Crash During DOS File Write
**Symptom:** ASM version crashed in `Int2KeyboardHandler` when `RngDumpWrite` called DOS `Open()`.

**Root cause:** The game installs custom interrupt handlers. Calling DOS functions while the game's keyboard interrupt handler is active can crash because DOS internally triggers interrupts that the game handler misinterprets.

**Fix:** Swap the Level 2 interrupt vector back to the OS handler before DOS calls, restore after:
```asm
; Before DOS calls:
move.l  (S_VBR).l,a0
move.l  (S_SavedInt2).l,LEVEL_2_INTERRUPT_AUTOVECTOR(a0)
; ... DOS calls ...
; After DOS calls:
move.l  (S_VBR).l,a0
move.l  #Int2KeyboardHandler,LEVEL_2_INTERRUPT_AUTOVECTOR(a0)
```
This matches the pattern used by all file operations in `Resload.s` (`_GetFileSize`, `_LoadFile`, `_Deletefile`, etc.).

### Issue 2: `rngDumpActive` Never Armed
**Symptom:** Dump file created but empty — `RngDumpCheck` always returned early.

**Root cause:** `rngDumpActive` was initialized to 0 and never set to 1.

**Fix:** Arm `rngDumpActive` in `InitGameRestoreTeams` after `INIT_DETERMINISTIC_RNG`:
- ASM: `move.w #1,(rngDumpActive).l` in `IF DETERMINISTIC_DUMP_FRAME` block
- C: `rngDumpActive = 1` in `_InitDeterministicMode()`

### Issue 3: `DETERMINISTIC_DUMP_FRAME` Set to 1
**Symptom:** Dump triggered after just 1 frame — no useful gameplay data.

**Fix:** Set to 1500 (~30 seconds at 50fps) in both versions.

### Issue 4: C Version File Path Wrong
**Symptom:** File written to `dh0/data/data/rng_dump.bin` (doubled path).

**Root cause:** Used `PROGDIR:data/rng_dump.bin` but `PROGDIR:` already resolves to the binary's directory (`dh1/data/`).

**Fix:** Changed to `PROGDIR:rng_dump.bin`.

### Issue 5: Dual Rand/Rand2 Implementations in C Build (CRITICAL)
**Symptom:** RNG dumps diverged at entry 9 — C version's Rand2 produced different values despite identical seeding.

**Root cause:** The C build had TWO independent `Rand2` implementations:
1. **ASM `Rand2`** in `data/SWOS2.s` — full implementation, called by all remaining asm code (`jsr Rand2`), **no RNG logging**
2. **C `_Rand2()`** in `src/random.c` — called by ported C code, **with RNG logging**

The asm `Rand2` advanced the seed state without being logged, so the C dump was missing entries AND the shared seed state diverged from expectations.

Same problem existed for `Rand`.

**Fix:** Replaced the full asm implementations with forwarding stubs:
```asm
Rand2:
    movem.l d1/a0,-(sp)    ; preserve scratch regs (asm callers expect this)
    jsr     _Rand2          ; call C version (which logs)
    movem.l (sp)+,d1/a0
    rts

Rand:
    movem.l d1/a0,-(sp)
    jsr     _Rand
    movem.l (sp)+,d1/a0
    rts
```

Now ALL Rand/Rand2 calls (from both C and asm) go through the C functions which handle logging.

## Sources of Non-Determinism

### 1. VBlank Counter (`currentGameTick` / `currentTick`) — PRIMARY SOURCE
- Incremented every VBlank interrupt (~20ms at 50Hz PAL)
- Used to seed `randomSeed` in `_InitCareerVariables()`: `randomSeed = currentTick`
- Also used in DIY competition setup functions
- **This is the only hardware entropy source for RNG**
- **Deterministic mode fix:** Reset to 0 in `InitGameRestoreTeams`, seed from `DETERMINISTIC_SEED`

### 2. Player Input (Joystick/Mouse)
- Read from hardware registers `HW_JOY0DAT`, `HW_JOY1DAT`, `HW_CIAA_PRA`
- NOT used for RNG seeding
- Affects game state through player actions (movement, passing, shooting)
- For SIMULATED matches (VIEW RESULT), no input is needed — fully deterministic once seeded

### 3. Timing-Dependent Branches (`currentGameTick`)
- `currentGameTick & N` used in some gameplay decisions (e.g., goal scoring chances, player state changes)
- Incremented by VBlank interrupt handler — hardware-driven, NOT deterministic
- **Deterministic mode fix:** Force `currentGameTick` to increment by exactly 1 per game frame via `deterministicTick` counter, overriding the hardware-driven value after each VBlank sync

### Issue 6: `currentGameTick` Driven by Hardware Timer
**Symptom:** Ball positions matched for 525 frames, then diverged. The `currentGameTick` value drifted by 1 tick at frame 359, then accumulated to hundreds of ticks difference.

**Root cause:** `currentGameTick` is incremented by the VBlank interrupt handler at hardware rate (~50Hz). The game loop reads it after waiting for VBlank. If the C version's frame processing takes slightly different time than the ASM version, the tick count read after each VBlank differs. Since gameplay code uses `currentGameTick & N` for branching decisions, a 1-tick difference cascades into completely different game states.

**Fix:** In deterministic mode, both versions now force `currentGameTick` to increment by exactly 1 per game frame:
- C version: `deterministicTick++; currentGameTick = deterministicTick;` after `waitVBlankSync()`
- ASM version: `addq.w #1,(deterministicTick).l; move.w (deterministicTick).l,(currentGameTick).l` after VBlank sync
- `deterministicTick` is reset to 0 in `InitGameRestoreTeams` / `_InitDeterministicMode()`

## Phase 2: Per-Frame State Dump

After confirming RNG sequences match, we switched to dumping per-frame gameplay state to find remaining divergences.

### State Dump Format
Each frame: 8 bytes (big-endian int16_t):
- `currentGameTick` (2 bytes)
- `ballSprite.x.s.i` (2 bytes) — ball X position integer part
- `ballSprite.y.s.i` (2 bytes) — ball Y position integer part
- `ballSprite.z.s.i` (2 bytes) — ball Z position integer part

### File Paths
- **ASM version:** `DH0:data/state_dump.bin`
- **C version:** `PROGDIR:state_dump.bin`

1500 frames × 8 bytes = 12KB per dump.

## Verification Protocol

1. Build both versions with `DETERMINISTIC_MODE` enabled and same `DETERMINISTIC_SEED`
2. Start a career with the same team
3. Press VIEW RESULT to simulate matches (no player input needed)
4. Both versions write `state_dump.bin` after 1500 frames and halt
5. Compare with Python: `struct.unpack('>hhhh', ...)` per 8-byte frame

## Bugs Found via Deterministic Comparison

### Bug: `playerIndex` Not Remapped in `_InitInGamePlayer` (game_init.c)
**Symptom:** RNG Randomize2 seeds for players 1-10 differed between ASM and C. Player 0 (goalkeeper) matched.

**Root cause:** In `_InitInGameTeam` (game_init.c), `_GetPlayerAtIndex()` remaps the ordinal (0-15) to the actual file index through position tables. In ASM, `GetPlayerAtIndex` modifies register d1 in-place to return the file index. In C, the return value is the pointer, and the file index is stored as a side-effect in `_cGetPlayerAtIndexFileIdx`. The C code was passing the original ordinal to `_InitInGamePlayer` instead of the remapped file index.

**Before:** `_InitInGamePlayer(d1, d2, playerBase, pi);` — d1 = ordinal (0, 1, 2, 3...)
**After:** `d1 = _cGetPlayerAtIndexFileIdx; _InitInGamePlayer(d1, d2, playerBase, pi);` — d1 = remapped file index (0, 2, 4, 8, 3...)

**Impact:** Every player's `playerIndex` field in the TeamGame was wrong, causing `_ApplyTeamTactics` to look up wrong player data for skill adjustments, and every subsequent player-related lookup to access wrong data.

## Files Modified

| File | Change |
|------|--------|
| `include/deterministic.h` | Defines, dump frame count |
| `src/random.c` | `_InitDeterministicMode()`, `_RngDumpCheck()` (now dumps per-frame state), `deterministicTick` |
| `src/match_setup.c` | Arms deterministic mode in `_InitGameRestoreTeams()` |
| `src/game_loop.c` | Calls `_RngDumpCheck()` each frame, forces `currentGameTick` in deterministic mode |
| `src/game_init.c` | Fixed `playerIndex` bug — use `_cGetPlayerAtIndexFileIdx` after `_GetPlayerAtIndex()` |
| `data/SWOS2.s` | `Rand`/`Rand2`/`SaveRand2State`/`RestoreRand2State`/`Randomize2` forwarding stubs to C |
| ASM version `SWOS2.s` | `RngDumpCheck` → `StateDumpWrite`, `deterministicTick`, forced tick increment |
| ASM version `Resload.s` | `frameLogBuf` BSS buffer (replaces old rngLog arrays) |

## Scope Limitations

- **Simulated matches (VIEW RESULT):** Fully deterministic with seed + forced tick — no input needed
- **Played matches:** Require input recording/replay for determinism (future work)
- **Menu navigation timing:** Affects when `currentTick` is sampled. For comparison, go straight to VIEW RESULT without delays
