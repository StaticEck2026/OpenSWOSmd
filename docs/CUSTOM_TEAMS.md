# MY CUSTOM TEAM — a team of your own for online play

> **Status: current** (2026-10-02). SDL builds only.

Three slots. A custom team is **a team of the database, renamed and redressed**: its owner
may change

- the team's name and the coach's,
- each player's name, face, nationality and shirt number,
- both kits,
- the line-up and the formation it starts in,

and nothing else. **No player's position, skills or value can be changed** —
that is the rule the whole thing is built round, and it is kept in three places, so that no
one of them has to be trusted.

## 1. The file has nowhere to put a skill

`CUSTOM1.TEAM` … `CUSTOM3.TEAM` in the save folder. JSON, like a career save is:

```json
{
  "format": "openswos-team",
  "version": 1,
  "base": { "country": 14, "ordinal": 10, "name": "FORT. DUSSELDORF" },
  "name": "BORUSSIA FULDA",
  "coach": "MARTIN HOHMANN",
  "tactics": 4,
  "first_kit": { "shirt": "stripes", "colour": "red", "second_colour": "white", "shorts": "white", "socks": "white" },
  "second_kit": { "shirt": "plain", "colour": "green", "second_colour": "green", "shorts": "white", "socks": "white" },
  "lineup": [0, 2, 4, 8, 3, 6, 9, 10, 7, 13, 14, 1, 5, 11, 12, 15],
  "players": [
    { "base": 0, "name": "GEORG KOCH", "face": 1 },
    …sixteen of them…
  ]
}
```

- `base` names the database team (`country` is the team file's number, `ordinal` the team's
  number in it; `name` is there for whoever reads the file and is not used).
- Each player is "the database team's player `base`", with a name, a face (0-2), a
  nationality (0-154, an index into the game's country list) and a shirt number (1-99).
- `lineup` is the team's `playerNumbers`: which player is in each of the sixteen places.
- `shirt` is `plain`, `sleeves`, `stripes` or `hoops`; a colour is `grey`, `white`, `black`,
  `orange`, `red`, `blue`, `brown`, `light_blue`, `green` or `yellow`. Numbers are read too.

Loading reads the database team and applies those (`load_json`, `src/custom_team.c`). A file
that names a player twice or not at all, a line-up that is not sixteen different places, a
colour that does not exist: each falls back to what the database has. Editing the file by hand
can rename a player; it cannot make him better.

**A `.TEAM` file** — the raw 684-byte team record SWOS 2020 and the team editors use — put
in the save folder under a slot's name (`CUSTOM1.TEAM` …) is **converted** the first time the
slot is read (`load_record`): the database team it was made from is found **by its players**,
everything a custom team may change is taken from it, and the slot is rewritten as the JSON
above, the original kept beside it as `CUSTOMn.TEAM.684`. The number inside the record is no
guide — SWOS 2020 gives a custom team a number of its own: Playaveli's `BORUSSIA FULDA` says
country 72, team 17 (GAMESMASTER here) and is BRAZIL (country 82, team 2) with its names,
nationalities, numbers, faces, kits and line-up changed. A record whose sixteen players are
not, with their positions, skills and values, the players of one database team is refused.

`bin/SWOS.x86_64 --convert-teams=<in>,<out>` runs a folder of them through the same
conversion, writing each under its own name (`.TEAM`, JSON inside, as a career is JSON in a
`.CAR`), reading each back to require the same team, and naming, for any it refuses, the
database team it comes closest to. On 2,522 community `.TEAM` files (2026-10-02): 2,519
converted, every one loading back identical — 808 of them carried a number that is not their
base team's, so only finding the base by its players could convert them — and 3 refused:
ARGENTINA 1978, GERMANY 1992 and M.F.T., historical squads whose players are in no team of the
96/97 database (1 of 16 match the closest). The bases most made from: ITALY 660, BRAZIL 406,
REAL MADRID 228, AC MILAN 163, BARCELONA 156.

## 2. The editor cannot edit one

MY CUSTOM TEAM on the matchmaking menu opens the three slots (SWOS 2020's
`selectMyCustomTeams` menu). Choosing an empty one asks for a team of the database first
(clubs and national sides); choosing a filled one edits it.

The editor is the game's own EDIT TEAM screen (`_editTeamsMenu`), which never could edit
skills, with two changes while a custom team is on it (`CustomTeam_Editing()`,
`src/edit_tactics.c`), both as SWOS 2020's custom team editor has them:

- **IMPORT** takes another database team whole — its number included, since that is what the
  team's players are checked against — and stays available afterwards.
- **UNDO is LINEUP**: the team screen a match shows (`CustomTeam_Lineup`), where FIRE on two
  players swaps them and the formation is chosen. CONFIRM keeps it, EXIT drops it.

OK saves the slot. What is saved is rebuilt from the database team (`write_json` refuses a
team that is not one).

## 3. A team from the other player is checked

`CustomTeam_Allowed`, called where the lobby reads the other player's team
(`read_peer_state`, `src/netplay_lobby.c`): the team's number must be a team of the database
here, and each of its sixteen players must be one of that team's players — a different one
each — with the same position, skills and value. Names, faces, nationalities, shirt numbers,
kits, line-up and formation are free. (Shirt numbers have to be: the team screen swaps two
players' numbers when it swaps a starter and a substitute. The first version of this check,
in the `9f838fc` drop, compared them, and so refused any team arranged that way.) A team that fails has not been picked as far as this machine is
concerned: the other player shows as having no team, the lobby says YOUR OPPONENT'S TEAM IS NOT
A DATABASE TEAM, and a match cannot start. That covers every team, custom or not.

It reads the database into a buffer of its own (the JSON database, or the binary `TEAM.nnn`
where a country is not in it), because the lobby is polled from inside whatever menu the
player is in and the game's own team buffer belongs to that menu. The last answer is kept, as
the lobby asks several times a second.

Runtime fields a team screen writes (the cached match value, form, cards and injuries, goal
counters) are not part of the check: they differ between a team fresh from the list and one
that has been through the team screen.

## Picking one for a match

The lobby's TEAMS button: a player who has custom teams is shown the three slots first, with
OTHER TEAMS for the game's own list; a player with none goes straight to the list, as before
(`CustomTeam_Pick`).

## Checking it

`bin/SWOS.x86_64 --custom-team-selftest` takes every team of the database (1,631) and requires:
it is allowed as it is; it is refused with one skill changed; renamed, redressed, with two
players swapped in the line-up and two in the record, two shirt numbers swapped and a
nationality and a number changed, it is still allowed, and comes back from the file's text as
exactly the team the rule makes of it. `JOIN_ENV="SWOS_LOBBY_LINEUP_SWAP=3:12"
scripts/relay_test.sh` plays a match whose joiner swapped a starter and a substitute. `SWOS_DATA_JSON=0` runs it on the
binary database.

`--menu=swos2020/selectMyCustomTeams` opens the slots, and what choosing one does, without the
matchmaking server.
