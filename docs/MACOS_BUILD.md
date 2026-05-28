# Building on macOS

The SDL port builds on macOS as a native Mach-O binary, either:

- **Single-arch** for your current Mac (Apple Silicon → `arm64`, Intel → `x86_64`), or
- **Universal binary** (`x86_64 + arm64`) that runs on both Apple Silicon and Intel.

Both paths target a **macOS 11 (Big Sur)** minimum by default, so a
universal build produced on a 2025-era Mac will still run on a 2021 M1
or a late-Intel iMac on Monterey.

---

## Prerequisites

### 1. Xcode Command Line Tools

```bash
xcode-select --install
```

(Skip if you have full Xcode installed.) This gives you `clang`, `make`,
`lipo`, and the macOS SDK.

### 2. SDL2

You have two choices. **The universal-binary target requires option A.**

#### Option A — SDL2.framework (universal, recommended)

The official build from libsdl.org ships as a universal binary that
links cleanly against both `x86_64` and `arm64` slices.

1. Download the latest **`SDL2-2.x.x.dmg`** from
   <https://github.com/libsdl-org/SDL/releases> (look in the
   "Assets" section of the latest 2.x release).
2. Open the dmg and drag `SDL2.framework` into one of:
   - `/Library/Frameworks/` (system-wide, requires admin password — preferred)
   - `~/Library/Frameworks/` (user-only)
3. Verify:
   ```bash
   lipo -info /Library/Frameworks/SDL2.framework/SDL2
   # → Architectures in the fat file: ... x86_64 arm64
   ```

The Makefile auto-detects `SDL2.framework` in these standard locations
and uses it in preference to Homebrew.

#### Option B — Homebrew SDL2 (single-arch only)

Faster to set up if you only need a native build for your Mac:

```bash
brew install sdl2 pkg-config
```

This installs a single-arch SDL2 matching the host Mac. The plain
`make` target will pick it up via `pkg-config`. `make macos-universal`
will fail because Homebrew's SDL2 isn't a universal binary.

---

## Building

Clone the repo somewhere convenient:

```bash
git clone <repo-url> SWOS_C_SDL
cd SWOS_C_SDL
```

### Native single-arch (for the Mac you're on)

```bash
make
```

Produces `bin/SWOS.macos` for the host architecture. Run it:

```bash
./bin/SWOS.macos
```

### Universal binary (x86_64 + arm64)

```bash
make macos-universal
```

This:
1. `make clean`
2. Builds the `x86_64` slice → `bin/SWOS.macos.x86_64`
3. `make clean`
4. Builds the `arm64` slice → `bin/SWOS.macos.arm64`
5. `lipo -create` → `bin/SWOS.macos` (universal)

Verify:
```bash
lipo -info bin/SWOS.macos
# → Architectures in the fat file: bin/SWOS.macos are: x86_64 arm64
```

Same binary runs on both architectures via the kernel's per-arch dispatch.

### Cross-build a single non-host slice

```bash
make MACOS_ARCH=x86_64        # build x86_64 slice on an M-series Mac
make MACOS_ARCH=arm64         # build arm64 slice on an Intel Mac
```

Requires SDL2.framework (universal). The output is still
`bin/SWOS.macos` — rename or copy if you want to keep both arches
around.

---

## Tweaking the deployment target

The default minimum is **macOS 11 (Big Sur)** because that's the
oldest macOS we currently care about. Override per-invocation:

```bash
make macos-universal MACOS_MIN=13.0      # Ventura+
make macos-universal MACOS_MIN=10.13      # Big Sur+ (latest SDL2 still supports it)
```

Going below ~10.13 is not recommended — newer SDL2 builds may not
load on those versions.

---

## Running

### From command line

```bash
./bin/SWOS.macos
```

The binary expects to find game data (`data/`, `grafs/`, `grafs_aga/`,
`sound/`) **relative to itself** in the same `bin/` directory.

### As an .app bundle (optional, for sharing)

```
make macos-app
```

Builds the universal binary first, then assembles `bin/SWOS.app`:

```
SWOS.app/
└── Contents/
    ├── Info.plist                   # bundle metadata (CFBundleName, version, min-OS, …)
    ├── PkgInfo                      # "APPL????" legacy marker
    ├── MacOS/
    │   ├── SWOS                     # the universal binary
    │   ├── data/                    # game assets (copied from bin/data/)
    │   ├── grafs/
    │   ├── grafs_aga/
    │   ├── sound/
    │   └── save/                    # whatever you have in bin/save/ at build time
    ├── Frameworks/
    │   └── SDL2.framework           # bundled (so user doesn't need to install)
    └── Resources/
        └── SWOS.icns                # optional, picked up if present in repo root
```

The binary's `-rpath @executable_path/../Frameworks` means a bundled
`SDL2.framework` at `Contents/Frameworks/` resolves automatically — end
users don't need to install SDL2 themselves.

Run it via `open bin/SWOS.app` (or just double-click in Finder).

**Gatekeeper note**: the bundle is unsigned. Running it on the same
Mac that built it is fine; copied to another Mac, macOS may refuse
to launch ("developer cannot be verified"). Two options:

```bash
# Ad-hoc sign — lets right-click → Open work on any Mac
codesign --force --deep --sign - bin/SWOS.app

# Wrap in a .dmg for distribution
hdiutil create -fs HFS+ -srcfolder bin/SWOS.app -volname SWOS bin/SWOS.dmg
```

Proper Developer-ID signing + notarisation needs a paid Apple Developer
account and `xcrun notarytool` — out of scope for this doc.

---

## Troubleshooting

- **`SDL2.framework not found in any standard location`** — install
  per *Option A* above. The Makefile checks `/Library/Frameworks`,
  `~/Library/Frameworks`, `/opt/homebrew/Frameworks`, and
  `/usr/local/Frameworks` (in that order).
- **`ld: warning: ignoring file ... building for macOS-arm64 but ... is x86_64`** —
  you've got Homebrew SDL2 on the search path AND tried a non-host
  arch. Switch to SDL2.framework (it's universal and beats this) or
  build only for the host arch.
- **`/usr/bin/make: Bad CPU type in executable`** when running an
  arm64-only binary on Intel (or vice versa) — the universal target
  fixes this. Use `make macos-universal`.
- **`ld: library 'SDL2' not found`** even though Homebrew installed
  it — `make clean` first; stale `.o` files from a prior build may
  reference an old SDL2 search path.
- **The app crashes immediately on Monterey but runs on Tahoe** — you
  probably built without the deployment-target flag (the default is
  `12.0`, so if you've overridden `MACOS_MIN` to a higher value the
  binary won't load on older systems).

---

## Quick sanity check on a built binary

```bash
file bin/SWOS.macos
# Universal: "Mach-O universal binary with 2 architectures: ..."
# Single-arch arm64: "Mach-O 64-bit executable arm64"
# Single-arch x86_64: "Mach-O 64-bit executable x86_64"

otool -L bin/SWOS.macos | head -5
# Should show @rpath/SDL2.framework/Versions/A/SDL2 (or similar)
# and the system libs (libSystem, libc++, etc.)

vtool -show-build bin/SWOS.macos | grep minos
# minos 11.0  ← the deployment target you built with
```
