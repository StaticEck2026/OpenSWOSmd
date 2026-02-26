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

#endif
