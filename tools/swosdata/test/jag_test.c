/*
 * jag_test -- build the Jaguar tier (grafs_jag) from the cartridge and check it.
 *
 *   jag_test <rom.jag> <dosDir> <setupDir> <outDir> [refDir [bmpDir]]
 *
 * Writes the eight files to <outDir>. With refDir (bin/grafs_jag), each is
 * compared byte for byte with refDir/<name>. With bmpDir (SwosGfx's
 * Amiga_Images_256_Jag), each bank is also compared index for index with
 * <BANK>.bmp, and the raw CJCGRAFS sheet with CJCGRAFS_JAGSHEET.bmp, so a
 * difference is found by pixel rather than by planar byte.
 */
#include "jag2amiga.h"
#include "sd_image.h"
#include "sd_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int32_t CompareFile(const SdJagFile *f, const char *refDir)
{
    char path[1024], err[256];
    SdBuf ref;
    uint32_t first = 0, diff = 0, n;
    int32_t bad;

    Sd_PathJoin(path, sizeof path, refDir, f->name);
    if (Sd_ReadFile(path, &ref, err) != 0) {
        printf("%-12s no reference (%s)\n", f->name, err);
        return 1;
    }
    n = ref.size < f->data.size ? ref.size : f->data.size;
    for (uint32_t i = 0; i < n; i++)
        if (ref.data[i] != f->data.data[i]) {
            if (!diff)
                first = i;
            diff++;
        }
    if (ref.size != f->data.size)
        printf("%-12s size %u, reference %u; %u of the common bytes differ\n", f->name,
               (unsigned)f->data.size, (unsigned)ref.size, (unsigned)diff);
    else if (diff)
        printf("%-12s %u bytes differ, first at %u\n", f->name, (unsigned)diff, (unsigned)first);
    else
        printf("%-12s identical (%u bytes)\n", f->name, (unsigned)f->data.size);
    bad = (diff || ref.size != f->data.size) ? 1 : 0;
    Sd_BufFree(&ref);
    return bad;
}

static int32_t CompareBmp(const SdImage *img, const char *bmpDir, const char *name)
{
    char path[1024], file[64], err[256];
    SdImage ref;
    int32_t diff = 0, fx = -1, fy = -1, palDiff = 0;

    snprintf(file, sizeof file, "%s.bmp", name);
    Sd_PathJoin(path, sizeof path, bmpDir, file);
    memset(&ref, 0, sizeof ref);
    if (SdImage_ReadBmp(&ref, path, err) != 0) {
        printf("  %-18s no reference (%s)\n", file, err);
        return 1;
    }
    if (ref.w != img->w || ref.h != img->h) {
        printf("  %-18s is %dx%d, ours %dx%d\n", file, (int)ref.w, (int)ref.h, (int)img->w, (int)img->h);
        SdImage_Free(&ref);
        return 1;
    }
    for (int32_t y = 0; y < img->h; y++)
        for (int32_t x = 0; x < img->w; x++)
            if (ref.px[y * img->w + x] != img->px[y * img->w + x]) {
                if (!diff) { fx = x; fy = y; }
                diff++;
            }
    palDiff = memcmp(ref.pal, img->pal, sizeof ref.pal) != 0;
    if (diff || palDiff)
        printf("  %-18s %d pixels differ (first at (%d,%d))%s\n", file, (int)diff, (int)fx, (int)fy,
               palDiff ? ", palette differs" : "");
    else
        printf("  %-18s identical\n", file);
    SdImage_Free(&ref);
    return (diff || palDiff) ? 1 : 0;
}

int main(int argc, char **argv)
{
    SdJagFile files[SD_JAG_FILE_COUNT];
    char err[256], path[1024];
    uint32_t celeb = 0;
    int32_t bad = 0;

    if (argc < 5) {
        fprintf(stderr, "usage: %s <rom.jag> <dosDir> <setupDir> <outDir> [refDir [bmpDir]]\n", argv[0]);
        return 2;
    }
    if (Sd_JagBuild(argv[1], argv[2], argv[3], files, &celeb, err) != 0) {
        fprintf(stderr, "jag2amiga: %s\n", err);
        return 1;
    }
    printf("celebrations drawn by hand: %s%s%s\n",
           celeb & (1u << SD_JAG_CJCTEAM1) ? "CJCTEAM1 " : "",
           celeb & (1u << SD_JAG_CJCTEAM2) ? "CJCTEAM2 " : "",
           celeb & (1u << SD_JAG_CJCTEAM3) ? "CJCTEAM3" : "");
    for (int32_t i = 0; i < SD_JAG_FILE_COUNT; i++) {
        Sd_PathJoin(path, sizeof path, argv[4], files[i].name);
        if (Sd_WriteFile(path, files[i].data.data, files[i].data.size, err) != 0) {
            fprintf(stderr, "%s\n", err);
            bad = 1;
        }
        if (argc > 5)
            bad |= CompareFile(&files[i], argv[5]);
    }
    Sd_JagFilesFree(files);

    if (argc > 6) {
        SdImage banks[SD_JAG_BANK_COUNT], grafs;
        SdBuf rom;

        if (Sd_ReadFile(argv[1], &rom, err) != 0 ||
            Sd_JagBanks(&rom, argv[2], argv[3], banks, &grafs, NULL, err) != 0) {
            fprintf(stderr, "jag2amiga: %s\n", err);
            return 1;
        }
        Sd_BufFree(&rom);
        printf("banks against %s:\n", argv[6]);
        for (int32_t b = 0; b < SD_JAG_BANK_COUNT; b++) {
            bad |= CompareBmp(&banks[b], argv[6], Sd_JagBankNames[b]);
            SdImage_Free(&banks[b]);
        }
        bad |= CompareBmp(&grafs, argv[6], "CJCGRAFS_JAGSHEET");
        SdImage_Free(&grafs);
    }
    printf(bad ? "FAILED\n" : "all identical\n");
    return bad ? 1 : 0;
}
