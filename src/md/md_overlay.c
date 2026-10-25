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

uint16_t md_ov[MD_OV_ROWS][MD_OV_COLS];
static uint32_t s_rowsDrawn;            /* rows with anything in md_ov this frame */
static uint32_t s_rowsShown;            /* rows not blank on screen */
static uint16_t s_rowHash[MD_OV_ROWS];  /* what each row on screen holds */
static int16_t s_offY = 24, s_rows = 28;

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

void md_ov_reset(int16_t offY, int16_t screenH)
{
    s_offY = offY;
    s_rows = (int16_t)(screenH / 8);
    if (s_rows > MD_OV_ROWS) s_rows = MD_OV_ROWS;
    for (int r = 0; r < MD_OV_ROWS; r++) {
        for (int c = 0; c < MD_OV_COLS; c++) md_ov[r][c] = 0;
        s_rowHash[r] = 0;
    }
    s_rowsDrawn = s_rowsShown = 0;
    s_cacheUsed = 0;
    md_ovSprCount = 0;
}

static uint16_t row_hash(const uint16_t *row)
{
    uint16_t h = 0;
    for (int c = 0; c < MD_OV_COLS; c++) h = (uint16_t)((h << 3 | h >> 13) ^ row[c] ^ c);
    return h;
}

/* End of frame: the rows that changed go to plane A, then the grid is emptied. The
 * writes are the CPU's, straight away (a few rows at most, and the text need not wait
 * for the frame's sprites). */
void md_ov_commit(void)
{
    uint32_t rows = s_rowsDrawn | s_rowsShown;
    for (int pass = 0; pass < 2 && rows; pass++) {
        int full = 0;
        for (int r = 0; r < s_rows; r++) {
            if (!(rows & (1UL << r))) continue;
            uint16_t *row = md_ov[r];
            uint16_t h = (s_rowsDrawn & (1UL << r)) ? row_hash(row) : 0;
            if (h == s_rowHash[r]) continue;
            uint16_t w[MD_OV_COLS];
            for (int c = 0; c < MD_OV_COLS; c++) {
                uint16_t v = row[c];
                w[c] = v ? cell_attr(v) : 0;
                if (v >> 8 && !w[c]) full = 1;
            }
            if (full) break;
            md_irq_off();
            MD_VDP_CTRL32 = VRAM_W(VRAM_PLANE_A + r * PLANE_W * 2);
            for (int c = 0; c < MD_OV_COLS; c++) MD_VDP_DATA = w[c];
            md_irq_on();
            s_rowHash[r] = h;
            if (h) s_rowsShown |= 1UL << r; else s_rowsShown &= ~(1UL << r);
        }
        if (!full) break;
        /* the cache ran out: empty it and write every row again from it */
        s_cacheUsed = 0;
        for (int r = 0; r < MD_OV_ROWS; r++) s_rowHash[r] = 0xFFFF;
        rows = s_rowsDrawn | s_rowsShown;
    }
    for (int r = 0; r < s_rows; r++)
        if (s_rowsDrawn & (1UL << r))
            for (int c = 0; c < MD_OV_COLS; c++) md_ov[r][c] = 0;
    s_rowsDrawn = 0;
}

/* ---- drawing: the engine's window -> cells ---- */

void md_ov_put(int16_t col, int16_t row, const char *s, int16_t max)
{
    if (row < 0 || row >= s_rows) return;
    for (; *s && max-- > 0; s++, col++) {
        if (col < 0) continue;
        if (col >= MD_OV_COLS) break;
        uint8_t ch = (uint8_t)*s;
        if (ch >= 'a' && ch <= 'z') ch = (uint8_t)(ch - 32);
        md_ov[row][col] = (uint16_t)((md_ov[row][col] & 0xFF00) | (ch == ' ' ? 0 : ch));
    }
    s_rowsDrawn |= 1UL << row;
}

static int16_t text_row(int16_t y) { return (int16_t)((y - s_offY + 4) >> 3); }
static int16_t text_col(int16_t x) { return (int16_t)((x - 8 + 4) >> 3); }

static uint16_t str_len(const uint8_t *s) { uint16_t n = 0; while (s[n]) n++; return n; }

/* The cells a rectangle covers: those whose centre is inside it. */
static void ov_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t bg)
{
    int16_t sx = (int16_t)(x - 8), sy = (int16_t)(y - s_offY);
    int16_t c0 = (int16_t)((sx + 3) >> 3), c1 = (int16_t)((sx + w - 5) >> 3);
    int16_t r0 = (int16_t)((sy + 3) >> 3), r1 = (int16_t)((sy + h - 5) >> 3);
    if (c0 < 0) c0 = 0;
    if (c1 >= MD_OV_COLS) c1 = MD_OV_COLS - 1;
    if (r0 < 0) r0 = 0;
    if (r1 >= s_rows) r1 = (int16_t)(s_rows - 1);
    for (int16_t r = r0; r <= r1; r++) {
        for (int16_t c = c0; c <= c1; c++) md_ov[r][c] = (uint16_t)((bg << 8) | (md_ov[r][c] & 0xFF));
        s_rowsDrawn |= 1UL << r;
    }
}

/* text_render.c: the string's left edge at x; returns where the next character goes */
uint16_t DrawMenuText(uint16_t x, uint16_t y, uint16_t color, uint8_t *text, uint8_t *charsTable)
{
    (void)color; (void)charsTable;
    uint16_t n = str_len(text);
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
