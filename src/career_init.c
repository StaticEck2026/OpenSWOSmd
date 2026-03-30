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
#include "menu_system.h"
#include "input_text.h"
#include "random.h"
#include "string_utils.h"
#include "panic.h"

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

/* careerCupPickList: first element of a 5-pointer array in BSS
 * (spills into adjacent BSS_01_164B4A for remaining 4 entries) */
extern uint32_t careerCupPickList;
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
    uint32_t *pickList = &careerCupPickList;
    int16_t i;

    for (i = 4; i >= 0; i--) {
        if (pickList[i] == (uint32_t)invitationTournamentCopy) {
            if (_Rand() & 1)
                pickList[i] = (uint32_t)tourCopy;
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
 * The low nibble of the bg high word holds the color palette index. */
__attribute__((used, externally_visible))
void _SetPlayerCoachFieldColor(void)
{
    /* menuEntries + 0x1F8 = &menuEntries[9] (0x1F8 / 56 = 9)
     * bg field at offset 30 — access high word via byte pointer to avoid
     * packed struct alignment warning */
    uint8_t *base = (uint8_t *)&menuEntries[9];
    uint16_t val;

    /* Read high word of bg (offset 30, big-endian) */
    val = *(uint16_t *)(base + 30);
    val = (val & 0xFFF0) + 13; /* BRIGHT_BLUE_TO_BLUE_13 */
    *(uint16_t *)(base + 30) = val;

    if (coachOrPlayer != 0) {
        /* Override: blue to purple (11) */
        val = *(uint16_t *)(base + 30);
        val = (val & 0xFFF0) + 11; /* BLUE_TO_PURPLE_11 */
        *(uint16_t *)(base + 30) = val;
    }
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
 * entry = a5 from menu system; fg field holds pointer to text buffer. */
__attribute__((used, externally_visible))
void _SetName(MenuEntry *entry)
{
    _InputText(9, (uint8_t *)entry->fg);
}

/* SetSurname: start text input for surname field (max 13 chars).
 * entry = a5 from menu system; fg field holds pointer to text buffer. */
__attribute__((used, externally_visible))
void _SetSurname(MenuEntry *entry)
{
    _InputText(13, (uint8_t *)entry->fg);
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
extern uint32_t selectedTeamPtr;
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
    ((uint8_t *)&careerTeam)[4] = controlByte;
    ((uint8_t *)&selTeamsPtr)[4] = controlByte;

    /* Copy 16 bytes of playerOrder (offset 60 in TeamFile) to currentMatchPlayers */
    teamPtr = _GetTeamPointer(selTeamNumber);
    uint8_t *playerOrderSrc = (uint8_t *)teamPtr + 60;
    for (int16_t i = 15; i >= 0; i--) {
        currentMatchPlayers[i] = playerOrderSrc[i];
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

/* careerContests table: array of {src_ptr, dest_ptr, byte_count}, terminated by 0xFFFFFFFF */
extern uint8_t  careerContests[];

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
extern uint32_t currentCompetitionPtr;
extern uint32_t secondaryCompetitionPtr;
extern uint32_t tertiaryCompetitionPtr;
extern uint32_t quaternaryCompetitionPtr;
extern uint32_t diyCompetitionPtr;
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
extern uint8_t  careerTeamFlagsByRegionTable[];  /* 6-entry pointer table */
extern uint8_t  someTeamFlags;
extern uint8_t  nationalTeamNationality;
extern uint8_t  nationalTeamFileId;
extern uint16_t worldCupQualifyingTeam;
extern uint16_t euroChampionshipQualTeam;
extern uint16_t seasonCycleNationalFlagsA[];
extern uint16_t seasonCycleNationalFlagsB[];
extern uint32_t primaryCompStringOffset;
extern uint32_t secondaryCompStringOffset;
extern uint32_t tertiaryCompStringOffset;
extern uint32_t quaternaryCompStringOffset;
extern uint8_t  euroCupCopy[];
extern uint8_t  cupWinnersCupCopy[];
extern uint8_t  uefaCupCopy[];
extern uint32_t competitionsTable[];
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

    /* Copy careerContests table entries: {src, dest, count} until src==0xFFFFFFFF.
     * Uses volatile to prevent LTO from eliminating the ROM→BSS copies
     * (same pattern as the tactic copy bug). */
    uint8_t *tbl = careerContests;
    for (;;) {
        uint32_t srcAddr = *(uint32_t *)tbl;
        tbl += 4;
        if (srcAddr == 0xFFFFFFFF)
            break;
        volatile uint8_t *dst = *(volatile uint8_t **)tbl;
        tbl += 4;
        uint16_t count = *(uint16_t *)tbl;
        tbl += 2;
        volatile uint8_t *src = (volatile uint8_t *)srcAddr;
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
    if (teamNoOfferAccepted != 0xFFFF) {
        if (youreSacked == 0) {
            _ShowLeavingMessage();
        }
        _Career_AdvanceToNextSeason_UpdateCompetitions();
        _Career_ContinueToNextSeasonInitTeamAndFunds(teamNoOfferAccepted, teamFundsAccepted);
        _InitializeNewSeason();
    } else {
        _ShowChairmanOpinionAboutTheSeason();
        _Career_AdvanceToNextSeason_UpdateCompetitions();
        _InitializeStartingBalanceForNewSeason();
        _InitializeNewSeason();
    }
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
    /* Get start of team list: base + 7 + base[7] */
    uint8_t numEntries = cupCopy[7];
    uint16_t *teamList = (uint16_t *)(cupCopy + 7 + numEntries);

    /* Get number of teams based on competition type (byte at offset 1) */
    uint8_t compType = cupCopy[1];
    int16_t numTeams;
    if (compType == 1) {
        numTeams = (int8_t)cupCopy[0xA];
    } else if (compType == 2) {
        numTeams = (int8_t)cupCopy[0xF];
    } else {
        numTeams = (int8_t)cupCopy[0xD];
    }

    /* Search for selTeamNumber in team list */
    uint16_t target = selTeamNumber;
    numTeams--;
    for (int16_t i = numTeams; i >= 0; i--) {
        if (*teamList++ == target)
            goto found;
    }
    return 1;  /* not found */

found:
    {
        /* Save quaternary slot state */
        uint16_t savedA = quaternaryCompSlotStateA;
        uint16_t savedB = quaternaryCompSlotStateB;
        uint16_t savedC = quaternaryCompSlotStateC;

        /* Set slot 3 for this euro cup (d0=3, d1=euroCupIndex, d2=0) */
        _Career_SetCurrentCompetitionSlot(cupCopy, 3, 0, 0);

        uint8_t *seasonPtr = _GetCurrentSeasonPointer();
        *(uint32_t *)(seasonPtr + 0x22) = quaternaryCompStringOffset;

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
    if (comp[1] != 0)
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
    teamsLoaded = 0;
    currentSeasonShowing++;
    if (currentSeasonShowing == 20)
        currentSeasonShowing = 0;

    selTeamSelectedPlayerPosition = 0xFFFF;

    /* Clear 4 entries of careerUnavailablePlayerList */
    *(uint16_t *)(careerUnavailablePlayerList + 0) = 0xFFFF;
    *(uint16_t *)(careerUnavailablePlayerList + 4) = 0xFFFF;
    *(uint16_t *)(careerUnavailablePlayerList + 8) = 0xFFFF;
    *(uint16_t *)(careerUnavailablePlayerList + 12) = 0xFFFF;

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
    uint8_t *seasonInfo = _GetCurrentSeasonPointer();
    *(uint16_t *)(seasonInfo + 0) = selTeamNumber;  /* CLUBNO */
    *(uint32_t *)(seasonInfo + 22) = 0;  /* LEAGUESTRINGOFFSET */
    *(uint32_t *)(seasonInfo + 26) = 0;  /* STROFFSET1 */
    *(uint32_t *)(seasonInfo + 30) = 0;  /* STROFFSET2 */
    *(uint32_t *)(seasonInfo + 34) = 0;  /* FIELD_22 */
    *(uint8_t *)(seasonInfo + 86) = 0;   /* FIELD_56 */
    *(uint8_t *)(seasonInfo + 87) = 0;   /* FINALPOSITION */
    *(uint8_t *)(seasonInfo + 88) = 0;   /* FIELD_58 */
    *(uint8_t *)(seasonInfo + 89) = 0;   /* FIELD_59 */
    *(uint8_t *)(seasonInfo + 40) = 0;   /* BESTSCORERNAME (first byte) */
    *(uint8_t *)(seasonInfo + 105) = 0;  /* MANAGERSTATUS */

    /* Copy team name to season info CLUBNAME (offset 2) */
    uint8_t *teamNameSrc = (uint8_t *)&careerTeam + 0 + 5;  /* TEAMFILEEX_TEAM(0) + TEAMFILE_TEAMNAME(5) */
    _StringCopy(teamNameSrc, seasonInfo + 2);

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
    uint32_t *compTablePtr;

    if (countryCode >= 80 && countryCode <= 85) {
        /* National team selector */
        isNationalSelector = 1;

        /* Check same nationality call */
        if (selTeamNumber == lastNationalityCall) {
            sameNationalCallFlag = 1;
        }

        /* Look up team flags from region table */
        uint32_t *regionTable = (uint32_t *)careerTeamFlagsByRegionTable;
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

        compTablePtr = &careerCupPickList;
    } else {
        /* Club team */
        nationalTeamFileCode = (uint8_t)(selTeamNumber >> 8);  /* move.b — high byte */

        uint16_t tableIdx = selTeamNumber >> 8;
        compTablePtr = (uint32_t *)competitionsTable[tableIdx];
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

        seasonInfo = _GetCurrentSeasonPointer();
        *(uint32_t *)(seasonInfo + 22) = primaryCompStringOffset;

        primaryCompSlotStateC = savedC;
        primaryCompSlotStateB = savedB;
        primaryCompSlotStateA = savedA;
    }

    /* Skip 1 long (4 bytes) past the primary comp entry */
    compTablePtr = (uint32_t *)((uint8_t *)compTablePtr + 4);

    /* Set up secondary competition (slot 1) if present */
    if (*(int32_t *)compTablePtr != (int32_t)0xFFFFFFFF) {
        uint16_t savedA = secondaryCompSlotStateA;
        uint16_t savedB = secondaryCompSlotStateB;
        uint16_t savedC = secondaryCompSlotStateC;

        uint8_t *comp = (uint8_t *)*compTablePtr;
        compTablePtr++;
        _Career_SetCurrentCompetitionSlot(comp, 1, 0, 0);

        seasonInfo = _GetCurrentSeasonPointer();
        *(uint32_t *)(seasonInfo + 0x1A) = secondaryCompStringOffset;

        secondaryCompSlotStateC = savedC;
        secondaryCompSlotStateB = savedB;
        secondaryCompSlotStateA = savedA;

        /* Set up tertiary competition (slot 2) if present */
        if (*(int32_t *)compTablePtr != (int32_t)0xFFFFFFFF) {
            uint16_t savedA2 = tertiaryCompSlotStateA;
            uint16_t savedB2 = tertiaryCompSlotStateB;
            uint16_t savedC2 = tertiaryCompSlotStateC;

            comp = (uint8_t *)*compTablePtr;
            _Career_SetCurrentCompetitionSlot(comp, 2, 0, 0);

            seasonInfo = _GetCurrentSeasonPointer();
            *(uint32_t *)(seasonInfo + 0x1E) = tertiaryCompStringOffset;

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

    /* diagnostic removed */

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
            lastKey = cachePtr[1];
            uint16_t stride = *(uint16_t *)(cachePtr + 2);
            cachePtr += stride;
        }
        uint16_t d0 = (uint16_t)(lastKey - careerJobOfferPhaseBias) & 0xFF;
        if (d0 == 0)
            d0 = 1;
        wageScalingDivisor = d0;
    }

    lastFinancialWeekIndex = careerJobOfferPhaseBias;

    _InitTeams();

    /* Copy manager name to team data (offset 0x24 in team file) */
    teamFile = _VerifyTeamLoaded(selTeamNumber);
    {
        uint8_t *namePos = (uint8_t *)teamFile + 0x24;
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

    /* diagnostic removed */
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
    uint8_t compTableIdx = src[2];
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

    /* Look up competitions table by index */
    uint32_t tableIdx = (uint16_t)(compTableIdx & 0xFF) << 2;
    uint32_t *compTable = (uint32_t *)((uint8_t *)competitionsTable + tableIdx);
    compTable = (uint32_t *)*compTable;

    /* Slot 0: primary competition */
    uint8_t *comp = (uint8_t *)*compTable;
    compTable++;
    _Career_SetCurrentCompetitionSlot(comp, 0, currentDivisionIndex, 1);

    /* Skip 4 bytes past primary entry */
    compTable = (uint32_t *)((uint8_t *)compTable + 4);

    /* Slot 1: secondary competition (if present) */
    if (*(int32_t *)compTable != (int32_t)0xFFFFFFFF) {
        comp = (uint8_t *)*compTable;
        compTable++;
        _Career_SetCurrentCompetitionSlot(comp, 1, 0, 0);

        /* Slot 2: tertiary competition (if present) */
        if (*(int32_t *)compTable != (int32_t)0xFFFFFFFF) {
            comp = (uint8_t *)*compTable;
            _Career_SetCurrentCompetitionSlot(comp, 2, 0, 0);
        }
    }

    seasonFinished = 0;
    careerFinished = 0;

    /* Count non-computer-controlled squads */
    seasonNumberOfSquads = 0;
    uint16_t *squadTeams = seasonSquadTeamNumbers;
    uint8_t *teamPtr = (uint8_t *)&selTeamsPtr;
    int16_t numTeams = numSelectedTeams - 1;
    for (int16_t i = numTeams; i >= 0; i--) {
        if (teamPtr[4] != 1) {  /* selTeamIsComputerFlag offset = 4 */
            seasonNumberOfSquads++;
            *squadTeams++ = *(uint16_t *)teamPtr;
        }
        teamPtr += 684;  /* sizeof(TeamFileEx) */
    }

    if (seasonNumberOfSquads == 0)
        careerFinished = 1;

    viewRandomPhase = careerJobOfferPhaseBias;
    _InitTeams();
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
    /* Step 1: Map competition pointer to its career copy via careerContests table.
     * Each careerContests entry is {uint32_t src, uint32_t dest, uint16_t len} = 10 bytes.
     * We search for comp in the src field and replace with dest. */
    uint8_t *tbl = careerContests;
    for (;;) {
        uint32_t src = *(uint32_t *)tbl;
        if (src == 0xFFFFFFFF)
            break;
        if (src == (uint32_t)comp) {
            comp = *(uint8_t **)(tbl + 4);
            break;
        }
        tbl += 10;
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
        uint8_t compType = comp[1];
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
