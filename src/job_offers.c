/* job_offers.c — Job offer queue management and menu functions.
 *
 * Chunk 79: 6 exported functions ported from SWOS2.s lines 84210-84322.
 * Chunk 81: 5 more exported functions from SWOS2.s lines 84081-84200.
 * Chunk 99: 8 job offer menu functions from SWOS2.s lines 82566-82903.
 */

#include "job_offers.h"
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
extern uint8_t *_GetTeamPointer(uint16_t teamId);
extern void _PrintFormatted(const uint8_t *fmt, uint8_t *out,
                     uint16_t n0, uint16_t n1, uint16_t n2, uint16_t n3,
                     uint16_t n4, uint16_t n5, uint16_t n6,
                     const uint8_t *strA, const uint8_t *strB, const uint8_t *strC);

/* ---- C-callable ASM bridges ---- */
extern uint8_t *_CCallGetTeamPointer(uint16_t teamNumber);
extern void _CInt2Pounds(uint32_t value, uint8_t *dst);
extern void _CShowSquad(uint8_t *team, uint8_t *title, uint32_t maskLo, uint32_t maskHi, int16_t arg2, int16_t arg3);
extern void _CCareer_ViewOrEditCompetitionSlot(uint8_t *comp, uint16_t sel);

/* Helper: menu entry from byte offset */
#define ME(off) menuEntries[(off) / 56]
#define MEP(off) (&menuEntries[(off) / 56])

/* Helper: access bg as the high 16 bits (big-endian BACKANDFRAMECOLOR) */
static inline uint16_t bgColor(MenuEntry *e) { return *(uint16_t *)&e->bg; }
static inline void setBgColor(MenuEntry *e, uint16_t v) { *(uint16_t *)&e->bg = v; }

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
        uint8_t *teamPtr = _CCallGetTeamPointer(offer->teamNo);
        _StringCopy((uint8_t *)((TeamFile *)teamPtr)->teamName, (uint8_t *)offer->teamName);
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
static inline void printToEntry(uint16_t entryOff, const uint8_t *fmt, const uint8_t *a, const uint8_t *b) {
    MenuEntry *e = MEP(entryOff);
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
            MenuEntry *exitEntry = MEP(0x1C0);
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
        MEP(0x1C0)->x += 65;
        MEP(0x2D8)->x -= 65;
        ME(0x1F8).isInvisible = 1;
        ME(0x2A0).isInvisible = 1;
    }

    if (teamNoOfferAccepted != 0xFFFF) {
        /* ---- Accepted job path ---- */
        goto acceptedJobPath;
    }

    /* ---- New/existing offer path ---- */
    /* loc_14A400 */
    if (jobOfferOrdinal >= 0) {
        ME(0x268).isInvisible = 1;
    } else {
        ME(0x230).isInvisible = 1;
    }

    if (!jobOffersFromRetired) {
        MenuEntry *acceptBtn = MEP(0x2D8);
        acceptBtn->isDisabled = 1;
        setBgColor(acceptBtn, (bgColor(acceptBtn) & 0xFFF0) | 9); /* PINK_TO_BROWN_9 */
    }

    /* Set up team pointer and format strings */
    teamOfferingJobPtr = (uint32_t)_GetTeamPointer(teamNoOfferingJob);

    /* Format "DEAR %a %b" with manager sex and surname */
    {
        uint8_t *out = (uint8_t *)MEP(0x38)->fg;
        _PrintFormatted(aDearAB, out, 0,0,0,0,0,0,0, managerSexBuffer, managerSurnameBuffer, 0);
    }

    if (jobOfferFunds != -1) {
        /* Domestic club offer */
        uint8_t *teamName = ((uint8_t *)teamOfferingJobPtr) + 5;

        {
            uint8_t *out = (uint8_t *)MEP(0x70)->fg;
            _PrintFormatted(&aWeHereAtAWould[1], out, 0,0,0,0,0,0,0, teamName, 0, 0);
        }
        printToEntry(0xA8, aCoachingJobAtO, 0, 0);

        _CInt2Pounds((uint32_t)jobOfferFunds, fundsBuffer);

        {
            uint8_t *out = (uint8_t *)MEP(0xE0)->fg;
            _PrintFormatted(aWeCanOfferYouA, out, 0,0,0,0,0,0,0, fundsBuffer, 0, 0);
        }
        {
            uint8_t *out = (uint8_t *)MEP(0x118)->fg;
            _PrintFormatted(aWeHopeToBeAble, out, 0,0,0,0,0,0,0, fundsBuffer, 0, 0);
        }
        {
            uint8_t *out = (uint8_t *)MEP(0x150)->fg;
            _PrintFormatted(aInTheRegionOfA, out, 0,0,0,0,0,0,0, fundsBuffer, 0, 0);
        }
        {
            uint8_t *out = (uint8_t *)MEP(0x188)->fg;
            _PrintFormatted(aWeLookForwardT_1, out, 0,0,0,0,0,0,0, fundsBuffer, 0, 0);
        }
    } else {
        /* International offer */
        MenuEntry *e0xA8 = MEP(0xA8);
        int16_t newY = e0xA8->y + 10;
        MEP(0xE0)->y = newY;

        uint8_t *teamName = ((uint8_t *)teamOfferingJobPtr) + 5;

        {
            uint8_t *out = (uint8_t *)MEP(0x70)->fg;
            _PrintFormatted(aTheMembersOfTh, out, 0,0,0,0,0,0,0, teamName, 0, 0);
        }
        printToEntry(0xA8, aOfAWouldLikeTo, teamName, 0);
        printToEntry(0xE0, aAsCoachOfTheNa, 0, 0);
        printToEntry(0x118, aWeCanPromiseYo, 0, 0);
        printToEntry(0x150, aAnAnnuallyRevi, 0, 0);
        printToEntry(0x188, aWeLookForwardT, 0, 0);
    }
    return;

acceptedJobPath:
    /* loc_14A5D0: team already accepted */
    menuEntries[0].fg = (uint32_t)aNewJob;
    {
        int16_t newY = MEP(0xA8)->y + 10;
        MEP(0xE0)->y = newY;
    }
    ME(0x230).isInvisible = 1;
    ME(0x2D8).isInvisible = 1;
    ME(0x1C0).isInvisible = 1;

    teamOfferingJobPtr = (uint32_t)_GetTeamPointer(teamNoOfferAccepted);

    {
        uint8_t *out = (uint8_t *)MEP(0x38)->fg;
        _PrintFormatted(aDearAB, out, 0,0,0,0,0,0,0, managerSexBuffer, managerSurnameBuffer, 0);
    }

    if (jobOfferFunds != -1) {
        /* Domestic accepted */
        uint8_t *teamName = ((uint8_t *)teamOfferingJobPtr) + 5;

        {
            uint8_t *out = (uint8_t *)MEP(0x70)->fg;
            _PrintFormatted(aWeHereAtAAreDe, out, 0,0,0,0,0,0,0, teamName, 0, 0);
        }
        printToEntry(0xA8, aHaveAcceptedTh, 0, 0);

        _CInt2Pounds(teamFundsAccepted, fundsBuffer);

        printToEntry(0xE0, aFromTheStartOf_0, fundsBuffer, 0);
        {
            uint8_t *out = (uint8_t *)MEP(0x118)->fg;
            _PrintFormatted(aWeHopeToBeAble_0, out, 0,0,0,0,0,0,0, fundsBuffer, 0, 0);
        }
        {
            uint8_t *out = (uint8_t *)MEP(0x150)->fg;
            _PrintFormatted(aInTheRegionOfA_0, out, 0,0,0,0,0,0,0, fundsBuffer, 0, 0);
        }
        printToEntry(0x188, aAndWeLookForwa, fundsBuffer, 0);
    } else {
        /* International accepted */
        {
            int16_t newY = MEP(0xE0)->y + 10;
            MEP(0x118)->y = newY;
        }
        MEP(0x188)->y -= 15;

        uint8_t *teamName = ((uint8_t *)teamOfferingJobPtr) + 5;

        {
            uint8_t *out = (uint8_t *)MEP(0x70)->fg;
            _PrintFormatted(aTheMembersOfTh_0, out, 0,0,0,0,0,0,0, teamName, 0, 0);
        }
        printToEntry(0xA8, aOfAAreDelighte, teamName, 0);
        printToEntry(0xE0, aAcceptedThePos, 0, 0);
        printToEntry(0x118, aFromTheStartOf, 0, 0);
        {
            uint8_t *out = (uint8_t *)MEP(0x150)->fg;
            _PrintFormatted(aWeLookForwardT_0, out, 0,0,0,0,0,0,0, 0, 0, 0);
        }
        {
            /* asm uses (aWeLookForwardT_0+1) which skips the leading \x00 byte */
            uint8_t *out = (uint8_t *)MEP(0x188)->fg;
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
    uint8_t *team = (uint8_t *)teamOfferingJobPtr;
    viewFutureTeamModeFlag = 1;
    _CShowSquad(team, aViewTeam, 0, 0, 1, -1);
    viewFutureTeamModeFlag = 0;
    _GotJobOfferFinish();
}

/* ================================================================
 * ViewFutureTeamLeague  (SWOS2.s line 82876)
 * ================================================================ */
__attribute__((used, externally_visible))
void _ViewFutureTeamLeague(void) {
    uint8_t *teamPtr = (uint8_t *)teamOfferingJobPtr;
    int16_t d7 = (int8_t)teamPtr[0x19];
    uint8_t countryIdx = teamPtr[0] & 0xFF;
    uint32_t *compTable = (uint32_t *)competitionsTable;
    uint8_t *compEntry = (uint8_t *)compTable[countryIdx];
    uint8_t *comp = (uint8_t *)(*(uint32_t *)compEntry);

    highlightedLeagueTeamId = *(uint16_t *)teamPtr;
    _CCareer_ViewOrEditCompetitionSlot(comp, (uint16_t)d7);
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
