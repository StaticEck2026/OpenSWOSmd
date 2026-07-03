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

/* Resolve a string-symbol NAME (e.g. "_aSerieA") to the ADDRESS of the compiled
 * string it names, or NULL if unknown. Backed by the compiled _stringSymbols
 * table (always present), so unlike tr() it works even when no translation file
 * loaded. Use as the tr()==NULL fallback for data-driven names so they resolve
 * to the compiled string (which tr_ptr can still localize at draw time) rather
 * than the raw symbol key. */
const void *Translations_SymbolAddr(const char *name);

/* Localize a string the caller only has as a POINTER: if `strAddr` is the
 * address of a compiled UI string symbol, return its translated text; else NULL
 * (caller keeps the original pointer). Used by the renderer for string_table
 * option values — no length limit, the pointer is swapped for a pool string. */
const char *tr_ptr(const void *strAddr);

/* Repoint countriesTable[] entries to their localized packed name strings. Call
 * once after Translations_Load (the country names are packed flag/name/nat, so
 * they need this rebuild rather than a plain tr_ptr at each of the ~8 sites). */
void Countries_Localize(void);

/* Localize a compiled message-string POINTER for display: returns the active
 * language's text for `s` (the address of a compiled UI string), or `s` itself if
 * untranslated/unknown. Wrap the format/message argument of code that shows a
 * string BY POINTER (e.g. PrintFormatted into a buffer) — the menu loader only
 * localizes static menu-entry text, and the renderer only tr_ptr's entry pointers,
 * so text formatted into a runtime buffer needs this at the point of use. No-op on
 * the Amiga (tr_ptr returns NULL there) -> keeps the English original. */
static inline const unsigned char *tr_fmt(const void *s)
{
    const char *t = tr_ptr(s);
    return t ? (const unsigned char *)t : (const unsigned char *)s;
}

#endif /* TRANSLATIONS_H */
