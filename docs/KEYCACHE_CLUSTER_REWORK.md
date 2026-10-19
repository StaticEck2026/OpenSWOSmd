# teamSelectionKeyCache cluster rework — design (read-only, pre-implementation)

Status: **IMPLEMENTED + TIGHTENED — DONE.** The cluster lift + de-overlap
(Tier 1 + foreign-market split + phase-bias promotion) landed in `d15d79f`; the
cache buffer was then tightened from the 20014-byte historical slack to an honest
**8192 B** bound (§8.3 finish). What remains is only the deferred per-record
writer redesign (§9, optional, Amiga-shared). The plan text below is preserved
for reference. (Was: the last untangle item remaining in the `careerFileBuffer`
overlay after the careerTeam step-4 rework `79bddb0`; modeled on
`docs/CAREERTEAM_STEP4_REWORK.md`.)
(2026-09-30: verified -- `src/career_key_cache.c`, `KEY_CACHE_BUF_SIZE 8192`; the
`sync_to_amiga_port.sh` steps below predate the 2026-08-12 merge, when that script
became a tombstone.)

Decision taken (2026-06-08): **design Tier 2 first** so the extraction and the
overlap-removal land together as a clean struct rather than relocating the bug
intact. §§8–10 below are the Tier 2 detailed design; §§1–7 are the original
audit.

**IMPLEMENTED 2026-06-08 (`d15d79f`), then TIGHTENED 2026-06-16.** Stage 1 lifted
the cluster out intact at the original `&teamSelectionKeyCacheEnd` bound (20014 B
past `teamSelectionKeyCache`) so the `0x8889` panic fired at the identical point —
no behaviour change to the bound, only stopping foreign market from sharing the
slack. Files: `src/career_key_cache.c` (cursors + cache buffer + end marker +
promoted `careerJobOfferPhaseBias`), `src/career_foreign_market.c`
(`MarketPlayer[432]`); overlay → one `.skip` hole (since RETIRED with the whole
careerFileBufferBuf overlay in Part B); new `FX("career_job_offer_phase_bias", …,
FLD_U8)`. **Stage 2 (2026-06-16, the §8.3 finish):** now that nothing shares the
slack, the dedicated buffer was tightened from 20014 → **8192 B** — an honest
bound at ~7x the measured ~1188 B HWM (and ~3x the §8.1 analytical ~2.5 KB heavy
case), so `InsertSpaceSorted`'s `0x8889` now fires loudly on a genuine overflow
instead of swallowing 18 KB of dead slack ("panics are signals"). `.skip` wired
to `KEY_CACHE_BUF_SIZE` via stringize so the BSS reservation can't drift from the
declared size. 3-arch build-verified; `nm` confirms `&End−&cache`=0x2000 (8192),
foreign market a separate region. Functional gate (a heavy multi-competition
career season; confirm no spurious `0x8889`) is the load-time test since this
region is outside the VERIFY window.

## 1. Goal

Lift the `teamSelectionKeyCache` cluster — the cursors, the cache buffer,
`careerForeignMarketPlayers`, and the end marker — out of the `careerFileBuffer`
overlay so the overlay can keep shrinking toward retirement (roadmap step 3).
The cluster is the gnarliest residue because three internal adjacencies bind its
members together. This doc establishes that **none of those adjacencies reach
outside the cluster**, so the whole thing can be relocated as one unit; and it
separates that low-risk relocation (**Tier 1**) from the genuinely hard
overlap-removal (**Tier 2**, the documented writer-overflow fix).

## 2. The cluster — authoritative layout

From `SWOS2.s` 129242–129261 (BSS `ds.b` run) and the current SDL reservation in
`src/swos_globals_career_overlay.c` 326–349:

| Symbol | bytes | offset in cluster | role |
|---|---:|---:|---|
| `teamSelectionKeyCacheIndex` | 2 | 0 | walk cursor — current entry index |
| `teamSelectionKeyCacheOffset` | 2 | 2 | walk cursor — byte offset into cache buffer |
| `teamSelectionKeyCacheCount` | 2 | 4 | walk cursor — number of entries |
| `careerSlotProcessingInProgress` | 2 | 6 | flag — slot mid-processing |
| `teamSelectionKeyCache` | 1 | 8 | **cache buffer start** |
| `careerJobOfferPhaseBias` | 1 | 9 | job-offer phase scalar **= cache byte 1** |
| *(unnamed `BSS_01_1699A2`)* | 1868 | 10 | cache body |
| `careerForeignMarketPlayers` | 14784 | 1878 | `MarketPlayer[352]` |
| *(unnamed `BSS_01_16E082`)* | 3360 | 16662 | tail (= `MarketPlayer[80]` more) |
| `teamSelectionKeyCacheEnd` | 2 | 20022 | end marker / overflow bound / season flag |
| **total** | **20024** | | |

The cache buffer is the 1870 contiguous bytes
`[teamSelectionKeyCache, teamSelectionKeyCache+1870)`. `careerForeignMarketPlayers`
begins immediately after. `teamSelectionKeyCacheEnd` is the 2-byte word at the
very end. The cluster has **no pointer fields** (`MarketPlayer` embeds a
pointer-free 38-byte `PlayerFile`), so it is the **same 20024 bytes on 32-bit and
64-bit** — no `#if UINTPTR_MAX` growth pad is needed (unlike the comp-slot /
seasonInfo / comp-copy regions).

Note the SDL overlay already folds the two unnamed `BSS_01_*` holes into the
preceding `.skip` (`careerJobOfferPhaseBias .skip 1869`,
`careerForeignMarketPlayers .skip 18144`), per the recipe convention. The save
treats `careerForeignMarketPlayers` as `MarketPlayer[432]` (= 18144/42), i.e. it
spans both the named 352-entry array **and** the 3360-byte tail, ending exactly
at `&teamSelectionKeyCacheEnd`.

## 3. The three internal adjacencies (all intra-cluster)

### Adjacency A — `careerJobOfferPhaseBias` is cache byte 1

`teamSelectionKeyCache` is `ds.b $1`, then `careerJobOfferPhaseBias` is `ds.b $1`
(SWOS2.s 129250–129252). So `&careerJobOfferPhaseBias == &teamSelectionKeyCache +
1` — it is physically the **`sortKey` byte of the cache's first entry**
(`TeamSelectionKeyCacheEntry.sortKey`, game_types.h:1105). Classic Amiga
memory-reuse: the byte is read as a job-offer phase bias during the offers phase
(career_init.c:1143/1155/1161/1288, career_status.c:1348, job_offers.c:1033) and
overwritten by the cache builder during competition-slot processing — disjoint
career phases.

Persistence: `careerJobOfferPhaseBias` has **no FX entry of its own**. It is
saved purely as **byte 1 of the `team_selection_key_cache` 1870-byte blob**
(save_format_json.c:1306). On a fresh/empty cache (count 0) the blob round-trips
its value; once the cache is built it is (correctly, matching Amiga) part of
entry 0's header.

### Adjacency B — `teamSelectionKeyCacheEnd` is dual-role (bound **and** flag), dual-typed

The same 2-byte symbol is declared two different ways across TUs (the
"extern decl can lie" trap, `project_extern_vs_storage_mismatch`):

- **As `uint8_t teamSelectionKeyCacheEnd[]`** (career_teams.c:36): its *address*
  is the cache's **overflow bound** — `_TeamSelectionKeyCache_InsertSpaceSorted`
  panics if a shifted entry would pass `(uintptr_t)teamSelectionKeyCacheEnd`
  (career_teams.c:309/325). Because the symbol sits *after*
  `careerForeignMarketPlayers`, this bound is `&teamSelectionKeyCache + 20014` —
  i.e. **the cache is permitted to overflow through the foreign-market region**.
- **As `uint16_t teamSelectionKeyCacheEnd`** (career_init/career_status/menus/
  save): a *value* used as a season-transition flag — set `= 1`
  (career_status.c:3367/3401/3435/3702), written `*(uint16_t *)…End = 0`
  (career_teams.c:1573), tested `!= 0` (menus.c:2751/3030), saved `FLD_U16`
  (save_format_json.c:1146).

Both readings reference the *same* word at cluster offset 20022. A standalone
struct keeps it as the last 2-byte member: `&member` = the bound, `member` value
= the flag. Both preserved.

### Adjacency C — the cache overflows into `careerForeignMarketPlayers`

This is the documented bug (`project_cache_overlay_writer_overflow`,
`docs/OVERLAPPED_DATA.md` #8). In career mode (`gameType==4`) the builder
allocates **22-byte slots** (career_teams.c:2748, `slotLen = 0x16`), but the
league fixture-result writer appends **12-byte records** within one slot until a
phase boundary — for an 18-team league this overflows slot *N* into slot *N+1*'s
header, stomping the host-native `entryStride` field at +2 with team-id bytes.
The walkers (`_TeamSelectionKeyCache_GetListPtr` career_teams.c:1422,
`_Career_AdvanceToNextTeamSelectionKeyCacheEntry` 1596, and the save's
`emit_cache_strides` 2720) all carry the **`stride < 10 || stride > 256` clamp**
that freezes the walk at the last sane entry instead of jumping into
foreign-market records. This mitigation replicates Amiga's accidental
"fell-off-into-zeros" behaviour deterministically and is the only reason SDL
doesn't panic where Amiga limps on.

**Consequence for persistence today:** the JSON save captures only the first
`TSKC_SIZE = 1870` bytes of the cache blob (save_format_json.c:2707); a cache
that has genuinely overflowed past 1870 has its tail bytes living in
`careerForeignMarketPlayers`, which is saved separately as typed `MarketPlayer`
objects. So an overflowed cache is already only *partially* round-tripped — the
walkers' stride clamp makes that harmless (season terminates at the corruption
point, matching Amiga). This is pre-existing and is **not** introduced or fixed
by Tier 1.

## 4. Audit result — nothing external depends on cluster placement

Verified by grep across `src/*.c`:

- **No raw `careerFileBuffer + <literal>` access** anywhere in the cluster range.
  Every reference is by named symbol (`teamSelectionKeyCache`,
  `careerForeignMarketPlayers[idx]`, the cursors, the end marker).
- **No external `&otherSymbol + offset` reaches into the cluster**, and nothing
  bounds backward off `competitionFileBuffer` (the symbol immediately after the
  cluster) into it.
- The only `&` uses are `&careerForeignMarketPlayers[idx]` (transfer_market.c:1403/
  1553) — symbol-relative array indexing that follows relocation transparently.
- The save is **entirely `&symbol`-addressed**: cursors as `FX(... FLD_U16)`
  (save_format_json.c:1142–1146); the 1870-byte cache as
  `FA("team_selection_key_cache", teamSelectionKeyCache, 1870, FLD_BYTES)` (1306)
  plus the endian-neutral `cache_strides`/`cache_scores` fixups
  (`emit_cache_strides`/`fixup_cache_strides_on_load`, base = `teamSelectionKeyCache`);
  the foreign market as typed `MarketPlayer[432]` objects
  (`career_foreign_market_players`, 3760/4112, base = `careerForeignMarketPlayers`).

**Therefore the whole cluster can be relocated to standalone storage with zero
consumer edits** — every symbol reference and every save descriptor follows the
new address. The cluster is, in effect, one indivisible region whose internal
overlaps are private to itself.

## 5. Two tiers

This cluster differs from the clean category-A/B/C extractions in one important
way: relocating it **preserves an internal overlap** (the cache↔foreign-market
overflow) rather than producing a tidy independent struct. So the work splits:

### Tier 1 — relocate the cluster intact (recipe pass, low risk)

Move all 8 cluster symbols, at their exact current relative offsets, into a new
`src/career_key_cache.c`, and leave a single `.skip 20024` hole in
`swos_globals_career_overlay.c`.

- New file uses the **real-label `__asm__` BSS block** (not `.equ`; macOS-safe) —
  literally the same 8 `.globl`/label/`.skip` triples currently in the overlay,
  moved verbatim, with `_Static_assert`s pinning the cluster size (20024) and key
  offsets (`teamSelectionKeyCache` at +8, `careerForeignMarketPlayers` at +1878,
  `teamSelectionKeyCacheEnd` at +20022). No `#if UINTPTR_MAX` pad (pointer-free).
- Overlay: replace the 8 triples (lines 326–349) with `.skip 20024` so every
  later label (`competitionFileBuffer` … `numSelectedTeams`) keeps its
  `careerFileBufferBuf` offset. **No-cascade invariant unchanged**:
  `nm` `numSelectedTeams − careerFileBuffer` stays 99092 (64-bit) / 98476 (32-bit).
- Add `career_key_cache.c` to `scripts/sync_to_amiga_port.sh` `SRC_FILES`.
- The three internal adjacencies come along **byte-for-byte** — the relative
  offsets are identical, so `&careerJobOfferPhaseBias == &teamSelectionKeyCache+1`,
  the `&teamSelectionKeyCacheEnd` bound, and the cache→foreign-market overflow
  region are all exactly preserved. **No behaviour change.**

**Save / verify consequence (important, and the same as every prior extraction):**
the cluster moves *out* of the `[careerFileBuffer, numSelectedTeams)` region, so:
- under the **JSON** format it is byte-neutral and round-trips by construction
  (all `&symbol`-addressed) — old JSON saves keep loading;
- it is **no longer covered by `SWOS_SAVE_FORMAT_JSON_VERIFY`** (which only diffs
  the in-buffer region). Cache + foreign-market are exactly the
  "lives-in-a-standalone-buffer, VERIFY-blind" case (handoff lesson #4 /
  `project_verify_expected_mismatches`). So Tier 1's gate must be a **functional
  cross-target test**, not VERIFY=0 alone (see §6);
- the legacy raw `.CAR` dump path would dump zeros for the hole — consistent with
  every prior region already lifted out, and squarely on the step-4 "retire the
  legacy dump" track. Not a regression unique to this change.

Risk: **low** (pure relocation, no representation change). It does **not** fix the
writer-overflow bug; it carries the overlap along intact, now wholly owned and
documented in one file.

### Tier 2 — remove the overlap (the real fix; design + Amiga coordination)

The documented "proper fix" for `project_cache_overlay_writer_overflow`: stop the
cache and `careerForeignMarketPlayers` from sharing memory so the 22-byte-slot
overflow can never corrupt foreign-player data. Two coordinated sub-changes:

1. **Bound the cache.** Determine the real maximum cache footprint (entries ×
   stride) — career mode uses 22-byte slots; the worst case is roughly
   `leagueTotalMatches` (+ cup entries) × 22 across all divisions. If it fits
   well under 1870, give the cache a dedicated, exactly-sized buffer with its
   *own* end marker, and the `InsertSpaceSorted` bound becomes the real cache end
   (a tighter, honest panic) instead of `&teamSelectionKeyCacheEnd` 18 KB away.
   If it can exceed 1870, the writer must advance to the next slot per fixture
   (the other half of the documented fix) so it never needs the overflow space.
2. **Split the symbols** into two independent standalone regions
   (`careerKeyCache` and `careerForeignMarketPlayers`). With the writer fixed, the
   overflow clamp in the three walkers can stay as defence-in-depth but is no
   longer load-bearing.

`careerJobOfferPhaseBias` (Adjacency A) can also be promoted to a true standalone
scalar here — but that **changes the save** (it currently rides as cache byte 1);
needs its own FX entry + a coordinated old-save migration. Optional within Tier 2.

Risk: **medium–high.** Touches save-critical career-progression state, changes
the writer the Amiga port shares, and alters the cache↔foreign-market memory
contract. Needs the cache-size analysis above, a functional cross-target test
(career through a multi-division league season with a populated foreign market +
active offers), and a back-port plan. **This is the step-3→step-4 untangle, and
should not be bundled into Tier 1.**

## 6. Risk / test summary

| Tier | Risk | Test |
|------|------|------|
| 1 (relocate intact) | low (no representation change) | 3-arch build + `nm` no-cascade (99092/98476) **and** functional: a career mid-competition with a populated cache + foreign-market players, save→reload, plus cross-target Amiga load (VERIFY can't see this region) |
| 2 (remove overlap) | medium–high (writer + memory contract, shared with Amiga) | cache-size analysis first; functional multi-division league season + foreign market + offers; cross-target; back-port |

## 7. Sites inventory (for the implementer)

- **Overlay reservation:** `swos_globals_career_overlay.c` 326–349 (the 8 triples
  → one `.skip 20024`).
- **SWOS2.s layout:** 129242–129261 (cluster), original sizes
  `careerForeignMarketPlayers $39C0` + tail `BSS_01_16E082 $D20`.
- **Types:** `game_types.h` 1091–1135 (`TeamSelectionKeyCacheMatchSlot` 12,
  `TeamSelectionKeyCacheEntry` 10, `MarketPlayer` 42).
- **Builder:** career_teams.c `addNextRoundCupEntry` 2665, `addCupEntry` 2696,
  `_TeamSelectionKeyCache_BuildFromCompetitionBlock` 2737 (career-mode `slotLen =
  0x16` at 2748 — the overflow source).
- **Walkers (stride clamp = Adjacency C mitigation):** `_GetListPtr` 1403/1422,
  `_GetCurrentSelectedElementPtr` 1471, `_GetCurrentMatchSlot` 1499,
  `_Career_AdvanceToNextTeamSelectionKeyCacheEntry` 1563/1596,
  `_TeamSelectionKeyCache_InsertSpaceSorted` 290 (bound 309/325 = Adjacency B).
- **Cursors / flag reads:** career_init.c 964/994–997/1149/1236–1239,
  career_status.c 3367/3401/3435/3702/3818/3829/3875/4025, menus.c
  2600/2620/2633/2652/2751/3030/3118/4513/4541/6485, diy_competition.c 2915/3185.
- **`careerJobOfferPhaseBias` (Adjacency A):** career_init.c 1143/1155/1161/1288,
  career_status.c 1348, job_offers.c 1033.
- **Foreign market:** transfer_market.c `_AddCareerPoolPlayer` 1399,
  `_Career_PatchTeamWithForeignMarketPlayers` 1434,
  `applyForeignMarketPlayersToTeams` 1473, builders 1403/1553.
- **Save:** cursors FX 1142–1146; cache blob FA 1306 + `emit_cache_strides`/
  `fixup_cache_strides_on_load` 2710/2753; foreign market typed objects 3760/4112.

---

# Tier 2 detailed design

## 8. Cache-size analysis — how big does the cache really get?

The whole point of Tier 2 is to give the cache a **dedicated, bounded buffer**
so the writer-overflow can corrupt only the cache's own tail (caught by the
existing stride-clamp) and never reach `careerForeignMarketPlayers`. That needs a
defensible size.

### 8.1 What's in the cache

`teamSelectionKeyCache` is built **per competition slot** and **appended** across
all of the player's competitions in a season (e.g. Serie A + Coppa + Champions
Cup), via `_TeamSelectionKeyCache_BuildFromCompetitionBlock` →
`_TeamSelectionKeyCache_InsertSpaceSorted` (`teamSelectionKeyCacheCount++` each
add). Each entry is a 10-byte header; in **career mode (`gameType==4`) every
entry is allocated `slotLen = 0x16 = 22` bytes** (header 10 + one 12-byte match
slot) — see career_teams.c 2748 / 2795 / 2852.

`leagueTotalMatches` (= the league entry count) comes from
`_DIY_RecomputeCompetitionDerivedFields` (diy_competition.c 1720–1755):

```
groupsPerRound         = numTeams / matchPairsPerRound          (= N/2 standard)
halfGroups             = groupsPerRound >> 1
totalMatchesPerSection = (groupsPerRound-1)*groupsPerRound / 2
leagueTotalMatches     = totalMatchesPerSection / halfGroups * playEachTeam
```

| N (teams) | entries (`leagueTotalMatches`, P=2) | player matches = (N-1)·P | bytes ≈ entries·10 + matches·12 |
|---:|---:|---:|---:|
| 16 | 14 | 30 | 500 |
| 18 | 18 | 34 | 588 |
| 20 | 18 | 38 | 636 |
| 22 | 22 | 42 | 724 |
| 24 | 22 | 46 | 772 |

### 8.2 Where the overflow comes from

There are **more records than entries**: the recorder
(`_DIY_RecordLeagueFixtureResultAndAdvanceProgress`, diy_competition.c 3138)
writes a 12-byte record per fixture and does `resultPtr += 0x0C` **within one
entry**, looping while `leagueCurrentSlot == 0` (a section). With ~34 player
matches spread over 18 entries, each entry receives ~1.9 records; the 2nd record
in an entry starts at `entry+0x16` = the next entry's header → ~12 B overflow per
over-full entry. The builder never sized the slot for >1 record. This is the
documented bug (Adjacency C / `project_cache_overlay_writer_overflow`).

So the **occupied footprint** for a single league ≈ `entries·10 + matches·12`
(the table above, ≈0.5–0.8 KB). A heavy career *season* sums the league + a
domestic cup (`addNextRoundCupEntry`, ~1–3 entries × ~6 rounds) + a European cup
(similar), each contributing its own headers + records. Analytical heavy-case
estimate: **≈1.5–2.5 KB** — which is exactly why the original 1870-byte buffer
*sometimes* overflows past its end into the foreign-market region (and why the
Amiga gave it the foreign-market span as accidental slack up to
`&teamSelectionKeyCacheEnd`, +20014).

### 8.3 The measurement to run before sizing (user action)

The analytical estimate is data-dependent (section structure × competitions), so
confirm it empirically before committing to a buffer size. Add a probe (no
behaviour change) that tracks the **high-water mark** of cache occupancy and log
it at each season transition:

- In `_TeamSelectionKeyCache_InsertSpaceSorted` (career_teams.c ~339, after the
  shift) and in the recorder's `resultPtr += 0x0C` site (diy_competition.c 3318),
  compute `extent = (ptr_or_resultPtr - teamSelectionKeyCache)` and keep a
  file-static `size_t s_keyCacheHWM = max(...)`.
- Log `s_keyCacheHWM` (and `teamSelectionKeyCacheCount`) from
  `_Career_FillCurrentSeasonInfo` / the end-of-season path.

Run **several full career seasons**: the biggest available league (24-team), a
multi-competition club career (league + domestic cup + European cup), and a
national-coach World Cup career. The largest HWM across all of them, **rounded up
and doubled for margin**, is the dedicated buffer size (expect a 4 KB or 8 KB
round number to be ample). Sizing the buffer so its end marker is the true
`InsertSpaceSorted` panic bound means a genuine overflow now **panics loudly**
(per CLAUDE.md "panics are signals") instead of silently eating player data —
*provided* the measured HWM is comfortably below it.

## 9. Tier 2 — the separation (recommended approach)

The minimal change that removes the overlap **without** touching the shared Amiga
writer:

1. **Two independent standalone symbols**, in their own files, sized cleanly:
   - `src/career_key_cache.c` — the cursors + a dedicated cache buffer +
     `teamSelectionKeyCacheEnd`, as one struct. Buffer size = the §8.3 measured
     value (fallback: keep the historical 20014 slack if measurement is
     impractical — still a win, foreign-market is freed either way).
   - `src/career_foreign_market.c` — `MarketPlayer careerForeignMarketPlayers[432]`
     (18144 B; the named 352 + the 80-slot tail are genuinely one 432-element
     pool — confirmed by the `0x1B0`-based refill quota at career_init.c 119).
2. **The overflow bound becomes the cache's own end.** `InsertSpaceSorted`
   (career_teams.c 309/325) already compares against `&teamSelectionKeyCacheEnd`;
   with `teamSelectionKeyCacheEnd` now sitting immediately after the dedicated
   buffer, that bound is honest and tight. The `stride < 10 || stride > 256`
   clamps in the three walkers (`_GetListPtr` 1422,
   `_Career_AdvanceToNext...` 1596, `emit_cache_strides` 2723) stay as
   defence-in-depth but stop being load-bearing for player-data safety — an
   overflow now stays inside the cache buffer.
3. **`teamSelectionKeyCacheEnd` stays the struct's last member** so both its
   readings survive: `&end` = the bound (Adjacency B address use), `end`-value =
   the season flag. Keep the dual `extern` decls or, better, unify them — the
   `uint8_t[]` decl in career_teams.c:36 only exists to take the address; it can
   become `extern uint16_t teamSelectionKeyCacheEnd;` with the two bound sites
   using `&teamSelectionKeyCacheEnd` explicitly (clearer, removes the
   extern-mismatch footgun, `project_extern_vs_storage_mismatch`).
4. **`careerJobOfferPhaseBias` (Adjacency A) — promote to a real scalar.** Today
   it is cache byte 1, so *building the cache overwrites it* (harmless only
   because the read happens in a disjoint phase — a latent footgun). Give it its
   own `uint8_t careerJobOfferPhaseBias;` (next to the other career scalars) and
   its own save descriptor. **Save-format implication:** it currently rides as
   byte 1 of the `team_selection_key_cache` 1870-byte blob; promoting it needs a
   new `FX("career_job_offer_phase_bias", …, FLD_U8)` and old JSON saves won't
   carry it (it defaults to 0 on load — acceptable, it is re-derived early in
   `_InitCareer`, career_init.c 1143/1155). Flag this to the user per
   `feedback_save_format_changes` before shipping.

**What this deliberately does *not* do:** redesign the writer to size slots for
>1 record. Separating the buffers already stops the cross-region corruption; the
intra-cache overflow remains, contained and clamped. A full writer fix (allocate
`slotLen` from the real records-per-entry, or advance to a fresh entry per
record) is a *further* cleanup that touches the Amiga-shared
builder/recorder and is best deferred to its own change with the §8.3 data in
hand. Note it as a follow-up; it is not required to free foreign-market.

## 10. Implementation order (once §8.3 size is known)

1. **Promote `careerJobOfferPhaseBias`** to a standalone scalar + save descriptor
   (independent, ship/verify first; functional check: job offers still appear
   with correct phase across a season; old-save load defaults cleanly).
2. **Split `careerForeignMarketPlayers`** into `career_foreign_market.c`
   (`MarketPlayer[432]`) via the standard recipe — `.skip 18144` hole in the
   overlay. Functional check: foreign-market signings still patch into teams.
3. **Move the cache + cursors + end marker** into `career_key_cache.c` with the
   dedicated buffer sized per §8.3, end marker last; unify the
   `teamSelectionKeyCacheEnd` extern. `.skip` the remaining cluster bytes in the
   overlay.
4. After all three, the overlay's `teamSelectionKeyCache` cluster is a single
   `.skip` hole; **no-cascade invariant must hold** (`nm`
   `numSelectedTeams − careerFileBuffer` = 99092 / 98476) at every step.
5. Each step: 3-arch build (`make clean` between) + the functional cross-target
   test (a career mid-competition with a populated cache **and** a populated
   foreign market, save→reload on SDL, then load on Amiga BE). VERIFY=0 is
   necessary but **not sufficient** — these regions leave the verify window
   (handoff lesson #4). Sync each new file into
   `scripts/sync_to_amiga_port.sh` `SRC_FILES`.

## 11. Risk / test summary (Tier 2)

| Step | Risk | Test |
|------|------|------|
| 1 (promote phase-bias) | low–medium (save change) | job-offer phase across a season; old-JSON-save loads (defaults 0); cross-target |
| 2 (split foreign market) | low (recipe pass, `&symbol`) | foreign signings patch into teams; 3-arch no-cascade; functional reload |
| 3 (dedicated cache buffer) | medium (bound becomes real) | full season w/ heavy multi-competition cache; confirm HWM ≪ buffer (no spurious `InsertSpaceSorted` panic); cross-target Amiga load |
| writer fix (deferred) | high (Amiga-shared builder/recorder) | out of scope here; revisit with §8.3 data |
