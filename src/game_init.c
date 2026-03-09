#include "game_init.h"
#include "game_types.h"
#include "play_match_menu.h"
#include "string_utils.h"

/* ---- External globals (in SWOS2.s code/BSS sections) ---- */
extern uint32_t currentScorer;
extern Sprite  *lastPlayerBeforeGoalkeeper;
extern uint16_t goalScored;
extern uint16_t runSlower;
extern uint16_t whichCard;
extern Sprite  *bookedPlayer;
extern uint16_t playerHadBall;
extern uint32_t lastKeeperPlayed;
extern TeamGeneralInfo *lastTeamPlayed;
extern Sprite  *lastPlayerPlayed;
extern uint16_t penalty;
extern uint16_t goalCameraMode;
extern uint16_t goalOut;
extern uint16_t gameNotInProgressCounterWriteOnly;
extern uint16_t fireBlocked;
extern TeamGeneralInfo *lastTeamPlayedBeforeBreak;
extern uint16_t stoppageTimerTotal;
extern uint16_t stoppageTimerActive;
extern uint16_t stoppageEventTimer;
extern uint16_t inGameCounter;
extern uint16_t gameStatePl;
extern uint16_t gameState;
extern uint16_t breakState;
extern uint16_t breakCameraMode;
extern uint16_t teamPlayingUp;
extern uint16_t topTeamPlayerNo;
extern uint16_t bottomTeamPlayerNo;
extern uint16_t topTeamCoachNo;
extern uint16_t bottomTeamCoachNo;
extern uint16_t pl1Coach;
extern uint16_t pl2Coach;
extern TeamGeneralInfo topTeamData;
extern TeamGeneralInfo bottomTeamData;
extern Sprite           *team1SpritesTable[];
extern Sprite           *team2SpritesTable[];
extern TeamGame         *topTeamPtr;
extern TeamGame         *bottomTeamPtr;
extern TeamStatisticsData team1StatsData;
extern TeamStatisticsData team2StatsData;
extern uint16_t pl1Tactics;
extern uint16_t pl2Tactics;

/* InitGameSaveTeams globals */
extern TeamGame  topTeamInGame;
extern TeamGame  bottomTeamInGame;
extern TeamGame  topTeamSaved;
extern TeamGame  bottomTeamSaved;
extern uint16_t pitchNumberAndType;
extern uint8_t  pl1Fire;
extern uint8_t  pl2Fire;
extern uint16_t paletteIndex;

/* Chunk 103 externs */
extern uint16_t trainingGame;
extern uint16_t plgD3Param;
extern uint16_t gameMaxSubstitutes;
extern TeamFileEx careerTeam;
extern uint8_t currentTeam[];

/* Chunk 104 externs */
extern uint16_t allPlayerTeamsEqual;
extern uint16_t avgSelectedTeamsPlayerPrice;
extern uint16_t gameType;
extern uint16_t team1SelectedPlayerIndex;
extern uint16_t team2SelectedPlayerIndex;
extern uint16_t selTeamSelectedPlayerPosition;
extern uint16_t currentTeamSelectedPlayerPosition;
extern uint8_t teamAName[];
extern uint8_t teamBName[];
extern TeamGame bottomTeamInGame;

/* ---- External function declarations ---- */
extern void _Player1StatusProc(void);
extern void _Player2StatusProc(void);
extern void _FadeOutToBlack(void);
extern void _LoadPitchMap(uint16_t index);
extern void _FixupAndColorPitchFile(void);
extern void _ConvertColorsAndTextSprites(TeamGame *team1, TeamGame *team2);
extern uint8_t *_GetPlayer(uint16_t teamNumber, uint16_t playerFileIndex);
extern uint8_t *_GetPlayerAtIndex(uint16_t teamNumber, uint16_t ordinal);
extern uint16_t _cGetPlayerAtIndexFileIdx;
extern uint8_t *_StringCopy(const uint8_t *src, uint8_t *dst);
extern uint8_t *_ExtractSurname(uint8_t *name, uint16_t maxLenAndFlags);
extern uint16_t _IsPlayerInjured(uint8_t *playerBase, int16_t mode);
extern int16_t _ShouldApplyRandomPlayerStatusAfterSim(void);
extern void _Career_UpdateSinglePlayerPostMatchStats(uint8_t *playerFileBase);
extern uint16_t _GetAveragePlayerPrice(uint8_t *teamFileBase);
extern uint8_t *_GetPlayerInfoBreakPosition(uint8_t *teamPtr, uint16_t requestedSlot,
                                              uint16_t *outSlot, uint16_t *outPlayerIndex);

/* ================================================================
 * InitTeamsData  (SWOS2.s line 19585)
 *
 * Clears all per-match game state globals, sets gameState to
 * ST_GAME_IN_PROGRESS, then initializes topTeamData and
 * bottomTeamData TeamGeneralInfo structs. Runs a 2-iteration
 * loop: first iteration fills the "top" slot, second fills
 * "bottom". If teamPlayingUp != 1, a0/a1 are swapped so the
 * struct that gets filled first is bottomTeamData instead.
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitTeamsData(void)
{
    /* Clear per-match game state */
    currentScorer = 0;
    lastPlayerBeforeGoalkeeper = (Sprite *)0;
    goalScored = 0;
    runSlower = 0;
    whichCard = 0;
    bookedPlayer = (Sprite *)0;
    playerHadBall = 0;
    lastKeeperPlayed = 0;
    lastTeamPlayed = (TeamGeneralInfo *)0;
    lastPlayerPlayed = (Sprite *)0;
    penalty = 0;
    goalCameraMode = 0;
    goalOut = 0;
    gameNotInProgressCounterWriteOnly = 0;
    fireBlocked = 0;
    lastTeamPlayedBeforeBreak = (TeamGeneralInfo *)0;
    stoppageTimerTotal = 0;
    stoppageTimerActive = 0;
    stoppageEventTimer = 0;
    inGameCounter = 0;
    gameStatePl = ST_GAME_IN_PROGRESS;
    gameState = ST_GAME_IN_PROGRESS;
    breakState = 0;
    breakCameraMode = 0xFFFF;

    /* 2-iteration loop: first for top team (d7=1, d5=1), second for bottom (d7=0, d5=2) */
    uint16_t          playerNo    = topTeamPlayerNo;
    uint16_t          coachNo     = topTeamCoachNo;
    uint16_t          isPlCoach   = pl1Coach;
    TeamGeneralInfo  *a0          = &topTeamData;       /* destination struct */
    TeamGeneralInfo  *a1          = &bottomTeamData;    /* opponent struct */
    Sprite          **spritesTab  = team1SpritesTable;
    TeamGame         *teamPtr     = topTeamPtr;
    TeamStatisticsData *statsPtr  = &team1StatsData;
    uint16_t         *tactics     = &pl1Tactics;
    uint16_t          teamNum     = 1;

    if (teamPlayingUp != 1) {
        /* swap a0/a1 */
        TeamGeneralInfo *tmp = a0;
        a0 = a1;
        a1 = tmp;
    }

    int16_t iter;
    for (iter = 1; iter >= 0; iter--) {
        /* Fill struct at a0 */
        a0->opponentsTeam                      = a1;
        a0->inGameTeamPtr                      = teamPtr;
        a0->teamStatsPtr                       = statsPtr;
        a0->playerNumber                       = (int16_t)playerNo;
        a0->playerCoachNumber                   = (int16_t)coachNo;
        a0->isPlCoach                           = (int16_t)isPlCoach;
        a0->spritesTable                       = spritesTab;
        a0->teamNumber                         = (int16_t)teamNum;
        a0->tactics                            = (int16_t)*tactics;
        a0->goalkeeperPlaying                  = 0;
        a0->resetControls                      = 0;
        a0->updatePlayerIndex                  = 10;
        a0->controlledPlayer                   = 0;
        a0->passToPlayerPtr                    = 0;
        a0->passingKickingPlayer               = 0;
        a0->playerHasBall                      = 0;
        a0->goalkeeperDivingRight              = 0;
        a0->goalkeeperDivingLeft               = 0;
        a0->ballOutOfPlayOrKeeper              = 0;
        a0->goaliePlayingOrOut                 = 0;
        a0->passingBall                        = 0;
        a0->passingToPlayer                    = 0;
        a0->playerSwitchTimer                  = 0;
        a0->ballInPlay                         = 0;
        a0->ballOutOfPlay                      = 0;
        a0->passKickTimer                      = 0;
        a0->ballCanBeControlled                = 0;
        a0->field_72                           = 0;
        a0->ballControllingPlayerDirection     = (int16_t)0xFFFF;
        a0->field_74                           = 0;
        a0->wonTheBallTimer                    = 0;
        a0->spinTimer                          = (int16_t)0xFFFF;
        a0->field_3C                           = 0;
        a0->goalkeeperSavedCommentTimer        = 0;
        a0->lastHeadingTacklingPlayer          = 0;
        a0->field_4E                           = 0;
        a0->headerOrTackle                     = 0;
        a0->shooting                           = 0;
        a0->passInProgress                     = 0;

        /* Set up for second iteration (bottom team) */
        playerNo    = bottomTeamPlayerNo;
        coachNo     = bottomTeamCoachNo;
        isPlCoach   = pl2Coach;
        a0          = &bottomTeamData;
        a1          = &topTeamData;
        spritesTab  = team2SpritesTable;
        teamPtr     = bottomTeamPtr;  /* bottom team pointer */
        statsPtr    = &team2StatsData;
        tactics     = &pl2Tactics;
        teamNum     = 2;

        if (teamPlayingUp != 1) {
            /* swap a0/a1 for second iteration too */
            TeamGeneralInfo *tmp = a0;
            a0 = a1;
            a1 = tmp;
        }
    }
}

/* ================================================================
 * InitGameSaveTeams  (SWOS2.s line 11941)
 *
 * Saves both team in-game structs (1018 bytes each) to snapshot
 * buffers, stores team pointers, loads pitch, converts colors
 * and sprites, waits for both fire buttons released, fades out,
 * sets paletteIndex=2.
 *
 * NOTE: In the original asm, this function falls through into
 * InitGameRestoreTeams via "bra.w *+4". The asm stub handles
 * that fall-through; this C function only covers the save portion.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _InitGameSaveTeams(TeamGame *leftTeam, TeamGame *rightTeam)
{
    /* Copy both team structs (1018 bytes each) to snapshot buffers */
    uint8_t *src1 = (uint8_t *)&topTeamInGame;
    uint8_t *dst1 = (uint8_t *)&topTeamSaved;
    uint8_t *src2 = (uint8_t *)&bottomTeamInGame;
    uint8_t *dst2 = (uint8_t *)&bottomTeamSaved;
    int16_t i;
    for (i = 1017; i >= 0; i--) {
        *dst1++ = *src1++;
        *dst2++ = *src2++;
    }

    /* Save team pointers */
    topTeamPtr = leftTeam;
    bottomTeamPtr = rightTeam;

    /* Load pitch map (high byte of pitchNumberAndType is the index) */
    _LoadPitchMap(pitchNumberAndType >> 8);
    _FixupAndColorPitchFile();
    _ConvertColorsAndTextSprites(leftTeam, rightTeam);

    /* Wait for both fire buttons released */
    for (;;) {
        _Player1StatusProc();
        if (pl1Fire)
            continue;
        _Player2StatusProc();
        if (!pl2Fire)
            break;
    }

    _FadeOutToBlack();
    paletteIndex = 2;
}

/* ================================================================
 * Chunk 103 — Game Init Player Utilities
 * ================================================================ */

/* ================================================================
 * UpdatePlayerInjuries  (SWOS2.s line 31278)
 *
 * Copies injury bitfield from in-game PlayerInfo back to the
 * PlayerFile in the team file for non-computer teams.
 *
 * Asm: a1 = TeamGame ptr, a2 = TeamFile ptr
 * ================================================================ */
__attribute__((used, externally_visible))
void _UpdatePlayerInjuries(TeamGame *tg, uint8_t *teamFileBase)
{
    TeamFile *tf = (TeamFile *)teamFileBase;
    if (tf->teamControls == 1)  /* TEAM_COMPUTER */
        return;

    for (int16_t i = 15; i >= 0; i--) {
        PlayerInfo *pi = &tg->players[i];
        if (pi->skip != 0)     /* substituted */
            continue;
        if (pi->isInjured == 0)
            continue;

        /* Get the player in the team file */
        uint16_t teamNum = *(uint16_t *)teamFileBase;
        uint8_t *playerBase = _GetPlayer(teamNum, (uint16_t)(int8_t)pi->playerIndex);
        PlayerFile *pf = (PlayerFile *)(playerBase + 76);

        /* Merge injury bitfield: keep low 5 bits, replace high 3 bits */
        uint8_t injuries = (uint8_t)pi->injuriesBitfield;
        uint8_t cardInj = pf->cardsInjuries;
        cardInj = (uint8_t)((cardInj & 0x1F) | injuries);
        pf->cardsInjuries = cardInj;
    }
}

/* ================================================================
 * AccumulateMatchGoalsIntoTeamFile  (SWOS2.s line 31308)
 *
 * Adds own-goals count to team file, then for each player who
 * scored, accumulates goals into the PlayerFile.goalStats[d7].
 *
 * Asm: a1 = TeamGame ptr, a2 = TeamFile ptr, d7.w = goalStats offset
 * ================================================================ */
__attribute__((used, externally_visible))
void _AccumulateMatchGoalsIntoTeamFile(TeamGame *tg, uint8_t *teamFileBase,
                                        uint16_t statOffset)
{
    TeamFile *tf = (TeamFile *)teamFileBase;
    tf->writesZeroHere += (uint8_t)tg->numOwnGoals;

    for (int16_t i = 15; i >= 0; i--) {
        PlayerInfo *pi = &tg->players[i];
        int8_t goals = pi->goalsScored;
        if (goals == 0)
            continue;

        uint16_t teamNum = *(uint16_t *)teamFileBase;
        uint8_t *playerBase = _GetPlayer(teamNum, (uint16_t)(int8_t)pi->playerIndex);
        /* goalStats are at offset 0x6D from playerBase in asm:
         * playerBase + 76 (TeamFile header) + 33 (goalStats offset in PlayerFile) = 109 = 0x6D */
        uint8_t *goalStatPtr = playerBase + 0x6D + statOffset;
        *goalStatPtr += (uint8_t)goals;
    }
}

/* ================================================================
 * SetPlayerNameAndFace  (SWOS2.s line 31434)
 *
 * Copies full player name from PlayerFile to PlayerInfo, and
 * extracts face index from positionAndFace.
 *
 * Asm: a5 = playerBase (from GetPlayer/GetPlayerAtIndex),
 *      a6 = PlayerInfo ptr
 * The asm uses $4C(a5) to reach PlayerFile fields ($4C = 76 =
 * sizeof(TeamFile) = offset to first PlayerFile entry).
 * ================================================================ */
__attribute__((used, externally_visible))
void _SetPlayerNameAndFace(uint8_t *playerBase, PlayerInfo *pi)
{
    PlayerFile *pf = (PlayerFile *)(playerBase + 76);

    /* Copy full player name */
    _StringCopy((uint8_t *)pf->playerName, (uint8_t *)pi->fullName);

    /* Extract face from positionAndFace: bits 3-4 → 0-3 */
    uint16_t face = (uint16_t)(pf->positionAndFace & 0x18) >> 3;
    pi->face2 = (int8_t)face;
}

/* ================================================================
 * InitInGamePlayer  (SWOS2.s line 31455)
 *
 * Initializes an in-game PlayerInfo from the corresponding
 * PlayerFile. Sets short name, face, position, substitution status,
 * cards, injuries, and shirt number.
 *
 * Asm: d1 = player index (d2 from outer loop),
 *      a5 = ptr to TeamFileEx base (raw player data),
 *      a6 = PlayerInfo ptr within TeamGame
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitInGamePlayer(uint16_t playerIndex, uint16_t playerSlot,
                        uint8_t *playerBase, PlayerInfo *pi)
{
    PlayerFile *pf = (PlayerFile *)(playerBase + 76);

    pi->playerIndex = (int8_t)playerIndex;
    pi->goalsScored = 0;
    pi->halfPlayed = 0;

    /* Extract surname for short name display */
    uint8_t *surname = _ExtractSurname((uint8_t *)pf->playerName, 0x800E);
    _StringCopy(surname, (uint8_t *)pi->shortName);

    /* Face from positionAndFace: bits 3-4 → 0-3 */
    uint16_t face = (uint16_t)(pf->positionAndFace & 0x18) >> 3;
    pi->face = (int8_t)face;

    /* Position: bits 5-7 of positionAndFace */
    uint16_t pos = (uint16_t)(pf->positionAndFace & 0xE0) >> 5;
    pi->position = (int8_t)pos;

    /* Substituted flag */
    pi->skip = 0;  /* not substituted */

    /* Check if player is injured/unavailable (bench slots 11+) */
    if (playerSlot >= 11) {
        int16_t mode = (int16_t)plgD3Param;
        uint16_t injured = _IsPlayerInjured(playerBase, mode);
        if (injured != 0) {
            /* Player is injured — mark as unavailable */
            pi->position = (int8_t)0xFF;
            pi->skip = 1;  /* substituted/unavailable */
        } else {
            /* Not injured but beyond max subs limit */
            uint16_t maxSubs = gameMaxSubstitutes + 11;
            if (playerSlot >= maxSubs) {
                pi->position = (int8_t)0xFF;
                pi->skip = 1;
            }
        }
    }

    /* Cards from player file */
    pi->numYellowCards = 0;
    uint8_t cardInj = pf->cardsInjuries;
    if (plgD3Param == 0) {
        /* League/friendly mode: extract card bits 2-4 → shift right 2 */
        uint16_t cards = (uint16_t)(cardInj & 0x1C) >> 2;
        pi->numYellowCards = (int8_t)cards;
    } else {
        /* Career/tournament mode: extract bits 0-1 */
        uint16_t cards = (uint16_t)(cardInj & 0x03);
        if (cards >= 1) {
            pi->numYellowCards = 2;
        }
    }

    pi->previousCards = pi->numYellowCards;
    pi->isInjured = 0;
    pi->field_B = 0;  /* disallowSubs */
    pi->injuriesBitfield = 0;

    /* Copy injury bitfield (high 3 bits of cardsInjuries) */
    pi->injuriesBitfield = (int8_t)(pf->cardsInjuries & 0xE0);

    pi->field_8 = 0;  /* fasterTackle */
    pi->shirtNumber = pf->shirtNumber;
}

/* ================================================================
 * ApplyPlayerStatusAfterSim  (SWOS2.s line 31227)
 *
 * After a simulated match, applies card/injury effects from
 * in-game PlayerInfo back to the PlayerFile.
 *
 * Asm: a1 = TeamGame ptr, a2 = TeamFile ptr
 * ================================================================ */
__attribute__((used, externally_visible))
void _ApplyPlayerStatusAfterSim(TeamGame *tg, uint8_t *teamFileBase)
{
    if (trainingGame != 0)
        return;

    TeamFile *tf = (TeamFile *)teamFileBase;
    if (tf->teamControls == 1)  /* TEAM_COMPUTER */
        return;

    for (int16_t i = 15; i >= 0; i--) {
        PlayerInfo *pi = &tg->players[i];
        if (pi->skip != 0)        /* substituted */
            continue;
        if (pi->numYellowCards == 0)  /* cards field */
            continue;

        uint16_t teamNum = *(uint16_t *)teamFileBase;
        uint8_t *playerBase = _GetPlayer(teamNum, (uint16_t)(int8_t)pi->playerIndex);

        uint8_t cardVal = (uint8_t)pi->numYellowCards;
        PlayerFile *pf = (PlayerFile *)(playerBase + 76);

        if (plgD3Param == 0) {
            /* League/friendly mode */
            uint16_t shifted = (uint16_t)(cardVal << 2) & 0x1C;
            if (_ShouldApplyRandomPlayerStatusAfterSim() == 0) {
                /* Don't apply — but if exactly 4 (one yellow?), skip entirely */
                if (shifted == 4)
                    continue;
                shifted = 8;  /* override to 2 yellows */
            }
            uint8_t cur = pf->cardsInjuries;
            cur = (uint8_t)((cur & ~0x1C) | shifted);
            pf->cardsInjuries = cur;
        } else {
            /* Career/tournament mode */
            if (cardVal == 1)
                continue;  /* no action for 1 */
            cardVal -= 1;
            uint16_t bits = (uint16_t)(cardVal & 0x03);
            uint8_t cur = pf->cardsInjuries;
            cur = (uint8_t)((cur & ~0x03) | bits);
            pf->cardsInjuries = cur;
        }
    }
}

/* ================================================================
 * UpdateCareerPlayerStatsAfterMatch  (SWOS2.s line 31196)
 *
 * For each non-substituted player who played the full match
 * (halfPlayed == 2), calls Career_UpdateSinglePlayerPostMatchStats.
 * Only runs for the career team and not during training.
 *
 * Asm: a1 = TeamGame ptr, a2 = TeamFile ptr
 * ================================================================ */
__attribute__((used, externally_visible))
void _UpdateCareerPlayerStatsAfterMatch(TeamGame *tg, uint8_t *teamFileBase)
{
    if (trainingGame != 0)
        return;
    if (teamFileBase != (uint8_t *)&careerTeam)
        return;

    for (int16_t i = 15; i >= 0; i--) {
        PlayerInfo *pi = &tg->players[i];
        if (pi->skip != 0)      /* substituted */
            continue;
        if (pi->halfPlayed != 2)  /* didn't play full match */
            continue;

        uint16_t teamNum = *(uint16_t *)teamFileBase;
        uint8_t *playerBase = _GetPlayer(teamNum, (uint16_t)(int8_t)pi->playerIndex);
        _Career_UpdateSinglePlayerPostMatchStats(playerBase);
    }
}

/* ================================================================
 * Chunk 104 — InitInGameTeamStructure
 * ================================================================ */

/* ================================================================
 * InitInGameTeamStructure  (SWOS2.s line 31332)
 *
 * Initializes a TeamGame struct from a TeamFile for match play.
 * For each of 16 players: calls GetPlayerAtIndex, InitInGamePlayer,
 * GetPlayerInfoBreakPosition, SetPlayerNameAndFace, and clears
 * skill fields. Computes team price percentage if needed.
 * Copies team name.
 *
 * Asm: a2=TeamFile*, a4=TeamGame*, a6=int16_t* avgPricePct
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitInGameTeamStructure(uint8_t *teamFileBase, TeamGame *tg,
                                int16_t *avgPricePct)
{
    TeamFile *tf = (TeamFile *)teamFileBase;

    tg->markedPlayer = -1;
    *avgPricePct = 100;

    /* Compute team price percentage for non-career, equal-teams mode */
    if (gameType != 4 /* GAME_TYPE_CAREER */) {
        if (allPlayerTeamsEqual != 0 && tf->teamControls == 2 /* TEAM_PLAYER_COACH */) {
            uint16_t avgPrice = _GetAveragePlayerPrice(teamFileBase);
            uint32_t pct = (uint32_t)avgSelectedTeamsPlayerPrice * 100;
            pct = pct / avgPrice;
            *avgPricePct = (int16_t)(pct + 6);
        }
    }

    /* Clear playerHasBall in the TeamGeneralInfo that overlays the start of TeamGame.
     * The asm does: movea.l a4,a6; clr.b TEAMGENERALINFO_PLAYERHASBALL(a6)
     * TEAMGENERALINFO_PLAYERHASBALL is at offset 40 in TeamGeneralInfo.
     * But a6 is now used as TeamGame base, and offset 40 = numOwnGoals.
     * This is the asm reusing the TeamGame memory for something else initially. */
    tg->numOwnGoals = 0;

    for (uint16_t d2 = 0; d2 < 16; d2++) {
        uint16_t teamNum = *(uint16_t *)teamFileBase;
        uint16_t d1 = d2;

        /* In training mode for currentTeam, slots 13+ map to slot 0 */
        if (trainingGame != 0 && teamFileBase == currentTeam && d1 >= 13) {
            d1 = 0;
        }

        uint8_t *playerBase = _GetPlayerAtIndex(teamNum, d1);
        PlayerInfo *pi = &tg->players[d2];

        _InitInGamePlayer(d1, d2, playerBase, pi);

        /* GetPlayerInfoBreakPosition: maps slot via tactics, returns mapped slot and file index */
        d1 = d2;  /* reset */
        if (trainingGame != 0 && teamFileBase == currentTeam && d1 >= 13) {
            d1 = 0;
        }
        uint16_t slot, playerIndex;
        uint8_t *playerBase2 = _GetPlayerInfoBreakPosition(teamFileBase, d1, &slot, &playerIndex);

        /* Check if this player is the selected player (for marking) */
        uint16_t selectedIdx;
        if (tg == &topTeamInGame) {
            selectedIdx = team1SelectedPlayerIndex;
        } else {
            selectedIdx = team2SelectedPlayerIndex;
        }
        if (playerIndex == selectedIdx) {
            tg->markedPlayer = (int16_t)slot;
        }

        /* Also check career/current team selected player position */
        uint16_t selPos = selTeamSelectedPlayerPosition;
        if (teamFileBase == (uint8_t *)&careerTeam) {
            if (playerIndex == selPos) {
                tg->markedPlayer = (int16_t)slot;
            }
        } else if (teamFileBase == currentTeam) {
            selPos = currentTeamSelectedPlayerPosition;
            if (playerIndex == selPos) {
                tg->markedPlayer = (int16_t)slot;
            }
        }

        /* SetPlayerNameAndFace uses the player ptr returned by GetPlayerInfoBreakPosition */
        _SetPlayerNameAndFace(playerBase2, pi);

        /* Clear all skill fields */
        pi->passing = 0;
        pi->shooting = 0;
        pi->heading = 0;
        pi->tackling = 0;
        pi->ballControl = 0;
        pi->speed = 0;
        pi->finishing = 0;
        pi->goalieSkill = 0;
    }

    /* Copy team name */
    uint8_t *nameSource;
    if (trainingGame != 0) {
        nameSource = teamAName;
        if (tg != &topTeamInGame) {
            nameSource = teamBName;
        }
    } else {
        nameSource = (uint8_t *)tf->teamName;
    }
    _StringCopy(nameSource, (uint8_t *)tg->teamName);
}

/* ========================================================================
 * Chunk 191 — SetupPlayers, InitAndPlayGame
 * ======================================================================== */

/* Externs for Chunk 191 */
extern uint16_t careerTeamCoachOnly;
extern uint16_t isCareer;
extern uint16_t exitGameFlag;
extern uint16_t twoPlayers;
extern uint16_t gameCanceled;
extern uint16_t maxSubstitutes;
extern uint16_t minSubstitutes;
extern uint8_t  gameName[];
extern uint8_t  gameRound;
extern uint16_t pitchTypeOrSeason;
extern uint16_t gamePitchTypeOrSeason;
extern uint16_t friendlySeason;
extern uint16_t gameSeason;
extern uint16_t friendlyPitchType;
extern uint16_t gamePitchType;
extern uint16_t team1GoalsFirstLeg;
extern uint16_t team2GoalsFirstLeg;
extern int16_t  extraTimeState;
extern int16_t  penaltiesState;
extern uint16_t secondLeg;
extern uint16_t playing2ndGame;
extern uint16_t isGameFriendly;
extern uint8_t  aTraining[];
extern uint8_t  aFriendly_1[];

/* _ShowPlayMatchMenu declared in play_match_menu.h */
/* _InitializeInGameTeamsAndStartGame declared in game_init.h */

extern uint16_t _ShouldSetupPlayers(void);
extern void _InitPlayerRemapTableForTeam(uint8_t *team, uint8_t *remapTable);

/* SetupPlayers
 *
 * Shows play match menu for one or both human players before a match.
 * Handles career team remap, coach mode, and player/coach number assignment.
 *
 * Parameters:
 *   team1, team2      — TeamFileEx pointers (asm a1, a2)
 *   maxSubs           — max substitutes (asm d1)
 *   excludeFlags      — passed through to ShowPlayMatchMenu d2
 *   allowSquadChanges — enable squad changes in menu (asm d6)
 *   mode              — mode flag (asm d7); 0 = normal, non-zero = skip player assignment
 */
__attribute__((used, externally_visible))
void _SetupPlayers(TeamFileEx *team1, TeamFileEx *team2, int16_t maxSubs,
                   int16_t excludeFlags, int16_t allowSquadChanges, int16_t mode) {
    uint16_t d0 = 0;

    careerTeamCoachOnly = 0;

    /* Career team remap check */
    if (isCareer == 0) {
        if (team1 == &careerTeam || team2 == &careerTeam) {
            if (_ShouldSetupPlayers() == 0) {
                _InitPlayerRemapTableForTeam((uint8_t *)&careerTeam, (uint8_t *)0);
            }
        }
    }

    isCareer = 0;
    exitGameFlag = 0;
    team1SelectedPlayerIndex = 0xFFFF;
    team2SelectedPlayerIndex = 0xFFFF;

    if (team1->team.teamControls != TEAM_COMPUTER &&
        team2->team.teamControls != TEAM_COMPUTER) {
        twoPlayers = 1;
    }

    /* Set pl1Coach: seq sets high byte to 0xFF → word = 0xFF00 */
    pl1Coach = 0;
    if (team1->team.teamControls == TEAM_COACH)
        *(uint8_t *)&pl1Coach = 0xFF;

    topTeamPlayerNo = 0;
    topTeamCoachNo = 0;

    /* Skip first menu only if team1 is computer and team2 is not.
     * If both are computer, still show the menu (computer vs computer viewing). */
    if (team1->team.teamControls == TEAM_COMPUTER &&
        team2->team.teamControls != TEAM_COMPUTER)
        goto setup_team2;

    /* Show play match menu for first player */
    d0 = _ShowPlayMatchMenu(3, maxSubs, excludeFlags, 1, allowSquadChanges,
                              (uint8_t *)team1, (uint8_t *)team2);

    if (exitGameFlag != 0) goto cancelled;

    if (careerTeamCoachOnly != 0) {
        pl1Coach = 0;
        topTeamPlayerNo = 0;
        topTeamCoachNo = 0;
    }

    if (team1->team.teamControls != TEAM_COMPUTER && mode == 0) {
        if (team1->team.teamControls == TEAM_COACH) {
            topTeamCoachNo = d0;
        } else {
            topTeamPlayerNo = d0;
        }
    }

setup_team2:
    /* Set pl2Coach: seq pattern same as pl1Coach */
    pl2Coach = 0;
    if (team2->team.teamControls == TEAM_COACH)
        *(uint8_t *)&pl2Coach = 0xFF;

    bottomTeamPlayerNo = 0;
    bottomTeamCoachNo = 0;

    if (team2->team.teamControls == TEAM_COMPUTER)
        goto set_tactics;

    if (careerTeamCoachOnly != 0)
        goto skip_second_menu;

    /* Calculate exclude flags for second player:
     * d0 = (topTeamPlayerNo | topTeamCoachNo) ^ 3 */
    d0 = (topTeamPlayerNo | topTeamCoachNo) ^ 3;

    /* Show second player menu with teams swapped (team2 as first, team1 as second) */
    d0 = _ShowPlayMatchMenu(d0, maxSubs, excludeFlags, 0, allowSquadChanges,
                              (uint8_t *)team2, (uint8_t *)team1);

    if (exitGameFlag != 0) goto cancelled;

    if (mode != 0)
        goto set_tactics_swapped;

skip_second_menu:
    if (team2->team.teamControls == TEAM_COACH) {
        bottomTeamCoachNo = d0;
    } else {
        bottomTeamPlayerNo = d0;
    }

set_tactics:
    pl1Tactics = (int16_t)(int8_t)team1->team.tactics;
    pl2Tactics = (int16_t)(int8_t)team2->team.tactics;
    return;

set_tactics_swapped:
    /* When mode != 0, teams stay swapped (exg a1,a2 not undone) */
    pl1Tactics = (int16_t)(int8_t)team2->team.tactics;
    pl2Tactics = (int16_t)(int8_t)team1->team.tactics;
    return;

cancelled:
    gameCanceled = 1;
    isCareer = 0;
    if (team1 == &careerTeam || team2 == &careerTeam) {
        isCareer = 1;
    }
}

/* InitAndPlayGame
 *
 * Entry point for friendly/training matches. Calls SetupPlayers for player
 * configuration, then sets up game parameters and calls
 * InitializeInGameTeamsAndStartGame.
 *
 * Parameters:
 *   team1, team2 — TeamFileEx pointers (asm a1, a2)
 */
__attribute__((used, externally_visible))
void _InitAndPlayGame(TeamFileEx *team1, TeamFileEx *team2) {
    /* SetupPlayers: mode=0, maxSubs=maxSubstitutes, excludeFlags=1, allowSquadChanges=1 */
    _SetupPlayers(team1, team2, (int16_t)maxSubstitutes, 1, 1, 0);

    if (exitGameFlag != 0)
        goto done;

    /* Set game name */
    if (trainingGame != 0) {
        _StringCopy(aTraining, gameName);
    } else {
        _StringCopy(aFriendly_1, gameName);
    }
    gameRound = 0;

    /* Set game parameters */
    gamePitchTypeOrSeason = pitchTypeOrSeason;
    gameSeason = friendlySeason;
    gamePitchType = friendlyPitchType;
    team1GoalsFirstLeg = 0;
    team2GoalsFirstLeg = 0;
    extraTimeState = 0;
    penaltiesState = 0;
    secondLeg = 0;
    playing2ndGame = 0;
    isGameFriendly = 1;

    /* d0=1, d1=minSubs, d2=maxSubs, d3=0, d7=-1, a1=team1, a2=team2 */
    _InitializeInGameTeamsAndStartGame(1, (int16_t)minSubstitutes,
        (int16_t)maxSubstitutes, 0, -1, &team1->team, &team2->team);

done:
    isCareer = 0;
}

/* ========================================================================
 * Chunk 214 — InitializeInGameTeamsAndStartGame
 * ======================================================================== */

/* Additional externs for Chunk 214 */
extern uint16_t hilStarted;
extern uint16_t plgD0Param;
extern uint16_t maxSubsPerTeam;
extern TeamFile *gameTeam1;
extern TeamFile *gameTeam2;
extern uint16_t teamsLoaded;
extern uint16_t poolplyrLoaded;
extern uint16_t team1Computer;
extern uint16_t team2Computer;
extern int16_t  team1AppPercent;
extern int16_t  team2AppPercent;
extern uint16_t team1NumAllowedInjuries;
extern uint16_t team2NumAllowedInjuries;
extern uint16_t menuMusic;
extern uint16_t deadVarAlways0;
extern uint16_t gameLength;
extern uint16_t gameLengthInGame;
extern volatile uint16_t currentGameTick;
extern uint8_t  stadiumMenu[];

/* C function declarations for called routines */
extern void _SetTeamSecondaryColors(uint16_t shirtType, uint8_t stripesColor,
    uint8_t basicColor, uint8_t shortsColor, uint8_t socksColor, TeamGame *team);
extern void _SetInGameTeamsPrimaryColors(TeamFile *team1File, TeamFile *team2File);
extern uint16_t _ComputeOldPlayerSkillForViewResult(uint8_t *team, uint16_t goalsCount);
extern uint16_t _GetAveragePlayerPriceInSelectedTeams(void);
extern void _InitInGameTeamStructure(uint8_t *teamFileBase, TeamGame *tg, int16_t *avgPricePct);
extern uint32_t _GetNumberOfAvailablePlayers(uint8_t *team, int16_t excludeCardsFlag, uint16_t numPl);
extern void _StopMusic(void);
extern void _ShowMenu(uint8_t *packedMenu);
extern void _ShowMatchSequence(void);
extern void _InitMenuMusic(void);
extern void _TryPlayMusic(void);
extern void _PrepareMenuForFadeIn(void);
extern void _InitInGameTeams_PlayerControlled(uint8_t *team1, uint8_t *team2, uint16_t mode);
extern void _AccumulateMatchGoalsIntoTeamFile(TeamGame *tg, uint8_t *teamFileBase, uint16_t statOffset);
extern void _UpdatePlayerInjuries(TeamGame *tg, uint8_t *teamFileBase);
extern void _ApplyPlayerStatusAfterSim(TeamGame *tg, uint8_t *teamFileBase);
extern void _UpdateCareerPlayerStatsAfterMatch(TeamGame *tg, uint8_t *teamFileBase);

/* InitializeInGameTeamsAndStartGame
 *
 * Sets up teams for a match, displays stadium menu, plays match,
 * then handles post-match statistics and injuries.
 *
 * Parameters (from asm register convention):
 *   randomizePitch (d0) — pitch randomization param
 *   minSubs (d1)        — minimum substitutes
 *   maxSubs (d2)        — maximum substitutes
 *   paramD3 (d3)        — passed to plgD3Param
 *   paramD7 (d7)        — flags; -1 = skip post-match processing
 *   team1 (a1)          — first team (TeamFile)
 *   team2 (a2)          — second team (TeamFile)
 */
__attribute__((used, externally_visible))
void _InitializeInGameTeamsAndStartGame(int16_t randomizePitch, int16_t minSubs,
    int16_t maxSubs, int16_t paramD3, int16_t paramD7,
    TeamFile *team1, TeamFile *team2) {
    uint16_t d0;
    int16_t disabledCount;
    int16_t allowedInjuries;
    uint32_t avail;

    hilStarted = 0;
    plgD0Param = (uint16_t)randomizePitch;
    maxSubsPerTeam = (uint16_t)minSubs;
    gameMaxSubstitutes = (uint16_t)maxSubs;
    plgD3Param = (uint16_t)paramD3;
    gameTeam1 = team1;
    gameTeam2 = team2;

    /* Compute old player skill for view result (for both teams) */
    _ComputeOldPlayerSkillForViewResult((uint8_t *)team1, (uint16_t)maxSubs);
    _ComputeOldPlayerSkillForViewResult((uint8_t *)team2, (uint16_t)maxSubs);

    teamsLoaded = 0;
    poolplyrLoaded = 0;

    /* Set computer flags:
     * clr.w + seq on low byte → 0x00FF if computer, 0x0000 otherwise */
    team1Computer = 0;
    team2Computer = 0;
    if (team1->teamControls == TEAM_COMPUTER)
        team1Computer = 0x00FF;
    if (team2->teamControls == TEAM_COMPUTER)
        team2Computer = 0x00FF;

    /* Set secondary colors for both teams */
    _SetTeamSecondaryColors((uint16_t)team1->prShirtType, team1->prStripesColor,
        team1->prBasicColor, team1->prShortsColor, team1->prSocksColor, &topTeamInGame);
    _SetTeamSecondaryColors((uint16_t)team2->prShirtType, team2->prStripesColor,
        team2->prBasicColor, team2->prShortsColor, team2->prSocksColor, &bottomTeamInGame);

    /* Set primary colors */
    _SetInGameTeamsPrimaryColors(team1, team2);

    /* If not career and all player teams are equal, compute average price */
    if (gameType != GAME_TYPE_CAREER && allPlayerTeamsEqual != 0) {
        avgSelectedTeamsPlayerPrice = _GetAveragePlayerPriceInSelectedTeams();
    }

    /* Initialize in-game team structures */
    _InitInGameTeamStructure((uint8_t *)team1, &topTeamInGame, &team1AppPercent);
    _InitInGameTeamStructure((uint8_t *)team2, &bottomTeamInGame, &team2AppPercent);

    /* Calculate allowed injuries for each team */
    if (gameType == GAME_TYPE_CAREER) {
        /* Career mode: allow up to 4 injuries */
        avail = _GetNumberOfAvailablePlayers((uint8_t *)team1, -1, 0);
        disabledCount = (int16_t)(avail >> 16); /* d6 = disabled */
        allowedInjuries = 4 - disabledCount;
        if (allowedInjuries < 0) allowedInjuries = 0;
        team1NumAllowedInjuries = (uint16_t)allowedInjuries;

        avail = _GetNumberOfAvailablePlayers((uint8_t *)team2, -1, 0);
        disabledCount = (int16_t)(avail >> 16);
        allowedInjuries = 4 - disabledCount;
        if (allowedInjuries < 0) allowedInjuries = 0;
        team2NumAllowedInjuries = (uint16_t)allowedInjuries;
    } else {
        /* Non-career: allow up to 2 injuries */
        avail = _GetNumberOfAvailablePlayers((uint8_t *)team1, -1, 0);
        disabledCount = (int16_t)(avail >> 16);
        allowedInjuries = 2 - disabledCount;
        if (allowedInjuries < 0) allowedInjuries = 0;
        team1NumAllowedInjuries = (uint16_t)allowedInjuries;

        avail = _GetNumberOfAvailablePlayers((uint8_t *)team2, -1, 0);
        disabledCount = (int16_t)(avail >> 16);
        allowedInjuries = 2 - disabledCount;
        if (allowedInjuries < 0) allowedInjuries = 0;
        team2NumAllowedInjuries = (uint16_t)allowedInjuries;
    }

    /* Override for friendly games */
    if (isGameFriendly != 0) {
        team1NumAllowedInjuries = 4;
        team2NumAllowedInjuries = 4;
    }

    /* Stop music if playing */
    if (menuMusic != 0) {
        _StopMusic();
    }

    /* Show stadium menu */
    _ShowMenu(stadiumMenu);

    /* Wait 150 frames (~3 seconds at 50Hz) */
    {
        int16_t i;
        for (i = 0x95; i >= 0; i--) {
            d0 = currentGameTick;
            while (currentGameTick == d0)
                ;
        }
    }

    deadVarAlways0 = 0;
    gameLengthInGame = gameLength;

    /* For non-friendly, non-season games, clear game length */
    if (isGameFriendly == 0 && gameType != 1) {
        gameLengthInGame = 0;
    }

    /* Play the match */
    _ShowMatchSequence();

    if (gameCanceled != 0)
        goto post_match;

    if (trainingGame != 0) {
        /* Training game: set flag and update injuries only */
        team1->andWith0xFE |= 1; /* bset #0,$17(a0) */
        team2->andWith0xFE |= 1;

        _UpdatePlayerInjuries(&topTeamInGame, (uint8_t *)team1);
        _UpdatePlayerInjuries(&bottomTeamInGame, (uint8_t *)team2);
        goto post_match;
    }

    /* Non-training: full post-match processing */
    _InitInGameTeams_PlayerControlled((uint8_t *)team1, (uint8_t *)team2, (uint16_t)plgD3Param);

    if (paramD7 < 0)
        goto post_match;

    /* Process team 1 */
    _AccumulateMatchGoalsIntoTeamFile(&topTeamInGame, (uint8_t *)team1, (uint16_t)paramD7);
    _UpdatePlayerInjuries(&topTeamInGame, (uint8_t *)team1);
    _ApplyPlayerStatusAfterSim(&topTeamInGame, (uint8_t *)team1);
    _UpdateCareerPlayerStatsAfterMatch(&topTeamInGame, (uint8_t *)team1);

    /* Process team 2 */
    _AccumulateMatchGoalsIntoTeamFile(&bottomTeamInGame, (uint8_t *)team2, (uint16_t)paramD7);
    _UpdatePlayerInjuries(&bottomTeamInGame, (uint8_t *)team2);
    _ApplyPlayerStatusAfterSim(&bottomTeamInGame, (uint8_t *)team2);
    _UpdateCareerPlayerStatsAfterMatch(&bottomTeamInGame, (uint8_t *)team2);

post_match:
    _InitMenuMusic();
    if (menuMusic != 0) {
        _TryPlayMusic();
    }

    _PrepareMenuForFadeIn();
    /* gameCanceled is tested by caller via return/tst.w */
}
