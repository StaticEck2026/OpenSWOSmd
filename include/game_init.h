/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef GAME_INIT_H
#define GAME_INIT_H

#include "game_types.h"

void InitTeamsData(void);
void InitGameSaveTeams(TeamGame *leftTeam, TeamGame *rightTeam);

/* --- Player init & post-match utilities (Chunk 103) --- */
void UpdatePlayerInjuries(TeamGame *tg, TeamFileEx *teamFileBase);
void AccumulateMatchGoalsIntoTeamFile(TeamGame *tg, TeamFileEx *teamFileBase,
                                        uint16_t statOffset);
void SetPlayerNameAndFace(uint8_t *playerBase, PlayerInfo *pi);
void InitInGamePlayer(uint16_t playerIndex, uint16_t playerSlot,
                        uint8_t *playerBase, PlayerInfo *pi);
void ApplyPlayerStatusAfterSim(TeamGame *tg, TeamFileEx *teamFileBase);
void UpdateCareerPlayerStatsAfterMatch(TeamGame *tg, TeamFileEx *teamFileBase);

/* --- Full team game structure init (Chunk 104) --- */
void InitInGameTeamStructure(TeamFileEx *teamFileBase, TeamGame *tg,
                                int16_t *avgPricePct);

/* --- InitializeInGameTeamsAndStartGame (Chunk 214) --- */
void InitializeInGameTeamsAndStartGame(int16_t randomizePitch, int16_t minSubs,
    int16_t maxSubs, int16_t paramD3, int16_t paramD7,
    TeamFile *team1, TeamFile *team2);

/* --- SetupPlayers & InitAndPlayGame (Chunk 191) --- */
void SetupPlayers(TeamFileEx *team1, TeamFileEx *team2, int16_t maxSubs,
                   int16_t excludeFlags, int16_t allowSquadChanges, int16_t mode);
void InitAndPlayGame(TeamFileEx *team1, TeamFileEx *team2);

#endif
