#ifndef GAME_INIT_H
#define GAME_INIT_H

#include "game_types.h"

void _InitTeamsData(void);
void _InitGameSaveTeams(TeamGame *leftTeam, TeamGame *rightTeam);

/* --- Player init & post-match utilities (Chunk 103) --- */
void _UpdatePlayerInjuries(TeamGame *tg, uint8_t *teamFileBase);
void _AccumulateMatchGoalsIntoTeamFile(TeamGame *tg, uint8_t *teamFileBase,
                                        uint16_t statOffset);
void _SetPlayerNameAndFace(uint8_t *playerBase, PlayerInfo *pi);
void _InitInGamePlayer(uint16_t playerIndex, uint16_t playerSlot,
                        uint8_t *playerBase, PlayerInfo *pi);
void _ApplyPlayerStatusAfterSim(TeamGame *tg, uint8_t *teamFileBase);
void _UpdateCareerPlayerStatsAfterMatch(TeamGame *tg, uint8_t *teamFileBase);

/* --- Full team game structure init (Chunk 104) --- */
void _InitInGameTeamStructure(uint8_t *teamFileBase, TeamGame *tg,
                                int16_t *avgPricePct);

#endif
