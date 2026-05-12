/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* job_offers.c — Job offer queue management and menu functions.
 *
 * Chunk 79: 6 exported functions ported from SWOS2.s lines 84210-84322.
 * Chunk 81: 5 more exported functions from SWOS2.s lines 84081-84200.
 * Chunk 99: 8 job offer menu functions from SWOS2.s lines 82566-82903.
 */

#include "job_offers.h"
#include "endian_helpers.h"
#include "game_types.h"
#include "menu_system.h"
#include "random.h"
#include "string_utils.h"

/* Prevent GCC from converting byte loops to memcpy/memset (no libc) */
#pragma GCC optimize("no-tree-loop-distribute-patterns")

/* ---- ASM externs ---- */
extern uint16_t numJobOffers;
extern JobOffer jobOffers[];         /* 5-entry job offers queue (in SWOS2.s BSS) */
extern uint16_t numNewJobOffers;
extern JobOffer newJobOffers[];      /* 5-entry new job offers queue (in SWOS2.s BSS) */
extern uint8_t fundsBuffer[];        /* temporary buffer for team name during offer conversion */
extern MenuEntry menuEntries[];
extern uint16_t teamNoOfferAccepted;
extern uint32_t teamFundsAccepted;
extern uint16_t youreSacked;
extern uint16_t highlightedLeagueTeamId;

/* Additional externs for Career_MaybeGenerateJobOffer */
extern uint16_t careerSeasonsCompleted;
extern uint16_t currentSeasonShowing;
extern uint16_t careerJobOfferMode;
extern uint8_t  viewRandomPhase;
extern int8_t   careerJobOfferPhaseBias;
extern uint16_t wageScalingDivisor;
extern uint16_t sameNationalCallFlag;
extern uint16_t lastNationalityCall;
extern int16_t  boardConfidenceScore;
extern int16_t  sackedReputationScore;
extern int16_t  nationalSelectorReputationPoints;
extern uint8_t  majorTrophyBonusValue;
extern uint16_t isNationalSelector;
extern uint16_t selTeamNumber;
extern uint16_t numSelectedTeams;
extern uint8_t  selTeamsPtr;
extern uint16_t currentDivisionIndex;
extern uint8_t competitionFileBuffer[];
extern uint16_t numDiyTeams;
extern uint16_t diyTeamMinDivisionIndex;
extern uint16_t diyTeamMaxDivisionOffset;
extern uint16_t nat_teams_rand_limit1;
extern uint8_t  nationalTeams[];
extern uint8_t  nationalTeamFileCode;
extern uint8_t  teamFileBuffer;
extern uint16_t careerFileBuffer;
extern uint8_t  tmdFileBuffer;

extern uint16_t _Rand(void);
extern uint16_t _GetAveragePlayerPrice(TeamFileEx *team);
extern int16_t  _IsNewJobOffer(uint16_t teamNumber);
extern int16_t  _IsOldJobOffer(uint16_t teamNumber);
extern void     _AddJobOffer(uint16_t teamNumber, uint32_t initialFunds);
extern int16_t  _GetBoardConfidenceScore(void);
extern int16_t  _IsTeamCodeOkForNationalSelector(uint16_t code);
extern int16_t  _IsPlayerOkForNationalSelector(void);
extern uint16_t _FindTeamInDIY(uint16_t teamId, DIYCompetition *compBuffer);
extern uint32_t _CalculateStartingBalance(uint16_t teamId);
extern void     _LoadTeamFile(uint16_t code);

/* Job offer menu data (in SWOS2.s) */
extern uint16_t jobOffersFromRetired;
extern int16_t  jobOfferOrdinal;
extern int16_t  oldJobOfferOrdinal;
extern uint16_t selectedOldJobOfferOrdinal;
extern uint16_t teamNoOfferingJob;
extern uint32_t teamOfferingJobPtr;
extern int32_t  jobOfferFunds;
extern uint16_t viewFutureTeamModeFlag;

/* String constants */
extern uint8_t aRetire[];
extern uint8_t aInternationalJ[];
extern uint8_t aNewJob[];
extern uint8_t aDearAB[];
extern uint8_t aWeHereAtAWould[];
extern uint8_t aCoachingJobAtO[];
extern uint8_t aWeCanOfferYouA[];
extern uint8_t aWeHopeToBeAble[];
extern uint8_t aInTheRegionOfA[];
extern uint8_t aWeLookForwardT_1[];
extern uint8_t aTheMembersOfTh[];
extern uint8_t aOfAWouldLikeTo[];
extern uint8_t aAsCoachOfTheNa[];
extern uint8_t aWeCanPromiseYo[];
extern uint8_t aAnAnnuallyRevi[];
extern uint8_t aWeLookForwardT[];
extern uint8_t aWeHereAtAAreDe[];
extern uint8_t aHaveAcceptedTh[];
extern uint8_t aFromTheStartOf_0[];
extern uint8_t aWeHopeToBeAble_0[];
extern uint8_t aInTheRegionOfA_0[];
extern uint8_t aAndWeLookForwa[];
extern uint8_t aTheMembersOfTh_0[];
extern uint8_t aOfAAreDelighte[];
extern uint8_t aAcceptedThePos[];
extern uint8_t aFromTheStartOf[];
extern uint8_t aWeLookForwardT_0[];
extern uint8_t managerSexBuffer[];
extern uint8_t managerSurnameBuffer[];
extern uint8_t aViewTeam[];

extern uint32_t competitionsTable[];
extern uint8_t seasonInformationsArray[];
extern uint16_t currentSeasonShowing;

/* Season info offsets */
#define SEASONINFO_MANAGERSTATUS  105
#define SEASONINFO_SIZE           106

/* ---- C-ported function externs ---- */
extern void _SetExitMenuFlag(void);
extern void _SetCurrentEntry(uint16_t index);
extern TeamFileEx *_GetTeamPointer(uint16_t teamId);
extern void _PrintFormatted(const uint8_t *fmt, uint8_t *out,
                     uint16_t n0, uint16_t n1, uint16_t n2, uint16_t n3,
                     uint16_t n4, uint16_t n5, uint16_t n6,
                     const uint8_t *strA, const uint8_t *strB, const uint8_t *strC);

/* ---- C-callable ASM bridges ---- */
extern void _Int2Pounds(uint32_t value, uint8_t *dst);
extern int16_t _ShowSquad(TeamFile *teamPtr, uint8_t *title, uint32_t maskLo, uint32_t maskHi, uint16_t arg2, uint16_t arg3);
extern void _Career_ViewOrEditCompetitionSlot(uint8_t *comp, uint16_t sel);

/* Helper: menu entry from byte offset */

/* Helper: access bg as the high 16 bits (big-endian BACKANDFRAMECOLOR) */
static inline uint16_t bgColor(MenuEntry *e) { return getBgColorWord(e); }
static inline void setBgColor(MenuEntry *e, uint16_t v) { setBgColorWord(e, v); }

/* Helper: GetCurrentSeasonPointer */
static inline uint8_t *getCurrentSeasonPointer(void) {
    return seasonInformationsArray + (uint16_t)currentSeasonShowing * SEASONINFO_SIZE;
}


/* ================================================================
 * IsOldJobOffer  (SWOS2.s line 84210)
 *
 * Checks if a team number already exists in the jobOffers queue.
 * Returns: 0 if found (Z set in asm), 1 if not found (Z clear).
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _IsOldJobOffer(uint16_t teamNumber)
{
    uint16_t count = numJobOffers;
    if (count == 0)
        return 1;

    for (uint16_t i = 0; i < count; i++) {
        if (jobOffers[i].teamNo == teamNumber)
            return 0;  /* found */
    }
    return 1;  /* not found */
}

/* ================================================================
 * GetJobOfferPtr  (SWOS2.s line 84231)
 *
 * Returns pointer to a job offer by index.
 * ================================================================ */
__attribute__((used, externally_visible))
JobOffer *_GetJobOfferPtr(uint16_t index)
{
    return &jobOffers[index];
}

/* ================================================================
 * AddJobOffer  (SWOS2.s line 84244)
 *
 * Adds a new job offer to the front of the newJobOffers queue.
 * Shifts existing entries right by one position. Max 5 offers.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _AddJobOffer(uint16_t teamNumber, uint32_t initialFunds)
{
    if (numNewJobOffers >= 5)
        return;

    uint16_t count = numNewJobOffers;

    /* Shift existing entries right by 26 bytes (word-at-a-time, backwards) */
    if (count > 0) {
        volatile uint16_t *base = (volatile uint16_t *)newJobOffers;
        uint16_t totalWords = count * (sizeof(JobOffer) / 2);
        for (int16_t i = (int16_t)(totalWords - 1); i >= 0; i--) {
            base[i + (sizeof(JobOffer) / 2)] = base[i];
        }
    }

    /* Write new entry at position 0 */
    newJobOffers[0].teamNo = teamNumber;
    newJobOffers[0].amountOffered = initialFunds;
    *(uint8_t *)newJobOffers[0].teamName = 0xFF;
    numNewJobOffers++;
}

/* ================================================================
 * RemoveJobOffer  (SWOS2.s line 84271)
 *
 * Removes a job offer from the newJobOffers queue by team number.
 * Shifts following entries left to fill the gap.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _RemoveJobOffer(uint16_t teamNumber)
{
    uint16_t count = numNewJobOffers;
    if (count == 0)
        return;

    for (uint16_t i = 0; i < count; i++) {
        if (newJobOffers[i].teamNo == teamNumber) {
            numNewJobOffers--;
            uint16_t remaining = count - 1 - i;
            if (remaining > 0) {
                volatile uint16_t *src = (volatile uint16_t *)&newJobOffers[i + 1];
                volatile uint16_t *dst = (volatile uint16_t *)&newJobOffers[i];
                uint16_t words = remaining * (sizeof(JobOffer) / 2);
                for (uint16_t w = 0; w < words; w++) {
                    *dst++ = *src++;
                }
            }
            return;
        }
    }
}

/* ================================================================
 * IsNewJobOffer  (SWOS2.s line 84301)
 *
 * Checks if a team number already exists in the newJobOffers queue.
 * Returns: 0 if found (Z set in asm), 1 if not found (Z clear).
 * ================================================================ */
__attribute__((used, externally_visible))
int16_t _IsNewJobOffer(uint16_t teamNumber)
{
    uint16_t count = numNewJobOffers;
    if (count == 0)
        return 1;

    for (uint16_t i = 0; i < count; i++) {
        if (newJobOffers[i].teamNo == teamNumber)
            return 0;  /* found */
    }
    return 1;  /* not found */
}

/* ================================================================
 * GetNewJobOfferPtr  (SWOS2.s line 84318)
 *
 * Returns pointer to a new job offer by index.
 * ================================================================ */
__attribute__((used, externally_visible))
JobOffer *_GetNewJobOfferPtr(uint16_t index)
{
    return &newJobOffers[index];
}

/* ================================================================
 * JobOffers_RemoveOffer  (SWOS2.s line 84180)
 *
 * Removes a job offer from the jobOffers (old offers) queue by
 * team number. Finds the entry, shifts following entries left by
 * 26 bytes (word-at-a-time), decrements count.
 * ================================================================ */
__attribute__((used, externally_visible, optimize("no-tree-loop-distribute-patterns")))
void _JobOffers_RemoveOffer(uint16_t teamNumber)
{
    uint16_t count = numJobOffers;
    if (count == 0)
        return;

    for (uint16_t i = 0; i < count; i++) {
        if (jobOffers[i].teamNo == teamNumber) {
            numJobOffers--;
            /* Shift remaining entries left by 26 bytes (word-at-a-time) */
            uint16_t wordsToShift = (count - 1 - i) * (sizeof(JobOffer) / 2);
            volatile uint16_t *dst = (volatile uint16_t *)&jobOffers[i];
            for (uint16_t j = 0; j < wordsToShift; j++) {
                dst[j] = dst[j + (sizeof(JobOffer) / 2)];
            }
            return;
        }
    }
}

/* ================================================================
 * JobOffers_MaybeExpireSomeNewOffers  (SWOS2.s line 84081)
 *
 * For each new job offer, there is a 1/32 chance (Rand2 < 0x20)
 * of expiring it by clearing its amountOffered field.
 * ================================================================ */
__attribute__((used, externally_visible))
void _JobOffers_MaybeExpireSomeNewOffers(void)
{
    uint16_t count = numNewJobOffers;
    if (count == 0)
        return;

    for (int16_t i = (int16_t)(count - 1); i >= 0; i--) {
        if (_Rand2() < 0x20) {
            newJobOffers[i].amountOffered = 0;
        }
    }
}

/* ================================================================
 * JobOffers_FillMissingNewOfferTeamNames  (SWOS2.s line 84099)
 *
 * For each new job offer whose team name starts with 0xFF (unset),
 * looks up the team via GetTeamPointer and copies the team name.
 * ================================================================ */
__attribute__((used, externally_visible))
void _JobOffers_FillMissingNewOfferTeamNames(void)
{
    uint16_t count = numNewJobOffers;
    if (count == 0)
        return;

    for (int16_t i = (int16_t)(count - 1); i >= 0; i--) {
        JobOffer *offer = &newJobOffers[i];
        if (*(uint8_t *)offer->teamName != 0xFF)
            continue;

        /* Look up team pointer and copy name */
        TeamFileEx *teamPtr = _GetTeamPointer(offer->teamNo);
        _StringCopy((uint8_t *)teamPtr->team.teamName, (uint8_t *)offer->teamName);
    }
}

/* ================================================================
 * JobOffers_RemoveExpiredOffers  (SWOS2.s line 84121)
 *
 * Removes all expired offers (amountOffered == 0) from both the
 * jobOffers and newJobOffers queues.
 * ================================================================ */
__attribute__((used, externally_visible))
void _JobOffers_RemoveExpiredOffers(void)
{
    /* Remove expired from old offers (jobOffers) */
    for (int16_t i = (int16_t)(numJobOffers - 1); i >= 0; i--) {
        if (jobOffers[i].amountOffered == 0) {
            _JobOffers_RemoveOffer(jobOffers[i].teamNo);
        }
    }

    /* Remove expired from new offers (newJobOffers) */
    for (int16_t i = (int16_t)(numNewJobOffers - 1); i >= 0; i--) {
        if (newJobOffers[i].amountOffered == 0) {
            _RemoveJobOffer(newJobOffers[i].teamNo);
        }
    }
}

/* ================================================================
 * JobOffers_ConvertNewOfferToOffer  (SWOS2.s line 84153)
 *
 * Converts a new job offer to an old (accepted) job offer.
 * Copies team name to fundsBuffer, removes from newJobOffers,
 * then appends to jobOffers with the team name from fundsBuffer.
 * ================================================================ */
__attribute__((used, externally_visible))
void _JobOffers_ConvertNewOfferToOffer(uint16_t teamNumber, uint32_t initialFunds)
{
    /* Find the new offer entry and copy its team name to fundsBuffer */
    uint16_t count = numNewJobOffers;
    for (uint16_t i = 0; i < count; i++) {
        if (newJobOffers[i].teamNo == teamNumber) {
            _StringCopy((uint8_t *)newJobOffers[i].teamName, fundsBuffer);
            break;
        }
    }

    /* Remove from new offers */
    _RemoveJobOffer(teamNumber);

    /* Add to old offers (max 5) */
    if (numJobOffers >= 5)
        return;

    JobOffer *dest = &jobOffers[numJobOffers];
    dest->teamNo = teamNumber;
    dest->amountOffered = initialFunds;
    _StringCopy(fundsBuffer, (uint8_t *)dest->teamName);
    numJobOffers++;
}

/* ================================================================
 * Chunk 99: Job Offer Menu Functions
 * ================================================================ */

/* Helper: format string into menu entry's text buffer */
static inline void printToEntry(uint16_t entryIdx, const uint8_t *fmt, const uint8_t *a, const uint8_t *b) {
    MenuEntry *e = &menuEntries[entryIdx];
    uint8_t *out = (uint8_t *)e->fg;
    _PrintFormatted(fmt, out, 0,0,0,0,0,0,0, a, b, 0);
}

/* ================================================================
 * GotJobOfferInit  (SWOS2.s line 82568)
 * ================================================================ */
__attribute__((used, externally_visible))
void _GotJobOfferInit(void) {
    teamNoOfferingJob = teamNoOfferAccepted;
    jobOfferFunds = (int32_t)teamFundsAccepted;

    if (teamNoOfferAccepted == 0xFFFF) {
        JobOffer *offer;
        if (jobOfferOrdinal >= 0) {
            offer = _GetNewJobOfferPtr((uint16_t)jobOfferOrdinal);
        } else {
            offer = _GetJobOfferPtr((uint16_t)oldJobOfferOrdinal);
        }
        teamNoOfferingJob = offer->teamNo;
        jobOfferFunds = (int32_t)offer->amountOffered;
        if (jobOfferOrdinal < 0) {
            _SetCurrentEntry(11);
        }
    } else {
        _SetCurrentEntry(11);
    }
    /* bra.w *+4 in asm = fallthrough to GotJobOfferFinish */
    _GotJobOfferFinish();
}

/* ================================================================
 * GotJobOfferFinish  (SWOS2.s line 82593)
 *
 * Sets up the job offer menu display text based on whether this is
 * a new offer (teamNoOfferAccepted == -1) or an accepted offer.
 * ================================================================ */
__attribute__((used, externally_visible))
void _GotJobOfferFinish(void) {
    /* If sacked and only 1 total offer, change exit to "RETIRE" with red color */
    if (youreSacked) {
        if ((uint16_t)(numNewJobOffers + numJobOffers) == 1) {
            MenuEntry *exitEntry = &menuEntries[8];
            exitEntry->fg = (uint32_t)aRetire;
            setBgColor(exitEntry, (bgColor(exitEntry) & 0xFFF0) | 10); /* RED_10 */
        }
    }

    /* International job offer adjustments */
    if (jobOfferFunds == -1) {
        MenuEntry *e0 = &menuEntries[0];
        e0->fg = (uint32_t)aInternationalJ;
        e0->x -= 30;
        e0->dwWidth += 60;
        menuEntries[8].x += 65;
        menuEntries[13].x -= 65;
        menuEntries[9].isInvisible = 1;
        menuEntries[12].isInvisible = 1;
    }

    if (teamNoOfferAccepted != 0xFFFF) {
        /* ---- Accepted job path ---- */
        goto acceptedJobPath;
    }

    /* ---- New/existing offer path ---- */
    /* loc_14A400 */
    if (jobOfferOrdinal >= 0) {
        menuEntries[11].isInvisible = 1;
    } else {
        menuEntries[10].isInvisible = 1;
    }

    if (!jobOffersFromRetired) {
        MenuEntry *acceptBtn = &menuEntries[13];
        acceptBtn->isDisabled = 1;
        setBgColor(acceptBtn, (bgColor(acceptBtn) & 0xFFF0) | 9); /* PINK_TO_BROWN_9 */
    }

    /* Set up team pointer and format strings */
    teamOfferingJobPtr = (uint32_t)_GetTeamPointer(teamNoOfferingJob);

    /* Format "DEAR %a %b" with manager sex and surname */
    {
        uint8_t *out = (uint8_t *)menuEntries[1].fg;
        _PrintFormatted(aDearAB, out, 0,0,0,0,0,0,0, managerSexBuffer, managerSurnameBuffer, 0);
    }

    if (jobOfferFunds != -1) {
        /* Domestic club offer */
        uint8_t *teamName = ((uint8_t *)teamOfferingJobPtr) + 5;

        {
            uint8_t *out = (uint8_t *)menuEntries[2].fg;
            _PrintFormatted(&aWeHereAtAWould[1], out, 0,0,0,0,0,0,0, teamName, 0, 0);
        }
        printToEntry(3, aCoachingJobAtO, 0, 0);

        _Int2Pounds((uint32_t)jobOfferFunds, fundsBuffer);

        {
            uint8_t *out = (uint8_t *)menuEntries[4].fg;
            _PrintFormatted(aWeCanOfferYouA, out, 0,0,0,0,0,0,0, fundsBuffer, 0, 0);
        }
        {
            uint8_t *out = (uint8_t *)menuEntries[5].fg;
            _PrintFormatted(aWeHopeToBeAble, out, 0,0,0,0,0,0,0, fundsBuffer, 0, 0);
        }
        {
            uint8_t *out = (uint8_t *)menuEntries[6].fg;
            _PrintFormatted(aInTheRegionOfA, out, 0,0,0,0,0,0,0, fundsBuffer, 0, 0);
        }
        {
            uint8_t *out = (uint8_t *)menuEntries[7].fg;
            _PrintFormatted(aWeLookForwardT_1, out, 0,0,0,0,0,0,0, fundsBuffer, 0, 0);
        }
    } else {
        /* International offer */
        MenuEntry *e0xA8 = &menuEntries[3];
        int16_t newY = e0xA8->y + 10;
        menuEntries[4].y = newY;

        uint8_t *teamName = ((uint8_t *)teamOfferingJobPtr) + 5;

        {
            uint8_t *out = (uint8_t *)menuEntries[2].fg;
            _PrintFormatted(aTheMembersOfTh, out, 0,0,0,0,0,0,0, teamName, 0, 0);
        }
        printToEntry(3, aOfAWouldLikeTo, teamName, 0);
        printToEntry(4, aAsCoachOfTheNa, 0, 0);
        printToEntry(5, aWeCanPromiseYo, 0, 0);
        printToEntry(6, aAnAnnuallyRevi, 0, 0);
        printToEntry(7, aWeLookForwardT, 0, 0);
    }
    return;

acceptedJobPath:
    /* loc_14A5D0: team already accepted */
    menuEntries[0].fg = (uint32_t)aNewJob;
    {
        int16_t newY = menuEntries[3].y + 10;
        menuEntries[4].y = newY;
    }
    menuEntries[10].isInvisible = 1;
    menuEntries[13].isInvisible = 1;
    menuEntries[8].isInvisible = 1;

    teamOfferingJobPtr = (uint32_t)_GetTeamPointer(teamNoOfferAccepted);

    {
        uint8_t *out = (uint8_t *)menuEntries[1].fg;
        _PrintFormatted(aDearAB, out, 0,0,0,0,0,0,0, managerSexBuffer, managerSurnameBuffer, 0);
    }

    if (jobOfferFunds != -1) {
        /* Domestic accepted */
        uint8_t *teamName = ((uint8_t *)teamOfferingJobPtr) + 5;

        {
            uint8_t *out = (uint8_t *)menuEntries[2].fg;
            _PrintFormatted(aWeHereAtAAreDe, out, 0,0,0,0,0,0,0, teamName, 0, 0);
        }
        printToEntry(3, aHaveAcceptedTh, 0, 0);

        _Int2Pounds(teamFundsAccepted, fundsBuffer);

        printToEntry(4, aFromTheStartOf_0, fundsBuffer, 0);
        {
            uint8_t *out = (uint8_t *)menuEntries[5].fg;
            _PrintFormatted(aWeHopeToBeAble_0, out, 0,0,0,0,0,0,0, fundsBuffer, 0, 0);
        }
        {
            uint8_t *out = (uint8_t *)menuEntries[6].fg;
            _PrintFormatted(aInTheRegionOfA_0, out, 0,0,0,0,0,0,0, fundsBuffer, 0, 0);
        }
        printToEntry(7, aAndWeLookForwa, fundsBuffer, 0);
    } else {
        /* International accepted */
        {
            int16_t newY = menuEntries[4].y + 10;
            menuEntries[5].y = newY;
        }
        menuEntries[7].y -= 15;

        uint8_t *teamName = ((uint8_t *)teamOfferingJobPtr) + 5;

        {
            uint8_t *out = (uint8_t *)menuEntries[2].fg;
            _PrintFormatted(aTheMembersOfTh_0, out, 0,0,0,0,0,0,0, teamName, 0, 0);
        }
        printToEntry(3, aOfAAreDelighte, teamName, 0);
        printToEntry(4, aAcceptedThePos, 0, 0);
        printToEntry(5, aFromTheStartOf, 0, 0);
        {
            uint8_t *out = (uint8_t *)menuEntries[6].fg;
            _PrintFormatted(aWeLookForwardT_0, out, 0,0,0,0,0,0,0, 0, 0, 0);
        }
        {
            /* asm uses (aWeLookForwardT_0+1) which skips the leading \x00 byte */
            uint8_t *out = (uint8_t *)menuEntries[7].fg;
            _PrintFormatted(&aWeLookForwardT_0[1], out, 0,0,0,0,0,0,0, 0, 0, 0);
        }
    }
}

/* ================================================================
 * AcceptJobOffer  (SWOS2.s line 82822)
 * ================================================================ */
__attribute__((used, externally_visible))
void _AcceptJobOffer(void) {
    JobOffer *offer;
    if (jobOfferOrdinal >= 0) {
        offer = _GetNewJobOfferPtr((uint16_t)jobOfferOrdinal);
    } else {
        offer = _GetJobOfferPtr((uint16_t)oldJobOfferOrdinal);
    }
    teamNoOfferAccepted = offer->teamNo;
    teamFundsAccepted = offer->amountOffered;
    numNewJobOffers = 0;
    numJobOffers = 0;

    /* GetCurrentSeasonPointer + check manager status */
    uint8_t *season = getCurrentSeasonPointer();
    if ((int8_t)season[SEASONINFO_MANAGERSTATUS] >= 0) {
        season[SEASONINFO_MANAGERSTATUS] = 1;
    }
    _SetExitMenuFlag();
}

/* ================================================================
 * j_SetExitMenuFlag_14  (SWOS2.s line 82844)
 * ================================================================ */
__attribute__((used, externally_visible))
void _j_SetExitMenuFlag_14(void) {
    _SetExitMenuFlag();
}

/* ================================================================
 * RejectJobOffer  (SWOS2.s line 82849)
 * ================================================================ */
__attribute__((used, externally_visible))
void _RejectJobOffer(void) {
    uint16_t teamNo = teamNoOfferingJob;
    if (jobOfferOrdinal < 0) {
        _JobOffers_RemoveOffer(teamNo);
    } else {
        _RemoveJobOffer(teamNo);
    }
    _SetExitMenuFlag();
}

/* ================================================================
 * ViewFutureTeam  (SWOS2.s line 82861)
 * ================================================================ */
__attribute__((used, externally_visible))
void _ViewFutureTeam(void) {
    viewFutureTeamModeFlag = 1;
    _ShowSquad((TeamFile *)teamOfferingJobPtr, aViewTeam, 0, 0, 1, -1);
    viewFutureTeamModeFlag = 0;
    _GotJobOfferFinish();
}

/* ================================================================
 * ViewFutureTeamLeague  (SWOS2.s line 82876)
 * ================================================================ */
__attribute__((used, externally_visible))
void _ViewFutureTeamLeague(void) {
    TeamFile *teamPtr = (TeamFile *)teamOfferingJobPtr;
    int16_t d7 = (int8_t)teamPtr->league;
    uint8_t countryIdx = teamPtr->countryNumber;
    uint32_t *compTable = (uint32_t *)competitionsTable;
    uint8_t *compEntry = (uint8_t *)compTable[countryIdx];
    uint8_t *comp = (uint8_t *)(*(uint32_t *)compEntry);

    highlightedLeagueTeamId = BE_READ_U16(teamPtr);
    _Career_ViewOrEditCompetitionSlot(comp, (uint16_t)d7);
    highlightedLeagueTeamId = 0xFFFF;
    _GotJobOfferFinish();
}

/* ================================================================
 * ConsiderJobOffer  (SWOS2.s line 82894)
 * ================================================================ */
__attribute__((used, externally_visible))
void _ConsiderJobOffer(void) {
    if (jobOfferOrdinal >= 0) {
        JobOffer *offer = _GetNewJobOfferPtr((uint16_t)jobOfferOrdinal);
        _JobOffers_ConvertNewOfferToOffer(offer->teamNo, offer->amountOffered);
    }
    _SetExitMenuFlag();
}

/* ================================================================
 * Chunk 116: JobOffers_UpdateMenuEntryLayout, JobOffersFinish,
 *            JobOffersCommon, OldJobOffersCommon, JobOffersExit
 * ================================================================ */

extern uint8_t gotJobOfferMenu[];

/* JobOffers_UpdateMenuEntryLayout — position old offer menu entries
 * based on number of new/old offers (SWOS2.s line 81335) */
__attribute__((used, externally_visible))
void _JobOffers_UpdateMenuEntryLayout(void)
{
    /* Hide entries 13,14 and 15,16 */
    menuEntries[13].isInvisible = 1;
    menuEntries[14].isInvisible = 1;
    menuEntries[15].isInvisible = 1;
    menuEntries[16].isInvisible = 1;

    /* Hide entry 6 (old offers header) */
    menuEntries[6].isInvisible = 1;

    if (numJobOffers == 0)
        return;

    /* Show entry 6 */
    menuEntries[6].isInvisible = 0;

    /* Calculate y position: start from entry 1's y, offset by new offers */
    uint16_t yPos = menuEntries[1].y;
    yPos += (uint16_t)(numNewJobOffers * 0xF);
    yPos += 0x1E;

    /* Entry 6: old offers header */
    menuEntries[6].y = yPos;
    yPos += 0x14;

    /* Entries 7-11: old offer lines (5 entries, each 0xF apart) */
    int16_t i;
    for (i = 0; i < 5; i++) {
        menuEntries[7 + i].y = yPos;
        yPos += 0xF;
    }
}

/* JobOffersFinish — update menu entry layout (SWOS2.s line 81275) */
__attribute__((used, externally_visible))
void _JobOffersFinish(void)
{
    _JobOffers_UpdateMenuEntryLayout();
}

/* JobOffersCommon — handle clicking on a new job offer entry (SWOS2.s line 81281) */
__attribute__((used, externally_visible))
void _JobOffersCommon(MenuEntry *entry)
{
    uint16_t baseOrdinal = menuEntries[1].ordinal;
    int16_t d7 = (int16_t)(entry->ordinal - baseOrdinal);
    jobOfferOrdinal = d7;
    oldJobOfferOrdinal = 0xFFFF;
    _ShowMenu(gotJobOfferMenu);

    if (teamNoOfferAccepted != 0xFFFF) {
        _SetExitMenuFlag();
        return;
    }
    if (numNewJobOffers != 0 || numJobOffers != 0) {
        _SetCurrentEntry(0xC);
        return;
    }
}

/* OldJobOffersCommon — handle clicking on an old job offer entry (SWOS2.s line 81305) */
__attribute__((used, externally_visible))
void _OldJobOffersCommon(MenuEntry *entry)
{
    uint16_t baseOrdinal = menuEntries[7].ordinal;
    int16_t d7 = (int16_t)(entry->ordinal - baseOrdinal);
    jobOfferOrdinal = 0xFFFF;
    oldJobOfferOrdinal = d7;
    _ShowMenu(gotJobOfferMenu);

    if (teamNoOfferAccepted != 0xFFFF) {
        _SetExitMenuFlag();
        return;
    }
    if (numNewJobOffers != 0 || numJobOffers != 0) {
        _SetCurrentEntry(0xC);
        return;
    }
}

/* JobOffersExit — remove expired offers and exit menu (SWOS2.s line 81328) */
__attribute__((used, externally_visible))
void _JobOffersExit(void)
{
    _JobOffers_RemoveExpiredOffers();
    _SetExitMenuFlag();
}

/* ================================================================
 * Chunk 169 — JobOffersInit, JobOffers_DrawOldOfferLine,
 *             JobOffersAfterDrawCommon
 * ================================================================ */

extern uint32_t irqCookieObfuscated;
extern uint8_t aInternationalJ_0[];  /* "INTERNATIONAL JOB OFFER FROM %a" */
extern uint8_t aJobOfferFromA[];     /* "JOB OFFER FROM %a" */
extern uint8_t aJobOfferFromAW[];    /* "JOB OFFER FROM %a WITHDRAWN" */

/* ================================================================
 * JobOffersInit  (SWOS2.s line 72959)
 *
 * Menu init callback for the job offers menu.
 * Does an anti-piracy cookie check (always passes in HDD version),
 * fills missing team names, resets selected ordinal, and falls
 * through to JobOffersFinish.
 * ================================================================ */
__attribute__((used, externally_visible))
void _JobOffersInit(void)
{
    /* Anti-piracy check: irqCookieObfuscated XOR 0xCFD1F80D must equal 0x17EA */
    uint32_t d0 = irqCookieObfuscated ^ 0xCFD1F80Du;
    if (d0 != 0x17EA)
        return;  /* In original asm this would crash; safe no-op here */

    _JobOffers_FillMissingNewOfferTeamNames();
    selectedOldJobOfferOrdinal = 0xFFFF;

    /* Fall through to JobOffersFinish */
    _JobOffersFinish();
}

/* ================================================================
 * JobOffers_DrawOldOfferLine  (SWOS2.s line 73003)
 *
 * After-draw callback for old job offer entries.
 * Calculates offer index from ordinal difference, formats offer
 * text, sets color highlighting for selected/withdrawn offers.
 *
 * in: a5 = current menu entry
 * ================================================================ */
__attribute__((used, externally_visible))
void _JobOffers_DrawOldOfferLine(void *entryPtr)
{
    MenuEntry *a5 = (MenuEntry *)entryPtr;
    a5->isDisabled = 1;

    /* d7 = a5->ordinal - entries[7].ordinal (entry 7 = 0x188/0x38) */
    int16_t d7 = (int16_t)a5->ordinal - (int16_t)menuEntries[7].ordinal;

    if ((uint16_t)d7 >= numJobOffers)
        return;

    a5->isDisabled = 0;

    /* Show entries 15 and 16 (asm offsets $348/$380) */
    menuEntries[15].isInvisible = 0;
    menuEntries[16].isInvisible = 0;

    /* d5 = highlight color (0 = normal, 0xD = selected) */
    uint16_t d5 = 0;
    if ((uint16_t)d7 == selectedOldJobOfferOrdinal)
        d5 = 0xD;

    /* Apply d5 to bg color bits (low 4) of entries 15 and 16 */
    {
        uint32_t bg15 = menuEntries[15].bg;
        menuEntries[15].bg = (bg15 & 0xFFF0FFFF) | ((uint32_t)d5 << 16);
    }
    {
        uint32_t bg16 = menuEntries[16].bg;
        menuEntries[16].bg = (bg16 & 0xFFF0FFFF) | ((uint32_t)d5 << 16);
    }

    /* Entry 15 (a3): set stringFlags base, y, number */
    MenuEntry *a3 = &menuEntries[15];
    uint16_t d3 = a3->stringFlags & 0xFFF0;
    a3->y = a5->y;
    *(uint16_t *)&a3->fg = (uint16_t)(d7 + 1);  /* ENTRY_NUMBER: write high word */

    /* Entry 16 (a4): set stringFlags base, y, get dest buffer */
    MenuEntry *a4 = &menuEntries[16];
    uint16_t d4 = a4->stringFlags & 0xFFF0;
    a4->y = a5->y;
    uint8_t *destBuf = (uint8_t *)a4->fg;

    /* Get offer and format text */
    JobOffer *offer = _GetJobOfferPtr((uint16_t)d7);
    uint8_t *teamName = offer->teamName;

    uint8_t *fmtStr;
    if (offer->amountOffered == 0xFFFFFFFF)
        fmtStr = aInternationalJ_0;  /* "INTERNATIONAL JOB OFFER FROM %a" */
    else
        fmtStr = aJobOfferFromA;     /* "JOB OFFER FROM %a" */

    /* If withdrawn (funds == 0), use withdrawn text and color 6 */
    if (offer->amountOffered == 0) {
        fmtStr = aJobOfferFromAW;    /* "JOB OFFER FROM %a WITHDRAWN" */
        d3 |= 6;
        d4 |= 6;
        a5->isDisabled = 1;
    }

    _PrintFormatted(fmtStr, destBuf, 0, 0, 0, 0, 0, 0, 0,
                    teamName, 0, 0);

    a3->stringFlags = d3;
    a4->stringFlags = d4;

    /* Draw entries 15-16 */
    _DrawMultipleItems(2, &menuEntries[15]);
}

/* ================================================================
 * JobOffersAfterDrawCommon  (SWOS2.s line 73070)
 *
 * After-draw callback for new job offer entries.
 * Similar to DrawOldOfferLine but operates on new offers queue
 * and uses entries 11-12.
 *
 * in: a5 = current menu entry
 * ================================================================ */
__attribute__((used, externally_visible))
void _JobOffersAfterDrawCommon(void *entryPtr)
{
    MenuEntry *a5 = (MenuEntry *)entryPtr;
    a5->isDisabled = 1;

    /* d7 = a5->ordinal - entries[1].ordinal (entry 1 = 0x38/0x38) */
    int16_t d7 = (int16_t)a5->ordinal - (int16_t)menuEntries[1].ordinal;

    if ((uint16_t)d7 >= numNewJobOffers)
        return;

    a5->isDisabled = 0;

    /* Show entries 13 and 14 (asm offsets $2D8/$310) */
    menuEntries[13].isInvisible = 0;
    menuEntries[14].isInvisible = 0;

    /* Entry 13 (a3): set stringFlags base, y, number */
    MenuEntry *a3 = &menuEntries[13];
    uint16_t d3 = a3->stringFlags & 0xFFF0;
    a3->y = a5->y;
    uint16_t numVal = (uint16_t)d7 + 1;
    *(uint16_t *)&a3->fg = numVal;  /* ENTRY_NUMBER: write high word */

    /* Entry 14 (a4): set stringFlags base, y, get dest buffer */
    MenuEntry *a4 = &menuEntries[14];
    uint16_t d4 = a4->stringFlags & 0xFFF0;
    a4->y = a5->y;
    uint8_t *destBuf = (uint8_t *)a4->fg;

    /* Get offer and format text */
    JobOffer *offer = _GetNewJobOfferPtr((uint16_t)d7);
    uint8_t *teamName = offer->teamName;

    uint8_t *fmtStr;
    if (offer->amountOffered == 0xFFFFFFFF)
        fmtStr = aInternationalJ_0;  /* "INTERNATIONAL JOB OFFER FROM %a" */
    else
        fmtStr = aJobOfferFromA;     /* "JOB OFFER FROM %a" */

    /* If withdrawn (funds == 0), use withdrawn text and color 6 */
    if (offer->amountOffered == 0) {
        fmtStr = aJobOfferFromAW;    /* "JOB OFFER FROM %a WITHDRAWN" */
        d3 |= 6;
        d4 |= 6;
        a5->isDisabled = 1;
    }

    _PrintFormatted(fmtStr, destBuf, 0, 0, 0, 0, 0, 0, 0,
                    teamName, 0, 0);

    a3->stringFlags = d3;
    a4->stringFlags = d4;

    /* Draw entries 13-14 */
    _DrawMultipleItems(2, &menuEntries[13]);
}

/* ================================================================
 * _Career_MaybeGenerateJobOffer  (SWOS2.s line 66143)
 *
 * Checks conditions and potentially generates a random job offer
 * from another team during career mode. Handles domestic teams,
 * foreign (TMD) teams, and national team offers.
 *
 * Asm register convention:
 *   in:  none (uses globals)
 *   out: none (may call AddJobOffer)
 * ================================================================ */
__attribute__((used, externally_visible))
void _Career_MaybeGenerateJobOffer(void)
{
    uint16_t foundTeamId = 0;

    if (teamNoOfferAccepted != 0xFFFF)
        return;
    if (careerSeasonsCompleted == 7)
        return;

    int16_t maxNewOffers = 5 - (int16_t)numJobOffers;
    if (maxNewOffers == 0)
        return;
    if ((int16_t)numNewJobOffers >= maxNewOffers)
        return;
    if (currentSeasonShowing == 0x13)
        return;

    if (careerJobOfferMode == 1)
        goto searchDomestic;
    if (careerJobOfferMode == 2)
        goto checkNational;
    if (youreSacked != 0)
        goto searchDomestic;

    /* Random phase check */
    {
        int8_t d0 = (int8_t)viewRandomPhase - careerJobOfferPhaseBias;
        int16_t d0w = (int16_t)d0;
        int32_t d0l = (int32_t)(d0w << 2);
        uint16_t divided = (uint16_t)((uint32_t)d0l / (uint32_t)wageScalingDivisor);
        if (divided != 3)
            return;
    }

checkNational:
    /* Try national team offer */
    if (sameNationalCallFlag == 0) {
        uint16_t natCall = lastNationalityCall;
        if (natCall != 0xFFFF) {
            if (careerJobOfferMode != 2) {
                if (boardConfidenceScore == 0)
                    goto noNational;
            }
            /* Check if already offered */
            if (_IsNewJobOffer(natCall) == 0)
                goto noNational;
            if (_IsOldJobOffer(natCall) == 0)
                goto noNational;
            if (careerJobOfferMode != 2) {
                int16_t conf = _GetBoardConfidenceScore();
                if (conf < 0xC8)
                    goto noNational;
            }
            /* Check if team code is valid for national selector */
            uint16_t natHigh = natCall >> 8;
            if (_IsTeamCodeOkForNationalSelector(natHigh) != 0) {
                /* Valid national offer */
                _AddJobOffer(natCall, (uint32_t)-1);
                return;
            }
        }
    }

noNational:
    if (careerJobOfferMode == 2)
        return;

    /* Random chance to generate offer */
    {
        uint16_t rnd = _Rand();
        if (rnd >= 42)
            return;
    }

searchDomestic:
    ;
    /* Calculate reputation range [d6..d7] */
    uint16_t repLow, repHigh;
    uint16_t repScore;

    if (youreSacked != 0 && isNationalSelector != 0) {
        repScore = (uint16_t)sackedReputationScore;
    } else if (isNationalSelector != 0) {
        repScore = (uint16_t)nationalSelectorReputationPoints;
    } else {
        /* Calculate from major trophy bonus and league position */
        uint16_t mtb = (uint16_t)(uint8_t)majorTrophyBonusValue;
        uint16_t low75 = (uint16_t)((uint16_t)(mtb * 0x4B) / 100);
        uint16_t high150 = (uint16_t)((uint16_t)(mtb * 0x96) / 100);

        /* Find team position in DIY table */
        int16_t pos = _FindTeamInDIY(selTeamNumber, (DIYCompetition *)&competitionFileBuffer);

        uint16_t posPct = (uint16_t)((uint16_t)((uint16_t)pos * 100) / (numDiyTeams - 1));
        uint16_t invPct = 100 - posPct;

        uint16_t range = high150 - low75;
        uint16_t adj = (uint16_t)((uint16_t)(range * invPct) / 100);
        repScore = adj + low75;

        if (isNationalSelector == 0) {
            sackedReputationScore = (int16_t)repScore;
            nationalSelectorReputationPoints = (int16_t)repScore;
        }
    }

    if (youreSacked == 0) {
        repHigh = repScore;
        repLow = (uint16_t)((uint16_t)(repHigh * 0x4B) / 100);
    } else {
        repHigh = repScore;
        repLow = (uint16_t)((uint16_t)(repHigh * 0) / 100);  /* mulu #0 */
    }

    if (careerJobOfferMode == 1)
        goto searchNational;

    if (youreSacked != 0 && isNationalSelector != 0)
        goto searchSacked;

    /* Try domestic (selected teams) first */
    {
        uint16_t rnd = _Rand();
        if (rnd < 0x15) {
            /* Try national team selector offer */
            if (isNationalSelector != 0)
                goto searchNational;

            /* Random chance for national team if player qualifies */
            rnd = _Rand();
            if (rnd < 0x55) {
                goto searchForeign;
            }
            if (_IsPlayerOkForNationalSelector() == 0)
                goto searchNational;
            goto searchForeign;
        }

        /* Search among selected teams (home division) */
        if (isNationalSelector != 0)
            return;

        uint16_t selTeam = selTeamNumber;
        uint8_t *a0;

        for (int16_t tries = 99; tries >= 0; tries--) {
            rnd = _Rand();
            uint16_t idx = (uint16_t)((uint16_t)(rnd * numSelectedTeams) >> 8);
            a0 = &selTeamsPtr + (uint32_t)idx * 0x2AC;

            /* Skip custom teams (type 4) */
            TeamFile *a0tf = (TeamFile *)a0;
            if (a0tf->league == 4)
                continue;

            /* Must be same division (country). selTeamNumber MSB = country.
             * Pre-fix `*(uint8_t *)&selTeamNumber` read LE byte[0] = ordinal,
             * which rarely matched any team's country byte — so the 99-try
             * home-country job-offer search effectively never found a
             * candidate, and offers only ever showed up via the
             * `searchForeign` fallback that triggers when youreSacked. */
            if ((uint8_t)(selTeamNumber >> 8) != a0tf->countryNumber)
                continue;

            /* Asm: move.w (a0),d3 — reads packed team number at offset 0 */
            uint16_t teamId = PACK_BE_U16(a0tf->countryNumber, a0tf->teamOrdinal);
            if (teamId == selTeam)
                continue;

            uint16_t avgPrice = _GetAveragePlayerPrice((TeamFileEx *)a0);
            if (avgPrice < repLow || avgPrice > repHigh)
                continue;

            if (_IsNewJobOffer(teamId) == 0)
                continue;
            if (_IsOldJobOffer(teamId) == 0)
                continue;

            /* Check division index match */
            uint16_t divIdx = currentDivisionIndex;
            if ((uint8_t)divIdx != a0tf->league) {
                foundTeamId = teamId;
                goto foundWithBalance;
            }

            /* Same division — check league position */
            _FindTeamInDIY(teamId, (DIYCompetition *)&competitionFileBuffer);
            uint16_t minDiv = diyTeamMinDivisionIndex + diyTeamMaxDivisionOffset;
            if ((uint16_t)divIdx < minDiv)
                continue;

            foundTeamId = teamId;
            goto foundWithBalance;
        }

        if (youreSacked != 0)
            goto searchForeign;
        return;
    }

searchForeign:
    /* Search among TMD (foreign) teams */
    {
        uint16_t selTeam = selTeamNumber;
        uint8_t *base = &tmdFileBuffer;

        for (int16_t tries = 99; tries >= 0; tries--) {
            uint16_t rnd = _Rand();
            uint16_t idx = (uint16_t)((uint16_t)(rnd * careerFileBuffer) >> 8);
            uint8_t *a0 = base + (uint32_t)idx * 0x2AC;

            TeamFile *a0tf = (TeamFile *)a0;
            if (a0tf->league == 4)
                continue;
            /* Foreign job-offer search — skip teams from the user's own
             * country. MSB of selTeamNumber is the country byte; the LE
             * byte-read pre-fix used the ordinal, so the skip rarely
             * fired and same-country teams accidentally counted as
             * foreign candidates. */
            if ((uint8_t)(selTeamNumber >> 8) == a0tf->countryNumber)
                continue;

            /* Asm: move.w (a0),d3 — reads packed team number at offset 0 */
            uint16_t teamId = PACK_BE_U16(a0tf->countryNumber, a0tf->teamOrdinal);
            if (teamId == selTeam)
                continue;

            uint16_t avgPrice = _GetAveragePlayerPrice((TeamFileEx *)a0);
            if (avgPrice < repLow || avgPrice > repHigh)
                continue;

            if (_IsNewJobOffer(teamId) == 0)
                continue;
            if (_IsOldJobOffer(teamId) == 0)
                continue;

            foundTeamId = teamId;
            goto foundWithBalance;
        }
        return;
    }

searchNational:
    /* Search among national teams */
    {
        for (;;) {
            for (int16_t tries = 99; tries >= 0; tries--) {
                uint16_t rnd = _Rand();
                uint32_t idx32 = (uint32_t)rnd * (uint32_t)nat_teams_rand_limit1;
                uint16_t idx = (uint16_t)(idx32 >> 8);
                uint8_t *entry = nationalTeams + (uint32_t)idx * 6;

                /* entry[0..1] = {countryNumber, teamOrdinal} byte pair.
                 * teamId is compared against selTeamNumber (BE-packed)
                 * below, so use PACK_BE_U16 to read the matching form
                 * on both hosts. */
                uint16_t teamId = PACK_BE_U16(entry[0], entry[1]);
                uint16_t skill = *(uint16_t *)(entry + 4);

                if (skill < repLow || skill > repHigh)
                    continue;
                if (teamId == selTeamNumber)
                    continue;
                if (teamId == lastNationalityCall)
                    continue;

                if (_IsTeamCodeOkForNationalSelector(entry[0]) == 0)
                    continue;
                if (_IsNewJobOffer(teamId) == 0)
                    continue;
                if (_IsOldJobOffer(teamId) == 0)
                    continue;

                /* Found a valid national team offer */
                _AddJobOffer(teamId, (uint32_t)-1);
                return;
            }

            /* Didn't find one — widen range and retry */
            if (repLow == 0)
                return;
            repLow -= 8;
            if ((int16_t)repLow < 0)
                repLow = 0;
        }
    }

searchSacked:
    /* Sacked + national selector: search teamFileBuffer */
    _LoadTeamFile(nationalTeamFileCode);
    repLow = repHigh >> 1;

    for (;;) {
        uint16_t selTeam = selTeamNumber;

        for (int16_t tries = 99; tries >= 0; tries--) {
            uint16_t rnd = _Rand();
            uint16_t numTeams = *(uint16_t *)&teamFileBuffer;
            uint16_t idx = (uint16_t)((uint16_t)(rnd * numTeams) >> 8);
            uint8_t *a0 = (&teamFileBuffer + 2) + (uint32_t)idx * 0x2AC;

            TeamFile *a0tf2 = (TeamFile *)a0;
            if (a0tf2->league == 4)
                continue;

            /* Asm: move.w (a0),d3 — reads packed team number at offset 0 */
            uint16_t teamId = PACK_BE_U16(a0tf2->countryNumber, a0tf2->teamOrdinal);

            uint16_t avgPrice = _GetAveragePlayerPrice((TeamFileEx *)a0);
            if (avgPrice < repLow || avgPrice > repHigh)
                continue;

            if (_IsNewJobOffer(teamId) == 0)
                continue;
            if (_IsOldJobOffer(teamId) == 0)
                continue;

            foundTeamId = teamId;
            goto foundWithBalance;
        }

        if (repLow == 0)
            return;
        repLow -= 8;
        if ((int16_t)repLow < 0)
            repLow = 0;
    }

foundWithBalance:
    {
        uint32_t funds = _CalculateStartingBalance(foundTeamId);
        _AddJobOffer(foundTeamId, funds);
    }
}
