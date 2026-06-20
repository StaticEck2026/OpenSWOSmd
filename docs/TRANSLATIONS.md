# Translations (multi-language UI text)

Goal: translate the game's **UI / menu / system text** into multiple languages,
driven by **JSON** locale files (consistent with the project's existing JSON use —
`save_format_json.c`, the `SWOS_DATA_JSON` data-drive, mjson). Target locales:

| code | language |
|---|---|
| `en` | English (the source/fallback) |
| `de` | German |
| `it` | Italian |
| `fr` | French¹ |

¹ The request listed the fourth locale as **"FE"** — read here as **French (FR)**.
If you meant Spanish (ES) or another, it's a one-line locale-code change; the
system is locale-agnostic.

## Current state

All UI text is hard-coded **English C-string globals** in `src/strings_data.c`
(~748 strings), referenced by symbol (e.g. `aSensibleWorldO`, `aCjcHelp`,
`aLeague_1`), and grouped into **string tables** of `char *` (e.g.
`playerPositionsStringTable[]`, `tacticsStringTable[]`, `tourStringTable[6]`,
`divisionStringTable[]`, `cupRoundStringTable[]`). There is **no** locale concept
today.

## What is and isn't translatable

**Translate (UI/system):** menu labels & buttons, screen titles, prompts &
confirmations, error/system messages, player **positions** (GK/DF/MF/FW), tactic
names, competition/round labels ("LEAGUE", "CUP", "QUARTER FINAL"), match HUD
words, chairman/board memo text, options screens.

**Do NOT translate (data, not UI):**
- Asset filenames (`grafs/charset.raw`, …) — these are paths.
- Player & team names, real competition names loaded from data files.
- Credits/legal strings that must stay verbatim.

Tag each `strings_data.c` entry as `ui` (translatable) or `data`/`asset`
(frozen). Only `ui` strings get keys + JSON entries.

## Design

### 1. Stable keys
Give every translatable string a **stable key**. Simplest: reuse the existing C
symbol name (`aSensibleWorldO`, `aCjcHelp`, …) so there's a 1:1 mapping and no
churn. (A semantic rename pass — `menu.title.swos` etc. — can come later; keep the
symbol keys for v1.)

### 2. JSON locale files — `bin/lang/<code>.json`
```json
{
  "locale": "de",
  "name": "Deutsch",
  "fallback": "en",
  "strings": {
    "aSensibleWorldO": "SENSIBLE WORLD OF SOCCER",
    "aLeague_1": "LIGA",
    "aCjcHelp": "HILFE"
  },
  "tables": {
    "playerPositionsStringTable": ["TW", "AB", "MF", "ST"],
    "cupRoundStringTable": ["…"]
  }
}
```
- `bin/lang/en.json` is generated from `strings_data.c` (the source of truth /
  fallback). `de/it/fr.json` are filled by translators.
- `strings` = scalar UI strings (keyed by symbol). `tables` = the string-pointer
  arrays (keyed by table symbol, value = ordered array).
- Missing keys fall back to `fallback` locale, ultimately to the compiled English
  literal — so a partial translation is always safe.

### 3. Runtime model — repoint the globals (least-invasive)
Mirror the `SWOS_DATA_JSON` "overwrite the compiled global" pattern (the data
loader already does this for competitions). Don't touch the hundreds of call
sites:

- Convert each translatable global from `const char foo[] = "…"` to
  `const char *foo = "…";` (pointer with the English default baked in). Call sites
  that use `foo` are unchanged. (Arrays-of-pointers tables are already `char *[]`.)
- Build a **registry** — `{ const char *key; const char **slot; }` — listing each
  translatable global and its address, plus the table arrays
  (`{key, char **arr, int count}`).
- `Translations_Load(locale)`:
  1. Load `bin/lang/<locale>.json` (mjson), and its `fallback` chain down to `en`.
  2. For each registry entry, look up the key; if found, `*slot = interned_copy`.
     The interned copies live in a per-load string arena (freed/reset on locale
     switch).
  3. Unset keys keep their compiled English default (or the fallback locale's).
- Locale switch (in-game options) = reset arena + `Translations_Load(newLocale)`.

### 4. Locale selection
- New INI section: `[translation] language=en` (or fold into `[video]`/a new
  `[locale]`). Plus `--lang=de` CLI (highest precedence), mirroring the `[dos]`
  plumbing in `main.c`/`swos_settings.c`.
- Default `en`. Persist the chosen locale on clean shutdown like other settings.

## Hard constraints (must solve before it looks right)

### Font / glyph coverage
The SWOS font (`grafs/charset.raw`, small/big chars tables) covers basic ASCII.
DE/IT/FR need accented glyphs: `ä ö ü ß  à è é ì ò ù  ç â ê î ô û ë ï`. Options,
in order of preference:
1. **Extend the font** — add the accented glyphs to the charset + the
   `conversionTable` (`smallCharsTable`/`bigCharsTable`, see the
   `chars-table-truncation` history) so they map and render.
2. **Transliterate** in the JSON (`ä→ae, ß→ss, é→e`) — ugly but zero font work;
   acceptable as a stop-gap.
Pick (1) for a real release. Either way, **validate** that every character used in
every locale JSON exists in the font's conversion table (a build-time lint).

### Text width / layout
Translations are longer (German notably). The menu system already measures with
`_GetTextWidth(text, charsTable)`; lean on that:
- Centre/right-align by measured width, don't assume English widths.
- Flag (lint) any translated string whose measured width exceeds its menu slot;
  the translator shortens it or we add abbreviations.
- StringTable entries (positions, rounds) are drawn in fixed-width columns —
  keep those translations short (2-3 chars for positions).

### Byte order / encoding
JSON is UTF-8. The font is index-based (one byte per glyph via the conversion
table), **not** UTF-8. The loader must map each source character (decoded from
UTF-8) to a font glyph index when interning — so the runtime strings are in the
game's own glyph encoding, not raw UTF-8. (This is where the accented-glyph
mapping from the font step is applied.)

## Tooling

- **`scripts/extract_strings.py`** — scan `strings_data.c` for `ui`-tagged
  globals + the table arrays → emit `bin/lang/en.json` (keys + English values).
  Re-runnable; new strings appear as new keys for translators.
- **`scripts/check_translations.py`** — for each `de/it/fr.json`: report missing
  keys (vs en.json), extra/stale keys, characters not in the font conversion
  table, and strings whose `_GetTextWidth` exceeds a per-key max (where known). CI
  gate.
- Translators edit the JSON directly (or via a sheet exported to JSON).

## Phasing

1. **Tag** `strings_data.c` entries `ui` vs `data`; convert `ui` arrays to `const
   char *` pointers (English defaults intact, call sites unchanged).
2. **Registry** + `Translations_Load` + mjson read + arena + UTF-8→glyph mapping.
3. **`[translation] language=` / `--lang`** wiring + persistence.
4. **`extract_strings.py`** → `en.json`; **font** accented-glyph extension +
   `check_translations.py` lint.
5. Produce `de.json` / `it.json` / `fr.json`; wire an in-game language option.

## Notes
- Keep `en.json` generated, never hand-edited — it's the key list of record.
- A partial/empty locale must always be safe (fallback to en → compiled literal).
- Don't translate `data`/asset strings; don't put filenames or player/team names
  in the locale files.
- The original DOS/Amiga SWOS shipped multi-language; if those original
  translations can be recovered they're a good seed for `de/it/fr.json`.
