/* translations.h — UI string localization (data_json/translations/<lang>.json).
 * The data files (menus, competitions) carry symbol NAMES; tr() maps a name to
 * the translated text for the active language, or NULL to fall back to the
 * compiled string symbol. See scripts/export_translations.py. */
#ifndef TRANSLATIONS_H
#define TRANSLATIONS_H

/* Load data_json/translations/<lang>.json into the lookup table. Safe to call
 * with NULL/"" (defaults to "en"); a missing file leaves every tr() returning
 * NULL (so callers fall back to compiled strings). */
void Translations_Load(const char *lang);

/* Resolve a string-symbol name (e.g. "aOptions") to its translated text for the
 * active language. Returns NULL if unknown — caller should fall back. The
 * returned pointer is owned by the translation table and lives until process
 * exit, so it is safe to store in long-lived menu/competition data. */
const char *tr(const char *symbol);

/* Localize a string the caller only has as a POINTER: if `strAddr` is the
 * address of a compiled UI string symbol, return its translated text; else NULL
 * (caller keeps the original pointer). Used by the renderer for string_table
 * option values — no length limit, the pointer is swapped for a pool string. */
const char *tr_ptr(const void *strAddr);

/* Repoint countriesTable[] entries to their localized packed name strings. Call
 * once after Translations_Load (the country names are packed flag/name/nat, so
 * they need this rebuild rather than a plain tr_ptr at each of the ~8 sites). */
void Countries_Localize(void);

#endif /* TRANSLATIONS_H */
