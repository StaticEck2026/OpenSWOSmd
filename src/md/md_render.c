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

/* ---- staged frame (written by md_render_frame, queued by md_render_commit) ---- */
typedef struct { uint16_t y, sizeLink, attr, x; } VdpSprite;
static VdpSprite s_sat[80];
static uint8_t  s_satCount;
static uint16_t s_hscroll, s_vscroll;
static uint16_t s_pal[64];
static uint8_t  s_palStaged;

#define UPLOAD_MAX 40
typedef struct { const void *src; uint16_t vram, bytes; } Upload;
static Upload s_up[UPLOAD_MAX];
static uint8_t s_upCount;

/* plane writes: runs of nametable words, contiguous (inc 2) or a column (inc 128) */
#define PLANE_JOBS 12
#define PLANE_WORDS 640
typedef struct { uint16_t vram; uint8_t count, inc; uint16_t first; } PlaneJob;
static PlaneJob s_pj[PLANE_JOBS];
static uint16_t s_pw[PLANE_WORDS];
static uint8_t  s_pjCount;
static uint16_t s_pwCount;
static uint8_t  s_staged;

static int plane_job(uint16_t vram, uint8_t inc, const uint16_t *words, uint8_t n)
{
    if (s_pjCount >= PLANE_JOBS || s_pwCount + n > PLANE_WORDS) return 0;
    PlaneJob *j = &s_pj[s_pjCount++];
    j->vram = vram; j->count = n; j->inc = inc; j->first = s_pwCount;
    for (uint8_t i = 0; i < n; i++) s_pw[s_pwCount++] = words[i];
    return 1;
}

/* ---- palettes ---- */

/* Amiga 12-bit 0RGB -> VDP 9-bit 0BGR (top three bits of each channel), faded. */
static uint16_t md_color(uint16_t amiga, uint16_t level)
{
    uint16_t r = (amiga >> 8) & 15, g = (amiga >> 4) & 15, b = amiga & 15;
    if (level < 0x80) {
        r = (uint16_t)((r * level) >> 7);
        g = (uint16_t)((g * level) >> 7);
        b = (uint16_t)((b * level) >> 7);
    }
    return (uint16_t)(((b >> 1) << 9) | ((g >> 1) << 5) | ((r >> 1) << 1));
}

static void build_palettes(void)
{
    uint16_t lvl = md_fadeLevel;
    for (int i = 0; i < 16; i++) {
        s_pal[i]      = md_color(_gamePalette[i], lvl);
        s_pal[16 + i] = md_color(_gamePalette[_team1PlayersColorTable[i] & 15], lvl);
        s_pal[32 + i] = md_color(_gamePalette[_team2PlayersColorTable[i] & 15], lvl);
        s_pal[48 + i] = md_color(_gamePalette[_colorTableReserves[i] & 15], lvl);
    }
    s_pal[0] = md_color(_gamePalette[0], lvl);     /* backdrop */
    s_palStaged = 1;
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
    for (int16_t i = 0; i < LOAD_W; i++) w[i] = cell_entry(s_cx0 + i, cy);
    plane_job((uint16_t)(VRAM_PLANE_B + (prow * PLANE_W + c0) * 2), 2, w, n1);
    if (n1 < LOAD_W)
        plane_job((uint16_t)(VRAM_PLANE_B + prow * PLANE_W * 2), 2, w + n1, (uint8_t)(LOAD_W - n1));
}

/* animated_patterns.c toggled a fan: its four cells, if they are loaded */
void md_pitch_cell_changed(uint16_t cell)
{
    if (!s_active) return;
    int16_t cx = (int16_t)((cell % PITCH_COLS) * 2), cy = (int16_t)((cell / PITCH_COLS) * 2);
    for (int16_t dy = 0; dy < 2; dy++) {
        int16_t y = cy + dy;
        if (y < s_cy0 || y >= s_cy0 + s_loadH) continue;
        for (int16_t dx = 0; dx < 2; dx++) {
            int16_t x = cx + dx;
            if (x < s_cx0 || x >= s_cx0 + LOAD_W) continue;
            uint16_t e = cell_entry(x, y);
            plane_job((uint16_t)(VRAM_PLANE_B + ((y & (PLANE_H - 1)) * PLANE_W + (x & (PLANE_W - 1))) * 2), 2, &e, 1);
        }
    }
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

/* ---- the sprite tile cache ---- */
#define CACHE_SLOTS 64
#define UNIT 4                              /* tiles per allocation unit */
#define UNITS (TILE_DYN_COUNT / UNIT)       /* 128 */
typedef struct { const MdImage *img; uint16_t lastFrame; uint8_t unit, units; } CacheEnt;
static CacheEnt s_cache[CACHE_SLOTS];
static uint8_t s_unitUsed[UNITS];           /* cache entry + 1 owning the unit, 0 free */
static uint16_t s_frame;
static uint16_t s_dmaBudget;

static void cache_reset(void)
{
    for (int i = 0; i < CACHE_SLOTS; i++) s_cache[i].img = 0;
    for (int i = 0; i < UNITS; i++) s_unitUsed[i] = 0;
}

static void cache_free(int e)
{
    for (int u = 0; u < s_cache[e].units; u++) s_unitUsed[s_cache[e].unit + u] = 0;
    s_cache[e].img = 0;
}

/* The image's first VRAM tile, uploading it if it is not resident; -1 if there is no
 * room or no DMA budget left this frame (the sprite is skipped for one frame). */
static int16_t cache_get(const MdImage *img)
{
    uint8_t h = (uint8_t)(((uint32_t)img >> 1) & (CACHE_SLOTS - 1));
    int free_e = -1;
    for (int k = 0; k < CACHE_SLOTS; k++) {
        int e = (h + k) & (CACHE_SLOTS - 1);
        if (s_cache[e].img == img) {
            s_cache[e].lastFrame = s_frame;
            return (int16_t)(TILE_DYN_SPRITES + s_cache[e].unit * UNIT);
        }
        if (!s_cache[e].img) { if (free_e < 0) free_e = e; break; }
    }
    uint16_t bytes = (uint16_t)(img->nTiles * 32);
    if (bytes > s_dmaBudget || s_upCount >= UPLOAD_MAX) return -1;
    uint8_t need = (uint8_t)((img->nTiles + UNIT - 1) / UNIT);
    for (int pass = 0; pass < 2; pass++) {
        uint8_t run = 0;
        for (int u = 0; u < UNITS; u++) {
            run = s_unitUsed[u] ? 0 : (uint8_t)(run + 1);
            if (run == need) {
                int first = u - need + 1;
                if (free_e < 0) {
                    for (int k = 0; k < CACHE_SLOTS; k++)
                        if (!s_cache[(h + k) & (CACHE_SLOTS - 1)].img) { free_e = (h + k) & (CACHE_SLOTS - 1); break; }
                    if (free_e < 0) return -1;
                }
                CacheEnt *c = &s_cache[free_e];
                c->img = img; c->lastFrame = s_frame; c->unit = (uint8_t)first; c->units = need;
                for (int v = 0; v < need; v++) s_unitUsed[first + v] = (uint8_t)(free_e + 1);
                s_up[s_upCount].src = MD_IMAGE_TILES(img);
                s_up[s_upCount].vram = (uint16_t)((TILE_DYN_SPRITES + first * UNIT) * 32);
                s_up[s_upCount].bytes = bytes;
                s_upCount++;
                s_dmaBudget -= bytes;
                return (int16_t)(TILE_DYN_SPRITES + first * UNIT);
            }
        }
        /* full: drop everything not drawn this frame, and try again */
        for (int e = 0; e < CACHE_SLOTS; e++)
            if (s_cache[e].img && s_cache[e].lastFrame != s_frame) cache_free(e);
        /* the hash chains are broken by the frees: rebuild by re-inserting survivors */
        {
            CacheEnt keep[CACHE_SLOTS]; int n = 0;
            for (int e = 0; e < CACHE_SLOTS; e++) if (s_cache[e].img) keep[n++] = s_cache[e];
            for (int e = 0; e < CACHE_SLOTS; e++) s_cache[e].img = 0;
            for (int i = 0; i < UNITS; i++) s_unitUsed[i] = 0;
            for (int i = 0; i < n; i++) {
                uint8_t hh = (uint8_t)(((uint32_t)keep[i].img >> 1) & (CACHE_SLOTS - 1));
                for (int k = 0; k < CACHE_SLOTS; k++) {
                    int e = (hh + k) & (CACHE_SLOTS - 1);
                    if (!s_cache[e].img) {
                        s_cache[e] = keep[i];
                        for (int v = 0; v < keep[i].units; v++) s_unitUsed[keep[i].unit + v] = (uint8_t)(e + 1);
                        break;
                    }
                }
            }
            free_e = -1;
            for (int k = 0; k < CACHE_SLOTS; k++)
                if (!s_cache[(h + k) & (CACHE_SLOTS - 1)].img) { free_e = (h + k) & (CACHE_SLOTS - 1); break; }
        }
    }
    return -1;
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

/* ---- the HUD: the engine's HUD sprites, as text on plane A ---- */
#define HUD_ROWS 2
#define HUD_COLS 40
static char s_hud[HUD_ROWS][HUD_COLS + 1], s_hudShown[HUD_ROWS][HUD_COLS + 1];
static uint8_t s_hudRowY[HUD_ROWS];

static void hud_put(int row, int col, const char *t)
{
    if (row < 0 || row >= HUD_ROWS) return;
    for (; *t && col < HUD_COLS; t++, col++)
        if (col >= 0) s_hud[row][col] = *t;
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
        md_snprintf(buf, 6, "%d ", p->shirtNumber);
        int n = 0; while (buf[n]) n++;
        str_copy(buf + n, p->shortName, 15);
        hud_put(row, col, buf);
    } else if (img >= 214 && img <= 216) {              /* the clock (game_state.c UpdateTime) */
        uint16_t mins = (uint16_t)(_gameTime[1] * 100 + _gameTime[2] * 10 + _gameTime[3]);
        md_snprintf(buf, sizeof buf, "%d MINS", mins);
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

static void stage_hud(void)
{
    for (int r = 0; r < HUD_ROWS; r++) {
        int changed = 0;
        for (int i = 0; i < HUD_COLS; i++) if (s_hud[r][i] != s_hudShown[r][i]) { changed = 1; break; }
        if (!changed) continue;
        uint16_t w[HUD_COLS];
        for (int i = 0; i < HUD_COLS; i++) {
            char ch = s_hud[r][i];
            w[i] = ch > ' ' && ch < 127 ? TILE_ATTR(0, 1, 0, 0, TILE_FONT + ch - 32) : 0;
            s_hudShown[r][i] = ch;
        }
        plane_job((uint16_t)(VRAM_PLANE_A + s_hudRowY[r] * PLANE_W * 2), 2, w, HUD_COLS);
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
    for (int r = 0; r < HUD_ROWS; r++)
        for (int i = 0; i <= HUD_COLS; i++) s_hud[r][i] = s_hudShown[r][i] = 0;
    build_palettes();
    md_cram_write(0, s_pal, 64);
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
    if (s_staged) return;           /* the last frame has not been shown yet */

    s_frame++;
    s_pjCount = 0;
    s_pwCount = 0;
    s_upCount = 0;
    s_dmaBudget = md_is_pal() ? 9000 : 5200;

    pitch_scroll(rx, ry);
    s_hscroll = (uint16_t)(-rx);
    s_vscroll = (uint16_t)ry;

    for (int r = 0; r < HUD_ROWS; r++)
        for (int i = 0; i < HUD_COLS; i++) s_hud[r][i] = ' ';

    /* sprites, front to back; the VDP links them in table order and shows the first on top */
    uint8_t n = 0;
    for (uint16_t i = 0; i < md_sprCount && n < 80; i++) {
        const MdSprCmd *c = &md_sprList[i];
        if (c->spr->z.s.i >= HUD_Z_MIN) { hud_sprite(c); continue; }
        uint8_t pal;
        const MdImage *img = image_for(c->image, &pal);
        if (!img || !img->nPieces) continue;
        int16_t base = cache_get(img);
        if (base < 0) continue;
        int16_t sx = (int16_t)(c->x - 8 + 128), sy = (int16_t)(c->y - s_offY + 128);
        for (uint8_t p = 0; p < img->nPieces && n < 80; p++) {
            const MdPiece *pc = &img->pieces[p];
            int16_t x = sx + pc->dx, y = sy + pc->dy;
            if (x <= 96 || x >= 128 + 320 || y <= 96 || y >= 128 + s_screenH) continue;
            VdpSprite *v = &s_sat[n];
            v->y = (uint16_t)y;
            v->sizeLink = (uint16_t)((pc->size << 8) | (n + 1));
            v->attr = TILE_ATTR(pal, 0, 0, 0, base + pc->tile);
            v->x = (uint16_t)x;
            n++;
        }
    }
    if (n == 0) {                    /* an empty table is one sprite off screen */
        s_sat[0].y = 0; s_sat[0].sizeLink = 0; s_sat[0].attr = 0; s_sat[0].x = 0;
        n = 1;
    }
    s_sat[n - 1].sizeLink &= 0xFF00;   /* end of the link chain */
    s_satCount = n;

    stage_hud();
    build_palettes();
    s_staged = 1;
}

/* At the VBlank that shows the frame (md_isr.c game_tick). */
void md_render_commit(void)
{
    if (!s_staged) return;
    md_dma_queue(VRAM_SPRITES, s_sat, (uint16_t)(s_satCount * 8));
    for (uint8_t i = 0; i < s_upCount; i++) md_dma_queue(s_up[i].vram, s_up[i].src, s_up[i].bytes);
    for (uint8_t i = 0; i < s_pjCount; i++)
        md_dma_queue_ex(MD_DMA_VRAM, s_pj[i].vram, &s_pw[s_pj[i].first], (uint16_t)(s_pj[i].count * 2), s_pj[i].inc);
    md_dma_queue_ex(MD_DMA_VSRAM, 2, &s_vscroll, 2, 2);     /* plane B's; plane A stays at 0 */
    md_dma_queue(VRAM_HSCROLL + 2, &s_hscroll, 2);
    if (s_palStaged) { md_dma_queue_ex(MD_DMA_CRAM, 0, s_pal, 128, 2); s_palStaged = 0; }
    s_staged = 0;
}

void md_render_shutdown(void)
{
    s_active = 0;
    s_staged = 0;
}
