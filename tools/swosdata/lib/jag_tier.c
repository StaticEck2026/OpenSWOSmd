/*
 * jag_tier.c -- the game's Jaguar sprite tier; see jag_tier.h.
 */
#include "jag_tier.h"

#include "amiga_raw.h"
#include "sg_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* make_jag_graphics.py KIT_BOXES: (x, y, w, h) in the CJCGRAFS sheet, drop
 * shadow included. A shirt and an icon per shirt type (0 ordinary, 1 coloured
 * sleeves, 2 vertical stripes, 3 hoops), the shorts, the two socks. */
static const int32_t kKitBoxes[][4] = {
    { 209, 63, 44, 40 }, { 257,  1, 44, 40 }, { 161,  1, 44, 41 }, { 209,  1, 44, 40 },
    { 161, 42, 21, 20 }, { 257, 42, 21, 20 }, { 193, 42, 21, 20 }, { 225, 42, 21, 20 },
    { 289, 42, 28, 22 }, { 161, 63, 14, 25 }, { 183, 63, 15, 25 },
};
#define NUM_KIT_BOXES ((int32_t)(sizeof kKitBoxes / sizeof kKitBoxes[0]))

static int32_t MakeKits(const SgImage *sheet, SgBuf *out, char *err)
{
    uint32_t size = 4 + 768 + 1, o;

    for (int32_t i = 0; i < NUM_KIT_BOXES; i++)
        size += 2 + (uint32_t)(kKitBoxes[i][2] * kKitBoxes[i][3]);
    out->data = malloc(size);
    if (!out->data) {
        snprintf(err, 256, "out of memory");
        return -1;
    }
    out->size = size;
    memcpy(out->data, "JGK1", 4);
    memcpy(out->data + 4, sheet->pal, 768);
    out->data[772] = (uint8_t)NUM_KIT_BOXES;
    o = 773;
    for (int32_t i = 0; i < NUM_KIT_BOXES; i++) {
        int32_t x = kKitBoxes[i][0], y = kKitBoxes[i][1], w = kKitBoxes[i][2], h = kKitBoxes[i][3];
        out->data[o++] = (uint8_t)w;
        out->data[o++] = (uint8_t)h;
        for (int32_t r = 0; r < h; r++, o += (uint32_t)w)
            memcpy(out->data + o, sheet->px + (y + r) * sheet->w + x, (size_t)w);
    }
    return 0;
}

void Sd_JagFilesFree(SdJagFile out[SD_JAG_FILE_COUNT])
{
    for (int32_t i = 0; i < SD_JAG_FILE_COUNT; i++)
        Sg_BufFree(&out[i].data);
}

int32_t Sd_JagBuild(const char *romPath, const char *dosDir, const char *setupDir,
                    SdJagFile out[SD_JAG_FILE_COUNT], uint32_t *celebrations, char *err)
{
    SgImage banks[SG_JAG_BANK_COUNT], grafs;
    SgBuf rom;
    uint8_t used[256];
    int32_t rc = -1;

    memset(out, 0, sizeof(SdJagFile) * SD_JAG_FILE_COUNT);
    if (Sg_ReadFile(romPath, &rom, err) != 0)
        return -1;
    if (rom.size != SG_JAG_ROM_SIZE) {
        snprintf(err, 256, "%s is %u bytes; the cartridge image is %u",
                 romPath, (unsigned)rom.size, (unsigned)SG_JAG_ROM_SIZE);
        Sg_BufFree(&rom);
        return -1;
    }
    if (Sg_JagBanks(&rom, dosDir, setupDir, banks, &grafs, celebrations, err) != 0) {
        Sg_BufFree(&rom);
        return -1;
    }
    Sg_BufFree(&rom);

    memset(used, 0, sizeof used);
    for (int32_t b = 0; b < SG_JAG_BANK_COUNT; b++) {
        snprintf(out[b].name, sizeof out[b].name, "%s.RAW", Sg_JagBankNames[b]);
        for (int32_t i = 0; i < (320 * 256); i++)
            used[banks[b].px[i]] = 1;
        if (Sg_AmigaRawEncode(&banks[b], 8, &out[b].data, err) != 0)
            goto done;
    }

    /* JAG.PAL: the palette and which of it the art uses (all six sheets share it). */
    snprintf(out[6].name, sizeof out[6].name, "JAG.PAL");
    out[6].data.size = 4 + 768 + 256;
    out[6].data.data = malloc(out[6].data.size);
    if (!out[6].data.data) {
        snprintf(err, 256, "out of memory");
        goto done;
    }
    memcpy(out[6].data.data, "JGP1", 4);
    memcpy(out[6].data.data + 4, banks[0].pal, 768);
    memcpy(out[6].data.data + 772, used, 256);

    snprintf(out[7].name, sizeof out[7].name, "JAGKITS.BIN");
    if (MakeKits(&grafs, &out[7].data, err) != 0)
        goto done;
    rc = 0;
done:
    for (int32_t b = 0; b < SG_JAG_BANK_COUNT; b++)
        SgImage_Free(&banks[b]);
    SgImage_Free(&grafs);
    if (rc != 0)
        Sd_JagFilesFree(out);
    return rc;
}
