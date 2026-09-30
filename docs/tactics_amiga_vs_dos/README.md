# Amiga vs DOS tactic (formation) data

> **Note (2026-06-29) — historical snapshot.** This data reflects the *original*
> Amiga formations. Tester-supplied `.TAC` fixes were later applied (commit
> `23f0c59`) to **4-4-2, Sweep, 4-3-3, 3-4-3** (buggy ball positions — CDs /
> attackers flipping sides). The shipped formations now live in
> `bin/data_json/tactics/*.json` and diverge from the "amiga" column here for
> those four. This snapshot is kept as the record of the original Amiga-vs-DOS
> delta, not of the current data.
>
> **Note (2026-09-03).** Most of the 68 / 64 bytes below are in slots 8 and 9 of
> 4-4-2 and Sweep, which neither release executes (`InitUserTactics` copies the
> 5-3-2's last two rows over them at startup). The *live* difference is 21 bytes
> — see `docs/TACTICS_POSITION_VALUE.md` §9.3 ("Retracted"). The DOS coverage is
> buildable as `make SWOS_DOS_TACTICS=1`, from `bin/data_json/tactics/<id>_dos.json`
> (§9.6).

The SWOS engine + team files are identical across DOS and Amiga; the only
gameplay-affecting data that differs between the two releases is the **default
formation coverage** (`TeamTactics`). This folder is the proof + the raw data.

"amiga" here = this port's `tactics_data.c`, which is **byte-identical to the
real Amiga** `SWOS2.s` `teamTactics*` (verified, all 12 formations). "dos" =
`swos.asm` `tact_*` (SWOS 96/97).

## Files
- **tactics_diff_summary.csv** — one row per formation: how many name/coverage
  bytes differ, and whether it's identical.
- **tactics_diff_detail.csv** — one row per *differing byte*, fully decoded:
  `formation, section, player_slot, ball_zone, zone_quadrant, amiga_hex/x/y, dos_hex/x/y`.
  (`x` = high nibble, `y` = low nibble of the packed coordinate.)
- **tactics_amiga.json / tactics_dos.json** — the full formation data for each
  side (10 players × 5 ball-zones × 7 quadrants), for side-by-side import.

## Result
Only **3 of 12** formations were re-tuned for DOS: **4-4-2** (2 name + 68
coverage bytes), **4-3-3** (1 + 10), **Sweep** (0 + 64). The other 9 are
byte-identical. In 4-4-2 the two central-defender slots are swapped at several
ball positions — that's the Aldair/Ronaldão tick/cross difference.
