/* md_overlay.c -- what the engine draws INTO the frame rather than as sprites: the
 * MATCH STATS board, the bench and its substitutes menu, the HUD's text.
 *
 * On the Amiga these are blits into the frontbuffer: text in the game's fonts
 * (DrawMenuText), the darkened grid behind the statistics (DarkenRectangle), the
 * substitutes menu's boxes (FillRectangle), and the faces, cards and bench players as
 * bobs (DrawSprite16Pixels, DrawSpriteInCameraView). The Mega Drive has no frontbuffer,
 * so the same calls draw into a grid of plane A cells -- one per 8x8 of the screen -- and
 * a short list of extra VDP sprites. md_render_frame (md_render.c) puts the grid on
 * plane A at the end of each frame, rewriting only the rows that changed, and empties
 * it for the next; a loop that holds the picture (KeepStatisticsOnScreen) draws nothing,
 * so what it drew stays up.
 *
 * A cell is a character over a background: none (the pitch shows through), darkened
 * (DarkenRectangle: a dither of black), or a solid colour of the game palette
 * (FillRectangle). Character-over-background tiles are made when first needed into a
 * small VRAM cache (TILE_UI, TILE_UI2 and the unused tail of the hscroll table).
 *
 * Coordinates are the engine's: its 336x272 simulation window, which the screen crops
 * 8 columns off each side and s_offY lines off the top (md_render.c).
 */
#include "md_hw.h"
#include "md_game.h"
#include "md_render.h"
#include "md_data.h"
#include "game_types.h"
#include "md_overlay.h"

extern const uint32_t md_font_tiles[96 * 8];
extern uint8_t *_spriteGraphicsPtr;
extern FixedPoint _cameraX, _cameraY;

/* the grid: a character and a background a cell (two byte planes: the engine's calls
 * write one or the other, a byte store each) */
static uint8_t s_ch[MD_OV_ROWS][MD_OV_COLS] __attribute__((aligned(4)));
static uint8_t s_bg[MD_OV_ROWS][MD_OV_COLS] __attribute__((aligned(4)));
static const uint32_t kBit[32] = {
    1u << 0, 1u << 1, 1u << 2, 1u << 3, 1u << 4, 1u << 5, 1u << 6, 1u << 7,
    1u << 8, 1u << 9, 1u << 10, 1u << 11, 1u << 12, 1u << 13, 1u << 14, 1u << 15,
    1u << 16, 1u << 17, 1u << 18, 1u << 19, 1u << 20, 1u << 21, 1u << 22, 1u << 23,
    1u << 24, 1u << 25, 1u << 26, 1u << 27, 1u << 28, 1u << 29, 1u << 30, 1u << 31,
};
static uint32_t s_rowsDrawn;            /* rows with anything in the grid this frame */
static uint32_t s_rowsShown;            /* rows not blank on screen */
static uint16_t s_rowHash[MD_OV_ROWS];  /* what each row on screen holds */
static int16_t s_offY = 24, s_rows = 28;
static uint8_t s_engineDrew, s_engineDrewLast;   /* the engine drew into the grid (not the HUD) */
static uint32_t s_callHash, s_lastCallHash;      /* the engine's calls this frame, the last */
static uint8_t s_static, s_sameRun;              /* (see call_hash below) */

MdSprCmd md_ovSpr[MD_OV_SPR_MAX];
uint16_t md_ovSprCount;

/* ---- the tile cache: (character, background) -> a tile ---- */
#define CACHE_N (32 + 64 + 31)
static uint16_t s_key[CACHE_N];         /* the cell value, 0 = free */
static uint8_t  s_cacheUsed;

static uint16_t cache_tile(uint8_t slot)
{
    if (slot < 32) return (uint16_t)(TILE_UI + slot);
    if (slot < 96) return (uint16_t)(TILE_UI2 + slot - 32);
    return (uint16_t)((VRAM_HSCROLL + 32) / 32 + slot - 96);    /* the hscroll table's tail */
}

#define VRAM_W(a) (0x40000000u | (((uint32_t)(a) & 0x3FFF) << 16) | (((uint32_t)(a) >> 14) & 3))

static void make_tile(uint16_t tile, uint16_t cell)
{
    uint8_t ch = (uint8_t)cell, bg = (uint8_t)(cell >> 8);
    const uint32_t *g = &md_font_tiles[(ch >= 32 && ch < 128 ? ch - 32 : 0) * 8];
    md_irq_off();
    MD_VDP_CTRL32 = VRAM_W(tile * 32);
    for (int y = 0; y < 8; y++) {
        uint32_t src = g[y], out = 0;
        for (int x = 7; x >= 0; x--) {
            uint32_t p = (src >> (x * 4)) & 15;
            if (!p) {
                if (bg == MD_OV_DARK) p = ((x + y) & 1) ? 3 : 0;
                else p = (uint32_t)(bg - MD_OV_SOLID) & 15;
            }
            out = (out << 4) | p;
        }
        MD_VDP_DATA32 = out;
    }
    md_irq_on();
}

/* the nametable word for a cell; 0 when the cache is full (the caller starts over) */
static uint16_t cell_attr(uint16_t cell)
{
    uint8_t ch = (uint8_t)cell;
    if (!(cell >> 8)) return ch > ' ' && ch < 128 ? TILE_ATTR(0, 1, 0, 0, TILE_FONT + ch - 32) : 0;
    for (uint8_t i = 0; i < s_cacheUsed; i++)
        if (s_key[i] == cell) return TILE_ATTR(0, 1, 0, 0, cache_tile(i));
    if (s_cacheUsed >= CACHE_N) return 0;
    s_key[s_cacheUsed] = cell;
    make_tile(cache_tile(s_cacheUsed), cell);
    return TILE_ATTR(0, 1, 0, 0, cache_tile(s_cacheUsed++));
}

static void rect_flush(void);

static void clear_drawn(void)
{
    uint32_t m = s_rowsDrawn;
    for (int r = 0; m; r++, m >>= 1)
        if (m & 1) {
            uint32_t *a = (uint32_t *)s_ch[r], *b = (uint32_t *)s_bg[r];
            for (int i = 0; i < MD_OV_COLS / 4; i++) a[i] = b[i] = 0;
        }
    s_rowsDrawn = 0;
}

int md_ov_static(void) { return s_static; }

/* Whether the frame needs the overlay pass: the engine drew this frame or the last one
 * (whose drawing is still on plane A). */
int md_ov_drawn(void) { return s_engineDrew | s_engineDrewLast; }

void md_ov_reset(int16_t offY, int16_t screenH)
{
    rect_flush();
    s_engineDrew = s_engineDrewLast = 0;
    s_static = s_sameRun = 0;
    s_callHash = s_lastCallHash = 0;
    s_offY = offY;
    s_rows = (int16_t)(screenH / 8);
    if (s_rows > MD_OV_ROWS) s_rows = MD_OV_ROWS;
    for (int r = 0; r < MD_OV_ROWS; r++) {
        for (int c = 0; c < MD_OV_COLS; c++) s_ch[r][c] = s_bg[r][c] = 0;
        s_rowHash[r] = 0;
    }
    s_rowsDrawn = s_rowsShown = 0;
    s_cacheUsed = 0;
    md_ovSprCount = 0;
}

static uint16_t row_hash(int r)
{
    const uint32_t *a = (const uint32_t *)s_ch[r], *b = (const uint32_t *)s_bg[r];
    uint32_t h = 0;
    for (int i = 0; i < MD_OV_COLS / 4; i++) h = (h << 5 | h >> 27) ^ a[i] ^ (b[i] << 1) ^ (uint32_t)i;
    h ^= h >> 16;
    return (uint16_t)(h ? h : 1);
}

/* End of frame: the rows that changed go to plane A, then the grid is emptied. The
 * writes are the CPU's, straight away (a few rows at most, and the text need not wait
 * for the frame's sprites). */
void md_ov_commit(int hudChanged)
{
    if (s_static) {
        int same = s_callHash == s_lastCallHash && !hudChanged;
        s_callHash = 0;
        clear_drawn();
        s_engineDrewLast = s_engineDrew;
        s_engineDrew = 0;
        if (!same) {                    /* something changed: draw in full from the next frame */
            s_static = 0;
            s_sameRun = 0;
            s_lastCallHash = 0;
            s_engineDrewLast = 1;
        }
        return;
    }
    rect_flush();
    uint32_t rows = s_rowsDrawn | s_rowsShown;
    for (int pass = 0; pass < 2 && rows; pass++) {
        int full = 0;
        for (int r = 0; r < s_rows; r++) {
            if (!(rows & kBit[r])) continue;
            uint16_t h = (s_rowsDrawn & kBit[r]) ? row_hash(r) : 0;
            if (h == s_rowHash[r]) continue;
            uint16_t w[MD_OV_COLS];
            for (int c = 0; c < MD_OV_COLS; c++) {
                uint16_t v = (uint16_t)(s_bg[r][c] << 8 | s_ch[r][c]);
                w[c] = v ? cell_attr(v) : 0;
                if (v >> 8 && !w[c]) full = 1;
            }
            if (full) break;
            md_irq_off();
            MD_VDP_CTRL32 = VRAM_W(VRAM_PLANE_A + r * PLANE_W * 2);
            for (int c = 0; c < MD_OV_COLS; c++) MD_VDP_DATA = w[c];
            md_irq_on();
            s_rowHash[r] = h;
            if (h) s_rowsShown |= kBit[r]; else s_rowsShown &= ~kBit[r];
        }
        if (!full) break;
        /* the cache ran out: empty it and write every row again from it */
        s_cacheUsed = 0;
        for (int r = 0; r < MD_OV_ROWS; r++) s_rowHash[r] = 0xFFFF;
        rows = s_rowsDrawn | s_rowsShown;
    }
    clear_drawn();
    if (s_engineDrew && s_callHash == s_lastCallHash) {
        if (++s_sameRun >= 2) s_static = 1;
    } else {
        s_sameRun = 0;
    }
    s_lastCallHash = s_callHash;
    s_callHash = 0;
    s_engineDrewLast = s_engineDrew;
    s_engineDrew = 0;
}

/* ---- drawing: the engine's window -> cells ---- */

void md_ov_put(int16_t col, int16_t row, const char *s, int16_t max)
{
    if (row < 0 || row >= s_rows) return;
    uint8_t *d = s_ch[row];
    for (; *s && max-- > 0; s++, col++) {
        if (col < 0) continue;
        if (col >= MD_OV_COLS) break;
        uint8_t ch = (uint8_t)*s;
        if (ch >= 'a' && ch <= 'z') ch = (uint8_t)(ch - 32);
        d[col] = ch == ' ' ? 0 : ch;
    }
    s_rowsDrawn |= kBit[row];
}

static int16_t text_row(int16_t y) { return (int16_t)((y - s_offY + 4) >> 3); }
static int16_t text_col(int16_t x) { return (int16_t)((x - 8 + 4) >> 3); }

static uint16_t str_len(const uint8_t *s) { uint16_t n = 0; while (s[n]) n++; return n; }

/* The cells a rectangle covers: those whose centre is inside it. */
static void rect_cells(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t bg)
{
    int16_t sx = (int16_t)(x - 8), sy = (int16_t)(y - s_offY);
    int16_t c0 = (int16_t)((sx + 3) >> 3), c1 = (int16_t)((sx + w - 5) >> 3);
    int16_t r0 = (int16_t)((sy + 3) >> 3), r1 = (int16_t)((sy + h - 5) >> 3);
    if (c0 < 0) c0 = 0;
    if (c1 >= MD_OV_COLS) c1 = MD_OV_COLS - 1;
    if (r0 < 0) r0 = 0;
    if (r1 >= s_rows) r1 = (int16_t)(s_rows - 1);
    if (c0 > c1) return;
    for (int16_t r = r0; r <= r1; r++) {
        uint8_t *d = &s_bg[r][c0], *e = &s_bg[r][c1];
        do *d = bg; while (d++ != e);
        s_rowsDrawn |= kBit[r];
    }
}

/* DrawGrid lays its squares side by side, nineteen to a row of the statistics board: a
 * rectangle that continues the last one (same row, same background, starting where it
 * ended) is merged into it, and the run is written when something else comes along. */
static int16_t s_px, s_py, s_pw, s_ph;
static uint8_t s_pbg;               /* 0: nothing pending */

static void rect_flush(void)
{
    if (s_pbg) rect_cells(s_px, s_py, s_pw, s_ph, s_pbg);
    s_pbg = 0;
}

/* ---- a frame that draws what the last ones drew ----
 * The statistics board is drawn again every frame it is up, the same each time. Every
 * call the engine makes is folded into s_callHash; once three frames in a row hash the
 * same, the overlay is static: the calls only hash, and a frame that hashes the same
 * again costs nothing more. One that differs ends it, and the next frame draws in full
 * (a frame later than it would have: the picture on plane A is the last one meanwhile). */
static inline void call_hash(uint32_t v)
{
    s_engineDrew = 1;
    s_callHash = (s_callHash << 5 | s_callHash >> 27) ^ v;
}

static void ov_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t bg)
{
    call_hash((uint32_t)(uint16_t)x ^ ((uint32_t)(uint16_t)y << 16) ^ ((uint32_t)w << 7) ^ ((uint32_t)h << 21) ^ bg);
    if (s_static) return;
    if (s_pbg == bg && y == s_py && h == s_ph && x == s_px + s_pw) { s_pw = (int16_t)(s_pw + w); return; }
    rect_flush();
    s_px = x; s_py = y; s_pw = w; s_ph = h; s_pbg = bg;
}

/* text_render.c: the string's left edge at x; returns where the next character goes */
uint16_t DrawMenuText(uint16_t x, uint16_t y, uint16_t color, uint8_t *text, uint8_t *charsTable)
{
    (void)color; (void)charsTable;
    uint16_t n = 0;
    {
        uint32_t h = (uint32_t)x ^ ((uint32_t)y << 16);
        while (text[n]) { h = (h << 3 | h >> 29) ^ text[n]; n++; }
        call_hash(h);
    }
    if (s_static) return (uint16_t)(x + n * 8);
    md_ov_put(text_col((int16_t)x), text_row((int16_t)y), (const char *)text, (int16_t)n);
    return (uint16_t)(x + n * 8);
}

uint16_t DrawMenuTextCentered(uint16_t x, uint16_t y, uint16_t color, uint8_t *text, uint8_t *charsTable)
{
    uint16_t n = str_len(text);
    return DrawMenuText((uint16_t)(x - n * 4), y, color, text, charsTable);
}

uint16_t DrawMenuTextRightAligned(uint16_t x, uint16_t y, uint16_t color, uint8_t *text, uint8_t *charsTable)
{
    uint16_t n = str_len(text);
    return DrawMenuText((uint16_t)(x - n * 8 + 1), y, color, text, charsTable);
}

uint16_t GetTextWidth(uint8_t *text, uint8_t *charsTable)
{
    (void)charsTable;
    return (uint16_t)(str_len(text) * 8);
}

/* sprites.c: the grid square behind the statistics (16x16) darkens what is under it */
void DarkenRectangle(uint16_t spriteIndex, int16_t x, int16_t y)
{
    const SpriteGraphics *g = ((SpriteGraphics *const *)_spriteGraphicsPtr)[spriteIndex];
    ov_rect(x, y, g->pixWidth ? g->pixWidth : 16, g->nlines ? g->nlines : 16, MD_OV_DARK);
}

/* statistics.c DrawGrid: its 16x16 squares as one area */
void MD_DarkenArea(int16_t x, int16_t y, int16_t w, int16_t h)
{
    ov_rect(x, y, w, h, MD_OV_DARK);
}

/* blitter.c: a box of one colour of the game palette */
void FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    ov_rect((int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, (uint8_t)(MD_OV_SOLID + (color & 15)));
}

static void ov_sprite(uint16_t idx, int16_t x, int16_t y)
{
    static Sprite s_flat;               /* z 0: what md_render.c reads of an overlay sprite */
    if (md_ovSprCount >= MD_OV_SPR_MAX) return;
    MdSprCmd *c = &md_ovSpr[md_ovSprCount++];
    c->spr = &s_flat;
    c->image = idx;
    c->x = x;
    c->y = y;
}

/* sprites.c: top-left at (x, y) of the window */
void DrawSprite16Pixels(uint16_t spriteIndex, int16_t x, int16_t y)
{
    ov_sprite(spriteIndex, x, y);
}

/* sprites.c: at a pitch position, culled as the original culls (SWOS2.s line 6520) */
void DrawSpriteInCameraView(uint16_t spriteIndex, int16_t x, int16_t y)
{
    const SpriteGraphics *g = ((SpriteGraphics *const *)_spriteGraphicsPtr)[spriteIndex];
    x = (int16_t)(x - _cameraX.s.i);
    y = (int16_t)(y - _cameraY.s.i);
    if (x >= 336 || y >= 272 || x <= -g->pixWidth || y <= -g->nlines) return;
    ov_sprite(spriteIndex, x, y);
}

void DrawSpriteHotspotInCameraView(uint16_t spriteIndex, int16_t x, int16_t y)
{
    const SpriteGraphics *g = ((SpriteGraphics *const *)_spriteGraphicsPtr)[spriteIndex];
    DrawSpriteInCameraView(spriteIndex, (int16_t)(x - g->centerX), (int16_t)(y - g->centerY));
}
