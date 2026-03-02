/* career_init.c — Career mode transfer market initialization.
 *
 * Chunk 61: 3 functions ported from SWOS2.s.
 */

#include "game_types.h"
#include "menu_system.h"
#include "input_text.h"
#include "random.h"
#include "string_utils.h"

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
