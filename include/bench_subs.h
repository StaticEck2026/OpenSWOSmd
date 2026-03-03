#ifndef BENCH_SUBS_H
#define BENCH_SUBS_H

#include "game_types.h"

/* ---- BenchCheckControls ----
 * Per-frame substitution controls check.
 * Called from GameLoop each frame.
 */
void _BenchCheckControls(void);

/* ---- DrawBenchAndSubsMenu ----
 * Draws bench player sprites, coach, and subs/formation menus.
 * Called from GameLoop each frame when bench is active.
 */
void _DrawBenchAndSubsMenu(void);

/* --- Chunk 117: Formation/bench state --- */
void _ShowFormationMenu(void);
void _SubsBenchOff(void);

/* --- Chunk 153: Bench player pointer lookup --- */
uint16_t _GetBenchPlayerPointer(uint16_t index);
void _UpdatePlayerToBeSubstitutedPosition(void);

#endif
