# scratch/ — one-shot probes

Each file here drives the game under the gdb harness (`scripts/swos_plugin.py`,
docs/TESTS.md) to answer ONE question: reproduce a tester's report, dump a
struct at a breakpoint, watch a buffer across a save/reload. They are written
fast, run a handful of times, and then the ANSWER is what matters — it goes into
docs/, into a fix, or into a test. The script itself is disposable.

So: **nothing here is maintained**, and nothing else may import from it. A probe
that stops working is deleted, not repaired. Anything worth keeping working
belongs in `scripts/` as a real tool with a `--help`.

Only `copa_drive.py` and `wc_drive2.py` are committed — several docs cite them as
the recipe for driving a career to a given screen. The rest are gitignored.

Run them from the REPO ROOT: most do `sys.path.insert(0, "scripts")`, which is
relative, so `python3 scratch/foo.py` works and `cd scratch && python3 foo.py`
does not.

Still tied to open work, as of 2026-09-16:

| probe | question |
|---|---|
| `mp_lobby_live.py` | MULTIPLAYER → login → lobby with a real opponent, in ONE harness run — a peer stopped at a breakpoint sends no ENET keepalives, so splitting it across runs lets the server drop the connection in the gap |
| `mp_menu_drive.py` | driving the multiplayer menus |
| `wc_grid_probe.py`, `wc_qf_trace.py`, `wc_bronze_probe.py` | the World Cup knockout bracket — the QF winner not carried into the semi-final is still open |
