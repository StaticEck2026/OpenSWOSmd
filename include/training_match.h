/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* training_match.h — Training match state save/restore. */
#pragma once
#include "game_types.h"

/* --- State save/restore --- */
void TrainingMatch_SaveSelectedPlayerId(uint16_t value);
void TrainingMatch_RemoveSavedPlayerId(uint16_t value);
void TrainingMatch_SaveLastMatchState(void);
void TrainingMatch_RestoreLastMatchState(void);
void TrainingMatch_RestoreSavedTeamSelectionState(void);
void TrainingMatch_SaveTeamSelectionState(void);
int16_t CheckIfTeamIsChanged(void);

/* --- Sub swap candidates and trial players (Chunk 176) --- */
void TrainingMatch_ApplyUseMatchTeamAndSwapSubs(void);
void TrainingMatch_TryAddTrialPlayerAtPosition(uint16_t position);
void TrainingMatch_AddTrialPlayersForNewSeason(void);

/* --- Chunk 200: PlayTrainingGame + BuildCurrentTeam --- */
void TrainingMatch_BuildCurrentTeamFromCareerTeam(void);
void PlayTrainingGame(void);
