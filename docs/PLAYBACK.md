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

## What each player saw

A live match's recording also keeps what each PLAYER saw of it (`PRED` and `SEEN`,
docs/REPLAY_FORMAT_V2.md §4): under rollback a player's screen shows guesses of the other
player's input, corrected a few frames later. When a recording has it, the page adds:

- **A timeline** over the bar, a row a player -- player 1 (the host, the top team) above in
  yellow, player 2 below in white. Brighter where more of the other player's input was
  guessed; **orange** where a guess was wrong but moved nothing visibly; **red** where the
  player SAW something that never happened. Dark grey: that player's half is missing. Click
  it to jump.
- **The overlay** on the picture, for the views chosen (**V** / the *view* button: both,
  player 2, player 1, off). Every object a player saw somewhere else gets a line from where
  it really was (a red dot) to where that player's screen showed it (a box in that player's
  colour), and the drawn spot's trail over the frames before: a rollback reads as a ghost
  that runs off and snaps back. At the bottom left a bar a player: its length the frame's
  lag, red when it showed something that did not happen.
- **[ and ]** (the *rollback* buttons): the previous or next moment someone saw something
  that did not happen, for the views shown.
- The line under the bar says it for this frame: `P1 guessed 6, replayed saw it wrong`.

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

Built by the unprivileged user and installed by the service script, like the matchmaker:

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
