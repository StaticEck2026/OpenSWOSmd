# MENUS.md — Consolidate the menu structs, move menus to JSON, cross-port the format

**Status: PLAN + early implementation.** The bootstrap exporter
(`scripts/export_menus.py` → `bin/data_json/menus/*.json`, all 47 menus) and the
name→address symbol registry (`scripts/gen_menu_symbols.py` →
`src/menu_symbols.{c,h}`, 663 symbols, compiles clean) are **DONE** (migration
steps 1–2, §8). The C loader, Python `json→bin`, and DOS import remain. This
document answers the six menu tasks:

1. **Consolidate** the `menu_data.c` struct families into the fewest possible
   structs (the same exercise as `docs/STRUCT_CONSOLIDATE.md` did for
   competitions).
2. **JSON-ify** the menu data and write a **C loader**.
3. A **Python `json → binary`** emitter producing the packed menu format the
   **Amiga** build keeps consuming.
4. *(no task 4)*
5. Reference: how `sws_menu.amiga.py` turns the Amiga asm into structured data,
   and what it tells us about the format.
6. The **DOS menu-data differences** (from `sws_menu_x86.idc`).
7. **Importing the DOS menus** (`swos-2020/src/menus/*.mnu`) into our JSON.

Our JSON menu files live in **`bin/data_json/menus/`** (one file per menu, e.g.
`bin/data_json/menus/swosMainMenu.json`), mirroring the existing
`bin/data_json/{teams,competitions}/` corpus (see `docs/DATA_JSON.md`).
Imported **DOS** menus (Task 7) land in the **`bin/data_json/menus/dos/`**
subfolder, kept separate from our own (Amiga-derived) menus so the two sets
never collide and the import can be re-run without clobbering hand-tuned files.

**Sources read for this plan:**
- `src/menu_data.c` (9886 lines, 47 menu instances) + `src/menu_data.h` (3477
  lines, the generated structs).
- `src/menu_system.c` `_UnpackMenu` / `_ActivateMenu` — the runtime parser (the
  format's ground truth) — and `src/menu_system.h` (`Menu`, `MenuEntry`).
- `C:\Projects\Amiga\SWOS-RE\IDAPro\Scripts\sws_menu.amiga.py` (Amiga menu
  beautifier) and `…\sws_menu_x86.idc` (DOS beautifier).
- `C:\Projects\GitHub\swos-2020\src\menus\*.mnu` (104 DOS-side menu files in a
  DSL) + the `…\swos-2020\mnu2h\` Python compiler (`Parser.py`,
  `CodeGenerator.py`, `Constants.py`, …) and `src\menus\menuCodes.h`.

---

## 0. The binary menu format — the one contract everything shares

Every representation in this document (the C structs, the JSON, the Amiga
binary, the DOS data, the swos-2020 DSL) is a different *spelling* of **one**
packed byte-stream. That stream is defined by the runtime parser
`_UnpackMenu(packed, dest)` in `src/menu_system.c:920`, which walks `packed`
and builds the runtime `Menu` header + a fixed-stride `MenuEntry[]` array.
**Get this section right and the rest is mechanical.**

### 0a. Header (one per menu)

| bytes | field | notes |
|---|---|---|
| ptr | `onInit`   | function pointer or 0 |
| ptr | `onReturn` | function pointer or 0 (becomes `Menu.afterDraw`) |
| ptr | `onDraw`   | function pointer or 0 |
| u32 | `defaultEntry` | initially-selected entry **ordinal** (NOT a pointer; resolved later) |

`ptr` width is target-specific: **4 bytes** on Amiga (BE) and DOS (LE), **8
bytes** on the SDL 64-bit build. `_UnpackMenu` reads the header through a typed
`PackedHdr` and pointers through `unpackReadPtr()` (`memcpy` of
`sizeof(uintptr_t)`), so the *static C data* uses native pointer width on every
target. Only `defaultEntry` is fixed at u32.

### 0b. Entry / marker stream (terminated by `end-menu`)

After the header comes a stream of **markers** and **entries**. A marker is a
leading control word; an entry is coords + a command list.

| marker word | name | payload | effect |
|---|---|---|---|
| `0xFC19` (-999) | **end menu** | – | stop |
| `0xFC1A` (-998) | **start menu / set origin** | `i16 menuX, i16 menuY` | set `menuX/menuY` added to subsequent entry coords (begins a positioning sub-block) |
| `0xFC1B` (-997) | **start template** | *(an entry)* | parse the next entry into the shared template instead of emitting it; restored on its `end` |
| `0xFC1C` (-996) | **reset template** | – | re-init template to defaults |
| `0xFFFE` | **wide coords** | – | subsequent entry coords are `i16 x,y,w,h` (default) |
| `0xFFFF` | **byte coords** | – | subsequent entry coords are `i8 x,y,w,h` (compact); **mode persists** until flipped |

Any word that isn't a marker begins an **entry**:

1. **Coords** — `x,y,w,h`, each `i16` (wide) or `i8` (byte) per the current
   coords mode. `_UnpackMenu` stores `x += 24 + menuX`, `y += 36 + menuY`
   (`24/36` are the Amiga menu-panel offsets — a *runtime* detail, not part of
   the on-disk numbers).
2. **Command list** — a sequence of `u16 opcode` records, terminated by opcode
   `0`. Opcodes (verbatim from `_UnpackMenu` cases 0–22):

| op | name | extra bytes | sets on `MenuEntry` |
|---|---|---|---|
| 0 | **end entry** | – | finalize entry, `ordinal++` |
| 1 | invisible | – | `isInvisible=1` |
| 2 | nav | `i8 left,right,up,down` | nav links |
| 3 | bg func | `ptr` | `background=FUNC1`, `bg=ptr` |
| 4 | colors | `u16` | `background=COLOR`, `bgColor=word` |
| 5 | bg sprite | `u16` | `background=SPRITE1`, `bgColor=idx` |
| 6 | invalid | `u16` | `background=4`, `bgColor=word` |
| 7 | fg func | `ptr` | `content=FUNC2`, `fg=ptr` |
| 8 | string | `u16 flags, ptr` | `content=STRING`, `stringFlags`, `fg=ptr` (ptr `1..255` ⇒ deferred `0xFFFFFFFF`) |
| 9 | fg sprite | `u16` | `content=SPRITE2`, `fgSpriteIdx=idx` |
| 10 | string table | `u16 flags, ptr` | `content=STRING_TABLE`, `fg=ptr` |
| 11 | multi-line text | `u16 flags, ptr` | `content=MULTI_LINE_TEXT`, `fg=ptr` |
| 12 | number | `u16 flags, u16 value` | `content=NUMBER`, `fgNumber=value` |
| 13 | onSelect (fire) | `ptr` | `controlMask=0x20`, `onSelect=ptr` |
| 14 | input func | `u16 mask, ptr` | `controlMask=mask`, `onSelect=ptr` |
| 15 | before draw | `ptr` | `beforeDraw=ptr` |
| 16 | after draw | `ptr` | `afterDrawCb=ptr` |
| 17–20 | left/right/up/down skip | `u8 skip, u8 dir` | skip + direction bytes |
| 21 | sprite copy | `ptr` | `content=SPRITE_COPY`, `fg=ptr` |
| 22 | **string-no-ptr** | `u16 flags` | `content=STRING`, `fg=0xFFFFFFFF` (deferred) — **Amiga only**, see §6 |

The same opcode names appear three times in the codebase, all consistent:
`menu_data.h` (`MENU_CMD_*` enum + `MenuCmd_*` structs), the IDA scripts (`type
N` comments), and `_UnpackMenu` (the `switch`). The IDA scripts (§5/§6) are the
canonical English description of each opcode's bytes.

### 0c. The pointer payloads are the hard part

Opcodes 3, 7, 8, 10, 11, 13, 14, 15, 16, 21 and the three header slots all carry
a **raw pointer** to a function, a string, a string-table, or a sprite-copy
struct. In `menu_data.c` these are C symbols the linker resolves
(`_CareerMenuInit`, `aOptions`, `&gameLengthStrTable`, `&editTeamsPrimaryShirts`,
`managerNameBuffer`, …). **This is the menu equivalent of the save format's
"pointer-to-disk" problem** (CLAUDE.md): a serialized menu must store a
*symbolic identity* (the name), never an address, and re-resolve it on load.
Exact distinct-symbol counts (from the generated registry, §2c — **663 total**):

- **385** callback functions (the `extern void _Xxx(void);` block at the top of
  `menu_data.c` plus its ~18 locally-defined stub callbacks).
- **185** string symbols (`aOptions`, `aEditTactics`, …).
- **73** string tables (`gameLengthStrTable`, `diyCupLegsStringTable0`, …).
- **6** sprite-copy structs (`editTeamsPrimaryShirts`, …).
- **14** writable / runtime buffers (`managerNameBuffer`, `saveFileTitle`,
  `teamTacticsUSERA..F`, the cast-marked `(char*)`/dotted refs like
  `diyCompetitionStart.base.competitionName`).

§2/§3 turn these into a single **symbol registry** shared by both ports.

---

## 1. Task 1 — Consolidating the menu structs

### 1a. What's there now (the IDA "one struct per instance" pattern again)

`menu_data.h` defines, exactly as IDA emitted them:

- **16 `MenuCmd_*` structs** — one per opcode payload (`MenuCmd_Colors`,
  `MenuCmd_TextPtr`, `MenuCmd_Nav`, `MenuCmd_Number`, `MenuCmd_StringTable`,
  `MenuCmd_SpriteFg`, `MenuCmd_InputFunc`, `MenuCmd_BeforeDraw`,
  `MenuCmd_AfterDraw`, `MenuCmd_SpriteCopy`, `MenuCmd_FgFunc`, `MenuCmd_BgFunc`,
  `MenuCmd_OnSelect`, `MenuCmd_Skip2`, `MenuCmd_Invisible`, `MenuCmd_TextNoPtr`)
  plus `MenuCoords8/16`, `MenuHeader`, `MenuMark_StartMenu`. These are the
  genuine, minimal building blocks — **keep them** (they are the canonical
  encoding of each opcode).
- **25 `MenuEntry_*` "shape" structs** — one per *combination of commands an
  entry happens to use*: `MenuEntry_Byte_ColorsOnly`,
  `MenuEntry_Byte_Nav_Colors_TextPtr_OnSelect`, `MenuEntry_Wide_NumberOnly`,
  `MenuEntry_Byte_Invisible_NavOnly`, … (15 `Byte`-coords + 10 `Wide`-coords).
  Each is just `MenuCoords{8,16}` + the subset of `MenuCmd_*` it uses + `u16
  end`.
- **51 `Menu_*` structs** — 47 per-menu (`Menu_SWOSMainMenu`, `Menu_Options`,
  `Menu_Career`, `Menu_EditTeams`, …) + 4 reusable sub-blocks
  (`Menu_DesignDIYCup_OptionRow`, `Menu_DIYCup_StatsBlock`,
  `Menu_DesignDiyTournament_StageBlockA/B`). Each is `MenuHeader` + a hand-laid
  concatenation of markers and `MenuEntry_*` / inline-`MenuCmd_*` fields.

These exist for the *exact same reason* the competition variants did
(`STRUCT_CONSOLIDATE.md` §intro): IDA can't resize a struct in place, so every
menu and every entry-shape got its own type so the bytes after it stay put.

### 1b. The decisive finding: the `MenuEntry_*` and `Menu_*` structs are pure layout scaffolding

For the competitions, the merged structs had to *survive* because game code reads
their fields by name (`comp->teamSeedList[i]`, `divisionsCount`, …). **Menus are
different and much easier:**

> **Nothing in the codebase does typed field access into a `Menu_*` or
> `MenuEntry_*` struct.** Every menu instance is consumed as raw bytes: the
> definition is `Menu_Foo fooMenu = {…}`, but *every reader* declares it `extern
> uint8_t fooMenu[]` and passes it to `_ActivateMenu(uint8_t*)` /
> `_ShowMenu(uint8_t*)`, which hand it to `_UnpackMenu` (the opcode walker in
> §0). A grep for `Menu_[A-Z]…*`/`(Menu_[A-Z]` casts or `.entryN.` field reads
> outside `menu_data.h` returns **zero** hits.

So the 25 `MenuEntry_*` + 47 `Menu_*` + 4 sub-block structs (**76 types**) are
*write-only at compile time* — they exist only to lay the bytes down in the
right order, then are immediately type-erased to `uint8_t*`. They are the
direct analogue of the competition `*_TEXT`/`*_ROUNDS` EQU symbols that
`STRUCT_CONSOLIDATE.md` §2a found were "pure layout documentation… zero hits
outside the EQU definitions themselves." **They can be deleted outright.**

### 1c. The iteration question (sentinel vs count vs stride)

Same question `STRUCT_CONSOLIDATE.md` §2 asked. Answer for menus:

- **Within an entry:** the command list is an **opcode-tagged stream terminated
  by opcode `0`** (`_UnpackMenu` `case 0`). No fixed stride — each opcode knows
  its own payload width. Self-describing.
- **Across entries:** entries are walked **until the `0xFC19` end-menu marker**;
  `_UnpackMenu` never steps "entry N+1 = entry N + sizeof(shape)". The shape
  structs' sizes are *never read* by anything.
- **Coords width** is a **mode flag** (`0xFFFF`/`0xFFFE`), not encoded per
  entry.

⇒ Exactly like the competitions: **no fixed-size record stepping anywhere.**
Consolidation is safe, and here it's *total* — we can discard the per-shape and
per-menu structs entirely rather than merge-to-max.

### 1d. Proposed consolidation — from 76 scaffolding structs to **3 model structs**

Replace the 76 layout structs with a small, generic, *authoring* model. Keep the
16 `MenuCmd_*` building blocks as the canonical opcode encodings; add three
model structs that describe a menu the way the JSON and the emitters think about
it:

```c
/* One tagged command (replaces all 25 MenuEntry_* shape combos). */
typedef struct {
    uint16_t opcode;            /* MENU_CMD_* (0..22) */
    /* exactly one of the following is meaningful, per opcode (see §0b table) */
    uint16_t flags;             /* string/number flags or control mask */
    int32_t  ints[4];           /* nav bytes / number / sprite idx / color / skip+dir */
    int32_t  symbol;            /* index into the symbol registry (§2c), or -1 */
} MenuCommandModel;

/* One entry (coords + its command list). */
typedef struct {
    int16_t  x, y, w, h;
    uint8_t  byteCoords;        /* emit i8 vs i16 coords */
    uint16_t numCommands;
    MenuCommandModel commands[MENU_MAX_CMDS_PER_ENTRY];
} MenuEntryModel;

/* One menu (header + ordered entries/markers). */
typedef struct {
    int32_t  onInit, onReturn, onDraw;   /* symbol registry indices, or -1 */
    uint16_t defaultEntry;
    uint16_t numItems;
    MenuItemModel items[MENU_MAX_ITEMS]; /* entry | set-origin | start/reset-template */
} MenuModel;
```

This is the **source of truth at authoring time** (and what the JSON
deserializes into). It is *not* what `_UnpackMenu` reads — that stays the packed
byte-stream of §0. The C loader (§2d) and Python emitter (§3) both turn a
`MenuModel` into that byte-stream. So:

- **`menu_data.h`:** drop the 25 `MenuEntry_*` and 51 `Menu_*` typedefs; keep
  the `MENU_CMD_*`/`MENU_MARK_*` enums, the 16 `MenuCmd_*` structs (still handy
  for the emitters), and add the 3 model structs above.
- **`menu_data.c`:** the 47 hand-written `Menu_Foo foo = {…}` initializers go
  away entirely — the menus now come from `bin/data_json/menus/*.json`. The big
  `extern void _Xxx(void);` block migrates into the **symbol registry** (§2c).

### 1e. What must change together (or it breaks)

- The `extern uint8_t fooMenu[]` decls at the ~30 call sites
  (`_ShowMenu(contestMenu)` etc., in `menus.c`, `career_init.c`,
  `edit_tactics.c`, `diy_competition.c`, `chairman_memo.c`, `game_init.c`,
  `file_io.c`) must resolve to the **loaded** packed buffer for that menu, not a
  static symbol. Provide a `uint8_t *menu_by_name("contestMenu")` (or keep the
  `contestMenu` symbol as a pointer the loader fills at init). Lowest-churn
  option: at startup, the loader builds every menu's packed buffer and assigns
  the legacy `contestMenu`/`swosMainMenu`/… symbols (now `uint8_t *`) to point at
  them — call sites stay byte-for-byte the same.
- `menu_system.c:160-177` already maps the legacy symbols to names for
  diagnostics; that table becomes the seed for `menu_by_name`.
- Sub-block reuse (`Menu_DIYCup_StatsBlock` ×6, `Menu_DesignDIYCup_OptionRow`
  ×6, tournament stage blocks) is just repetition in the byte-stream — the JSON
  expresses it with a `repeat`/array, the emitter unrolls it. No struct needed.

---

## 2. Task 2 — JSON format + C loader

### 2a. Design rules (inherited from the save/data JSON work)

Per CLAUDE.md "Save/load is typed JSON of NAMED fields" and `docs/DATA_JSON.md`:
**named fields, no addresses, no raw dumps, pointers as semantic identities.**
For menus that means: opcodes by name, coords as numbers, and every pointer as a
**symbol name string**. The format must survive 32↔64-bit and LE↔BE unchanged
(the Amiga emitter and the SDL loader read the *same* JSON).

### 2b. The JSON schema (one file per menu in `bin/data_json/menus/`)

```jsonc
// bin/data_json/menus/swosMainMenu.json
{
  "name": "swosMainMenu",
  "onInit":   "_SWOSMainMenuInit",
  "onReturn": "_GlobalRetn",
  "onDraw":   null,
  "defaultEntry": 6,
  "items": [
    { "kind": "entry",
      "coords": { "x": 88, "y": -24, "w": 128, "h": 76, "wide": true },
      "commands": [
        { "op": "sprite_fg", "index": 224 }
      ]
    },
    { "kind": "set_origin", "x": 0, "y": 1 },          // 0xFC1A marker
    { "kind": "coords_mode", "byte": true },           // 0xFFFF marker
    { "kind": "entry",
      "coords": { "x": 0, "y": 64, "w": -106, "h": 16 },
      "commands": [
        { "op": "nav",     "left": -1, "right": 6, "up": -1, "down": 2 },
        { "op": "colors",  "value": "0x3" },
        { "op": "text",    "flags": "0x10", "str": "aEditTactics" },
        { "op": "onselect_fire", "func": "_EditTacticsMenu" }
      ]
    },
    // … entries 2..10 …
    { "kind": "set_origin", "x": 0, "y": 0 },
    { "kind": "end" }                                   // 0xFC19
  ]
}
```

Conventions:

- **`op`** is the lowercase opcode name (`nav`, `colors`, `text`, `text_no_ptr`,
  `string_table`, `multiline`, `number`, `sprite_fg`, `sprite_bg`,
  `sprite_copy`, `bg_func`, `fg_func`, `onselect_fire`, `input_func`,
  `before_draw`, `after_draw`, `invisible`, `left_skip`/`right_skip`/…). The end
  marker (`0`) is implicit — it's the close of the `commands` array.
- **Pointers** (`func`, `str`, `table`, `sprite_copy`) are **symbol-name
  strings**, resolved via the registry (§2c). A deferred/dynamic string is
  `"text_no_ptr"` (Amiga) — see §6 — the loader maps it to the `0xFFFFFFFF`
  sentinel regardless of target.
- **Markers** are `items` of `kind` `set_origin` / `coords_mode` /
  `start_template` / `reset_template` / `end`, interleaved with `kind: "entry"`.
- **Templates** become `"kind": "start_template"` followed by the template
  entry; `reset_template` clears it. (Authoring nicety: the importer in §7 can
  fold a template + N near-identical entries into a `"repeat"` block; the loader
  expands it.)

### 2c. The symbol registry — the heart of both loader and emitter

A single generated table maps every menu-referenced name to its
address/value, the menu analogue of the save format's identity re-derivation:

```c
/* src/menu_symbols.c (generated; ~620 rows) */
typedef struct { const char *name; void *ptr; uint8_t kind; } MenuSymbol;
const MenuSymbol menuSymbols[] = {
    { "_SWOSMainMenuInit",        _SWOSMainMenuInit,        SYM_FUNC   },
    { "_EditTacticsMenu",         _EditTacticsMenu,         SYM_FUNC   },
    { "aEditTactics",             aEditTactics,             SYM_STRING },
    { "gameLengthStrTable",      &gameLengthStrTable,       SYM_TABLE  },
    { "editTeamsPrimaryShirts",  &editTeamsPrimaryShirts,   SYM_SPRITE_COPY },
    { "managerNameBuffer",        managerNameBuffer,        SYM_BUFFER },
    /* … */
};
```

- It is **generated**, not hand-maintained: a script scans the current
  `menu_data.c` (the `extern` block + every symbol that appears as a `func`/
  `str`/`table`/`sprite_copy`/buffer in an initializer) and emits both this `.c`
  and the `extern` decls. (This is also exactly the set the bootstrap exporter in
  §7a needs.)
- `menu_resolve(name)` is a `bsearch` (table sorted by name) returning the
  pointer; the loader caches a per-menu `int32 symbol-index → ptr` map so each
  JSON name is hashed once.
- **Both ports keep this table.** It is the only place that knows
  name→address; everything else is layout-free. This satisfies the "no
  addresses in the serialized form" rule — the addresses live in compiled code,
  the JSON/binary carry names/indices.

### 2d. The C loader (SDL build)

`src/menu_load_json.c`, modeled on `save_format_json.c` (uses the in-tree
`src/mjson.c`):

1. At startup, for each `bin/data_json/menus/*.json`: `mjson_find` the top
   object, read header symbols via the registry, then `mjson_next`-walk `items`.
2. Build the **packed byte-stream of §0** directly into a per-menu buffer
   (header with native-width pointers resolved through the registry, then the
   marker/entry/opcode bytes). Endianness is host-native here (the SDL build
   reads its own freshly-built buffer), but all multi-byte writes go through
   `src/endian_helpers.h` so the code is back-port-clean.
3. Assign the legacy `uint8_t *fooMenu` symbols to point at their buffers
   (§1e), so `_ActivateMenu(fooMenu)` keeps working unchanged. `_UnpackMenu`
   then runs exactly as today.

Why build the packed stream rather than fill `Menu`/`MenuEntry[]` directly:
`_UnpackMenu` already implements every subtlety (template apply/restore, coords
mode persistence, `menuX/menuY`, deferred-string fixups, the `+24/+36` offsets,
ordinal numbering). Reusing it means the JSON path can't drift from the asm
path. The loader's only job is JSON → §0 bytes.

A `SWOS_MENU_JSON=1` build flag selects the JSON loader; without it the legacy
static `menu_data.c` still compiles, so the migration is incremental and
verifiable menu-by-menu (round-trip a menu, diff its packed bytes vs the static
struct's bytes — a zero-diff is the green light, same discipline as
`SWOS_SAVE_FORMAT_JSON_VERIFY`).

> **STATUS (2026-06-20): C loader + verify DONE.** `src/menu_load_json.c`
> (`MenuJson_BuildPacked` + `MenuJson_VerifyAll`) builds the §0 packed stream
> for every menu and `memcmp`s it against the compiled static bytes. Gated on
> **env** `SWOS_MENU_JSON` (matching the `SWOS_DATA_JSON` precedent, not a
> compile flag) and wired into `_FrontEndPreLoopSetup`. Result: **47/47 menus
> byte-identical, 0 diff.** Two corpus subtleties that the round-trip flushed
> out and the loader now handles: (1) the skip opcodes are `UP/DOWN/LEFT/RIGHT
> = 0x11/0x12/0x13/0x14` (the §0b "left/right/up/down" ordering above is
> misleading — trust the `MENU_CMD_*` enum); (2) a `text`/`string_table`/
> `multiline` pointer field may be a SYMBOL name, a literal INTEGER
> deferred-string sentinel (`"str": 4` → fg `0xFFFFFFFF`), or `null`. This is
> the green light to repoint the legacy `uint8_t fooMenu[]` call sites at the
> loaded buffers (§1e) and retire the static `menu_data.c` initializers.

---

## 3. Task 3 — Python `json → binary` (the Amiga path)

The Amiga build "continues to use the binary format" = the **packed §0
byte-stream** that `_UnpackMenu` reads. We generate it from JSON at build time
instead of hand-writing `menu_data.c`.

### 3a. `scripts/menu_json_to_bin.py`

Reads `bin/data_json/menus/*.json` and emits, per menu, the §0 byte-stream with
**Amiga conventions**:

- **Big-endian** multi-byte fields (use the same byte order the existing
  `.TAC`/team-file tooling uses; mirror `endian_helpers.h` semantics).
- **4-byte pointers.**
- **Markers / opcodes / coords** exactly per §0.

But a `.bin` can't bake absolute addresses (only the linker knows them). Two
supported outputs — pick per build need:

1. **Relocatable blob + symbol table (recommended, true "binary format").**
   Every pointer slot is written as a **u32 index into `menuSymbols`** (§2c)
   plus a per-menu **relocation list** (`[offset, symbolIndex]`). A tiny Amiga
   loader (`menu_reloc.c`, shared with SDL) walks the relocation list at startup
   and patches each slot with `menuSymbols[idx].ptr`. This keeps the on-disk
   form address-free and identical across targets — only the patch step is
   per-target. *This is the menu analogue of the JSON loader: names→addresses
   resolved at load, never stored.*
2. **Generated C array (fallback / lowest-risk).** Emit `menu_data_gen.c`
   containing `const uint8_t swosMainMenu_packed[] = { … };` with the pointer
   slots written as the **C symbols themselves** (`(uintptr_t)_SWOSMainMenuInit`,
   `(uintptr_t)aOptions`, …) via a small `#define`-free initializer, letting the
   **linker** do relocation the way it does today. No runtime patching; the
   Amiga keeps its current "menus are compiled-in" model, just generated from
   JSON rather than hand-written.

Both consume the same JSON and the same `menuSymbols` name set, so they can't
disagree on content. Start with (2) to de-risk (byte-identical to today's
compiled menus → provable), then move to (1) when the relocation loader is
trusted.

### 3b. Verification

Round-trip discipline like `docs/DATA_JSON.md`: `menu_data.c (struct bytes) →
JSON → bin → bytes` must be **0-diff** against the bytes the current
`Menu_Foo foo = {…}` produces (with pointer slots compared by *symbol*, not
address). A green round-trip on all 47 menus is the signal the static
`menu_data.c` can be retired (roadmap step 3/4 for menus).

---

## 4 / Task 5 — What `sws_menu.amiga.py` tells us (the Amiga reference)

`sws_menu.amiga.py` (IDAPython, Ben Baker 2025; port of Zlatko Karakaš's IDC) is
a **read-only beautifier**: pointed at a menu label it walks the bytes and
annotates IDA with comments — it never renames or restructures. Its `main()` is
a direct, executable spec of §0 and was the primary cross-check for this
document:

- Header: 3 dwords (on-init / on-return / on-draw, made offsets if non-zero) + 1
  dword default entry.
- Then the marker loop: `64537`→end, `64538`→start-menu(+x,+y),
  `64539`→template (with the wide/byte-coords sub-handling), `64540`→init-entry,
  `65534/65535`→wide/byte coords.
- Per entry: coords (byte or word per `byte_coords` state) then the opcode loop
  `t == 0..22`, with helper annotators `write_colors` (opcode 4),
  `write_str_flags` (opcodes 8/10/11/12/22), `write_ctrl_flags` (opcode 14).

Two things to lift directly into our tooling:

- **`write_str_flags` / `write_colors` / `write_ctrl_flags`** decode the flag
  words into human strings (text color/font/alignment; background+frame colors;
  control-mask bit names). Port these into the JSON exporter so the JSON can
  carry **readable** flags (e.g. `"flags": "big|left|white"`) with the raw hex as
  a fallback — a big editability win, and they're already written and tested.
- The script's `byte_coords` handling and **type 22** branch are what make it
  the *Amiga* reader (vs the DOS one) — see §6.

This script operates on the *original Amiga binary*; our `menu_data.c` is the C
port of that same data, so the script is the authority when our struct layout
and the asm disagree.

---

## 5 / Task 6 — DOS menu-data differences (`sws_menu_x86.idc`)

`sws_menu_x86.idc` is the older DOS/x86 beautifier (Zlatko Karakaš, 2002–2021)
and the Amiga `.py` is a port of it. Diffing the two pins down exactly how the
**DOS** menu data differs from the Amiga's. All differences are *additive on the
Amiga side* — the DOS format is a strict subset.

| aspect | DOS (`sws_menu_x86.idc`) | Amiga (`sws_menu.amiga.py`) |
|---|---|---|
| **Coords width** | **Always 16-bit (word).** No byte-coords path at all — there is no `65534/65535` handling; every entry reads `x,y,w,h` as 4 words. | Adds a **byte-coords mode** (`0xFFFF`=byte, `0xFFFE`=wide), persisting until flipped, to shrink entries. |
| **Template (`-997`)** | Reads `x,y,w,h` as **4 words inline** (advances 10 bytes), always wide. | Reads the wide/byte-coords marker first, then `x,y,w,h` as bytes or words. |
| **Opcode 22** | **Does not exist.** Deferred/dynamic text is **opcode 8 with pointer `0xFFFFFFFF`** (the IDC explicitly labels `Dword == 0xFFFFFFFF` as `"STDMENUTEXT - gets filled later"`). | Adds **opcode 22** (`text_no_ptr`, str-flags only, no pointer word) as the compact deferred-text encoding; `_UnpackMenu` case 22 sets `fg=0xFFFFFFFF` — same runtime effect, 4 bytes smaller. |
| **Pointer endianness** | Little-endian 32-bit. | Big-endian 32-bit. |
| **Everything else** | Header (3 fn-ptrs + default entry), markers `-999/-998/-997/-996`, opcodes 0–21, the flag-word meanings (`write_colors`/`write_str_flags`/`write_ctrl_flags` are **byte-identical** between the two scripts). | Same. |

**Consequences for our format:**

- The JSON is a **superset** that covers both: it carries `coords.wide`
  (DOS=always true) and uses `op: "text_no_ptr"` for deferred text. When the
  Python emitter targets DOS-compatible output it would (a) force wide coords and
  (b) lower `text_no_ptr` → `text` with a `0xFFFFFFFF` pointer + LE byte order.
  For our Amiga/SDL targets it keeps byte-coords and opcode 22 + BE.
- Because DOS is a subset, a DOS menu imported to JSON (§7) **always** round-trips
  to the Amiga binary; the reverse (Amiga→DOS) only round-trips if the menu used
  no byte-coords and no opcode 22 (the emitter can re-encode both, so it always
  can in practice, just larger).

---

## 6 / Task 7 — Reading the DOS menus → our JSON

The DOS menus live as a **text DSL** in `C:\Projects\GitHub\swos-2020\src\menus\`
(104 `.mnu` files) and are compiled to the binary menu format by the Python
toolchain in `…\swos-2020\mnu2h\` (`Parser.py`, `Tokenizer.py`,
`Preprocessor.py`, `CodeGenerator.py`, `Constants.py`, `Menu.py`, `Entry.py`).
`CodeGenerator.py` currently emits **C++ structs** (from `menuCodes.h`:
`MenuHeader` + a stream of `EntryElement` subclasses with the same opcode enum
`kEndOfEntry…kColorConvertedSprite` = 0..21). We want JSON instead.

### 6a. The DSL in one screen

```
Menu mainMenu {
    y: 1
    onInit: ~SWOSMainMenuInit          // ~name  = function pointer
    onReturn: ~SWOSMainMenuOnReturn
    initialEntry: competition           // entry name → ordinal
    kButtonWidth = 148                  // menu-scoped constant
    defaultX: kColumn1X                 // default* → template mechanism
    defaultY: @prevEndY + 6             // @name = built-in / relative layout
    defaultTextFlags: @kBigText
    Entry competition {
        y: kStartY
        color: @kDarkBlue
        downEntry: practice             // entry-ref nav (name | ordinal | < | >)
        rightEntry: options
        text: "COMPETE"
        onSelect: ~"SWOS::competitionMainMenu_onSelect"   // ~"ns::fn" raw
    }
    …
}
```

DSL → opcode mapping (from `Constants.py` + `menuCodes.h`):

| DSL property | binary | our JSON |
|---|---|---|
| `onInit/onReturn/onDraw` | header ptrs | header `func` names |
| `initialEntry` | header `defaultEntry` | `defaultEntry` (ordinal) |
| `x/y/width/height` | entry coords | `coords` |
| `color` | op 4 | `colors` |
| `text:"…"` / `text:~sym` | op 8 | `text` (inline literal → string pool, see 6c) |
| `number` | op 12 | `number` |
| `sprite` | op 9 | `sprite_fg` |
| `stringTable:[…]` | op 10 | `string_table` |
| `customDrawForeground` | op 7 | `fg_func` |
| `customDrawBackground` | op 3 | `bg_func` |
| `invisible:true` | op 1 | `invisible` |
| `up/down/left/rightEntry` | op 2 (positions) | `nav` |
| `skipUp/Down/Left/Right` + `directionX` | ops 17–20 | `*_skip` |
| `onSelect` (+`controlsMask`) | op 13 / 14 | `onselect_fire` / `input_func` |
| `beforeDraw` | op 15 | `before_draw` |
| `onReturn` (entry) | op 16 | `after_draw` |
| (`kColorConvertedSprite`) | op 21 | `sprite_copy` |

Symbol/operator syntax to resolve when importing: `~name` and `~"ns::name"` =
function pointers; `@name` = built-in constants (`@kDarkBlue`, `@kBigText`,
`@kCenterAligned`, …) and **relative-layout** tokens (`@prevEndY`, `@prevX`,
`@kScreenHeight`, …); bare `k`-names = menu-scoped constants; quoted text =
inline string literal; arithmetic expressions are evaluated by the
preprocessor. Templates: `default*` properties + `TemplateEntry`/`ResetTemplate`
map to the `-997`/`-996` markers and auto-layout (`@prevEndY`) resolves to
concrete coords at compile time.

### 6b. The importer — reuse the existing parser, retarget the backend

The cleanest importer doesn't re-implement the DSL: it **reuses
`swos-2020/mnu2h/` as a library** (it already tokenizes, preprocesses, evaluates
expressions, resolves `@`/`~`/`k` symbols, applies templates, and resolves nav
names→ordinals) and swaps `CodeGenerator.py` for a **JSON emitter** that walks
the same `Menu`/`Entry` objects (`Menu.py`/`Entry.py`) and writes our §2b schema:

```
scripts/import_dos_menus.py  (imports mnu2h as a package)
  for each *.mnu:
      ast   = mnu2h.Parser(...).parse()         # existing
      model = mnu2h.lower(ast)                   # existing: Menu + Entry objects,
                                                 # constants expanded, navs→ordinals,
                                                 # templates/@prevEndY resolved
      write_json(model, bin/data_json/menus/dos/<name>.json)   # NEW backend
```

This sidesteps re-writing 100 KB of mature parser/preprocessor code and
guarantees the import matches what swos-2020 actually compiles. Where DSL
constructs have no opcode equivalent (DOS-only `@k…` symbols, multiplayer-only
menus), the emitter records them and the run logs them — *don't* silently drop
(per CLAUDE.md's "no silent caps").

### 6c. Caveats specific to the swos-2020 corpus

- **It's a modern superset, not pure DOS.** Of the 104 files many are
  swos-2020-only (`leaderboard`, `matchMaking`, `scenario*`, `customChallenges*`,
  `logIn`, `ticker`, multiplayer lobbies) and several are backups
  (`*_bak`, `*-old`, `*_WIP`, date-stamped). Import is most useful for the menus
  that overlap our port (`main`, `options`, `audioOptions`, `controlOptions`,
  `competition`, `editing`, `extras`, `credits`, `message`, …). Filter to a
  curated allow-list; log the skipped ones.
- **Inline string literals** (`text: "COMPETE"`) have no symbol name. The
  importer interns them into a per-menu **string pool** and the JSON references
  the pool key (the loader/emitter materializes the bytes). This is the one place
  menus carry literal text rather than a symbol — keep it explicit in the schema
  (`{"op":"text","literal":"COMPETE"}` vs `{"op":"text","str":"aOptions"}`).
- **Function names won't match ours.** swos-2020 uses `SWOS::competitionMainMenu_
  onSelect`; our port uses `_CompetitionMenu` etc. Importing is a *starting
  point* for layout/coords/flags; the `func`/`str` symbols need a name-mapping
  pass (or are left as TODO markers the registry flags as unresolved). The DOS
  data's value to us is the **geometry and structure**, not the (different) C
  symbols.
- **menuMouse** (`menuMouse.cpp/.h`) is DOS-only mouse hit-testing layered over
  the same entry coords — no new menu-data fields, nothing to import.

---

## 7. File / tooling layout (where everything lives)

| path | role |
|---|---|
| `bin/data_json/menus/*.json` | **the menu corpus** (one file per menu) — authoring source of truth |
| `bin/data_json/menus/dos/*.json` | imported DOS menus (Task 7), kept separate from our own |
| `src/menu_data.h` | trimmed: keep `MENU_CMD_*`/`MENU_MARK_*` enums + 16 `MenuCmd_*` + the 3 model structs (§1d); drop the 76 scaffolding structs |
| `src/menu_data.c` | **retired** once JSON load is proven (its 47 initializers move to JSON) |
| `src/menu_symbols.c` (+`.h`) | **generated** name→address registry (§2c), kept on both ports |
| `src/menu_load_json.c` | SDL C loader: JSON → §0 packed buffer → legacy `fooMenu` ptr (§2d) |
| `src/menu_reloc.c` | shared relocation patcher for the Amiga blob path (§3a-1) |
| `src/menu_system.c` | **unchanged** — `_UnpackMenu` still reads §0 bytes |
| `scripts/export_menus.py` | **bootstrap**: parse current `menu_data.c` initializers → JSON + the symbol list (§7a) |
| `scripts/menu_json_to_bin.py` | JSON → Amiga binary / generated C (§3) |
| `scripts/import_dos_menus.py` | swos-2020 `.mnu` → JSON in `menus/dos/`, reusing `mnu2h/` (§6b) |

### 7a. Bootstrap (how we get the first JSON corpus)

Don't hand-write 47 JSON files. Write `scripts/export_menus.py` to parse the
existing `menu_data.c` initializers (they're regular, brace-nested
`{ MENU_CMD_*, … }` lists) and emit both the JSON corpus and the
`menuSymbols` name set. This is the menu analogue of
`scripts/export_competitions.py` and the `decode_data.py --dump-json` exporter.
Cross-check each exported menu by feeding its JSON back through the C loader and
diffing the packed bytes against the static struct (§2d/§3b).

---

## 8. Migration order (incremental, verifiable — mirrors `STRUCT_CONSOLIDATE.md` §7)

1. **✅ DONE — Symbol registry** (`scripts/gen_menu_symbols.py` →
   `src/menu_symbols.{c,h}`, 663 symbols: 385 func / 185 string / 73 table / 6
   sprite-copy / 14 buffer). Pure addition, compiles clean against the project
   flags; gives `menu_resolve(name)` and the name set everything else needs.
   (Re-run it whenever `_symbols.json` changes.)
2. **✅ DONE — Bootstrap export** (`scripts/export_menus.py`) →
   `bin/data_json/menus/*.json` for all 47 menus + `_symbols.json`. No code-path
   change yet.
3. **C loader behind `SWOS_MENU_JSON=1`** (§2d) — NEXT. Bring up menu-by-menu;
   green = packed bytes match the static struct (pointers compared by symbol).
4. **Flip the default to JSON**, delete the 47 `menu_data.c` initializers and the
   76 scaffolding structs (§1d). `menu_data.h` keeps only enums + `MenuCmd_*` +
   the 3 model structs.
5. **Python `json→bin`** (§3): start with the generated-C fallback (3a-2,
   byte-identical to today), then the relocatable blob + `menu_reloc.c` (3a-1).
   Back-port to the Amiga tree (don't auto-commit it — see memory
   `feedback_amiga_port_no_autocommit`).
6. **DOS import** (`import_dos_menus.py`, §6b) — last, opportunistic; lands
   geometry/structure for the overlapping menus, with a name-mapping pass for
   the callbacks.

Each step round-trips and, where it touches the packed bytes, builds all three
Linux arches (memory `project_target_strategy`) before back-porting.

---

## 9. Summary

- **Consolidate (Task 1):** the 25 `MenuEntry_*` + 51 `Menu_*` structs are
  **pure compile-time layout scaffolding** — nothing reads their fields; every
  menu is type-erased to `uint8_t*` and walked by `_UnpackMenu`. So they don't
  need *merging* (the competition fix), they can be **deleted** in favor of **3
  generic model structs** + the 16 existing `MenuCmd_*` opcode encodings. No
  fixed-stride record stepping exists anywhere → safe.
- **JSON + C loader (Task 2):** one file per menu in **`bin/data_json/menus/`**,
  opcodes by name, **all pointers as symbol-name strings** resolved through a
  generated **symbol registry** (`menu_symbols.c`) — the menu cure for the
  "pointer-to-disk" disease. The loader builds the §0 packed bytes and reuses
  `_UnpackMenu` unchanged.
- **Python `json→bin` (Task 3):** emits the §0 byte-stream with Amiga (BE,
  4-byte-ptr) conventions; pointers as relocations against the same registry
  (or a generated C file as the de-risking fallback). Verified by 0-diff
  round-trip.
- **Amiga reference (Task 5):** `sws_menu.amiga.py` is the executable spec of
  §0; lift its `write_str_flags`/`write_colors`/`write_ctrl_flags` decoders into
  the exporter for human-readable flags.
- **DOS differences (Task 6):** DOS is a strict subset — **no byte-coords, no
  opcode 22** (deferred text = opcode 8 + `0xFFFFFFFF`), LE pointers. Our JSON
  is the superset; the emitter lowers to either dialect.
- **DOS import (Task 7):** reuse the existing `swos-2020/mnu2h/` parser as a
  library and swap in a JSON backend; mind that the corpus is a modern superset
  with different C symbol names — import for geometry/structure, remap callbacks.
