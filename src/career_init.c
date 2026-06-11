/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* career_init.c — Career mode transfer market initialization.
 *
 * Chunk 61: 3 functions ported from SWOS2.s.
 */

#include "game_types.h"
#include <string.h>   /* memcpy in _Rebuild_CupRecordNameSourcePointers */
#include "endian_helpers.h"
#include "menu_system.h"
#include "input_text.h"
#include "save_format_json.h"
#include "random.h"
#include "string_utils.h"
#include "panic.h"
#include "auto_skip_debug.h"

/* ---- External globals (all in SWOS2.s BSS) ---- */
extern uint16_t numPlayersOnTransferMarket;
extern uint16_t numForeignMarketPlayers2;
extern uint16_t transferMarketRandOffset0;
extern uint16_t transferMarketRandOffset1;
extern uint16_t transferMarketRandOffset2;
extern uint16_t transferMarketPhase;
extern uint16_t timeToNegotiate;
extern uint16_t numOffers;
extern uint16_t transferPoolSlotsUsed;
extern uint16_t offerSwapSlotsUsed;
extern uint16_t lastOfferPrimaryPlayerOrdinal;
extern uint16_t offerNegotiationAttemptCount;
extern uint16_t careerSeasonsCompleted;
extern uint16_t currentSeasonShowing;
extern uint16_t seasonTransferQuota;
extern uint16_t pendingForeignPlayerCount;
extern uint16_t offerSwapSlotLimit;
extern uint16_t transferPoolSlotLimit;
extern uint16_t homeMarketUpdateBacklog;
extern uint16_t foreignMarketUpdateBacklog;
extern uint16_t offerExpiryIterationsRemaining;
extern uint16_t removedOfferPlayersFromSelectedTeamCount;
extern uint16_t removedOfferPlayersFromOtherTeamsCount;

/* careerCupPickList: 5-entry array of COMPETITION POINTERS (national-coach
 * cup pick list). Pointer-sized entries — was extern'd uint32_t (truncating
 * the 64-bit pointers); now a standalone void *[5] (see swos_data_typed_stubs.c). */
extern void *careerCupPickList[];
extern uint8_t invitationTournamentCopy[];
extern uint8_t tourCopy[];

/* Globals for manager setup menu (Chunk 110) */
extern MenuEntry menuEntries[];
extern uint16_t coachOrPlayer;
extern uint16_t mrOrMs;
extern uint8_t managerSexBuffer[];
extern uint8_t aMr[];           /* "MR" */
extern uint8_t aMs[];           /* "MS" */

/* Globals for Career_InitializeSelectedTeamAndStartingBalance (Chunk 170) */
extern int32_t  startingBalance;
extern uint16_t teamNoOfferAccepted;
extern uint16_t stayingInOldClub;
extern uint16_t careerTeamLeague;
extern uint16_t selTeamNumber;
extern TeamFileEx careerTeam;
extern uint8_t  selTeamsPtr;     /* start of selected teams buffer (boundary spill) */
extern uint8_t  currentMatchPlayers[];
extern uint16_t teamsLoaded;
extern int32_t  newBalance;
extern uint16_t careerNumPlayers;
extern uint16_t sensisoftPlcSponsorshipFactor;
extern uint16_t additionalInvestments;
extern uint16_t isNationalSelector;

/* ================================================================
 * Career_InitTransferMarketState  (SWOS2.s line 72836)
 *
 * Clears transfer market counters and generates three random
 * offsets (0..15) used during transfer market initialization.
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_InitTransferMarketState(void)
{
    numPlayersOnTransferMarket = 0;
    numForeignMarketPlayers2 = 0;
    transferMarketRandOffset1 = _Rand2() & 0xF;
    transferMarketRandOffset2 = _Rand2() & 0xF;
    transferMarketPhase = 0;
    transferMarketRandOffset0 = _Rand2() & 0xF;
}

/* ================================================================
 * Career_InitSeasonTransferMarketState  (SWOS2.s line 72853)
 *
 * Initializes per-season transfer market state: negotiation time,
 * offer counts, pool quotas, and backlog counters.
 *
 * Season transfer quota formula:
 *   numerator = 432 - numForeignMarketPlayers2 - (7 - seasons) * 16
 *   divisor   = 20 - currentSeasonShowing
 *   quota     = (uint32_t)(int32_t)(int16_t)numerator / divisor
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_InitSeasonTransferMarketState(void)
{
    timeToNegotiate = 6;
    numOffers = 0;
    transferPoolSlotsUsed = 0;
    offerSwapSlotsUsed = 0;
    lastOfferPrimaryPlayerOrdinal = 0xFFFF;
    offerNegotiationAttemptCount = 0;

    /* Compute season transfer quota */
    uint16_t seasonsAdj = (uint16_t)(7 - careerSeasonsCompleted) * 16;  /* mulu #$10 */
    uint16_t divisor = 20 - currentSeasonShowing;
    int16_t numerator = (int16_t)(0x1B0 - numForeignMarketPlayers2 - seasonsAdj);
    /* ext.l + divu.w: sign-extend to 32-bit then unsigned divide by word */
    uint16_t quota = (uint16_t)((uint32_t)(int32_t)numerator / divisor);

    seasonTransferQuota = quota;
    pendingForeignPlayerCount = 0;

    uint16_t half = quota >> 1;   /* lsr.w #1 */
    offerSwapSlotLimit = half;
    transferPoolSlotLimit = quota - half;   /* neg.w of (half - quota) */

    homeMarketUpdateBacklog = 0;
    foreignMarketUpdateBacklog = 0;
    offerExpiryIterationsRemaining = 0;
    removedOfferPlayersFromSelectedTeamCount = 0;
    removedOfferPlayersFromOtherTeamsCount = 0;
}

/* ================================================================
 * Career_RandomizeInvitationTournamentCup  (SWOS2.s line 35893)
 *
 * Scans the 5-entry careerCupPickList for entries pointing to
 * invitationTournamentCopy. For each such entry, 50% chance to
 * replace it with tourCopy (randomized via Rand).
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_RandomizeInvitationTournamentCup(void)
{
    void **pickList = careerCupPickList;
    int16_t i;

    for (i = 4; i >= 0; i--) {
        if (pickList[i] == (void *)invitationTournamentCopy) {
            if (_Rand() & 1)
                pickList[i] = (void *)tourCopy;
        }
    }
}

/* ================================================================
 * Manager setup menu callbacks (Chunk 110)
 * Asm: SetPlayerCoachFieldColor, CheckSex, ChangeSex,
 *      SetName, SetSurname, CoachOrPlayerCoach
 * ================================================================ */

/* SetPlayerCoachFieldColor: update background color palette index
 * for menu entry 9 based on coachOrPlayer toggle.
 * The low nibble of the bg color word holds the color palette index. */
__attribute__((used, externally_visible))
void _SetPlayerCoachFieldColor(void)
{
    MenuEntry *e = &menuEntries[9];
    e->bgColor = (e->bgColor & 0xFFF0) | 13; /* BRIGHT_BLUE_TO_BLUE_13 */
    if (coachOrPlayer != 0)
        e->bgColor = (e->bgColor & 0xFFF0) | 11; /* BLUE_TO_PURPLE_11 */
}

/* CheckSex: copy "MR" or "MS" to managerSexBuffer based on mrOrMs toggle */
__attribute__((used, externally_visible))
void _CheckSex(void)
{
    uint8_t *src = aMr;
    if (mrOrMs != 0)
        src = aMs;
    _StringCopy(src, managerSexBuffer);
}

/* ChangeSex: toggle mrOrMs, update sex string, redraw current menu item.
 * entry = a5 from menu system (current MenuEntry). */
__attribute__((used, externally_visible))
void _ChangeSex(MenuEntry *entry)
{
    mrOrMs ^= 1;
    _CheckSex();
    _CallDrawMenuItem(entry);
}

/* SetName: start text input for first name field (max 9 chars).
 * entry = a5 from menu system; InputText reads entry->fg itself. */
__attribute__((used, externally_visible))
void _SetName(MenuEntry *entry)
{
    _InputText(9, (uint8_t *)entry);
}

/* SetSurname: start text input for surname field (max 13 chars).
 * entry = a5 from menu system; InputText reads entry->fg itself. */
__attribute__((used, externally_visible))
void _SetSurname(MenuEntry *entry)
{
    _InputText(13, (uint8_t *)entry);
}

/* CoachOrPlayerCoach: toggle coach/player-coach mode, update color, redraw.
 * entry = a5 from menu system (current MenuEntry). */
__attribute__((used, externally_visible))
void _CoachOrPlayerCoach(MenuEntry *entry)
{
    coachOrPlayer ^= 1;
    _SetPlayerCoachFieldColor();
    _CallDrawMenuItem(entry);
}

/* ================================================================
 * Chunk 148: ChooseTeamAndStartCareer, LoadLiverpool
 * ================================================================ */

extern uint8_t contestMenu[];
extern void _ActivateMenu(uint8_t *menu);
extern void _SetExitMenuFlag(void);

/* Forward declarations for Chunk 197 functions (defined later in this file) */
void _InitCareer(uint16_t teamNo);
void _ContinueCareerNextSeason(void);
void _InitializeNewSeason(void);
void _DIY_SelectCompetitionSlot4FromCurrentDivision(void);
uint16_t _Career_SelectEuroCupSlotIfTeamParticipates(uint8_t *cupCopy, uint16_t euroCupIndex);

extern uint16_t selectedTeamCountryNumber;
extern TeamFileEx *selectedTeamPtr;
void _SelectTeamToManage(void);

/* ChooseTeamAndStartCareer: let user pick a team, then start career.
 * Called as menu onSelect callback. */
__attribute__((used, externally_visible))
void _ChooseTeamAndStartCareer(void)
{
    _SelectTeamToManage();
    if (selectedTeamPtr == 0) {
        _SetExitMenuFlag();
        return;
    }
    _InitCareer(selectedTeamCountryNumber);
    _ActivateMenu(contestMenu);
}

/* LoadLiverpool: debug/cheat — start career with Liverpool (team 0x811).
 * Called as menu onSelect callback. */
__attribute__((used, externally_visible))
void _LoadLiverpool(void)
{
    coachOrPlayer = 1;
    _InitCareer(0x811);
    _ActivateMenu(contestMenu);
}

/* ================================================================
 * Chunk 149: ContinueCareer  (SWOS2.s line 55140)
 *
 * If season == 19, show retirement message + reset career variables.
 * Otherwise advance to next season and re-activate contest menu.
 * ================================================================ */

extern void _ShowRetirementMessage(void);
extern void _InitCareerVariables(uint16_t gameTypeParam);

__attribute__((used, externally_visible))
void _ContinueCareer(void)
{
    if (currentSeasonShowing == 19) {
        _ShowRetirementMessage();
        _InitCareerVariables(0);
        _SetExitMenuFlag();
    } else {
        _ContinueCareerNextSeason();
        _ActivateMenu(contestMenu);
    }
}

/* ================================================================
 * Chunk 151: InitCompetitionTeamSelection functions
 *
 * These initialize team selection state for career competition setup.
 * They all follow a similar pattern:
 *   1. Clear careerFileBuffer
 *   2. Call LoadSomeEuroCup
 *   3. Clear numSelectedTeams
 *   4. Call Career_InitCompetitionTeamSelectionFrom*Spec
 *   5. Read numNeededTeams from the competition record
 *   6. Return 0
 * ================================================================ */
extern uint16_t numSelectedTeams;
extern uint16_t careerFileBuffer;
extern uint16_t numNeededTeams;

extern void _LoadSomeEuroCup(uint8_t *whichCup);
extern void _Career_InitCompetitionTeamSelectionFromTableEntrySpec(uint8_t *base, uint16_t idx);
extern void _Career_InitCompetitionTeamSelectionFromTableHeaderSpec(uint8_t *base);
extern void _Career_InitCompetitionTeamSelectionFromAltTableHeaderSpec(uint8_t *base);

__attribute__((used, externally_visible))
uint16_t _InitCompetitionTeamSelectionFromTableEntry(uint16_t idx, uint8_t *base)
{
    careerFileBuffer = 0;
    _LoadSomeEuroCup(base);
    numSelectedTeams = 0;
    _Career_InitCompetitionTeamSelectionFromTableEntrySpec(base, idx);

    /* numNeededTeams = record byte at offset 0xD in idx'th 6-byte entry */
    uint16_t offset = idx * 6;
    numNeededTeams = (int8_t)base[0xD + offset];
    return 0;
}

__attribute__((used, externally_visible))
uint16_t _InitCompetitionTeamSelectionFromTableHeader(uint8_t *base)
{
    careerFileBuffer = 0;
    _LoadSomeEuroCup(base);
    numSelectedTeams = 0;
    _Career_InitCompetitionTeamSelectionFromTableHeaderSpec(base);

    numNeededTeams = (int8_t)base[0xA];
    return 0;
}

__attribute__((used, externally_visible))
uint16_t _InitCompetitionTeamSelectionFromAltTableHeader(uint8_t *base)
{
    careerFileBuffer = 0;
    _LoadSomeEuroCup(base);
    numSelectedTeams = 0;
    _Career_InitCompetitionTeamSelectionFromAltTableHeaderSpec(base);

    numNeededTeams = (int8_t)base[0xF];
    return 0;
}

/* ================================================================
 * Career_InitializeSelectedTeamAndStartingBalance (SWOS2.s line 34621)
 *
 * Sets up the career team data by copying the selected team's file
 * data to both careerTeam and selTeamsPtr buffers, sets the
 * coach/player control byte, copies player order to currentMatchPlayers,
 * and initializes financial globals.
 *
 * Asm: d0=teamNumber (uint16), d1=startingBalance (int32)
 * ================================================================ */
extern int16_t _GetTeamLeague(uint16_t teamNumber);
extern TeamFileEx *_GetTeamPointer(uint16_t teamId);

__attribute__((used, externally_visible))
void _Career_InitializeSelectedTeamAndStartingBalance(uint16_t teamNumber, int32_t balance)
{
    startingBalance = balance;
    teamNoOfferAccepted = 0xFFFF;
    stayingInOldClub = 0xFFFF;
    numSelectedTeams = 0;

    /* Get team league for career tracking */
    careerTeamLeague = (uint16_t)_GetTeamLeague(teamNumber);

    /* Get pointer to team file data and copy to both buffers */
    TeamFileEx *teamPtr = _GetTeamPointer(teamNumber);
    selTeamNumber = teamNumber;

    /* Copy 342 words (684 bytes = one TeamFileEx) to both careerTeam and selTeamsPtr */
    uint16_t *src = (uint16_t *)teamPtr;
    uint16_t *dst1 = (uint16_t *)&careerTeam;
    uint16_t *dst2 = (uint16_t *)&selTeamsPtr;
    for (int16_t i = 341; i >= 0; i--) {
        *dst1++ = *src;
        *dst2++ = *src++;
    }

    numSelectedTeams = 1;

    /* Set teamNameControl byte (offset 4) based on coach-or-player setting:
     * 2 = coach only, 3 = player-coach */
    uint8_t controlByte = (coachOrPlayer != 0) ? 2 : 3;
    careerTeam.team.teamControls = controlByte;
    ((TeamFileEx *)&selTeamsPtr)->team.teamControls = controlByte;

    /* Copy 16 bytes of playerNumbers (TeamFile.playerNumbers) to currentMatchPlayers */
    teamPtr = _GetTeamPointer(selTeamNumber);
    for (int16_t i = 15; i >= 0; i--) {
        currentMatchPlayers[i] = teamPtr->team.playerNumbers[i];
    }

    careerNumPlayers = 16;
    teamsLoaded = 0;
    newBalance = 0;
    sensisoftPlcSponsorshipFactor = 0;
    additionalInvestments = 0;
}

/* ================================================================
 * Career_ContinueToNextSeasonInitTeamAndFunds (SWOS2.s line 72514)
 *
 * Prepares for a new career season: clears transfer market, increments
 * season counter, optionally rebuilds foreign market pool, applies a
 * random ±jitter (80-104%) to the base funds, then initializes the
 * team with the adjusted balance.
 *
 * Asm: d0=teamNumber (uint16), d1=baseFunds (uint32)
 * ================================================================ */
extern void _Career_BuildForeignMarketPoolFromSelectedTeam(void);
extern uint16_t _Rand2(void);

__attribute__((used, externally_visible))
void _Career_ContinueToNextSeasonInitTeamAndFunds(uint16_t teamNumber, uint32_t baseFunds)
{
    numPlayersOnTransferMarket = 0;
    careerSeasonsCompleted++;

    if (isNationalSelector == 0) {
        _Career_BuildForeignMarketPoolFromSelectedTeam();
    }

    /* Apply random jitter to funds: 80-104% of original
     * Formula: ((baseFunds / 1000) * (80 + rand25pct)) / 100 * 1000
     * where rand25pct = (Rand2() * 25) >> 8 = 0..24 */
    uint16_t fundsDiv = (uint16_t)(baseFunds / 1000);
    uint16_t rand = _Rand2();
    uint16_t jitterPct = ((rand * 25) >> 8) + 80;
    uint32_t adjusted = (uint32_t)fundsDiv * (uint32_t)jitterPct;
    uint16_t adjustedDiv = (uint16_t)(adjusted / 100);
    uint32_t finalFunds = (uint32_t)adjustedDiv * 1000;

    _Career_InitializeSelectedTeamAndStartingBalance(teamNumber, (int32_t)finalFunds);
}

/* ================================================================
 * Chunk 197: InitCareer, ContinueCareerNextSeason,
 *   Career_SelectEuroCupSlotIfTeamParticipates,
 *   DIY_SelectCompetitionSlot4FromCurrentDivision,
 *   InitializeNewSeason, InitNewSeason
 * ================================================================ */

/* --- External globals (Chunk 197) --- */
extern uint16_t worldCupHostTeam;
extern uint8_t  diyLeagueSeasonStateTable[];
extern uint16_t sameNationalCallFlag;
extern uint16_t boardConfidenceHistory0;
extern uint16_t boardConfidenceHistory1;
extern uint16_t boardConfidenceHistory2;
extern uint16_t boardConfidenceHistory3;
extern uint16_t boardConfidenceScore;
extern uint16_t gamesPlayed;
extern uint16_t numWins;
extern uint16_t numDrawn;
extern uint16_t numLost;
extern uint16_t goalsScored;
extern uint16_t goalsTaken;
extern uint16_t careerReputationPoints;
extern uint16_t winCupBonusCount;
extern uint16_t minorTrophyBonusCount;
extern uint16_t majorTrophyBonusCount;
extern uint16_t playPromotedAnimation;
extern uint16_t playRelegatedAnimation;
extern uint16_t previousBoardConfidence;
extern uint16_t playWinCupAnimation;
extern uint16_t minorTrophyBonusActive;
extern uint16_t leagueTitleBonusActive;
extern uint16_t majorTrophyBonusActive;
extern uint16_t cupBonus1Active;
extern uint16_t cupBonus2Active;
extern uint16_t wonWorldCup;
extern uint16_t wonContinentalCup;
extern uint16_t qualifyingStage1PendingFlag0;
extern uint16_t qualifyingStage1PendingFlag1;
extern uint16_t qualifyingStage2PendingFlag0;
extern uint16_t qualifyingStage2PendingFlag1;
extern uint16_t wonInvitationTournament;

extern uint8_t  euroCup[];
extern uint8_t  cupWinnersCup[];
extern uint8_t  uefaCup[];

/* careerContests table — typed CompetitionCareer[] entries
 * {src_ptr, dest_ptr, byte_count} terminated by `competition ==
 * (CompetitionCup *)0xFFFFFFFFu`. Was `extern uint8_t careerContests[]`
 * with byte-stride walks (10 B per entry); on 64-bit each entry is
 * 18 B so consumers iterate the typed array instead. */
extern CompetitionCareer careerContests[];

extern uint16_t primaryCompSlotStateA;
extern uint16_t primaryCompSlotStateB;
extern uint16_t primaryCompSlotStateC;
extern uint16_t secondaryCompSlotStateA;
extern uint16_t secondaryCompSlotStateB;
extern uint16_t secondaryCompSlotStateC;
extern uint16_t tertiaryCompSlotStateA;
extern uint16_t tertiaryCompSlotStateB;
extern uint16_t tertiaryCompSlotStateC;
extern uint16_t quaternaryCompSlotStateA;
extern uint16_t quaternaryCompSlotStateB;
extern uint16_t quaternaryCompSlotStateC;
extern uint8_t  seasonPlaying;

extern uint16_t poolplyrLoaded;

extern void _DIY_LoadLeagueScratchFromTable(void);
extern uint32_t _CalculateStartingBalance(uint16_t teamNumber);

/* --- InitializeNewSeason externs --- */
extern uint16_t selTeamSelectedPlayerPosition;
extern uint8_t  careerUnavailablePlayerList[];
extern uint16_t retired;
extern uint16_t youreSacked;
extern uint16_t competitionSlotUpdatedFlag;
extern uint16_t boardConfidenceMod0;
extern uint16_t boardConfidenceMod1;
extern uint16_t boardConfidenceMod2;
extern uint16_t boardConfidenceMod3;
extern uint16_t boardConfidenceValue0;
extern uint16_t boardConfidenceValue1;
extern uint16_t boardConfidenceMod4;
extern uint16_t boardConfidenceMod5;
extern uint16_t boardConfidenceMod6;
extern uint16_t boardConfidenceMod7;
extern uint16_t boardConfidenceMod8;
extern uint16_t boardConfidenceMod9;
extern uint16_t currentDivisionIndex;
extern uint16_t contractNegotiationActive;
extern uint16_t removedPlayerStatAccum;
extern uint16_t teamSelectionKeyCacheEnd;
extern uint16_t isCareer;
extern uint16_t careerMatchSimulatedFlag;
extern uint16_t numNewJobOffers;
extern uint16_t numJobOffers;
extern uint16_t careerTeamLostMatch;
extern volatile uint16_t currentMessage;
extern uint8_t  careerLastMatchIndexByCompetitionSlot[];
extern uint8_t *currentCompetitionPtr;
extern uint8_t *secondaryCompetitionPtr;
extern uint8_t *tertiaryCompetitionPtr;
extern uint8_t *quaternaryCompetitionPtr;
extern uint8_t *diyCompetitionPtr;
extern uint8_t competitionFileBuffer[];
extern uint32_t competitionContext;
extern uint32_t tertiaryCompetitionContext;
extern uint32_t euroCupsCompetitionContext;
extern uint32_t diyCompetitionContext;
extern uint8_t *careerCompetitionSlotPtrTable[];
extern uint16_t teamSelectionKeyCacheCount;
extern uint16_t teamSelectionKeyCacheIndex;
extern uint16_t teamSelectionKeyCacheOffset;
extern uint16_t careerSlotProcessingInProgress;
extern uint16_t lastNationalityCall;
extern void    *careerTeamFlagsByRegionTable[];  /* 6-entry pointer table; native width on 32 and 64-bit. Storage is `void *[6]` in swos_amiga_ptr_imports.c. */
extern uint8_t  someTeamFlags;
extern uint8_t  nationalTeamNationality;
extern uint8_t  nationalTeamFileId;
extern uint16_t worldCupQualifyingTeam;
extern uint16_t euroChampionshipQualTeam;
extern uint16_t seasonCycleNationalFlagsA[];
extern uint16_t seasonCycleNationalFlagsB[];
extern uint8_t *primaryCompStringOffset;
extern uint8_t *secondaryCompStringOffset;
extern uint8_t *tertiaryCompStringOffset;
extern uint8_t *quaternaryCompStringOffset;
extern uint8_t  euroCupCopy[];
extern uint8_t  cupWinnersCupCopy[];
extern uint8_t  uefaCupCopy[];
extern void *competitionsTable[];
/* careerCupPickList: declared at top of file (line 39) */
extern uint16_t euroCupSlotIndex;
extern uint32_t *diyCompetitionTablePtr;
extern uint8_t  careerJobOfferPhaseBias;
extern uint8_t  viewRandomPhase;
extern uint8_t  lastFinancialWeekIndex;
extern uint16_t wageScalingDivisor;
extern uint16_t leagueFirstRoundNumber;
extern uint16_t seasonFinished;
extern uint16_t careerFinished;
extern uint8_t  managerNameBuffer[];
extern uint8_t  managerSurnameBuffer[];
extern uint16_t seasonNumberOfSquads;
extern uint16_t seasonSquadTeamNumbers[];
extern uint16_t careerLoadedFlag;
extern uint8_t  teamSelectionKeyCache[];
extern uint32_t teamFundsAccepted;
extern uint8_t  nationalTeamFileCode;

extern uint8_t *_GetCurrentSeasonPointer(void);
extern void _DIY_SaveLeagueScratchToTable(void);
extern TeamFileEx *_VerifyTeamLoaded(uint16_t teamNo);
extern void _InitTeams(void);
extern void _FillForeignMarketPlayers(void);
extern void _FillCareerTeamPlayers(void);
extern void _Career_InitializeNewSeasonFinances(void);
extern void _InitializeCareerSeasonFinancials(void);
extern void _ShowEnjoyLastSeasonMessage(void);
extern void _ShowWishYouSuccessMessage(void);
extern void _ShowLastSeasonMessage(void);
extern void _ShowGreetingMessage(void);
extern void _ShowLeavingMessage(void);
extern void _TrainingMatch_AddTrialPlayersForNewSeason(void);
extern void _Career_InitInternationalSquadAndLookupTablesForNewSeason(TeamFileEx *teamPtr);
extern void _Career_SetCupPickList_ForSelectedRegion(uint16_t *teamList);
extern void _Career_UpdateLastNationalityCall(void);
extern void _InitializeNewSeason_BuildTeamBudgetRankTable(void);
extern void _Career_SetCurrentCompetitionSlot(uint8_t *comp, uint16_t slot, uint16_t divisionIndex, uint16_t hasSelectedTeams);
extern void _ShowChairmanOpinionAboutTheSeason(void);
extern void _InitializeStartingBalanceForNewSeason(void);
extern void _Career_AdvanceToNextSeason_UpdateCompetitions(void);
extern uint32_t _GetNationalityAndTeamFile(uint16_t teamNo);

/* ================================================================
 * InitCareer  (SWOS2.s line 31722)
 *
 * Initializes a new career: clears season state, calls transfer market
 * init, calculates starting balance, copies competition data, sets
 * slot states, then calls InitializeNewSeason.
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitCareer(uint16_t teamNo)
{
    worldCupHostTeam = 20994;

    /* Clear 2000 bytes of diyLeagueSeasonStateTable (volatile to prevent memset optimization) */
    {
        volatile uint8_t *p = diyLeagueSeasonStateTable;
        for (int16_t i = 1999; i >= 0; i--)
            *p++ = 0;
    }

    _DIY_LoadLeagueScratchFromTable();

    sameNationalCallFlag = 0;
    careerSeasonsCompleted = 0;
    boardConfidenceHistory0 = 0;
    boardConfidenceHistory1 = 0;
    boardConfidenceHistory2 = 0;
    boardConfidenceHistory3 = 0;
    boardConfidenceScore = 0;
    gamesPlayed = 0;
    numWins = 0;
    numDrawn = 0;
    numLost = 0;
    goalsScored = 0;
    goalsTaken = 0;
    careerReputationPoints = 0;
    winCupBonusCount = 0;
    minorTrophyBonusCount = 0;
    majorTrophyBonusCount = 0;
    playPromotedAnimation = 0;
    playRelegatedAnimation = 0;
    teamsLoaded = 0;
    poolplyrLoaded = 0;
    careerFileBuffer = 0;
    previousBoardConfidence = 0xFFFF;
    playWinCupAnimation = 0;
    minorTrophyBonusActive = 0;
    leagueTitleBonusActive = 0;
    majorTrophyBonusActive = 0;
    cupBonus1Active = 0;
    cupBonus2Active = 0;
    wonWorldCup = 0;
    wonContinentalCup = 0;
    qualifyingStage1PendingFlag0 = 0;
    qualifyingStage1PendingFlag1 = 0;
    qualifyingStage2PendingFlag0 = 0;
    qualifyingStage2PendingFlag1 = 0;
    wonInvitationTournament = 0;

    _Career_InitTransferMarketState();
    _InitCareerVariables(4);

    uint32_t balance = _CalculateStartingBalance(teamNo);
    _Career_InitializeSelectedTeamAndStartingBalance(teamNo, (int32_t)balance);

    careerFileBuffer = 0;

    _LoadSomeEuroCup(euroCup);
    _LoadSomeEuroCup(cupWinnersCup);
    _LoadSomeEuroCup(uefaCup);

    /* Copy careerContests table entries: {src, dest, count} until
     * src == 0xFFFFFFFF. Iterate as the typed CompetitionCareer[]
     * — on 64-bit each entry is 18 bytes (two 8-byte pointers + uint16);
     * the old byte-stride walk assumed 10-byte entries and would slip
     * 8 bytes between each iteration. Volatile cursors prevent LTO
     * from eliminating the ROM→BSS copies (same pattern as the tactic
     * copy bug). */
    extern CompetitionCareer careerContests[];
    for (CompetitionCareer *e = careerContests;
         (uintptr_t)e->competition != 0xFFFFFFFFu; e++) {
        volatile uint8_t *src = (volatile uint8_t *)e->competition;
        volatile uint8_t *dst = (volatile uint8_t *)e->competitionCopy;
        uint16_t count = e->length;
        for (int16_t i = count - 1; i >= 0; i--)
            *dst++ = *src++;
    }

    teamsLoaded = 0;
    primaryCompSlotStateA = 3;
    primaryCompSlotStateB = 2;
    primaryCompSlotStateC = 8;
    secondaryCompSlotStateA = 3;
    secondaryCompSlotStateB = 2;
    secondaryCompSlotStateC = 8;
    tertiaryCompSlotStateA = 3;
    tertiaryCompSlotStateB = 2;
    tertiaryCompSlotStateC = 8;
    quaternaryCompSlotStateA = 3;
    quaternaryCompSlotStateB = 2;
    quaternaryCompSlotStateC = 8;
    currentSeasonShowing = 0xFFFF;
    seasonPlaying--;
    _InitializeNewSeason();
    careerLoadedFlag = 0;
}

/* ================================================================
 * ContinueCareerNextSeason  (SWOS2.s line 32355)
 *
 * If the manager has accepted a new job offer, show leaving message,
 * update competitions, init team with new funds, then init new season.
 * Otherwise, show chairman opinion, update competitions, recalculate
 * starting balance, then init new season.
 * ================================================================ */
__attribute__((used, externally_visible))
void _ContinueCareerNextSeason(void)
{
    extern uint16_t gameType;
    SWOS_AUTOSKIP_LOG("[bug1] ContinueCareerNextSeason ENTER: youreSacked=%u "
                      "teamNoOfferAccepted=0x%04X gameType=%u",
                      (unsigned)youreSacked,
                      (unsigned)teamNoOfferAccepted,
                      (unsigned)gameType);
    if (teamNoOfferAccepted != 0xFFFF) {
        if (youreSacked == 0) {
            _ShowLeavingMessage();
        }
        SWOS_AUTOSKIP_LOG("[bug1] ContinueCareerNextSeason: offer-accepted branch");
        _Career_AdvanceToNextSeason_UpdateCompetitions();
        _Career_ContinueToNextSeasonInitTeamAndFunds(teamNoOfferAccepted, teamFundsAccepted);
        _InitializeNewSeason();
    } else {
        SWOS_AUTOSKIP_LOG("[bug1] ContinueCareerNextSeason: chairman branch (no offer accepted)");
        _ShowChairmanOpinionAboutTheSeason();
        SWOS_AUTOSKIP_LOG("[bug1] back from ShowChairmanOpinion: youreSacked=%u", (unsigned)youreSacked);
        _Career_AdvanceToNextSeason_UpdateCompetitions();
        SWOS_AUTOSKIP_LOG("[bug1] back from AdvanceToNextSeason: youreSacked=%u", (unsigned)youreSacked);
        _InitializeStartingBalanceForNewSeason();
        SWOS_AUTOSKIP_LOG("[bug1] back from InitStartingBalance: youreSacked=%u", (unsigned)youreSacked);
        _InitializeNewSeason();
        SWOS_AUTOSKIP_LOG("[bug1] back from InitializeNewSeason: youreSacked=%u", (unsigned)youreSacked);
    }
    SWOS_AUTOSKIP_LOG("[bug1] ContinueCareerNextSeason EXIT");
}

/* ================================================================
 * Career_TeamInEuroCup
 *
 * Returns 1 if selTeamNumber is in cupCopy's static team list (the player's
 * club plays this euro cup), else 0. Shared by the season-init enrolment
 * (_Career_SelectEuroCupSlotIfTeamParticipates) and the load-time slot-3
 * re-derivation (_RebuildPointersAfterLoad) so both agree on participation —
 * see the load-fixup site for why a divergence panics 0x6006.
 * ================================================================ */
static int _Career_TeamInEuroCup(uint8_t *cupCopy)
{
    /* team list = cupCopy + 7 + cupCopy[7], with the 64-bit text[2] widening
     * compensation (the static competition's unk1[] is packed BE (country,
     * ordinal) byte pairs; read via BE_READ_U16 to match selTeamNumber, which
     * is PACK_BE_U16(country, ordinal) everywhere). */
    uint8_t numEntries = cupCopy[7];
    uint16_t numEntriesAdj = (uint16_t)numEntries +
                             2u * (sizeof(char *) - sizeof(uint32_t));
    const uint8_t *teamList = (const uint8_t *)(cupCopy + 7 + numEntriesAdj);

    /* number of teams depends on competition type (byte at offset 1) */
    uint8_t compType = cupCopy[1];
    int16_t numTeams;
    if (compType == 1)      numTeams = (int8_t)cupCopy[0xA];
    else if (compType == 2) numTeams = (int8_t)cupCopy[0xF];
    else                    numTeams = (int8_t)cupCopy[0xD];

    uint16_t target = selTeamNumber;
    for (int16_t i = (int16_t)(numTeams - 1); i >= 0; i--) {
        uint16_t entry = BE_READ_U16(teamList);
        teamList += 2;
        if (entry == target)
            return 1;   /* participating */
    }
    return 0;           /* not in this cup */
}

/* ================================================================
 * Career_SelectEuroCupSlotIfTeamParticipates  (SWOS2.s line 32087)
 *
 * Check if selTeamNumber is in the euro cup's team list.
 * If not found, return 1 (non-zero, "not participating").
 * If found, set up slot 3 (quaternary) for this euro cup and return 0.
 *
 * in:  a0 = euroCupCopy pointer, d7 = euroCupIndex (0/1/2)
 * out: d0 = 0 if participating, 1 if not
 * ================================================================ */
__attribute__((used, externally_visible))
uint16_t _Career_SelectEuroCupSlotIfTeamParticipates(uint8_t *cupCopy, uint16_t euroCupIndex)
{
    /* Participation test (shared with the load-time slot-3 re-derivation in
     * _RebuildPointersAfterLoad). */
    if (!_Career_TeamInEuroCup(cupCopy))
        return 1;  /* not participating */
    {
#ifdef SWOS_DEBUG_AUTOSKIP
        SWOS_AUTOSKIP_LOG("[eurocup] team 0x%04X qualifies euroCupIndex=%u (cupCopy=%p) "
                          "-> activating slot 3",
                          (unsigned)selTeamNumber, (unsigned)euroCupIndex, (void *)cupCopy);
#endif
        /* Save quaternary slot state */
        uint16_t savedA = quaternaryCompSlotStateA;
        uint16_t savedB = quaternaryCompSlotStateB;
        uint16_t savedC = quaternaryCompSlotStateC;

        /* Set slot 3 for this euro cup (d0=3, d1=euroCupIndex, d2=0) */
        _Career_SetCurrentCompetitionSlot(cupCopy, 3, 0, 0);

        /* quaternaryCompStringOffset == euroCupsCompetitionContext+0x28
         * namePtr on Amiga (slot 3). */
        ((SeasonInformations *)_GetCurrentSeasonPointer())->strPtr3 =
            ((DIYCompetition *)careerCompetitionSlotPtrTable[3])->namePtr;

        /* Restore quaternary slot state */
        quaternaryCompSlotStateC = savedC;
        quaternaryCompSlotStateB = savedB;
        quaternaryCompSlotStateA = savedA;

        euroCupSlotIndex = euroCupIndex;
        return 0;  /* participating */
    }
}

/* ================================================================
 * DIY_SelectCompetitionSlot4FromCurrentDivision  (SWOS2.s line 30972)
 *
 * If the primary competition is a league (type 0), check if it has
 * a linked DIY competition and set up slot 4 for it.
 * ================================================================ */
__attribute__((used, externally_visible))
void _DIY_SelectCompetitionSlot4FromCurrentDivision(void)
{
    uint8_t *comp = (uint8_t *)currentCompetitionPtr;
    if (((Competition *)comp)->type != 0)
        return;

    uint16_t offset = currentDivisionIndex * 6;
    uint8_t *entry = comp + offset;
    uint8_t diyOffset = entry[0x12];
    if (diyOffset == 0)
        return;

    /* Follow the DIY link */
    uint8_t *diyEntry = entry + 0x12 + diyOffset;
    uint8_t *diyComp = *(uint8_t **)diyEntry;
    diyCompetitionTablePtr = *(uint32_t **)(diyEntry + 4);

    _Career_SetCurrentCompetitionSlot(diyComp, 4, 0, 0);
}

/* ================================================================
 * InitializeNewSeason  (SWOS2.s line 31818)
 *
 * Large function that initializes a new career season: increments
 * season counter, clears board confidence, sets up competition
 * slots (primary/secondary/tertiary/quaternary/diy), determines
 * if national selector, assigns euro cups, builds team budget rank
 * table, copies manager name, fills market players, inits finances,
 * and shows chairman message.
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitializeNewSeason(void)
{
    SWOS_AUTOSKIP_LOG("[bug1] InitializeNewSeason ENTER: youreSacked=%u "
                      "teamNoOfferAccepted=0x%04X selTeamNumber=0x%04X",
                      (unsigned)youreSacked,
                      (unsigned)teamNoOfferAccepted,
                      (unsigned)selTeamNumber);
    teamsLoaded = 0;
    currentSeasonShowing++;
    if (currentSeasonShowing == 20)
        currentSeasonShowing = 0;

    selTeamSelectedPlayerPosition = 0xFFFF;

    /* Clear 4 entries of careerUnavailablePlayerList */
    /* careerUnavailablePlayerList holds 4 × { uint16 teamNumber,
     * uint16 playerIndex } entries at byte stride 4. Mark the first 4
     * teamNumber slots as empty (0xFFFF). */
    {
        uint16_t *upl = (uint16_t *)careerUnavailablePlayerList;
        upl[0] = 0xFFFF;
        upl[2] = 0xFFFF;
        upl[4] = 0xFFFF;
        upl[6] = 0xFFFF;
    }

    retired = 0;
    youreSacked = 0;
    competitionSlotUpdatedFlag = 0;
    stayingInOldClub++;
    seasonPlaying++;
    if (seasonPlaying == 100)
        seasonPlaying = 0;

    _DIY_SaveLeagueScratchToTable();

    boardConfidenceMod0 = 0;
    boardConfidenceMod1 = 0;
    boardConfidenceMod2 = 0;
    boardConfidenceMod3 = 0;
    boardConfidenceValue0 = 0;
    boardConfidenceValue1 = 0;
    boardConfidenceMod4 = 0;
    boardConfidenceMod5 = 0;
    boardConfidenceMod6 = 0;
    boardConfidenceMod7 = 0;
    boardConfidenceMod8 = 0;
    boardConfidenceMod9 = 0;

    /* Get current season info pointer and fill it */
    SeasonInformations *si = (SeasonInformations *)_GetCurrentSeasonPointer();
    si->clubNo            = selTeamNumber;
    si->leagueStrPtr      = 0;
    si->strPtr1           = 0;
    si->strPtr2           = 0;
    si->strPtr3           = 0;
    si->comp1Result       = 0;
    si->comp2Result       = 0;
    si->comp3Result       = 0;
    si->comp4Result       = 0;
    si->bestScorerName[0] = 0;
    si->managerStatus     = 0;

    /* Copy team name to season info CLUBNAME */
    _StringCopy((uint8_t *)careerTeam.team.teamName, (uint8_t *)si->clubName);

    /* Verify team loaded and get current division index */
    TeamFileEx *teamFile = _VerifyTeamLoaded(selTeamNumber);
    uint16_t globalTeamNo = teamFile->team.globalTeamNumber;
    int8_t diyLeagueState = (int8_t)diyLeagueSeasonStateTable[globalTeamNo];
    int16_t divIdx = (int16_t)diyLeagueState + (int16_t)careerTeamLeague;
    currentDivisionIndex = (uint16_t)divIdx;
    teamFile->team.league = (uint8_t)divIdx;

    contractNegotiationActive = 0;
    removedPlayerStatAccum = 0;
    _Career_InitSeasonTransferMarketState();

    teamSelectionKeyCacheEnd = 0;
    isCareer = 0;
    careerMatchSimulatedFlag = 0;
    numNewJobOffers = 0;
    numJobOffers = 0;
    careerTeamLostMatch = 0;
    currentMessage = 0;

    /* Fill careerLastMatchIndexByCompetitionSlot with 0xFF (162 bytes, volatile to avoid memset) */
    {
        volatile uint8_t *dst = careerLastMatchIndexByCompetitionSlot;
        for (int16_t i = 161; i >= 0; i--)
            *dst++ = 0xFF;
    }

    /* Initialize competition slot pointer table */
    careerCompetitionSlotPtrTable[0] = (uint8_t *)&competitionFileBuffer;
    careerCompetitionSlotPtrTable[1] = (uint8_t *)&competitionContext;
    careerCompetitionSlotPtrTable[2] = (uint8_t *)&tertiaryCompetitionContext;
    careerCompetitionSlotPtrTable[3] = (uint8_t *)&euroCupsCompetitionContext;
    careerCompetitionSlotPtrTable[4] = (uint8_t *)&diyCompetitionContext;

    /* Clear all competition pointers */
    currentCompetitionPtr = 0;
    secondaryCompetitionPtr = 0;
    tertiaryCompetitionPtr = 0;
    quaternaryCompetitionPtr = 0;
    diyCompetitionPtr = 0;

    numSelectedTeams = 1;
    teamSelectionKeyCacheCount = 0;
    teamSelectionKeyCacheIndex = 0;
    teamSelectionKeyCacheOffset = 0;
    careerSlotProcessingInProgress = 0;
    isNationalSelector = 0;

    /* Determine if this is a national team (country code 80..85 = CN_EUROPE..CN_OCEANIA) */
    uint16_t countryCode = selTeamNumber >> 8;
    /* Walker over a country's competition pointer-array. Entries are
     * full-width pointers — `Competition *` — interleaved with -2/-1
     * sentinels. Iterating as `void **` makes the stride scale with
     * pointer width (4 B on Amiga/32-bit, 8 B on 64-bit) and lets the
     * sentinel comparison use the full pointer width via `(uintptr_t)-1`. */
    void **compTablePtr;

    if (countryCode >= 80 && countryCode <= 85) {
        /* National team selector */
        isNationalSelector = 1;

        /* Check same nationality call */
        if (selTeamNumber == lastNationalityCall) {
            sameNationalCallFlag = 1;
        }

        /* Look up team flags from region table. Walk as `void **` so
         * each slot is native pointer width (was `uint32_t *` which
         * read half a pointer on 64-bit and warned at the
         * (uint8_t *)<u32> cast). */
        void **regionTable = careerTeamFlagsByRegionTable;
        uint16_t regionIdx = (uint8_t)(selTeamNumber >> 8);  /* move.b — high byte = country */
        int16_t regionOffset = (int16_t)(int8_t)regionIdx - 80;
        uint8_t *flagsArray = (uint8_t *)regionTable[regionOffset];
        someTeamFlags = flagsArray[currentSeasonShowing];

        /* Get nationality and team file */
        uint32_t natResult = _GetNationalityAndTeamFile(selTeamNumber);
        nationalTeamNationality = (uint8_t)(natResult >> 16);  /* d0 */
        nationalTeamFileId = (uint8_t)(natResult & 0xFF);       /* d1 */

        /* Look up season cycle flags */
        uint16_t cycleIdx = (currentSeasonShowing >> 2) << 1;
        worldCupQualifyingTeam = seasonCycleNationalFlagsA[cycleIdx >> 1];
        euroChampionshipQualTeam = seasonCycleNationalFlagsB[cycleIdx >> 1];

        /* Set cup pick list for selected region (param unused, function writes to global) */
        _Career_SetCupPickList_ForSelectedRegion(0);

        compTablePtr = (void **)&careerCupPickList;
    } else {
        /* Club team */
        nationalTeamFileCode = (uint8_t)(selTeamNumber >> 8);  /* move.b — high byte */

        uint16_t tableIdx = selTeamNumber >> 8;
        compTablePtr = (void **)competitionsTable[tableIdx];
    }

    _Career_UpdateLastNationalityCall();

    /* Set up primary competition (slot 0) */
    {
        uint16_t savedA = primaryCompSlotStateA;
        uint16_t savedB = primaryCompSlotStateB;
        uint16_t savedC = primaryCompSlotStateC;

        uint8_t *comp = (uint8_t *)*compTablePtr;
        compTablePtr++;
        _Career_SetCurrentCompetitionSlot(comp, 0, currentDivisionIndex, 0);

        /* SWOS2.s reads (primaryCompStringOffset), which on Amiga IS
         * competitionFileBuffer+0x28 = the comp namePtr that
         * _Career_SetCurrentCompetitionSlot just wrote. Our standalone
         * primaryCompStringOffset global is never populated (only zeroed),
         * so read the slot-0 buffer's namePtr directly. */
        ((SeasonInformations *)_GetCurrentSeasonPointer())->leagueStrPtr =
            ((DIYCompetition *)careerCompetitionSlotPtrTable[0])->namePtr;

        primaryCompSlotStateC = savedC;
        primaryCompSlotStateB = savedB;
        primaryCompSlotStateA = savedA;
    }

    /* Skip the per-primary divisionIndex slot (one pointer-sized entry,
     * usually -2 = "no extra division pointer"). */
    compTablePtr++;

    /* Set up secondary competition (slot 1) if present */
    if ((uintptr_t)*compTablePtr != (uintptr_t)-1) {
        uint16_t savedA = secondaryCompSlotStateA;
        uint16_t savedB = secondaryCompSlotStateB;
        uint16_t savedC = secondaryCompSlotStateC;

        uint8_t *comp = (uint8_t *)*compTablePtr;
        compTablePtr++;
        _Career_SetCurrentCompetitionSlot(comp, 1, 0, 0);

        /* secondaryCompStringOffset == competitionContext+0x28 namePtr on
         * Amiga (slot 1); read the buffer namePtr directly. */
        ((SeasonInformations *)_GetCurrentSeasonPointer())->strPtr1 =
            ((DIYCompetition *)careerCompetitionSlotPtrTable[1])->namePtr;

        secondaryCompSlotStateC = savedC;
        secondaryCompSlotStateB = savedB;
        secondaryCompSlotStateA = savedA;

        /* Set up tertiary competition (slot 2) if present */
        if ((uintptr_t)*compTablePtr != (uintptr_t)-1) {
            uint16_t savedA2 = tertiaryCompSlotStateA;
            uint16_t savedB2 = tertiaryCompSlotStateB;
            uint16_t savedC2 = tertiaryCompSlotStateC;

            comp = (uint8_t *)*compTablePtr;
            _Career_SetCurrentCompetitionSlot(comp, 2, 0, 0);

            /* tertiaryCompStringOffset == tertiaryCompetitionContext+0x28
             * namePtr on Amiga (slot 2). */
            ((SeasonInformations *)_GetCurrentSeasonPointer())->strPtr2 =
                ((DIYCompetition *)careerCompetitionSlotPtrTable[2])->namePtr;

            tertiaryCompSlotStateC = savedC2;
            tertiaryCompSlotStateB = savedB2;
            tertiaryCompSlotStateA = savedA2;
        }
    }

    /* Try euro cups for slot 3 (quaternary) */
    if (_Career_SelectEuroCupSlotIfTeamParticipates(euroCupCopy, 0) != 0) {
        if (_Career_SelectEuroCupSlotIfTeamParticipates(cupWinnersCupCopy, 1) != 0) {
            _Career_SelectEuroCupSlotIfTeamParticipates(uefaCupCopy, 2);
        }
    }

    /* Set up DIY competition for slot 4 */
    _DIY_SelectCompetitionSlot4FromCurrentDivision();

    _InitializeNewSeason_BuildTeamBudgetRankTable();

    /* Determine leagueFirstRoundNumber */
    {
        uint16_t firstRound = 1;
        uint8_t *primaryComp = (uint8_t *)currentCompetitionPtr;
        if (primaryComp[1] == 0) {
            firstRound = (uint16_t)primaryComp[9];
        }
        leagueFirstRoundNumber = firstRound;
    }

    seasonFinished = 0;
    careerFinished = 0;

    viewRandomPhase = careerJobOfferPhaseBias;

    /* Walk teamSelectionKeyCache to compute wageScalingDivisor */
    {
        uint8_t *cachePtr = teamSelectionKeyCache;
        uint8_t lastKey = 0;
        int16_t count = teamSelectionKeyCacheCount - 1;
        for (int16_t i = count; i >= 0; i--) {
            lastKey = ((TeamSelectionKeyCacheEntry *)cachePtr)->sortKey;
            uint16_t stride = ((TeamSelectionKeyCacheEntry *)cachePtr)->entryStride;
            cachePtr += stride;
        }
        uint16_t d0 = (uint16_t)(lastKey - careerJobOfferPhaseBias) & 0xFF;
        if (d0 == 0)
            d0 = 1;
        wageScalingDivisor = d0;
    }

    lastFinancialWeekIndex = careerJobOfferPhaseBias;

    _InitTeams();

    /* Copy manager name into the team's coach-name slot */
    teamFile = _VerifyTeamLoaded(selTeamNumber);
    {
        uint8_t *namePos = (uint8_t *)teamFile->team.coachName;
        _StringCopy(managerNameBuffer, namePos);
        /* Find end of copied name and add space + surname */
        while (*namePos != 0)
            namePos++;
        *namePos++ = 0x20;  /* space */
        _StringCopy(managerSurnameBuffer, namePos);
    }

    _FillForeignMarketPlayers();
    _FillCareerTeamPlayers();
    _Career_InitializeNewSeasonFinances();
    _InitializeCareerSeasonFinancials();

    /* Show appropriate chairman message */
    if (stayingInOldClub != 0) {
        if (currentSeasonShowing == 0x13) {
            _ShowEnjoyLastSeasonMessage();
        } else {
            _ShowWishYouSuccessMessage();
        }
    } else {
        if (currentSeasonShowing == 0x13) {
            _ShowLastSeasonMessage();
        } else {
            _ShowGreetingMessage();
        }
    }

    /* Post-message: trial players and international squad setup */
    if (stayingInOldClub == 0) {
        _TrainingMatch_AddTrialPlayersForNewSeason();
        if (isNationalSelector != 0) {
            _Career_InitInternationalSquadAndLookupTablesForNewSeason((TeamFileEx *)0);
        }
    }

}

/* ================================================================
 * InitNewSeason  (SWOS2.s line 32148)
 *
 * Simplified new season init used when continuing with a new team
 * (after job change). Sets up competition slots from the competition
 * table, counts non-computer squads, inits teams.
 *
 * in:  a0 = source comp record, d1 = divisionIndex
 * ================================================================ */
__attribute__((used, externally_visible))
void _InitNewSeason(uint8_t *src, uint16_t divisionIndex)
{
    retired = 0;
    uint8_t compTableIdx = (uint8_t)((Competition *)src)->teamNumber;
    currentDivisionIndex = divisionIndex;

    /* Initialize competition slot pointer table */
    careerCompetitionSlotPtrTable[0] = (uint8_t *)&competitionFileBuffer;
    careerCompetitionSlotPtrTable[1] = (uint8_t *)&competitionContext;
    careerCompetitionSlotPtrTable[2] = (uint8_t *)&tertiaryCompetitionContext;
    careerCompetitionSlotPtrTable[3] = (uint8_t *)&euroCupsCompetitionContext;
    careerCompetitionSlotPtrTable[4] = (uint8_t *)&diyCompetitionContext;

    currentCompetitionPtr = 0;
    secondaryCompetitionPtr = 0;
    tertiaryCompetitionPtr = 0;
    quaternaryCompetitionPtr = 0;
    diyCompetitionPtr = 0;

    teamSelectionKeyCacheCount = 0;
    teamSelectionKeyCacheIndex = 0;
    teamSelectionKeyCacheOffset = 0;
    careerSlotProcessingInProgress = 0;

    /* Look up competitions table by index — entries are full-width
     * pointers (8 B on 64-bit, 4 B on Amiga). Iterate as `void **` so
     * stride/sentinel scale with pointer width — see equivalent walker
     * in `_InitializeNewSeason`. */
    void **compTable = (void **)competitionsTable[compTableIdx & 0xFF];

    /* Slot 0: primary competition */
    uint8_t *comp = (uint8_t *)*compTable;
    compTable++;
    _Career_SetCurrentCompetitionSlot(comp, 0, currentDivisionIndex, 1);

    /* Skip the per-primary divisionIndex slot (one pointer entry, -2 = "none"). */
    compTable++;

    /* Slot 1: secondary competition (if present) */
    if ((uintptr_t)*compTable != (uintptr_t)-1) {
        comp = (uint8_t *)*compTable;
        compTable++;
        _Career_SetCurrentCompetitionSlot(comp, 1, 0, 0);

        /* Slot 2: tertiary competition (if present) */
        if ((uintptr_t)*compTable != (uintptr_t)-1) {
            comp = (uint8_t *)*compTable;
            _Career_SetCurrentCompetitionSlot(comp, 2, 0, 0);
        }
    }

    seasonFinished = 0;
    careerFinished = 0;

    /* Count non-computer-controlled squads */
    seasonNumberOfSquads = 0;
    uint16_t *squadTeams = seasonSquadTeamNumbers;
    TeamFileEx *teamPtr = (TeamFileEx *)&selTeamsPtr;
    int16_t numTeams = numSelectedTeams - 1;
    for (int16_t i = numTeams; i >= 0; i--) {
        if (teamPtr->team.teamControls != 1) {
            seasonNumberOfSquads++;
            *squadTeams++ = PACK_BE_U16(teamPtr->team.countryNumber,
                                        teamPtr->team.teamOrdinal);
        }
        teamPtr++;
    }

    if (seasonNumberOfSquads == 0)
        careerFinished = 1;

    viewRandomPhase = careerJobOfferPhaseBias;
    _InitTeams();
}

/* ================================================================
 * _RebuildPointersAfterLoad  (no asm equivalent — new code)
 *
 * Several fields in the careerFileBuffer overlay region (see SAVE_DATA.md)
 * are typed as absolute pointers — valid when the save was written but
 * meaningless in any subsequent binary build (or any other machine). The
 * JSON save format never stores these as raw addresses; it serializes a
 * stable IDENTITY instead (competition ID, buffer index, string-pool key).
 * After `SaveJson_LoadCareer` restores the typed fields, this function
 * re-derives every pointer field from its identity (current run's buffer
 * address, competition ID, etc.).
 *
 * This is now the ONLY pointer fixup. The legacy Amiga-absolute-address
 * translation LUT (`_TranslateAmigaAddressesAfterLoad` /
 * `save_pointer_lut.c` / `generated/amiga_address_map.c`) was deleted once
 * the raw-dump readers were retired — JSON identities make it unnecessary.
 *
 * Fields handled (see SAVE_DATA.md "Pointer-field summary for .CAR"):
 *
 *   1. careerCompetitionSlotPtrTable[5]  — five competition slot bases.
 *      Re-derived from `&competitionFileBuffer / &competitionContext /
 *      &tertiaryCompetitionContext / &euroCupsCompetitionContext /
 *      &diyCompetitionContext` in the current build's BSS (now
 *      aliased into careerFileBufferBuf by the overlay).
 *
 *   2. diyCompetitionStart — runtime DIY working buffer (separate 4 KB
 *      BSS, NOT inside the careerFileBuffer overlay). Repopulated
 *      from the loaded slot-0 (primary competition) state.
 *
 *   3. Embedded `nameSource` pointers inside each cup *Copy record at
 *      offset (5 + nameOffset). Copied from the matching static cup
 *      template via the careerContests table.
 *
 *   4. Top-level competition pointers
 *      `currentCompetitionPtr / secondaryCompetitionPtr /
 *      tertiaryCompetitionPtr / diyCompetitionPtr /
 *      diyCompetitionTablePtr`. Re-derived from competitionsTable +
 *      saved selTeamNumber (or careerCupPickList for national
 *      selectors), mirroring `_InitializeNewSeason` lines 994-1067
 *      but without re-initialising slot state.
 *
 * Not yet handled (will be layered in as we hit menus that read
 * them):
 *
 *   *CompStringOffset      — would re-derive from string-table
 *                              lookup by competition.
 *   quaternaryCompetitionPtr (slot-3 euro cup) — needs a saved-state
 *                              probe to disambiguate which cup. NULLed
 *                              for now; the slot is reachable via
 *                              careerCompetitionSlotPtrTable[3].
 *   tmdFileBuffer/careerTeam runtime pointers
 *                          — should be zeroed; rebuilt at match start.
 *
 * On the Amiga C port (post back-port) this function is identical —
 * neither port pins data at fixed addresses anymore, so both need the
 * same rebuild.
 * ================================================================ */
extern void _Career_LoadCompetitionSlotStateFromBuffer(uint8_t *src);
/* declared at top of file (line 615) — uint16_t *teamList */

/* Step 3 helper: walk careerContests and re-derive each *Copy
 * record's embedded text[] pointer pair.
 *
 * Each cup record carries a signed `nameOffset` byte at offset 5. At
 * `record + 5 + nameOffset` lives an inline `text[2]` array — two
 * 4-byte pointers to static name strings (e.g. text[0] = "EUROPEAN
 * CHAMPIONS", text[1] = "EUROPEAN CUP"). The remaining bytes after
 * those 8 are the `unk1[]` lookup table (scalars, not pointers).
 *
 * The static template carries the canonical pointer values for the
 * current build; the saved *Copy carries stale values from the
 * saving build. Copy both pointers from src→dest. */
static void _Rebuild_CupRecordNameSourcePointers(void)
{
    /* Iterate as typed CompetitionCareer[] — entry stride is 10 on
     * 32-bit, 18 on 64-bit. The text[] copy reads/writes two pointer-
     * width entries (4 B each → 8 B on 32-bit, 8 B each → 16 B on
     * 64-bit), so use sizeof(void *) and memcpy 2*sizeof(void *). */
    extern CompetitionCareer careerContests[];
    for (CompetitionCareer *e = careerContests;
         (uintptr_t)e->competition != 0xFFFFFFFFu; e++) {
        uint8_t *src  = (uint8_t *)e->competition;
        uint8_t *dest = (uint8_t *)e->competitionCopy;
        int8_t nameOffset = ((Competition *)src)->unk1;
        if (nameOffset != 0) {
            memcpy(dest + 5 + nameOffset,
                   src  + 5 + nameOffset,
                   2 * sizeof(void *));
        }
    }
}

/* Step 4 helper: re-derive the top-level competition slot pointers.
 *
 * Mirrors the lookup in `_InitializeNewSeason` (lines 994-1067) but
 * does NOT call `_Career_SetCurrentCompetitionSlot` — that helper is
 * destructive (resets slot state from the static comp template,
 * losing the loaded season's progress). Here we only re-establish
 * the pointer values; the slot state itself is already correctly
 * loaded from the .CAR.
 *
 * National-selector careers populate these pointers via
 * `careerCupPickList` instead of competitionsTable. The pickList is
 * inside the saved overlay, so its entries are stale too — rebuild
 * via `_Career_SetCupPickList_ForSelectedRegion(0)` first. */
static void _Rebuild_CompetitionSlotPointers(void)
{
    /* Country / pickList tables are `Competition *[]` — 8 bytes per
     * entry on 64-bit. The Amiga path walked them as `uint32_t *`
     * which only read half a pointer per slot. Use `void **` so the
     * stride is correct on both targets. Sentinels stored as
     * `(Competition *)-1` / `-2` compare via (uintptr_t)X == -1u /
     * -2u — both halves of the pointer are all-ones on Amiga and
     * zero-extend identically on 64-bit. */
    void **compTablePtr;

    if (isNationalSelector != 0) {
        _Career_SetCupPickList_ForSelectedRegion(0);
        compTablePtr = (void **)&careerCupPickList;
    } else {
        uint16_t tableIdx = (selTeamNumber == 0xFFFF)
            ? SaveJson_GetLoadedPrimaryCountryCode()
            : (selTeamNumber >> 8);
        compTablePtr = (void **)competitionsTable[tableIdx];
    }
    if (!compTablePtr)
        return;

    /* Slot 0 (primary): always present. */
    currentCompetitionPtr = (uint8_t *)*compTablePtr++;
    /* One filler slot after the primary entry (the `(Competition *)-2`
     * sentinel in country tables like albaniaTable). */
    compTablePtr++;

    /* Slot 1 (secondary): may be -1 sentinel. */
    if ((uintptr_t)*compTablePtr != (uintptr_t)-1) {
        secondaryCompetitionPtr = (uint8_t *)*compTablePtr++;
        if ((uintptr_t)*compTablePtr != (uintptr_t)-1) {
            tertiaryCompetitionPtr = (uint8_t *)*compTablePtr;
        } else {
            tertiaryCompetitionPtr = 0;
        }
    } else {
        secondaryCompetitionPtr = 0;
        tertiaryCompetitionPtr = 0;
    }

    /* Slot 4 (DIY) — derive from primary comp's DIY link. Mirrors
     * `_DIY_SelectCompetitionSlot4FromCurrentDivision`. The diyEntry
     * payload is two pointers (diy comp ptr + diy table ptr) — 8 B
     * stride on 64-bit. Use `void **` read instead of the old
     * `*(uint32_t *)` which truncated. */
    diyCompetitionPtr = 0;
    diyCompetitionTablePtr = (uint32_t *)0;
    if (currentCompetitionPtr != 0) {
        uint8_t *primary = (uint8_t *)currentCompetitionPtr;
        if (((Competition *)primary)->type == 0) {
            uint16_t off = currentDivisionIndex * 6;
            uint8_t *entry = primary + off;
            uint8_t diyOffset = entry[0x12];
            if (diyOffset != 0) {
                void **diyEntry = (void **)(entry + 0x12 + diyOffset);
                diyCompetitionPtr = (uint8_t *)diyEntry[0];
                diyCompetitionTablePtr = (uint32_t *)diyEntry[1];
            }
        }
    }

    /* Slot 3 (quaternary, euro cups) — re-derive from the SAVED euro-cup
     * slot CONTEXT, not from quaternaryCompetitionPtr.
     *
     * The JSON save format no longer stores quaternaryCompetitionPtr (it is
     * a pointer-to-disk field). The previous gate `quaternaryCompetitionPtr
     * != 0` therefore read the stale pre-load value — 0 on a fresh-start
     * load — and left an ACTIVE euro-cup slot inactive. The careerMenu then
     * draws the slot-3 schedule entry, _CareerGameListCommon sets
     * compInfo = quaternaryCompetitionPtr = NULL, and `compInfo[5]` SIGSEGVs
     * (Playaveli's CLAUDE.CAR repro, in _CareerGameListCommon menus.c:6687).
     *
     * Reliable saved signal: the slot-3 context (euroCupsCompetitionContext)
     * is persisted and restored on load. Its competitionType (+0x00) and
     * isActive (+0x02) are 2/1 for an active European Cup (type-2 group
     * tournament) and 1/1 for the Cup Winners'/UEFA cup (type-1); an
     * inactive slot reads back all-zero. euroCupSlotIndex (also saved)
     * selects which of the three cups; we then VALIDATE that the saved
     * context's type agrees with that cup, so a stale euroCupSlotIndex (it
     * is only written on qualification) or a stale context can't activate a
     * mismatched slot — the original _FindTeamInDIY-panic concern. */
    {
        uint8_t *euroCup3 = 0;
        if (euroCupSlotIndex == 0)      euroCup3 = euroCupCopy;        /* type 2 */
        else if (euroCupSlotIndex == 1) euroCup3 = cupWinnersCupCopy;  /* type 1 */
        else if (euroCupSlotIndex == 2) euroCup3 = uefaCupCopy;        /* type 1 */

        DIYCompetition *q3 = (DIYCompetition *)careerCompetitionSlotPtrTable[3];
        /* Participation gate: euroCupSlotIndex is only written on qualification
         * (_Career_SelectEuroCupSlotIfTeamParticipates) and is SAVED, so it can
         * be stale from a prior season when the club DID qualify. The euro cup
         * is contested every season by the qualifying clubs, so its slot-3
         * context can be active + type-matching while the player's club is NOT
         * in it. Without the participation check the fixup re-activates slot 3
         * for a non-participant, and ShowChairmanOpinionAboutTheSeason then
         * calls _FindTeamInDIY(selTeamNumber, euroCupsCompetitionContext) which
         * hard-panics 0x6006 for a club coach (team not found). Mirror the
         * season-init enrolment: only activate if the club is actually in the
         * cup's team list. (gdb-confirmed 2026-06-10: club 5141 in SERIE A, euro
         * slot held EUROPEAN CHAMPIONS CUP whose Italian entrant is 5142, stale
         * euroCupSlotIndex=0.) */
        if (euroCup3 != 0 && q3->isActive != 0 &&
            q3->competitionType == ((Competition *)euroCup3)->type &&
            _Career_TeamInEuroCup(euroCup3)) {
            quaternaryCompetitionPtr = euroCup3;
        } else {
            quaternaryCompetitionPtr = 0;  /* slot 3 inactive this season */
        }
    }
}

/* Step 5 helper: zero the four *CompStringOffset fields on load.
 *
 * These are stale absolute pointers into the strings table (saved
 * verbatim from the saving binary's runtime address space). The only
 * consumer is `_InitializeNewSeason` which copies them into the
 * SeasonInformations buffer at offsets 0x16/0x1A/0x1E/0x22 to display
 * the previous season's competition names on the season-end summary.
 * If we zero them, the summary may show empty strings for the prior
 * season's results, but no crash from dereferencing a stale pointer.
 *
 * Future improvement: re-derive each from its slot's compId by
 * looking up the comp record's `text[0]` field (same source as the
 * cup-record nameSource). Deferred until we have a save that exposes
 * a visible regression here. */
static void _Rebuild_CompStringOffsets(void)
{
    primaryCompStringOffset    = 0;
    secondaryCompStringOffset  = 0;
    tertiaryCompStringOffset   = 0;
    quaternaryCompStringOffset = 0;
}

__attribute__((used, externally_visible))
void _RebuildPointersAfterLoad(void)
{
    /* Step 1: refresh careerCompetitionSlotPtrTable[0..4]. Same five
     * lines as `_InitNewSeason` lines 1165-1169 — but this time after
     * load, so the saved (now stale) pointer values get replaced with
     * the current run's competition-buffer addresses. */
    careerCompetitionSlotPtrTable[0] = (uint8_t *)&competitionFileBuffer;
    careerCompetitionSlotPtrTable[1] = (uint8_t *)&competitionContext;
    careerCompetitionSlotPtrTable[2] = (uint8_t *)&tertiaryCompetitionContext;
    careerCompetitionSlotPtrTable[3] = (uint8_t *)&euroCupsCompetitionContext;
    careerCompetitionSlotPtrTable[4] = (uint8_t *)&diyCompetitionContext;

    /* Step 1b: re-derive each slot buffer's DIYCompetition.namePtr (+0x28).
     *
     * namePtr is a pointer-to-disk field: at save time it holds the
     * SAVING binary's absolute address of the division/cup name string
     * (e.g. divisionStringTable[d7], assigned in _InitDIYLeagueFromRecord
     * diy_competition.c:8869 / _InitDIYCupFromRecord:8924). The JSON save
     * stores it as raw bytes (primary_name_ptr), so after a cross-target
     * load it's a foreign address — on the Amiga a 64-bit-SDL value like
     * 0x1_004A70B8 truncates to a ~garbage 32-bit pointer. It then flows
     * into SeasonInformations.leagueStrPtr (line ~1067 below) and is
     * dereferenced by the season-recap renderer (statistics.c:2184,
     * _StringCopy/RenderCompLine), which walks it to a NUL — off a wild
     * address that smashes the stack (observed: wild PC + "SWOS2" string
     * bytes on the stack walking past the comp buffer).
     *
     * Re-point it at the slot's OWN competitionName[36] buffer (+0x04),
     * which the same init code already filled with the full display name
     * ("GERMAN 1. LIGA"). That's valid, cross-target-stable memory in the
     * just-loaded buffer, so the recap renders and nothing derefs a
     * foreign address.
     *
     * LIMITATION (deferred): the original namePtr was the *bare* division
     * name ("1. LIGA") and is also tested by IDENTITY (== aFriendly,
     * diy_competition.c:2930/3223) and by ADDRESS RANGE (aEuropeanTour..
     * aOceaniaTour, career_status.c:3774) to classify friendlies/tours.
     * Pointing at competitionName loses those — fine for ordinary
     * league/cup (not a friendly, not a tour, and the recap shows the
     * country-prefixed name), but a friendly/tour career would
     * mis-classify. The full fix is a SYMBOLIC save (table-id + division
     * index, restored to the original static-string address) so identity
     * and range survive; see project_primary_comp_block_handoff. This
     * repoint unblocks the common case without that larger change. */
    for (int _slot = 0; _slot < 5; _slot++) {
        DIYCompetition *c = (DIYCompetition *)careerCompetitionSlotPtrTable[_slot];
        if (c != NULL && c->namePtr != NULL)
            c->namePtr = (uint8_t *)c->competitionName;
    }

    /* Step 2: repopulate diyCompetitionStart (the runtime working
     * struct outside the careerFileBuffer overlay) from the primary
     * slot's saved bytes. Without this, the upcoming-match list
     * shows fixtures generated against whatever competition the
     * previous in-memory state held — typically the player team
     * "playing" every other team at home, with 00'00 kickoff times,
     * because the league fixture order/times live in
     * diyCompetitionStart and not in the saved overlay slot.
     *
     * VIEW SQUADS already worked without this step because that path
     * goes through `_Career_OpenCompetitionSlotMenuAndApplyChanges`,
     * which calls _Career_LoadCompetitionSlotStateFromBuffer itself.
     * The Play Match / Next Match menus skip that helper, so we
     * have to populate diyCompetitionStart up-front at load time. */
    _Career_LoadCompetitionSlotStateFromBuffer((uint8_t *)&competitionFileBuffer);

    /* Step 3: walk careerContests and rewrite each *Copy record's
     * embedded nameSource pointer at (record + 5 + nameOffset).
     * Without this, `_InitDIYLeagueFromRecord` SIGSEGVs on a stale
     * string pointer when auto-advancing euroCupCopy /
     * cupWinnersCupCopy / uefaCupCopy after a cross-build load. */
    _Rebuild_CupRecordNameSourcePointers();

    /* Step 4: re-derive top-level competition slot pointers
     * (`currentCompetitionPtr` / `secondaryCompetitionPtr` /
     * `tertiaryCompetitionPtr` / `quaternaryCompetitionPtr` /
     * `diyCompetitionPtr` / `diyCompetitionTablePtr`) from
     * competitionsTable + selTeamNumber + euroCupSlotIndex. Saved
     * values were stale BSS addresses from the saving build. */
    _Rebuild_CompetitionSlotPointers();

    /* Step 5: zero the four *CompStringOffset fields. They held
     * stale absolute pointers into the strings table; downstream
     * consumers (_InitializeNewSeason → SeasonInformations) will
     * tolerate NULL by displaying empty text. */
    _Rebuild_CompStringOffsets();
}

/* ================================================================
 * Chunk 198: Career_ViewOrEditCompetitionSlot  (SWOS2.s line 32242)
 *
 * Views or edits a competition slot: maps the competition pointer
 * to its career copy via careerContests table, handles special
 * europeanChampionships/worldCup replacements, then determines which
 * slot owns the competition and shows appropriate menu.
 * If the competition isn't in any active slot, auto-advances it.
 *
 * in:  a0 = competition pointer, d0 = divisionIndex/selection
 * ================================================================ */
extern uint8_t europeanChampionships[];
extern uint8_t europeanChampionships2Copy[];
extern uint8_t worldCup[];
extern uint8_t worldCup2Copy[];
/* nationalTeamCompetitionsAndQualifications1+0x96 effectively points
 * 6 bytes before careerContests[0] — the loop skips 6 bytes then reads
 * 4 bytes per iteration, matching the 10-byte careerContests entries. */

extern void _ShowDiyLeaguesTournamentMenu(uint16_t type, uint16_t viewOnly, uint16_t focusOverride);
extern void _ShowDIYCupMenuWithParams(uint16_t arg0, uint16_t arg1, uint16_t arg2);
extern void _DIY_EditCompetitionSlot(uint16_t paramA, uint16_t paramB, uint16_t paramC);
extern void _SaveSelectedTeams(void);
extern void _RestoreSelectedTeams(void);
extern void _Career_LoadCompetitionSlotStateFromBuffer(uint8_t *src);
extern void _Career_AutoAdvanceInternationalCompetitionFixtures(uint8_t *record, uint16_t index);
extern void _Career_AutoAdvanceCupCompetitionFromRecord(uint8_t *record);
extern void _Career_AutoAdvanceLeagueCompetitionFromRecord(uint8_t *record);

__attribute__((used, externally_visible))
void _Career_ViewOrEditCompetitionSlot(uint8_t *comp, uint16_t sel)
{
    /* Step 1: Map competition pointer to its career copy via the
     * typed CompetitionCareer[] — entry stride differs by bitness
     * (10 B on 32-bit, 18 B on 64-bit), so iterate as the typed
     * array instead of advancing tbl by a literal byte count. */
    extern CompetitionCareer careerContests[];
    for (CompetitionCareer *e = careerContests;
         (uintptr_t)e->competition != 0xFFFFFFFFu; e++) {
        if ((uint8_t *)e->competition == comp) {
            comp = (uint8_t *)e->competitionCopy;
            break;
        }
    }

    /* Step 2: Special replacements for europeanChampionships and worldCup */
    if (comp == europeanChampionships)
        comp = europeanChampionships2Copy;
    if (comp == worldCup)
        comp = worldCup2Copy;

    /* Step 3: Find which slot owns this competition */
    uint8_t *slotCtx = (uint8_t *)&competitionFileBuffer;

    if (comp == (uint8_t *)currentCompetitionPtr) {
        if (sel == currentDivisionIndex)
            goto found_slot;
    }

    slotCtx = (uint8_t *)&competitionContext;
    if (comp == (uint8_t *)secondaryCompetitionPtr)
        goto found_slot;

    slotCtx = (uint8_t *)&tertiaryCompetitionContext;
    if (comp == (uint8_t *)tertiaryCompetitionPtr)
        goto found_slot;

    slotCtx = (uint8_t *)&euroCupsCompetitionContext;
    if (comp == (uint8_t *)quaternaryCompetitionPtr)
        goto found_slot;

    /* Not in any active slot — auto-advance this external competition */
    _SaveSelectedTeams();
    {
        int8_t compType = ((Competition *)comp)->type;
        if (compType == 1) {
            _Career_AutoAdvanceCupCompetitionFromRecord(comp);
        } else if (compType == 2) {
            _Career_AutoAdvanceLeagueCompetitionFromRecord(comp);
        } else {
            _Career_AutoAdvanceInternationalCompetitionFixtures(comp, sel);
        }
    }
    _RestoreSelectedTeams();
    return;

found_slot:
    _Career_LoadCompetitionSlotStateFromBuffer(slotCtx);
    {
        uint8_t compType = slotCtx[1];
        if (compType == 1) {
            _ShowDIYCupMenuWithParams(1, 1, 0);
        } else if (compType == 2) {
            _DIY_EditCompetitionSlot(1, 1, 0);
        } else {
            _ShowDiyLeaguesTournamentMenu(1, 1, 0);
        }
    }
}
