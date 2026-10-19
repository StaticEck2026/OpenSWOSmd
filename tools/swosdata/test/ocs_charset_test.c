/* ocs_charset_test <original CHARSET.RAW> <glyphs.txt> <shipped grafs/CHARSET.RAW>
 * Is the shipped OCS charset the original one with -font-extend applied? */
#include "amiga_raw.h"
#include "font_extend.h"
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv)
{
    char err[256];
    SdBuf in, un, ref, refu, out;
    SdImage img;
    if (argc != 4) return 2;
    if (Sd_ReadFile(argv[1], &in, err) || Sd_RncUnpack(&in, &un, err) ||
        Sd_AmigaRawDecode(&un, 320, 256, 4, &img, err) || Sd_FontExtend(&img, argv[2], err) ||
        Sd_AmigaRawEncode(&img, 4, &out, err) || Sd_ReadFile(argv[3], &ref, err) || Sd_RncUnpack(&ref, &refu, err)) {
        printf("FAIL %s\n", err);
        return 1;
    }
    uint32_t diff = 0;
    for (uint32_t i = 0; i < out.size && i < refu.size; i++) diff += out.data[i] != refu.data[i];
    printf("%u vs %u bytes, %u differ\n", out.size, refu.size, diff);
    return diff != 0 || out.size != refu.size;
}
