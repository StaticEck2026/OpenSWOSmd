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

/* --- InitializeInGameTeamsAndStartGame (Chunk 214) --- */
void _InitializeInGameTeamsAndStartGame(int16_t randomizePitch, int16_t minSubs,
    int16_t maxSubs, int16_t paramD3, int16_t paramD7,
    TeamFile *team1, TeamFile *team2);

/* --- SetupPlayers & InitAndPlayGame (Chunk 191) --- */
void _SetupPlayers(TeamFileEx *team1, TeamFileEx *team2, int16_t maxSubs,
                   int16_t excludeFlags, int16_t allowSquadChanges, int16_t mode);
void _InitAndPlayGame(TeamFileEx *team1, TeamFileEx *team2);

#endif
