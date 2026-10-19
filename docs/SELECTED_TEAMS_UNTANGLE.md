# Selected-teams cluster untangle (careerFileBuffer overlay, tail)

**Status: DONE.** Phases 1–2 landed (decouple + relocate the cluster to a typed
`.bss` carrier in `src/selected_teams.c`). **Phase 3 done 2026-06-18 (commit
debff93):** the 49 raw `(TeamFileEx *)&selTeamsPtr` cast sites across 12 files
converted to named `selectedTeams[i]` access via `extern TeamFileEx
selectedTeams[]` in `game_types.h` — byte-identical (same address+type), builds
x86_64/armhf/aarch64, Amiga-synced. Phase 4 (verify) is covered: the relocation
was build- + cross-target-save-validated earlier, and phase 3 changes no bytes.
The `(uint8_t*)&selTeamsPtr + offset` identity-resolve sites are intentionally
kept (teamSelOffset byte-offset identity). Original design pass below (kept for
history), same discipline as `docs/CAREERTEAM_STEP4_REWORK.md`.
(2026-09-30: the carrier is now spelled `_selectedTeams[100]` (`game_types.h`, with
the `_selTeamsPtr`/`_selTeamNameBase`/... views as macros over it). "Amiga-synced"
and the `sync_to_amiga_port.sh` step predate the 2026-08-12 merge.)

## Goal

Promote the raw `selTeamsPtr` byte region + its five alias labels into a typed
`TeamFileEx selectedTeams[]` array with named-field access, removing the last
substantive raw/adjacency cluster from the `careerFileBuffer` overlay (roadmap
step 3). The on-disk save is already typed for this region, so this is an
**in-memory-only** change — save-format-neutral.

## Current overlay layout (verified)

In `src/swos_globals_career_overlay.c`, immediately after `numSelectedTeams`,
the Amiga `ds.b` run (offsets from `&selTeamsPtr`):

| label | offset | reserved | what it really is |
|---|---|---|---|
| `selTeamsPtr` | +0 | 4 | base of inline `TeamFileEx[]` (NOT a pointer) |
| `selTeamIsComputerFlag` | +4 | 1 | `selectedTeams[0].team.teamControls` |
| `selTeamNameBase` | +5 | 20 | `selectedTeams[0].team.teamName[17]` |
| `selTeamImportField` | +25 | 5 | `selectedTeams[0].team.league` |
| `selTeamSocksColor` | +30 | 30 | `selectedTeams[0].team.prSocksColor` |
| `selTeamPlayerIndices` | +60 | 1308 | `selectedTeams[0].team.playerNumbers[16]` |
| `editTeamsBuffer` | +1368 | 43778 | edit-teams workspace (overlaps `selectedTeams[2..]`) |
| `editTeamsSaveVarsArea` | +45146 | 23254 | edit-teams save scratch |

## Verified mechanism

- **`selTeamsPtr` is the array base, not a pointer.** `extern uint8_t
  selTeamsPtr;` and team `i` is `(TeamFileEx *)&selTeamsPtr + i` (684-byte
  stride) — `diy_competition.c:74`. It used to be `TeamFile *selTeamsPtr` (a real
  4-byte pointer); a 684-byte write to `&selTeamsPtr` stomped the next global and
  hung the friendly-team menu, so it was redefined as a backing-buffer base
  (`swos_globals.c:590`), exactly like `currentMenu`/`menuEntries`.
- **The five `selTeam*` labels are aliases into record [0]** at the `TeamFile`
  offsets in the table above. Code derives the per-record field offset as
  `(&selTeamX - &selTeamsPtr)`, valid for every record incl. `careerTeam`
  (`diy_competition.c:80`). `selTeamImportField` is a misnomer — it's the
  `league` byte (offset 25), saved/restored during team import in
  `edit_tactics.c`.
- **`numSelectedTeams`** (uint16) bounds the array. Max count TBC — see Open
  questions; the legacy save size is `numSelectedTeams * 684 + 2`.
- **`editTeamsBuffer` overlaps the array.** Only `1368 B = exactly 2 × 684` sit
  between `selTeamsPtr` and `editTeamsBuffer`, so `selectedTeams[0..1]` live in
  the named region and `selectedTeams[2..N-1]` live inside `editTeamsBuffer`'s
  space. Career-play (selected teams) and edit-teams use this memory in
  different modes — an intentional union (cf. the league-table / audio-bank
  overlay, `project_diycomp_league_table_overlay`).
- **Save is already typed.** `emit_team_file_ex_body` / `load_team_file_ex_slice`
  serialize a `selected_teams` JSON array of `TeamFileEx` and load each into
  `(TeamFileEx *)&selTeamsPtr + i` (`save_format_json.c` ~2433/2513/3925). `teamSel`
  is already a stable identity (careerTeam=-1, else array index), not a raw
  offset (`careerTeam` step 4). So retyping the in-memory region changes no
  bytes on disk.

## Proposed design

Introduce a typed carrier and re-point the labels at it, keeping the
`editTeamsBuffer` union intact.

```c
/* selected_teams.c (new) — career-mode selected-team records.
 * Shares its backing with editTeamsBuffer (different game modes), so the
 * carrier is sized to the LARGER edit-teams view, and selectedTeams[] is the
 * typed view over its head. Do NOT split the union (overlapped-data rule). */
#define MAX_SELECTED_TEAMS 32   /* career_teams.c:262 caps numSelectedTeams<=32;
                                 * SAVE_SIZE 0x5582 = 2 + 32*684 confirms it */

union SelectedTeamsArea {
    TeamFileEx selectedTeams[MAX_SELECTED_TEAMS];  /* career view (head) */
    uint8_t    editTeamsBuffer[43778];             /* edit-teams view */
};
```

- `selTeamsPtr` → real label at `&selectedTeams[0]`.
- `selTeamIsComputerFlag/NameBase/ImportField/SocksColor/PlayerIndices` → real
  labels at the corresponding `selectedTeams[0].team.<field>` offsets (so
  `(&selTeamX - &selTeamsPtr)` keeps yielding the right per-record offset).
- `editTeamsBuffer` → the union's byte view (`+0` of the union, i.e. the same
  base; note: on Amiga `editTeamsBuffer` started at `+1368`, so the union base
  is `selTeamsPtr`, and `editTeamsBuffer` is `(uint8_t*)&selectedTeams[2]` —
  **confirm this is acceptable vs. keeping `editTeamsBuffer` at its exact legacy
  offset**; see Open questions).
- `editTeamsSaveVarsArea` stays as-is (after the union).

The overlay reserves the run's bytes with `.skip` (no cascade), matching the
pilot/region recipe; the typed carrier lives in the new file with the legacy
names re-emitted as real labels so every reader and the save `FX(...)`/`FA(...)`
descriptors keep working unchanged.

## Constraints discovered (post-design recon)

Two things assume `selectedTeams` sits **contiguously after `numSelectedTeams`**
inside `careerFileBufferBuf`:

1. **`_Save/_RestoreSelectedTeams`** (`career_teams.c:247/260`) stream one
   `0x5582`-byte block from `&numSelectedTeams` (= `numSelectedTeams` +
   `selectedTeams[32]`). **Decouplable** into two independent runs
   (count + array) with no behaviour change — same bytes.
2. **Legacy `.CAR` reader** (`_LoadCareerFile`, `file_io.c:446`): for a
   non-JSON save it does `_LoadFileEx(filename, &careerFileBuffer, …)` — a raw
   contiguous load. The team rosters land at `&selTeamsPtr` *because* it's
   adjacent to `numSelectedTeams`. **Not decouplable without a load-side fixup.**
   (The JSON writer is retired; the JSON reader path — `SaveJson_LoadCareer` —
   is typed and unaffected.)

Note: prior extractions (careerFinances, careerTeam, …) already relocated
fields out of `careerFileBufferBuf`, so a legacy `.CAR` load is **already lossy**
for those fields (their old bytes land in `.skip` holes; the relocated symbols
stay zero). Legacy `.CAR` is a best-effort read path; JSON is the format. So
relocating `selectedTeams` *continues* that trajectory — but `selectedTeams` is
the most critical payload (team rosters), so degrading it = a legacy `.CAR` load
that produces a teamless/crashing career.

Access sites (blast radius) are all `(TeamFileEx *)&selTeamsPtr` casts already —
`career_init.c` (378/390/1274), `career_teams.c` (105/141/683/823/908/1016),
`edit_tactics.c` (601/2378/2385/2412), `diy_competition.c` (74/80). So the data
is *already accessed as typed `TeamFileEx`*; the only raw-blob aspect is the
`uint8_t selTeamsPtr` backing.

## Decision needed: relocate vs in-place typed view

- **In-place typed view** — add `TeamFileEx *const selectedTeams = (TeamFileEx
  *)&selTeamsPtr;` + convert the casts to `selectedTeams[i].team.<field>`. Keeps
  the backing in `careerFileBufferBuf` → preserves legacy `.CAR` load and both
  adjacency readers. Low risk, build-safe. BUT low value: the casts already give
  typed access; this is mostly cosmetic — the backing stays a byte region.
- **Relocate** (careerTeam-style) — `selectedTeams[32]`+`editTeamsBuffer` union
  becomes a real typed symbol; `.skip` in the overlay. Genuine step-3 win, but
  needs the `_Save/_Restore` decouple AND accepts (or fixes up) the legacy `.CAR`
  rost­er loss. Bigger, can't build-verify locally.
- **Relocate + legacy-`.CAR` fixup** — relocate, and in `_LoadCareerFile`'s
  legacy branch copy the `numSelectedTeams*684` team block from the hole into the
  relocated array. Preserves compat; most work.

## Phased plan (mirrors careerTeam step 4)

1. **Decouple / scalar tail** — confirm no reader does raw `&selTeamsPtr + N`
   arithmetic that assumes adjacency to `editTeamsBuffer` beyond the union
   (grep `selTeamsPtr`, `editTeamsBuffer`). Resolve the offset question below.
2. **Introduce the typed carrier** (`selectedTeams[]` + alias labels) behind the
   existing offsets; overlay reserves bytes. Build all 3 Linux arches.
3. **Convert raw access sites** to `selectedTeams[i].team.<field>` /
   `selectedTeams[i]` where it clarifies (optional, incremental — the `.equ`/
   real-label aliases keep raw sites working meanwhile).
4. **Verify**: `SWOS_SAVE_FORMAT_JSON_VERIFY=1` round-trip = 0 mismatches;
   load an existing career `.CAR`; cross-target save round-trip (SDL→Amiga).

## Open questions (resolve before/at phase 1)

1. **Max `numSelectedTeams`** — RESOLVED: **32**. `career_teams.c:262`
   (`_RestoreSelTeamsPtr`) panics if `numSelectedTeams > 32`, and the backup
   `SAVE_SIZE 0x5582 = 21890 = numSelectedTeams(2) + 32 * 684`. So the carrier
   head is exactly `TeamFileEx selectedTeams[32]` (21888 B). Note: that backup
   (`career_teams.c` `savedTeamsBackupBuffer[0x5582]`) raw-memcpy's
   `numSelectedTeams`+array as one contiguous block — a raw-access site that
   relies on `selTeamsPtr` immediately following `numSelectedTeams`; the retype
   keeps that adjacency, so it stays valid.
2. **`editTeamsBuffer` base offset** — legacy `editTeamsBuffer` = `selTeamsPtr +
   1368` (= `&selectedTeams[2]`). Is anything sensitive to its *exact* address,
   or only to it being a ≥43778-byte scratch region that shares memory with the
   tail of `selectedTeams`? If the latter, the union as drawn is fine; if the
   former, keep `editTeamsBuffer` anchored at `+1368` and treat `selectedTeams`
   as the head that runs into it.
3. **Is the union load-bearing?** RESOLVED: **yes.** `swos_globals.c:583`:
   `career_teams.c:1660` stores `selTeamsPtr`-relative offsets in competition-slot
   fields that reach into `editTeamsBuffer`'s space (the array overruns into it for
   teams 2..31). So `selectedTeams`, `editTeamsBuffer`, and the contiguous
   `editTeamsSaveVarsArea` must relocate **as one 68400-byte block** (= 4+1+20+5+30
   +1308+43778+23254) to keep those offsets valid. All three are `&symbol`-addressed
   only and not saved (`save_format_json.c:2558` dropped them), so a contiguous
   relocation is clean — but `editTeamsBuffer`'s *position* has a history of
   adjacency surprises (`swos_amiga_data_stubs.c:917`, imageCmdWorkBuf overflow);
   that overflow is now contained (imageCmdWorkBuf is self-sized 2 MB), so moving
   the block is safe.

## Phase-2 implementation shape (resolved)

New file `selected_teams.c` — one `__asm__` `.bss` block (careerTeam pattern),
real labels at the exact offsets, total 68400 B:
`selectedTeams`+`selTeamsPtr` (same base, +0) · `selTeamIsComputerFlag` +4 ·
`selTeamNameBase` +5 · `selTeamImportField` +25 · `selTeamSocksColor` +30 ·
`selTeamPlayerIndices` +60 · `editTeamsBuffer` +1368 · `editTeamsSaveVarsArea`
+45146. The overlay replaces the eight label defs with one `.skip 68400` (no
cascade — `saveGameLength` et al. keep their offsets). Add `selected_teams.c` to
the Makefile object list and to `sync_to_amiga_port.sh` SRC_FILES. `selectedTeams`
is added as a typed `extern TeamFileEx selectedTeams[]` alias at the base so new
code can use named-field access; existing `(TeamFileEx *)&selTeamsPtr` casts keep
working (Phase 3 converts them incrementally).

## Save-format impact

None. `selected_teams` is already a typed JSON array and `teamSel` is already an
identity. This is purely the in-memory representation (roadmap step 3). No
`.CAR`/`.PRE`/`.DIY` re-save required.

## Related

- `docs/CAREERTEAM_STEP4_REWORK.md` (the precedent + the identity decoupling that
  unblocks this), `docs/OVERLAPPED_DATA.md`, memory `project_career_buffer_overlay`,
  `project_overlapped_data_regions`, `project_signed_teamseloffset_64bit_trap`.
