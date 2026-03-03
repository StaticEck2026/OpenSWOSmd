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

/* --- Chunk 154: Substitution eligibility & initial selection --- */
uint16_t _IsPlayerOkToSubstitute(void);
void _FindInitialPlayerToBeSubstituted(void);

/* --- Chunk 155: Formation movement & player data swap --- */
void _HandleFormationMenuMovement(void);
void _SwapPlayerNameData(uint16_t enterIndex, uint16_t leaveIndex);

/* --- Chunk 156: Training swap, leave subs, throw-in/keeper check --- */
void _TrainingSwapBenchTeams(void);
void _LeaveSubsMenu(void);
void _CheckForThrowInAndGoalkeepersBall(void);

#endif
