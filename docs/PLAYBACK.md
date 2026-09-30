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

## How it works

It is the ordinary browser build of the game with a different page: `make WASM=1 PLAYER=1`
builds `dist/playback` -- `SWOS.html/.js/.wasm/.data` from `wasm/player.html`, and
`index.html` from `wasm/playback-index.html` -- with its own objects
(`build/emscripten/wasm-player`) and no netplay.

The page fetches the recording into the game's file system and starts it with
`--replay-player=/in/<name>`: `--view-replay` (src/replay_v2.c), except that it runs in
real time, holds at the last frame instead of exiting, and takes the page's controls --
`ReplayV2_PlayerFrames/TickRate/Frame/Paused/Pause/Seek/Step`, exported to the page with
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

```nginx
# in the us.swos.eu HTTPS server block, beside /recordings/
location /playback/ {
    alias /srv/swos-matchmaker/playback/;
}
```

The page reads `../recordings/` -- the listing nginx already publishes for
`scripts/fetch_recordings.py` -- so the two must stay side by side under one host.
`?base=<url>` on either page points it at another folder.
