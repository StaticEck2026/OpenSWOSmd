/* md_render.c -- the match on the VDP: the pitch plane, the sprites, the HUD, the palettes.
 *
 * The engine draws by calling DrawSprites (md_display.c), which leaves md_sprList: every
 * sprite on screen this frame, front to back, relative to the simulation camera's
 * 336x272 window. At the end of the frame (MD_FrameDone, md_isr.c) md_render_frame turns
 * that into VDP work and stages it; at the VBlank that shows the frame -- the same one the
 * Amiga swaps its buffers on -- md_render_commit queues it, and the VBlank's DMA runs it.
 *
 * The screen. The Mega Drive shows 320x224 (NTSC) or 320x240 (PAL) of the engine's
 * 336x272 window, centred: 8 columns off each side and 24 (NTSC) or 16 (PAL) lines off
 * the top and the bottom. The engine still decides everything -- who is on screen, which
 * player the AI may pick -- against its own 336x272 (DrawSprites' cull), so the match
 * plays as it does on the Amiga; the picture is a crop of it.
 *
 * The pitch. 42x55 Amiga tiles of 16x16, i.e. 84x110 VDP cells; plane B is a 64x32-cell
 * ring over it. A camera move writes the column or row of cells it uncovers (at most a
 * couple a frame). Each Amiga tile is four VDP cells (md_pitch.cells), so a fan the crowd
 * animation toggles (animated_patterns.c) is four cells rewritten.
 *
 * Sprites. Each image is an MdImage: up to a few VDP sprites ("pieces") with their tiles.
 * Tiles live in a 512-tile VRAM cache and are uploaded by DMA from ROM when an image
 * first shows; images on screen frame after frame cost nothing. Kits: the player images
 * of team 1 are drawn with palette line 1, team 2's with line 2, each line the game
 * palette through that team's colour table -- exactly the remap the Amiga applies to the
 * pixels (color_sprites.c ConvertColorsAndTextSprites). The kit pattern (plain, sleeves,
 * hoops) picks the image set: md_kitImage[sheet].
 */
#include "md_hw.h"
#include "md_game.h"
#include "md_render.h"
#include "md_data.h"
#include "game_types.h"
#include "display.h"
#include "md_overlay.h"

extern FixedPoint _cameraX, _cameraY;
extern uint8_t _pitchDatBuffer[];
extern volatile uint16_t md_fadeLevel;
extern volatile uint8_t md_paletteDirty;
extern uint16_t _gamePalette[16];
extern uint16_t _team1PlayersColorTable[16], _team2PlayersColorTable[16];
extern uint16_t _colorTableReserves[16];
extern uint16_t _pitchNumberAndType;
extern TeamGame _topTeamInGame, _bottomTeamInGame;
extern uint8_t _gameTime[4];
extern uint16_t _statsTeam1Goals, _statsTeam2Goals;
extern void *_teamFilenamesTable[];
extern uintptr_t _team1Filename, _team2Filename;

#define TILE_SIZE_AMIGA 128     /* a 16x16 tile at 4 bitplanes: the slot unit */
#define PITCH_COLS 42
#define PITCH_ROWS 55
#define HUD_Z_MIN  8000

static uint8_t s_active;        /* a match is on screen */
static int16_t s_offY;          /* simulation window -> screen: 24 NTSC, 16 PAL */
static int16_t s_screenH;
static const MdPitch *s_pitch;
static uint8_t s_kit1, s_kit2;  /* kit sheet (0..2) of each team */

/* ---- staged frames (written by md_render_frame, queued by md_render_commit) ----
 * Two: the game may finish the next frame before this one has been shown (md_isr.c). */
typedef struct { uint16_t y, sizeLink, attr, x; } VdpSprite;
#define UPLOAD_MAX 40
typedef struct { const void *src; uint16_t vram, bytes; } Upload;
/* plane writes: runs of nametable words, contiguous (inc 2) or a column (inc 128) */
#define PLANE_JOBS 40
#define PLANE_WORDS 400
typedef struct { uint16_t vram; uint8_t count, inc; uint16_t first; } PlaneJob;
typedef struct {
    VdpSprite sat[80];
    uint8_t   satCount, upCount, pjCount, palStaged;
    uint16_t  hscroll, vscroll, pwCount;
    Upload    up[UPLOAD_MAX];
    PlaneJob  pj[PLANE_JOBS];
    uint16_t  pw[PLANE_WORDS];
    uint16_t  pal[64];
} Stage;
static Stage s_stage[2];
static Stage *s_w;              /* the frame being staged */
static uint8_t s_wIdx, s_rIdx;  /* next to stage, next to show */

/* crowd tiles the engine toggled this frame (md_pitch_cell_changed), drawn when staged */
#define FAN_MAX 24
static uint16_t s_fan[FAN_MAX];
static uint8_t s_fanCount;

static int plane_job(uint16_t vram, uint8_t inc, const uint16_t *words, uint8_t n)
{
    Stage *st = s_w;
    if (st->pjCount >= PLANE_JOBS || st->pwCount + n > PLANE_WORDS) return 0;
    PlaneJob *j = &st->pj[st->pjCount++];
    j->vram = vram; j->count = n; j->inc = inc; j->first = st->pwCount;
    for (uint8_t i = 0; i < n; i++) st->pw[st->pwCount++] = words[i];
    return 1;
}

/* ---- palettes ---- */

/* Amiga 12-bit 0RGB -> VDP 9-bit 0BGR (top three bits of each channel), faded the way
 * the Amiga's ApplyFadeStep does: level/32 steps of a shift (fade.c's shift/mask table). */
static uint16_t md_color(uint16_t amiga, uint16_t level)
{
    uint16_t r = (amiga >> 8) & 15, g = (amiga >> 4) & 15, b = amiga & 15;
    if (level < 0x80) {
        uint16_t sh = (uint16_t)(4 - (level >> 5));
        r >>= sh; g >>= sh; b >>= sh;
    }
    return (uint16_t)(((b >> 1) << 9) | ((g >> 1) << 5) | ((r >> 1) << 1));
}

/* Rebuilt at kick-off and on each fade step (ApplyFadeStep sets md_paletteDirty): the
 * game palette and the colour tables are set before the match's first frame and stay. */
static void build_palettes(uint16_t *pal)
{
    uint16_t lvl = md_fadeLevel;
    for (int i = 0; i < 16; i++) {
        pal[i]      = md_color(_gamePalette[i], lvl);
        pal[16 + i] = md_color(_gamePalette[_team1PlayersColorTable[i] & 15], lvl);
        pal[32 + i] = md_color(_gamePalette[_team2PlayersColorTable[i] & 15], lvl);
        pal[48 + i] = md_color(_gamePalette[_colorTableReserves[i] & 15], lvl);
    }
}

/* ---- the pitch ---- */
static int16_t s_cx0, s_cy0;            /* top-left loaded cell */
#define LOAD_W 41                       /* cells kept loaded across and down */
static int16_t s_loadH;

/* The 16x16 tile at a map slot: the engine's slot for the crowd rows it animates
 * (animated_patterns.c keeps rows 1-3 and 51-53 in _pitchDatBuffer, as tile offsets),
 * the ROM map everywhere else. */
static inline uint16_t slot_tile(uint16_t slot)
{
    if (slot >= 42 && slot < 42 + 126)
        return (uint16_t)(((const uint32_t *)_pitchDatBuffer)[slot - 42] / TILE_SIZE_AMIGA);
    if (slot >= 2142 && slot < 2142 + 126)
        return (uint16_t)(((const uint32_t *)_pitchDatBuffer)[126 + slot - 2142] / TILE_SIZE_AMIGA);
    return s_pitch->map[slot];
}

/* LoadPitchMap (md_assets.c): the engine picked pitch n; its crowd slots as the Amiga's
 * FixupAndColorPitchFile leaves them, every fan still. */
void md_pitch_select(uint16_t n)
{
    s_pitch = &md_pitches[n <= 5 ? n : 0];
    uint32_t *p = (uint32_t *)_pitchDatBuffer;
    for (uint16_t i = 0; i < 126; i++) {
        p[i] = (uint32_t)s_pitch->map[42 + i] * TILE_SIZE_AMIGA;
        p[126 + i] = (uint32_t)s_pitch->map[2142 + i] * TILE_SIZE_AMIGA;
    }
}

static inline uint16_t cell_entry(int16_t cx, int16_t cy)
{
    if (cx < 0 || cy < 0 || cx >= PITCH_COLS * 2 || cy >= PITCH_ROWS * 2) return 0;
    uint16_t slot = (uint16_t)((cy >> 1) * PITCH_COLS + (cx >> 1));
    uint16_t t16 = slot_tile(slot);
    if (t16 >= s_pitch->nTiles16) t16 = 0;
    return s_pitch->cells[t16 * 4 + (cx & 1) + ((cy & 1) << 1)];
}

/* The nametable words of `n` cells of one cell row (cy) from column cx0, a 16x16 tile at a
 * time: each tile gives two cells of the row. */
static void row_cells(uint16_t *w, int16_t cx0, int16_t cy, int16_t n)
{
    int16_t cx = cx0, end = (int16_t)(cx0 + n);
    if (cy < 0 || cy >= PITCH_ROWS * 2) { for (int16_t i = 0; i < n; i++) w[i] = 0; return; }
    uint16_t rowSlot = (uint16_t)((cy >> 1) * PITCH_COLS);
    uint16_t half = (uint16_t)((cy & 1) << 1);
    const uint16_t *cells = s_pitch->cells;
    uint16_t nt = s_pitch->nTiles16;
    while (cx < end) {
        uint16_t e;
        if (cx < 0 || cx >= PITCH_COLS * 2) e = 0;
        else {
            uint16_t t16 = slot_tile((uint16_t)(rowSlot + (cx >> 1)));
            if (t16 >= nt) t16 = 0;
            e = cells[t16 * 4 + half + (cx & 1)];
        }
        *w++ = e;
        cx++;
    }
}

static void stage_column(int16_t cx)
{
    uint16_t w[PLANE_H];
    int16_t pcx = cx & (PLANE_W - 1);
    /* the ring's rows from s_cy0: plane row (cy & 31); written in two runs if it wraps */
    int16_t r0 = s_cy0 & (PLANE_H - 1);
    uint8_t n1 = (uint8_t)(PLANE_H - r0 < s_loadH ? PLANE_H - r0 : s_loadH);
    for (int16_t i = 0; i < s_loadH; i++) w[i] = cell_entry(cx, s_cy0 + i);
    plane_job((uint16_t)(VRAM_PLANE_B + (r0 * PLANE_W + pcx) * 2), 128, w, n1);
    if (n1 < s_loadH)
        plane_job((uint16_t)(VRAM_PLANE_B + pcx * 2), 128, w + n1, (uint8_t)(s_loadH - n1));
}

static void stage_row(int16_t cy)
{
    uint16_t w[LOAD_W];
    int16_t prow = cy & (PLANE_H - 1);
    int16_t c0 = s_cx0 & (PLANE_W - 1);
    uint8_t n1 = (uint8_t)(PLANE_W - c0 < LOAD_W ? PLANE_W - c0 : LOAD_W);
    row_cells(w, s_cx0, cy, LOAD_W);
    plane_job((uint16_t)(VRAM_PLANE_B + (prow * PLANE_W + c0) * 2), 2, w, n1);
    if (n1 < LOAD_W)
        plane_job((uint16_t)(VRAM_PLANE_B + prow * PLANE_W * 2), 2, w + n1, (uint8_t)(LOAD_W - n1));
}

/* animated_patterns.c toggled a fan: noted, and its four cells rewritten when the frame
 * is staged (stage_fans) */
void md_pitch_cell_changed(uint16_t cell)
{
    if (!s_active || s_fanCount >= FAN_MAX) return;
    s_fan[s_fanCount++] = cell;
}

static void stage_fans(void)
{
    for (uint8_t f = 0; f < s_fanCount; f++) {
        uint16_t cell = s_fan[f];
        int16_t cx = (int16_t)((cell % PITCH_COLS) * 2), cy = (int16_t)((cell / PITCH_COLS) * 2);
        if (cx < s_cx0 || cx + 1 >= s_cx0 + LOAD_W) continue;
        for (int16_t dy = 0; dy < 2; dy++) {
            int16_t y = cy + dy;
            if (y < s_cy0 || y >= s_cy0 + s_loadH) continue;
            uint16_t e[2];
            e[0] = cell_entry(cx, y);
            e[1] = cell_entry(cx + 1, y);
            if ((cx & (PLANE_W - 1)) == PLANE_W - 1) {     /* the pair straddles the ring's edge */
                plane_job((uint16_t)(VRAM_PLANE_B + ((y & (PLANE_H - 1)) * PLANE_W + PLANE_W - 1) * 2), 2, &e[0], 1);
                plane_job((uint16_t)(VRAM_PLANE_B + ((y & (PLANE_H - 1)) * PLANE_W) * 2), 2, &e[1], 1);
            } else {
                plane_job((uint16_t)(VRAM_PLANE_B + ((y & (PLANE_H - 1)) * PLANE_W + (cx & (PLANE_W - 1))) * 2), 2, e, 2);
            }
        }
    }
    s_fanCount = 0;
}

static void pitch_full_load(int16_t rx, int16_t ry)
{
    /* display off, interrupts off: the whole visible ring written by the CPU */
    s_cx0 = rx >> 3;
    s_cy0 = ry >> 3;
    for (int16_t i = 0; i < s_loadH; i++) {
        int16_t cy = s_cy0 + i;
        for (int16_t j = 0; j < LOAD_W; j++) {
            int16_t cx = s_cx0 + j;
            md_set_nametable_cell(VRAM_PLANE_B, PLANE_W, (uint16_t)(cx & (PLANE_W - 1)),
                                  (uint16_t)(cy & (PLANE_H - 1)), cell_entry(cx, cy));
        }
    }
}

static void pitch_scroll(int16_t rx, int16_t ry)
{
    int16_t ncx = rx >> 3, ncy = ry >> 3;
    while (s_cx0 < ncx) { s_cx0++; stage_column(s_cx0 + LOAD_W - 1); }
    while (s_cx0 > ncx) { s_cx0--; stage_column(s_cx0); }
    while (s_cy0 < ncy) { s_cy0++; stage_row(s_cy0 + s_loadH - 1); }
    while (s_cy0 > ncy) { s_cy0--; stage_row(s_cy0); }
}

/* ---- the sprite tile cache ----
 * By image index: within a match an index always draws the same image (the kit sheets are
 * fixed at kick-off), except the advert boards, whose window moves -- an entry remembers
 * which image it holds, and a different one is uploaded over it. */
#define CACHE_SLOTS 96
#define UNIT 4                              /* tiles per allocation unit */
#define UNITS (TILE_DYN_COUNT / UNIT)       /* 128 */
typedef struct { const MdImage *img; uint16_t idx, lastFrame; uint8_t unit, units; uint16_t attrBase; } CacheEnt;
static CacheEnt s_cache[CACHE_SLOTS];
static uint8_t s_imgEnt[MD_NUM_IMAGES];     /* cache entry + 1 holding the image, 0 none */
static uint8_t s_unitUsed[UNITS];           /* 1: the unit is taken */
static uint8_t s_cacheUsed;                 /* entries in use */
static uint16_t s_frame;
static uint16_t s_dmaBudget;

static void cache_reset(void)
{
    for (int i = 0; i < CACHE_SLOTS; i++) s_cache[i].img = 0;
    for (int i = 0; i < MD_NUM_IMAGES; i++) s_imgEnt[i] = 0;
    for (int i = 0; i < UNITS; i++) s_unitUsed[i] = 0;
    s_cacheUsed = 0;
}

static void cache_free(uint8_t e)
{
    CacheEnt *c = &s_cache[e];
    for (uint8_t u = 0; u < c->units; u++) s_unitUsed[c->unit + u] = 0;
    s_imgEnt[c->idx] = 0;
    c->img = 0;
    s_cacheUsed--;
}

static int16_t find_units(uint8_t need)
{
    uint8_t run = 0;
    for (int16_t u = 0; u < UNITS; u++) {
        run = s_unitUsed[u] ? 0 : (uint8_t)(run + 1);
        if (run == need) return (int16_t)(u - need + 1);
    }
    return -1;
}

static void upload(const MdImage *img, uint8_t unit)
{
    uint16_t bytes = (uint16_t)(img->nTiles * 32);
    Upload *u = &s_w->up[s_w->upCount++];
    u->src = MD_IMAGE_TILES(img);
    u->vram = (uint16_t)((TILE_DYN_SPRITES + unit * UNIT) * 32);
    u->bytes = bytes;
    s_dmaBudget -= bytes;
}

/* Image `idx` (whose current picture is `img`): its first VRAM tile, uploaded if needed;
 * -1 when there is no room or no DMA left this frame (drawn next frame instead). */
static int16_t cache_get(uint16_t idx, const MdImage *img)
{
    uint8_t e = s_imgEnt[idx];
    uint16_t bytes = (uint16_t)(img->nTiles * 32);
    if (e) {
        CacheEnt *c = &s_cache[e - 1];
        c->lastFrame = s_frame;
        if (c->img == img) return (int16_t)(TILE_DYN_SPRITES + c->unit * UNIT);
        /* the same slot shows another picture now (an advert's window moved) */
        if ((uint8_t)((img->nTiles + UNIT - 1) / UNIT) == c->units && bytes <= s_dmaBudget && s_w->upCount < UPLOAD_MAX) {
            c->img = img;
            upload(img, c->unit);
            return (int16_t)(TILE_DYN_SPRITES + c->unit * UNIT);
        }
        cache_free((uint8_t)(e - 1));
    }
    if (bytes > s_dmaBudget || s_w->upCount >= UPLOAD_MAX) return -1;
    uint8_t need = (uint8_t)((img->nTiles + UNIT - 1) / UNIT);
    int16_t first = find_units(need);
    if (first < 0 || s_cacheUsed >= CACHE_SLOTS) {
        /* full: drop everything not drawn this frame */
        for (uint8_t k = 0; k < CACHE_SLOTS; k++)
            if (s_cache[k].img && s_cache[k].lastFrame != s_frame) cache_free(k);
        first = find_units(need);
        if (first < 0) return -1;
    }
    uint8_t k = 0;
    while (s_cache[k].img) k++;
    CacheEnt *c = &s_cache[k];
    c->img = img; c->idx = idx; c->lastFrame = s_frame; c->unit = (uint8_t)first; c->units = need;
    for (uint8_t v = 0; v < need; v++) s_unitUsed[first + v] = 1;
    s_imgEnt[idx] = (uint8_t)(k + 1);
    s_cacheUsed++;
    upload(img, (uint8_t)first);
    return (int16_t)(TILE_DYN_SPRITES + first * UNIT);
}

/* ---- images ---- */
extern uint8_t *_advert1Pointers[8], *_advert2Pointers[8], *_advert3Pointers[8];

static const MdImage *image_for(uint16_t idx, uint8_t *pal)
{
    const SpriteGraphics *g = md_spriteTable[idx];
    const MdImage *img = (const MdImage *)g->sprData;
    *pal = 0;
    if (idx >= MD_KIT_FIRST && idx < MD_KIT_LAST) {
        uint8_t team2 = idx >= 530;
        *pal = team2 ? 2 : 1;
        img = (const MdImage *)md_kitImage[team2 ? s_kit2 : s_kit1][idx - MD_KIT_FIRST];
    } else if (idx >= 833 && idx < 1065) {
        *pal = idx >= 949 ? 2 : 1;
    } else if (idx >= 1196 && idx < 1220) {
        *pal = 3;
    } else if (idx >= MD_ADVERT_FIRST && idx < MD_ADVERT_FIRST + MD_ADVERT_COUNT) {
        /* adverts.c scrolls the board by moving sprData a line at a time down its own
         * image (from the base CopyAdvertisementsData recorded): the line it is at picks
         * the precut window. */
        uint16_t k = (uint16_t)(idx - MD_ADVERT_FIRST);
        const uint8_t *base = (k < 8 ? _advert1Pointers : k < 16 ? _advert2Pointers : _advert3Pointers)[k & 7];
        int32_t line = base ? ((const uint8_t *)g->sprData - base) / (g->wquads * 8) : 0;
        if (line < 0 || line >= MD_ADVERT_LINES) line = 0;
        if (g->nlines == 6) img = (const MdImage *)md_advertImage[k][line];
    }
    return img;
}

/* ---- the HUD: the engine's HUD sprites, as text on plane A (md_overlay.c) ---- */
#define HUD_ROWS 2
static uint8_t s_hudRowY[HUD_ROWS];
static uint32_t s_hudSig;       /* the HUD on screen (md_render_frame) */

static void hud_put(int row, int col, const char *t)
{
    md_ov_put((int16_t)col, s_hudRowY[row], t, 40);
}

static int str_copy(char *d, const int8_t *s, int max)
{
    int n = 0;
    while (n < max && s[n]) { d[n] = (char)s[n]; n++; }
    d[n] = 0;
    return n;
}

static void hud_sprite(const MdSprCmd *c)
{
    uint16_t img = c->image;
    int16_t sx = c->x - 8, sy = c->y - s_offY;
    int row = sy < s_screenH / 2 ? 0 : 1;
    int col = sx / 8;
    char buf[24];
    if (img >= 0x8D && img < 0x8D + 32) {               /* a player's name: number + name */
        const TeamGame *t = img < 0x9D ? &_topTeamInGame : &_bottomTeamInGame;
        const PlayerInfo *p = &t->players[(img - 0x8D) & 15];
        int n = 0, num = p->shirtNumber;
        if (num >= 10) buf[n++] = (char)('0' + num / 10);
        buf[n++] = (char)('0' + num % 10);
        buf[n++] = ' ';
        str_copy(buf + n, p->shortName, 15);
        hud_put(row, col, buf);
    } else if (img >= 214 && img <= 216) {              /* the clock (game_state.c UpdateTime) */
        int n = 0;
        if (_gameTime[1]) buf[n++] = (char)('0' + _gameTime[1]);
        if (_gameTime[1] || _gameTime[2]) buf[n++] = (char)('0' + _gameTime[2]);
        buf[n++] = (char)('0' + _gameTime[3]);
        buf[n] = 0;
        str_copy(buf + n, (const int8_t *)" MINS", 5);
        hud_put(row, col, buf);
    } else if (img == 193 || img == 194) {              /* a team's name on the result */
        str_copy(buf, (img == 193 ? _topTeamInGame : _bottomTeamInGame).teamName, 16);
        hud_put(row, col, buf);
    } else if (img >= 173 && img < 193) {               /* the big result digits */
        buf[0] = (char)('0' + (img - 173) % 10); buf[1] = 0;
        hud_put(row, col, buf);
    } else if (img == 195) {
        hud_put(row, col, "-");
    }
}

/* ---- per match ---- */

void md_render_init_match(void)
{
    s_active = 0;                   /* the first frame does the full set-up */
}

static void match_setup(int16_t rx, int16_t ry)
{
    if (!s_pitch) md_pitch_select(0);
    s_offY = md_is_pal() ? 16 : 24;
    s_screenH = (int16_t)md_screen_h();
    s_loadH = (int16_t)(s_screenH / 8 + 1);
    s_hudRowY[0] = 1;
    s_hudRowY[1] = (uint8_t)(s_screenH / 8 - 2);

    /* kits: teamFilenamesTable (shirt type -> sheet) as ConvertColorsAndTextSprites set */
    s_kit1 = s_kit2 = 0;
    for (int k = 0; k < 4; k++) {
        if (_team1Filename == (uintptr_t)_teamFilenamesTable[k]) s_kit1 = (uint8_t)(k == 1 ? 1 : k == 3 ? 2 : 0);
        if (_team2Filename == (uintptr_t)_teamFilenamesTable[k]) s_kit2 = (uint8_t)(k == 1 ? 1 : k == 3 ? 2 : 0);
    }

    md_irq_off();
    md_vdp_display(0);
    md_vdp_set_reg(11, 0x00);                           /* full-screen scroll */
    md_vram_dma(TILE_PITCH * 32, s_pitch->tiles, (uint16_t)(s_pitch->nVdpTiles * 32));
    md_vram_fill(VRAM_PLANE_A, 0, PLANE_W * PLANE_H);
    md_vram_fill(VRAM_PLANE_B, 0, PLANE_W * PLANE_H);
    md_vram_fill(VRAM_SPRITES, 0, 4);
    md_font_upload();
    pitch_full_load(rx, ry);
    md_vram_fill(VRAM_HSCROLL, 0, 2);
    md_vsram_write(0, 0);
    md_vsram_write(1, 0);
    cache_reset();
    md_ov_reset(s_offY, s_screenH);
    s_hudSig = 0xFFFFFFFF;
    {
        uint16_t pal[64];
        build_palettes(pal);
        md_cram_write(0, pal, 64);
    }
    s_wIdx = s_rIdx = 0;
    s_w = &s_stage[0];
    s_w->pjCount = 0; s_w->pwCount = 0; s_w->upCount = 0;
    s_fanCount = 0;
    md_vdp_display(1);
    md_irq_on();
    s_active = 1;
}

void md_render_frame(void)
{
    int16_t rx = (int16_t)(_cameraX.s.i + 8);
    int16_t ry;
    if (!s_active) {
        s_offY = md_is_pal() ? 16 : 24;
        match_setup(rx, (int16_t)(_cameraY.s.i + s_offY));
    }
    ry = (int16_t)(_cameraY.s.i + s_offY);
    /* md_isr.c has made sure s_w is free (at most two frames queued) */
    s_w = &s_stage[s_wIdx];
    Stage *st = s_w;
    s_frame++;
    st->pjCount = 0;
    st->pwCount = 0;
    st->upCount = 0;
    st->palStaged = 0;
    s_dmaBudget = md_is_pal() ? 9000 : 5200;

    pitch_scroll(rx, ry);
    stage_fans();
    st->hscroll = (uint16_t)(-rx);
    st->vscroll = (uint16_t)ry;

    /* The HUD's text, then everything else the engine drew into the frame. A frame whose
     * HUD is the last one's (its sprites and the values their text is made of), with
     * nothing else drawn this frame or the one before, leaves plane A as it is. */
    {
        uint32_t sig = (uint32_t)_gameTime[3] | ((uint32_t)_gameTime[2] << 4) | ((uint32_t)_gameTime[1] << 8);
        for (uint16_t i = 0; i < md_sprCount; i++) {
            const MdSprCmd *c = &md_sprList[i];
            if (c->spr->z.s.i >= HUD_Z_MIN)
                sig = (sig << 5 | sig >> 27) ^ c->image ^ ((uint32_t)(c->x >> 3) << 11) ^ ((uint32_t)(c->y >> 6) << 20);
        }
        int hudChanged = sig != s_hudSig;
        if (hudChanged || md_ov_drawn()) {
            s_hudSig = sig;
            if (!md_ov_static())
                for (uint16_t i = 0; i < md_sprCount; i++)
                    if (md_sprList[i].spr->z.s.i >= HUD_Z_MIN) hud_sprite(&md_sprList[i]);
            md_ov_commit(hudChanged);
        }
    }

    /* sprites, front to back; the VDP links them in table order and shows the first on top */
    uint8_t n = 0;
    {
        const MdSprCmd *c = md_ovSprCount ? md_ovSpr : md_sprList;
        const MdSprCmd *end = md_ovSprCount ? md_ovSpr + md_ovSprCount : md_sprList + md_sprCount;
        VdpSprite *v = st->sat;
        int16_t ymax = (int16_t)(128 + s_screenH), offY = (int16_t)(s_offY - 128);
        uint16_t prio = md_ovSprCount ? 0x8000 : 0;   /* the overlay's over plane A's boxes */
        for (;; c++) {
            if (c == end) {
                if (end != md_ovSpr + md_ovSprCount || !md_ovSprCount) break;
                c = md_sprList; end = md_sprList + md_sprCount; prio = 0;
                if (c == end) break;
            }
            if (c->spr->z.s.i >= HUD_Z_MIN) continue;
            uint16_t idx = c->image;
            uint8_t e = s_imgEnt[idx];
            const MdImage *img;
            uint16_t attr;
            /* an image already resident: one lookup (an index draws one picture for the
             * whole match -- but an advert board's window moves, so those are re-checked) */
            if (e && (uint16_t)(idx - MD_ADVERT_FIRST) >= MD_ADVERT_COUNT) {
                CacheEnt *ce = &s_cache[e - 1];
                ce->lastFrame = s_frame;
                img = ce->img;
                attr = ce->attrBase;
            } else {
                uint8_t pal;
                img = image_for(idx, &pal);
                if (!img->nPieces) continue;
                int16_t base = cache_get(idx, img);
                if (base < 0) continue;
                attr = TILE_ATTR(pal, 0, 0, 0, base);
                s_cache[s_imgEnt[idx] - 1].attrBase = attr;
            }
            int16_t sx = (int16_t)(c->x + 120), sy = (int16_t)(c->y - offY);
            const MdPiece *pc = img->pieces, *pend = pc + img->nPieces;
            for (; pc < pend; pc++) {
                int16_t x = (int16_t)(sx + pc->dx), y = (int16_t)(sy + pc->dy);
                if (x <= 96 || x >= 128 + 320 || y <= 96 || y >= ymax) continue;
                if (n >= 80) goto sat_full;
                n++;
                v->y = (uint16_t)y;
                v->sizeLink = (uint16_t)((pc->size << 8) | n);
                v->attr = (uint16_t)((attr + pc->tile) | prio);
                v->x = (uint16_t)x;
                v++;
            }
        }
    }
sat_full:
    if (n == 0) {                    /* an empty table is one sprite off screen */
        st->sat[0].y = 0; st->sat[0].sizeLink = 0; st->sat[0].attr = 0; st->sat[0].x = 0;
        n = 1;
    }
    st->sat[n - 1].sizeLink &= 0xFF00;   /* end of the link chain */
    st->satCount = n;

    md_ovSprCount = 0;
    if (md_paletteDirty) { md_paletteDirty = 0; build_palettes(st->pal); st->palStaged = 1; }
    s_wIdx ^= 1;
}

/* At the VBlank that shows the frame (md_isr.c game_tick): the oldest staged one. */
void md_render_commit(void)
{
    Stage *st = &s_stage[s_rIdx];
    s_rIdx ^= 1;
    md_dma_queue(VRAM_SPRITES, st->sat, (uint16_t)(st->satCount * 8));
    for (uint8_t i = 0; i < st->upCount; i++) md_dma_queue(st->up[i].vram, st->up[i].src, st->up[i].bytes);
    for (uint8_t i = 0; i < st->pjCount; i++)
        md_dma_queue_ex(MD_DMA_VRAM, st->pj[i].vram, &st->pw[st->pj[i].first], (uint16_t)(st->pj[i].count * 2), st->pj[i].inc);
    md_dma_queue_ex(MD_DMA_VSRAM, 2, &st->vscroll, 2, 2);     /* plane B's; plane A stays at 0 */
    md_dma_queue(VRAM_HSCROLL + 2, &st->hscroll, 2);
    if (st->palStaged) md_dma_queue_ex(MD_DMA_CRAM, 0, st->pal, 128, 2);
}

void md_render_shutdown(void)
{
    s_active = 0;
}
