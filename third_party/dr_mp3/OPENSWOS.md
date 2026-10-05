# dr_mp3 in OpenSWOS

The MP3 decoder of the streamed music tracks (`src/music_stream.c`, docs/DOS_SOUND.md
"The CD track"): the PC CD release's title song.

| | |
|---|---|
| upstream | https://github.com/mackron/dr_libs |
| version | 0.7.4 — commit `dfe8377631000664666519fdb83da193fd8037f4` |
| licence | public domain (Unlicense) or MIT No Attribution, at the user's choice — the text is at the end of `dr_mp3.h` |
| files taken | `dr_mp3.h` — **unmodified** |
| sha256 | `997b7ee18de6e6b81e2a83f1ea9fc62aef25c62b28d48db95635f49e65de0a2f` |

Compiled once, by `src/music_stream.c` (`DR_MP3_IMPLEMENTATION`, `DR_MP3_NO_STDIO`: the
file is read by the game and decoded from memory). SDL builds only.
