# ENET in OpenSWOS

The UDP transport for online play: the matchmaking client (`src/mm_client.c`) and
the local matchmaking server (`third_party/matchmaker`) both speak it, and it
carries the peer addresses that a P2P match is then played over
(docs/NETPLAY.md).

| | |
|---|---|
| upstream | https://github.com/lsalzman/enet |
| version | 1.3.18 (`5a9c537`, "fix erroneous NET_TO_HOST") |
| licence | MIT (`LICENSE`) |
| files taken | the 9 `*.c`, `include/enet/*.h`, `LICENSE` — **unmodified apart from CRLF -> LF** |

Vendored rather than linked for the same reason as miniz: the SDL targets have no
common libenet. Cygwin has no package, emscripten none at all, and the release
archives deliberately ship no extra libraries.

**One copy, deliberately.** SWOS 2020 bundles its own ENET (1.3.17) inside
SimpleMatchmaker, and the vendored server here was re-pointed at THIS copy rather
than keeping that second one. `include/enet/protocol.h` is byte-identical between
1.3.17 and 1.3.18 apart from line endings, so the wire format is unchanged and a
stock swos-2020 client still interoperates — but two ENETs in one tree is exactly
how you end up with a pair that is wire-compatible and subtly different in
behaviour.

**Build knobs are explicit, not probed.** Upstream discovers `HAS_SOCKLEN_T`,
`HAS_POLL`, `HAS_FCNTL`, `HAS_INET_PTON`, `HAS_INET_NTOP` and `HAS_MSGHDR_FLAGS`
with autotools/CMake feature tests. Our Makefile and
`third_party/matchmaker/CMakeLists.txt` both set them by hand for non-Windows
targets. Getting one wrong does not fail the build — it silently changes socket
behaviour — so they are written out rather than inherited.

`win32.c` is compiled only on Windows targets, `unix.c` everywhere else; never
both.

To update: replace the `*.c`, `include/enet/*.h` and `LICENSE` from a newer
release (normalising line endings), record the version above, rebuild every SDL
target and the server, and run `bin/SWOS.x86_64 --matchmaker-selftest` against a
locally running `scripts/run_matchmaker.sh`.
