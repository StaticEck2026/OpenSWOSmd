# MY CUSTOM TEAM — a team of your own for online play

> **Status: current** (2026-10-02). SDL builds only.

Three slots. A custom team is **a team of the database, renamed and redressed**: its owner
may change

- the team's name and the coach's,
- each player's name and face,
- both kits,
- the line-up and the formation it starts in,

and nothing else. **No skill, position, shirt number, nationality or value can be changed** —
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
- Each player is "the database team's player `base`", with a name and a face (0-2).
- `lineup` is the team's `playerNumbers`: which player is in each of the sixteen places.
- `shirt` is `plain`, `sleeves`, `stripes` or `hoops`; a colour is `grey`, `white`, `black`,
  `orange`, `red`, `blue`, `brown`, `light_blue`, `green` or `yellow`. Numbers are read too.

Loading reads the database team and applies those (`load_json`, `src/custom_team.c`). A file
that names a player twice or not at all, a line-up that is not sixteen different places, a
colour that does not exist: each falls back to what the database has. Editing the file by hand
can rename a player; it cannot make him better.

**A SWOS 2020 `.TEAM` file** (the raw 684-byte team record) put in the save folder under a
slot's name is read by the same rule: its names, faces, kits and line-up are taken, and it is
refused if its players are not the database team's (`load_record`). Note what that means for
files made in SWOS 2020's online mode: those were made from SWOS 2020's *own* online team
database, whose teams and skills are not the 96/97 ones — such a file names a team number that
is a different team here, and is refused.

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
each — with the same nationality, shirt number, position, skills and value. Names, faces, kits,
line-up and formation are free. A team that fails has not been picked as far as this machine is
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
players swapped in the line-up and two in the record, it is still allowed, and comes back from
the file's text as exactly the team the rule makes of it. `SWOS_DATA_JSON=0` runs it on the
binary database.

`--menu=swos2020/selectMyCustomTeams` opens the slots, and what choosing one does, without the
matchmaking server.
