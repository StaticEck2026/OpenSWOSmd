# careerTeam / currentTeam step-4 rework — design (read-only, pre-implementation)

> Status (2026-09-30): **implemented** -- all four phases landed: range checks bound on `CAREER_TEAM_BUF_SIZE`, `DIYTeamStats.teamSelOffset` holds a team identity resolved by `diy_resolve_team()` / `diy_team_to_identity()` (the field kept its legacy name, not `teamIdentity`), and `careerTeam`/`currentTeam` live in `src/career_team_buffers.c`. The "DESIGN ONLY" line below is the original header; kept as the record of how.

Status: **DESIGN ONLY — no code changed.** This is the plan for the final two
careerFileBuffer-overlay residents that the byte-neutral extraction recipe
(categories A, B1–B7, C) could not safely move. Review before implementing.

## 1. Goal

Let `careerTeam` (1292 B) and `currentTeam` (1292 B) be lifted out of the
`careerFileBuffer` overlay into standalone storage, the same way every other
region now lives in its own `career_*.c`. Two load-bearing **adjacencies**
currently block that move. This rework removes both, then the extraction is a
routine recipe pass.

`careerTeam` layout reminder: `TeamFileEx (684) + 16 reserve PlayerFile (608) =
1292` bytes (the player's club team plus its 16 reserve players).

## 2. The two blockers

### Blocker A — `selTeamsPtr`-relative `teamSelOffset`

`DIYTeamStats.teamSelOffset` (game_types.h:637) is an `int32` at offset 0 of each
18-byte league-standings row (`diyCompetitionStart + DIY_STANDINGS_OFFSET`,
0x2B4). It is **a byte distance from `&selTeamsPtr` to a team record**, not a
pointer:

- Regular selected teams sit **above** `selTeamsPtr` (the variable team tail):
  `teamSelOffset = index * 684` (≥ 0).
- `careerTeam` sits **below** `selTeamsPtr` in the overlay, so its
  `teamSelOffset` is **negative** (currently ≈ −42 KB).

Set at diy_competition.c:533 (`row->teamSelOffset = off`). Recovered as a team
pointer in ~10 sites, all of the form:

```c
TeamFileEx *t = (TeamFileEx *)(&selTeamsPtr + row->teamSelOffset);
```

(career_teams.c:1978/1982, diy_competition.c:1973/1974, 2214/2243, 3194/3195,
7351, 11028/11029). The same offset is also added to a **second** base for the
team *name*: `&selTeamNameBase + row->teamSelOffset` (diy_competition.c:580/581,
8598/8605) — `selTeamNameBase` aliases the same array shifted by the name field's
offset within `TeamFile`.

**Why it works today:** the offset is recomputed *live* at store time, so
recovery always lands on the right address regardless of layout — *provided* the
offset fits `int32` and is sign-extended correctly when added to the pointer.

**Why relocation is risky:** moving `careerTeam` to standalone BSS grows
`&careerTeam − &selTeamsPtr` from ≈ −42 KB to ≈ −5.9 MB (~140×). It still fits
`int32`, but it stresses exactly the signed-offset pointer arithmetic that has
bitten before (see `project_signed_teamseloffset_64bit_trap` — a zero-extension
bug that put the pointer ~4 GB off). And `SWOS_SAVE_FORMAT_JSON_VERIFY` would
**not** catch a regression here: it checks the saved byte image, not the runtime
pointer recovery. Only a functional test (careerTeam appearing in a league
standings table) exercises it.

### Blocker B — `careerNumPlayers` as `careerTeam`'s end-bound

Three sites test "is this player pointer inside the career team?" using the
*next overlay symbol* as the upper bound:

```c
if ((uint8_t *)playerFile >= (uint8_t *)&careerTeam &&
    (uint8_t *)playerFile <  (uint8_t *)&careerNumPlayers) { ... }
```

(menus.c:5080, menus.c:5122, teams.c:569). This relies on `careerNumPlayers`
being exactly `&careerTeam + 1292`. (career_status.c:3515 uses the *value*
`careerNumPlayers` as a loop count — not an address bound — and is unaffected.)

## 3. Key enabling fact — the JSON save is **already identity-based**

The portable save does **not** persist the raw `teamSelOffset`. The league
standings override and the grid-slot table emit a stable **identity** via
`diy_teamseloffset_to_identity()` (save_format_json.c:2865):

- `off ≥ 0` → team **index** `off / 684`
- `careerTeam`'s negative `off` → **`-1` sentinel**

and load converts back with `diy_identity_to_teamseloffset()`. So the on-disk
format is identity-based and layout-independent **already**; only the in-memory
`DIYTeamStats` field is offset-based. Aligning the runtime to the save is
therefore **save-format-neutral** (old JSON saves keep loading; VERIFY stays 0 by
construction — the override re-writes the field on both save and load).

## 4. Proposed rework (phased)

### Phase 1 — decouple `careerNumPlayers` (independent, low-risk, ship first)

Replace the 3 range-bound sites with an explicit size, e.g. a single helper:

```c
/* careerTeam is TeamFileEx + 16 reserve PlayerFile = 1292 bytes */
#define CAREER_TEAM_SIZE (sizeof(TeamFileEx) + 16 * sizeof(PlayerFile))   /* 1292 */
static inline int ptr_in_career_team(const void *p) {
    return (const uint8_t *)p >= (const uint8_t *)&careerTeam &&
           (const uint8_t *)p <  (const uint8_t *)&careerTeam + CAREER_TEAM_SIZE;
}
```

Pure refactor — `CAREER_TEAM_SIZE` == the current `&careerNumPlayers − &careerTeam`
gap (1292), so behaviour is identical. After this, `careerNumPlayers` (and the
`selTeamSelectedPlayerPosition` / `removedPlayerStatAccum` scalars beside it) no
longer depend on adjacency to `careerTeam` and can be extracted by the normal
recipe immediately. VERIFY-only test suffices for Phase 1.

### Phase 2 — centralise team recovery behind resolvers (no behaviour change)

Introduce two helpers and route **every** recovery site through them, still
storing the raw offset (so this phase is a no-op refactor, easy to verify):

```c
TeamFileEx *diy_resolve_team(int32_t v);       /* v is still an offset for now */
uint8_t    *diy_resolve_team_name(int32_t v);
```

This isolates the ~12 call sites so Phase 3 can flip the representation in one
place.

### Phase 3 — flip `teamSelOffset` to an identity (the real change)

- **SET** (diy_competition.c:533, the grid-slot writer): store the *identity*
  (`-1` for careerTeam, else the `selTeamsPtr` index) instead of the byte offset.
- **RESOLVERS:** `diy_resolve_team(id)` → `id == -1 ? &careerTeam :
  (TeamFileEx *)&selTeamsPtr + id`; `diy_resolve_team_name(id)` likewise off the
  team pointer's name field. No pointer arithmetic on a huge negative offset ever
  occurs — careerTeam is reached by a branch, not by `&selTeamsPtr + (−5.9 MB)`.
- **SAVE:** the override now stores/loads the field **verbatim** (it is already
  the identity) — `diy_teamseloffset_to_identity` / `diy_identity_to_teamseloffset`
  collapse to pass-throughs. JSON format unchanged.
- Rename the field `teamSelOffset → teamIdentity` (with a comment) so the meaning
  is explicit and greppable.

Phase 3 is the one that needs the **functional** test: play/sim a career league
where `careerTeam` is one of the standings rows and confirm the table renders the
right team, name, and sort order; plus a cup (the grid-slot `teamOffsetTable`
uses the same encoding) and a cross-target (Amiga BE) load.

### Phase 4 — extract `careerTeam` / `currentTeam` (now safe)

With both blockers gone, relocate the `careerTeam`, `careerNumPlayers`,
`selTeamSelectedPlayerPosition`, `removedPlayerStatAccum`, `currentTeam` run to a
standalone `career_team_buffers.c` via the standard recipe (real labels,
`.skip` hole, sync-list entry, 3-arch + VERIFY). `currentTeam` is the simpler of
the two (no `selTeamsPtr` relationship); it could even go in Phase 1 once the
range-bound is gone, if it has no other adjacency (verify first).

## 5. Risk / test summary

| Phase | Risk | Test |
|-------|------|------|
| 1 (careerNumPlayers) | very low (sizeof == current gap) | build + VERIFY 0 |
| 2 (resolvers) | low (no-op refactor) | build + VERIFY 0 |
| 3 (identity flip) | **medium** — touches save-critical DIY standings + grid slots | functional (careerTeam in league + cup standings) **and** cross-target (Amiga) **and** VERIFY 0 |
| 4 (extract) | low once 1–3 land | recipe: 3-arch + VERIFY 0 |

## 6. Sites inventory (for the implementer)

- `teamSelOffset` field: `game_types.h:637` (DIYTeamStats).
- SET: `diy_competition.c:533`; grid-slot `teamOffsetTable` writer (same encoding,
  diy_competition.c ~3066 / save side 3076).
- Recover-as-team (`&selTeamsPtr + off`): career_teams.c:1978,1982;
  diy_competition.c:1973-74, 2214, 2243, 3194-95, 7351, 11028-29.
- Recover-as-name (`&selTeamNameBase + off`): diy_competition.c:580-81, 8598, 8605;
  `diy_competition.c:6313` (`bracketScratch.teamOffsetTable` variant).
- Save identity conversion: save_format_json.c:2865 / 2875 (and callers ~3076, ~3303).
- careerNumPlayers range-bound: menus.c:5080, menus.c:5122, teams.c:569.
- Bounds guard already present: diy_competition.c:11068+ (auto-advance panic on
  out-of-range offset) — keep, adapt to identity range.
