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

/* The whole SLOT_ALIAS `.equ` family is RETIRED (untangle Part A complete, S1-S6).
 * All 5 career competition slots are now accessed exclusively through their typed
 * CompetitionSlotBuffer fields (.tourn.header.* / .tourn.* / DIYCompetitionEx and
 * DIYTournamentState casts of &<buf>), across every consumer + the save
 * descriptors. The legacy globals (competitionFileBuffer, competitionContext,
 * tertiaryCompetitionContext, euroCupsCompetitionContext, diyCompetitionContext,
 * and the per-slot competitionOver, *CompetitionName, *TrophyBonusValue, *CompWinnerTeam,
 * *CompSlotStateA-C, *CompFinished, numDiyTeams, diyTeamMin/MaxDivision,
 * league{Promotion,Relegation}Count, leagueWarningsEnabled, diyTeamIdListType0-2
 * names) no longer exist as symbols.
 *
 * Per-slot history: S2 diy (slot 4), S3 tertiary (slot 2), S4 secondary (slot 1),
 * S5 euro cups (slot 3), S6 primary (slot 0). Dual-use header offsets are modelled
 * as D1 anonymous unions in DIYCompetition (game_types.h); slot-0's
 * leagueWarningsEnabled@0x60 became a named DIYTournamentState field (D-W1); the
 * three slot-0 BE team-id views (type0/1/2) map onto leagueTeamIds/cupTeamIds/teamIds
 * via DIYCompetitionEx/DIYTournamentState casts (D-A1), BE access preserved. */
