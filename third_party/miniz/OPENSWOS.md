# miniz in OpenSWOS

Deflate/inflate for the match-state resync packet (`src/state_wire.c`, docs/ROLLBACK.md
"The wire format").

| | |
|---|---|
| upstream | https://github.com/richgel999/miniz |
| version | 3.0.2 — release asset `miniz-3.0.2.zip` |
| sha256 of the zip | `ada38db0b703a56d3dd6d57bf84a9c5d664921d870d8fea4db153979fb5332c5` |
| licence | MIT (`LICENSE`) |
| files taken | `miniz.c`, `miniz.h`, `LICENSE` — **unmodified** |

It is compiled once, by `src/miniz_impl.c`, with the configuration in
`include/miniz_swos.h`: no stdio, no time, no ZIP archive API, and no zlib-compatible
names (so it can never clash with a system zlib pulled in by another library). Only the
raw deflate API is used (`tdefl_compress_mem_to_mem` / `tinfl_decompress_mem_to_mem`).

Vendored rather than linked because the SDL targets have no common zlib: Cygwin and
emscripten ship none, and the release archives deliberately bundle no extra libraries.

To update: replace the three files from a newer release zip, record its version and
sha256 above, rebuild every SDL target, and run `scripts/rollback_test.sh --mode resync --all`.
