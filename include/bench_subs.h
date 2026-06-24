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
void BenchCheckControls(void);

/* ---- DrawBenchAndSubsMenu ----
 * Draws bench player sprites, coach, and subs/formation menus.
 * Called from GameLoop each frame when bench is active.
 */
void DrawBenchAndSubsMenu(void);

/* --- Chunk 117: Formation/bench state --- */
void ShowFormationMenu(void);
void SubsBenchOff(void);

/* --- Chunk 153: Bench player pointer lookup --- */
uint16_t GetBenchPlayerPointer(uint16_t index);
void UpdatePlayerToBeSubstitutedPosition(void);

/* --- Chunk 154: Substitution eligibility & initial selection --- */
uint16_t IsPlayerOkToSubstitute(void);
void FindInitialPlayerToBeSubstituted(void);

/* --- Chunk 155: Formation movement & player data swap --- */
void HandleFormationMenuMovement(void);
void SwapPlayerNameData(uint16_t enterIndex, uint16_t leaveIndex);

/* --- Chunk 156: Training swap, leave subs, throw-in/keeper check --- */
void TrainingSwapBenchTeams(void);
void LeaveSubsMenu(void);
void CheckForThrowInAndGoalkeepersBall(void);

/* --- Chunk 157: Bench menu handlers & substitute scanning --- */
int16_t IsThisPlayerOkToSelect(void);
void FirePressedInSubsMenu(void);
void HandleFormationMenuControls(void);
void UpdateSubstitutePlayerIndices(void);
void CallUpdateSubstitutePlayerIndices(void);
void state3_handle_fire_press(void);

/* --- Chunk 158: Substitution confirm, execution & mark/swap --- */
void SelectPlayerToSubstituteMenuHandler(void);
void SubstitutePlayer(void);
void MarkPlayersMenuHandler(void);

/* --- Chunk 159: Main bench menu dispatcher --- */
void BenchMenuHandleControls(void);

#endif
