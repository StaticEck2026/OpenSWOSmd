/*
 * dos_gfx_test -- the PC game's pitches and full-screen pictures, converted.
 *
 *   dos_gfx_test <dosDir> <outDir> [<2020Dir>] [<agaDir>]
 *
 * Writes <outDir>/SWCPICH1-6.MAP (unpacked), MENUBG.bmp (SWTITLE.256) and
 * MENUBG2.bmp (STAD.256), and P2020_1-6.MAP from a SWOS 2020 grafs folder when
 * one is given. With an AGA folder (bin/grafs_aga) each result is compared with
 * the shipped file: the maps byte for byte after unpacking, the pictures index
 * for index against MENUBG.RAW / MENUBG2.RAW (8 bitplanes, row-interleaved).
 * Exit status 1 when anything differs.
 */
#include "dos_pitch.h"
#include "dos_picture.h"
#include "sd_util.h"

#include <stdio.h>
#include <string.h>

static int32_t bad = 0;

static int32_t load_unpacked(const char *dir, const char *name, SdBuf *out)
{
    char path[1024], err[256];
    SdBuf raw;
    if (Sd_FindNoCase(dir, name, path, sizeof path) || Sd_ReadFile(path, &raw, err)) {
        printf("  %s: no reference in %s\n", name, dir);
        return -1;
    }
    if (Sd_RncUnpack(&raw, out, err)) {
        printf("  %s: %s\n", name, err);
        Sd_BufFree(&raw);
        return -1;
    }
    Sd_BufFree(&raw);
    return 0;
}

static void check_map(const char *agaDir, const char *name, const SdBuf *map)
{
    SdBuf ref;
    uint32_t i, diff = 0, n;
    if (!agaDir || load_unpacked(agaDir, name, &ref))
        return;
    n = map->size < ref.size ? map->size : ref.size;
    for (i = 0; i < n; i++)
        diff += map->data[i] != ref.data[i];
    if (map->size == ref.size && !diff)
        printf("  %s: identical (%u bytes)\n", name, (unsigned)ref.size);
    else {
        printf("  %s: DIFFERS (%u vs %u bytes, %u differ)\n", name, (unsigned)map->size,
               (unsigned)ref.size, (unsigned)diff);
        bad = 1;
    }
    Sd_BufFree(&ref);
}

static void check_picture(const char *agaDir, const char *name, const SdImage *img)
{
    SdBuf ref;
    int32_t x, y, p, planes = 8, bpr = img->w / 8, diff = 0;
    if (!agaDir || load_unpacked(agaDir, name, &ref))
        return;
    if (ref.size != (uint32_t)(bpr * planes * img->h)) {
        printf("  %s: %u bytes, not %d x %d x 8 planes\n", name, (unsigned)ref.size, img->w, img->h);
        bad = 1;
        Sd_BufFree(&ref);
        return;
    }
    for (y = 0; y < img->h; y++)
        for (x = 0; x < img->w; x++) {
            int32_t v = 0;
            for (p = 0; p < planes; p++)
                v |= ((ref.data[(y * planes + p) * bpr + x / 8] >> (7 - x % 8)) & 1) << p;
            diff += v != img->px[y * img->w + x];
        }
    if (!diff)
        printf("  %s: identical (%dx%d indices)\n", name, img->w, img->h);
    else {
        printf("  %s: DIFFERS (%d of %d pixels)\n", name, diff, img->w * img->h);
        bad = 1;
    }
    Sd_BufFree(&ref);
}

static void pitches(const char *srcDir, const char *prefix, const char *outDir, const char *agaDir)
{
    char err[256], name[64], path[1024];
    int32_t n;
    for (n = 1; n <= 6; n++) {
        SdBuf map;
        snprintf(name, sizeof name, "%s%d.MAP", prefix, (int)n);
        if (Sd_DosPitchToAgaMap(srcDir, n, &map, err)) {
            printf("FAIL %s: %s\n", name, err);
            bad = 1;
            continue;
        }
        Sd_PathJoin(path, sizeof path, outDir, name);
        if (Sd_WriteFile(path, map.data, map.size, err)) {
            printf("FAIL %s\n", err);
            bad = 1;
        }
        printf("%s: %u bytes%s\n", name, (unsigned)map.size,
               map.size > SD_PITCH_AGA_BUFFER ? "  -- OVER the pitch buffer" : "");
        check_map(agaDir, name, &map);
        Sd_BufFree(&map);
    }
}

static void picture(const char *dosDir, const char *in, const char *out, const char *raw,
                    const char *outDir, const char *agaDir)
{
    char err[256], path[1024];
    SdImage pic, big;
    int32_t tc;
    if (Sd_FindNoCase(dosDir, in, path, sizeof path)) {
        printf("FAIL %s is not in %s\n", in, dosDir);
        bad = 1;
        return;
    }
    if (Sd_DosPictureLoadEx(path, &pic, &tc, err) || Sd_DosPictureScaleToAmiga(&pic, &big, err)) {
        printf("FAIL %s: %s\n", in, err);
        bad = 1;
        return;
    }
    Sd_PathJoin(path, sizeof path, outDir, out);
    if (Sd_DosPictureWriteBmp(&big, path, err)) {
        printf("FAIL %s\n", err);
        bad = 1;
    }
    printf("%s -> %s: %dx%d, text colour %d\n", in, out, big.w, big.h, (int)tc);
    check_picture(agaDir, raw, &big);
    SdImage_Free(&pic);
    SdImage_Free(&big);
}

int main(int argc, char **argv)
{
    const char *aga;
    if (argc < 3) {
        printf("usage: dos_gfx_test <dosDir> <outDir> [<2020Dir>|-] [<agaDir>]\n");
        return 2;
    }
    aga = argc > 4 ? argv[4] : NULL;
    pitches(argv[1], "SWCPICH", argv[2], aga);
    if (argc > 3 && strcmp(argv[3], "-") != 0)
        pitches(argv[3], "P2020_", argv[2], aga);
    picture(argv[1], "SWTITLE.256", "MENUBG.bmp", "MENUBG.RAW", argv[2], aga);
    picture(argv[1], "STAD.256", "MENUBG2.bmp", "MENUBG2.RAW", argv[2], aga);
    printf(bad ? "SOMETHING DIFFERS\n" : "all good\n");
    return bad;
}
