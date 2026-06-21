/* ======================================================================
 * translations.c — UI string localization.
 *
 * Loads data_json/translations/<lang>.json — a { "strings": { symbol: text } }
 * map — into a sorted table, and resolves a symbol name to its translated text
 * via tr() (bsearch). This is the string analogue of the menu/competition JSON:
 * the data files store the symbol NAME (e.g. "aDivision1"), and the language
 * file holds the text, so changing en.json (or shipping de.json/fr.json)
 * re-skins every name without touching the data or the code.
 *
 * Portable C (mjson + malloc + stdio) — back-port-clean; only the call sites are
 * SDL-gated for now. A missing language file is non-fatal: tr() returns NULL and
 * every caller falls back to the compiled string symbol.
 * ====================================================================== */

#include "translations.h"
#include "mjson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const char *sym; const char *text; } TrEntry;

static TrEntry *s_tr      = NULL;   /* sorted by sym */
static int      s_trCount = 0;

static int tr_cmp(const void *a, const void *b)
{
    return strcmp(((const TrEntry *)a)->sym, ((const TrEntry *)b)->sym);
}

/* Copy an mjson token to a fresh NUL-terminated string, stripping the
 * surrounding quotes a string token carries. (en.json values have no escapes —
 * verified at export — so a raw copy is exact.) */
static char *tr_dup(const char *tok, int len)
{
    if (len >= 2 && tok[0] == '"' && tok[len - 1] == '"') { tok++; len -= 2; }
    if (len < 0) len = 0;
    char *s = (char *)malloc((size_t)len + 1);
    if (!s) return NULL;
    memcpy(s, tok, (size_t)len);
    s[len] = '\0';
    return s;
}

void Translations_Load(const char *lang)
{
    s_tr = NULL; s_trCount = 0;
    if (!lang || !lang[0]) lang = "en";

    char path[160];
    snprintf(path, sizeof path, "data_json/translations/%s.json", lang);
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[i18n] %s not found — using compiled strings\n", path);
        fflush(stderr);
        return;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return; }
    char *buf = (char *)malloc((size_t)n);
    if (!buf) { fclose(f); return; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    if (got != (size_t)n) { free(buf); return; }

    const char *strs; int sl;
    if (mjson_find(buf, (int)n, "$.strings", &strs, &sl) != MJSON_TOK_OBJECT) {
        free(buf);
        return;
    }
    /* count members, then fill */
    int cap = 0, o = 0, ko, kl, vo, vl, vt;
    while ((o = mjson_next(strs, sl, o, &ko, &kl, &vo, &vl, &vt)) != 0) cap++;
    if (cap <= 0) { free(buf); return; }
    s_tr = (TrEntry *)malloc((size_t)cap * sizeof *s_tr);
    if (!s_tr) { free(buf); return; }

    o = 0;
    int k = 0;
    while ((o = mjson_next(strs, sl, o, &ko, &kl, &vo, &vl, &vt)) != 0 && k < cap) {
        if (vt != MJSON_TOK_STRING) continue;
        char *sym  = tr_dup(strs + ko, kl);
        char *text = tr_dup(strs + vo, vl);
        if (sym && text) { s_tr[k].sym = sym; s_tr[k].text = text; k++; }
        else { free(sym); free(text); }
    }
    s_trCount = k;
    qsort(s_tr, (size_t)s_trCount, sizeof *s_tr, tr_cmp);
    free(buf);   /* tr_dup copied everything we keep */

    fprintf(stderr, "[i18n] loaded %d strings from %s\n", s_trCount, path);
    fflush(stderr);
}

const char *tr(const char *symbol)
{
    if (!s_tr || !symbol) return NULL;
    TrEntry key;
    key.sym = symbol;
    key.text = NULL;
    TrEntry *e = (TrEntry *)bsearch(&key, s_tr, (size_t)s_trCount, sizeof *s_tr, tr_cmp);
    return e ? e->text : NULL;
}
