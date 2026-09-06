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

**Not yet built end to end.** The SDL fork was not available on the machine
this was written on, so the target has never linked or run.

What *has* been verified, by compiling the tree with the real NintendoClang
(`Nintendo version 1.21.2 clang 21.1.8`, target `aarch64-nintendo-nx-elf`) from
the SDK at 22.2.10:

| | |
|---|---|
| sources compiling clean for the Switch | **138 of 147** |
| remaining 9 | blocked *only* by `SDL.h` not being present |
| SDK pieces the build references | all confirmed present at the paths used |

So the C is portable and the toolchain flags are right. What is untested is
everything that needs a link: startup, the loader accepting the package, the
save mount, input, audio, and whether the frame pacing holds.

Two placeholders **must** be replaced before submission — see
[Metadata](#metadata).

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

The fork is built once, separately, and `Makefile.switch` links the result
(`SDL_LIB`, default `$(SDL_SWITCH_ROOT)/build-nx/libSDL2_nx.a`). It needs no
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

**1. The content is read-only at `rom:/` and there is no `chdir`.** Every other
target `chdir`s to the binary's directory at startup and then opens `data/…`,
`grafs/…` relative to it. `swos_data_path()` (`include/swos_io.h`) anchors
those reads instead — identity on every other target, returning the caller's
own pointer with nothing copied. It is applied at the four places that open
game data: `resload.c`'s `resolve_path` (the funnel for every Amiga-named
asset), and the three JSON loaders (`translations.c`, `menu_load_json.c`,
`tactics_load_json.c`).

**2. `SDL_GetPrefPath` mounts `save:/` against the account chosen at launch,**
so `StartupUserAccount` must not be `None` — with `None` it returns nothing and
every career would be lost at exit. The mount is flat, so unlike the desktops
there is no `user/` and `user/save` split: `SWOS.ini` and the `.CAR` files
share it.

**3. A write to `save:/` is not durable until it is committed,** and SDL will
not do it. Without the commit a save looks written, reads back correctly for
the rest of the session, and is simply absent next launch. `swos_save_commit()`
calls `SDL_NintendoSwitch_CommitSaveData("save")` and is called from
`swos_io_save()` and `Settings_Save()`, in both cases **after** the file is
closed, which the commit requires.

**4. `printf` and `stderr` reach nobody.** The desktop builds redirect `stderr`
to `SWOS.log` in the user folder; on the Switch that is skipped — a log file on
the save mount would burn the player's quota and need committing to survive
anyway. Output goes to the target log; see [Debugging](#deploying-and-debugging).

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

`data grafs grafs_aga grafs_dos sound data_json` — the Amiga data, the two
Amiga graphics tiers, the DOS tier, the audio, and the editable JSON the
competition/menu/tactics tables bake from.

`save/` is deliberately **not** shipped: the console mounts the player's own
save at `save:/` instead, and `bin/save` in this checkout is 24 MB of test
careers. `grafs_2020` and `grafs_arcade` are also left out for now — arcade
mode is a coin-op cabinet mode that does not apply here.

---

## Metadata

`packaging/switch/swos.nmeta`. **Two values are borrowed and must be replaced
before submission:**

| field | current | why |
|---|---|---|
| `ApplicationId` | `0x01004b9000490000` | the SDK's Demo1 sample. Installing this build overwrites Demo1 if that sample is on the console, and two titles sharing an id overwrite each other's saves |
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
  other formats with a confusing message about baseline JPEG. Generate it with
  `python3 scripts/make_switch_icon.py` — it scales the game's own title logo
  (`bin/grafs_arcade/LOGO.bmp`) rather than committing an unexplained blob.
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
  input-related certification pain, and SWOS is local-only.
- Verify saves survive a relaunch, and that the commit is in the right places.
- Handheld/docked transitions, audio on the console's speakers, frame pacing.
- Cert has not been surveyed.
