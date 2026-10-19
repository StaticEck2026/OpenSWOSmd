/*
 * amiga_raw_test -- check amiga_raw.c and credits_aga.c against the shipped files.
 *
 *   amiga_raw_test <amigaDir> <grafsAgaDir> <menubg.bmp> <credits_icons.bmp> [bmpOutDir]
 *
 * amigaDir: the ORIGINAL 4-plane RAWs (SwosGfx's Amiga folder, or the disks').
 * 1. each Amiga-only picture: original -> 256 colours -> auto-depth RAW must equal
 *    the RNC-unpacked grafs_aga file (and, with bmpOutDir, its BMP is written there
 *    to compare with SwosGfx's Amiga_Images_256).
 * 2. every grafs_aga RAW listed decodes and re-encodes to the same bytes.
 * 3. CREDITS.RAW from menubg.bmp + credits_icons.bmp, and again from MENUBG.RAW.
 */
#include "amiga_raw.h"
#include "credits_aga.h"
#include "sg_image.h"
#include "sg_util.h"

#include <stdio.h>
#include <string.h>

static const char *k_pictures[] = {
    "LOADER00", "LOADER01", "LOADER1", "LOADER10", "LOADER2", "LOADER3", "LOADER4",
    "LOADER5", "LOADER6", "LOADER7", "LOADER8A", "LOADER8B", "LOADER9", "DISK22", "OLDDISK1",
};
static const char *k_roundTrip[] = {
    "LOADER00", "LOADER01", "LOADER1", "LOADER10", "LOADER2", "LOADER3", "LOADER4",
    "LOADER5", "LOADER6", "LOADER7", "LOADER76", "LOADER8A", "LOADER8B", "LOADER9",
    "DISK22", "OLDDISK1", "MENUBG", "MENUBG2", "CREDITS", "RAINBOW",
    "CHARSET", "MENUS", "MENUS2", "CJCGRAFS", "CJCTEAM1", "CJCTEAM2", "CJCTEAM3",
    "CJCTEAMG", "CJCBITS", "SOCCER_S", "CJCBENCH",
};

static int32_t LoadUnpacked(const char *dir, const char *name, SgBuf *out, char *err)
{
    char path[1024];
    SgBuf in;
    snprintf(path, sizeof path, "%s/%s.RAW", dir, name);
    if (Sg_ReadFile(path, &in, err))
        return -1;
    int32_t rc = Sg_RncUnpack(&in, out, err);
    Sg_BufFree(&in);
    return rc;
}

static uint32_t CountDiff(const SgBuf *a, const SgBuf *b)
{
    uint32_t n = a->size < b->size ? a->size : b->size, d = 0;
    for (uint32_t i = 0; i < n; i++)
        d += a->data[i] != b->data[i];
    return d + (a->size > b->size ? a->size - b->size : b->size - a->size);
}

static int32_t Report(const char *what, const char *name, const SgBuf *got, const SgBuf *want)
{
    uint32_t d = CountDiff(got, want);
    if (d == 0) {
        printf("  OK    %-9s %-9s %u bytes\n", what, name, (unsigned)got->size);
        return 0;
    }
    printf("  DIFF  %-9s %-9s %u of %u bytes differ (got %u)\n", what, name,
           (unsigned)d, (unsigned)want->size, (unsigned)got->size);
    return 1;
}

int main(int argc, char **argv)
{
    char err[256], path[1024];
    int32_t fails = 0;
    if (argc < 5) {
        fprintf(stderr, "usage: %s amigaDir grafsAgaDir menubg.bmp credits_icons.bmp [bmpOutDir]\n", argv[0]);
        return 2;
    }
    const char *amigaDir = argv[1], *agaDir = argv[2], *bmpOut = argc > 5 ? argv[5] : NULL;

    printf("Amiga-only pictures (original -> 256 colours -> AGA RAW):\n");
    for (size_t i = 0; i < sizeof k_pictures / sizeof k_pictures[0]; i++) {
        const char *name = k_pictures[i];
        SgBuf orig, want, got;
        SgImage img;
        snprintf(path, sizeof path, "%s/%s.RAW", amigaDir, name);
        if (Sg_ReadFile(path, &orig, err) || Sg_AmigaPictureTo256(name, &orig, &img, err) ||
            LoadUnpacked(agaDir, name, &want, err) || Sg_AmigaRawEncode(&img, 0, &got, err)) {
            printf("  FAIL  %s: %s\n", name, err);
            fails++;
            continue;
        }
        fails += Report("picture", name, &got, &want);
        if (bmpOut) {
            snprintf(path, sizeof path, "%s/%s.bmp", bmpOut, name);
            if (SgImage_WriteBmp(&img, path, err))
                printf("  FAIL  bmp %s: %s\n", name, err);
        }
        SgImage_Free(&img);
        Sg_BufFree(&orig); Sg_BufFree(&want); Sg_BufFree(&got);
    }

    printf("Round trip (decode, encode at the same depth):\n");
    for (size_t i = 0; i < sizeof k_roundTrip / sizeof k_roundTrip[0]; i++) {
        const char *name = k_roundTrip[i];
        SgBuf raw, got;
        SgImage img;
        int32_t w, h, planes = 4;
        if (LoadUnpacked(agaDir, name, &raw, err)) {
            printf("  skip  %s: %s\n", name, err);
            continue;
        }
        if (Sg_AmigaRawDimensions(raw.size, 4, &w, &h)) {
            planes = 8;
            if (Sg_AmigaRawDimensions(raw.size, 8, &w, &h)) {
                printf("  skip  %s: %u bytes is no known size\n", name, (unsigned)raw.size);
                Sg_BufFree(&raw);
                continue;
            }
        }
        if (Sg_AmigaRawDecode(&raw, w, h, planes, &img, err) || Sg_AmigaRawEncode(&img, planes, &got, err)) {
            printf("  FAIL  %s: %s\n", name, err);
            fails++;
        } else {
            char what[32];
            snprintf(what, sizeof what, "%dx%dx%d", (int)w, (int)h, (int)planes);
            fails += Report(what, name, &got, &raw);
            Sg_BufFree(&got);
            SgImage_Free(&img);
        }
        Sg_BufFree(&raw);
    }

    printf("CREDITS.RAW:\n");
    {
        SgImage bg, strip, bg2;
        SgBuf want, got, mraw;
        if (SgImage_ReadBmp(&bg, argv[3], err) || SgImage_ReadBmp(&strip, argv[4], err) ||
            LoadUnpacked(agaDir, "CREDITS", &want, err)) {
            printf("  FAIL  %s\n", err);
            return 1;
        }
        if (Sd_MakeCreditsAga(&bg, &strip, &got, err)) {
            printf("  FAIL  credits: %s\n", err);
            fails++;
        } else {
            fails += Report("from bmp", "CREDITS", &got, &want);
            Sg_BufFree(&got);
        }
        if (LoadUnpacked(agaDir, "MENUBG", &mraw, err) || Sg_AmigaRawDecode(&mraw, 352, 272, 8, &bg2, err) ||
            Sd_MakeCreditsAga(&bg2, &strip, &got, err)) {
            printf("  FAIL  credits from MENUBG.RAW: %s\n", err);
            fails++;
        } else {
            fails += Report("from RAW", "CREDITS", &got, &want);
            Sg_BufFree(&got);
        }
    }

    printf("%s (%d failures)\n", fails ? "FAILED" : "ALL OK", (int)fails);
    return fails ? 1 : 0;
}
