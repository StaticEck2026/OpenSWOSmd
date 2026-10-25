/* md_export.c -- the player's SWOS data, decoded, for the Mega Drive ROM (SDL only).
 *
 *   SWOS --md-export=DIR       then   make -f Makefile.md MD_EXPORT=DIR
 *
 * The cartridge has no disk: scripts/md/make_md_data.py turns the game's art into VDP
 * tiles at build time. It cannot decode the Amiga files itself, but this build can -- so
 * this writes what it decoded, in the plain format the script reads (docs/MEGADRIVE.md
 * "The game data"):
 *
 *   sprites.bin, kit1..3.bin   "MDSP", u16 count, then per image: u16 index, i16 width,
 *                              height, centreX, centreY, width*height palette indices
 *   pitch1..6.bin              "MDPT", u16 tile count, 256 palette indices a 16x16 tile,
 *                              the 42x55 map of tile numbers (u16)
 *   teams.bin                  every database team: 684-byte TeamFileEx records, as on
 *                              the Amiga disk
 *
 * all big-endian. The art is the Amiga's own (OCS, 16 colours: the Mega Drive's palette
 * lines are 16 colours): the grafs/ set, which this build normally passes over for
 * grafs_aga/ (resload.c), so the run turns that off (g_grafsOcsOnly). The sprites are the
 * image script's output BEFORE ConvertColorsAndTextSprites' colour conversions -- the
 * cartridge does those with palette lines -- and the pitches are the files as they are,
 * before FixupAndColorPitchFile colours the crowd in this match's kits.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "game_types.h"
#include "team_db.h"
#include "file_io.h"
#include "swos_io.h"
#include "endian_helpers.h"
#include "display.h"

extern uint8_t *_spriteGraphicsPtr;
extern uint16_t _menuColorAndTextScript[];
extern uint8_t *_menuImageStreamBaseEnd;
extern uintptr_t _team1Filename, _team2Filename;
extern void *_teamFilenamesTable[];
extern void *_swcPitchMapFiles[];
extern volatile uint16_t _playGame;
extern int32_t g_grafsOcsOnly;          /* resload.c */
extern void ProcessImageCommandStream(uint16_t *stream, uint8_t *dest);

#define NUM_IMAGES      1310
#define KIT_FIRST       227
#define KIT_LAST        833             /* one past */
#define PITCH_SLOTS     (42 * 55)
#define PITCH_HDR_BYTES (PITCH_SLOTS * 4)
#define OCS_TILE_BYTES  128             /* 16 rows x 4 plane words */

static char s_dir[900];
static int32_t s_armed;

int32_t MdExport_Arm(const char *dir)
{
    if (!dir || !*dir || strlen(dir) >= sizeof s_dir) return -1;
    strcpy(s_dir, dir);
    s_armed = 1;
    return 0;
}

int32_t MdExport_Armed(void) { return s_armed; }

/* ---- big-endian output ---- */
static void put16(FILE *f, uint16_t v) { uint8_t b[2]; BE_WRITE_U16(b, v); fwrite(b, 1, 2, f); }

static FILE *open_out(const char *name)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", s_dir, name);
    FILE *f = fopen(path, "wb");
    if (!f) fprintf(stderr, "md-export: cannot write %s\n", path);
    return f;
}

/* An image of the sprite table, or 0 when the slot holds none. */
static const SpriteGraphics *image(uint16_t idx)
{
    const SpriteGraphics *g = ((SpriteGraphics *const *)_spriteGraphicsPtr)[idx];
    if (!g || !g->sprData || g->pixWidth <= 0 || g->nlines <= 0) return 0;
    if (g->pixWidth > g->wquads * 16) return 0;
    return g;
}

/* Images [first, last) to an MDSP file. The decoded rows are wquads*16 bytes apart
 * (image_cmd.c), one palette index a byte; the file has pixWidth of each. Returns the
 * number written, -1 on an error (a pixel past 15: not the 16-colour art). */
static int32_t write_images(const char *name, uint16_t first, uint16_t last)
{
    FILE *f = open_out(name);
    uint16_t n = 0;
    if (!f) return -1;
    for (uint16_t i = first; i < last; i++) if (image(i)) n++;
    fwrite("MDSP", 1, 4, f);
    put16(f, n);
    for (uint16_t i = first; i < last; i++) {
        const SpriteGraphics *g = image(i);
        if (!g) continue;
        put16(f, i);
        put16(f, (uint16_t)g->pixWidth);
        put16(f, (uint16_t)g->nlines);
        put16(f, (uint16_t)g->centerX);
        put16(f, (uint16_t)g->centerY);
        for (int16_t y = 0; y < g->nlines; y++) {
            const uint8_t *row = (const uint8_t *)g->sprData + (uint32_t)y * (uint32_t)(g->wquads * 16);
            for (int16_t x = 0; x < g->pixWidth; x++)
                if (row[x] > 15) {
                    fprintf(stderr, "md-export: image %u has colour %u: not the Amiga's 16-colour art\n",
                            i, row[x]);
                    fclose(f);
                    return -1;
                }
            fwrite(row, 1, (size_t)g->pixWidth, f);
        }
    }
    fclose(f);
    return n;
}

/* The match's image script, with this kit sheet for both teams (what
 * ConvertColorsAndTextSprites runs, without its colour conversions after). */
static void load_images(uint16_t sheet)
{
    _team1Filename = _team2Filename = (uintptr_t)_teamFilenamesTable[sheet];
    ProcessImageCommandStream(_menuColorAndTextScript, *(uint8_t *volatile *)&_menuImageStreamBaseEnd);
}

/* Pitch n (0..5): the file's 42x55 map of 32-bit tile offsets, then its OCS tiles. */
static int32_t write_pitch(int32_t n)
{
    static uint8_t *buf;
    const uint32_t bufSize = (uint32_t)PITCHFILE_MAX_SIZE;
    const char *fname = (const char *)_swcPitchMapFiles[n];
    uint32_t size = 0, ntiles;
    char out[16];
    FILE *f;

    if (!buf && !(buf = malloc(bufSize))) return -1;
    if (!swos_data_exists(fname)) { fprintf(stderr, "md-export: no %s\n", fname); return -1; }
    if (LoadFileEx((uint8_t *)fname, buf, &size) != 0 || size <= PITCH_HDR_BYTES || size > bufSize) {
        fprintf(stderr, "md-export: cannot read %s\n", fname);
        return -1;
    }
    if ((size - PITCH_HDR_BYTES) % OCS_TILE_BYTES) {
        fprintf(stderr, "md-export: %s is not a 4-bitplane pitch\n", fname);
        return -1;
    }
    ntiles = (size - PITCH_HDR_BYTES) / OCS_TILE_BYTES;
    snprintf(out, sizeof out, "pitch%d.bin", (int)n + 1);
    if (!(f = open_out(out))) return -1;
    fwrite("MDPT", 1, 4, f);
    put16(f, (uint16_t)ntiles);
    for (uint32_t t = 0; t < ntiles; t++) {
        const uint8_t *src = buf + PITCH_HDR_BYTES + t * OCS_TILE_BYTES;
        uint8_t px[256];
        for (int row = 0; row < 16; row++) {
            const uint8_t *r = src + row * 8;   /* 4 plane words, big-endian (pitch.c) */
            for (int x = 0; x < 16; x++) {
                uint8_t v = 0;
                for (int pl = 0; pl < 4; pl++)
                    v |= (uint8_t)(((BE_READ_U16(r + pl * 2) >> (15 - x)) & 1u) << pl);
                px[row * 16 + x] = v;
            }
        }
        fwrite(px, 1, 256, f);
    }
    for (int k = 0; k < PITCH_SLOTS; k++) {
        uint32_t off = BE_READ_U32(buf + k * 4);
        uint32_t tile = off / OCS_TILE_BYTES;
        put16(f, (uint16_t)(tile < ntiles ? tile : 0));
    }
    fclose(f);
    return (int32_t)ntiles;
}

/* Every database team, country by country (so the cartridge's FRIENDLY screen finds a
 * country's teams together). TeamFile's one word, globalTeamNumber, is written
 * big-endian whichever database it came from: the JSON one holds it in host order, the
 * binary TEAM.nnn files (SWOS_DATA_JSON=0) as on the disk. */
static int32_t write_teams(void)
{
    const char *json = getenv("SWOS_DATA_JSON");
    int binary = json && json[0] == '0' && json[1] == '\0';
    FILE *f = open_out("teams.bin");
    int32_t n = 0;
    if (!f) return -1;
    for (int32_t c = 0; c < 256; c++) {
        const TeamDbEntry *e;
        for (int32_t i = 0; (e = TeamDb_Entry(c, i)) != NULL; i++) {
            uint8_t rec[sizeof(TeamFileEx)];
            memcpy(rec, &e->team, sizeof rec);
            if (!binary) BE_WRITE_U16(rec + 2, (uint16_t)e->team.team.globalTeamNumber);
            fwrite(rec, 1, sizeof rec, f);
            n++;
        }
    }
    fclose(f);
    return n;
}

void MdExport_Run(void)
{
    static const char *const kOcsFiles[] = {
        "grafs/cjcgrafs.raw", "grafs/cjcteam1.raw", "grafs/cjcteam2.raw", "grafs/cjcteam3.raw",
        "grafs/cjcteamg.raw", "grafs/cjcbits.raw", "grafs/soccer_s.raw", "grafs/cjcbench.raw", 0
    };
    int32_t n, failed = 0;

    /* the image script asks for its files again for ever when one is missing */
    for (int i = 0; kOcsFiles[i]; i++)
        if (!swos_data_exists(kOcsFiles[i])) {
            fprintf(stderr, "md-export: no %s -- the Mega Drive ROM is built from the Amiga\n"
                            "game's graphics (OpenSWOS Setup with the Amiga disks)\n", kOcsFiles[i]);
            exit(1);
        }
    mkdir(s_dir, 0777);
    g_grafsOcsOnly = 1;
    _playGame = 0;                  /* every skin's images (image_cmd.c skips some in a match) */

    /* the sprite table with kit sheet 1, then the player images of each kit sheet; the
     * table index of a sheet is a shirt type that uses it (_teamFilenamesTable) */
    load_images(0);
    if ((n = write_images("sprites.bin", 0, NUM_IMAGES)) < 0) failed = 1;
    else printf("md-export: %d images\n", (int)n);
    {
        static const uint16_t kSheetShirt[3] = { 0, 1, 3 };   /* cjcteam1, 2, 3 */
        for (int s = 0; s < 3 && !failed; s++) {
            char name[16];
            load_images(kSheetShirt[s]);
            snprintf(name, sizeof name, "kit%d.bin", s + 1);
            if (write_images(name, KIT_FIRST, KIT_LAST) < 0) failed = 1;
        }
    }
    for (int32_t p = 0; p < 6 && !failed; p++) {
        if ((n = write_pitch(p)) < 0) failed = 1;
        else printf("md-export: pitch %d, %d tiles\n", (int)p + 1, (int)n);
    }
    if (!failed) {
        n = write_teams();
        if (n <= 0) { fprintf(stderr, "md-export: no teams\n"); failed = 1; }
        else printf("md-export: %d teams\n", (int)n);
    }
    g_grafsOcsOnly = 0;
    if (failed) exit(1);
    printf("md-export: wrote %s -- now: make -f Makefile.md MD_EXPORT=%s\n", s_dir, s_dir);
    exit(0);
}
