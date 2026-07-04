# Save Data

Where SWOS stores your saves, what it stores, and how the path is chosen.

## Where saves live

Saves go to the platform's **per-user data location** — never inside the app
bundle or install directory. Those install locations are often read-only
(macOS Gatekeeper "App Translocation" for a downloaded `.app`, `/Applications`,
`C:\Program Files`, a mounted `.dmg`), and even when writable, writing there is
wrong: saves would be lost on reinstall and can break a signed bundle.

| Platform | Save directory |
|----------|----------------|
| **Windows** | `%USERPROFILE%\Documents\SWOS\save` (e.g. `C:\Users\Alice\Documents\SWOS\save`) |
| **macOS** | `~/Library/Application Support/SWOS/save` |
| **Linux** | `~/.swos/save` |
| **Web (WASM)** | Browser IndexedDB, mounted at `/save` (see `wasm/pre.js`) |

The directory is created automatically on first launch (including any missing
parent folders). If the per-user location can't be determined or created, SWOS
falls back to a relative `save/` folder next to the binary — this keeps a
developer/CLI run working from the game directory, but a normally-installed
build always uses the per-user path above.

### Finding your saves

- **Windows** — open `Documents\SWOS\save`.
- **macOS** — Finder → **Go → Go to Folder…** → paste
  `~/Library/Application Support/SWOS/save`.
- **Linux** — `~/.swos/save` (a dotfolder; `ls -a ~` or show hidden files).
- **Web** — saves live in the browser's IndexedDB for the page's origin; there's
  no file on disk. Clearing site data for the page erases them.

## What's stored there

Every file the game reads/writes with a save-type extension resolves to the
save directory:

| Extension | Contents |
|-----------|----------|
| `.CAR` | Career |
| `.SEA` `.PRE` `.DIY` | Career-related overlays (season / pre-season / DIY competition) |
| `.TAC` | Custom tactics |
| `.HIL` | Match highlights |
| `.EDT` | Edited teams (e.g. `CUSTOMS.EDT`; also a SWOS-community interop format) |
| `.RPL` | Whole-match replay (PC extra; not native to the Amiga) |

## Save format & versioning

- **Career saves (`.CAR`/`.SEA`/`.PRE`/`.DIY`) are JSON** (`src/save_format_json.c`),
  carrying a `schema_version`. This decouples the on-disk format from the runtime
  memory layout, so a save survives rebuilds and 32↔64-bit / LE↔BE.
- The loader **verifies `schema_version` and hard-rejects a mismatch** — the UI
  shows *"FILE SAVED BY DIFFERENT S.W.O.S. VERSION"*. During bring-up there is
  **no cross-version migration**: bumping the schema makes older saves
  unloadable by design. Start a fresh career after a schema change.
- The small fixed-layout records (`.TAC`, `.HIL`, `.EDT`) are still binary — see
  `docs/SAVE_DATA.md` "Format migration / read-support policy".

## No bundled default saves

A fresh install ships with an **empty save directory** — no default career or
tactics files. Start a new career (or create tactics in the editor) and it's
saved to the per-user location above.

## Implementation

- `swos_save_dir()` returns the base save directory; `swos_save_dir_init()`
  resolves it once at startup (`src/main.c`, right after `SDL_Init`). Both live
  in `src/swos_io.c`.
- The path is consulted by **both** resolvers so load and save always agree:
  `resolve_save_path()` (reads, `swos_io.c`) and `resolve_path()` (writes,
  `resload.c`).
- Platform selection is compile-time (`_WIN32`/`__CYGWIN__`, `__APPLE__`, else
  Unix; `SWOS_WASM_BUILD` keeps the relative `save` that maps to the IDBFS
  `/save` mount).

## Amiga

The Amiga port is unaffected: it saves through its own filesystem loader
(`RobNorthenLoader`) to the game's own directory, not `swos_save_dir()`. This
document covers the portable SDL/WASM builds.
