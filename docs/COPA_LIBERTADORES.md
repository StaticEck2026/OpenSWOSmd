# Copa Libertadores — design

Design for adding the **Copa Libertadores** (CONMEBOL's Champions-League
equivalent) to OpenSWOS, modelled on the **1996 edition**
(https://en.wikipedia.org/wiki/1996_Copa_Libertadores). See `docs/FEATURES.md` §1
for how it fits the bigger picture; this doc is the detailed competition design.

Reference mock-ups (the intended SWOS screens):
`D:\Projects\SensibleSoccer\OpenSWOS\Copa Libertadores\Bildschirmfoto_*_9.04.18_PM.png`
(competition summary) and `*_9.08.20_PM.png` (the 20-team list).

## 1. Format (from the SWOS summary mock-up)

```
COPA LIBERTADORES         SEASON  MAR – JUN     AWAY GOALS  OFF
SUBSTITUTES  3  FROM  5

 TEAMS   SEEDED                  DESCRIPTION
  20   5 GROUPS OF 4   3 PTS FOR A WIN   PLAY EACH TEAM ×2
          ↓  (TOP 3 IN EACH GROUP AND BEST RUNNER-UP QUALIFY)
  16   KNOCKOUT        2 LEGS   NO EXTRA TIME   PENALTIES
          ↓  (MATCH WINNERS ON AGGREGATE QUALIFY)
   8   QUARTER-FINAL   2 LEGS   NO EXTRA TIME   PENALTIES
          ↓
   4   SEMI-FINAL      2 LEGS   NO EXTRA TIME   PENALTIES
          ↓
   2   FINAL           2 LEGS   NO EXTRA TIME   PENALTIES
              (MATCH WINNER ON AGGREGATE WINS TOURNAMENT)
```

- **Group stage**: 20 teams, **5 groups of 4**, double round-robin (**play each
  team twice**, home & away), **3 pts** for a win. Top 3 of each group (15) **+ 1**
  → **16** into the knockout.
- **Knockout**: round of 16 → quarter-finals → semi-finals → final. **All
  two-legged**, no extra time, decided on **aggregate**, **away goals OFF**,
  **penalties** if level on aggregate after both legs.
- Season window MAR–JUN; subs 3 from 5.

> ⚠ **The "16th team" needs clarifying.** Top-3-of-5-groups is only 15; the
> bracket needs 16. The mock-up says "**BEST RUNNER-UP**", but runners-up are
> already inside the top 3 — so the extra qualifier must be the **best
> 4th-placed team** across the groups (this is what the real 1996 tournament did
> to reach 16). Treat "best runner-up" as a wording slip for **best 4th-placed**,
> or confirm the intended rule. (Real-1996 also kept the holder Grêmio out of the
> groups; the SWOS edition uses a clean 20-in-groups instead.)

## 2. The 20 teams (1996 edition)

From the team-list mock-up (alphabetical) with country + SWOS country code:

| # | Team (SWOS label) | Country | CN_ code |
|---|---|---|---|
| 1 | AMERICA CALI | Colombia | `CN_COLOMBIA` |
| 2 | ATLETICO JUNIOR | Colombia | `CN_COLOMBIA` |
| 3 | BARCELONA | Ecuador | `CN_ECUADOR` |
| 4 | BOTAFOGO | Brazil | `CN_BRAZIL` |
| 5 | CARACAS | Venezuela | `CN_VENEZUELA` |
| 6 | CERRO PORTENO | Paraguay | `CN_PARAGUAY` |
| 7 | CORINTHIANS | Brazil | `CN_BRAZIL` |
| 8 | DEFENSOR SPORT | Uruguay | `CN_URUGUAY` |
| 9 | ESPOLI | Ecuador | `CN_ECUADOR` |
| 10 | GUABIRA | Bolivia | `CN_BOLIVIA` |
| 11 | MINERVEN | Venezuela | `CN_VENEZUELA` |
| 12 | OLIMPIA | Paraguay | `CN_PARAGUAY` |
| 13 | PENAROL | Uruguay | `CN_URUGUAY` |
| 14 | RIVER PLATE | Argentina | `CN_ARGENTINA` |
| 15 | SAN JOSE ORURO | Bolivia | `CN_BOLIVIA` |
| 16 | SAN LORENZO | Argentina | `CN_ARGENTINA` |
| 17 | SPORTING CRISTAL | Peru | `CN_PERU` |
| 18 | UNIV. CATOLICA | Chile | `CN_CHILE` |
| 19 | UNIV. DE CHILE | Chile | `CN_CHILE` |
| 20 | UNIVERSITARIO | Peru | `CN_PERU` |

All 10 nations already have SWOS country codes (`game_types.h`: `CN_ARGENTINA`=43,
`CN_BOLIVIA`=45, `CN_BRAZIL`=46, `CN_CHILE`=48, plus `CN_COLOMBIA`, `CN_ECUADOR`,
`CN_PARAGUAY`, `CN_PERU`, `CN_URUGUAY`, `CN_VENEZUELA`).

## 3. Group draw — how teams sort into A–E (the key rule)

Per the 1996 tournament, each group **pairs two associations, 2 teams each**
(intra-confederation seeding so the two clubs from a country are split across…
no — both clubs of a nation go *into the same group*, paired with another
nation's two clubs). The 1996 pairings:

| Group | Nation 1 (2 teams) | Nation 2 (2 teams) |
|---|---|---|
| **A** | Ecuador — BARCELONA, ESPOLI | Paraguay — CERRO PORTENO, OLIMPIA |
| **B** | Uruguay — PENAROL, DEFENSOR SPORT | Peru — SPORTING CRISTAL, UNIVERSITARIO |
| **C** | Colombia — AMERICA CALI, ATLETICO JUNIOR | Bolivia — SAN JOSE ORURO, GUABIRA |
| **D** | Brazil — CORINTHIANS, BOTAFOGO | Chile — UNIV. DE CHILE, UNIV. CATOLICA |
| **E** | Argentina — RIVER PLATE, SAN LORENZO | Venezuela — MINERVEN, CARACAS |

**Rule for the engine:** a group = `{ nationX.team1, nationX.team2,
nationY.team1, nationY.team2 }`. Two implementation options:
1. **Fixed table** — hardcode the five A–E nation-pairings above (simplest;
   exact 1996 reproduction). This is the recommended starting point.
2. **General association-pairing draw** — given 10 nations each contributing 2
   clubs, pair nations into 5 groups (by a seeding/region rule, or random). Needed
   only if the lineup is to be regenerated (e.g. a career CONMEBOL cup drawing
   from current South-American league finishers — see §6).

Within a group the double round-robin is the standard SWOS league mini-table
(3 pts/win), then rank 1/2/3 + best 4th → seeded into the round of 16.

## 4. SWOS engine mapping

Copa Libertadores is a **group phase + knockout** competition — it does **not**
fit the pure-knockout `euroCup`/`cupWinnersCup`/`uefaCup` model. It maps to the
**"type-2" tournament machinery** (the World Cup path), which already does groups
→ knockout:

- The World Cup is implemented via the type-2 tournament state
  (`DIYTournamentState`, `diy_stage_is_league` slot-type gate, `worldCup2Copy` +
  `worldCupQualifyingGroup1..8Copy` in `career_comp_copies.c`; group standings +
  knockout bracket). Reuse this for the group phase + bracket.
- **Hybrid wrinkle:** the World Cup knockout is **single-leg**; Copa Libertadores
  knockout is **two-legged on aggregate** (like the euro cups). So combine: the
  type-2 **group phase** + the euro-cup-style **two-legged aggregate knockout**
  (`career_eurocups.c` handles 2-legged aggregate ties, away-goals toggle,
  penalties). This is the one piece that needs genuinely new wiring.
- **Competition record**: add a `copaLibertadores` definition (type-2 tournament)
  in `competition_data.c` with the 20-team seed list and 5×4 group layout; a
  per-career `*Copy` if it becomes a career competition (register in
  `careerContests[]`, cap 35).
- **Group config**: 5 groups of 4, double round-robin, 3 pts/win, 3+1 advance.
  The type-2 path is parameterised for group count / teams-per-group / advancers —
  verify it supports 5×4 with "top-3 + best-4th" (WC uses different numbers).
- **Knockout seeding**: per the 1996 rule, seeds 1/5/9/13/17 = group winners,
  2/6/10/14/18 = runners-up, 3/7/11/15/19 = thirds (+ the 16th). Keep it simple
  first (winners vs lower seeds); the real "no two clubs of the same nation meet
  before QF" constraint is optional polish.

## 5. Data, strings, assets

- **Team data**: the 20 South-American **club** teams need `TeamFile` records
  (SWOS ships South-American *national* teams, not these clubs). Add them — ideally
  via the **data-drive** (`docs/FEATURES.md` §3 / `DATA_JSON.md`) so they're JSON,
  not compiled. 20 teams fits `tmdFileBuffer = TeamFileEx[80]`.
- **Strings**: team labels (table §2), `aCopaLibertadores` title, group labels
  A–E, "BEST RUNNER-UP"/round captions → `strings_data.c` + `docs/TRANSLATIONS.md`
  keys.
- **Match settings**: away goals OFF, 2-legged aggregate, no extra time,
  penalties, subs 3-from-5 — set on the competition record.
- **Kits/graphics**: club kit colours per `TeamFile`; no new sprite work beyond
  the team data.

## 6. Optional: as a career continental cup

Beyond the standalone 1996 preset, Copa Libertadores could become a **career
CONMEBOL cup** (the South-American analogue of the career euro cups): each season
the South-American league champions/runners-up auto-qualify and are drawn into the
five groups by the §3 association-pairing rule. This depends on:
- South-American **club leagues** existing in the data (career euro cups draw from
  European league finishers; the equivalent CONMEBOL leagues must exist — another
  data-drive item).
- Generalising `career_eurocups.c` qualification to be **confederation-
  parameterised** (Europe → euro cups; South America → Libertadores).
`competition_data.c:1098` already has the CONMEBOL nation list
(`{CN_ARGENTINA, CN_BOLIVIA, CN_BRAZIL, CN_CHILE, CN_COLOMBIA, CN_ECUADOR,
CN_PARAGUAY, CN_PERU, CN_SURINAM, CN_URUGUAY, CN_VENEZUELA}`).

## 7. Implementation phasing

1. **Team data**: add the 20 club `TeamFile`s (via data-drive).
2. **Competition record**: `copaLibertadores` type-2 tournament, 5×4 groups, fixed
   A–E pairing (§3 option 1), 20-team seed list.
3. **Group phase**: reuse type-2 group standings; confirm 5×4 + "top-3 + best-4th".
4. **Two-legged knockout**: combine type-2 bracket with euro-cup aggregate ties
   (away goals OFF, penalties).
5. **Screens/strings**: summary screen (§1) + team list + TRANSLATIONS keys.
6. **(Later)** career CONMEBOL cup (§6) once South-American leagues are data-driven.

## Open questions
- The **16th qualifier**: confirm "best runner-up" really means **best 4th-placed**
  (to reach 16 from 5 groups). (Doc assumes best 4th.)
- Group draw: ship the **fixed 1996 pairing**, or a general association-pairing
  draw for replayability / career use?
- Knockout: enforce the "no same-nation clubs before QF" seeding constraint, or
  keep a plain seeded bracket?
- Standalone preset only, or also the career continental cup (§6) now?
