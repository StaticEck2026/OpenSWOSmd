# OpenSWOS on the Nintendo Switch

How to build, package and debug the Switch target, and why the parts that are
not obvious are the way they are.

**This target is for registered Nintendo developers.** The NintendoSDK and the
Switch fork of SDL are both under NDA and **neither is vendored here, and
neither may be committed to this repository**. Nothing in this document or in
the build reproduces SDK source; it describes how this project uses one.

The port follows the design in `SWITCH_PORT.md` from the Jungool port, which is
the reference this was built against — same SDK, same link set, same packaging.
Where SWOS differs it is called out below.

---

## Status

**Builds, links and packages.** `bin/SWOS.nss` (5.6 MB) and `dist/SWOS.nsp`
(84 MB) come out of a clean run against SDK 22.2.10 and a checkout of the SDL
fork. **Not yet installed or run on hardware** — everything past the package is
unverified.

The linked module is what the loader expects:

```
ELF 64-bit LSB pie executable, ARM aarch64
.rocrt.init  placed first in .text        MOD0 header present at 0x13c040
DT_NEEDED    opengl.nss  vulkan.nss  nnSdk.nss
```

which is why the code directory needs `subsdk0`/`subsdk1`/`sdk` alongside
`main`. `e_entry` is 0 and that is fine — the loader reads MOD0, and the
failure the reference port describes is MOD0 *absent* with entry 0.

What is untested: startup, whether the loader accepts the package, the save
mount and its commit, input, audio, and frame pacing.

Two placeholders **must** be replaced before submission — see
[Metadata](#metadata).

### What building it actually took

Four things beyond the reference port's list, each found by the link failing:

- **Every `nintendoswitch` backend in the SDL fork is C++**, including
  `main/nintendoswitch`, which is where `nnMain` lives. Globbing `*.c` only
  gives `undefined reference: nnMain` with nothing to suggest a file
  extension is the problem. `Makefile.switch` builds `.cpp` too, with
  `-std=gnu++20` and `-fno-use-cxa-atexit`, and links with `clang++`.
- **`DATA_FLAGS` is not optional.** The SDL Makefile passes nine
  `-D*_DATA_C=1` toggles that compile in the C versions of the leagues,
  tactics, fixtures, palettes and strings. Without them those files compile to
  almost nothing — `tactics_data.o` came out at 760 bytes — and the link fails
  on a wall of undefined data symbols like `_teamTactics532` and `_uefaCup`.
- **`SIGHUP` is defined but `signal()` is not exported.** `main()` guarded a
  Cygwin-only `signal(SIGHUP, SIG_IGN)` on `#ifdef SIGHUP`, which is true here
  and links to nothing. There is no controlling terminal on a console anyway.
- **`SDL_NintendoSwitch_CommitSaveData` is declared in `SDL_filesystem.h`,**
  not in a header of its own; there is no `SDL_nintendoswitch.h` in the fork.

---

## What you need

| | |
|---|---|
| NintendoSDK | built against 22.2.10 |
| SDL fork | `libsdl-org/SDL-switch`, licensee-only |
| Host | Windows. The compiler is a Windows executable |

```
NINTENDO_SDK_ROOT   default D:\Nintendo\NativeSDK22.2.10\NintendoSDK
SDL_SWITCH_ROOT     default D:\Projects\GitHub\SDL-switch
```

Visual Studio is **not** required. `make` is; Cygwin's is used automatically if
none is on `PATH`, since this checkout already needs a Cygwin for the Windows
target.

### Building SDL

`Makefile.switch` builds the fork itself, into `obj-switch/libSDL2_nx.a`, so
this target needs nothing but a checkout. Point `SDL_LIB` at a prebuilt library
instead if you have one. It needs no
special defines: NintendoClang predefines `__NINTENDO__` and `__NX`, so
`SDL_platform.h` sets `__NINTENDO_SWITCH__` and SDL picks
`SDL_config_nintendoswitch.h` by itself.

Only these directories under `src/` belong in an NX build — the tree carries
every platform's backend:

```
atomic cpuinfo dynapi events file power render render/opengl render/opengles2
render/software sensor sensor/dummy stdlib thread thread/pthread timer
timer/unix video video/dummy video/yuv2rgb audio audio/disk audio/dummy
audio/nintendoswitch filesystem/nintendoswitch haptic haptic/nintendoswitch
joystick joystick/nintendoswitch loadso/nintendoswitch locale
locale/nintendoswitch misc misc/nintendoswitch video/nintendoswitch
main/nintendoswitch
```

`main/nintendoswitch` is SDL's `nnMain`, which calls `SDL_main` — our `main()`.

---

## Building

```bat
scripts\build_switch.bat            :: -> bin\SWOS.nss
scripts\build_switch.bat package    :: the above, then dist\SWOS.nsp
scripts\build_switch.bat clean
```

```bash
scripts/build_switch.sh [package|clean]   # the same, driven from WSL
```

`build_switch.sh` wraps the `.bat` through Windows interop, the same approach
`scripts/build_dist.sh` already uses to reach Cygwin. The compile itself
**must** happen on the Windows side: NintendoClang is a Windows executable and
would be handed Linux paths from WSL.

### Why a makefile

The SDK has no CMake support and integrates with Visual Studio through MSBuild
props and targets; building an NX `.vcxproj` needs the VSI package. But
NintendoClang takes its flags from plain config files under
`Build/ClangConfigs/NX-NXFP2-a64/`, so the same compiler is driven directly and
SWOS keeps one makefile per target — `Makefile`, `Makefile.amiga`,
`Makefile.switch` — instead of gaining a project file that would drift.

The flags are reproduced rather than passed with `--config` because the config
files bake in the language standard, which the build has to choose itself.
**If the SDK is updated, diff `Makefile.switch` against**
`Build/ClangConfigs/NX-NXFP2-a64/{Compile/C,Link}/Application.cfg`.

### Things that cost an afternoon each

Inherited from the reference port, and all still true here:

- **A build-type macro is mandatory.** Without `NN_SDK_BUILD_DEVELOP` every SDK
  header stops with `ビルドタイプマクロが定義されていません` — "the build type
  macro is not defined".
- **`nnSdk` does not bring its own dependencies.** It references
  `__nnDetailNintendoSdkRuntimeObjectFile` (in `nnApplication.o`) and `malloc`
  (in `libnn_init_memory.a`). The platform has no C runtime to fall back on.
- **`rocrt.o` is not optional.** It supplies the `.rocrt.init` section the
  linker script places first in `.text`, and with it the `MOD0` header the
  loader reads. Without it everything links, the ELF looks healthy, `MakeNso`
  produces a structurally valid NSO — and the console refuses to launch it.
- **Order matters** in the link set; all of these are referenced by `nnSdk`,
  which comes after them:

```
rocrt.o  nnApplication.o  libnn_init_memory.a  nnSdk.nss  crtend.o
```

plus `libnn_gll.a`, `opengl.nss` and `vulkan.nss`, because SDL's NX video
backend renders through GL.

---

## What is actually different about SWOS

Almost nothing in the game. The Switch is a flavour of the SDL build, so unlike
`Makefile.amiga` there is no excluded-source list — the same `src/` compiles.
SWOS renders 8bpp chunky into its own frontbuffer and only meets SDL at the
texture upload, so the whole simulation is untouched.

All four differences are filesystem or logging, and each is one `#ifdef`:

**1. The content is read-only at `rom:/`, there is no `chdir`, and the mount
does not happen by itself.** `SDL_GetBasePath()` is what calls
`nn::fs::MountRom("rom", ...)`, on its first call — nothing else mounts it. So
`swos_save_dir_init()` calls it at startup before anything reads a file and
keeps the result as the data prefix, rather than hard-coding `"rom:/"` so the
two can never disagree. **Miss this and every `fopen` fails and the game comes
up with no data at all**, which looks exactly like the content not being in the
package. Every other
target `chdir`s to the binary's directory at startup and then opens `data/…`,
`grafs/…` relative to it. `swos_data_path()` (`include/swos_io.h`) anchors
those reads instead — identity on every other target, returning the caller's
own pointer with nothing copied. It is applied at the four places that open
game data: `resload.c`'s `resolve_path` (the funnel for every Amiga-named
asset), and the three JSON loaders (`translations.c`, `menu_load_json.c`,
`tactics_load_json.c`).

**Anchoring breaks any prefix test written against a relative path**, and it
does it silently. `resload.c`'s AGA redirect — which sends a `grafs/FILE` load
to `grafs_aga/FILE`, i.e. the 8bpp AGA assets rather than the 4bpp OCS ones —
tested `strncmp(path, "grafs/", 6)`, but by the time it ran the path was
`rom:/grafs/FILE`. The test matched nothing, the redirect no-opped, and the
console rendered the OCS set for everything named in `strings_data.c`:
`soccer_s.raw`, `charset.raw`, the six `swcpich*.map` pitch maps, the `cjc*`
team and bench banks, `menus.raw` — 36 names, which is essentially everything
you see. It was not total, which is what made it hard to spot: `SWOS.c`,
`asset_loader.c`, `asset_sys.c` and `loader_intro.c` compose their names from
the `GRAFS` macro, which is already `"grafs_aga/"` under `#define AGA`, so
those loads were unaffected and the console came up as a mixture.

`swos_data_root()` (`include/swos_io.h`) is the counterpart to
`swos_data_path()` for this: it hands back the prefix — `""` off-Switch,
`"rom:"` on it — so a test can strip the root before matching and re-anchor
each candidate it builds. **If you add a path test to shared code, write it
against the relative part**; a Switch-only failure of this shape does not show
up on any other target, and it does not look like a path bug from the couch —
it looks like the wrong art.

**2. `SDL_GetPrefPath` mounts `save:/` against the account chosen at launch,**
so `StartupUserAccount` must not be `None` — with `None` it returns nothing and
every career would be lost at exit. The mount is flat, so unlike the desktops
there is no `user/` and `user/save` split: `SWOS.ini` and the `.CAR` files
share it.

**3. A write to `save:/` is not durable until it is committed,** and SDL will
not do it. Without the commit a save looks written, reads back correctly for
the rest of the session, and is simply absent next launch. `swos_save_commit()`
calls `SDL_NintendoSwitch_CommitSaveData("save")` and is called, always **after**
the file is closed (which the commit requires), from every write to the save
mount (`70bfbb8`): `rob_save()`/`rob_delete()` in `resload.c` — the path careers,
tactics, edited teams and options actually take — plus `swos_io_save()`,
`Settings_Save()` and the first-launch `SWOS.ini`, the `.SRP` replays
(`replay_v2.c`), the arcade high-score table and the match-report zip.

**4. `printf` and `stderr` reach nobody.** Nothing connects stdio to a devkit,
so a build that fails during startup gives an error code and not one word about
what it was doing. `swos_platform.h` redirects both to `NN_LOG`, which does
reach `ControlTarget read-log`:

```c
#define fprintf(stream, ...) \
    (((stream) == stderr) ? SwosSwitchLog(__VA_ARGS__) : fprintf(stream, __VA_ARGS__))
#define printf(...)  SwosSwitchLog(__VA_ARGS__)
```

Done as macros rather than by editing several hundred call sites. The `fprintf`
form tests the stream so real file writes — `SWOS.ini`, the JSON saves — still
go to their files; only the stderr side is diverted. The `fprintf` in the else
arm is the real function, because a macro is never expanded recursively inside
its own expansion.

`src/platform_switch.cpp` is the other end, and is the project's **only
Switch-specific C++ translation unit** — it exists because `NN_LOG` is a C++
macro (`::nn::diag::`). (It is no longer the only `.cpp` in `src/`:
`netplay_netem_install.cpp` bridges netplay to GGPO-X, and compiles to nothing
here because the Switch build does not define `SWOS_NETPLAY`.)
`Makefile.amiga` filters it out through `SWITCH_ONLY_SRCS`, the same way
`SDL_ONLY_SRCS` filters C. The desktop builds' `stderr`-to-`SWOS.log`
`freopen` is skipped here: a log file on the save mount would burn the player's
quota and need committing to survive anyway.

### In-match controls

The console has no keyboard, and everything the match understands beyond fire
and direction is a KEY — so `input_sdl.c` maps pad buttons to them. Not
Switch-gated: a desktop pad had the same problem, it was just easier to ignore
because a keyboard was in reach.

| button | key | function |
|---|---|---|
| A / B | — | fire (B also skips) |
| + (Start) | P | pause |
| − (Back) | S | statistics |
| Y | R | instant replay |
| X | H | replay after the match |
| L | UP | substitutes, bottom team |
| R | DOWN | substitutes, top team |
| R-stick click | ESC | quit the match |

The keys go in as **raw Amiga scancodes**, through the same `_keyBuffer` a
keypress uses, so `_keycodeLut` and everything downstream is untouched — this
adds a producer, not a code path. Worth knowing that the LUT is why the match
compares against `0x50` for pause: that is ASCII `'P'` *after* translation, not
the raw scancode for P, which is `0x19`. Reading the comparison as a scancode
gets you F1, which is wrong.

Quit is on a stick click deliberately — it ends the match, and a click is not
something you hit by accident the way a face button is. HOME is left alone; it
belongs to the system.

The bindings are printed to the log when a pad connects, since there is no
manual to check on a console.

### Not yet done

- **Screenshots** (`platform_sdl.c`, `SWOS_SCREENSHOT_INTERVAL` / F9) still
  write a relative path, which will fail on the read-only mount. The console
  has its own capture button, so the honest fix is probably to compile them out
  rather than route them to the save.
- **Resolution.** 1280x720 gives `_screen_width = 272 * 16/9 = 484`, the
  widescreen path — the same one three camera bugs were just fixed in, and
  worth watching on hardware. Docked 1080p is the same aspect and therefore the
  same 484.
- The `#pragma GCC optimize` in four files is ignored by clang, so the Switch
  build is one optimisation short there. Not known to matter; it is the
  `-Ofast` miscompile workaround, and clang is not gcc.

---

## Packaging

`scripts\build_switch.bat package` runs three tools in order, because the
linked ELF is not something the console can run:

1. **`MakeNso`** turns it into the loadable module.
2. **`MakeMeta`** turns the `.nmeta` into the process descriptor the loader
   checks.
3. **`AuthoringTool creatensp`** wraps both plus the data tree into the package.

### The code directory

Easy to get wrong, because nothing warns you. It holds **six** files, and the
loader finds them by **fixed name**, not by the `DT_NEEDED` strings:

| file | source |
|---|---|
| `main` | our NSO |
| `main.npdm` | from `MakeMeta` |
| `rtld` | `nnrtld.nso` |
| `sdk` | `nnSdk.nso` |
| `subsdk0` | `opengl.nso` |
| `subsdk1` | `vulkan.nso` |

`--compress-compatible` is passed to `MakeNso`. Without it the NSO carries
flags `0xbf`; with it `0x3f`, which is what the SDK's own shipped NSOs have.
Bit 7 is a newer compression format. It costs about 400 KB and removes a
variable.

### What goes into `rom:/`

`data grafs grafs_aga grafs_dos grafs_2020 sound data_json` — the
`FULL_DATA_DIRS` list `scripts/build_dist.sh` ships in the desktop drop, which
is the project's tested "what the game needs at runtime" set. The asset tiers
are selectable at runtime and `data_json` carries the translations `tr()`
reads, so none of it is build-only.

Two deliberate omissions:

- **`save/`** — the console mounts the player's own save at `save:/`, and
  `bin/save` in this checkout is 24 MB of test careers.
- **`grafs_arcade`** — 27 MB of a 111 MB package for the coin-op cabinet mode,
  which is dedicated-hardware and off by default. Put it back in the list in
  `scripts/build_switch.bat` if arcade mode is ever wanted here.

For scale, the package is dominated by the SDK's own modules, not by SWOS:
`opengl.nso` 24 MB and `vulkan.nso` 23 MB in the code directory against 1.1 MB
for `main`.

---

## Metadata

`packaging/switch/swos.nmeta`. **Two values are borrowed and must be replaced
before submission:**

| field | current | why |
|---|---|---|
| `ApplicationId` | `0x0100c0d000494000` | an SDK sample's id (PrepoSimple — deliberately *not* Demo1, `0x01004b9000490000`, which the reference port on the same devkit uses). Installing this build overwrites that sample if it is on the console, and two titles sharing an id overwrite each other's saves |
| `ApplicationErrorCodeCategory` | `ABC12` | required because SDL's assert dialog calls `nn::err::ShowApplicationError` |

Do not invent an id that looks like a development range:
`0x0100000000010000` was tried on the reference port and refused with "Failed
to launch application" and an empty log, because an all-zero middle is the
**system title** range. Every real id is `0x01` + twelve varying hex digits +
`000`.

Other fields worth knowing:

- **`MainThreadStackSize` `0x00100000`.** The platform's default is 80 KB, far
  smaller than the other targets give a main thread, and SWOS is a 1990s Amiga
  program with large automatic buffers on the menu and career paths.
- **`UserAccountSaveDataSize` `0x4000000` (64 MB)** — sized for SWOS, not for a
  settings file. A single career `.CAR` is around 96 KB and the game keeps many
  alongside `.SEA`/`.PRE`/`.DIY` and the `.HIL` highlights.
- The icon is a **1024x1024 24-bit uncompressed BMP**; AuthoringTool rejects
  other formats with a confusing message about baseline JPEG. It is generated,
  with every other platform's icon, from `SWOS.svg` by
  `python3 scripts/generate_icons.py`: the ball, centred on black.
- XML comments cannot contain a double hyphen. `MakeMeta` fails with an
  `XmlException` that does not name the file.

---

## Deploying and debugging

```bat
ControlTarget.exe install-application dist\SWOS.nsp
ControlTarget.exe read-log
```

Run `read-log` **before** launching, then start the game from the home menu.

**Use `Load an Application` rather than `Install application` when
diagnosing.** Install gives a generic failure; Load validates the code
directory directly and names what it does not like.

`AuthoringTool get-unpublishable-error dist\SWOS.nsp` reports submission
problems, and on the reference port one of them — an unconfigured error code
category — was a genuine blocker rather than a submission nicety.

### Loader errors

Both mean the loader rejected the process before any of the game's own code
ran, so the log is empty. Decode rather than guess:

```
result = (description << 9) | module
0x00000A09 -> module 9 (Loader), description 5
```

| symptom | cause |
|---|---|
| `Failed to launch application`, empty log | metadata. Check `ApplicationId` is a real application id |
| `The NSO file in the code directory is not valid (0x00000A09)` | the module or what surrounds it — a missing `rocrt.o`, or an incomplete code directory |

---

## Outstanding

- **Link and run it.** Everything below this line is unverified.
- Replace the borrowed `ApplicationId` and error code category.
- Decide what screenshots do on a read-only mount.
- `SDL_NINTENDOSWITCH_SINGLEPLAYER=1` in the fork's config removes a lot of
  input-related certification pain, and the Switch build is local-only (no
  netplay is compiled into it).
- Verify saves survive a relaunch, and that the commit is in the right places.
- Handheld/docked transitions, audio on the console's speakers, frame pacing.
- Cert has not been surveyed.
