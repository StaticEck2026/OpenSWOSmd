/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 */
/* swos_competition_slots.c — UNTANGLE step 3/4 (Phase A).
 *
 * The four career "context" competition slots (secondary / tertiary /
 * euro-cups / diy) used to live as tightly-packed `.equ`-aliased byte
 * regions INSIDE the careerFileBuffer overlay (swos_globals_career_overlay.c).
 * There the tournament team-index table had to overlay the DIYCompetition
 * header tail at raw 0x5A — which the 64-bit namePtr growth slid
 * `currentRoundIndex`@0x5C into, corrupting it (→ the euro-cup group-stage
 * cache never filled → end-of-season SIGSEGV).
 *
 * Here each slot is a standalone, properly-sized `CompetitionSlotBuffer`
 * (DIYTournamentState + stageSnapshot), so `teamIndexOrder` lives at its
 * real 0x41E with no header overlap and the per-slot standings no longer
 * spill into the next slot. The legacy global names that referred to header
 * fields (`*CompetitionName`, `*CompWinnerTeam`, `*CompSlotStateA/B/C`,
 * `*BonusValue`, `*CompFinished`) are re-based here onto the new buffers via
 * `.equ` so every existing reader keeps working unchanged — the targets and
 * their aliases live in this one TU, satisfying the .equ same-TU rule.
 *
 * (Phase B) competitionFileBuffer / slot 0 — the careerCompetitionSlotPtrTable[0]
 * "primary competition" — used to be the head of a ~20-label adjacent-globals
 * run INSIDE careerFileBufferBuf (competitionFileBuffer, competitionOver,
 * diyPresetNameOfCompetition, numDiyTeams, primaryComp*, leagueWarningsEnabled,
 * diyTeamIdListType2/0/1 = matchData/leagueTeamIds/cupTeamIds, …) that
 * collectively formed the slot-0 DIYCompetitionEx/DIYTournamentState union, with
 * the SAME 0x5A teamIndexOrder / 0x420 standings overlap the *Context slots had —
 * so on 64-bit a type-2 (WC) primary tournament corrupted currentRoundIndex@0x5C.
 * Now it is its own standalone CompetitionSlotBuffer (`primaryCompetitionBuf`)
 * exactly like the four context slots; the 18 named globals re-base onto it via
 * .equ at their historical cumulative offsets (CFB_NP = +4/64-bit namePtr shift,
 * 2*CFB_NP after the DIYCompetition header for the matchData-and-beyond region).
 * The overlay run in swos_globals_career_overlay.c is replaced by one
 * arch-conditional .skip of the run's old size so every label after it
 * (viewRandomPhase…) keeps its absolute careerFileBufferBuf offset — no cascade.
 */

#include "game_types.h"

/* Standalone backing storage — zero-init BSS, correctly sized + laid out. */
CompetitionSlotBuffer primaryCompetitionBuf;          /* slot 0 (primary)   */
CompetitionSlotBuffer competitionContextBuf;          /* slot 1 (secondary) */
CompetitionSlotBuffer tertiaryCompetitionContextBuf;  /* slot 2 (tertiary)  */
CompetitionSlotBuffer euroCupsCompetitionContextBuf;  /* slot 3 (euro cups) */
CompetitionSlotBuffer diyCompetitionContextBuf;       /* slot 4 (diy)       */

/* Re-base the legacy slot globals onto the typed buffers. `CFB_NP` is the
 * 64-bit namePtr-growth shift (+4) applied to every DIYCompetition field
 * past namePtr@0x28 — competitionName@0x04 is before it, so unshifted. The
 * offsets match the historical overlay layout exactly (see the .skip
 * sequences this replaces in swos_globals_career_overlay.c). */
#define SLOT_ALIAS(name, buf, off) \
    ".globl " #name "\n" \
    ".equ "   #name ", " #buf " + " #off "\n"

__asm__(
#if UINTPTR_MAX != 0xFFFFFFFFu
    ".set CFB_NP, 4\n"
#else
    ".set CFB_NP, 0\n"
#endif
    /* ---- slot 0: primary (competitionFileBuffer) ----
     * The 18 named globals of the old careerFileBufferBuf run, re-based onto
     * primaryCompetitionBuf at their historical cumulative offsets. competitionFile-
     * Buffer/competitionOver/diyPresetNameOfCompetition sit at/below namePtr@0x28
     * (no shift); everything from avgSkill@0x30 takes CFB_NP; the DIYCompetitionEx
     * region from minSubstitutes (= sizeof(DIYCompetition)) on takes 2*CFB_NP. */
    SLOT_ALIAS(competitionFileBuffer,     primaryCompetitionBuf, 0)
    SLOT_ALIAS(competitionOver,           primaryCompetitionBuf, 0x02)
    SLOT_ALIAS(diyPresetNameOfCompetition, primaryCompetitionBuf, 0x04)
    SLOT_ALIAS(majorTrophyBonusValue,     primaryCompetitionBuf, 0x30 + CFB_NP)
    SLOT_ALIAS(numDiyTeams,               primaryCompetitionBuf, 0x32 + CFB_NP)
    SLOT_ALIAS(primaryCompWinnerTeam,     primaryCompetitionBuf, 0x3A + CFB_NP)
    SLOT_ALIAS(primaryCompSquadViewParam, primaryCompetitionBuf, 0x3C + CFB_NP)
    SLOT_ALIAS(primaryCompSlotStateA,     primaryCompetitionBuf, 0x3E + CFB_NP)
    SLOT_ALIAS(primaryCompSlotStateB,     primaryCompetitionBuf, 0x40 + CFB_NP)
    SLOT_ALIAS(primaryCompSlotStateC,     primaryCompetitionBuf, 0x42 + CFB_NP)
    SLOT_ALIAS(diyTeamMinDivisionIndex,   primaryCompetitionBuf, 0x54 + CFB_NP)
    SLOT_ALIAS(diyTeamMaxDivisionOffset,  primaryCompetitionBuf, 0x56 + CFB_NP)
    SLOT_ALIAS(leaguePromotionCount,      primaryCompetitionBuf, 0x58 + CFB_NP)
    SLOT_ALIAS(leagueRelegationCount,     primaryCompetitionBuf, 0x5A + CFB_NP)
    SLOT_ALIAS(leagueWarningsEnabled,     primaryCompetitionBuf, 0x60 + 2*CFB_NP)
    SLOT_ALIAS(diyTeamIdListType2,        primaryCompetitionBuf, 0x9A + 2*CFB_NP)
    SLOT_ALIAS(diyTeamIdListType0,        primaryCompetitionBuf, 0x12E + 2*CFB_NP)
    SLOT_ALIAS(diyTeamIdListType1,        primaryCompetitionBuf, 0x24E + 2*CFB_NP)

    /* ---- slot 1: secondary ---- MIGRATED (untangle Part A, S4): consumers now use
     * the typed competitionContextBuf.tourn.header.{competitionType,competitionName,
     * trophyBonusValue,winnerTeam,slotStateA/B/C,compFinished} directly; the
     * competitionContext / secondaryCompetitionName / minorTrophyBonusValue /
     * secondaryCompWinnerTeam / secondaryCompSlotStateA-C / secondaryCompFinished aliases are retired. */

    /* ---- slot 2: tertiary ---- MIGRATED (untangle Part A, S3): consumers now use
     * the typed tertiaryCompetitionContextBuf.tourn.header.{competitionType,competitionName,
     * trophyBonusValue,winnerTeam,slotStateA/B/C,compFinished} directly; the
     * tertiaryCompetitionContext / tertiaryCompetitionName / leagueTitleBonusValue /
     * tertiaryCompWinnerTeam / tertiaryCompSlotStateA-C / tertiaryCompFinished aliases are retired. */

    /* ---- slot 3: euro cups (euroCupsCompetitionContext) ---- */
    SLOT_ALIAS(euroCupsCompetitionContext, euroCupsCompetitionContextBuf, 0)
    SLOT_ALIAS(quaternaryCompetitionName,  euroCupsCompetitionContextBuf, 0x04)
    SLOT_ALIAS(majorTrophyOrCupBonusValue, euroCupsCompetitionContextBuf, 0x30 + CFB_NP)
    SLOT_ALIAS(quaternaryCompWinnerTeam,   euroCupsCompetitionContextBuf, 0x3A + CFB_NP)
    SLOT_ALIAS(quaternaryCompSlotStateA,   euroCupsCompetitionContextBuf, 0x3E + CFB_NP)
    SLOT_ALIAS(quaternaryCompSlotStateB,   euroCupsCompetitionContextBuf, 0x40 + CFB_NP)
    SLOT_ALIAS(quaternaryCompSlotStateC,   euroCupsCompetitionContextBuf, 0x42 + CFB_NP)

    /* ---- slot 4: diy ---- MIGRATED (untangle Part A, S2): consumers now use
     * the typed diyCompetitionContextBuf.tourn.header.{competitionType,slotStateA/B/C}
     * directly; the diyCompetitionContext / diyCompSlotStateA/B/C aliases are retired. */
);
