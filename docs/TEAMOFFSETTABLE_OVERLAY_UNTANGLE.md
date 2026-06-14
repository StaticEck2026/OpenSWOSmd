# teamOffsetTable / bracketScratch overlay untangle (roadmap step 3)

Status: **DESIGN / SCOPED** — 2026-06-13. The working code keeps the rebuild
band-aids (see commit `3ac95cf`). This doc captures the root cause and the proper
fix so it can be executed deliberately.

## Symptom chain (what the rebuilds compensate for)

`bracketScratch.teamOffsetTable[64]` (cup grid-slot → team identity) is part of
the cup scratch region that is **overlay-shared** with the DIY league standings
table (and `cupTeamIds` / `teamGridPairings` / `teamShuffleOrder` /
`audioInstrumentBank`). See `clearLeagueTableStats` writing 64×18-byte rows from
`diyCompetitionStart+0x2B4 .. +0x734`, which overlaps `teamOffsetTable` at
`bracketScratch+0x344`.

In a career, league and cup competitions are processed across slots in the same
season. Processing a **league** slot writes standings into that overlapped region,
**zeroing the cup's `teamOffsetTable` bracket slots** — in practice the tail
slots **62/63** come back as `0`. (Confirmed live: `teamOffsetTable[0..61]` valid
ids/`-1`, `[62]=[63]=0`, while `cupTeamIds[62]=0x0857`, `[63]=0x0839` are valid
teams.)

The cup consumers then read those `0` slots:
- `_DIY_RecordCupBracketProgress` (keycache fill) → the original **panic 0x6666**
  (player duplicated into two fixtures, 22-byte entry overflow).
- `_Career_AutoSimulateCupMatchesUntilHumanTeam` (before-match auto-sim).
- `_DIY_PlayCupMatchAndAdvance` via `_Career_ProcessCurrentCompetitionSlotAfterMatch`
  (after-match auto-play).
- `_DIY_CompareCurrentMatchTeamCategories`, menu name draws, etc.

The current code rebuilds `teamOffsetTable` (`_DIY_BuildTeamSelectionPointerCache`,
re-derives from `cupTeamIds`) at each consumer entry: `career_teams.c:1812` (menu),
`career_status.c:4402` (season, gated `compType==1`), and the fill choke point
(`3ac95cf`). These are **overlay-clobber compensation**, not pure staleness fixes.

## Why the "stable team-id" representation alone is NOT enough

Attempted 2026-06-13 (reverted): change `teamOffsetTable`/`teamSelOffset` from a
selTeams **index** to the team's stable **id** (`PACK_BE_U16(country,ordinal)`),
resolved via `_VerifyTeamLoaded`; keep `-1` for careerTeam.

- This *does* fix the orthogonal **index-reorder staleness** (careerFileBuffer
  overlay reorders selTeams between slot store/load) and makes the JSON save
  robust (no volatile indices on disk).
- BUT the strict id-resolver **panics on a clobbered `0` slot** where the old
  `&selTeamsPtr + index` arithmetic silently returned `selTeams[0]`. Because the
  clobbering is pervasive, this turned into whack-a-mole panics across every
  consumer. The id change is only viable **after** the overlay clobbering is gone.

Keep the reverted diff in mind for re-application post-untangle (core change was
~2 functions in `diy_competition.c` + 2 raw-index writes in `save_format_json.c`).

## The proper fix

Give the cup scratch (`teamOffsetTable`, and as needed `cupTeamIds` /
`teamGridPairings` / `teamShuffleOrder`) its **own backing storage**, separate
from the league standings overlay, so league processing can no longer clobber it.
Then:
- the 3 rebuild calls (+ the auto-sim one that had to be added) can be **removed**;
- `teamOffsetTable` is built once at cup init and stays valid all season;
- the stable team-id representation can be re-applied cleanly.

### Constraint / caveat

Current guidance (memory: *"Overlapped data regions — do NOT separate during
64-bit transition"*) deliberately preserves these overlays. This untangle is a
**deliberate exception** for one runtime-scratch region (NOT a saved-data overlay).
Validate that nothing relies on the cup/league scratch aliasing, and cross-target
test saves (SDL ↔ Amiga) since `cupTeamIds`/standings serialization touches the
same region. Back-port to the Amiga C port.

## Separate, unrelated open bug (do not conflate)

Season-transition **panic 0x4459** in `_Career_SetCurrentCompetitionSlot` (slot 4,
DIVISION TWO): `_DIY_SelectCompetitionSlot4FromCurrentDivision`
(`career_init.c:856`) walks `englishLeaguesBlock` with raw Amiga byte offsets
(`divisions[divIndex].unk1` as a self-relative offset to a `{diyComp,
diyCompetitionTablePtr}` pointer pair). On 64-bit the `name[10]`/`playoffLinkPtrs[2]`
pointer arrays grew 4→8 bytes, so the offset lands on `name[4]` (= `aDivisionTwo`)
instead of the DIY comp. Fix = typed `EnglishLeaguesBlock` field access (see the
model in `career_season.c:1472+`). Asm ref: SWOS2.s line 59313.
