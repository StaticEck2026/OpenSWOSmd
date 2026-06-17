# Stage-2 cross-target verification — `leagueStandings` field move (8f14738)

## What this verifies

Roadmap step-3 stage-1 (`8f14738`) moved the league/group standings out of the
cup `bracketScratch` overlay into a real trailing field,
`DIYCompetitionEx.leagueStandings[64]` (Amiga `+0x2B4`/`+0x420` →
`DIY_STANDINGS_OFFSET`). The on-paper coverage audit
(`project_leaguestandings_stage2_audit`) found **no code gap** — every standings
access goes through `DIY_STANDINGS_OFFSET` / `diy_ovr_row` / `leagueStandings[]`,
and both save pairs (`.CAR`/`.SEA` career, `.PRE`/`.DIY` preset) serialize the
rows as typed, endian-neutral, named fields with a target-independent **team
identity** (`-1` = career team, else the `selected_teams` index).

What remains is **functional** confirmation: a save written by the 64-bit LE SDL
build must load on the 32-bit **big-endian Amiga** build with the standings
intact (no byte-swap, no `0x5C8→0x5D0` 32↔64 offset shift, every team resolving
to the right club rather than collapsing onto `selected_teams[0]`/career team).

The standings field reaches the save through three distinct paths, so each needs
one fresh career:

| # | Competition                         | JSON location                          | Status |
|---|-------------------------------------|----------------------------------------|--------|
| 1 | Primary **domestic LEAGUE**         | `competition_overrides.league_rows`    | save exists |
| 2 | Primary **national WC/Euro GROUP**  | top-level `stage_standings`            | needs save |
| 3 | Per-context **club-cup GROUP**      | `<ctx>_comp_slot_stage_standings`      | needs save |

Path 2's identity is reconstructed on load as the row index `i` (national teams
sit in `selTeamsPtr` order). Paths 1 and 3 serialize the explicit identity
because the rows are a subset / sort in place. The historical bug class this
guards against (`aba1a9b`): the int32 `teamSelOffset` rode a raw blob and
byte-swapped LE→BE, so an entire Champions-Cup group table showed the career
team in every row.

## The tool: `scripts/decode_save.py --standings`

The JSON save is target-neutral text, so decoding it tells you exactly what the
SDL build wrote — the **expected** table. The decoder resolves each row's stored
team identity to a name and prints the league/group table the way the in-game
screen shows it (single-group league sorted by points → GD → GF → name, matching
`_DIY_SortLeagueTableByStandings`; multi-group comps in storage order — match
those by team name).

```
scripts/decode_save.py --standings bin/save/MILAN5.CAR   # league/group tables
scripts/decode_save.py            bin/save/MILAN5.CAR    # full slot dump too
```

Use it three ways:
1. **Gate** a freshly-created save — confirm the right standings table is present
   (so the career actually reached the phase you wanted) before touching the Amiga.
2. **Expected reference** — print the table, then eyeball the Amiga standings
   screen against it.
3. **Objective round-trip** — load the SDL save on the Amiga, *save it again*
   there, copy it back, and diff the two decodes:
   ```
   diff <(scripts/decode_save.py --standings SDL_SAVE.CAR) \
        <(scripts/decode_save.py --standings AMIGA_RESAVE.CAR)
   ```
   Identical standings ⇒ the round-trip is byte-faithful, no human eyeballing
   needed.

## Pass / fail signatures

**PASS** — the Amiga standings screen lists the same teams with the same
GP/W/D/L/GF/GA/Pts as the decoded table; the career team (`*`) is where you
expect; the Amiga re-save's `--standings` diff is empty.

**FAIL** — any of:
- Every (or most) rows show the **same team name** — the classic
  identity-byte-swap collapse onto `selected_teams[0]` / career team.
- Stats are wildly large (e.g. `768`, `256`, `>200` games) — a byte-swapped /
  overlay-corrupt row. `decode_save.py` (default mode) flags these as `GARBAGE`.
- A team's stats are right but attached to the **wrong club** — identity
  resolved against a shifted base.

## Scenario 1 — domestic LEAGUE (ready now)

**Save:** `bin/save/MILAN5.CAR` (or `MILAN.CAR`) already covers this — primary
type-0 `ITALIAN SERIE A`, 18 teams, full season. No new career needed.

**Expected table** (`scripts/decode_save.py --standings bin/save/MILAN5.CAR`,
career team INTER MILAN):

```
  LEAGUE TABLE — ITALIAN SERIE A  (primary, type-0)   * = your career team
    Pos  Team                       GP   W   D   L  GF  GA  GD Pts
      1  AC MILAN                  34  23   9   2  61  20  +41  78
      2  ROMA                      34  24   5   5  56  23  +33  77
      3  LAZIO                     34  23   7   4  71  33  +38  76
      4  PARMA                     34  17   9   8  46  27  +19  60
      5  NAPOLI                    34  16  10   8  47  37  +10  58
      6  INTER MILAN *             34  16   9   9  42  32  +10  57
      ...                                              (18 teams)
```

**Amiga steps:** copy `MILAN5.CAR` into the Amiga build's save dir → CONTINUE
CAREER / load it → open the league table screen → compare to the table above.
For the objective check, save the career on the Amiga, copy it back, and run the
`diff <(... SDL) <(... AMIGA)` recipe.

## Scenario 2 — national WC/Euro GROUP phase (needs a save)

**Goal state:** a career managing a **national team**, advanced into the World
Cup or European Championship, **stopped during the group phase** (after at least
one group match, before the group completes), then SAVE.

**Fast path:** `make … SWOS_INJECT_WC=1` jumps an already-started national career
toward the World Cup (it only mutates a career that is already national — you
still start the national career manually).

**Gate the save:**
```
scripts/decode_save.py --standings YOURSAVE.CAR
```
You want a `GROUP STANDINGS — primary tournament (type-2)` table. If it prints
"no standings tables" you're in the knockout (or pre-group) phase — keep
playing / re-save during the group phase. Path 2 rows have no stored id (`row i
→ selected_teams[i]`), so verify **by row order**: the Amiga group screen,
position-for-position, must match the decoded stats.

**Amiga steps:** load on the Amiga → group standings screen → compare. Then the
re-save diff for the objective check.

## Scenario 3 — club-cup GROUP phase (needs a save)

**Goal state:** a **club** career in a European cup that has a group stage (the
Champions Cup), advanced into its **group phase**, SAVE during the group stage.

**Gate the save:**
```
scripts/decode_save.py --standings YOURSAVE.CAR
```
You want a `GROUP STANDINGS — <ctx> context (type-2 club cup)` table (e.g.
`euro_comp_slot_stage`). These carry the explicit identity, so the decoded names
should match the Amiga screen club-for-club — this is the exact path the
`aba1a9b` collapse-onto-`selected_teams[0]` bug lived in, so it's the most
important of the three.

**Amiga steps:** load on the Amiga → the club cup's group table → compare
club-by-club. Then the re-save diff.

## Why creation is user-driven

WSL has no input injector (xdotool / wtype / ydotool all absent), no save-load
argv, and no auto-load env, and `SWOS_INJECT_WC` only mutates an
already-started national career — so a *fresh* career in a specific phase can't
be produced headlessly. Create the saves on your Cygwin build (or under WSLg),
gate each with `decode_save.py --standings`, and drive the Amiga BE load. The
decoder + this protocol make each check a few minutes of eyeballing (or a single
`diff` for the objective version).

See also `docs/SAVE_DATA.md` and `project_leaguestandings_stage2_audit`.
