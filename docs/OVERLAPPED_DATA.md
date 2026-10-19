# Overlapped data regions

> **Status (2026-09-30): reference, partly resolved.** #5 (league standings past
> the struct end) is resolved — `DIYCompetitionEx.leagueStandings[64]` is a real
> field (`44e6f81`, docs/TEAMOFFSETTABLE_OVERLAY_UNTANGLE.md); #8 is resolved
> (`d15d79f`); the `careerFileBufferBuf` overlay is retired
> (`src/swos_globals_career_overlay.c` holds two plain globals). #1 is still not
> modelled. The `game_types.h:NNN` line numbers below predate many edits (it now
> lives at `include/game_types.h`) — grep the symbol. The "64-bit transition"
> framing is historical (docs/64BIT_TRANSITION.md).

## What this is

Several runtime structures in SWOS deliberately re-use the same memory
bytes for different semantic views at different points in the game's
lifecycle. This was a memory-saving pattern on the Amiga — the
competition struct, for example, never needed both "team stats during
matches" and "team-pointer cache during selection" data live at the
same time, so the IDA-reversed type makes them a `union`:

```c
union DIYCompetitionEx_Overlap {
    struct DIYTeamStats              teamStats[24];   // 432 B
    struct DIYCompetitionEx_Overlap_1 cacheView;       // 240 B (rest = pad)
    byte                              overlapRaw[432];
};
```

Same bytes; the consumer code decides which view applies based on
game phase.

## Why it matters for this port

Two reasons:

1. **Bug-hunting.** When a value at a given offset has unexpected
   contents, it might be because *another* subsystem just wrote a
   different view of the same bytes. The asm/IDA reference shows the
   union clearly; our C port often has only the dominant view typed
   as a struct field, with the alternate view hidden behind raw
   byte arithmetic in the few functions that need it. Grepping for
   the field name doesn't surface those alternate writers.

2. **Future separation.** Long-term we want to split each overlap
   into separate, lifetime-distinct buffers. Modern memory is cheap;
   the storage pressure that justified the overlap is gone. After
   separation, each subsystem owns named storage and the "this byte
   means X or Y depending on phase" trap goes away.

   We are **not** doing the separation as part of the 64-bit
   transition. Structural changes to these regions risk regressions
   in code paths we may not have full coverage for. The 64-bit work
   keeps the existing overlaps, just gives every byte-level access a
   named struct field (Phase 1 of the 64-bit plan).

## Known overlap regions

This list is incomplete — add to it as we discover more.

### 1. `DIYCompetitionEx_Overlap` (DIYTeamStats vs cacheView)

**Where:** `DIYCompetitionEx` extended region. In the IDA reference
this is `union overlap` at parent offset ~0x308 (after
`cupTeamIds[64]`), 432 bytes.

**View A — `teamStats[24]`:** array of `DIYTeamStats` records (18
bytes each: `teamSelPtr` u32 + `gamesPlayed`/wins/draws/losses/
goalsFor/goalsAgainst/points u16 each). Used during league play to
accumulate per-team results.

**View B — `cacheView` (DIYCompetitionEx_Overlap_1):**
- `byte _teamStats0_7[144]` — first 8 teamStats records' bytes
  remain readable here
- `uint teamPtrCache[24]` — 24 cached `selTeamsPtr` offsets used
  during team selection / draw-order generation

**Why it works:** league play populates teamStats while the
selection cache is unused, and the selection / draw flow only runs
before/between competitions when teamStats is dormant.

**Our port:** we don't yet have this union in `game_types.h`.
Consumers reach the alternate view through raw byte arithmetic. Worth
auditing when we get to Phase 1B follow-up (the `_FindTeamInDIY`
function in particular reads through `raw + 0x2B4` to walk
`DIYTeamStats` records).

### 2. `matchData` union (cup vs league view)

**Where:** `DIYCompetitionEx.matchData`, parent +0x64..+0x14D
(0xEA bytes).

**View A — `cup` (`DIYCompetitionEx_CupView`):** `currentMatchIndex`,
`roundLegs[6]`, `roundExtraTime[6]`, `roundPenalties[6]`,
`cupRoundResults[100]`, `_shuffleIndicesAlias[2]`, …

**View B — `league` (`DIYCompetitionEx_LeagueView`):**
`leagueMinSubstitutes`, `leagueMaxSubstitutes`, `hasGroupsDiy`,
`totalMatchesPerSection`, `teamEntryOffsets[14]`, `cupRoundResults[100]`
(same 100 bytes as cup view — re-used as scratch storage in league
mode), `shuffleAndPad[64]`, `leagueTeamIds[14]`, `competitionFormat`,
`roundFormat`.

**Status:** modelled in our `game_types.h:619-669`. Consumers
correctly switch between `.cup.foo` and `.league.bar` based on
competition type.

### 3. `bracketScratch` overlay on `teamShuffleOrder`

**Where:** `DIYCompetitionEx`, parent +0x30E..+0x5C7 (698 bytes,
SWOS2.s named `diyTeamShuffleOrder`).

**View A — `teamShuffleOrder[698]`:** raw bytes used at the start
of cup setup for shuffled team indices.

**View B — `bracketScratch` (anonymous struct):**
- `shuffleHead[0x20]` — keeps the raw shuffle bytes alias for the
  first 32 bytes
- bracket-progression counters (`secondLegCounter`, `groupSectionFlag`,
  `matchCount`, `totalMatches`, `curMonthIdx`)
- `progTable[0xC]` — month progression bytes
- `teamPtrTable[64]` — per-team `selTeamsPtr` offset cache
- `scratchTail[0x184]`

**Status:** modelled as an anonymous union in our
`game_types.h:751-764` so callers can keep using
`teamShuffleOrder[i]` for the head bytes and
`bracketScratch.<field>` for the named bracket state.

### 4. `diyFileBufferCopy` vs `diyCompetitionStart`

**Where:** Two separate buffers; they're contiguous in BSS on
both Amiga and SDL (diyCompetitionStart is aliased at
diyFileBufferCopyBuf + 0x420 — see the "diyFileBufferCopy + diyCompetitionStart
share one BSS run" comment in `src/swos_globals.c`).

- diyFileBufferCopy: DIYCompetition base (0x60), then
  `BSS_00_2DB6A` (0x3A), then `diyTournamentTeamIds` (0x386)
- diyCompetitionStart: full `DIYCompetitionEx` (0x5C8)

The two buffers share the same `DIYCompetition` base layout for the
first 0x60 bytes — both can be cast to `DIYCompetition *` for the
header fields. Past 0x60 they diverge: diyFileBufferCopy uses the
adjacent region for tournament slot records and team-id tables;
diyCompetitionStart uses it for the cup/league match data union and
team stats.

**`_FindTeamInDIY` consequence:** the function takes a generic
`DIYCompetition *` parameter and accesses extended bytes via raw
pointer arithmetic because the meaning of bytes past 0x60 depends
on which buffer the caller passed in. Can't be cleanly typed
without introducing a separate `DIYFileBufferCopy` struct that maps
the second region's layout.

**Status:** partly documented in `game_types.h:704-711`. Phase 4 of
the 64-bit transition will need to define the alternate struct
properly when retiring the careerFileBufferBuf overlay aliases.

### 5. `clearLeagueTableStats` writes past struct end

**Where:** `diyCompetitionStart + 0x2B4` to `+0x734` (1152 bytes,
64 rows × 18 bytes of DIYTeamStats / DIYLeagueTableRow).

The byte range starts inside `DIYCompetitionEx` (at parent 0x2B4 =
the league standings area inside the `cupTeamIds[64]` array
according to one view, or `teamGridPairings`/`teamShuffleOrder`
according to another) and runs PAST `sizeof(DIYCompetitionEx)`
(0x5C8) into the address space of the adjacent `audioInstrumentBank`
global.

**Why it works:** audio bank isn't loaded during DIY menu
operation, so its memory is free to be repurposed as a 64-row
league standings table.

**Status:** documented in `game_types.h:698-703` and
`memory/project_diycomp_league_table_overlay.md`. The "extension
past struct end" is intentional shared-memory overlay; it is **not**
represented as a struct field; reached only via raw byte arithmetic
in the table-clear routine.

**RESOLVED (2026-06-13, `44e6f81`):** the standings are now the real
trailing field `DIYCompetitionEx.leagueStandings[64]` (offset 0x5C8 /
0x5D0 on 64-bit), reached via `DIY_STANDINGS_OFFSET` / `diy_standings_row`,
so they no longer overlay `cupTeamIds` or run past the struct. Verified
across SDL↔Amiga saves 2026-06-18 (`1b837bf`, docs/VERIFY_STAGE2.md).

### 6. `imageCmdWorkBuf` sprite-pool overflow

**Where:** `imageCmdWorkBuf` global, originally 0xFFDC bytes on
Amiga.

The Amiga's sprite-graphics allocator deliberately overflows past
the end of `imageCmdWorkBuf` into the next ~12 adjacent BSS labels
(menuInsBank, unnamed scratch, tempTeamBuffer, teamFileBuffer, and
more — ~230 KB of effectively-contiguous storage).

**SDL port:** we don't have adjacent BSS — each global is its own
linker symbol. `imageCmdWorkBuf` is sized at 2 MB in our port so
sprite writes never overflow into adjacent globals (the Amiga target
now sizes it 0x46000 in chip RAM for the 4-plane pool). See
`memory/project_sprite_pool_overflow.md` and the comment block above
`_imageCmdWorkBuf` in `src/swos_amiga_data_stubs.c`.

This is a different shape of overlap (cross-symbol overflow rather
than within-struct union), but the same root pattern: the Amiga
freely reused adjacent BSS as logical extensions of one buffer.

### 7. `team1Scorers` / `team2Scorers` aliased onto loader-intro code

**Status:** already separated in our port — this entry is here as a
notable historical example, not active concern.

**Where (Amiga original):** `ShowLoaderIntroSequence` is a 68k
function at `$10FE2x` and friends. The asm took its address and
treated the function's machine-code bytes as `ScorerInfo
team1Scorers[8]` (528 bytes of in-match scorer data). The same bytes
were `team2Scorers[8]` starting at `loc_10FE24+2`. From SWOS2.s:

```
                lea     (ShowLoaderIntroSequence).l,a3 ; ScorerInfo team1Scorers[8]
                lea     ((loc_10FE24+2)).l,a4         ; ScorerInfo team2Scorers[8]
```

**Why it worked:** the intro sequence runs once at boot and never
again — by the time a match starts the function bytes are dead
weight. Reusing them as in-match scorer state saved 528 bytes of
RAM. On a 512 KB Amiga that was meaningful.

**Why it's not portable:** in a C/SDL build, function-machine-code
bytes are in read-only `.text`; you can't write to them. Even if you
could, the code is x86 / ARM / m68k — completely different bytes
than what the asm expected, so no offset within the function gives
useful "scorer data" content. And the runtime might fault on writes
to `.text` (most do).

**Our port:** `team1Scorers[8]` and `team2Scorers[8]` are normal
typed BSS globals (`_team1Scorers`/`_team2Scorers`) in
`src/swos_data_typed_stubs.c`. The intro-sequence code is
`ShowLoaderIntroSequence` in `src/loader_intro.c`, completely unrelated memory. Consumers in
`game_state.c`, `goal_scored.c`, etc. use the named globals.

This is the most aggressive overlap in the Amiga binary — *code*
and *data* sharing the same bytes via lifetime separation. Worth
remembering as the upper bound of what the original was doing for
memory pressure.

### 8. `teamSelectionKeyCache` vs `careerForeignMarketPlayers` / `careerJobOfferPhaseBias`

**Where:** all three are aliased into `careerFileBufferBuf` at:

| label | base offset | distance from cache start |
|---|---|---|
| `teamSelectionKeyCache`     | +73292 |   0  B |
| `careerJobOfferPhaseBias`   | +73293 |   1  B |
| `careerForeignMarketPlayers`| +75162 | 1870  B |
| `teamSelectionKeyCacheEnd`  | +93306 | 20014 B |

Defined as `.equ` aliases in `src/swos_globals_career_overlay.c:520-535`.

**View A — `teamSelectionKeyCache[]`:** 10-byte slot headers + 12-byte
fixture records, written by
`_TeamSelectionKeyCache_BuildFromCompetitionBlock` and
`_DIY_RecordLeagueFixtureResultAndAdvanceProgress`. In career mode
the slot stride is 22 bytes (one fixture per slot per the original
asm).

**View B — `careerForeignMarketPlayers[]`:** populated when the user
picks a career team; contains foreign-player records (compact 6-byte
records of country/team/player-slot triples). Lives at offset +1870
within the cache region.

**View C — `careerJobOfferPhaseBias`:** single byte at +1 inside
`teamSelectionKeyCache`. Used by the job-offer scheduler. Effectively
overlaid with the first byte of slot[0]'s header byte 1 (the
month-phase key). Odd but apparently intentional.

**Why it usually works:** the cache is small relative to the region —
even an 18-team Liga only needs 34 slots × 22 = 748 bytes, well
within the 1870-byte gap before foreign-market data starts. Foreign
market players don't overlap the live cache *bytes*, just the
*region*.

**Why it breaks on SDL:** the writer overflows the 22-byte career
slot (writes ≥2 fixtures per slot during phase-boundary loops),
stomping slot[N+1]'s stride field with team-ID bytes. The walker
then reads a corrupted stride and jumps somewhere past the
legitimate cache.

- **Amiga BE:** stride bytes `0E 0A` read as `0x0E0A=3594` → jump
  lands deeper in foreign-market data where bytes happen to be
  zero → walker reads numGames=0 → terminates cleanly. Accidental
  safety net.
- **SDL LE:** same bytes read as `0x0A0E=2574` → jump lands at the
  start of populated foreign-market records → walker treats them
  as fake cache entries → reader reads `02 00` as team id 0x0200 →
  `_VerifyTeamLoaded` panic.

**Why we can't enlarge the slot:** growing the slot past
1870/numSlots bytes would clobber `careerForeignMarketPlayers`. The
22-byte slot is load-bearing for the overlay packing.

**Current mitigation:** `_BuildGameListArrayFromTeamSelectionKeyCache`
(src/menus.c) validates the stride field at `listEntry+2` before
jumping. If outside [10, 256], treat as end-of-cache. Replicates the
Amiga's accidental safety-net behaviour deterministically.

**Status: RESOLVED (overlap removed).** The untangle landed in `d15d79f`:
`teamSelectionKeyCache` (+ cursors + end marker + the promoted
`careerJobOfferPhaseBias` scalar) now lives in `src/career_key_cache.c` and
`careerForeignMarketPlayers` in `src/career_foreign_market.c` as two independent
standalone symbols — they no longer share a BSS run, so a cache overflow can no
longer reach foreign-market player data. The cache buffer was then tightened from
the 20014-byte historical slack to an honest **8192 B** bound (~7x the measured
~1188 B HWM), so `InsertSpaceSorted`'s `0x8889` panic now fires on a genuine
overflow instead of silently eating slack (see `docs/KEYCACHE_CLUSTER_REWORK.md`
§8.3). The `[10,256]` stride clamp in the walkers stays as defence-in-depth but
is no longer load-bearing for player-data safety.

The remaining **intra-cache** per-record writer overflow (a career-mode 22-byte
slot can take >1 fixture record and spill into the next slot's header, still
matching the asm) is now contained inside the cache's own buffer and clamped —
harmless. A full writer redesign (advance to the next slot per fixture; size
`slotLen` from real records-per-entry) is a deferred follow-up that touches the
Amiga-shared builder/recorder — tracked in KEYCACHE_CLUSTER_REWORK.md §9.

### 9. Adjacent-globals reads

See `memory/project_adjacent_globals_bss.md` for the per-site list
(`stadiumMenuLittlePlayer` / `littlePlayerSpriteGeneric` /
`littlePlayersColorTable` triple is the classic). These aren't
struct-level unions — they're separate-symbol reads where the
consumer treats the symbols as one contiguous record. Phase 3 of
the 64-bit transition retires this pattern.

A particularly subtle variant lives in the `careerFileBuffer`
overlay: `competitionFileBuffer` is a 2-byte named global, but the
code casts `&competitionFileBuffer` to `DIYCompetition *` and reads
struct fields from it. The struct's fields-by-byte-position only
match the overlay's named globals on **32-bit Amiga** layout —
when `DIYCompetition.namePtr` grew 4 → 8 bytes on 64-bit, the
struct's `substitutesCount` slid off its original named global onto
a different one (`primaryCompSlotStateB`) whose value was unrelated
and happened to be `2`. See `docs/64BIT_TRANSITION.md` §3
"Concrete bite" and `memory/project_career_overlay_nameptr_gap.md`
for the bug write-up and the conditional `.skip 4` workaround. The
permanent fix is the broader 64-bit overlay alignment in
`64BIT_TRANSITION.md` Phase 4. *(Done: the overlay is retired and
`competitionFileBuffer` is now the standalone `CompetitionSlotBuffer`
`primaryCompetitionBuf`.)*

## Reference structs (IDA-reversed)

When the port's struct definition doesn't match the IDA reference,
the IDA version is generally more complete. Quick reference for the
DIYCompetitionEx region:

`data/SWOS2.h:2578-2613` (in this repo) —
full `DIYCompetitionEx` layout including the `overlap` union and
the trailing fields (`teamGrid[64]`, `teamOrder[32]`,
`unselectedTeamCount`, `replayFlag`, `matchesPlayedInRound`,
`totalMonthEntries`, `currentMonthIndex`, `cupMonthTable[12]`,
`gap_586[94]`).

`data/SWOS2.h:592-601` —
`DIYTeamStats` (18 bytes), the per-team record that `teamStats[24]`
in the overlap union contains.

Use these as authoritative when adding missing fields to our port's
`game_types.h`. Don't blindly copy the layout — our port has
diverged in places (the `bracketScratch` union, the
`leagueSectionState` broken-out struct) and those divergences are
intentional. But where IDA gives a name for a region that our port
treats as raw bytes, prefer the IDA name.

## Rule of thumb during 64-bit transition

- **Add struct fields.** If a region currently accessed via raw
  byte offsets is also a known overlap, define both views as a
  union and use named fields per view. Match the IDA reference
  layout where possible.
- **Don't separate yet.** Don't try to split an overlap into two
  buffers during the 64-bit work. That's a behavioural change
  with regression risk we shouldn't take on while also touching
  pointer widths.
- **Document new overlaps as found.** Add to this file's list when
  a Phase 1 conversion uncovers a previously-undocumented overlap.
  Tag the consumer functions so the next reader can find both
  writers.

The separation work happens after 64-bit is stable. At that point
we know the structural change is the only variable, so any
regression has a single cause to investigate.
