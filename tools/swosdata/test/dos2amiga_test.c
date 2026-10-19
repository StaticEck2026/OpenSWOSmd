/*
 * dos2amiga_test -- rebuild the Amiga banks from the PC game and write them.
 *
 *   dos2amiga_test <dosDir> <outDir> [glyphs.txt [refDir]]
 *
 * Writes <outDir>/<BANK>.bmp for all eleven banks; with glyphs.txt, CHARSET
 * gets the font extension first. With refDir (bin/grafs_aga), each bank is
 * also compared, index for index, with refDir/<BANK>.RAW: RNC-packed,
 * 320x256, row-interleaved planes (per line: plane 0's 40 bytes, plane 1's...,
 * plane 0 = the index's low bit), 4 or 8 planes by its size.
 */
#include "dos2amiga.h"
#include "font_extend.h"
#include "sg_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int32_t Compare(const SgImage *img, const char *refDir, const char *name)
{
    char rel[64], path[1024], err[256];
    SgBuf packed, raw;
    int32_t planes, diff = 0, fx = -1, fy = -1;
    const int32_t rowBytes = SG_BANK_W / 8;

    snprintf(rel, sizeof rel, "%s.RAW", name);
    if (Sg_FindNoCase(refDir, rel, path, sizeof path) != 0) {
        printf("%-9s no reference\n", name);
        return -1;
    }
    if (Sg_ReadFile(path, &packed, err) != 0 || Sg_RncUnpack(&packed, &raw, err) != 0) {
        printf("%-9s %s\n", name, err);
        return -1;
    }
    Sg_BufFree(&packed);
    planes = (int32_t)(raw.size / (rowBytes * SG_BANK_H));
    if ((planes != 4 && planes != 8) || raw.size != (uint32_t)(planes * rowBytes * SG_BANK_H)) {
        printf("%-9s reference is %u bytes: not a 320x256 bank\n", name, (unsigned)raw.size);
        Sg_BufFree(&raw);
        return -1;
    }
    for (int32_t y = 0; y < SG_BANK_H; y++)
        for (int32_t x = 0; x < SG_BANK_W; x++) {
            int32_t v = 0;
            for (int32_t pl = 0; pl < planes; pl++) {
                uint8_t b = raw.data[(y * planes + pl) * rowBytes + x / 8];
                v |= ((b >> (7 - (x & 7))) & 1) << pl;
            }
            if (v != img->px[y * SG_BANK_W + x]) {
                if (diff == 0) { fx = x; fy = y; }
                diff++;
            }
        }
    Sg_BufFree(&raw);
    if (diff == 0)
        printf("%-9s identical (%d planes)\n", name, (int)planes);
    else
        printf("%-9s %d pixels differ (%d planes), first at (%d,%d)\n", name, (int)diff, (int)planes, (int)fx, (int)fy);
    return diff ? 1 : 0;
}

int main(int argc, char **argv)
{
    SgImage banks[SG_BANK_COUNT];
    char err[256], path[1024];
    int32_t bad = 0;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <dosDir> <outDir> [glyphs.txt [refDir]]\n", argv[0]);
        return 2;
    }
    if (Sg_DosToAmigaBanks(argv[1], banks, err) != 0) {
        fprintf(stderr, "dos2amiga: %s\n", err);
        return 1;
    }
    if (argc > 3 && Sg_FontExtend(&banks[SG_BANK_CHARSET], argv[3], err) != 0) {
        fprintf(stderr, "font-extend: %s\n", err);
        return 1;
    }
    for (int32_t b = 0; b < SG_BANK_COUNT; b++) {
        snprintf(path, sizeof path, "%s/%s.bmp", argv[2], Sg_BankNames[b]);
        if (SgImage_WriteBmp(&banks[b], path, err) != 0) {
            fprintf(stderr, "%s\n", err);
            return 1;
        }
        if (argc > 4 && Compare(&banks[b], argv[4], Sg_BankNames[b]) != 0)
            bad = 1;
        SgImage_Free(&banks[b]);
    }
    return bad;
}
