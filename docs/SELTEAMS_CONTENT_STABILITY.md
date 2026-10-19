# selTeams content stability — the cup teamOffsetTable staleness investigation

**Status: ✅ RESOLVED 2026-06-14 — and the selTeams-instability hypothesis below
was WRONG.** Live gdb proved `selTeams` is stable and complete
(`numSelectedTeams==numTeams`, every cup team at its expected index). The real
root was a **comp-slot copy-length truncation**: the type-1 (cup) store/load and
the `competitionFileBuffer<->diyCompetitionStart` copies used a hardcoded `0x222`
words (the 32-bit "through teamOffsetTable" size). On 64-bit the `namePtr` growth
pushed `teamOffsetTable[62]` (0x444) and `[63]` (0x448) **past** that boundary, so
store dropped them and load left the destination's stale `0` → those cup slots
resolved to `selTeams[0]` (the player) → double-booked fixture → phantom PLAY
MATCH menu. Fixed by deriving the length from `offsetof` (`DIY_CUP_COPY_WORDS` in
`game_types.h`, commit `37d4bab`; `0x222` on 32-bit/Amiga unchanged, `0x226` on 64-bit). Validated:
4 full seasons clean. The investigation notes below are kept as the record of
what was ruled out (selTeams content was NOT the problem).

## What we proved (static RE, 2026-06-14)

1. **`teamOffsetTable[62/63]=0` are NEVER-WRITTEN entries.** `_VerifyTeamLoaded`
   (career_teams.c:126) *panics* on a cache miss — it never returns
   `selTeams[0]`. So a `0` is the zero-initialised buffer value, not a
   resolved-to-player fallback. The dump `[56..61]={56..61}, [62/63]={0,0}` is
   the signature of a build that covered fewer slots than the round references.

2. **The build is grid-driven and stores selTeams INDICES.**
   `_DIY_BuildTeamSelectionPointerCache` (diy_competition.c:1604, asm
   `DIY_BuildTeamSelectionPointerCache` SWOS2.s:55523) writes
   `teamOffsetTable[grid[i]] = identity(cupTeamIds[grid[i]])` for
   `i=0..numTeams-1`. `identity()` (`diy_team_to_identity`, diy_competition.c:86)
   = the team's index into the cumulative `selTeams` array (or `-1` for
   careerTeam). `teamOffsetTable` is a parallel array to `cupTeamIds`.

3. **The asm builds it ONCE at `DIY_InitCupBracketState` and never rebuilds.**
   Amiga `selTeams` is address- and content-stable, so the cached indices stay
   valid for the whole season. The SDL rebuilds (career_teams.c:1817,
   career_status.c:4409, the choke-point diy_competition.c:2920) were *added*.

4. **The after-match auto-play path does NO rebuild — and that is FAITHFUL.**
   `Career_ProcessCurrentCompetitionSlot{For,After}Match` (SWOS2.s:60617/60649)
   is just Load → AutoSim/AutoPlay → Store, no rebuild, no Save/Restore. If the
   player is eliminated from a cup, that cup is *only ever* processed via this
   no-rebuild path → its stale `teamOffsetTable` is used as-is.

5. **`selTeams` is ONE CUMULATIVE array of ALL competitions' teams** (the model
   correction). `_AppendTeamsToSelectionFromTmdOrTeamFile` (career_teams.c:651)
   appends at `numSelectedTeams` and dedups via `findTeamInCache`; each slot's
   `_Career_InitCompetitionTeamSelection_Dispatch` appends. So slots 0–4 coexist
   in one dedup'd `selTeams`, and each cup's `teamOffsetTable` indexes into it.

6. **The slot load/store carries only the DIYCompetition block, not selTeams**
   (asm verified). `selTeams` is bracketed by single-buffer
   `_Save/_RestoreSelectedTeams` (one snapshot) across `career_season.c`.

## The SDL divergence (the bug)

On Amiga the cumulative `selTeams` is stable through the season, so a cup's
`teamOffsetTable` indices (built at season setup) stay valid at after-match
resolution. On SDL the inline comments (career_teams.c:1804-1816,
career_status.c:4398-4407) assert `selTeams` is "repopulated/reordered between a
cup slot store and load" — so the indices go stale, and the never-written
62/63 resolve to `selTeams[0]` = the player (id 0x0800, teamControls=2) →
player double-booked → phantom PLAY MATCH menu opened mid-auto-play
(`AutoPlayCupLoop → PlayCupMatchAndAdvance → SetupPlayers → ShowPlayMatchMenu`)
→ career can't progress → "inputs dead".

**Stage 1 (leagueStandings relocation, 44e6f81) already removed the standings
clobber** of `teamOffsetTable` — so this residual is NOT that. It is the
cumulative-`selTeams` shrink/reorder.

## The open question (must answer to fix correctly)

**Between the cup's `teamOffsetTable` build (season setup) and the after-match
fixture resolution, does the cumulative `selTeams` change on SDL — and how?**
Two concrete sub-questions:

- Does `numSelectedTeams` *shrink* (e.g. a `_RestoreSelectedTeams` brings back a
  smaller player-only snapshot, so the cup's high indices fall out of range and
  index 0 = player)?
- Or does the careerFileBuffer overlay reuse *stomp/reorder* the `selTeamsPtr`
  byte region (selTeams lives in that overlay)?

The asm uses the SAME single-buffer Save/Restore, so for Amiga to work the
backup at each Save point must hold the FULL cumulative array. If SDL ever Saves
a *reduced* selTeams (or restores one), that is the divergence.

## Recommended immediate step: instrument, don't guess

The corruption is runtime state and the static blast radius is the whole
careerFileBuffer overlay, so pinpoint it with capture (behind
`SWOS_DEBUG_AUTOSKIP`) on the tester's next fresh career:

- At the cup `teamOffsetTable` build (career_status.c:4409 and
  diy_competition.c:1604): log `numSelectedTeams`, `numTeams`, and the full
  `cupTeamIds[k] → identity` map for k=0..numTeams-1.
- At the after-match cup fixture resolution that double-books
  (`_Career_AutoPlayCupMatchAndAdvanceLoop` / `_DIY_PlayCupMatchAndAdvance`):
  log `numSelectedTeams`, `currentMatchIndex`, `teamGridPairings[2*idx..+1]`,
  and `teamOffsetTable[slot1/slot2]`.

A single capture pair tells us whether `numSelectedTeams` shrank (→ Save/Restore
snapshot fix) or the indices were stomped at constant count (→ overlay-write
fix), which then picks the concrete fix below.

## Candidate fixes (decide after the capture)

- **A. Keep the cumulative selTeams whole across the season** — if a reduced
  snapshot is being Saved/Restored, ensure the backup always holds the full
  cumulative array (the asm relies on this). Smallest, most asm-aligned.
- **B. Stop the overlay stomp** — if a careerFileBuffer-overlay write corrupts
  `selTeamsPtr`, fix that write (this dovetails with
  `docs/SELECTED_TEAMS_UNTANGLE.md` relocating selTeams out of the overlay).
- **C. selTeams-independent resolution (Stage 4 fallback)** — if (A)/(B) prove
  intractable, store the BE team-id in `teamOffsetTable` and resolve via
  `_GetTeamPointer` (loads from disk if absent) so resolution no longer depends
  on the cumulative array's stability. Larger asm divergence; last resort.

## Constraints

- Do NOT add another blind `_DIY_BuildTeamSelectionPointerCache` rebuild to the
  after-match path (handoff directive; masks the real selTeams corruption).
- Per CLAUDE.md, a `_VerifyTeamLoaded` panic from the fix is a *useful signal*,
  not something to clamp.
- Back-port any selTeams-lifecycle change to the Amiga C port (it has the same
  cumulative selTeams + Save/Restore).

## Related

`project_cup_teamoffsettable_and_elim_freeze`, `docs/TEAMOFFSETTABLE_OVERLAY_UNTANGLE.md`,
`docs/SELECTED_TEAMS_UNTANGLE.md`, `docs/CAREERTEAM_STEP4_REWORK.md`,
memory `project_career_buffer_overlay`, `project_signed_teamseloffset_64bit_trap`.
