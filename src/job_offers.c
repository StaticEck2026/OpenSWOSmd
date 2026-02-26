/* job_offers.c — Job offer queue management.
 *
 * Chunk 79: 6 exported functions ported from SWOS2.s lines 84210-84322.
 * Chunk 81: 5 more exported functions from SWOS2.s lines 84081-84200.
 */

#include "job_offers.h"
#include "game_types.h"
#include "random.h"
#include "string_utils.h"

/* ---- ASM externs ---- */
extern uint16_t numJobOffers;
extern JobOffer jobOffers[];         /* 5-entry job offers queue (in SWOS2.s BSS) */
extern uint16_t numNewJobOffers;
extern JobOffer newJobOffers[];      /* 5-entry new job offers queue (in SWOS2.s BSS) */
extern uint8_t fundsBuffer[];        /* temporary buffer for team name during offer conversion */

/* ---- C-callable ASM bridges ---- */
extern uint8_t *_CCallGetTeamPointer(uint16_t teamNumber);


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
