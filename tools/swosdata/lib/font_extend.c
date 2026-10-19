/*
 * font_extend.c -- the extended glyphs drawn into a CHARSET bank. A port of
 * SwosGfx SwosFont.Extend (fillMissing on, no integration.txt); see
 * font_extend.h.
 *
 * glyphs.txt, as SwosFont reads it, line by line (UTF-8, BOM allowed):
 *   "@<c> <font>"   starts a block for character c (the first character after
 *                   '@'; <font> is what follows the LAST space: small or big)
 *   blank or "#..." ends the block in hand; outside a block it is a comment
 *   anything else   a row of the block ('*' or '#' = ink, '+' = shadow, else blank)
 * A later block for the same character and font replaces an earlier one.
 */
#include "font_extend.h"
#include "dos2amiga.h"
#include "sd_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WHITE          2      /* ink */
#define SHADE          8      /* drop shadow */
#define SPLIT_ROW      14     /* the big font starts at row 14 (y = 112) */
#define FONT_EXT_START 1220   /* first font-extension sprite slot */
#define BOX_FILL_ROWS  28     /* rows 0-27 get the missing-glyph box */

#define MAX_BLOCKS     512
#define MAX_KEEP_ROWS  8      /* Finalize uses 6 (small) or 8 (big) rows */
#define MAX_COLS       64

enum { FONT_SMALL, FONT_BIG, FONT_OTHER };

/* The stock Amiga charset's "missing glyph" marker (from CHARSET.RAW). */
static const uint8_t kSmallBox[6][6] = {
    { 2, 2, 2, 2, 2, 0 },
    { 2, 0, 2, 0, 2, 8 },
    { 2, 2, 0, 2, 2, 8 },
    { 2, 0, 2, 0, 2, 8 },
    { 2, 2, 2, 2, 2, 8 },
    { 0, 8, 8, 8, 8, 8 },
};
static const uint8_t kBigBox[8][8] = {
    { 2, 2, 2, 2, 2, 2, 2, 0 },
    { 2, 0, 2, 2, 2, 0, 2, 8 },
    { 2, 2, 0, 2, 0, 2, 2, 8 },
    { 2, 2, 2, 0, 2, 2, 2, 8 },
    { 2, 2, 0, 2, 0, 2, 2, 8 },
    { 2, 0, 2, 2, 2, 0, 2, 8 },
    { 2, 2, 2, 2, 2, 2, 2, 8 },
    { 0, 8, 8, 8, 8, 8, 8, 8 },
};

/* The glyph order (swos_font.py's): punctuation, inverted ! and ?, $, euro, OE,
 * then the accented capitals. Glyphs in the file but not here follow, in file order. */
static const uint32_t kOrder[] = {
    '!', '"', '#', '&', '<', '=', '>', '@', '[', '\\', ']', '^', '_', '`', '{', '|', '}', '~',
    0x00A1, 0x00BF, '$', 0x20AC, 0x0152,
    0x00C0, 0x00C1, 0x00C2, 0x00C7, 0x00C9, 0x00C8, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
    0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D9, 0x00DA, 0x00DB, 0x0178,
};
#define ORDER_COUNT ((int32_t)(sizeof kOrder / sizeof kOrder[0]))

typedef struct {
    uint32_t cp;
    int32_t  font;
    int32_t  nrows;                        /* rows read (only the first 8 kept) */
    int32_t  len[MAX_KEEP_ROWS];           /* characters in each kept row */
    uint8_t  cell[MAX_KEEP_ROWS][MAX_COLS];
} Block;

typedef struct {
    Block    *blocks;
    int32_t   nblocks;
    uint32_t  fileOrder[MAX_BLOCKS];
    int32_t   norder;
} Glyphs;

/* A finished glyph bitmap: h rows of w. */
typedef struct {
    int32_t w, h;
    uint8_t px[8][MAX_COLS];
} Art;

/* ---- glyphs.txt ----------------------------------------------------------- */

/* Decode one UTF-8 character at *p (advancing it); a bad byte reads as itself. */
static uint32_t Utf8Next(const uint8_t **p, const uint8_t *end)
{
    const uint8_t *s = *p;
    uint32_t c = *s++;
    int32_t more = 0;

    if (c >= 0xF0 && c < 0xF8)      { c &= 0x07; more = 3; }
    else if (c >= 0xE0)             { c &= 0x0F; more = 2; }
    else if (c >= 0xC0)             { c &= 0x1F; more = 1; }
    for (; more > 0; more--) {
        if (s >= end || (*s & 0xC0) != 0x80) {
            c = **p;                    /* not valid UTF-8: take the byte */
            s = *p + 1;
            break;
        }
        c = (c << 6) | (*s++ & 0x3F);
    }
    *p = s;
    return c;
}

static int32_t IsSpace(uint32_t c)
{
    return c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == 0xA0 || c == 0x85;
}

static uint8_t CellOf(uint32_t ch)
{
    return (ch == '#' || ch == '*') ? WHITE : (ch == '+' ? SHADE : 0);
}

static Block *FindBlock(Glyphs *g, uint32_t cp, int32_t font)
{
    for (int32_t i = 0; i < g->nblocks; i++)
        if (g->blocks[i].cp == cp && g->blocks[i].font == font)
            return &g->blocks[i];
    return NULL;
}

/* Store the block in hand (replacing an earlier one with the same key). */
static int32_t CommitBlock(Glyphs *g, const Block *b)
{
    Block *old;

    if (b->font == FONT_OTHER)
        return 0;                       /* a font SwosFont never asks for */
    old = FindBlock(g, b->cp, b->font);
    if (old) {
        *old = *b;
        return 0;
    }
    if (g->nblocks == MAX_BLOCKS)
        return -1;
    g->blocks[g->nblocks++] = *b;
    return 0;
}

static int32_t ReadGlyphs(const char *path, Glyphs *g, char *err)
{
    SdBuf f;
    const uint8_t *p, *end;
    Block cur;
    int32_t have = 0;

    if (Sd_ReadFile(path, &f, err) != 0)
        return -1;
    g->blocks = calloc(MAX_BLOCKS, sizeof *g->blocks);
    if (!g->blocks) {
        Sd_BufFree(&f);
        snprintf(err, 256, "out of memory");
        return -1;
    }
    p = f.data;
    end = f.data + f.size;
    if (end - p >= 3 && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF)
        p += 3;

    while (p < end) {
        uint32_t line[256];
        int32_t n = 0, blank = 1;

        /* one line, split on \n, \r\n or \r */
        while (p < end && *p != '\n' && *p != '\r') {
            uint32_t c = Utf8Next(&p, end);
            if (n < (int32_t)(sizeof line / sizeof line[0]))
                line[n++] = c;
        }
        if (p < end && *p == '\r') {
            p++;
            if (p < end && *p == '\n')
                p++;
        } else if (p < end) {
            p++;                        /* '\n' */
        }

        for (int32_t i = 0; i < n; i++)
            if (!IsSpace(line[i])) { blank = 0; break; }

        if (n > 0 && line[0] == '@') {
            int32_t sp = -1;
            if (have && CommitBlock(g, &cur) != 0)
                goto full;
            have = 0;
            /* s = line[1..]; sp = last space in s */
            for (int32_t i = n - 1; i >= 1; i--)
                if (line[i] == ' ') { sp = i - 1; break; }
            if (sp <= 0)
                continue;
            memset(&cur, 0, sizeof cur);
            cur.cp = line[1];
            {
                int32_t fl = n - (sp + 2);
                const uint32_t *fn = line + sp + 2;
                if (fl == 5 && fn[0] == 's' && fn[1] == 'm' && fn[2] == 'a' && fn[3] == 'l' && fn[4] == 'l')
                    cur.font = FONT_SMALL;
                else if (fl == 3 && fn[0] == 'b' && fn[1] == 'i' && fn[2] == 'g')
                    cur.font = FONT_BIG;
                else
                    cur.font = FONT_OTHER;
            }
            have = 1;
            {
                int32_t known = 0;
                for (int32_t i = 0; i < g->norder; i++)
                    if (g->fileOrder[i] == cur.cp) { known = 1; break; }
                if (!known && g->norder < MAX_BLOCKS)
                    g->fileOrder[g->norder++] = cur.cp;
            }
        } else if (blank || line[0] == '#') {
            if (have && CommitBlock(g, &cur) != 0)
                goto full;
            have = 0;
        } else if (have) {
            if (cur.nrows < MAX_KEEP_ROWS) {
                int32_t r = cur.nrows;
                if (n > MAX_COLS) {
                    snprintf(err, 256, "%s: a glyph row is longer than %d", path, MAX_COLS);
                    Sd_BufFree(&f);
                    return -1;
                }
                cur.len[r] = n;
                for (int32_t i = 0; i < n; i++)
                    cur.cell[r][i] = CellOf(line[i]);
            }
            cur.nrows++;
        }
    }
    if (have && CommitBlock(g, &cur) != 0)
        goto full;
    Sd_BufFree(&f);
    return 0;

full:
    snprintf(err, 256, "%s: more than %d glyph blocks", path, MAX_BLOCKS);
    Sd_BufFree(&f);
    return -1;
}

/* Finalize: keep the font's height (6 small, 8 big), add the shadow, trim to
 * the glyph's natural width (at least 1, never wider than its rows). */
static void Finalize(const Block *b, int32_t font, Art *a)
{
    int32_t hRows = font == FONT_SMALL ? 6 : 8;
    int32_t h = b->nrows < hRows ? b->nrows : hRows;
    int32_t w = 0, width = 1;
    uint8_t src[8][MAX_COLS];

    memset(a, 0, sizeof *a);
    memset(src, 0, sizeof src);
    if (h == 0)
        h = 1;                          /* an empty row */
    for (int32_t r = 0; r < h && r < b->nrows; r++) {
        if (b->len[r] > w)
            w = b->len[r];
        memcpy(src[r], b->cell[r], (size_t)b->len[r]);
    }
    /* FillShadow: 1px down-right under each ink pixel, inside the art */
    memcpy(a->px, src, sizeof src);
    for (int32_t r = 0; r < h; r++)
        for (int32_t c = 0; c < w; c++)
            if (src[r][c] == WHITE && r + 1 < h && c + 1 < w && a->px[r + 1][c + 1] == 0)
                a->px[r + 1][c + 1] = SHADE;
    for (int32_t r = 0; r < h; r++)
        for (int32_t c = 0; c < w; c++)
            if (a->px[r][c] != 0 && c + 1 > width)
                width = c + 1;
    a->h = h;
    a->w = width < w ? width : w;
}

/* ---- the sheet ------------------------------------------------------------ */

/* Cells (16x8) of the stock charset: the intro script's glyph cuts between its
 * leading bank load and the first font-extension slot. occ[row][col]. */
static void CoreCells(uint8_t occ[32][16])
{
    int32_t n, i = 0;
    const uint16_t *s = Sd_ScriptWords(SD_SCRIPT_INTRO, &n);
    uint8_t seen[FONT_EXT_START];
    int32_t cx[FONT_EXT_START], cy[FONT_EXT_START];

    memset(occ, 0, 32 * 16);
    memset(seen, 0, sizeof seen);
    while (i < n && ((s[i] & 0xF000) == 0x7000 || (s[i] & 0xF000) == 0x8000))
        i += 3;
    while (i + 2 < n && s[i] != 0xFFFF) {
        int32_t op = s[i], spr = op & 0x0FFF;
        if ((op & 0xF000) != 0xC000 || spr >= FONT_EXT_START)
            break;
        seen[spr] = 1;                  /* a repeated slot: the last cut wins */
        cx[spr] = s[i + 1] >> 8;
        cy[spr] = s[i + 1] & 0xFF;
        i += 3;
    }
    for (int32_t k = 0; k < FONT_EXT_START; k++) {
        if (!seen[k])
            continue;
        if (cy[k] / 8 < 32 && cx[k] / 16 < 16)
            occ[cy[k] / 8][cx[k] / 16] = 1;
    }
}

static int32_t FreeRows(uint8_t occ[32][16], int32_t lo, int32_t hi, int32_t *rows)
{
    int32_t n = 0;
    for (int32_t r = lo; r < hi; r++) {
        int32_t free_ = 1;
        for (int32_t c = 0; c < 16; c++)
            if (occ[r][c]) { free_ = 0; break; }
        if (free_)
            rows[n++] = r;
    }
    return n;
}

static void SetPx(SdImage *s, int32_t x, int32_t y, uint8_t v)
{
    if (x >= 0 && y >= 0 && x < s->w && y < s->h)
        s->px[y * s->w + x] = v;
}

static int32_t Lay(SdImage *sheet, const Art *arts, int32_t count, const int32_t *rows, int32_t nrows,
                   const char *font, char *err)
{
    int32_t ri = 0, col = 0;

    for (int32_t g = 0; g < count; g++) {
        const Art *a = &arts[g];
        int32_t x, y;

        if (col >= 16) { col = 0; ri++; }
        if (ri >= nrows) {
            snprintf(err, 256, "not enough free %s rows in the charset for %d glyphs", font, (int)count);
            return -1;
        }
        x = col * 16;
        y = rows[ri] * 8;
        for (int32_t cc = 0; cc < 16; cc++)        /* clear the cell, then draw */
            for (int32_t rr = 0; rr < 8; rr++)
                SetPx(sheet, x + cc, y + rr, 0);
        for (int32_t rr = 0; rr < a->h; rr++)
            for (int32_t cc = 0; cc < a->w; cc++)
                if (a->px[rr][cc] != 0)
                    SetPx(sheet, x + cc, y + rr, a->px[rr][cc]);
        col++;
    }
    return 0;
}

static void FillMissingBoxes(SdImage *sheet)
{
    for (int32_t row = 0; row < BOX_FILL_ROWS; row++) {
        int32_t big = row >= SPLIT_ROW, bs = big ? 8 : 6;
        for (int32_t col = 0; col < 16; col++) {
            int32_t x = col * 16, y = row * 8, empty = 1;
            if (x + 16 > sheet->w || y + 8 > sheet->h)
                continue;
            for (int32_t rr = 0; rr < 8 && empty; rr++)
                for (int32_t cc = 0; cc < 16; cc++)
                    if (sheet->px[(y + rr) * sheet->w + x + cc] != 0) { empty = 0; break; }
            if (!empty)
                continue;
            for (int32_t rr = 0; rr < bs; rr++)
                for (int32_t cc = 0; cc < bs; cc++) {
                    uint8_t v = big ? kBigBox[rr][cc] : kSmallBox[rr][cc];
                    if (v)
                        sheet->px[(y + rr) * sheet->w + x + cc] = v;
                }
        }
    }
}

int32_t Sd_FontExtend(SdImage *charset, const char *glyphsTxtPath, char *err)
{
    Glyphs g;
    uint32_t seq[MAX_BLOCKS];
    int32_t nseq = 0, rc = -1;
    Art *small = NULL, *big = NULL;
    uint8_t occ[32][16];
    int32_t smallRows[32], bigRows[32], nSmall, nBig;

    memset(&g, 0, sizeof g);
    if (ReadGlyphs(glyphsTxtPath, &g, err) != 0)
        goto out;

    CoreCells(occ);
    nSmall = FreeRows(occ, 0, SPLIT_ROW, smallRows);
    nBig = FreeRows(occ, SPLIT_ROW, 32, bigRows);

    /* the fixed order first, then the file's other glyphs; a glyph needs both fonts */
    for (int32_t i = 0; i < ORDER_COUNT; i++)
        if (FindBlock(&g, kOrder[i], FONT_SMALL) && FindBlock(&g, kOrder[i], FONT_BIG))
            seq[nseq++] = kOrder[i];
    for (int32_t i = 0; i < g.norder; i++) {
        uint32_t ch = g.fileOrder[i];
        int32_t skip = 0;
        for (int32_t k = 0; k < ORDER_COUNT && !skip; k++)
            if (kOrder[k] == ch) skip = 1;
        for (int32_t k = 0; k < nseq && !skip; k++)
            if (seq[k] == ch) skip = 1;
        if (!skip && FindBlock(&g, ch, FONT_SMALL) && FindBlock(&g, ch, FONT_BIG))
            seq[nseq++] = ch;
    }

    small = calloc((size_t)(nseq ? nseq : 1), sizeof *small);
    big = calloc((size_t)(nseq ? nseq : 1), sizeof *big);
    if (!small || !big) {
        snprintf(err, 256, "out of memory");
        goto out;
    }
    for (int32_t i = 0; i < nseq; i++) {
        Finalize(FindBlock(&g, seq[i], FONT_SMALL), FONT_SMALL, &small[i]);
        Finalize(FindBlock(&g, seq[i], FONT_BIG), FONT_BIG, &big[i]);
    }

    if (Lay(charset, small, nseq, smallRows, nSmall, "small", err) != 0 ||
        Lay(charset, big, nseq, bigRows, nBig, "big", err) != 0)
        goto out;
    FillMissingBoxes(charset);
    rc = 0;

out:
    free(small);
    free(big);
    free(g.blocks);
    return rc;
}
