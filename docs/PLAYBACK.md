# PLAYBACK.md — the recordings player

`https://us.swos.eu/playback/` lists every match the server has recorded (RECORD MATCHES,
docs/NETPLAY.md §7 "The recorder") and plays any of them in the browser under a scrub bar,
so a moment a tester describes -- "the first corner kick, the short pass after it" -- can
be found and named by its **frame number**. The frame is what everything else works in:
`--srp-resim` stops there, a trace names it, a fix is checked against it.

It replaced the G key (a tester's in-match "mark this moment"), which went on 2026-09-29.

## Using it

- **The list** (`/playback/`): every `.SRP` in `/recordings/`, newest first, filterable by
  date or match id. It loads none of the game, so it is quick.
- **The player** (`/playback/SWOS.html?srp=<name>`): the recording at full speed.

| control | does |
|---|---|
| the bar | drag to any frame |
| Space / the play button | pause, play |
| ← → (or the buttons) | one frame back or on, and pause |
| Shift + ← → | ten frames |
| **copy link** | a link that opens paused on this frame (`&f=<frame>`) |

The frame number under the picture counts from the first frame of the recording (the
kick-off of the first half). While paused, the page's address follows the frame, so a
reload stays there and the address bar is itself the link to send.

## Who played whom

Each row of the list says who played -- `PAYAVELI (ENGLAND) 0-0 UMTY (BRAZIL)` -- with a
mark for a match that desynced (and its frame) or that one player ended early, and the
filter searches names and teams as well as dates and match ids. The player's bar and its
tab title say it too.

It comes from the recorder: when a match closes it writes a CARD beside the `.SRP`
(`<name>.json`: the players' names, teams, score, builds and platforms, how each one's
match ended, whether it was relayed, the desync frame) and `index.json`, every card in the
folder newest first (`third_party/matchmaker/Server/Recorder.cpp`, `WriteMatchCard`). The
page reads `index.json` beside nginx's listing and lists every recording either way; one
without a card shows its match id as before. Addresses are never on a card -- the page is
public; it says who, not where from. A player who does not record (`[netplay] record = off`, a
browser, or a build from before 2026-09-30, when it became on by default) never streams, but still gets a name: the other player's opening message names
both (`"recorded": false` on the card, `peer` in the ledger), and for matches from before the
recorder kept that, `recordings_index.py` reads it from the match report's `rating.txt`. A
recording the ledger never closed (the server stopped mid-match) gets a card from the
recording itself -- its teams, score and length from META, and since 2026-09-30 its players'
names from its own `PLYR` chunk (docs/REPLAY_FORMAT_V2.md §4), which every online match
writes from kick-off -- so no recording is left showing only its id. The player page reads
`PLYR` too when a recording has no card. Recordings made before the recorder wrote cards get
theirs from the ledger's `match` lines, and a card whose recording was cleared goes:
`scripts/recordings_index.py`, which `install`, `restart` and the admin page's
"Update recordings player" run, and the "Clear" buttons after them.

## Deleting a recording

**manage** (top right of the list) puts a **delete** button on every row. A delete asks for
confirmation, names the match, and removes the `.SRP` and its card from the server. It goes to
the admin service behind `/cc/` (`POST /cc/delete`, `tools/matchmaker-admin/main.py
handle_delete`), so the browser asks for the admin page's login the first time, and nobody
without it can delete anything; the request carries an `X-SWOS-Admin: 1` header, which a form
on another site cannot send, so no other page can delete through a logged-in browser. Only a
recording's own name is accepted. The admin page's **Clear all recordings** is still there
for everything at once. `scripts/serve_playback.py` answers the same request locally, without
a login, for trying it on this machine.

## A recording is shown in the view it was played in

The DOS view draws 200 lines, the Amiga view 272. A DOS-view match shown 272 lines tall shows
what its players never saw: players below their picture, and at the bottom goal rows past
the bottom of the stadium (`20260929-205209-f526dc63.SRP`, frame 7056). So the viewer --
the in-game LOAD/VIEW, `--view-replay`, this page -- switches to the recording's view for
the replay and back after (`ReplayV2_UseRecordedView`). META's flags say which it was from
now on (8 the DOS view, 16 "the view is recorded"); a netplay recording's SIMV said before
that; an older file is read from its camera, which goes below Y 608 only when the match
was simulated 200 lines tall. Where the camera still points past the pitch for the screen
it is shown on, it is held at the bottom (`shown_camera_y`).

## What each player saw

A live match's recording also keeps what each PLAYER saw of it (`PRED` and `SEEN`,
docs/REPLAY_FORMAT_V2.md §4): under rollback a player's screen shows guesses of the other
player's input, corrected a few frames later. When a recording has it, the page adds:

- **A timeline** over the bar, a row a player -- player 1 (the host, the top team) above in
  yellow, player 2 below in white. Brighter where more of the other player's input was
  guessed; **orange** where a guess was wrong but moved nothing visibly; **red** where the
  player SAW something that never happened. Dark grey: that player's half is missing. Click
  it to jump. Under each row a thin line marks the frames that player's game was being
  slowed: **cyan** by a prediction balance wait, **blue** by a time sync wait (`READ`,
  docs/REPLAY_FORMAT_V2.md §4). Beside the buttons the page says the match's input delay
  and whether the balance was ON (it takes both players), off, or not recorded (a match
  from before 2026-10-09, or a server not yet updated); the line under the bar names a
  wait on the frame shown.
- **The overlay** on the picture, for the views chosen (**V** / the *view* button: both,
  player 2, player 1, off). Every object a player saw somewhere else gets a line from where
  it really was (a red dot) to where that player's screen showed it (a box in that player's
  colour), and the drawn spot's trail over the frames before: a rollback reads as a ghost
  that runs off and snaps back. At the bottom left a bar a player: its length the frame's
  lag, red when it showed something that did not happen.
- **Three screens** (**S** / the *screen* button: the match, player 1's screen, player 2's).
  *The match* is the recording as it was finally played. A player's screen is the same
  picture with every man and the ball that player's game drew somewhere else drawn THERE
  -- the sprites themselves moved, not boxes over them -- so a rollback is seen as that
  player saw it: a man who runs on and snaps back. `&screen=1` or `&screen=2` in a link
  opens it that way, and *copy link* keeps it. What it cannot show is in "What a
  player's screen is made from" below.
- **[ and ]** (the *rollback* buttons): the previous or next moment someone saw something
  that did not happen, for the views shown.
- The line under the bar says it for this frame: `P1 guessed 6, replayed saw it wrong`.

### What a player's screen is made from

`SEEN` keeps, for each player and frame, where his game drew each of the 22 men and the
ball when that was not where the match finally had them. A recorded sprite is its
object's position less its graphic's centre, so the sprite that belongs to an object is
found exactly (`perspective_shifts`, src/replay_v2.c) -- over a whole match every
recorded position found its sprite -- and is drawn moved by the difference. The ball's
shadow moves with the ball.

That makes the POSITIONS exact. Three things are still the final match's, because the
recording does not hold them per player:

- **the pose**: the animation frame and the way a man faces are the ones finally played;
- **the camera**: each game aims its own, and only the final one is recorded;
- **everything else**: the number over the controlled man, the referee, the score.

Two men standing on the same pixel -- SWOS allows it -- are told apart only by drawing
order, so their two pictures can be exchanged for that frame.

An exact screen, pose and camera included, takes replaying each player's game from the
inputs and the lag (`INPT` + `PRED` + `STAT` are enough for that, REPLAY_FORMAT_V2.md §4),
or recording the pose and camera per player in `SEEN`. Neither is done.

`SWOS_REPLAY_SCREEN=1|2` starts `--view-replay` and `--replay-player` on that screen.

To try it with no server, record a match over an emulated bad link and open it locally:

```bash
RECORD=1 NETEM=mobile MATCHES=1 FRAMES=4500 \
  HOST_ENV=SWOS_LIVE_CHASE_INPUT=1 JOIN_ENV=SWOS_LIVE_CHASE_INPUT=2 scripts/relay_test.sh
mkdir -p build/recordings-pred && cp /tmp/swos-relay.*/recordings/*.SRP build/recordings-pred/
scripts\serve_playback.bat --recordings build\recordings-pred     # on Windows; or:
python3 scripts/serve_playback.py --recordings build/recordings-pred
```

## How it works

It is the ordinary browser build of the game with a different page: `make WASM=1 PLAYER=1`
builds `dist/playback` -- `SWOS.html/.js/.wasm/.data` from `wasm/player.html`, and
`index.html` from `wasm/playback-index.html` -- with its own objects
(`build/emscripten/wasm-player`) and no netplay.

The page fetches the recording into the game's file system and starts it with
`--replay-player=/in/<name>`: `--view-replay` (src/replay_v2.c), except that it runs in
real time, holds at the last frame instead of exiting, and takes the page's controls --
`ReplayV2_PlayerFrames/TickRate/Frame/Paused/Pause/Seek/Step` -- and for what each player
saw `ReplayV2_PlayerPrediction/PredAt/Overlay` -- exported to the page with
`EMSCRIPTEN_KEEPALIVE`. Every frame of a recording's `FRMS` chunk has its own offset, so a
seek is only the cursor, the sound cursor, and the camera put straight where the match had
it -- what the first frame of any replay does.

The native build takes `--replay-player=<file>` too, which is the quick way to try a change:

```bash
bin/SWOS.x86_64 --replay-player=/abs/path/to/X.SRP
```

## On the server

**To update it:** the admin page's **Update recordings player** button, or
`scripts/update_playback.sh` on the server -- it builds the player, copies it into
`/srv/swos-matchmaker/playback` and refreshes the match cards, as the service account, with
no root and no restart (a match in progress is untouched). The admin gateway needs write
access there: after pulling this, `tools/matchmaker-admin/matchmaker-admin-service.sh
install` once.

The first time, it is built by the unprivileged user and installed by the service script,
like the matchmaker:

```bash
scripts/build_playback.sh                      # -> dist/playback; needs emsdk (EMSDK_DIR, default ~/emsdk)
scripts/run_matchmaker.sh --build-only --with-playback    # the server and the player (the admin page's Rebuild)
scripts/swos-matchmaker-service.sh install     # copies dist/playback -> /srv/swos-matchmaker/playback
```

`build_playback.sh` exits 3 when there is no emsdk, and the rest carries on without the
player; `install` and `restart` copy it only when it has been built, and say so otherwise.
Once, emsdk and nginx (docs/NETPLAY_SETUP.md §12, "The recordings player"):

```bash
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
cd ~/emsdk && ./emsdk install latest && ./emsdk activate latest
```

**nginx is set up by `install` / `restart`** (since 2026-09-29 -- the first server install
copied the player and left nginx alone, so every `/playback/` URL fell through to the
site's catch-all, "Nothing to see here", and the list showed nothing). The script writes
`/etc/nginx/snippets/swos-playback.conf`:

```nginx
location = /playback { return 301 /playback/; }
location /playback/ {
    alias /srv/swos-matchmaker/playback/;
    index index.html;
    types { text/html html; application/javascript js; application/wasm wasm; application/octet-stream data; }
}
```

and adds `include /etc/nginx/snippets/swos-playback.conf;` to the ONE enabled config that has
`location /recordings/`, just before that block, keeping the original as `<file>.swos-bak`.
It runs `nginx -t` -- a failure puts the file back -- reloads nginx, and fetches
`https://<server_name>/playback/SWOS.wasm` to check it is served as `application/wasm`.
Anything it could not do (no nginx, no `/recordings/` block or more than one, a rejected
config, a failed check) ends the run with a **WARNING** block saying what to add by hand.

The page reads `../recordings/` -- the listing nginx already publishes for
`scripts/fetch_recordings.py` -- so the two must stay side by side under one host.
`?base=<url>` on either page points it at another folder.
