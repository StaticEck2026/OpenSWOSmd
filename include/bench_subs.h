/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
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

/* --- Chunk 157: Bench menu handlers & substitute scanning --- */
int16_t _IsThisPlayerOkToSelect(void);
void _FirePressedInSubsMenu(void);
void _HandleFormationMenuControls(void);
void _UpdateSubstitutePlayerIndices(void);
void _CallUpdateSubstitutePlayerIndices(void);
void _state3_handle_fire_press(void);

/* --- Chunk 158: Substitution confirm, execution & mark/swap --- */
void _SelectPlayerToSubstituteMenuHandler(void);
void _SubstitutePlayer(void);
void _MarkPlayersMenuHandler(void);

/* --- Chunk 159: Main bench menu dispatcher --- */
void _BenchMenuHandleControls(void);

#endif
