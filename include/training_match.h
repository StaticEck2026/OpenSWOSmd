/* training_match.h — Training match state save/restore. */
#pragma once
#include "game_types.h"

/* --- State save/restore --- */
void _TrainingMatch_SaveSelectedPlayerId(uint16_t value);
void _TrainingMatch_RemoveSavedPlayerId(uint16_t value);
void _TrainingMatch_SaveLastMatchState(void);
void _TrainingMatch_RestoreLastMatchState(void);
void _TrainingMatch_RestoreSavedTeamSelectionState(void);
void _TrainingMatch_SaveTeamSelectionState(void);
int16_t _CheckIfTeamIsChanged(void);
