# Shaders — a plan for a GPU present path

OpenSWOS draws into an **8bpp chunky frontbuffer** and has done since the port
began (CLAUDE.md, "Graphics pipeline is 8bpp chunky end-to-end"). Everything the
game does is palette indices; colour happens once, at the very end, in
`chunky_to_rgba`. That single boundary is what makes a GPU path attractive: the
conversion the CPU does every frame is a texture lookup, and once the palette
lives on the GPU a whole class of effects becomes possible that an overlay cannot
do — **true blended shadows** and **a curved screen** being the two that prompted
this.

This document is the plan, not the implementation. It exists so the work can be
done in the right order, and so the platform questions are decided up front
rather than discovered late. Two of them turned out to have answers already:
**the browser** sets the ceiling (WebGL2, §6), and **the Switch** — the target
that looked like the risk — offers more than we need and is the reason the
SDL3 question in §10 is worth asking at all.

> **Nothing here touches the Amiga target.** All of it lives behind
> `SWOS_SDL_BUILD`, and the m68k build keeps its copper list and its blitter.

## 1. What the present path is today

Per frame, in `src/platform_sdl.c`:

1. the game draws into the chunky frontbuffer (`_frontbufferBasePtr`, stride
   `FRONTBUFFER_ROW_STRIDE_BYTES`);
2. **`chunky_to_rgba`** (line ~1231) expands every pixel through `_gamePalette`
   into an ARGB8888 staging buffer — 336×272 = 91 392 lookups, ~366 KB written;
3. one `SDL_UpdateTexture` uploads it to a streaming texture;
4. `SDL_RenderCopyF` draws it with the sub-pixel scroll offset (`--subpixel`) and
   the integer pre-scale of `--scaling=smooth`, letterboxed to the window;
5. `CrtEffects_Apply` (`src/crt_effects.c`, 211 lines, derived from
   cannonball-se) blends a **generated overlay texture** — scanlines and an
   aperture mask baked into RGBA by `build()` — over the result;
6. `SDL_RenderPresent`.

It is honest, portable and entirely CPU-side. Its ceiling is also obvious: step 5
can only ever *multiply something on top*. A curved screen needs the sampling
position to move, and a blended shadow needs a second layer — neither is an
overlay.

## 2. The three things we want

| | want | why it needs more than an overlay |
|---|---|---|
| **Shadows** | a player's shadow darkens the pitch under it instead of being a palette colour | the shadow's own colour has to come from what it falls on |
| **Palette on the GPU** | upload indices, look up colour in a shader | removes `chunky_to_rgba` per frame; makes fades a 1 KB upload |
| **CRT geometry** | barrel/curved screen, edge rounding, proper mask and bloom | the sample position must move per pixel |

The middle one is the enabler: once the frame reaches the GPU as **indices**,
the other two are shader work.

## 3. Why not just add shaders to what we have

**SDL2's `SDL_Renderer` has no shader API**, and there is no supported way to mix
your own GL calls into it. So the options are:

| option | verdict |
|---|---|
| Raw **GL context** beside the existing renderer, chosen at startup | **this plan.** GL is the only API that reaches all six targets, including the browser |
| **SDL3 GPU API** (`SDL_GPU*`) | the nicest API of the three — one backend set, SPIR-V, no GLSL dialects — but **it has no WebGPU backend**, so the web build would still need the GL path and we would maintain two renderers. Revisit when that lands (§10) |
| **`SDL_RenderGeometry`** (SDL ≥ 2.0.18, and in SDL3) | no shaders at all: a warped triangle mesh with distorted UVs gives **curvature** on the plain renderer. Keep as the fallback for hosts with no usable GL |
| Metal/D3D backends directly | multiplies the work by the number of platforms for nothing we need |

**None of this needs SDL3.** `SDL_GL_CreateContext` is in both APIs, so the
backend below can be written today and survives the migration in §10 untouched.

So: **a second present backend**, selected at runtime, with the current one kept
as the fallback. Not a replacement — a fallback that never disappears is what
makes "works on Switch" a decision we can defer rather than a gamble.

## 4. The design

### 4.1 Layers

The GPU gets three things instead of one RGBA image:

| texture | format | size | uploaded |
|---|---|---|---|
| **index** | `GL_LUMINANCE`/`R8`, 1 byte/px | 336×272 = 91 KB | every frame |
| **palette** | RGBA8, 256×1 | 1 KB | only when `_gamePalette` changes |
| **shadow mask** | `GL_LUMINANCE`, 1 byte/px | 91 KB | every frame, only when shadows are on |

That is **a quarter of today's upload** for the frame itself, and the palette —
which fades rewrite constantly — stops costing a full-frame re-expansion.

### 4.2 The shaders

**The tier is GL 3.3 core on the desktop and GLES 3.0 everywhere else** — one
shader body, **two** preambles (`#version 330 core` / `#version 300 es` +
`precision mediump float;`). That pair covers every target we ship: desktop GL
3.3 is 2010 hardware, macOS tops out at 4.1 core, the Switch offers GL 4.6 and
GLES 3.2, and the browser's WebGL2 *is* GLES 3.0.

It is also the easier target, which is the part worth noticing: GLES2 would have
forced a half-texel fudge to hit the middle of a palette entry, and
`GL_LUMINANCE` because it has no `GL_R8`. At this tier `texelFetch` indexes the
palette **exactly**, by integer, with no filtering and no rounding to argue about:

```glsl
// fragment — body shared, preamble per target
in  vec2 vUV;
out vec4 oColor;
uniform sampler2D uIndex;    // R8, NEAREST
uniform sampler2D uPalette;  // RGBA8 256x1
uniform sampler2D uShadow;   // R8: 0 = lit, >0 = in shadow
uniform float     uShadowK;  // 0 = shadows off

void main() {
    int   idx = int(texture(uIndex, vUV).r * 255.0 + 0.5);   // exact for 8-bit unorm
    vec3  rgb = texelFetch(uPalette, ivec2(idx, 0), 0).rgb;  // no seam, no half-texel
    float s   = texture(uShadow, vUV).r;
    oColor = vec4(rgb * (1.0 - s * uShadowK), 1.0);
}
```

If a host ever turns out to be stuck below this tier, it takes the
`SDL_RenderGeometry` fallback rather than a third shader dialect.

Barrel distortion is a vertex-stage or fragment-stage remap of `vUV` before the
first sample; the aperture mask, scanlines and vignette then apply to the result,
in the same pass, with the curvature already applied — which is precisely what
the overlay cannot do (its scanlines stay straight while the picture bends).

### 4.3 What must not change

The GL path is a **drop-in for step 4–6 only**. Everything the SDL path does for
presentation has to survive it, or it is a regression dressed as a feature:

- `--scale`, `--fullscreen=desktop|exclusive`, Alt+Enter, `+`/`-`;
- `--subpixel` (the fractional scroll offset — it becomes a UV offset, which is
  *more* exact than the current quantise-to-output-pixel);
- `--scaling=sharp|smooth` (the integer pre-scale becomes a sampling decision);
- letterbox bars and the window icon;
- **`SWOS_PRESENT_CAPTURE` / `SWOS_SCREENSHOT_INTERVAL`** — they call
  `SDL_RenderReadPixels`, which a GL path must answer with `glReadPixels`, or
  every render test in the project goes blind (docs/TESTS.md,
  `feedback_verify_render_changes_by_looking`);
- `--renderer=software` must still reach the CPU path untouched.

### 4.4 What must not move

**Rendering writes match state** (CLAUDE.md, netplay section: `GameLoop_Frame`'s
draw calls are part of the simulated frame). So the rule for every phase below
is: *the contents of the frontbuffer at the end of a frame may change; what the
simulation reads must not.* Any phase that moves pixels out of the frontbuffer —
phase 2 does — is not done until `scripts/rollback_test.sh --all`,
`scripts/netplay_test.sh --all` and `scripts/replay_v2_test.sh --all` pass
unchanged.

## 5. Shadows — and why the cheap version comes first

**A shader cannot blend a shadow that is already in the frontbuffer.**
`blitChunkySprite` (`src/sprites.c:886`) ends with

```c
if (src) dstRow[dx] = src;
```

so by the time the frame reaches the GPU the grass under the shadow has been
overwritten. There is no background left to see through. Two ways out, and they
share the same change at the blit site:

**(a) Blend on the CPU, at blit time — works today, on every target.**
The background *is* still there when the sprite is written. A shadow pixel looks
up a **darkening LUT** instead of writing its own colour:

```c
uint8_t src = srcRow[px];
if (src) dstRow[dx] = s_darken[src] ? s_darken[src][dstRow[dx]] : src;
```

`s_darken[shade]` is a 256-byte table: for each palette entry, the entry nearest
to that colour × the shade's factor. Seven shades (the Jaguar's shadow ramp) is
1792 bytes, built once per palette change with the nearest-entry search
`jag_graphics.c` already has. The shadow then falls correctly on grass, on the
penalty spot, on the white lines and on other players — none of which the current
"recolour the shadow to match the pitch" can do.

Its limit is the palette: "grass at 70%" has to *exist* as an entry, or the
nearest one is used. Our palette carries dimmed copies of everything
(docs/JAGUAR.md), so in practice it is close; on a white line it will visibly
snap. Overlapping shadows also darken twice.

**(b) Blend on the GPU, from a mask — smooth, needs phase 1.**
The blit writes the shadow's *darkness* to the mask layer and leaves the
frontbuffer alone; the shader multiplies. No palette quantisation, no
double-darkening if the mask is written with `max()`, and the shadow survives the
CRT pass as part of the picture.

Both need the same thing first: **the blitter has to know which indices are
shadow**, a 256-entry flag table owned by whoever drew them (`jag_graphics.c`
today). Do (a) now — it is ~15 lines and ships the visible win — and it becomes
the fallback for (b) on hosts with no GL.

## 6. Platforms

One shader body, a per-target preamble, and a fallback chain that always ends
somewhere that works.

| target | context | shading language | notes |
|---|---|---|---|
| Linux | GL 3.3 core | `330 core` | llvmpipe under WSL2 reaches 4.5 but is slow — **must** fall back cleanly, not crawl |
| Windows (Cygwin) | GL 3.3 core | `330 core` | the exe runs on sdl2-compat over SDL3 (CLAUDE.md), so the context comes from SDL3 underneath. Verify on a machine with **only the four shipped DLLs** |
| macOS | GL 3.3 core (4.1 max) | `330 core` | GL is deprecated but present. A core profile needs a VAO bound or it draws nothing, silently |
| WASM | WebGL2 = GLES 3.0 | `300 es` | needs `-sMAX_WEBGL_VERSION=2`; WebGL2 is ~97% of browsers, and WebGL1-only hosts take the fallback |
| **Switch** | GL 4.6 core/compat, **GLES 3.2**, or Vulkan 1.0 | `300 es` | the SDL3 NX port sets up an EGL surface for `SDL_WINDOW_OPENGL` and states all three outright; no `nvn` renderer is needed (§8) |
| Amiga | — | — | not applicable, not compiled |

### Traps, by platform

- **The render width is adaptive, and the textures must follow it.**
  `FRONTBUFFER_ROW_STRIDE_BYTES` is `_screen_width` on SDL, so the rows *are*
  tight and a whole-buffer `glTexSubImage2D` is correct — but `_screen_width` is
  derived from the monitor's aspect ratio at startup (336 / 480 / 672 …,
  `include/display.h`), so the textures are sized per width and recreated when it
  changes. Set `GL_UNPACK_ALIGNMENT = 1`: the default of 4 silently skews every
  row the moment a width is not a multiple of 4, and 336/480/672 all are, so it
  will look fine right up until someone's monitor gives a width that is not.
  (GLES 3.0 *does* have `GL_UNPACK_ROW_LENGTH` if we ever need it — WebGL1 did
  not, which is one more reason the tier moved up.)
- **`GL_RED`/`GL_R8` needs the swizzle thought through**: sampling an R8 texture
  gives `(r,0,0,1)`, which is what the shader wants (`.r`), but anything that
  expects luminance-style replication will look black.
- **NPOT textures** in WebGL1/GLES2 need `CLAMP_TO_EDGE` and no mipmaps. We want
  `NEAREST` + `CLAMP_TO_EDGE` anyway; getting it wrong gives a black screen, not
  a warning.
- **GLSL ES needs precision qualifiers** (`precision mediump float;`) that
  desktop GLSL rejects — hence the preamble split, not `#ifdef` inside the shader
  body. Keep the body free of `#version`, `in`/`out` aside, so the two preambles
  stay the only difference between the browser's shader and the Switch's.
- **Shaders are C string literals**, not files. A file would mean an asset tier,
  a WASM preload entry, a Switch package entry and a `resload` path for something
  that is 40 lines of text. Embedding also means a shader can never go missing in
  a drop.
- **macOS core profile needs a VAO bound** or nothing draws, silently.

### The fallback chain

```
--renderer=gl      -> GL 3.3 core / GLES 3.0; if the context or a shader fails, say so and fall through
--renderer=accel   -> today's SDL_Renderer (+ SDL_RenderGeometry mesh for curvature)
--renderer=software-> today's CPU path, no effects beyond the overlay
```

A failed shader compile must print its log and fall back. A black screen with no
message is the failure mode to design against.

## 7. Phases, each shippable

| phase | what | done when |
|---|---|---|
| **0** | Shadow LUT on the CPU (§5a) + the shadow-index flag table | a Jaguar shadow visibly darkens the white line it crosses; `rollback/netplay/replay_v2` suites unchanged |
| **1** | GL backend that reproduces today's output **exactly**: index + palette textures, no effects | `SWOS_PRESENT_CAPTURE` frames from `--renderer=gl` and `--renderer=accel` match pixel for pixel at scale 1; measured upload drops ~4× |
| **2** | Shadow mask layer, composited in the shader (§5b) | smooth shadows; the CPU LUT still runs on the fallback path and the suites still pass |
| **3** | CRT shader: curvature, aperture mask, scanlines, vignette, bloom — and the `SDL_RenderGeometry` mesh fallback for curvature | side-by-side with `cannonball-se` for the mask/scanline look; the fallback bends the picture too |
| **4** | Switch validation (§8) | the `.nsp` runs the GL path, or is proven to need the fallback |

Phase 1 is the one worth being strict about: **if it is not pixel-identical, it
is not a backend, it is a second look.** Every later phase then has a known-good
baseline to diff against, which is the only way to tell a shader bug from a
palette bug.

## 8. The Switch, which is the best-placed target, not the worst

The worry was that the NDA SDL fork might have no shader path. The opposite is
true. **`D:\Projects\GitHub\SDL3-switch` is icculus's official NDA SDL3 port for
NX**, and its README says it plainly:

> If you create a window with `SDL_WINDOW_OPENGL`, SDL will set up an EGL surface
> and `SDL_GL_CreateContext` will work. OpenGL 4.6 (Core and Compatibility
> profiles) and OpenGL ES 3.2 are supported. `SDL_WINDOW_VULKAN` will produce a
> Vulkan 1.0-compatible surface.

and, on rendering: *"There is not an 'nvn' backend for rendering, as OpenGL,
GLES, and Vulkan are supported on the Switch."* The SDL2-era evidence in
docs/SWITCH.md agrees from the other side — the binary already links
`opengl.nss` + `libnn_gll.a`, and its SDL builds `render/opengl` and
`render/opengles2`.

So the Switch comfortably clears the §6 tier — it is the only target that offers
*more* than we ask for. Two notes about that repo specifically:

- **It is a platform overlay, not a fork of the whole library.** It ships
  `src/{video,audio,joystick,filesystem,storage,locale,loadso,main}` and private
  headers, to be merged into an upstream SDL3 tree — so *we* choose the SDL3
  revision, and it is not a stale fork we inherit.
- It brings NX behaviours worth reading before bring-up: `SDL_GetBasePath()`
  mounts the ROM filesystem as `rom:/`, `SDL_GetPrefPath()` returns `save:/` for
  the preselected user and **requires `SDL_NintendoSwitch_CommitSaveData()` or
  saves are lost**, and GL contexts survive backbuffer recreation. The save path
  interacts with `swos_io.c` (`project_user_paths`), not with shaders.

What is still unproven is only that it **runs**: the Switch target has never been
linked or launched. So phase 4 stays a smoke test — create the context, compile
the phase-1 shader, print `GL_VERSION`/`GL_SHADING_LANGUAGE_VERSION` and the
log, present one frame — but it is now expected to pass, and the browser's
`300 es` shaders are what it should run.

## 9. Open questions

- **Does anything read the frontbuffer back for logic?** Phase 2 moves shadow
  pixels out of it. `LoadFileEx` stages through the frontbuffer, and the sprite
  undraw path is dead on SDL (the pitch is redrawn per frame) — but this needs an
  audit, not an assumption, before phase 2.
- **Should the stock 16-colour shadow get the same treatment?** Colour 8 is one
  near-black colour; blending it would look better and would no longer be SWOS.
  Jaguar-only by default, an option at most.
- **Does the CRT shader run before or after the letterbox?** Before means the
  curve bends the picture inside its bars (right); after means the bars bend too
  (wrong, and what a naive full-screen pass gives you).
- **`--scaling=smooth`'s integer pre-scale** may be redundant once sampling is a
  shader decision. Prove it with captures before deleting anything.

## 10. Should we move to SDL3?

**Yes — but as its own piece of work, not as part of this one.** The two
decisions look joined and are not: `SDL_GL_CreateContext` exists in both APIs, so
every line of the GL backend above is written once and survives the migration.
Gating a rendering feature on a platform-wide dependency change would delay the
visible work behind the risky work, for no technical gain.

**What SDL3 is actually worth, in this project's terms:**

- **The Switch's only SDL is SDL3.** The port above is SDL3, official and under
  the same NDA we already hold. Bringing that target up on SDL2 means an older
  unofficial fork; bringing it up on SDL3 means the supported path.
- **It deletes a whole class of Windows bug.** `bin/SWOS.exe` links
  **sdl2-compat**, a shim that `dlopen`s `cygSDL3-0.dll` — the invisible
  dependency behind the tester report in CLAUDE.md ("`Failed loading SDL3
  library.`") and the reason the drop needs a four-DLL rule derived by running
  `cygcheck` on a *library* rather than the exe. Linking SDL3 directly removes
  the shim, one DLL, and the `dlopen` that `cygcheck` cannot see. **We are
  already running on SDL3 on Windows** — through a translation layer.
- SDL2 is in maintenance; Wayland, HiDPI, gamepad and audio work happens in SDL3.
- Its audio model suits us: the WSL2 "push from the game loop" queue
  (`project_wsl2_audio_queue_mode`) is exactly what `SDL_AudioStream` +
  `SDL_PutAudioStreamData` is for.

**What it costs, measured rather than guessed:**

| | |
|---|---|
| SDL call sites in the tree | **380** |
| of which in `platform_sdl.c` (150), `audio_sdl.c` (76), `input_sdl.c` (46) | **272 — 72%** |
| the rest | `arcade.c` 17, `crt_effects.c` 13, `swos_io.c` 10, `netplay_match.c` 9, `main.c` 9, tail |

The port already isolates SDL behind platform files, which is what makes this a
contained migration rather than a rewrite. The renames with real teeth:
`SDL_GameController*` → `SDL_Gamepad*` (22 uses), `SDL_GetTicks` returning
`Uint64` (24 uses — check every subtraction and wrap assumption, our frame pacing
and netplay clock live here), `SDL_RenderCopy`/`CopyF` → `SDL_RenderTexture`
(12), `SDL_QueueAudio` → `SDL_PutAudioStreamData` (4, inside a 76-call file that
is the real audio work). Upstream ships `build-scripts/rename_symbols.py` for the
mechanical half.

**Per-target availability, checked:**

| target | SDL3 today |
|---|---|
| Switch | the NDA port above ✓ |
| Windows | Cygwin already ships `cygSDL3-0.dll` ✓ (we link it through a shim as it is) |
| WASM | emscripten 6.0.1 here has an `sdl3` port ✓ (`tools/ports/sdl3.py`) |
| macOS | Homebrew `sdl3`; the `.app` bundles `SDL3.framework` instead of SDL2's |
| Linux | **Ubuntu 22.04 here has no `libsdl3-dev`** — build it for the four cross targets and bundle `libSDL3.so.0` in `SWOS-full.zip` in place of `libSDL2` (and re-check whether libdecor is still needed: CLAUDE.md bundles it *because SDL2 is its only consumer*) |

**And `SDL_GPU`?** Not yet, and the reason is the browser: SDL3's GPU backends
are Vulkan, D3D12 and Metal, with no WebGPU backend, so the web build would still
need the GL path — two renderers to keep in step for one API's convenience. Worth
revisiting the day a WebGPU backend lands, at which point the Switch could use
its Vulkan 1.0 surface through the same code.

**Recommended order:**

1. **GL backend on SDL2** (phases 0–3). Isolated in `platform_sdl.c`, delivers
   the shadows and the curved screen, touches neither audio nor input.
2. **SDL3 migration**, on its own branch, target by target, with every suite
   re-run (`netplay_test.sh`, `replay_v2_test.sh`, `rollback_test.sh` —
   `SDL_GetTicks` alone is reason enough) and the Windows drop re-verified from
   a DLL-only directory.
3. **Switch bring-up on SDL3**, its first link ever, against the GL path that is
   by then proven on five targets.

**The one thing that flips this order:** if Switch bring-up moves to the front of
the queue, do SDL3 first. That target has never been linked, its SDL *is* SDL3,
and there is no sense writing its context-creation code twice.
