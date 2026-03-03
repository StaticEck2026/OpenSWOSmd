# CLAUDE.md — SWOS (Amiga 68000) Assembly → C Port Plan

## Project Summary

This is a port of the Amiga Motorola 68000 game **Sensible World of Soccer (SWOS)** from assembly (`SWOS2.s`) to C (with support code in `Resload.s`).

`SWOS2.s` is a **reverse engineered** assembly reconstruction (IDA Pro 9.0). Treat it as mostly correct but not infallible—especially around data offsets, hard-coded addresses, and pointer arithmetic after data relocation.

Build environment:
- VSCode extension: **Amiga C/C++ Compile, Debug & Profile** by Bartman^Abyss
  https://marketplace.visualstudio.com/items?itemName=BartmanAbyss.amiga-debug

### How to Build

The project **must** be built through the VSCode extension — not from a raw terminal/cygwin shell.

**Build from VSCode:** Use the default build task (`Ctrl+Shift+B` or Terminal → Run Build Task).
This runs the "compile" task defined in `.vscode/tasks.json`.

**What the build task does:**
- Extension path: `C:\Users\headk\.vscode\extensions\bartmanabyss.amiga-debug-1.7.9\bin\win32\`
- Runs: `gnumake.exe -j4 program=uae/dh0/data/SWOS2`
- Sets PATH to include `<ext-bin>\opt\bin` (for `m68k-amiga-elf-gcc`) and `<ext-bin>` (for `vasmm68k_mot`, `elf2hunk`)
- `${command:amiga.bin-path}` in tasks.json resolves to the extension's platform-specific bin directory at runtime

**Why not from terminal:** The Makefile checks `ifdef OS` (set on Windows) and uses `SHELL = cmd.exe` with Windows-specific commands. The m68k cross-compiler toolchain is bundled inside the extension and is NOT on the system PATH. Running `make` from cygwin/bash will silently fail.

**Build outputs:**
- Binary: `uae/dh0/data/SWOS2` (Amiga hunk executable)
- ELF: `uae/dh0/data/SWOS2.elf`
- Map: `uae/dh0/data/SWOS2.map`
- Object files: `obj/*.o`

**Clean:** Run the "clean" task from VSCode (Terminal → Run Task → clean)

**CLI build (from Claude Code):** Use `build.cmd` which sets up PATH and invokes gnumake:
```
C:/Windows/System32/cmd.exe //c C:\\Projects\\AI\\SWOS\\SWOS_C\\build.cmd -j4 program=uae/dh0/data/SWOS2
```
The `//c` flag is needed when invoking cmd.exe from cygwin/msys bash (single `/c` is interpreted as a path).

Current status:
- The project **compiles and runs from `main`** in `main.c`.
- Only the **`start`** assembly routine has been ported to C:
  - Ported routine name: `_start`
  - Location: `SWOS.c`
- Most code is still in `SWOS2.s`.
- `Resload.s` is also part of the runtime: it provides the loader/utility routines used by this HDD-run version (it replaces the original WHDLoad-style interfaces, so it must be ported to C as well).

Goal:
- Port **all** remaining subroutines from `SWOS2.s` (and `Resload.s`) to C in small, safe chunks.
- Keep the project **compiling at all times**.
- Keep a running **progress log** here so a new session can resume without re-discovery.
- Follow the **Project Phases** below — each phase has a hard stop-gate before proceeding.

### Floppy Disk Routines — SKIP

The floppy disk routines (`FloppyAsset_DispatchOp`, `FloppyDev_TransferImpl`, `Floppy_ParseDrivePrefix`, `Floppy_StreamInit`, `Floppy_LoadByName`, `Floppy_SelectId`, `Floppy_DumpStatus`, `Floppy_SendTextRecord`, `Floppy_AllocTmpEntry`, `Floppy_ClearTmpEntry`, `Floppy_ReadFromTmpEntry`, etc.) do **not** need to be ported.

The game is already patched to use HDD routines via `Resload.s` — the floppy code paths are dead code. Leave them in `SWOS2.s` as-is and ignore them during porting.

---

## Project Phases — MANDATORY STOP-GATES

This project follows a strict phased approach. **Do NOT skip phases or combine them.**

### Phase 1: Subroutine Porting (CURRENT)
Port all remaining subroutines from `SWOS2.s` and `Resload.s` to C.

**Completion criteria:** Zero unported subroutines remain (only data declarations, forwarding stubs, and dead floppy code left in assembly).

**When Phase 1 is complete, you MUST:**
1. Output exactly: `*** READY FOR TESTING PHASE! ***`
2. **STOP all porting work immediately.** Do not proceed to Phase 2 or Phase 3.
3. Wait for the user to confirm the game runs correctly before moving on.

### Phase 2: Runtime Testing and Bug Fixing
The user will test the game in the emulator. Any bugs found will be reported back for fixing.

**Completion criteria:** The user confirms the game is running correctly and gives explicit approval to proceed.

**Do NOT start Phase 3 until the user explicitly says the game is working and gives the go-ahead.**

### Phase 3: Assembly Code Removal
Once testing passes, remove all remaining assembly **code** (stubs, dead floppy routines) from `SWOS2.s` and `Resload.s`, leaving **only data declarations** in assembly.

**Completion criteria:** Assembly files contain only data (initialized variables, tables, BSS). All code is in C. Build still works.

**After Phase 3, stop and test again before proceeding to Phase 4.**

### Phase 4: Data Migration (Assembly → C)
Gradually replace assembly data declarations with initialized C data. **This phase requires extreme care:**

- Data in the original assembly is **position-sensitive** — many variables must remain sequential/contiguous in memory because game code relies on adjacent layout (see "Boundary Spill" section above).
- Migrate data in **small, tested batches** — not all at once.
- After each batch:
  1. Build and verify the map file confirms correct placement.
  2. **User tests the game** to confirm nothing broke.
  3. Only then proceed to the next batch.
- Use the linker script (`SWOS2.ld`) to control section placement and ensure Chip RAM data stays in Chip RAM.
- When data has known adjacency dependencies (contiguous globals), migrate the entire contiguous group together as a struct or adjacent declarations — never split them across separate migration batches.
- Document each migration batch in PROGRESS.md with: which symbols moved, which memory section, any adjacency constraints preserved.

**This phase will require a coordinated plan before starting — enumerate all data groups, identify adjacency dependencies, plan the migration order, and get user approval before executing.**

---

## Required C Types

Use **only** these integer types:

- `int8_t`
- `int16_t`
- `int32_t`
- `uint8_t`
- `uint16_t`
- `uint32_t`

Avoid `long`, `short`, `char` (except for raw byte buffers and char* strings), `size_t`, `bool` unless there’s a strong reason and it does not obscure the 68k semantics.

## Struct Access — Use Typed Structs, Not Byte Offsets

All game struct definitions live in **`include/game_types.h`**: `Sprite`, `TeamGeneralInfo`, `TeamGame`, `TeamStatisticsData`, `PlayerInfo`, `GoalieSkill`, `SpriteGraphics`, `GoalInfo`, `ScorerInfo`, `FixedPoint`.

**When porting new code, always use proper struct access:**

- Declare variables with the correct struct type: `Sprite *player`, `TeamGeneralInfo *team`, `TeamGame *teamGame`
- Access fields via `->` or `.`: `player->x.s.i`, `team->controlledPlayer`, `teamGame->prShirtCol`
- For FixedPoint (16.16) fields: `.raw` (full int32_t), `.s.i` (int16_t integer part), `.s.frac` (uint16_t fractional part)
- Include `"game_types.h"` in both `.c` and `.h` files (it provides typedefs too, replacing manual `typedef unsigned char uint8_t` blocks)
- Declare externs with proper types: `extern Sprite ballSprite;`, `extern TeamGeneralInfo topTeamData;`, `extern TeamGame topTeamInGame;`
- For pointer-valued globals defined in asm, use typed pointers (`extern Sprite *lastPlayerPlayed;`), NOT `extern uint32_t` — the latter causes LTO type-mismatch warnings

**Do NOT use byte-offset macros** like `RD16(ptr, SPRITE_X)`, `WR16(ptr, offset, val)`, `*(int16_t *)(ptr + 30)`, `TG_W(team, TEAMGAME_PRSHIRTTYPE)`, etc. These were fully removed from the codebase.

---

## Reference Materials

The `data/` folder contains helpful artifacts:

- `data/Data.c` — initialized data from IDA
- `data/Data.h` — header for IDA initialized data
- `data/SWOS2.c` — Ghidra decompiled C (reference only; not authoritative)
- `data/SWOS2.h` — Ghidra header (structs/types reference)
- `data/SWOS2_IDA.h` — IDA header (structs/types reference)
- `data/SWOS.s` — the original assembly file for reference

Use these to:
- identify structs and field layouts
- cross-check calling conventions
- match globals/labels
- validate control-flow and bit-level logic

---

## Memory Layout, CHIP RAM, and Link Validation

### CHIP RAM must remain CHIP RAM
Data stored in **Chip RAM** must stay there. In particular, variables declared in the **`BSSInChip`** section in `SWOS2.s` must continue to be allocated in Chip RAM after porting.

Practical rules:
- If a variable currently lives in `BSSInChip`, do **not** accidentally move it to a default `.bss`/FAST RAM region when translating to C.
- When translating such globals to C, place them in a dedicated linker section that maps to Chip RAM, matching the original intent (see `SWOS2.ld` for memory layout).

### Linker script is authoritative
The file **`SWOS2.ld`** contains memory layout and section placement rules (including Chip RAM placement). Use it as the primary reference when adding new C modules/sections or when migrating globals out of assembly.

### Validate placement with the map file
After each build, inspect:

- `uae\dh0\data\SWOS2.map`

Use the map to confirm:
- code and data are landing in the expected memory regions (Chip vs Fast)
- `BSSInChip`-equivalent data is still in Chip RAM
- no large unintended shifts in segment sizes/addresses (which can expose hard-coded offset bugs)


---

## Overlapping Data, Code-as-Data, and Boundary Spill Issues

The original game (and therefore `SWOS2.s`) may intentionally **overlap memory regions** or reuse an earlier **code block as a data buffer** after it is no longer executed. This is common in highly memory-constrained Amiga/68k code and can also be an artifact of reverse-engineering.

### Pattern A: “Code label is actually data”
Example (from `ResetResultSprites` in `SWOS2.s`):

```asm
                lea     (ShowLoaderIntroSequence).l,a3 ; ScorerInfo team1Scorers[8]
                lea     ((loc_10FE24+2)).l,a4          ; ScorerInfo team2Scorers[8]
```

In your comments you identified that:
- `ShowLoaderIntroSequence` should really be `team1Scorers`
- `loc_10FE24+2` should really be `team2Scorers`

**Porting rule:** do not preserve these “fake” code-label aliases long-term. Move the backing storage into **real variables**:
- define `team1Scorers` and `team2Scorers` as explicit globals (in the correct memory section)
- update call sites / `lea` references to point at the real symbols
- if the original build relied on this region being in CHIP RAM, ensure the replacement storage also lands in CHIP RAM

### Pattern B: Boundary spill (reads/writes past the declared end)
Sometimes assembly references data **beyond the end of a label’s intended C declaration**, relying on the fact that the next label is placed immediately after it in memory. This can show up as:
- indexed reads/writes that exceed the “official” array length
- memcpy/clear loops that run longer than the declared object size
- struct field access that assumes extra trailing fields/padding from an adjacent object

In these cases, a naive C translation that “tightens” sizes will break behavior.

**Mitigation options (choose the least invasive that preserves layout):**
1) **Create a wrapper layout struct** that matches the real contiguous memory region, e.g.
   - `struct Combined { TypeA a; TypeB b; /* ... */ };`
   - and expose pointers/aliases for `a` and `b` as needed
2) **Increase the C declaration size** to cover the actual accessed range (and document why).
   - Example: enlarge an array to include “spill” bytes, or add explicit trailing padding:
     - `uint8_t pad[N];`
3) **Keep the object in assembly until verified**, while porting the logic that uses it.
4) **Add explicit symbols for the “spill” region** if it’s semantically distinct, rather than relying on adjacency.

### How to detect these issues
- Look for `lea`/`movea` of **code labels** into address registers followed by data-like indexing.
- Look for `clr`/`move` loops where the loop count does not match the declared C size.
- Search for symbol+offset patterns that “walk” across neighboring objects.
- Use `uae\dh0\data\SWOS2.map` to confirm whether two objects were originally contiguous and in which memory region.

### Documentation requirement
When you identify a code-as-data or boundary-spill dependency:
- fix the declarations/layout so the behavior remains correct after porting
- add a short note either in:
  - the local module header (`*.h`) next to the declarations, **and**
  - the “Bug Log” / “Pitfalls” area of this document if it was subtle or caused breakage

---

## High-Level Porting Strategy

### Keep it compiling: “chunked port”
Port small groups of subroutines at a time (5–30 functions depending on complexity), and verify:
1) the project **compiles**
2) it **links**
3) the **link/map output** confirms correct memory placement (see map validation section)

### Split the C port into modules
Do not grow one giant C file. Break translated code into thematic files, e.g.:

- `SWOS.c` / `SWOS.h` (core entrypoints + glue)
- `menus.c` / `menus.h`
- `sprites.c` / `sprites.h`
- `render.c` / `render.h`
- `audio.c` / `audio.h`
- `input.c` / `input.h`
- `teams.c` / `teams.h`
- `filesystem.c` / `filesystem.h`
- `math68k.c` / `math68k.h` (mul/div, fixed-point helpers, bit ops)
- `mem_amiga.c` / `mem_amiga.h` (Chip/Fast allocations, DMA/copper helpers if needed)
- `debug.c` / `debug.h` (optional instrumentation)

Prefer “one header per module” with clear exported symbols.

---

## Naming + Collision Rules

This project will temporarily have both:
- C implementations of routines
- assembly stubs (or remaining assembly routines)
- IDA/assembly globals

To avoid symbol collisions and to make it obvious what is “ported”, use these conventions:

### Functions
- C-ported functions: prefix with underscore: `_<name>`
  - example: `_start` (already done)
  - example: `_ShowMenu`, `_DrawSprite`, `_AudioIRQUpdate`
- Assembly stubs/exported wrappers should keep original names **without underscore** when needed.

This makes it possible to:
- keep original symbol names in assembly for call sites
- redirect calls to the C implementation by changing the call site or by providing a small asm stub that calls `_name`

### Globals / Variables
- C-side globals that correspond to original variables: prefix with underscore by default: `_<name>`
- Keep original global names in `data/Data.c` / `data/Data.h` as-is, unless there is an explicit reason to rename.
- If both exist, the underscore form should be “the new one”, and the non-underscore is “legacy/original”.

---

## “Always Compiles” Plan (critical)

When you translate a subroutine to C, you may remove it from `SWOS2.s` to reclaim space. That can break:
- calls from other assembly code
- references to symbols (functions, labels, data)
- linkage order expectations

To prevent the build from breaking, use a two-layer approach:

### Layer 1: Assembly stubs (preferred)
After removing a function body from `SWOS2.s`, replace it with a tiny stub that preserves the symbol and forwards to C.

Example (conceptual 68k):
- Original symbol: `Foo`
- New C function: `_Foo`

Stub keeps the original entrypoint alive:
- `Foo: jmp _Foo` (or `jsr _Foo; rts` depending on ABI and return expectations)

This ensures:
- any remaining assembly callers still link
- the C implementation is used immediately

### Layer 2: Temporary C stubs (fallback)
If you cannot yet port the full behavior but need the symbol:
- implement `_Foo` as a stub that returns safely
- add TODO and expected semantics (inputs/outputs/register behavior)
- keep the project compiling while you port dependencies in a later chunk

### Layer 3: Minimal “export map”
Maintain a small mapping list (in this file) describing:
- original function name
- C function name
- stub status
- dependencies remaining in asm

This prevents “lost work” between sessions.

---

## Calling Convention & Register Semantics

The original code uses 68k register calling heavily.

When translating to C:
- Model register-passed parameters explicitly in the C signature where possible.
- If the build system uses compiler extensions for register parameters, consider using them only if stable; otherwise:
  - pass explicit arguments in C and create a tiny asm wrapper to shuffle registers → stack args.

Return values:
- Single return: normal C return.
- Multi-register return (e.g., `@<d1:d2>`) should use a struct return in C:
  - `typedef struct { int16_t x; int16_t y; } Position;`
  - Return `Position` by value.

Preserve side effects:
- Some routines return results via pointers or global variables.
- Some routines clobber registers intentionally. Do not rely on C register allocation matching; explicitly encode outputs.

---

## Porting Workflow (repeatable)

For each chunk:

1) **Select a chunk** of functions that are tightly related (same subsystem).
2) Build a dependency list:
   - called functions
   - global variables used
   - hardware registers accessed
3) Create/extend a module (e.g., `audio.c`).
4) Translate function-by-function:
   - keep logic identical first (no refactors)
   - preserve signedness and bit widths
   - keep “magic numbers” intact initially
5) Add any required structs/enums to the appropriate header.
6) Add assembly forwarding stubs for the translated functions.
7) Build.
8) Review the generated map file and section placement for regressions.
9) Update **Progress Log** in this file.

**Never** translate a large amount of code without a compile check.

---

## Verification Checklist (per chunk)

- [ ] `make clean && make` succeeds
- [ ] Binary outputs to expected path under `uae/dh0` (per project config)
- [ ] `uae\dh0\data\SWOS2.map` confirms code/data are placed in the correct memory sections
- [ ] No new unresolved symbols
- [ ] Stubs are in place for any removed assembly routines
- [ ] Updated module headers compile cleanly (no implicit declarations)

---

## Progress Log

> **Full progress log moved to `.claude/PROGRESS.md`** to prevent prompt size overflow.
> Read it with: `Read .claude/PROGRESS.md`

**Current status:** 1373 functions ported across 152 chunks.

**Last completed chunk:** Chunk 152 — DIY League/Tournament Match Callbacks & Visibility (8 → diy_competition.c): 6 match callbacks + MatchCommon + UpdateMenuEntryVisibilityAndFocus.

**Next candidates:** DIYLeagueTournamentOnRestore, DIYLeagueTournament16BeforeDraw, ShowDiyLeaguesTournamentMenu, Int2StrWithCommas/Int2Ascii*, WriteStatsNumber, AI_ResumeGameDelay, remaining career/transfer/squad functions.

---

## Porting Notes / Rules of Thumb


## Reverse-Engineering Pitfalls and Hard-Coded Offset Bugs

Because `SWOS2.s` is reconstructed from IDA, subtle mistakes can exist—especially when data has moved from the original binary. These issues may look plausible but be wrong in a way that breaks logic after relocation.

Example pattern:
- An instruction uses a symbol+offset where the real intent was a fixed constant:
  - `ori.l   #(goalBase+$2292),d0` **should be** `ori.l   #$10000,d0`

Common sources of failure:
- hard-coded absolute addresses that were not fully normalized during RE
- symbol-relative offsets that should have been literal constants
- struct field offsets that are off by a word/long due to incorrect type/packing assumptions
- pointer arithmetic that assumes an old layout

### If you find one of these bugs
1) Fix it (prefer the minimal change that restores the intended semantics).
2) Record it here so it isn’t rediscovered later.

#### Bug Log Template (append entries)
- Date:
- Location in `SWOS2.s` (label + line/address):
- Symptom:
- Root cause:
- Before:
- After:
- Why it was wrong (reasoning/evidence):
- Notes / Follow-ups:

#### Bug #1 — IDA address-as-constant in UpdateBall keeper-holds-ball deltaZ
- Date: 2026-02-19
- Location: `SWOS2.s`, UpdateBall body, original line ~9517 (`move.l #(frontbufferBaseAlt+$1CAA),d3`)
- Symptom: deltaZ for keeper-holds-ball (Z<5) path would use garbage address value instead of physics constant
- Root cause: IDA interpreted raw binary value 0x00010002 as symbol+offset (frontbufferBaseAlt+0x1CAA = 0x64058, not a valid deltaZ)
- Before: `move.l #(frontbufferBaseAlt+$1CAA),d3`
- After: `d3 = 0x00010002` (literal constant in C port)
- Why: 0x00010002 is the positive counterpart of 0xFFFEFFFE used for the Z>5 branch. The two branches handle ball rising vs falling: Z<5 needs positive delta (push ball up), Z>5 needs negative (bring ball down). Exact negation: 0x00010002 = 65538, ~65538+1 = 0xFFFEFFFE. Map file confirmed frontbufferBaseAlt=0x623ae, so 0x623ae+0x1CAA=0x64058 which is nonsensical as a physics value.

- Preserve behavior first, refactor later.
- Treat `uint16_t` vs `int16_t` seriously (branch conditions depend on signedness).
- Prefer explicit casts when mixing widths.
- Avoid C UB (shifts, signed overflow). Mirror 68k behavior explicitly.
- Where assembly uses `swap`, `ext`, `asr/lsl`, model it with explicit masking and casts.

---

## Session Handoff Notes (how to resume quickly)

When starting a new session:
1) Read `.claude/PROGRESS.md` for the full progress log.
2) Check the **Progress Log** summary above for current chunk status.
3) Continue from where the last session left off.

**IMPORTANT: Keep CLAUDE.md compact.** Detailed chunk logs go in PROGRESS.md only. The summary above should be ~10 lines max. This prevents "Prompt too long" errors.
