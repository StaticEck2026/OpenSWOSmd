# Design note: untangle the tournament buffer into a typed struct

Status: **DONE** (this header was stale). The tournament buffer is typed —
`diyFileBufferCopy` is viewed through `DIYTournamentState` (`game_types.h`, with
offset static-asserts), landed via the type-2 / cross-target-save work. Design
notes below kept for history.
Author context: fixes the 64-bit-only preset-WORLD-CUP bug (panic `0x8034`,
ROMANIA×5 in every group, inflated PL column) by removing the overlap that
causes it, rather than band-aiding the offset. This is roadmap **step 3
(untangle the data overlays)** scoped to the tournament path.

---

## 1. Why this is the fix (and why now)

The bug, confirmed from a `SWOS_DEBUG=1` log: the tournament's 24-team index
list lives at the **literal byte offset `0x5A`** inside `diyFileBufferCopy`.
On 64-bit, `DIYCompetition` grew +4 (the `namePtr` widened 4→8), so the header
fields `competitionFinished` (→ `0x5A`) and `currentRoundIndex` (→ `0x5C`) now
sit **on top of** the team list. A routine `comp->competitionFinished = 0`
during slot load zeroes team entries `[0..3]` → 4 teams collapse to
`selTeamsPtr[0]` (ROMANIA) → corrupted qualifiers → `d1=0x32` not in
`diyGroupPatternLookup` → panic. 32-bit / Amiga never had it (no struct
growth, no collision).

Shifting the literal (`0x5A`→`0x5E`) would just relocate the landmine: the
list stays hand-overlapped onto the header. The real fix is to give the buffer
**named fields the compiler lays out without overlap** — then the collision
cannot exist on any target.

This is safe to do **now** because the **JSON save format already decoupled
the on-disk format from the in-memory layout**. The legacy `.CAR` memory-dump
is what forced us to preserve the exact Amiga byte layout, overlaps and all.
With saves serializing *named fields*, we are free to relayout the buffer.

---

## 2. Current layout (ground truth)

Backing buffer `diyFileBufferCopyBuf` (`swos_globals.c:1105-1146`), 8192 B:

| Offset | Size | Type | Holds | Notes |
|--------|------|------|-------|-------|
| `0x000` | 0x5A* | `DIYCompetition` header | competition descriptor | typed struct is 0x60 (32b) / **0x68 (64b)** — the +8 growth is the bug source |
| `0x05A` | 0x40 (64 B) | `uint8_t[64]` | **team-index / seed order** | init `0..63`; **mutated by seeding → persistent, must round-trip** |
| `0x09A` | 0x80 (128 B) | `uint16_t[64]` | `diyTournamentTeamIds` (BE-packed) | written by `_CopySelectedTeamNumbers` |
| `0x11A` | 0x40 (64 B) | `uint8_t[64]` | `groupMap` (team→group) | derived, but rides the copy; feeds the 0x8034 lookup |
| `0x15A` | 0x2C4 (708 B) | `DIYTournamentSlot[6]` | the 6 stage slots | ends `0x41E` |
| `0x420` | — | `DIYCompetitionEx` | **`diyCompetitionStart`** (current-stage view) | separate `.equ` alias, see §4 |

\* The header *as used by the tournament* effectively ends where the list
begins (`0x5A`). On the Amiga the list deliberately overlapped
`numRounds`/`subRoundCounter`/`playEachTeam` (0x5A/0x5C/0x5E) — fields the
tournament does **not** use as header. The +8 growth slid the *used* fields
(`competitionFinished`/`currentRoundIndex`) into the list. That is the whole
bug.

---

## 3. The coupling that expands the scope

The four copy functions (`diy_competition.c:1023-1072`) are **flat word
copies** between `competitionFileBuffer` and `diyFileBufferCopy`:

- `_SaveDIYTournament` / `_DIY_CopyCompetitionFileBufferToDiyFileBufferCopy`: 1450 words `cfb → diyFileBufferCopy`
- `_RestoreDIYTournament`: 1450 words `diyFileBufferCopy → cfb`
- `_DIY_CopyDiyFileBufferCopyToCompetitionFileBuffer`: 1450 words back

They **assume `competitionFileBuffer` and `diyFileBufferCopy` have identical
internal layout** for type-2 competitions, and the 2900-byte length
deliberately reaches past `0x420` into the `diyCompetitionStart` standings
(that is why the `0x420` adjacency exists). Consequences:

1. `competitionFileBuffer` is the same overlap layout (it's a `DIYCompetitionEx`
   in the `careerFileBuffer` overlay, 2-byte-anchored —
   `swos_globals_career_overlay.c:937`), and its 2900-byte tournament tail
   **spills into adjacent overlay globals by design**
   (`diyTeamIdListType2/0/1`).
2. `_FindTeamInDIY` steps from the tournament buffer into the current-stage
   buffer with a **literal `raw += 0x420`** (`diy_competition.c:9649`).

So once `diyFileBufferCopy` stops being byte-identical to `competitionFileBuffer`,
**the four copies can no longer be flat** and the `0x420` adjacency must be
preserved (or both folded into one parent struct).

---

## 4. Proposed target shape

```c
/* The tournament working record (was diyFileBufferCopy 0x000..0x41F). */
typedef struct DIYTournamentState {
    DIYCompetition    header;            /* 0x00 */
    uint8_t           teamIndexOrder[64];/* seed/shuffle order (was +0x5A) */
    uint16_t          teamIds[64];       /* BE-packed ids   (was +0x9A) */
    uint8_t           groupMap[64];      /* team->group     (was +0x11A) */
    DIYTournamentSlot slots[6];          /* stage slots     (was +0x15A) */
} DIYTournamentState;
```

`teamIndexOrder` is now a real field placed **after** the header by the
compiler — no overlap, collision gone on every target. The named fields
replace the ~85 raw-offset sites in `diy_competition.c`.

To keep the `0x420` adjacency (the copy-reach + `_FindTeamInDIY`), fold the two
regions into one workspace so the relationship is a struct fact, not a magic
constant:

```c
typedef struct DIYWorkspace {
    DIYTournamentState tournament;   /* was diyFileBufferCopy   */
    DIYCompetitionEx   current;      /* was diyCompetitionStart */
} DIYWorkspace;
/* diyFileBufferCopy := &ws.tournament ; diyCompetitionStart := &ws.current */
```

`diyTournamentCompetition` (the title-only alias, `swos_globals.c:1145`) becomes
`ws.tournament.header.competitionName`; its two read sites in `menu_data.c`
(2806/2811) get repointed.

---

## 5. The `competitionFileBuffer` decision (key review point)

Two ways to handle the copy coupling:

- **Option A — minimal, lower-risk (recommended first stage).** Untangle
  `diyFileBufferCopy` into the typed struct above; leave
  `competitionFileBuffer`'s tournament *backup* in its current overlap byte
  layout. Rewrite the four copies as **explicit field maps** (read cfb at the
  old offsets, write the named struct fields, and vice-versa). This fixes the
  bug (the *played* buffer is `diyFileBufferCopy`) and does **not** touch the
  `careerFileBuffer` overlay, so league/cup careers are insulated.
- **Option B — full untangle.** Also give `competitionFileBuffer` the typed
  `DIYTournamentState`, which means restructuring the `careerFileBuffer`
  overlay where it (and its intentional spill into `diyTeamIdListType*`) lives.
  Cleaner end-state, but it reaches into the overlay league/cup careers depend
  on — higher regression risk, better done as a later pass.

**Recommendation: A now, B later.** A fully fixes the reported bug and is the
roadmap direction; B is the remaining overlay debt and can follow once A is
validated.

---

## 6. Preserve the static-data populate

Type-2 records (`worldCup2`, `europeanChampionships2` — `competition_data.c:196/318`)
load via `_InitCompetitionFromRecord` → **`_InitDIYLeagueFromRecord`**
(`diy_competition.c:8972`), which: calls `_InitDIYTournament` (slots from
`diyTournamentInitTable`, `teamIndexOrder=0..63`); reorders `teamIndexOrder`
from the static `unk1[]` BE seed table against `teamIds`; fills the six slots
from the static stage records. This mapping is **behaviour we must keep** — the
migration only changes *how the destination bytes are addressed* (named fields
instead of `+0x5A/+0x15A`), not the algorithm.

---

## 7. Save-format follow-through

- The committed JSON `tournament_slots` serializer reads slots at raw `+0x15A`;
  it moves to `ws.tournament.slots[]` (cleaner, same data).
- `teamIndexOrder` is **persistent seeding state** → it must round-trip in the
  JSON save (add a named field). `teamIds` likewise become a named field.
- No schema bump needed for the layout change itself (in-memory only); the JSON
  field set is what matters, and it stays additive.

---

## 8. Staged migration plan

1. **Structs**: add `DIYTournamentState` (+ `DIYWorkspace`) to `game_types.h`;
   re-point `diyFileBufferCopy`/`diyCompetitionStart`/`diyTournamentCompetition`
   onto the workspace in `swos_globals.c`.
2. **Copies**: rewrite the four `copyWords` tournament copies as typed field
   maps (Option A).
3. **Access sites**: convert the ~85 raw-offset reads/writes in
   `diy_competition.c` to named fields; update `_FindTeamInDIY`'s `+0x420`.
4. **Populate**: re-express `_InitDIYTournament` / `_InitDIYLeagueFromRecord`
   destinations as fields (behaviour unchanged).
5. **JSON**: point `tournament_slots` at `slots[]`; add `teamIndexOrder` /
   `teamIds` named fields.
6. **Validate** on `SWOS.x86_64`: WORLD CUP preset plays a full group stage +
   advance (no `0x8034`, distinct teams, correct PL); **and** a normal
   league/cup career is unaffected (regression gate). Rebuild 32-bit too and
   confirm byte-identical behaviour.
7. **Back-port** the same structs to the Amiga C port (`SWOS_C`); I sync the
   files, you review + commit.

---

## 9. Risks / open questions for review

- **League/cup regression** — Option A avoids the `careerFileBuffer` overlay,
  but the four copies and `_DIY_LoadCurrentCompetitionSlot` are shared with
  league/cup type-2-stage handling; the regression gate (step 6) is mandatory.
- **`teamIndexOrder` persistence** — confirmed it carries seeding state, so it
  must be saved, not just regenerated. (Flagging in case you expected it to be
  pure scratch.)
- **Scope of `competitionFileBuffer`** — confirm **Option A vs B** (I recommend A).
- **`DIYWorkspace` vs two adjacent allocations** — folding both into one struct
  makes the `0x420` adjacency a compiler fact; acceptable?
- **Size assert** — I'll add a `_Static_assert` that
  `offsetof(DIYTournamentState, slots) ==` the populate path's expectation on
  both arches, so the layout can't silently drift again.

---

## 10. What does NOT change

- The disk/static competition records (`Competition`, `CompetitionCup2`) — only
  how we copy *into* the working buffer.
- League/cup careers (Option A).
- The JSON save schema version (in-memory layout change only).
