/* career_status.c — Player status decay, availability, team init, career finances,
 * chairman warnings, board confidence, reputation bonuses.
 *
 * Ported from SWOS2.s: CyclePlayerStatusFlagsForTeam, MaybeDecayPlayerStatusBucket,
 * MaybeDecayPlayerStatusBucketForTeam, InitInGameTeams_PlayerControlled,
 * InitInGameTeams_WithMatchParam, BuildCareerTeamUnavailablePlayerList,
 * ClearCareerMatchPlayerStatusBucket, ShouldApplyRandomPlayerStatusAfterSim,
 * IsPlayerInjured, GetNumberOfSubstitutes, GetNumberOfSubstitutesInSelectionList,
 * GetNumberOfAvailablePlayers, GetNumberOfAvailableSubstitutes,
 * GetNumberOfAvailableStartingPlayers, GetAgeAdjustedPlayerFormFactor,
 * ClampNewBalanceAndUpdateClubStatus, ClearOverdraftStateIfRecovered,
 * SetOverdraftWarningCooldown, SetPerformanceWarningCooldown,
 * IsMajorInternationalTournament, IsMajorTournamentAllowingRandomStatus,
 * IsMajorTournamentAllowingSquadChanges, IsTournamentAllowingSquadChangesIfNotCareer,
 * ShouldSetupPlayers, CalcLeagueMatchGateReceiptsFactor, CalcCupMatchGateReceiptsFactor,
 * UpdateCareerFinancesAfterMatch, MaybeSackManagerAfterSeasonReview,
 * UpdateCareerChairmanWarningsAfterMatch, UpdateCareerAfterMatch,
 * ApplyWeeklyFinancialDeductions, InitializeCareerSeasonFinancials,
 * GetBoardConfidenceScore, UpdateBoardConfidenceScore, PushBoardConfidenceHistory,
 * CalcBoardConfidenceDeltaPercent, UpdateBoardConfidenceModifiers,
 * UpdateCareerReputationBonuses, AddReputationFromBonusValueDiv4,
 * AddReputationFromBonusValueDiv8, InitializeStartingBalanceForNewSeason,
 * GetExpectedLeagueRankIndex.
 */

#include "career_status.h"
#include "game_types.h"

/* ---- Assembly externs (functions still in asm) ---- */
extern uint16_t _Rand(void);
extern uint16_t _Rand2(void);
extern uint8_t *_VerifyTeamLoaded(uint16_t teamNum);
extern uint16_t _CGetTeamValueForGateReceipts(uint8_t *team);
extern uint16_t _CGetTeamValueBase(uint16_t avgPrice);
extern uint16_t _CFindTeamInDIY(uint16_t teamNum, uint8_t *compBuf);
extern uint16_t _CGetMoreExpensiveTeamsCount(uint16_t teamNum, uint8_t *compBuf);
extern void _CShowChairmanMessage(void);
extern void _CAddCareerTeamPlayer(void);
extern void _CCheckPlayersLeavingComing(void);
extern uint8_t *_CTeamSelectionKeyCache_GetCurrentListPtr(void);

/* C-callable asm bridges for GetPlayer/GetPlayerAtIndex */
extern uint8_t *_CallGetPlayer(uint16_t teamNumber, uint16_t playerIndex);
extern uint8_t *_CGetPlayerAtIndex(uint16_t teamNumber, uint16_t ordinal);
extern uint16_t _cGetPlayerAtIndexFileIdx;

/* Already-ported C functions */
extern uint16_t _GetDisplayedPlayerValue(uint8_t *ptr);

/* ---- Data externs ---- */
extern TeamFileEx careerTeam;
extern uint8_t    currentMatchPlayers[32];
extern uint16_t   careerNumPlayers;
extern TeamFileEx currentTeam;
extern uint8_t    playerPositions[32];
extern uint16_t   numPlayers;
extern uint16_t   trainingGame;
extern uint16_t   gameType;
extern uint16_t   isNationalSelector;
extern uint8_t    careerUnavailablePlayerList[16];
extern uint8_t    careerUnavailablePlayerLookupTable[];
extern uint8_t    playerFormModifiers[32];
extern uint16_t   selTeamNumber;
extern uint16_t   panicReasonCode;
extern void       _PanicHaltLoop(void);

/* Financial globals */
extern int32_t    newBalance;
extern int32_t    gateReceipts;
extern int16_t    gateReceiptsFactor;
extern int32_t    seasonGateReceiptsAccum;
extern int32_t    seasonGateReceiptsAccum2;
extern uint16_t   seasonMatchReceiptsCount;
extern uint16_t   seasonWeeksDeducted;
extern int32_t    seasonRunningCostsAccum;
extern int32_t    seasonRunningCostsAccum2;
extern int32_t    seasonWagesAccum;
extern int32_t    playerWagesBill;
extern int32_t    generalRunningCosts;
extern int32_t    weeklyPlayerWages;
extern int32_t    weeklyRunningCosts;
extern int32_t    totalTeamValue;
extern uint16_t   teamValueDivisor;
extern uint16_t   wageScalingDivisor;
extern int32_t    weeklyWageByPlayerIndex[];
extern uint16_t   avgTeamValueBases[];
extern int32_t    old_balance;
extern int32_t    startingBalance;
extern int32_t    startingBalanceCopy;
extern int32_t    competitionBonuses;

/* Warning/sacking state */
extern uint16_t   overdraftStreak;
extern uint16_t   overdraftWarningState;
extern uint16_t   performanceWarningState;
extern uint16_t   overdraftWarningCooldown;
extern uint16_t   performanceWarningCooldown;
extern uint16_t   clubBusinessActive;
extern uint16_t   youreSacked;
extern uint16_t   currentMessage;
extern uint16_t   stayingInOldClub;
extern uint16_t   careerTeamLostMatch;
extern uint16_t   competitionSlotUpdatedFlag;
extern uint16_t   removedOfferPlayersFromSelectedTeamCount;

/* Board confidence */
extern int16_t    boardConfidenceScore;
extern int16_t    boardConfidenceHistory0;
extern int16_t    boardConfidenceHistory1;
extern int16_t    boardConfidenceHistory2;
extern int16_t    boardConfidenceHistory3;

/* Trophy/win flags */
extern uint16_t   wonWorldCup;
extern uint16_t   wonContinentalCup;
extern uint16_t   wonInvitationTournament;
extern uint16_t   playWinCupAnimation;
extern uint16_t   playPromotedAnimation;
extern uint16_t   majorTrophyBonusActive;
extern uint16_t   cupBonus1Active;
extern uint16_t   cupBonus2Active;
extern uint16_t   minorTrophyBonusActive;
extern uint16_t   leagueTitleBonusActive;

/* Competition pointers and contexts */
extern uint32_t   currentCompetitionPtr;
extern uint32_t   secondaryCompetitionPtr;
extern uint32_t   tertiaryCompetitionPtr;
extern uint32_t   quaternaryCompetitionPtr;
extern uint8_t    competitionContext[];
extern uint8_t    tertiaryCompetitionContext[];
extern uint8_t    euroCupsCompetitionContext[];
extern uint8_t    competitionFileBuffer[];

/* Competition copy symbols (addresses used for comparisons) */
extern uint8_t    worldCup2Copy[];
extern uint8_t    europeanChampionships2Copy[];
extern uint8_t    copaAmericaCopy[];
extern uint8_t    africanNationsCupCopy[];
extern uint8_t    northAmericanCupCopy[];
extern uint8_t    asianCupCopy[];
extern uint8_t    oceaniaCupCopy[];
extern uint8_t    invitationTournamentCopy[];
extern uint8_t    tourCopy[];
extern uint8_t    friendlyCopy[];
extern uint8_t    worldCupQualifyingGroup1Copy[];
extern uint8_t    worldCupQualifyingGroup8Copy[];
extern uint8_t    ecQualifyingGroup1Copy[];
extern uint8_t    ecQualifyingGroup8Copy[];
extern uint8_t    northAmericaWorldCupQualificationCopy[];
extern uint8_t    southAmericaWorldCupQualificationCopy[];
extern uint8_t    africaWorldCupQualificationCopy[];
extern uint8_t    asiaWorldCupQualificationCopy[];
extern uint8_t    oceaniaWorldCupQualificationCopy[];

/* Season/career state */
extern uint16_t   currentDivisionIndex;
extern uint16_t   seasonReview;
extern uint16_t   seasonFinished;
extern uint16_t   secondaryCompFinished;
extern uint16_t   tertiaryCompFinished;
extern uint16_t   leaguePromotionCount;
extern uint16_t   leagueRelegationCount;
extern uint16_t   leagueWarningsEnabled;
extern uint16_t   expectedLeagueRankIndex;
extern uint16_t   diyFoundTeamIndex;
extern uint16_t   diyCompHasGroupStage;
extern uint16_t   numDiyTeams;
extern uint16_t   isCareer;
extern int8_t     careerJobOfferPhaseBias;
extern uint8_t    viewRandomPhase;
extern uint8_t    someTeamFlags;
extern uint16_t   worldCupQualifyingTeam;
extern uint16_t   worldCupHostTeam;
extern uint16_t   euroChampionshipQualTeam;
extern uint16_t   ballSpeed;
extern uint32_t   currentGameTick;
extern uint8_t    lastFinancialWeekIndex;
extern uint8_t    diyFileBufferCopy[];

/* Winner teams */
extern uint16_t   primaryCompWinnerTeam;
extern uint16_t   secondaryCompWinnerTeam;
extern uint16_t   tertiaryCompWinnerTeam;
extern uint16_t   quaternaryCompWinnerTeam;
extern uint16_t   euroCupSlotIndex;

/* Qualifying flags */
extern uint16_t   qualifyingStage1PendingFlag0;
extern uint16_t   qualifyingStage1PendingFlag1;
extern uint16_t   qualifyingStage2PendingFlag0;
extern uint16_t   qualifyingStage2PendingFlag1;

/* Reputation */
extern int16_t    nationalSelectorReputationPoints;
extern int16_t    careerReputationPoints;
extern uint16_t   winCupBonusCount;
extern uint16_t   minorTrophyBonusCount;
extern uint16_t   majorTrophyBonusCount;
extern uint8_t    majorTrophyBonusValue;
extern uint8_t    minorTrophyBonusValue;
extern uint8_t    leagueTitleBonusValue;
extern uint8_t    majorTrophyOrCupBonusValue;

/* DIY competition buffer — kept as uint8_t[] to avoid size warnings */
extern uint8_t    diyCompetitionStart[];

/* DIY competition struct cast for diyFileBufferCopy / diyCompetitionStart access */
#define DIY_COMP(ptr)  ((DIY_Competition *)(ptr))

#define GAME_TYPE_CAREER 4

/* cardsInjuries bit masks */
#define CI_CARD_BITS         0x03   /* bits 0-1: card severity */
#define CI_INJURY_BITS       0x1C   /* bits 2-4: injury severity */
#define CI_STATUS_BUCKET     0xE0   /* bits 5-7: status bucket */

/* Position extraction from positionAndFace */
#define PF_POSITION_MASK     0xE0
#define PF_POSITION_SHIFT    5

/* ================================================================
 * Helper: get the player index list pointer for a given team.
 * Returns the appropriate array (currentMatchPlayers, playerPositions,
 * or team->team.playerNumbers) and sets *outCount to the player count.
 * ================================================================ */
static uint8_t *getPlayerList(uint8_t *team, uint16_t *outCount) {
    if (team == (uint8_t *)&careerTeam) {
        *outCount = careerNumPlayers;
        return currentMatchPlayers;
    }
    if (team == (uint8_t *)&currentTeam) {
        *outCount = numPlayers;
        return playerPositions;
    }
    *outCount = 16;
    return ((TeamFileEx *)team)->team.playerNumbers;
}

/* ================================================================
 * MaybeDecayPlayerStatusBucket (static)
 * Probabilistically decays the status bucket field (bits 5-7) of a
 * player's cardsInjuries byte.
 *
 * Input: d1 = current bucket bits (already masked to 0xE0), a1 = player record
 * The pointer a1 is team_base + playerIndex * 38 (shifted).
 * ================================================================ */
static void maybeDecayPlayerStatusBucket(uint16_t bucketBits, PlayerFile *playerFile) {
    uint16_t d1 = bucketBits;

    if (d1 == 0x20) {
        /* Bucket level 1: 50% chance to decay to 0 */
        uint16_t r = _Rand2();
        if (r & 1)
            return;  /* no decay */
        d1 = 0;
    } else if (d1 == 0xC0) {
        /* Bucket level 6: 1/8 chance to decay to random level 1-5 */
        uint16_t r = _Rand2();
        if ((r & 7) != 0)
            return;  /* no decay */
        r = _Rand2();
        uint16_t newLevel = (uint16_t)((r * 5) >> 8);
        d1 = (newLevel << 5) + 0x20;
    } else if (d1 == 0xE0) {
        /* Bucket level 7: never decays */
        return;
    } else {
        /* Other levels: decrement by one step */
        d1 -= 0x20;
        if (d1 == 0x20) {
            /* Reached level 1 after decrement: 50% chance to clear to 0 */
            uint16_t r = _Rand2();
            if (r & 1) {
                /* keep at 1, write it */
            } else {
                d1 = 0;
            }
        }
    }

    /* Write back the new bucket value */
    uint8_t ci = playerFile->cardsInjuries;
    ci = (ci & 0x1F) | (uint8_t)d1;
    playerFile->cardsInjuries = ci;
}

/* ================================================================
 * MaybeDecayPlayerStatusBucketForTeam (static)
 * Iterates all players in a team and calls MaybeDecayPlayerStatusBucket
 * for those with nonzero status bucket bits.
 * ================================================================ */
static void maybeDecayPlayerStatusBucketForTeam(uint8_t *team) {
    TeamFileEx *teamEx = (TeamFileEx *)team;

    /* Skip computer-controlled teams */
    if (teamEx->team.teamControls == 1)
        return;

    if (team == (uint8_t *)&careerTeam) {
        /* Career team: use currentMatchPlayers for indirection */
        int16_t count = (int16_t)careerNumPlayers - 1;
        for (int16_t i = count; i >= 0; i--) {
            int16_t idx = (int8_t)currentMatchPlayers[i];
            PlayerFile *pf = &teamEx->players[idx];
            uint16_t bucket = (uint16_t)pf->cardsInjuries & CI_STATUS_BUCKET;
            if (bucket != 0) {
                maybeDecayPlayerStatusBucket(bucket, pf);
            }
        }
    } else {
        /* Non-career: iterate 16 players sequentially */
        PlayerFile *pf = teamEx->players;
        for (int16_t i = 15; i >= 0; i--) {
            uint16_t bucket = (uint16_t)pf->cardsInjuries & CI_STATUS_BUCKET;
            if (bucket != 0) {
                maybeDecayPlayerStatusBucket(bucket, pf);
            }
            pf++;
        }
    }
}

/* ================================================================
 * CyclePlayerStatusFlagsForTeam (static)
 * Decays injury severity (mode==0) or card severity (mode!=0) for
 * all players in a team.
 * ================================================================ */
static void cyclePlayerStatusFlagsForTeam(uint8_t *team, uint16_t mode) {
    TeamFileEx *teamEx = (TeamFileEx *)team;

    /* Skip if training game or computer-controlled team */
    if (trainingGame != 0)
        return;
    if (teamEx->team.teamControls == 1)
        return;

    uint8_t *playerList;
    int16_t count;

    if (team == (uint8_t *)&careerTeam) {
        playerList = currentMatchPlayers;
        count = (int16_t)careerNumPlayers - 1;
    } else {
        playerList = teamEx->team.playerNumbers;
        count = 15;
    }

    for (int16_t i = count; i >= 0; i--) {
        int16_t idx = (int8_t)playerList[i];
        PlayerFile *pf = &teamEx->players[idx];
        uint8_t ci = pf->cardsInjuries;

        if (mode == 0) {
            /* Decay injury bits (2-4) */
            uint16_t injury = (uint16_t)ci & CI_INJURY_BITS;
            if (injury == 0)
                continue;

            uint16_t newInjury;
            if (injury == 0x10)
                newInjury = 0x0C;       /* severity 4 → 3 */
            else if (injury == 0x0C)
                newInjury = 0x08;       /* severity 3 → 2 */
            else if (injury == 0x08)
                newInjury = 0x00;       /* severity 2 → 0 (healed) */
            else
                continue;               /* other values: skip */

            ci = (ci & 0xE3) | (uint8_t)newInjury;
            pf->cardsInjuries = ci;
        } else {
            /* Decay card bits (0-1) */
            uint16_t card = (uint16_t)ci & CI_CARD_BITS;
            if (card == 0)
                continue;

            uint16_t newCard;
            if (card == 2)
                newCard = 1;            /* card level 2 → 1 */
            else
                newCard = 0;            /* card level 1 or 3 → 0 */

            ci = (ci & 0xFC) | (uint8_t)newCard;
            pf->cardsInjuries = ci;
        }
    }
}

/* ================================================================
 * InitInGameTeams_PlayerControlled
 * Decays status buckets and cycles injury/card flags for both teams.
 * mode parameter is passed to CyclePlayerStatusFlagsForTeam.
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitInGameTeams_PlayerControlled(uint8_t *team1, uint8_t *team2, uint16_t mode) {
    maybeDecayPlayerStatusBucketForTeam(team1);
    maybeDecayPlayerStatusBucketForTeam(team2);
    cyclePlayerStatusFlagsForTeam(team1, mode);
    cyclePlayerStatusFlagsForTeam(team2, mode);
}

/* ================================================================
 * InitInGameTeams_WithMatchParam / InitInGameTeams_WithMatchParam_Alt
 * Same as PlayerControlled but loads mode from diyCompetitionStart->substitutesCount.
 * The Alt variant is an identical copy in the original game.
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitInGameTeams_WithMatchParam(uint8_t *team1, uint8_t *team2) {
    uint16_t mode = DIY_COMP(diyCompetitionStart)->substitutesCount;
    maybeDecayPlayerStatusBucketForTeam(team1);
    maybeDecayPlayerStatusBucketForTeam(team2);
    cyclePlayerStatusFlagsForTeam(team1, mode);
    cyclePlayerStatusFlagsForTeam(team2, mode);
}

/* ================================================================
 * ShouldApplyRandomPlayerStatusAfterSim
 * Returns 0 if in career mode with national selector and not a major
 * tournament. Returns 1 otherwise.
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _ShouldApplyRandomPlayerStatusAfterSim(void) {
    if (gameType == GAME_TYPE_CAREER) {
        if (isNationalSelector != 0) {
            uint16_t isMajor = _IsMajorTournamentAllowingRandomStatus();
            if (isMajor == 0) {
                return 0;
            }
        }
    }
    return 1;
}

/* ================================================================
 * BuildCareerTeamUnavailablePlayerList
 * Builds a list (max 4) of career team players who are injured.
 * ================================================================ */
__attribute__((used, externally_visible))
void _BuildCareerTeamUnavailablePlayerList(void) {
    uint16_t *list = (uint16_t *)careerUnavailablePlayerList;
    /* Init 4 entries to -1 (each is 4 bytes = 2 words) */
    list[0] = 0xFFFF;
    list[2] = 0xFFFF;
    list[4] = 0xFFFF;
    list[6] = 0xFFFF;

    if (_ShouldApplyRandomPlayerStatusAfterSim() != 0)
        return;

    uint16_t slotsLeft = 4;
    TeamFileEx *teamEx = &careerTeam;
    int16_t count = (int16_t)careerNumPlayers - 1;
    uint8_t *outPtr = careerUnavailablePlayerList;

    for (int16_t i = count; i >= 0; i--) {
        int16_t idx = (int8_t)currentMatchPlayers[i];
        PlayerFile *pf = &teamEx->players[idx];
        uint16_t injury = (uint16_t)pf->cardsInjuries & CI_INJURY_BITS;
        if (injury == 0)
            continue;

        /* Copy 4-byte entry from lookup table */
        uint16_t tableOff = (uint16_t)i * 4;
        uint8_t *src = careerUnavailablePlayerLookupTable + tableOff;
        outPtr[0] = src[0];
        outPtr[1] = src[1];
        outPtr[2] = src[2];
        outPtr[3] = src[3];
        outPtr += 4;

        slotsLeft--;
        if (slotsLeft == 0)
            return;
    }
}

/* ================================================================
 * ClearCareerMatchPlayerStatusBucket
 * Clears status bucket bits (5-7) for all career team players.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ClearCareerMatchPlayerStatusBucket(void) {
    TeamFileEx *teamEx = &careerTeam;
    int16_t count = (int16_t)careerNumPlayers - 1;

    for (int16_t i = count; i >= 0; i--) {
        int16_t idx = (int8_t)currentMatchPlayers[i];
        PlayerFile *pf = &teamEx->players[idx];
        uint8_t ci = pf->cardsInjuries;
        ci &= 0x1F;  /* clear bits 5-7 */
        pf->cardsInjuries = ci;
    }
}

/* ================================================================
 * IsPlayerInjured
 * playerBase = team + playerIndex * 38 (shifted pointer).
 * Checks injury (bits 2-4), cards (bits 0-1), and status bucket
 * (bits 5-7) based on mode:
 *   mode -1: check both injuries and cards
 *   mode  0: check injuries only
 *   mode  1: check cards only
 * Returns 1 if injured, 0 if not.
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _IsPlayerInjured(uint8_t *playerBase, int16_t mode) {
    PlayerFile *pf = (PlayerFile *)(playerBase + 76);
    uint8_t ci = pf->cardsInjuries;

    if (trainingGame != 0)
        goto check_bucket;

    if (mode != 1) {
        /* Check injury bits (2-4) */
        uint16_t injury = (uint16_t)ci & CI_INJURY_BITS;
        if (injury != 0 && injury != 4)
            return 1;  /* injured (severity > 1) */

        if (mode >= 0)
            goto check_bucket;
    }

    /* Check card bits (0-1) */
    {
        uint16_t card = (uint16_t)ci & CI_CARD_BITS;
        if (card != 0)
            return 1;  /* has card suspension */
    }

check_bucket:
    /* Check status bucket bits (5-7) */
    {
        uint16_t bucket = (uint16_t)ci & CI_STATUS_BUCKET;
        if (bucket == 0 || bucket == 0x20)
            return 0;  /* no bucket or minor (0x20) → not injured */
        return 1;      /* serious bucket → injured */
    }
}

/* ================================================================
 * GetNumberOfSubstitutes
 * Counts players with position == 0 (goalkeeper) in the team.
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _GetNumberOfSubstitutes(uint8_t *team, uint16_t numPl) {
    TeamFileEx *teamEx = (TeamFileEx *)team;
    int16_t iter = (int16_t)numPl - 1;
    uint8_t *playerList;
    uint16_t dummy;

    /* Three-way dispatch for player list source */
    if (team == (uint8_t *)&careerTeam) {
        playerList = currentMatchPlayers;
    } else if (team == (uint8_t *)&currentTeam) {
        playerList = playerPositions;
    } else {
        playerList = teamEx->team.playerNumbers;
    }

    uint16_t count = 0;
    for (; iter >= 0; iter--) {
        int16_t idx = (int8_t)playerList[iter];
        PlayerFile *pf = &teamEx->players[idx];
        uint16_t position = ((uint16_t)pf->positionAndFace & PF_POSITION_MASK) >> PF_POSITION_SHIFT;
        if (position == 0)
            count++;
    }
    return count;
}

/* ================================================================
 * GetNumberOfSubstitutesInSelectionList
 * Counts goalkeepers in selection list (indices 11 to extraCount+10).
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _GetNumberOfSubstitutesInSelectionList(uint8_t *team, uint16_t extraCount) {
    TeamFileEx *teamEx = (TeamFileEx *)team;
    int16_t iter = (int16_t)extraCount + 10;
    uint8_t *playerList;

    if (team == (uint8_t *)&careerTeam) {
        playerList = currentMatchPlayers;
    } else if (team == (uint8_t *)&currentTeam) {
        playerList = playerPositions;
    } else {
        playerList = teamEx->team.playerNumbers;
    }

    uint16_t count = 0;
    for (; iter >= 11; iter--) {
        int16_t idx = (int8_t)playerList[iter];
        PlayerFile *pf = &teamEx->players[idx];
        uint16_t position = ((uint16_t)pf->positionAndFace & PF_POSITION_MASK) >> PF_POSITION_SHIFT;
        if (position == 0)
            count++;
    }
    return count;
}

/* ================================================================
 * Common helper for availability checking.
 * Checks if a player is disabled based on injury/card/bucket bits.
 * Returns 1 if disabled, 0 if available.
 * ================================================================ */
static uint16_t isPlayerDisabled(PlayerFile *pf, int16_t excludeCardsFlag) {
    uint8_t ci = pf->cardsInjuries;

    /* Check status bucket bits (5-7) */
    uint16_t bucket = (uint16_t)ci & CI_STATUS_BUCKET;
    if (bucket != 0 && bucket != 0x20)
        return 1;  /* serious bucket → disabled */

    if (trainingGame != 0)
        return 0;  /* training games ignore injuries/cards */

    if (excludeCardsFlag != 1) {
        /* Check injury severity bits (2-4) */
        uint16_t injury = (uint16_t)ci & CI_INJURY_BITS;
        if (injury != 0 && injury != 4)
            return 1;  /* injured (severity > 1) */

        if (excludeCardsFlag >= 0)
            return 0;  /* mode 0: skip card check */
    }

    /* Check card bits (0-1) */
    uint16_t card = (uint16_t)ci & CI_CARD_BITS;
    if (card != 0)
        return 1;  /* has card → disabled */

    return 0;
}

/* ================================================================
 * GetNumberOfAvailablePlayers
 * Returns (disabled << 16) | (available & 0xFFFF).
 * ================================================================ */
__attribute__((used, externally_visible))
uint32_t _GetNumberOfAvailablePlayers(uint8_t *team, int16_t excludeCardsFlag, uint16_t numPl) {
    TeamFileEx *teamEx = (TeamFileEx *)team;
    uint8_t *playerList;
    uint16_t totalCount;

    if (team == (uint8_t *)&careerTeam) {
        playerList = currentMatchPlayers;
        totalCount = careerNumPlayers;
    } else if (team == (uint8_t *)&currentTeam) {
        playerList = playerPositions;
        totalCount = numPlayers;
    } else {
        playerList = teamEx->team.playerNumbers;
        totalCount = 16;
    }

    if (numPl != 0)
        totalCount = numPl;

    uint16_t disabled = 0;
    uint16_t available = totalCount;
    int16_t iter = (int16_t)totalCount - 1;

    for (; iter >= 0; iter--) {
        int16_t idx = (int8_t)playerList[iter];
        PlayerFile *pf = &teamEx->players[idx];
        if (isPlayerDisabled(pf, excludeCardsFlag)) {
            disabled++;
            available--;
        }
    }

    return ((uint32_t)disabled << 16) | (uint32_t)available;
}

/* ================================================================
 * GetNumberOfAvailableSubstitutes
 * Counts available goalkeeper-position players.
 * Returns (disabled << 16) | (available & 0xFFFF).
 * ================================================================ */
__attribute__((used, externally_visible))
uint32_t _GetNumberOfAvailableSubstitutes(uint8_t *team, int16_t flag) {
    TeamFileEx *teamEx = (TeamFileEx *)team;
    uint8_t *playerList;
    uint16_t totalCount;

    if (team == (uint8_t *)&careerTeam) {
        playerList = currentMatchPlayers;
        totalCount = careerNumPlayers;
    } else if (team == (uint8_t *)&currentTeam) {
        playerList = playerPositions;
        totalCount = numPlayers;
    } else {
        playerList = teamEx->team.playerNumbers;
        totalCount = 16;
    }

    uint16_t disabled = 0;
    uint16_t goalkeepers = 0;
    int16_t iter = (int16_t)totalCount - 1;

    for (; iter >= 0; iter--) {
        int16_t idx = (int8_t)playerList[iter];
        PlayerFile *pf = &teamEx->players[idx];

        /* Check if goalkeeper (position == 0) */
        uint16_t position = ((uint16_t)pf->positionAndFace & PF_POSITION_MASK) >> PF_POSITION_SHIFT;
        if (position != 0)
            continue;

        goalkeepers++;
        if (isPlayerDisabled(pf, flag)) {
            disabled++;
            goalkeepers--;
        }
    }

    return ((uint32_t)disabled << 16) | (uint32_t)goalkeepers;
}

/* ================================================================
 * GetNumberOfAvailableStartingPlayers
 * Counts available non-goalkeeper players. Uses career path only
 * (does not check currentTeam).
 * Returns (disabled << 16) | (available & 0xFFFF).
 * ================================================================ */
__attribute__((used, externally_visible))
uint32_t _GetNumberOfAvailableStartingPlayers(uint8_t *team, int16_t flag) {
    TeamFileEx *teamEx = (TeamFileEx *)team;
    uint8_t *playerList;
    uint16_t totalCount;

    if (team == (uint8_t *)&careerTeam) {
        playerList = currentMatchPlayers;
        totalCount = careerNumPlayers;
    } else {
        playerList = teamEx->team.playerNumbers;
        totalCount = 16;
    }

    uint16_t disabled = 0;
    uint16_t starters = 0;
    int16_t iter = (int16_t)totalCount - 1;

    for (; iter >= 0; iter--) {
        int16_t idx = (int8_t)playerList[iter];
        PlayerFile *pf = &teamEx->players[idx];

        /* Check if NOT goalkeeper (position != 0) */
        uint16_t position = ((uint16_t)pf->positionAndFace & PF_POSITION_MASK) >> PF_POSITION_SHIFT;
        if (position == 0)
            continue;

        starters++;
        if (isPlayerDisabled(pf, flag)) {
            disabled++;
            starters--;
        }
    }

    return ((uint32_t)disabled << 16) | (uint32_t)starters;
}

/* ================================================================
 * GetAgeAdjustedPlayerFormFactor
 * playerBase = team + playerIndex * 38 (shifted pointer).
 * Reads age (price field at +0x6C), form (someFlag at +0x71),
 * position (positionAndFace at +0x66).
 * Returns form / ageDivisor.
 * ================================================================ */

static const uint8_t kPlayerAgeToFormDivisorTable[50] = {
    0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,
    0x04,0x04,0x05,0x05,0x05,0x05,0x05,0x05,0x06,0x06,
    0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x08,
    0x08,0x08,0x08,0x08,0x08,0x09,0x09,0x09,0x0A,0x0A,
    0x0C,0x0C,0x0C,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F
};

__attribute__((used, externally_visible))
int16_t _GetAgeAdjustedPlayerFormFactor(uint8_t *playerBase) {
    PlayerFile *pf = (PlayerFile *)(playerBase + 76);
    int8_t age = pf->price;
    int16_t ageDivisor = (int16_t)(uint8_t)kPlayerAgeToFormDivisorTable[(uint8_t)age];

    int16_t formValue;
    uint8_t flagByte = (uint8_t)pf->someFlag;

    if (flagByte == 0x9B || flagByte == 0x9A) {
        /* Special career form: compute player index within careerTeam */
        uint32_t offset = (uint32_t)((uint32_t)playerBase - (uint32_t)&careerTeam);
        uint16_t playerIdx = offset / 38;
        formValue = (int16_t)(int8_t)playerFormModifiers[playerIdx];
        formValue <<= 1;  /* * 2 */
    } else {
        formValue = (int16_t)(int8_t)flagByte;
        /* If bit 0 of positionAndFace is set, double the form */
        if (pf->positionAndFace & 0x01) {
            formValue <<= 1;
        }
    }

    /* divs.w d1,d0: signed 32÷16 division */
    int32_t num = (int32_t)formValue;
    int16_t result = (int16_t)(num / ageDivisor);
    return result;
}

/* ================================================================
 * Chairman message constants
 * ================================================================ */
#define CHAIRMAN_BANK_BALANCE_OVERDRAFT      5
#define CHAIRMAN_OVERDRAFT_LAST_WARNING      6
#define CHAIRMAN_CLEAR_OVERDRAFT_IMMEDIATELY 7
#define CHAIRMAN_YOURE_SACKED                8
#define CHAIRMAN_IMPROVE_LEAGUE_POSITION     9
#define CHAIRMAN_IMPROVE_LEAGUE_POSITION_SOON 10
#define CHAIRMAN_THREE_MORE_MATCHES          11
#define CHAIRMAN_YOURE_SACKED_2              12
#define CHAIRMAN_WE_DIDNT_RENEW_YOUR_CONTRACT 38

/* ================================================================
 * Data tables (moved from asm to static const in C)
 * ================================================================ */
static const uint16_t leagueMatchReceiptsMultiplierTable[6] = {
    400, 300, 200, 150, 125, 100
};

static const uint16_t cupMatchReceiptsMultiplierTable[6] = {
    800, 600, 400, 300, 250, 200
};

static const int16_t seasonReviewConfidenceDeltaTable[5] = {
    50, 25, 0, -25, -50
};

static const int16_t nationalSelectorSeasonReviewReputationDeltaTable[5] = {
    5, 2, 0, -2, -5
};

/* ================================================================
 * Group A: Simple utility functions
 * ================================================================ */

/* ClampNewBalanceAndUpdateClubStatus
 * Clamp balance to 100M, set clubBusinessActive via smi, manage overdraftStreak.
 */
__attribute__((used, externally_visible))
void _ClampNewBalanceAndUpdateClubStatus(void) {
    if (newBalance > 100000000)
        newBalance = 100000000;
    /* smi: set byte to -1 if negative, 0 if positive */
    clubBusinessActive = (newBalance < 0) ? -1 : 0;
    if (newBalance < 0)
        overdraftStreak++;
    else
        overdraftStreak = 0;
}

/* ClearOverdraftStateIfRecovered */
__attribute__((used, externally_visible))
void _ClearOverdraftStateIfRecovered(void) {
    if (overdraftWarningState != 0) {
        if (newBalance >= 0)
            overdraftWarningState = 0;
    }
}

/* SetOverdraftWarningCooldown — random 4-8 */
static void setOverdraftWarningCooldown(void) {
    uint16_t r = _Rand2();
    uint16_t val = (uint16_t)((r * 5) >> 8) + 4;
    overdraftWarningCooldown = val;
}

/* SetPerformanceWarningCooldown — random 4-8 */
static void setPerformanceWarningCooldown(void) {
    uint16_t r = _Rand2();
    uint16_t val = (uint16_t)((r * 5) >> 8) + 4;
    performanceWarningCooldown = val;
}

/* ================================================================
 * Group B: Tournament check functions
 * ================================================================ */

/* IsMajorInternationalTournament
 * Returns 1 if current/secondary competition is a major international tournament.
 * Sets *outCtx to the appropriate competition context pointer.
 */
__attribute__((used, externally_visible))
uint16_t _IsMajorInternationalTournament(uint8_t **outCtx) {
    uint32_t curComp = currentCompetitionPtr;

    /* Check primary competition against 9 tournament types */
    if (curComp == (uint32_t)worldCup2Copy ||
        curComp == (uint32_t)europeanChampionships2Copy ||
        curComp == (uint32_t)copaAmericaCopy ||
        curComp == (uint32_t)africanNationsCupCopy ||
        curComp == (uint32_t)northAmericanCupCopy ||
        curComp == (uint32_t)asianCupCopy ||
        curComp == (uint32_t)oceaniaCupCopy ||
        curComp == (uint32_t)invitationTournamentCopy ||
        curComp == (uint32_t)tourCopy) {
        *outCtx = competitionFileBuffer;
        return 1;
    }

    /* Check secondary competition against 8 tournament types (no worldCup) */
    uint32_t secComp = secondaryCompetitionPtr;
    if (secComp == (uint32_t)europeanChampionships2Copy ||
        secComp == (uint32_t)copaAmericaCopy ||
        secComp == (uint32_t)africanNationsCupCopy ||
        secComp == (uint32_t)northAmericanCupCopy ||
        secComp == (uint32_t)asianCupCopy ||
        secComp == (uint32_t)oceaniaCupCopy ||
        secComp == (uint32_t)invitationTournamentCopy ||
        secComp == (uint32_t)tourCopy) {
        *outCtx = competitionContext;
        return 1;
    }

    return 0;
}

/* IsMajorTournamentAllowingRandomStatus
 * Wraps IsMajorInternationalTournament, checks offset 0x44.
 * Returns nonzero if major tournament allows random status.
 */
__attribute__((used, externally_visible))
uint16_t _IsMajorTournamentAllowingRandomStatus(void) {
    uint8_t *ctx;
    uint16_t isMajor = _IsMajorInternationalTournament(&ctx);
    if (isMajor == 0)
        return 0;
    return *(uint16_t *)(ctx + 0x44);
}

/* IsMajorTournamentAllowingSquadChanges
 * Wraps IsMajorInternationalTournament, checks offset 0x46.
 */
__attribute__((used, externally_visible))
uint16_t _IsMajorTournamentAllowingSquadChanges(void) {
    uint8_t *ctx;
    uint16_t isMajor = _IsMajorInternationalTournament(&ctx);
    if (isMajor == 0)
        return 0;
    return *(uint16_t *)(ctx + 0x46);
}

/* IsTournamentAllowingSquadChangesIfNotCareer */
__attribute__((used, externally_visible))
uint16_t _IsTournamentAllowingSquadChangesIfNotCareer(void) {
    if (isCareer != 0)
        return 0;
    return _IsMajorTournamentAllowingSquadChanges();
}

/* ShouldSetupPlayers */
__attribute__((used, externally_visible))
uint16_t _ShouldSetupPlayers(void) {
    if (gameType == GAME_TYPE_CAREER) {
        if (isNationalSelector != 0) {
            uint16_t allows = _IsMajorTournamentAllowingSquadChanges();
            if (allows == 0)
                return 0;
        }
    }
    return 1;
}

/* ================================================================
 * Group C: Gate receipt calculations
 * ================================================================ */

/* CalcLeagueMatchGateReceiptsFactor (static)
 * Computes gate receipts factor for league matches from team values.
 * Returns factor in d7. Panics on 16-bit overflow.
 */
static uint16_t calcLeagueMatchGateReceiptsFactor(uint8_t *team1, uint8_t *team2) {
    uint32_t d0;

    /* team1 value */
    if (team1 == (uint8_t *)&careerTeam) {
        d0 = (uint32_t)totalTeamValue;
    } else {
        d0 = _CGetTeamValueForGateReceipts(team1);
    }
    d0 = (uint16_t)(d0 / 4000);    /* divu #$FA0 */
    d0 = (uint32_t)(uint16_t)d0 * 10000;  /* mulu #$2710 */
    uint32_t d7 = d0;

    /* team2 value */
    if (team2 == (uint8_t *)&careerTeam) {
        d0 = (uint32_t)totalTeamValue;
    } else {
        d0 = _CGetTeamValueForGateReceipts(team2);
    }
    d0 = (uint16_t)(d0 / 4000);    /* divu #$FA0 */
    d0 = (uint32_t)(uint16_t)d0 * 2000;   /* mulu #$7D0 */
    d7 += d0;

    d7 >>= 4;   /* lsr.l #4 */

    /* divu.w teamValueDivisor — check for 16-bit overflow */
    if (teamValueDivisor == 0 || d7 > 0xFFFFFFFFu) {
        /* bvs → panic */
        panicReasonCode = 0x9232;
        _PanicHaltLoop();
        return 0; /* unreachable */
    }
    uint32_t quotient = d7 / (uint32_t)teamValueDivisor;
    uint16_t remainder = (uint16_t)(d7 % (uint32_t)teamValueDivisor);
    /* bvs checks if quotient > 0xFFFF (overflow in 16-bit result) */
    if (quotient > 0xFFFF) {
        panicReasonCode = 0x9232;
        _PanicHaltLoop();
        return 0;
    }
    return (uint16_t)quotient;
}

/* CalcCupMatchGateReceiptsFactor (static)
 * Same as league but with different multipliers.
 */
static uint16_t calcCupMatchGateReceiptsFactor(uint8_t *team1, uint8_t *team2) {
    uint32_t d0;

    if (team1 == (uint8_t *)&careerTeam) {
        d0 = (uint32_t)totalTeamValue;
    } else {
        d0 = _CGetTeamValueForGateReceipts(team1);
    }
    d0 = (uint16_t)(d0 / 4000);    /* divu #$FA0 */
    d0 = (uint32_t)(uint16_t)d0 * 6000;   /* mulu #$1770 */
    uint32_t d7 = d0;

    if (team2 == (uint8_t *)&careerTeam) {
        d0 = (uint32_t)totalTeamValue;
    } else {
        d0 = _CGetTeamValueForGateReceipts(team2);
    }
    d0 = (uint16_t)(d0 / 4000);
    d0 = (uint32_t)(uint16_t)d0 * 6000;
    d7 += d0;

    d7 >>= 4;

    if (teamValueDivisor == 0 || d7 > 0xFFFFFFFFu) {
        panicReasonCode = 0x9232;
        _PanicHaltLoop();
        return 0;
    }
    uint32_t quotient = d7 / (uint32_t)teamValueDivisor;
    if (quotient > 0xFFFF) {
        panicReasonCode = 0x9232;
        _PanicHaltLoop();
        return 0;
    }
    return (uint16_t)quotient;
}

/* UpdateCareerFinancesAfterMatch
 * Complex dispatcher that calculates gate receipts based on competition type.
 */
__attribute__((used, externally_visible))
void _UpdateCareerFinancesAfterMatch(uint8_t *ctx, uint8_t *team) {
    if (isNationalSelector != 0)
        return;

    uint8_t *a3 = ctx;
    int16_t compType = *(int16_t *)(a3 + 0x2C);
    uint32_t d7;

    if (compType == 0) {
        goto league_match;
    } else if (compType == 4) {
        goto cup_match_type4;
    } else if (compType == 1 || compType == 2) {
        goto cup_match_type1or2;
    } else if (compType == 3) {
        goto diy_comp_match;
    } else {
        panicReasonCode = 0x9231;
        _PanicHaltLoop();
        return;
    }

league_match:
    d7 = calcLeagueMatchGateReceiptsFactor(team, team + 0 /* a2 not used directly, see asm */);
    /* Actually, asm passes a1=team, a2=team2 separately. For the C version,
       the league path doesn't use the factor from the cup table;
       instead it computes a full gate receipt factor from team rank. */
    {
        int16_t isDiy = *(int16_t *)(a3 + 0x48);
        if (isDiy != 0) {
            /* DIY league: check group state */
            uint16_t groupIdxField = (uint16_t)DIY_COMP(diyFileBufferCopy)->currentRound;
            uint16_t offset = groupIdxField * 0x76;
            uint8_t *groupPtr = a3 + offset;
            if (*(int16_t *)(groupPtr + 0x15E) != 0)
                goto compute_receipts;
            if (*(int16_t *)(groupPtr + 0x16E) == 2)
                goto compute_receipts;
            goto increment_count;
        }

        /* Non-DIY league */
        if (*(int16_t *)a3 != 0) {
            panicReasonCode = 0x9235;
            _PanicHaltLoop();
            return;
        }

        /* Compute gate receipt factor based on division and rank */
        int16_t d3, d4;
        if (currentDivisionIndex == 0) {
            d3 = 0x52; d4 = 0x7D;
        } else if (currentDivisionIndex == 1) {
            d3 = 0x55; d4 = 0x81;
        } else if (currentDivisionIndex == 2) {
            d3 = 0x59; d4 = 0x78;
        } else {
            d3 = 0x58; d4 = 0x71;
        }

        /* Find team rank in league table */
        uint16_t *leagueTable = (uint16_t *)(a3 + 0x12E);
        int16_t d1 = -2;
        do {
            d1 += 2;
            int16_t tableEntry = *(int16_t *)(a3 + 0x6E + d1);
            int32_t extended = (int32_t)tableEntry;
            uint16_t divided = (uint16_t)((uint32_t)extended / 18);
            uint16_t shifted = divided << 1;
            /* cmp.w (a0,d0.w),d2 */
        } while (leagueTable[((*(int16_t *)(a3 + 0x6E + d1)) >= 0 ?
                 (uint16_t)((uint32_t)(int32_t)(*(int16_t *)(a3 + 0x6E + d1)) / 18) :
                 (uint16_t)((uint32_t)(int32_t)(*(int16_t *)(a3 + 0x6E + d1)) / 18)) << 0]
                 != selTeamNumber);
        /* Simplify: search for our team in the sorted league table */
        d1 >>= 1;   /* lsr.w #1,d1 → team rank (0-based from bottom) */

        int16_t numTeamsInDiv = *(int16_t *)(a3 + 0x32);
        int16_t rankDelta = numTeamsInDiv - d1 - 1;
        int16_t numTeamsMinus1 = numTeamsInDiv - 1;
        uint16_t pct = (uint16_t)((uint32_t)((uint16_t)rankDelta * 100) / (uint16_t)numTeamsMinus1);
        int16_t range = d4 - d3;
        uint16_t factorFromRange = (uint16_t)((uint32_t)(pct * (uint16_t)range) / 100);
        int16_t receiptIndex = d3 + (int16_t)factorFromRange;
        ballSpeed = (uint16_t)receiptIndex;

        /* Multiply by crowd factor from ctx+0x30, divide by avg player price */
        int8_t crowdFactor = *(int8_t *)(a3 + 0x30);
        uint32_t product = (uint32_t)(uint16_t)receiptIndex * (uint16_t)(int16_t)crowdFactor;
        uint16_t avgPrice = _CGetTeamValueForGateReceipts((uint8_t *)&careerTeam);
        if (avgPrice == 0) avgPrice = 1; /* safety */
        uint16_t priceRatio = (uint16_t)(product / (uint32_t)avgPrice);
        int16_t gateFactorResult = (int16_t)((int16_t)d7 * (int16_t)priceRatio) / 100;
        gateReceiptsFactor = gateFactorResult;
        d7 = (uint32_t)(uint16_t)gateFactorResult;
        goto compute_receipts;
    }

cup_match_type4:
    /* compType 4 falls through to cupMatchType1or2 */
cup_match_type1or2:
    d7 = calcCupMatchGateReceiptsFactor(team, team);
    {
        if (compType != 4) {
            /* compType 1 or 2: cup match */
            if (*(int16_t *)a3 != 1) {
                panicReasonCode = 0x9236;
                _PanicHaltLoop();
                return;
            }
        }
        int16_t totalRounds = *(int16_t *)(a3 + 0x5A);
        int16_t currentRound = *(int16_t *)(a3 + 0x58);
        int16_t roundIndex = totalRounds - 1 - currentRound;
        d7 = (uint16_t)((uint32_t)(d7 * leagueMatchReceiptsMultiplierTable[roundIndex]) / 100);

        /* Check match result */
        int16_t matchSlot = currentRound << 1;
        if (*(int16_t *)(a3 + 0x66 + matchSlot) == 2)
            goto compute_receipts;
        goto increment_count;
    }

diy_comp_match:
    d7 = calcCupMatchGateReceiptsFactor(team, team);
    {
        int16_t isDiy = *(int16_t *)(a3 + 0x48);
        if (isDiy == 0) {
            /* Non-DIY comp type 3 */
            if (*(int16_t *)a3 == 0)
                goto compute_receipts;
            if (*(int16_t *)a3 == 1) {
                /* Cup match path */
                int16_t totalRounds = *(int16_t *)(a3 + 0x5A);
                int16_t currentRound = *(int16_t *)(a3 + 0x58);
                int16_t roundIndex = totalRounds - 1 - currentRound;
                d7 = (uint16_t)((uint32_t)(d7 * cupMatchReceiptsMultiplierTable[roundIndex]) / 100);
                int16_t matchSlot = currentRound << 1;
                if (*(int16_t *)(a3 + 0x66 + matchSlot) == 2)
                    goto compute_receipts;
                goto increment_count;
            }
            panicReasonCode = 0x9239;
            _PanicHaltLoop();
            return;
        } else {
            /* DIY competition with group stages
             * NOTE: IDA bug at original L28446-28454. The original used
             * lea ($2DAD0).l,a0 which should be diyFileBufferCopy.
             * Also the second currentRound read used a0=cupMatchReceiptsMultiplierTable
             * but should have been diyFileBufferCopy->currentRound (offset $54).
             */
            uint16_t totalRoundsField = (uint16_t)DIY_COMP(diyFileBufferCopy)->totalRounds;
            uint16_t curRoundField = (uint16_t)DIY_COMP(diyFileBufferCopy)->currentRound;
            int16_t roundIndex = (int16_t)(totalRoundsField - 1 - curRoundField);
            d7 = (uint16_t)((uint32_t)(d7 * cupMatchReceiptsMultiplierTable[roundIndex]) / 100);

            /* Use curRoundField as group index to check match state */
            uint16_t offset = curRoundField * 0x76;
            uint8_t *groupPtr = a3 + offset;
            if (*(int16_t *)(groupPtr + 0x15E) != 0)
                goto compute_receipts;
            if (*(int16_t *)(groupPtr + 0x16E) == 2)
                goto compute_receipts;
            goto increment_count;
        }
    }

compute_receipts:
    if (team == (uint8_t *)&careerTeam) {
        d7 <<= 1;   /* double for career team home matches */
        seasonMatchReceiptsCount++;
    } else {
        d7 = 0;
        goto final_update;
    }

increment_count:
    seasonMatchReceiptsCount++;

final_update:
    d7 &= 0xFFFF;
    d7 <<= 4;
    gateReceipts += (int32_t)d7;
    newBalance += (int32_t)d7;
    seasonGateReceiptsAccum2 += (int32_t)d7;
    seasonGateReceiptsAccum += (int32_t)d7;
    _ClampNewBalanceAndUpdateClubStatus();
    _ClearOverdraftStateIfRecovered();
}

/* ================================================================
 * Group D: Chairman warning system
 * ================================================================ */

/* MaybeSackManagerAfterSeasonReview */
__attribute__((used, externally_visible))
void _MaybeSackManagerAfterSeasonReview(void) {
    if (stayingInOldClub == 0)
        return;
    if (wonWorldCup != 0)
        return;
    if (wonContinentalCup != 0)
        return;
    if (qualifyingStage1PendingFlag0 != 0)
        return;
    if (qualifyingStage1PendingFlag1 != 0)
        return;
    if (seasonReview < 2)
        return;

    uint16_t randVal = _Rand();
    uint16_t threshold;
    if (seasonReview == 2)
        threshold = 0x20;
    else if (seasonReview == 3)
        threshold = 0x55;
    else
        threshold = 0xAA;

    if (randVal > threshold)
        return;

    currentMessage = CHAIRMAN_WE_DIDNT_RENEW_YOUR_CONTRACT;
    _CShowChairmanMessage();
    youreSacked = 1;
}

/* Helper: check if secondary/tertiary comps are finished (blocks escalation) */
static uint16_t areActiveCompsFinished(void) {
    if (secondaryCompetitionPtr != 0) {
        if (secondaryCompFinished != 0)
            return 1;
    }
    if (tertiaryCompetitionPtr != 0) {
        if (tertiaryCompFinished != 0)
            return 1;
    }
    return 0;
}

/* UpdateCareerChairmanWarningsAfterMatch */
__attribute__((used, externally_visible))
void _UpdateCareerChairmanWarningsAfterMatch(void) {
    if (isNationalSelector != 0)
        return;

    /* Decrement cooldowns */
    if (overdraftWarningCooldown != 0)
        overdraftWarningCooldown--;
    if (performanceWarningCooldown != 0)
        performanceWarningCooldown--;

    /* Get team rank in DIY league */
    uint16_t d7 = _CFindTeamInDIY(selTeamNumber, competitionFileBuffer);

    /* Compute performance pressure factor */
    int16_t viewPhase = (int16_t)(int8_t)viewRandomPhase;
    int16_t bias = (int16_t)careerJobOfferPhaseBias;
    int16_t delta = (int16_t)(viewPhase - bias);
    int16_t pressureFactor = (int16_t)((int32_t)(delta << 2) / (int32_t)wageScalingDivisor);

    if (pressureFactor == 0)
        goto skip_performance_warning;

    /* Check if league warnings are applicable */
    if (*(int16_t *)competitionFileBuffer != 2) {
        if (leagueWarningsEnabled == 0)
            goto skip_performance_warning;
        /* Check if team is in relegation zone */
        uint16_t relegationBoundary = leaguePromotionCount + leagueRelegationCount;
        if (relegationBoundary == 0)
            relegationBoundary = 1;
        uint16_t boundary = numDiyTeams - relegationBoundary;
        if (d7 < boundary)
            goto check_expected_rank;
    }

    goto performance_warning_active;

check_expected_rank:
    {
        int16_t expRank = (int16_t)expectedLeagueRankIndex;
        int16_t rankDiff = expRank - (int16_t)d7;
        int32_t pct = ((int32_t)rankDiff * 100) / (int32_t)(int16_t)numDiyTeams;
        if (pct > -25)
            goto clear_performance_warning;
    }

performance_warning_active:
    {
        uint16_t cooldown = performanceWarningCooldown;
        if (performanceWarningState == 0) {
            /* State 0: first warning */
            if (cooldown != 0)
                goto done_performance;
            performanceWarningState = 1;
            setPerformanceWarningCooldown();
            currentMessage = CHAIRMAN_IMPROVE_LEAGUE_POSITION;
            _CShowChairmanMessage();
            return;
        }
        if (performanceWarningState == 1) {
            /* State 1: second warning */
            if (cooldown != 0)
                goto done_performance;
            performanceWarningState = 2;
            goto issue_improve_league_soon;
        }
        if (performanceWarningState == 2) {
            /* State 2: third warning or escalate */
            if (cooldown != 0)
                goto done_performance;
            if (overdraftWarningState >= 3)
                goto random_escalate;
            if (d7 == 0)
                goto random_escalate;
            if (careerTeamLostMatch != 0)
                goto random_escalate;
            /* Check if secondary/tertiary comps block escalation */
            if (secondaryCompetitionPtr != 0 && secondaryCompFinished != 0)
                goto random_escalate;
            if (tertiaryCompetitionPtr != 0 && tertiaryCompFinished != 0)
                goto random_escalate;
            /* Issue 3-more-matches warning */
            performanceWarningState = 3;
            setPerformanceWarningCooldown();
            currentMessage = CHAIRMAN_THREE_MORE_MATCHES;
            _CShowChairmanMessage();
            return;
        }
        /* State >= 3: countdown to sacking */
        if (d7 == 0) {
            performanceWarningState = 3;
            goto done_performance;
        }
        performanceWarningState++;
        if (performanceWarningState < 6)
            goto done_performance;
        if (careerTeamLostMatch != 0)
            goto done_performance;
        if (competitionSlotUpdatedFlag != 0)
            goto done_performance;
        /* Sacked for poor performance */
        currentMessage = CHAIRMAN_YOURE_SACKED_2;
        _CShowChairmanMessage();
        youreSacked = 1;
        return;
    }

random_escalate:
    /* 50% chance to escalate or re-issue warning */
    if ((currentGameTick & 0x04) == 0) {
issue_improve_league_soon:
        setPerformanceWarningCooldown();
        currentMessage = CHAIRMAN_IMPROVE_LEAGUE_POSITION_SOON;
        _CShowChairmanMessage();
        return;
    }
    goto done_performance;

clear_performance_warning:
    performanceWarningState = 0;

skip_performance_warning:
    setPerformanceWarningCooldown();

done_performance:
    /* Overdraft warning section */
    _ClearOverdraftStateIfRecovered();

    if (newBalance >= 0) {
        setOverdraftWarningCooldown();
        return;
    }

    /* Check if league warnings are applicable */
    if (*(int16_t *)competitionFileBuffer != 2) {
        if (leagueWarningsEnabled == 0)
            return;
    }

    {
        uint16_t cooldown = overdraftWarningCooldown;
        if (overdraftWarningState == 0) {
            if (cooldown != 0)
                return;
            overdraftWarningState = 1;
            setOverdraftWarningCooldown();
            currentMessage = CHAIRMAN_BANK_BALANCE_OVERDRAFT;
            _CShowChairmanMessage();
            return;
        }
        if (overdraftWarningState == 1) {
            if (cooldown != 0)
                return;
            goto issue_overdraft_last_warning;
        }
        if (overdraftWarningState == 2) {
            if (cooldown != 0)
                return;
            if (performanceWarningState >= 3)
                goto issue_overdraft_last_warning;
            if (d7 == 0)
                goto issue_overdraft_last_warning;
            if (careerTeamLostMatch != 0)
                goto issue_overdraft_last_warning;
            if (secondaryCompetitionPtr != 0 && secondaryCompFinished != 0)
                goto issue_overdraft_last_warning;
            if (tertiaryCompetitionPtr != 0 && tertiaryCompFinished != 0)
                goto issue_overdraft_last_warning;
            /* Clear overdraft immediately */
            overdraftWarningState = 3;
            setOverdraftWarningCooldown();
            currentMessage = CHAIRMAN_CLEAR_OVERDRAFT_IMMEDIATELY;
            _CShowChairmanMessage();
            return;
        }
        /* State >= 3: countdown to sacking */
        if (d7 == 0) {
            overdraftWarningState = 3;
            return;
        }
        overdraftWarningState++;
        if (overdraftWarningState < 6)
            return;
        if (careerTeamLostMatch != 0)
            return;
        if (competitionSlotUpdatedFlag != 0)
            return;
        currentMessage = CHAIRMAN_YOURE_SACKED;
        _CShowChairmanMessage();
        youreSacked = 1;
        return;
    }

issue_overdraft_last_warning:
    overdraftWarningState = 2;
    setOverdraftWarningCooldown();
    currentMessage = CHAIRMAN_OVERDRAFT_LAST_WARNING;
    _CShowChairmanMessage();
}

/* UpdateCareerAfterMatch — orchestrator */
__attribute__((used, externally_visible))
void _UpdateCareerAfterMatch(void) {
    _ClampNewBalanceAndUpdateClubStatus();
    _ClearOverdraftStateIfRecovered();
    _UpdateCareerChairmanWarningsAfterMatch();
    if (removedOfferPlayersFromSelectedTeamCount != 0) {
        removedOfferPlayersFromSelectedTeamCount--;
        _CAddCareerTeamPlayer();
    }
    _CCheckPlayersLeavingComing();
}

/* ================================================================
 * Group E: Weekly/season finances
 * ================================================================ */

/* ApplyWeeklyFinancialDeductions */
__attribute__((used, externally_visible))
void _ApplyWeeklyFinancialDeductions(void) {
    uint8_t *listPtr = _CTeamSelectionKeyCache_GetCurrentListPtr();
    uint8_t currentWeek = listPtr[1];
    uint8_t lastWeek = lastFinancialWeekIndex;
    int16_t diff = (int16_t)(uint16_t)currentWeek - (int16_t)(uint16_t)lastWeek;
    if (diff < 0)
        goto update_and_clamp;
    if (diff == 0)
        goto save_week_and_clamp;

    /* Deduct for (diff) weeks, counting from 0..diff-1 */
    int16_t weeks = diff - 1;
    for (int16_t i = weeks; i >= 0; i--) {
        int32_t wages = weeklyPlayerWages;
        playerWagesBill += wages;
        newBalance -= wages;
        seasonWagesAccum += wages;
        int32_t running = weeklyRunningCosts;
        generalRunningCosts += running;
        newBalance -= running;
        seasonRunningCostsAccum += running;
        seasonRunningCostsAccum2 += running;
        seasonWeeksDeducted++;
    }

save_week_and_clamp:
    listPtr = _CTeamSelectionKeyCache_GetCurrentListPtr();
    lastFinancialWeekIndex = listPtr[1];

update_and_clamp:
    _ClampNewBalanceAndUpdateClubStatus();
    _ClearOverdraftStateIfRecovered();
}

/* GetExpectedLeagueRankIndex (static helper)
 * Computes expected league rank from competition context.
 * a6 = competition context buffer, uses d2 from GetMoreExpensiveTeamsCount.
 */
static uint16_t getExpectedLeagueRankIndex(uint8_t *ctx, uint16_t moreExpensiveCount) {
    uint16_t d3 = *(uint16_t *)(ctx + 0x32);
    uint16_t compType = *(uint16_t *)ctx;

    if (compType == 0) {
        /* League: (numTeams-1) * multiplier from offset 0x5E */
        uint16_t d7 = d3 - 1;
        d7 = (uint16_t)(d7 * *(uint16_t *)(ctx + 0x5E));
        return d7;
    }

    if (compType == 1) {
        /* Cup: walk through rounds halving d3, counting d7 */
        uint16_t d7 = 0;
        uint16_t *roundPtr = (uint16_t *)(ctx + 0x66);
        for (;;) {
            d7++;
            if (*roundPtr++ == 2)
                d7++;
            d3 >>= 1;
            if (moreExpensiveCount >= d3)
                return d7;
            if (d3 == 1)
                return d7;
        }
    }

    /* DIY (compType >= 2): walk through groups */
    {
        uint16_t numGroups = *(uint16_t *)(ctx + 0x52);
        uint8_t *groupPtr = ctx;
        uint16_t d7 = 0;

        for (uint16_t i = numGroups; i > 0; i--) {
            if (*(int16_t *)(groupPtr + 0x15E) != 0) {
                uint16_t numRounds = *(uint16_t *)(groupPtr + 0x16C);
                uint16_t multiplier = *(uint16_t *)(groupPtr + 0x166);
                uint16_t val = (numRounds - 1) * multiplier;
                d7 += val;
            } else {
                d7++;
                if (*(int16_t *)(groupPtr + 0x16E) == 2)
                    d7++;
            }
            if (moreExpensiveCount >= *(uint16_t *)(groupPtr + 0x164))
                return d7;
            groupPtr += 0x76;
        }
        return d7;
    }
}

/* InitializeCareerSeasonFinancials */
__attribute__((used, externally_visible))
void _InitializeCareerSeasonFinancials(void) {
    /* Get more expensive teams count and set expected rank */
    uint16_t moreExpensiveCount = _CGetMoreExpensiveTeamsCount(selTeamNumber, competitionFileBuffer);
    expectedLeagueRankIndex = moreExpensiveCount;

    /* Calculate team value divisor from all active competitions */
    uint16_t d5 = getExpectedLeagueRankIndex(competitionFileBuffer, moreExpensiveCount);

    if (secondaryCompetitionPtr != 0) {
        uint16_t mec2 = _CGetMoreExpensiveTeamsCount(selTeamNumber, competitionContext);
        d5 += getExpectedLeagueRankIndex(competitionContext, mec2);
    }
    if (tertiaryCompetitionPtr != 0) {
        uint16_t mec3 = _CGetMoreExpensiveTeamsCount(selTeamNumber, tertiaryCompetitionContext);
        d5 += getExpectedLeagueRankIndex(tertiaryCompetitionContext, mec3);
    }
    if (quaternaryCompetitionPtr != 0) {
        uint16_t mec4 = _CGetMoreExpensiveTeamsCount(selTeamNumber, euroCupsCompetitionContext);
        d5 += getExpectedLeagueRankIndex(euroCupsCompetitionContext, mec4);
    }

    teamValueDivisor = d5;

    /* Compute total team value: weeklyPlayerWages * wageScalingDivisor */
    int32_t d6 = 0;
    int32_t wages = weeklyPlayerWages;
    uint16_t scalingCount = wageScalingDivisor;
    for (uint16_t i = 0; i < scalingCount; i++)
        d6 += wages;
    totalTeamValue = d6;

    /* Compute weekly running costs from total team value */
    uint32_t d0 = (uint32_t)d6;
    d0 = (uint16_t)(d0 / 1000);           /* divu #$3E8 */
    d0 = (uint32_t)(uint16_t)d0 * 207;    /* mulu #$CF */
    d0 = (uint16_t)(d0 / 6144);           /* divu #$1800 */
    d0 = (uint32_t)(uint16_t)d0 * 96;     /* mulu #$60 */
    d0 = (uint16_t)(d0 / wageScalingDivisor); /* divu wageScalingDivisor */
    d0 = (uint32_t)(uint16_t)d0 * 1000;   /* mulu #$3E8 */
    weeklyRunningCosts = (int32_t)d0;

    /* Clear accumulators */
    seasonRunningCostsAccum = 0;
    seasonWagesAccum = 0;
    seasonGateReceiptsAccum = 0;
    seasonMatchReceiptsCount = 0;
    seasonWeeksDeducted = 0;
    seasonGateReceiptsAccum2 = 0;
    seasonRunningCostsAccum2 = 0;

    /* Handle staying/leaving club balance adjustments */
    if (stayingInOldClub == 0) {
        int32_t bal = startingBalanceCopy + competitionBonuses;
        old_balance += bal;
        startingBalanceCopy = 0;
        competitionBonuses = 0;
    }

    /* Reset warning states */
    overdraftStreak = 0;
    overdraftWarningState = 0;
    performanceWarningState = 0;
    overdraftWarningCooldown = 0;
    performanceWarningCooldown = 0;
}

/* ================================================================
 * Group F: Board confidence system
 * ================================================================ */

/* CalcBoardConfidenceDeltaPercent (static) */
static int16_t calcBoardConfidenceDeltaPercent(void) {
    if (isNationalSelector != 0) {
        if (seasonFinished == 0)
            return boardConfidenceHistory3;
        return seasonReviewConfidenceDeltaTable[seasonReview];
    }

    /* Club manager: compute from league rank delta */
    uint16_t teamRank = _CFindTeamInDIY(selTeamNumber, competitionFileBuffer);
    int16_t d0 = (int16_t)expectedLeagueRankIndex - (int16_t)teamRank;
    int16_t numTeams = *(int16_t *)(competitionFileBuffer + 0x32);
    d0 = (int16_t)(((int32_t)d0 * 100) / (int32_t)numTeams);
    return d0;
}

/* GetBoardConfidenceScore */
__attribute__((used, externally_visible))
int16_t _GetBoardConfidenceScore(void) {
    int16_t d0 = calcBoardConfidenceDeltaPercent();
    d0 += boardConfidenceHistory0;
    d0 += boardConfidenceHistory1;
    d0 += boardConfidenceHistory2;
    d0 += boardConfidenceScore;
    return d0;
}

/* UpdateBoardConfidenceScore */
__attribute__((used, externally_visible))
int16_t _UpdateBoardConfidenceScore(void) {
    int16_t d0 = boardConfidenceScore;

    if (majorTrophyBonusActive != 0)
        d0 += 100;
    if (cupBonus2Active != 0)
        d0 += 50;
    if (cupBonus1Active != 0)
        d0 += 50;
    if (playWinCupAnimation != 0 && currentDivisionIndex == 0)
        d0 += 50;
    if (minorTrophyBonusActive != 0)
        d0 += 25;
    if (wonWorldCup != 0)
        d0 += 150;
    if (wonContinentalCup != 0)
        d0 += 100;

    if (d0 > 200)
        d0 = 200;

    boardConfidenceScore = d0;
    return (uint16_t)d0;
}

/* PushBoardConfidenceHistory */
__attribute__((used, externally_visible))
int16_t _PushBoardConfidenceHistory(void) {
    boardConfidenceHistory3 = boardConfidenceHistory2;
    boardConfidenceHistory2 = boardConfidenceHistory1;
    boardConfidenceHistory1 = boardConfidenceHistory0;
    int16_t d0 = calcBoardConfidenceDeltaPercent();
    boardConfidenceHistory0 = d0;
    return d0;
}

/* UpdateBoardConfidenceModifiers */
__attribute__((used, externally_visible))
void _UpdateBoardConfidenceModifiers(void) {
    qualifyingStage1PendingFlag0 = 0;
    qualifyingStage1PendingFlag1 = 0;
    qualifyingStage2PendingFlag0 = 0;
    qualifyingStage2PendingFlag1 = 0;

    if (isNationalSelector == 0)
        goto check_winners;

    /* National selector: check World Cup qualifying */
    if ((someTeamFlags & 0x40) == 0)
        goto check_ec_qualifying;

    {
        uint16_t team = selTeamNumber;
        if (team == worldCupQualifyingTeam || team == worldCupHostTeam)
            goto set_qual1_flag0;

        uint32_t curComp = currentCompetitionPtr;
        if (curComp < (uint32_t)worldCupQualifyingGroup1Copy ||
            curComp > (uint32_t)oceaniaWorldCupQualificationCopy)
            goto check_ec_qualifying;

        /* In WC qualifying: find team rank and determine qualifying slots */
        uint16_t teamRank = _CFindTeamInDIY(selTeamNumber, competitionFileBuffer);
        uint32_t comp = currentCompetitionPtr;
        uint16_t qualSlots;

        if (comp == (uint32_t)northAmericaWorldCupQualificationCopy)
            qualSlots = 3;
        else if (comp == (uint32_t)southAmericaWorldCupQualificationCopy)
            qualSlots = 4;
        else if (comp == (uint32_t)africaWorldCupQualificationCopy)
            qualSlots = 5;
        else if (comp == (uint32_t)asiaWorldCupQualificationCopy)
            qualSlots = 2;
        else if (comp == (uint32_t)oceaniaWorldCupQualificationCopy)
            qualSlots = 1;
        else if (comp == (uint32_t)worldCupQualifyingGroup8Copy)
            qualSlots = 1;
        else
            qualSlots = 2;

        if (diyFoundTeamIndex != 0) {
            goto set_qual2_flag0;
        }
        if (diyCompHasGroupStage == 0)
            goto check_ec_qualifying;
        if (teamRank >= qualSlots)
            goto set_qual2_flag0;
    }

set_qual1_flag0:
    qualifyingStage1PendingFlag0 = 1;
    goto check_ec_qualifying;

set_qual2_flag0:
    qualifyingStage2PendingFlag0 = 1;

check_ec_qualifying:
    if ((someTeamFlags & 0x01) == 0)
        goto check_winners;

    {
        uint16_t team = selTeamNumber;
        if (team == euroChampionshipQualTeam)
            goto set_qual1_flag1;

        uint32_t curComp = currentCompetitionPtr;
        if (curComp < (uint32_t)ecQualifyingGroup1Copy ||
            curComp > (uint32_t)ecQualifyingGroup8Copy)
            goto check_winners;

        uint16_t teamRank = _CFindTeamInDIY(selTeamNumber, competitionFileBuffer);
        uint16_t qualSlots;
        if (currentCompetitionPtr == (uint32_t)ecQualifyingGroup8Copy)
            qualSlots = 1;
        else
            qualSlots = 2;

        if (diyFoundTeamIndex != 0)
            goto set_qual2_flag1;
        if (diyCompHasGroupStage == 0)
            goto check_winners;
        if (teamRank >= qualSlots)
            goto set_qual2_flag1;
    }

    qualifyingStage1PendingFlag1 = 1;
    goto check_winners;

set_qual1_flag1:
    qualifyingStage1PendingFlag1 = 1;
    goto check_winners;

set_qual2_flag1:
    qualifyingStage2PendingFlag1 = 1;

check_winners:
    /* Clear trophy flags */
    wonWorldCup = 0;
    wonContinentalCup = 0;
    wonInvitationTournament = 0;
    playWinCupAnimation = 0;

    /* Check if we won the primary competition */
    if (selTeamNumber != primaryCompWinnerTeam)
        goto check_secondary_winner;
    if (currentCompetitionPtr == (uint32_t)friendlyCopy)
        goto check_secondary_winner;
    if (currentCompetitionPtr == (uint32_t)tourCopy)
        goto check_secondary_winner;
    /* Skip if EC qualifying */
    if (currentCompetitionPtr >= (uint32_t)ecQualifyingGroup1Copy &&
        currentCompetitionPtr <= (uint32_t)ecQualifyingGroup8Copy)
        goto check_secondary_winner;
    /* Skip if WC qualifying */
    if (currentCompetitionPtr >= (uint32_t)worldCupQualifyingGroup1Copy &&
        currentCompetitionPtr <= (uint32_t)oceaniaWorldCupQualificationCopy)
        goto check_secondary_winner;

    /* Determine which trophy was won */
    if (currentCompetitionPtr == (uint32_t)invitationTournamentCopy) {
        wonInvitationTournament = 1;
    } else if (currentCompetitionPtr == (uint32_t)worldCup2Copy) {
        wonWorldCup = 1;
    } else if (currentCompetitionPtr == (uint32_t)europeanChampionships2Copy ||
               currentCompetitionPtr == (uint32_t)copaAmericaCopy ||
               currentCompetitionPtr == (uint32_t)africanNationsCupCopy ||
               currentCompetitionPtr == (uint32_t)northAmericanCupCopy ||
               currentCompetitionPtr == (uint32_t)asianCupCopy ||
               currentCompetitionPtr == (uint32_t)oceaniaCupCopy) {
        wonContinentalCup = 1;
    } else {
        playWinCupAnimation = 1;
    }

check_secondary_winner:
    minorTrophyBonusActive = 0;

    if (secondaryCompetitionPtr == 0)
        goto check_tertiary_winner;
    if (selTeamNumber != secondaryCompWinnerTeam)
        goto check_tertiary_winner;
    if (secondaryCompetitionPtr == (uint32_t)friendlyCopy)
        goto check_tertiary_winner;
    if (currentCompetitionPtr == (uint32_t)tourCopy)
        goto check_tertiary_winner;

    if (secondaryCompetitionPtr == (uint32_t)invitationTournamentCopy) {
        wonInvitationTournament = 1;
    } else if (secondaryCompetitionPtr == (uint32_t)europeanChampionships2Copy ||
               secondaryCompetitionPtr == (uint32_t)copaAmericaCopy ||
               secondaryCompetitionPtr == (uint32_t)africanNationsCupCopy ||
               secondaryCompetitionPtr == (uint32_t)northAmericanCupCopy ||
               secondaryCompetitionPtr == (uint32_t)asianCupCopy ||
               secondaryCompetitionPtr == (uint32_t)oceaniaCupCopy) {
        wonContinentalCup = 1;
    } else {
        minorTrophyBonusActive = 1;
    }

check_tertiary_winner:
    leagueTitleBonusActive = 0;

    if (tertiaryCompetitionPtr != 0 &&
        selTeamNumber == tertiaryCompWinnerTeam &&
        tertiaryCompetitionPtr != (uint32_t)friendlyCopy) {
        leagueTitleBonusActive = 1;
    }

    /* Check quaternary (Euro cups) */
    majorTrophyBonusActive = 0;
    cupBonus1Active = 0;
    cupBonus2Active = 0;

    if (quaternaryCompetitionPtr == 0)
        return;
    if (selTeamNumber != quaternaryCompWinnerTeam)
        return;

    if (euroCupSlotIndex == 0) {
        majorTrophyBonusActive = 1;
    } else if (euroCupSlotIndex == 2) {
        cupBonus2Active = 1;
    } else {
        cupBonus1Active = 1;
    }
}

/* ================================================================
 * Group G: Reputation + starting balance
 * ================================================================ */

/* AddReputationFromBonusValueDiv4 (static) */
static void addReputationFromBonusValueDiv4(int8_t bonusValue) {
    int16_t val = (int16_t)bonusValue;
    val >>= 2;  /* lsr.w #2 - but ext.w d0 first, so this is unsigned shift */
    careerReputationPoints += val;
}

/* AddReputationFromBonusValueDiv8 (static) */
static void addReputationFromBonusValueDiv8(int8_t bonusValue) {
    int16_t val = (int16_t)bonusValue;
    val >>= 3;
    careerReputationPoints += val;
}

/* UpdateCareerReputationBonuses */
__attribute__((used, externally_visible))
void _UpdateCareerReputationBonuses(void) {
    /* National selector reputation delta */
    if (isNationalSelector != 0) {
        int16_t delta = nationalSelectorSeasonReviewReputationDeltaTable[seasonReview];
        nationalSelectorReputationPoints += delta;
        if (nationalSelectorReputationPoints < 0)
            nationalSelectorReputationPoints = 0;
        if (nationalSelectorReputationPoints > 49)
            nationalSelectorReputationPoints = 49;
    }

    /* Club reputation from league rank */
    if (*(int16_t *)competitionFileBuffer != 2) {
        /* Not a pure cup — compute reputation from rank delta */
        uint16_t avgPrice = _CGetTeamValueForGateReceipts((uint8_t *)&careerTeam);
        int16_t d7 = (int16_t)avgPrice;

        /* Find team rank in league table */
        uint16_t *leagueTable = (uint16_t *)(competitionFileBuffer + 0x12E);
        int16_t d1 = -2;
        do {
            d1 += 2;
            int16_t entry = *(int16_t *)(competitionFileBuffer + 0x6E + d1);
            int32_t extended = (int32_t)entry;
            uint16_t divided = (uint16_t)((uint32_t)extended / 18);
            uint16_t shifted = divided << 1;
        } while (leagueTable[(uint16_t)((uint32_t)(int32_t)(*(int16_t *)(competitionFileBuffer + 0x6E + d1)) / 18)] != selTeamNumber);
        d1 >>= 1;

        int16_t rankDelta = (int16_t)expectedLeagueRankIndex - d1;
        int16_t numTeams = *(int16_t *)(competitionFileBuffer + 0x32);
        int16_t pct = (int16_t)(((int32_t)rankDelta * 100) / (int32_t)numTeams);
        int16_t repDelta = (int16_t)(((int32_t)pct * (int32_t)d7) / 200);
        careerReputationPoints += repDelta;
    }

    /* Trophy reputation bonuses */
    if (wonWorldCup != 0)
        addReputationFromBonusValueDiv4((int8_t)majorTrophyBonusValue);

    if (wonContinentalCup != 0) {
        /* Check if it's a major continental or minor */
        uint32_t comp = currentCompetitionPtr;
        int8_t bonusVal;
        if (comp == (uint32_t)europeanChampionships2Copy ||
            comp == (uint32_t)copaAmericaCopy ||
            comp == (uint32_t)africanNationsCupCopy ||
            comp == (uint32_t)northAmericanCupCopy ||
            comp == (uint32_t)asianCupCopy ||
            comp == (uint32_t)oceaniaCupCopy) {
            bonusVal = (int8_t)majorTrophyBonusValue;
        } else {
            bonusVal = (int8_t)minorTrophyBonusValue;
        }
        addReputationFromBonusValueDiv4(bonusVal);
    }

    if (playWinCupAnimation != 0) {
        winCupBonusCount++;
        addReputationFromBonusValueDiv4((int8_t)majorTrophyBonusValue);
    }

    if (minorTrophyBonusActive != 0) {
        minorTrophyBonusCount++;
        addReputationFromBonusValueDiv4((int8_t)minorTrophyBonusValue);
    }

    if (leagueTitleBonusActive != 0)
        addReputationFromBonusValueDiv8((int8_t)leagueTitleBonusValue);

    if (majorTrophyBonusActive != 0 || cupBonus1Active != 0 || cupBonus2Active != 0) {
        majorTrophyBonusCount++;
        addReputationFromBonusValueDiv4((int8_t)majorTrophyOrCupBonusValue);
    }
}

/* InitializeStartingBalanceForNewSeason */
__attribute__((used, externally_visible))
void _InitializeStartingBalanceForNewSeason(void) {
    /* Get team value base from avg player price */
    uint16_t avgPrice = _CGetTeamValueForGateReceipts((uint8_t *)&careerTeam);
    uint16_t teamBase = _CGetTeamValueBase(avgPrice);
    uint16_t d2 = teamBase;

    /* Compute adjusted starting balance from expected rank */
    uint16_t teamRank = _CFindTeamInDIY(selTeamNumber, competitionFileBuffer);
    int16_t rankDelta = (int16_t)expectedLeagueRankIndex - (int16_t)teamRank;
    int16_t pct = (int16_t)(((int32_t)rankDelta * 100) / (int32_t)(int16_t)numDiyTeams);
    pct >>= 1;   /* asr.w #1 */
    pct += 100;   /* addi.w #$64 */
    uint16_t adjustedBase = (uint16_t)(((uint32_t)(uint16_t)pct * (uint32_t)d2) / 100);

    /* Add random factor (90-110%) */
    uint16_t r = _Rand2();
    uint16_t randPct = (uint16_t)((r * 20) >> 8) + 90;  /* 90 to ~110 */
    uint16_t finalBase = (uint16_t)(((uint32_t)randPct * (uint32_t)adjustedBase) / 100);
    startingBalance = (int32_t)((uint32_t)finalBase * 1000);

    /* Cup win bonuses by division */
    if (playWinCupAnimation != 0) {
        int16_t bonusVal = (int16_t)(int8_t)majorTrophyBonusValue;
        if (currentDivisionIndex == 0) {
            /* Division 0: add twice (once extra) */
            startingBalance += (int32_t)((uint32_t)(uint16_t)bonusVal * 0xEDC8);
            startingBalance += (int32_t)((uint32_t)(uint16_t)bonusVal * 0xEDC8);
        } else {
            uint32_t multiplier;
            if (currentDivisionIndex == 1)
                multiplier = 0xB720;
            else if (currentDivisionIndex == 2)
                multiplier = 0x7FDD;
            else
                multiplier = 0x61A8;
            startingBalance += (int32_t)((uint32_t)(uint16_t)bonusVal * multiplier);
        }
    }

    /* Minor trophy bonus */
    if (minorTrophyBonusActive != 0) {
        int16_t bonusVal = (int16_t)(int8_t)minorTrophyBonusValue;
        startingBalance += (int32_t)((uint32_t)(uint16_t)bonusVal * 0xCED0);
    }

    /* League title bonus */
    if (leagueTitleBonusActive != 0) {
        int16_t bonusVal = (int16_t)(int8_t)leagueTitleBonusValue;
        startingBalance += (int32_t)((uint32_t)(uint16_t)bonusVal * 0x44F1);
    }

    /* Major trophy / Euro cup bonuses */
    if (majorTrophyBonusActive != 0) {
        int16_t bonusVal = (int16_t)(int8_t)majorTrophyOrCupBonusValue;
        uint32_t amount = (uint32_t)(uint16_t)bonusVal * 0xF426;
        startingBalance += (int32_t)amount;
        startingBalance += (int32_t)amount;  /* added twice in asm */
    }

    if (cupBonus1Active != 0) {
        int16_t bonusVal = (int16_t)(int8_t)majorTrophyOrCupBonusValue;
        uint32_t amount = (uint32_t)(uint16_t)bonusVal * 0x7A14;
        startingBalance += (int32_t)amount;
        startingBalance += (int32_t)amount;
    }

    if (cupBonus2Active != 0) {
        int16_t bonusVal = (int16_t)(int8_t)majorTrophyOrCupBonusValue;
        uint32_t amount = (uint32_t)(uint16_t)bonusVal * 0xB190;
        startingBalance += (int32_t)amount;
        startingBalance += (int32_t)amount;
    }

    /* Promoted bonus (only if no cup win) */
    if (playWinCupAnimation != 0)
        return;
    if (playPromotedAnimation == 0)
        return;

    {
        int16_t bonusVal = (int16_t)(int8_t)majorTrophyBonusValue;
        if (currentDivisionIndex == 0) {
            /* Cannot be promoted to division 0 — panic */
            panicReasonCode = 0x1782;
            _PanicHaltLoop();
            return;
        }
        uint32_t multiplier;
        if (currentDivisionIndex == 1)
            multiplier = 0x6DE0;
        else if (currentDivisionIndex == 2)
            multiplier = 0x4709;
        else
            multiplier = 0x30D4;
        startingBalance += (int32_t)((uint32_t)(uint16_t)bonusVal * multiplier);
    }
}

/* ================================================================
 * recalcTotalWeeklyWages  (SWOS2.s line 86716, loc_14B3AE)
 *
 * Iterates all career team players and sums their cached
 * weekly wages into weeklyPlayerWages.
 * Shared by Career_UpdatePlayerWeeklyWageCache (fall-through)
 * and Career_RemovePlayerWeeklyWageAndRecalcWeeklyWages.
 * ================================================================ */
static void recalcTotalWeeklyWages(void)
{
    int32_t total = 0;
    int16_t i;
    for (i = (int16_t)(careerNumPlayers - 1); i >= 0; i--) {
        _CGetPlayerAtIndex(selTeamNumber, (uint16_t)i);
        uint16_t fileIdx = _cGetPlayerAtIndexFileIdx;
        total += weeklyWageByPlayerIndex[fileIdx];
    }
    weeklyPlayerWages = total;
}

/* ================================================================
 * GetAvgTeamValueBaseForCurrentPlayer  (SWOS2.s line 71480)
 *
 * Gets the average team value base for a player.
 * Calls GetPlayer to get player pointer, then GetDisplayedPlayerValue
 * to get the displayed value (1-80), and indexes into avgTeamValueBases.
 *
 * In asm, d0=teamNumber and d1=playerIndex are set by caller.
 * We take playerIndex as a parameter and read selTeamNumber internally,
 * matching the actual calling convention where d0 is always selTeamNumber.
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _GetAvgTeamValueBaseForCurrentPlayer(uint16_t teamNumber, uint16_t playerIndex)
{
    uint8_t *player = _CallGetPlayer(teamNumber, playerIndex);
    uint16_t displayedValue = _GetDisplayedPlayerValue(player);
    displayedValue &= 0xFF;
    return avgTeamValueBases[displayedValue];
}

/* ================================================================
 * Career_UpdatePlayerWeeklyWageCache  (SWOS2.s line 86678)
 *
 * Calculates and stores the weekly wage for a single player.
 * Steps:
 *   1. Get average team value base for the player
 *   2. Check player status flags for wage reduction
 *      (0x9B = trialist → 33%, 0x9A = youth → 66%)
 *   3. Apply scaling: value * 1000 / 768 * 96 / wageScalingDivisor
 *   4. Store in weeklyWageByPlayerIndex[playerIndex]
 *   5. Recalculate total weekly wages
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_UpdatePlayerWeeklyWageCache(uint16_t playerIndex)
{
    uint16_t avgBase = _GetAvgTeamValueBaseForCurrentPlayer(selTeamNumber, playerIndex);

    /* Get player pointer to check status flags */
    uint8_t *player = _CallGetPlayer(selTeamNumber, playerIndex);
    PlayerFile *pf = (PlayerFile *)(player + 76);
    uint8_t someFlag = (uint8_t)pf->someFlag;

    if (someFlag == 0x9B) {
        /* Trialist: reduce wage to 33% */
        avgBase = (uint16_t)((uint32_t)avgBase * 33 / 100);
    } else if (someFlag == 0x9A) {
        /* Youth: reduce wage to 66% */
        avgBase = (uint16_t)((uint32_t)avgBase * 66 / 100);
    }

    /* Scale: value * 1000 / 768 * 96 / wageScalingDivisor */
    uint32_t wage = (uint32_t)avgBase * 1000;
    wage = wage / 768;
    wage = wage * 96;
    wage = wage / (uint32_t)wageScalingDivisor;
    wage &= 0xFFFF;  /* keep 16-bit quotient (68k divu behavior) */

    weeklyWageByPlayerIndex[playerIndex] = (int32_t)wage;

    recalcTotalWeeklyWages();
}

/* ================================================================
 * Career_RemovePlayerWeeklyWageAndRecalcWeeklyWages  (SWOS2.s line 86711)
 *
 * Clears the weekly wage entry for a player and recalculates
 * the total weekly wages.
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_RemovePlayerWeeklyWageAndRecalcWeeklyWages(uint16_t playerIndex)
{
    weeklyWageByPlayerIndex[playerIndex] = 0;
    recalcTotalWeeklyWages();
}

/* ================================================================
 * ShowXxxMessage functions  (SWOS2.s lines 38068-38130)
 *
 * Each selects a message ID based on isNationalSelector
 * (national vs club chairman messages), sets currentMessage,
 * then calls ShowChairmanMessage.
 * ================================================================ */

/* ShowLeavingMessage — "Thanks for all your help" */
__attribute__((used, externally_visible))
void _ShowLeavingMessage(void)
{
    currentMessage = isNationalSelector ? 32 : 18;
    _CShowChairmanMessage();
}

/* ShowGreetingMessage — "Welcome, we look forward..." */
__attribute__((used, externally_visible))
void _ShowGreetingMessage(void)
{
    currentMessage = isNationalSelector ? 33 : 19;
    _CShowChairmanMessage();
}

/* ShowLastSeasonMessage — "Enjoy your last season" */
__attribute__((used, externally_visible))
void _ShowLastSeasonMessage(void)
{
    currentMessage = isNationalSelector ? 34 : 20;
    _CShowChairmanMessage();
}

/* ShowWishYouSuccessMessage — "Wish you success" */
__attribute__((used, externally_visible))
void _ShowWishYouSuccessMessage(void)
{
    currentMessage = isNationalSelector ? 35 : 21;
    _CShowChairmanMessage();
}

/* ShowEnjoyLastSeasonMessage — "Enjoy last season before..." */
__attribute__((used, externally_visible))
void _ShowEnjoyLastSeasonMessage(void)
{
    currentMessage = isNationalSelector ? 36 : 22;
    _CShowChairmanMessage();
}

/* ShowRetirementMessage — "Enjoy your retirement" */
__attribute__((used, externally_visible))
void _ShowRetirementMessage(void)
{
    currentMessage = isNationalSelector ? 37 : 23;
    _CShowChairmanMessage();
}

/* ================================================================
 * _CalculateStartingBalance  (SWOS2.s line 86092)
 *
 * Calculates career starting balance for a given team.
 * Gets team value base * 3, applies random percentage (80-119%),
 * divides by 100 and multiplies by 1000 to get final balance.
 *
 * Asm: CalculateStartingBalance (d0=teamNumber → d1=balance)
 * ================================================================ */
extern uint8_t *_GetTeamPointer(uint16_t teamId);
extern uint16_t _GetAveragePlayerPrice(uint8_t *team);

__attribute__((used, externally_visible))
uint32_t _CalculateStartingBalance(uint16_t teamNumber)
{
    uint8_t *teamPtr = _GetTeamPointer(teamNumber);
    uint16_t avgPrice = _GetAveragePlayerPrice(teamPtr);
    uint16_t baseValue = _CGetTeamValueBase(avgPrice);
    uint16_t base3 = (uint16_t)(baseValue * 3);     /* mulu.w #3 + move.w truncation */
    uint16_t r = _Rand2();
    uint16_t randFactor = (uint16_t)((uint16_t)(r * 40) >> 8);  /* 0-39 */
    uint16_t pct = (uint16_t)(randFactor + 80);                  /* 80-119 */
    uint32_t product = (uint32_t)pct * (uint32_t)base3;
    uint16_t divided = (uint16_t)(product / 100);                /* divu.w quotient */
    return (uint32_t)divided * 1000;
}

/* ================================================================
 * Chunk 95 — Chairman message helpers, post-match stat updates,
 *            CheckPlayersLeavingComing, AddTrialPlayer
 * ================================================================ */

/* Message ID constants for chairman messages */
#define MSG_PLAYER_JOINED_CLUB_ON_TRIAL   38
#define MSG_TRIAL_PLAYER_LEFT_CLUB        39
#define MSG_RESERVE_PLAYER_LEFT_CLUB      40

extern uint8_t *plNameForMessage;
extern uint8_t *_GetPlayer(uint16_t teamNumber, uint16_t playerFileIndex);
extern uint8_t *_GetPlayerAtIndex(uint16_t teamNumber, uint16_t ordinal);
extern void _Career_UpdatePlayerWeeklyWageCache(uint16_t playerIndex);
extern void _RemovePlayerFromTeam(uint16_t playerOrd);

/* asm bridges for functions not yet ported */
extern void _CCareer_ConvertPlayerToTrialistFromName(uint16_t teamNum, uint16_t playerIdx);
extern void _CTrainingMatch_SaveSelectedPlayerId(uint16_t value);

extern uint8_t competitionFileBuffer[];


/* ================================================================
 * _PlayerJoinedClubOnTrial — show chairman message for player joining on trial.
 * Asm: PlayerJoinedClubOnTrial (a1=name ptr)
 * ================================================================ */
__attribute__((used, externally_visible))
void _PlayerJoinedClubOnTrial(uint8_t *name)
{
    plNameForMessage = name;
    currentMessage = MSG_PLAYER_JOINED_CLUB_ON_TRIAL;
    _CShowChairmanMessage();
}

/* ================================================================
 * _TrialPlayerLeftClubMessage — show chairman message for trial player leaving.
 * Asm: TrialPlayerLeftClubMessage (a1=name ptr)
 * ================================================================ */
__attribute__((used, externally_visible))
void _TrialPlayerLeftClubMessage(uint8_t *name)
{
    plNameForMessage = name;
    currentMessage = MSG_TRIAL_PLAYER_LEFT_CLUB;
    _CShowChairmanMessage();
}

/* ================================================================
 * _ReservePlayerLeftClubMessage — show chairman message for reserve player leaving.
 * Asm: ReservePlayerLeftClubMessage (a1=name ptr)
 * ================================================================ */
__attribute__((used, externally_visible))
void _ReservePlayerLeftClubMessage(uint8_t *name)
{
    plNameForMessage = name;
    currentMessage = MSG_RESERVE_PLAYER_LEFT_CLUB;
    _CShowChairmanMessage();
}

/* ================================================================
 * _Career_UpdateSinglePlayerPostMatchStats — update one player's post-match form.
 * Asm: Career_UpdateSinglePlayerPostMatchStats (a6=player raw ptr within careerTeam)
 *
 * The player pointer is within careerTeam; index = (ptr - careerTeam) / 38.
 * Handles form transitions: $9B→$9A, $9A→restored form (20% chance),
 * in-form players get +2 to form clamped at 100.
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_UpdateSinglePlayerPostMatchStats(uint8_t *playerSlotPtr)
{
    if (isNationalSelector)
        return;

    /* Compute player index from raw offset */
    uint32_t offset = (uint32_t)(playerSlotPtr - (uint8_t *)&careerTeam);
    uint16_t playerIdx = (uint16_t)(offset / 38);

    PlayerFile *pf = (PlayerFile *)(playerSlotPtr + 76);
    uint8_t positionAndFace = pf->positionAndFace;
    uint8_t form = (uint8_t)pf->someFlag;

    if (!(positionAndFace & 0x01)) {
        /* Player is NOT in form (bit 0 clear) */
        if (form == 0x9B) {
            /* Trialist hired → demote to "on trial" ($9A) */
            pf->someFlag = (int8_t)0x9A;
            _Career_UpdatePlayerWeeklyWageCache(playerIdx);
            return;
        }

        if (form != 0x9A)
            return;  /* Not trial — nothing to do */

        /* Form = $9A (on trial) — ~20% chance to promote */
        if (_Rand() >= 0x33)
            return;

        /* Restore form from saved modifier table and set in-form bit */
        pf->someFlag = (int8_t)playerFormModifiers[playerIdx];
        pf->positionAndFace = positionAndFace | 0x01;  /* bset #0 */
        _Career_UpdatePlayerWeeklyWageCache(playerIdx);
        return;
    }

    /* Player is in form (bit 0 set) */
    form = (uint8_t)(form + 2);
    pf->someFlag = (int8_t)form;

    if ((int8_t)form < 0) {
        /* Overflowed past 127 → clear in-form bit */
        pf->positionAndFace = positionAndFace & (uint8_t)~0x01;
    }

    /* Clamp form at 100 ($64) */
    if ((int8_t)pf->someFlag > 0x64) {
        pf->someFlag = 0x64;
    }
}

/* ================================================================
 * _Career_ApplyPostMatchPlayerStatModsIfCareerTeam — apply post-match
 * stat mods to career team players.
 * Asm: Career_ApplyPostMatchPlayerStatModsIfCareerTeam (a0=team, a1=mods)
 *
 * Only processes if NOT national selector and team IS careerTeam.
 * Iterates 11 players (d7=10..0), uses currentMatchPlayers[d7] as
 * player file index, checks remap[fileIndex] — if negative, 20% chance
 * to use remap value as replacement modifier.
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_ApplyPostMatchPlayerStatModsIfCareerTeam(uint8_t *team, uint8_t *mods)
{
    if (isNationalSelector)
        return;
    if (team != (uint8_t *)&careerTeam)
        return;

    int16_t d7;
    for (d7 = 0x0A; d7 >= 0; d7--) {
        int8_t d6 = (int8_t)currentMatchPlayers[d7];
        uint8_t modByte = mods[(uint8_t)d6];

        if (!((int8_t)modByte < 0)) {
            /* modByte is non-negative — check 12.5% chance */
            if (_Rand() >= 0x20)
                goto do_update;
            d6 = (int8_t)modByte;
        }

    do_update:
        {
            uint16_t d2 = (uint16_t)(uint8_t)d6;
            d2 = (uint16_t)(d2 * 38);  /* mulu.w #$26 */
            uint8_t *playerSlotPtr = team + d2;
            _Career_UpdateSinglePlayerPostMatchStats(playerSlotPtr);
        }
    }
}

/* ================================================================
 * _AddTrialPlayer — generate a random trial player and add to roster.
 * Asm: AddTrialPlayer (no params, result: a3=new player ptr)
 *
 * Picks a random position from trialPlayerPositionTable, then calls
 * TrainingMatch_AddTrialPlayerAtPosition.
 * Returns pointer to the new player (for accessing name at offset 79).
 * ================================================================ */
static const uint8_t trialPlayerPositionTable[] = {3,3,6,6,7,7,0,1,2,4,5,0};

__attribute__((used, externally_visible))
uint8_t *_AddTrialPlayer(void)
{
    uint16_t r = _Rand();
    uint16_t posIdx = (uint16_t)((uint16_t)(r * 11) >> 8);
    uint16_t position = (uint16_t)trialPlayerPositionTable[posIdx];
    return _TrainingMatch_AddTrialPlayerAtPosition(position);
}

/* ================================================================
 * _TrainingMatch_AddTrialPlayerAtPosition — add trial player at given position.
 * Asm: TrainingMatch_AddTrialPlayerAtPosition (d0=position)
 *
 * Finds unused player slot in team, sets up as trialist, inserts into
 * currentMatchPlayers array at correct position, assigns random form modifier.
 * Returns pointer to the new player record.
 * ================================================================ */
__attribute__((used, externally_visible))
uint8_t *_TrainingMatch_AddTrialPlayerAtPosition(uint16_t position)
{
    /* Find an unused player index (not in currentMatchPlayers[0..careerNumPlayers-1]) */
    uint16_t freeSlot = 0;
    while (1) {
        uint16_t np = careerNumPlayers;
        uint16_t found = 0;
        uint16_t i;
        for (i = 0; i < np; i++) {
            if (currentMatchPlayers[i] == (uint8_t)freeSlot) {
                freeSlot++;
                found = 1;
                break;
            }
        }
        if (!found)
            break;
    }

    /* Get the player record at that slot */
    uint8_t *player = _GetPlayer(selTeamNumber, freeSlot);
    PlayerFile *pf = (PlayerFile *)(player + 76);
    _CTrainingMatch_SaveSelectedPlayerId(freeSlot);

    /* Set shirtNumber as a flag (used as a marker here) */
    pf->shirtNumber = 0x10;

    /* Store position into positionAndFace (position << 5) */
    pf->positionAndFace = (uint8_t)(position << 5);

    /* Convert player to trialist */
    _CCareer_ConvertPlayerToTrialistFromName(selTeamNumber, freeSlot);

    /* Determine insertion point in currentMatchPlayers */
    uint16_t insertOffset;
    {
        uint8_t *compBuf = competitionFileBuffer;
        uint16_t compType = *(uint16_t *)compBuf;
        if (compType == 2) {
            insertOffset = *(uint16_t *)(compBuf + 0x4E);
        } else {
            insertOffset = *(uint16_t *)(compBuf + 0x66);
        }
        insertOffset += 0x0B;
    }

    /* Shift entries up to make room at insertOffset */
    {
        uint8_t *arr = currentMatchPlayers;
        uint16_t endIdx = careerNumPlayers;
        uint8_t *endPtr = &arr[endIdx];
        uint16_t count = endIdx - insertOffset;
        if (count > 0) {
            uint16_t i;
            for (i = 0; i < count; i++) {
                endPtr[-1 - (int16_t)i] = endPtr[-2 - (int16_t)i];
            }
        }
        arr[insertOffset] = (uint8_t)freeSlot;
    }

    /* Assign random negative form modifier */
    {
        uint16_t r = _Rand();
        uint16_t formVal = (uint16_t)((uint16_t)(r * 50) >> 8);  /* 0-49 */
        formVal += 40;   /* 40-89 */
        formVal = (uint16_t)(-(int16_t)formVal);  /* -40 to -89 */
        playerFormModifiers[freeSlot] = (uint8_t)formVal;
    }

    careerNumPlayers += 1;
    _Career_UpdatePlayerWeeklyWageCache(freeSlot);

    return player;
}

/* ================================================================
 * _CheckPlayersLeavingComing — check for random player arrivals/departures.
 * Asm: CheckPlayersLeavingComing (no params)
 *
 * If not national selector and not sacked:
 * - If roster < 30 and 25/256 chance → add trial player, show message
 * - Otherwise scan roster backwards for trialists ($9B form) and
 *   reserves ($9A form) with small random chance to leave
 * ================================================================ */
__attribute__((used, externally_visible))
void _CheckPlayersLeavingComing(void)
{
    if (isNationalSelector)
        return;
    if (youreSacked)
        return;

    /* Try to add a trial player if roster not full */
    if (careerNumPlayers < 30) {
        if (_Rand() < 25) {
            uint8_t *newPlayer = _AddTrialPlayer();
            PlayerFile *npf = (PlayerFile *)(newPlayer + 76);
            _PlayerJoinedClubOnTrial((uint8_t *)npf->playerName);
            return;
        }
    }

    /* Scan roster backwards for players that might leave */
    {
        int16_t d2;
        for (d2 = (int16_t)(careerNumPlayers - 1); d2 >= 0; d2--) {
            uint8_t *player = _GetPlayerAtIndex(selTeamNumber, (uint16_t)d2);
            PlayerFile *pf = (PlayerFile *)(player + 76);
            uint8_t form = (uint8_t)pf->someFlag;

            if (form == 0x9B) {
                /* Trial player ($9B) — 5/256 chance to leave */
                if (_Rand() >= 5)
                    continue;
                _TrialPlayerLeftClubMessage((uint8_t *)pf->playerName);
                goto remove;
            }

            if (form == 0x9A) {
                /* Reserve/loan player ($9A) — 2/256 chance to leave */
                if (_Rand() >= 2)
                    continue;
                _ReservePlayerLeftClubMessage((uint8_t *)pf->playerName);
                goto remove;
            }

            continue;

        remove:
            _RemovePlayerFromTeam((uint16_t)d2);
            return;  /* Only one removal per call */
        }
    }
}
