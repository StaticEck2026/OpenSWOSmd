/* md_frontend.c -- the cartridge's front end: title, main menu, friendly, options, result.
 *
 * The Amiga's menus (src/menus.c and the menu system) draw into a 336x256 bitmap and
 * run from data the cartridge cannot hold in 64 KB of RAM, so the Mega Drive has a front
 * end of its own, in the same spirit: a FRIENDLY screen picks the two teams (by country,
 * then team) and who plays each, an OPTIONS screen sets what the Amiga's OPTIONS menu
 * does (game length, pitch type, auto replays), and the match itself is the shared
 * engine's (md_match.c md_play_friendly -> InitAndPlayGame).
 *
 * Drawing: plane A holds the text, plane B the background and the buttons. Plane B is
 * scrolled up 4 lines so a two-cell-tall button is centred on one row of text. All of it
 * is CPU writes with the interrupt off (the VBlank's DMA queue shares the VDP's address
 * register); a screen changes a few rows at a time.
 *
 * Tiles (outside a match the pitch's VRAM is free; md_render.c reloads it at kick-off):
 *   1..384    the font at twice the size (generated from md_font_tiles)
 *   400       background pattern;  401..409 the button's nine pieces
 *   420..431  the two teams' kit drawings (2x3 cells each)
 */
#include "md_hw.h"
#include "md_game.h"
#include "md_data.h"
#include "game_types.h"
#include "build_info.h"
#include <string.h>

extern const uint32_t md_font_tiles[96 * 8];
extern void *_countriesTable[];
extern uint16_t _gamePalette[16];
extern uint8_t _shirtColorTable[10];
extern uint16_t _gameLength, _autoReplays;
extern uint16_t _team1TotalGoals, _team2TotalGoals, _team1PenaltyGoals, _team2PenaltyGoals;
extern uint16_t _gameCanceled;
extern TeamStatisticsData _team1StatsData, _team2StatsData;

#define TILE_BIG   1
#define TILE_BG    400
#define TILE_BOX   401
#define TILE_KIT   420

#define RGB(r, g, b) ((uint16_t)(((b) << 9) | ((g) << 5) | ((r) << 1)))

/* text palettes: 0 white, 1 yellow (the selected item), 2 light blue (labels), 3 grey */
#define PAL_TEXT 0
#define PAL_SEL  1
#define PAL_LBL  2
#define PAL_DIM  3

static uint16_t s_pal[64];
static uint8_t s_row0;          /* first row of the 28-row layout: 1 on PAL (30 rows) */

/* ---------------------------------------------------------------- the options */
MdOptions md_options = { MD_PITCH_SEASONAL, 8 /* SEP */ };

/* ---------------------------------------------------------------- drawing */

#define VRAM_W(a) (0x40000000u | (((uint32_t)(a) & 0x3FFF) << 16) | (((uint32_t)(a) >> 14) & 3))

static void cells(uint16_t plane, uint16_t x, uint16_t y, const uint16_t *w, uint16_t n)
{
    md_irq_off();
    MD_VDP_CTRL32 = VRAM_W(plane + ((y + s_row0) * PLANE_W + x) * 2);
    while (n--) MD_VDP_DATA = *w++;
    md_irq_on();
}

static void cell_fill(uint16_t plane, uint16_t x, uint16_t y, uint16_t attr, uint16_t n)
{
    md_irq_off();
    MD_VDP_CTRL32 = VRAM_W(plane + ((y + s_row0) * PLANE_W + x) * 2);
    while (n--) MD_VDP_DATA = attr;
    md_irq_on();
}

static uint16_t str_len(const char *s) { uint16_t n = 0; while (s[n]) n++; return n; }

static void text(uint16_t x, uint16_t y, uint8_t pal, const char *s)
{
    uint16_t w[40], n = 0;
    for (; *s && x + n < 40; s++) {
        uint8_t c = (uint8_t)*s;
        if (c >= 'a' && c <= 'z') c = (uint8_t)(c - 32);
        w[n++] = c > ' ' && c < 127 ? TILE_ATTR(pal, 0, 0, 0, TILE_FONT + c - 32) : 0;
    }
    cells(VRAM_PLANE_A, x, y, w, n);
}

static void text_c(uint16_t x0, uint16_t width, uint16_t y, uint8_t pal, const char *s)
{
    uint16_t n = str_len(s);
    if (n > width) n = width;
    text((uint16_t)(x0 + (width - n) / 2), y, pal, s);
}

static void clear_row(uint16_t y) { cell_fill(VRAM_PLANE_A, 0, y, 0, 40); }

/* the double-size font: four tiles a character, two rows */
static void big(uint16_t x, uint16_t y, uint8_t pal, const char *s)
{
    uint16_t top[20], bot[20], n = 0;
    for (; *s && n < 20; s++) {
        uint8_t c = (uint8_t)*s;
        uint16_t t = c > ' ' && c < 127 ? (uint16_t)(TILE_BIG + (c - 32) * 4) : 0;
        top[n] = t ? TILE_ATTR(pal, 0, 0, 0, t) : 0;
        bot[n] = t ? TILE_ATTR(pal, 0, 0, 0, t + 2) : 0;
        n++;
    }
    {
        uint16_t w[40];
        for (uint16_t i = 0; i < n; i++) { w[i * 2] = top[i]; w[i * 2 + 1] = top[i] ? top[i] + 1 : 0; }
        cells(VRAM_PLANE_A, x, y, w, (uint16_t)(n * 2));
        for (uint16_t i = 0; i < n; i++) { w[i * 2] = bot[i]; w[i * 2 + 1] = bot[i] ? bot[i] + 1 : 0; }
        cells(VRAM_PLANE_A, x, (uint16_t)(y + 1), w, (uint16_t)(n * 2));
    }
}

static void big_c(uint16_t y, uint8_t pal, const char *s)
{
    big((uint16_t)(20 - str_len(s)), y, pal, s);
}

/* a button on plane B: w cells wide, two tall, at text row y (plane B is 4 lines up) */
static void button(uint16_t x, uint16_t y, uint16_t w, int sel)
{
    uint16_t a[40], b[40];
    uint8_t pal = sel ? 1 : 0;
    for (uint16_t i = 0; i < w; i++) {
        uint16_t col = i == 0 ? 0 : i == w - 1 ? 2 : 1;
        a[i] = TILE_ATTR(pal, 0, 0, 0, TILE_BOX + col);
        b[i] = TILE_ATTR(pal, 0, 0, 0, TILE_BOX + 6 + col);
    }
    cells(VRAM_PLANE_B, x, y, a, w);
    cells(VRAM_PLANE_B, x, (uint16_t)(y + 1), b, w);
}

static void unbutton(uint16_t x, uint16_t y, uint16_t w)
{
    cell_fill(VRAM_PLANE_B, x, y, TILE_ATTR(0, 0, 0, 0, TILE_BG), w);
    cell_fill(VRAM_PLANE_B, x, (uint16_t)(y + 1), TILE_ATTR(0, 0, 0, 0, TILE_BG), w);
}

/* a button with its label (and arrows, for a value that cycles) */
static void item(uint16_t x, uint16_t y, uint16_t w, int sel, int arrows, const char *label)
{
    button(x, y, w, sel);
    cell_fill(VRAM_PLANE_A, x, y, 0, w);
    if (arrows) {
        text(x + 1, y, sel ? PAL_SEL : PAL_TEXT, "<");
        text((uint16_t)(x + w - 2), y, sel ? PAL_SEL : PAL_TEXT, ">");
        text_c((uint16_t)(x + 2), (uint16_t)(w - 4), y, sel ? PAL_SEL : PAL_TEXT, label);
    } else {
        text_c(x, w, y, sel ? PAL_SEL : PAL_TEXT, label);
    }
}

/* ---------------------------------------------------------------- set-up */

static uint32_t scale_row(uint32_t r, int half)   /* 4 pixels of a font row, doubled */
{
    uint32_t o = 0;
    r >>= half ? 0 : 16;
    for (int i = 3; i >= 0; i--) {
        uint32_t p = (r >> (i * 4)) & 15;
        o = (o << 8) | (p << 4) | p;
    }
    return o;
}

static void put_tile(uint16_t tile, const uint32_t *rows)
{
    md_irq_off();
    MD_VDP_CTRL32 = VRAM_W(tile * 32);
    for (int i = 0; i < 8; i++) MD_VDP_DATA32 = rows[i];
    md_irq_on();
}

static void make_tiles(void)
{
    uint32_t t[8];
    for (int c = 0; c < 96; c++) {
        const uint32_t *g = &md_font_tiles[c * 8];
        for (int q = 0; q < 4; q++) {           /* TL, TR, BL, BR */
            int right = q & 1, lower = q >> 1;
            for (int y = 0; y < 8; y++) t[y] = scale_row(g[lower * 4 + y / 2], right);
            put_tile((uint16_t)(TILE_BIG + c * 4 + q), t);
        }
    }
    /* the background: a diagonal weave of colours 1 and 4 */
    for (int y = 0; y < 8; y++) {
        uint32_t r = 0;
        for (int x = 0; x < 8; x++) r = (r << 4) | (((x + y) & 7) < 3 ? 4u : 1u);
        t[y] = r;
    }
    put_tile(TILE_BG, t);
    /* the button: border 6, fill 7, a dark line under it (3) */
    for (int p = 0; p < 9; p++) {
        int col = p % 3, row = p / 3;
        for (int y = 0; y < 8; y++) {
            uint32_t r = 0;
            for (int x = 0; x < 8; x++) {
                uint32_t c = 7;
                if ((row == 0 && y == 0) || (col == 0 && x == 0) || (col == 2 && x == 7)) c = 6;
                if (row == 2 && y >= 6) c = y == 6 ? 6 : 3;
                if (row == 2 && y == 7 && ((col == 0 && x == 0) || (col == 2 && x == 7))) c = 0;
                if (row == 0 && y == 0 && ((col == 0 && x == 0) || (col == 2 && x == 7))) c = 0;
                r = (r << 4) | c;
            }
            t[y] = r;
        }
        put_tile((uint16_t)(TILE_BOX + p), t);
    }
}

static uint16_t vdp_color(uint16_t amiga)
{
    uint16_t r = (amiga >> 8) & 15, g = (amiga >> 4) & 15, b = amiga & 15;
    return (uint16_t)(((b >> 1) << 9) | ((g >> 1) << 5) | ((r >> 1) << 1));
}

static void set_palettes(void)
{
    static const uint16_t base[16] = {
        RGB(0, 0, 0), RGB(0, 0, 2), RGB(7, 7, 7), RGB(0, 0, 0),
        RGB(0, 1, 3), RGB(4, 6, 7), RGB(3, 5, 7), RGB(1, 2, 5),
    };
    for (int l = 0; l < 4; l++)
        for (int i = 0; i < 8; i++) s_pal[l * 16 + i] = base[i];
    s_pal[16 + 2] = RGB(7, 7, 1);           /* selected: yellow text, a bright button */
    s_pal[16 + 6] = RGB(7, 7, 4);
    s_pal[16 + 7] = RGB(5, 2, 0);
    s_pal[32 + 2] = RGB(4, 6, 7);           /* labels */
    s_pal[48 + 2] = RGB(4, 4, 4);           /* dim */
}

static void palette_show(int level)        /* 0 black .. 8 full */
{
    uint16_t p[64];
    for (int i = 0; i < 64; i++) {
        uint16_t c = s_pal[i];
        uint16_t r = (c >> 1) & 7, g = (c >> 5) & 7, b = (c >> 9) & 7;
        r = (uint16_t)(r * level / 8); g = (uint16_t)(g * level / 8); b = (uint16_t)(b * level / 8);
        p[i] = RGB(r, g, b);
    }
    md_irq_off();
    md_cram_write(0, p, 64);
    md_irq_on();
}

static void fade_in(void)  { for (int l = 1; l <= 8; l++) { md_wait_vblank(); palette_show(l); } }
static void fade_out(void) { for (int l = 7; l >= 0; l--) { md_wait_vblank(); palette_show(l); } }

/* a blank screen: the background on plane B, nothing on plane A */
static void screen_clear(void)
{
    md_wait_vblank();
    md_irq_off();
    md_vdp_display(0);
    md_vdp_set_reg(11, 0x00);
    md_vram_fill(VRAM_SPRITES, 0, 4);
    md_vram_fill(VRAM_PLANE_A, 0, PLANE_W * PLANE_H);
    md_vram_fill(VRAM_PLANE_B, TILE_ATTR(0, 0, 0, 0, TILE_BG), PLANE_W * PLANE_H);
    md_vram_fill(VRAM_HSCROLL, 0, 2);
    md_vsram_write(0, 0);
    md_vsram_write(1, 4);           /* plane B 4 lines up: a button centres on its text */
    md_vdp_display(1);
    md_irq_on();
}

static void ui_init(void)
{
    s_row0 = md_is_pal() ? 1 : 0;
    md_irq_off();
    md_vdp_display(0);
    md_font_upload();
    md_irq_on();
    make_tiles();
    set_palettes();
    palette_show(0);
    screen_clear();
}

/* ---------------------------------------------------------------- input */

/* Presses, with the d-pad repeating while held (after 18 frames, then every 5). Either
 * pad drives the menus. */
#define K_UP    PAD_UP
#define K_DOWN  PAD_DOWN
#define K_LEFT  PAD_LEFT
#define K_RIGHT PAD_RIGHT
#define K_OK    (PAD_A | PAD_C | PAD_START)
#define K_BACK  PAD_B

static uint16_t s_held, s_holdTime;

static uint16_t keys(void)
{
    md_wait_vblank();
    md_pad_poll();
    uint16_t now = md_pad[0] | md_pad[1];
    uint16_t edge = md_pad_edge[0] | md_pad_edge[1];
    uint16_t dirs = now & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT);
    if (dirs && dirs == s_held) {
        if (++s_holdTime >= 18) { s_holdTime = 13; edge |= dirs; }
    } else {
        s_held = dirs;
        s_holdTime = 0;
    }
    return edge;
}

/* ---------------------------------------------------------------- teams */

static uint8_t country_of(uint16_t n) { return md_rom_team(n)->team.countryNumber; }

static uint16_t group_first(uint16_t n)
{
    uint8_t c = country_of(n);
    while (n > 0 && country_of((uint16_t)(n - 1)) == c) n--;
    return n;
}

static uint16_t group_next(uint16_t n)          /* first team of the next country */
{
    uint8_t c = country_of(n);
    do { n = (uint16_t)(n + 1 >= md_numTeams ? 0 : n + 1); } while (country_of(n) == c && n != 0);
    return group_first(n);
}

static uint16_t group_prev(uint16_t n)
{
    n = group_first(n);
    n = (uint16_t)(n == 0 ? md_numTeams - 1 : n - 1);
    return group_first(n);
}

static uint16_t team_step(uint16_t n, int dir)  /* the next team of the same country */
{
    uint8_t c = country_of(n);
    if (dir > 0) {
        uint16_t m = (uint16_t)(n + 1);
        return m < md_numTeams && country_of(m) == c ? m : group_first(n);
    }
    if (n > 0 && country_of((uint16_t)(n - 1)) == c) return (uint16_t)(n - 1);
    while (n + 1 < md_numTeams && country_of((uint16_t)(n + 1)) == c) n++;
    return n;
}

static const char *country_name(uint8_t c)
{
    const char *p = (const char *)_countriesTable[c];
    return p ? p + 1 : "OTHER TEAMS";
}

static void team_name(char *out, uint16_t n)
{
    const int8_t *s = md_rom_team(n)->team.teamName;
    int i = 0;
    for (; i < 16 && s[i]; i++) out[i] = (char)s[i];
    out[i] = 0;
}

/* A team's kit, drawn 16x24 into six tiles: colour `c0` the shirt, c0+1 its stripes,
 * c0+2 the shorts, c0+3 the socks (the CRAM entries set below). */
static const char *const kKitArt[24] = {
    "....bbb..bbb....", "..bbbbbbbbbbbb..", ".bbbbbbbbbbbbbb.", "bbbbbbbbbbbbbbbb",
    "bbbbbbbbbbbbbbbb", "bbb.bbbbbbbb.bbb", "bb..bbbbbbbb..bb", "....bbbbbbbb....",
    "....bbbbbbbb....", "....bbbbbbbb....", "....bbbbbbbb....", "....bbbbbbbb....",
    "....ssssssssss..", "...sssssssssss..", "...sssssssssss..", "...ssss...ssss..",
    "...kkkk...kkkk..", "...kkkk...kkkk..", "...kkkk...kkkk..", "...kkkk...kkkk..",
    "...kkkk...kkkk..", "...xxxxx..xxxxx.", "................", "................",
};

static void kit_draw(int side, uint16_t n)
{
    const TeamFile *t = &md_rom_team(n)->team;
    uint8_t c0 = (uint8_t)(8 + side * 4);
    uint8_t ids[4] = { t->prBasicColor, t->prStripesColor, t->prShortsColor, t->prSocksColor };
    for (int i = 0; i < 4; i++) {
        uint16_t col = vdp_color(_gamePalette[_shirtColorTable[ids[i] % 10] & 15]);
        for (int l = 0; l < 4; l++) s_pal[l * 16 + c0 + i] = col;
    }
    palette_show(8);
    for (int ty = 0; ty < 3; ty++)
        for (int tx = 0; tx < 2; tx++) {
            uint32_t rows[8];
            for (int y = 0; y < 8; y++) {
                uint32_t r = 0;
                int py = ty * 8 + y;
                for (int x = 0; x < 8; x++) {
                    int px = tx * 8 + x;
                    uint32_t c = 0;
                    switch (kKitArt[py][px]) {
                    case 'b': {
                        int stripe = 0;
                        if (t->prShirtType == 1) stripe = (px >> 1) & 1;               /* stripes */
                        else if (t->prShirtType == 2) stripe = px < 4 || px > 11;      /* sleeves */
                        else if (t->prShirtType == 3) stripe = (py >> 1) & 1;          /* hoops */
                        c = (uint32_t)(c0 + stripe);
                        break;
                    }
                    case 's': c = (uint32_t)(c0 + 2); break;
                    case 'k': c = (uint32_t)(c0 + 3); break;
                    case 'x': c = 3; break;
                    }
                    r = (r << 4) | c;
                }
                rows[y] = r;
            }
            put_tile((uint16_t)(TILE_KIT + side * 6 + ty * 2 + tx), rows);
        }
}

static void kit_place(int side, uint16_t x, uint16_t y)
{
    for (uint16_t ty = 0; ty < 3; ty++) {
        uint16_t w[2] = { TILE_ATTR(0, 0, 0, 0, TILE_KIT + side * 6 + ty * 2),
                          TILE_ATTR(0, 0, 0, 0, TILE_KIT + side * 6 + ty * 2 + 1) };
        cells(VRAM_PLANE_A, x, (uint16_t)(y + ty), w, 2);
    }
}

/* ---------------------------------------------------------------- screens */

static const char *const kControl[3] = { "COMPUTER", "PLAYER 1", "PLAYER 2" };

static void draw_build_line(uint16_t y)
{
    char s[40];
    md_snprintf(s, sizeof s, "BUILD %s  %s", Build_Id(), Build_Date());
    text_c(0, 40, y, PAL_DIM, s);
}

/* The title. Returns 1 for START, 0 when it timed out (the attract-mode demo). */
static int title_screen(void)
{
    screen_clear();
    big_c(4, PAL_TEXT, "SENSIBLE");
    big_c(7, PAL_TEXT, "WORLD OF SOCCER");
    text_c(0, 40, 11, PAL_LBL, "OPENSWOS  -  MEGA DRIVE");
    text_c(0, 40, 21, PAL_LBL, "THE AMIGA GAME'S OWN MATCH ENGINE");
    draw_build_line(26);
    fade_in();
    for (uint16_t t = 0;; t++) {
        uint16_t k = keys();
        if (k & (K_OK | PAD_B)) { fade_out(); return 1; }
        if ((t & 31) == 0) text_c(0, 40, 16, PAL_SEL, "PRESS START");
        if ((t & 31) == 20) clear_row(16);
        if (t >= 1200) { fade_out(); return 0; }
    }
}

static void controls_screen(void)
{
    static const char *const lines[] = {
        "D-PAD          RUN",
        "A, B OR C      PASS, SHOOT, TACKLE",
        "               (THE AMIGA'S FIRE)",
        "START          PAUSE",
        "X              INSTANT REPLAY",
        "Y              MATCH STATISTICS",
        "MODE, OR       LEAVE THE MATCH",
        "START + A",
        "",
        "A THREE-BUTTON PAD PLAYS THE MATCH;",
        "X, Y AND MODE NEED A SIX-BUTTON PAD.",
        0
    };
    screen_clear();
    big_c(2, PAL_TEXT, "CONTROLS");
    for (int i = 0; lines[i]; i++) text(2, (uint16_t)(7 + i), PAL_TEXT, lines[i]);
    item(14, 23, 12, 1, 0, "BACK");
    fade_in();
    for (;;) if (keys() & (K_OK | K_BACK)) break;
    fade_out();
}

static const char *const kLength[4] = { "3 MINS", "5 MINS", "7 MINS", "10 MINS" };
static const char *const kPitch[9] = { "SEASONAL", "RANDOM", "FROZEN", "MUDDY", "WET", "SOFT",
                                       "NORMAL", "DRY", "HARD" };
static const char *const kMonth[12] = { "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
                                        "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER",
                                        "DECEMBER" };

static int pitch_choice(void)       /* md_options.pitch as an index of kPitch */
{
    if (md_options.pitch == MD_PITCH_SEASONAL) return 0;
    if (md_options.pitch == MD_PITCH_RANDOM) return 1;
    return md_options.pitch + 2;
}

static void options_screen(void)
{
    enum { O_LENGTH, O_PITCH, O_MONTH, O_REPLAYS, O_SOUND, O_SPEED, O_BACK, O_N };
    static const char *const labels[O_N] = { "GAME LENGTH", "PITCH", "MONTH", "AUTO REPLAYS",
                                             "SOUND", "GAME SPEED", 0 };
    int sel = 0;
    screen_clear();
    big_c(2, PAL_TEXT, "OPTIONS");
    for (int dirty = 1;;) {
        if (dirty) {
            for (int i = 0; i < O_N; i++) {
                uint16_t y = (uint16_t)(6 + i * 3);
                const char *v = "";
                int dim = 0;
                switch (i) {
                case O_LENGTH:  v = kLength[_gameLength & 3]; break;
                case O_PITCH:   v = kPitch[pitch_choice()]; break;
                case O_MONTH:   v = kMonth[md_options.month % 12]; dim = md_options.pitch != MD_PITCH_SEASONAL; break;
                case O_REPLAYS: v = _autoReplays ? "ON" : "OFF"; break;
                case O_SOUND:   v = md_soundOn ? "ON" : "OFF"; break;
                case O_SPEED:   v = md_is_pal() ? "50 HZ" : md_clock60 ? "60 HZ (FAST)" : "50 HZ (AMIGA)";
                                dim = md_is_pal(); break;
                }
                if (i == O_BACK) { item(14, y, 12, sel == i, 0, "BACK"); continue; }
                cell_fill(VRAM_PLANE_A, 1, y, 0, 15);
                text(2, y, dim ? PAL_DIM : PAL_LBL, labels[i]);
                item(17, y, 21, sel == i, 1, v);
            }
            if (sel == O_SPEED && !md_is_pal())
                text_c(0, 40, 27, PAL_DIM, "50 HZ PLAYS AT THE AMIGA'S SPEED");
            else
                clear_row(27);
            dirty = 0;
        }
        uint16_t k = keys();
        int d = (k & K_RIGHT) ? 1 : (k & K_LEFT) ? -1 : 0;
        if (k & K_UP)   { sel = (sel + O_N - 1) % O_N; dirty = 1; }
        if (k & K_DOWN) { sel = (sel + 1) % O_N; dirty = 1; }
        if (k & K_BACK) break;
        if ((k & K_OK) && sel == O_BACK) break;
        if ((k & K_OK) && sel != O_BACK) d = 1;
        if (!d) continue;
        dirty = 1;
        switch (sel) {
        case O_LENGTH:  _gameLength = (uint16_t)((_gameLength + 4 + d) & 3); break;
        case O_PITCH: {
            int c = (pitch_choice() + 9 + d) % 9;
            md_options.pitch = c == 0 ? MD_PITCH_SEASONAL : c == 1 ? MD_PITCH_RANDOM : (uint16_t)(c - 2);
            break;
        }
        case O_MONTH:   md_options.month = (uint8_t)((md_options.month + 12 + d) % 12); break;
        case O_REPLAYS: _autoReplays ^= 1; break;
        case O_SOUND:   md_soundOn ^= 1; break;
        case O_SPEED:   if (!md_is_pal()) md_clock60 ^= 1; break;
        }
    }
    fade_out();
}

/* The match result and its statistics, as the Amiga's MATCH STATS board lists them. */
static void result_screen(const MdFriendly *f)
{
    char a[17], b[17], s[24];
    const TeamStatisticsData *s1 = &_team1StatsData, *s2 = &_team2StatsData;
    screen_clear();
    big_c(2, PAL_TEXT, _gameCanceled || md_abandoned ? "ABANDONED" : "FULL TIME");
    team_name(a, f->teamA);
    team_name(b, f->teamB);
    text_c(0, 18, 7, PAL_SEL, a);
    text_c(22, 18, 7, PAL_SEL, b);
    md_snprintf(s, sizeof s, "%u-%u", _team1TotalGoals, _team2TotalGoals);
    big_c(9, PAL_TEXT, s);
    if (_team1PenaltyGoals || _team2PenaltyGoals) {
        md_snprintf(s, sizeof s, "%u-%u ON PENALTIES", _team1PenaltyGoals, _team2PenaltyGoals);
        text_c(0, 40, 12, PAL_LBL, s);
    }
    kit_draw(0, f->teamA);
    kit_draw(1, f->teamB);
    kit_place(0, 8, 9);
    kit_place(1, 30, 9);
    {
        static const char *const rows[] = { "POSSESSION", "GOAL ATTEMPTS", "ON TARGET",
                                            "CORNERS WON", "FOULS CONCEDED", "BOOKINGS",
                                            "SENDINGS OFF" };
        uint16_t tot = (uint16_t)(s1->ballPossession + s2->ballPossession);
        uint16_t p1 = tot ? (uint16_t)((uint32_t)(uint16_t)s1->ballPossession * 100u / tot) : 0;
        int16_t v1[7] = { (int16_t)p1, s1->goalAttempts, s1->onTarget, s1->cornersWon,
                          s1->foulsConceded, s1->bookings, s1->sendingsOff };
        int16_t v2[7] = { (int16_t)(tot ? 100 - p1 : 0), s2->goalAttempts, s2->onTarget,
                          s2->cornersWon, s2->foulsConceded, s2->bookings, s2->sendingsOff };
        for (int i = 0; i < 7; i++) {
            uint16_t y = (uint16_t)(14 + i);
            text_c(10, 20, y, PAL_LBL, rows[i]);
            if (i == 0 && !tot) { text_c(0, 10, y, PAL_TEXT, "-"); text_c(30, 10, y, PAL_TEXT, "-"); continue; }
            md_snprintf(s, sizeof s, i == 0 ? "%d%%" : "%d", v1[i]);
            text_c(0, 10, y, PAL_TEXT, s);
            md_snprintf(s, sizeof s, i == 0 ? "%d%%" : "%d", v2[i]);
            text_c(30, 10, y, PAL_TEXT, s);
        }
    }
    item(12, 24, 16, 1, 0, "CONTINUE");
    fade_in();
    for (;;) if (keys() & (K_OK | K_BACK)) break;
    fade_out();
}

static void play(MdFriendly *f)
{
    md_wait_vblank();
    palette_show(0);
    md_play_friendly(f);
    ui_init();
}

/* FRIENDLY: two columns, one a team; then PLAY MATCH. Returns when BACK is chosen. */
static MdFriendly s_friendly = { 0, 1, 1, 0 };

static void friendly_screen(void)
{
    enum { F_COUNTRY1, F_TEAM1, F_CTRL1, F_COUNTRY2, F_TEAM2, F_CTRL2, F_PLAY, F_BACK, F_N };
    MdFriendly *f = &s_friendly;
    int sel = F_PLAY, kitDirty = 3;
    if (f->teamA >= md_numTeams) f->teamA = 0;
    if (f->teamB >= md_numTeams) f->teamB = md_numTeams > 1 ? 1 : 0;
    screen_clear();
    big_c(1, PAL_TEXT, "FRIENDLY");
    text_c(0, 20, 5, PAL_LBL, "TEAM 1");
    text_c(20, 20, 5, PAL_LBL, "TEAM 2");
    text_c(18, 4, 18, PAL_TEXT, "V");
    fade_in();
    for (int dirty = 1;;) {
        if (kitDirty) {
            if (kitDirty & 1) kit_draw(0, f->teamA);
            if (kitDirty & 2) kit_draw(1, f->teamB);
            kit_place(0, 9, 16);
            kit_place(1, 29, 16);
            kitDirty = 0;
        }
        if (dirty) {
            char n[17];
            for (int side = 0; side < 2; side++) {
                uint16_t x = (uint16_t)(side * 20), team = side ? f->teamB : f->teamA;
                uint8_t ctrl = side ? f->ctrlB : f->ctrlA;
                int base = side * 3;
                item(x, 7, 20, sel == base + 0, 1, country_name(country_of(team)));
                team_name(n, team);
                item(x, 10, 20, sel == base + 1, 1, n);
                item(x, 13, 20, sel == base + 2, 1, kControl[ctrl]);
            }
            item(10, 22, 20, sel == F_PLAY, 0, "PLAY MATCH");
            item(14, 25, 12, sel == F_BACK, 0, "BACK");
            dirty = 0;
        }
        uint16_t k = keys();
        int d = (k & K_RIGHT) ? 1 : (k & K_LEFT) ? -1 : 0;
        if (k & K_UP)   { sel = (sel + F_N - 1) % F_N; dirty = 1; }
        if (k & K_DOWN) { sel = (sel + 1) % F_N; dirty = 1; }
        if (k & K_BACK) break;
        if (k & K_OK) {
            if (sel == F_BACK) break;
            if (sel == F_PLAY) {
                fade_out();
                play(f);
                result_screen(f);
                screen_clear();
                big_c(1, PAL_TEXT, "FRIENDLY");
                text_c(0, 20, 5, PAL_LBL, "TEAM 1");
                text_c(20, 20, 5, PAL_LBL, "TEAM 2");
                text_c(18, 4, 18, PAL_TEXT, "V");
                dirty = 1; kitDirty = 3;
                fade_in();
                continue;
            }
            d = 1;
        }
        if (!d || sel >= F_PLAY) continue;
        dirty = 1;
        {
            int side = sel / 3, what = sel % 3;
            uint16_t *team = side ? &f->teamB : &f->teamA;
            uint8_t *ctrl = side ? &f->ctrlB : &f->ctrlA, other = side ? f->ctrlA : f->ctrlB;
            if (what == 0) *team = d > 0 ? group_next(*team) : group_prev(*team);
            if (what == 1) *team = team_step(*team, d);
            if (what == 2) {                    /* a pad plays one side at most */
                do { *ctrl = (uint8_t)((*ctrl + 3 + d) % 3); } while (*ctrl && *ctrl == other);
            }
            if (what < 2) kitDirty |= 1 << side;
        }
    }
    fade_out();
}

/* The attract mode: two teams the computer plays, until a button. */
static void demo_match(void)
{
    MdFriendly f;
    uint16_t r = md_vblanks;
    f.teamA = (uint16_t)(r % md_numTeams);
    f.teamB = (uint16_t)((r / 7 + 1 + f.teamA) % md_numTeams);
    if (f.teamB == f.teamA) f.teamB = (uint16_t)((f.teamA + 1) % md_numTeams);
    f.ctrlA = f.ctrlB = 0;
    md_demo = 1;
    play(&f);
    md_demo = 0;
}

static int main_menu(void)
{
    enum { M_FRIENDLY, M_DEMO, M_OPTIONS, M_CONTROLS, M_N };
    static const char *const names[M_N] = { "FRIENDLY", "WATCH A MATCH", "OPTIONS", "CONTROLS" };
    static int sel;
    screen_clear();
    big_c(2, PAL_TEXT, "MAIN MENU");
    draw_build_line(26);
    fade_in();
    for (int dirty = 1, idle = 0;; idle++) {
        if (dirty) {
            for (int i = 0; i < M_N; i++) item(10, (uint16_t)(8 + i * 4), 20, sel == i, 0, names[i]);
            dirty = 0;
        }
        uint16_t k = keys();
        if (k) idle = 0;
        if (k & K_UP)   { sel = (sel + M_N - 1) % M_N; dirty = 1; }
        if (k & K_DOWN) { sel = (sel + 1) % M_N; dirty = 1; }
        if (k & K_BACK) { fade_out(); return 0; }
        if (k & K_OK) { fade_out(); return sel + 1; }
        if (idle >= 1800) { fade_out(); return M_DEMO + 1; }
    }
}

void md_frontend(void)
{
    ui_init();
#ifdef MD_TEST_HUMAN
    /* scripts/md tests: straight into a match, pad 1 the first team */
    { MdFriendly f = { 0, 1, 1, 0 }; for (;;) { play(&f); } }
#endif
#ifdef MD_TEST_DEMO
    for (;;) { MdFriendly f = { 0, 1, 0, 0 }; play(&f); }
#endif
    for (;;) {
        if (!title_screen()) { demo_match(); continue; }
        for (int m; (m = main_menu()) != 0;) {
            if (m == 1) friendly_screen();
            else if (m == 2) demo_match();
            else if (m == 3) options_screen();
            else if (m == 4) controls_screen();
        }
    }
}
