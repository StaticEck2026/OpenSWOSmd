/* rnc_pack_test -- for each RNC-packed file named on the command line:
 *
 *   identity:   unpack it, re-pack with its own method at SwosGfx's settings, and
 *               compare with the file byte for byte (reported, counted);
 *   round trip: Sg_RncUnpack(Sg_RncPack(x, m)) == x for m = 1 and 2, and the packed
 *               file smaller than x (a failure fails the run).
 *
 * Files that are not packed are skipped. Exit status 1 when a round trip failed.
 * With -q only mismatches and the summary are printed. */
#include "rnc_pack.h"
#include "sg_util.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    char err[256];
    int32_t files = 0, same = 0, differ = 0, rtOk = 0, rtFail = 0, quiet = 0, i, m;

    for (i = 1; i < argc; i++) {
        SgBuf in, un, re;
        if (!strcmp(argv[i], "-q")) { quiet = 1; continue; }
        if (Sg_ReadFile(argv[i], &in, err)) { printf("FAIL %s: %s\n", argv[i], err); rtFail++; continue; }
        if (!Sg_IsRnc(&in)) { Sg_BufFree(&in); continue; }
        files++;
        if (Sg_RncUnpack(&in, &un, err)) { printf("FAIL %s: unpack: %s\n", argv[i], err); rtFail++; Sg_BufFree(&in); continue; }

        /* identity at the file's own method */
        if (Sg_RncPack(&un, in.data[3], &re, err)) {
            printf("DIFF %s: repack failed: %s\n", argv[i], err);
            differ++;
        } else if (re.size == in.size && !memcmp(re.data, in.data, in.size)) {
            same++;
            if (!quiet) printf("SAME %s (m%d, %u -> %u)\n", argv[i], in.data[3], un.size, in.size);
        } else {
            uint32_t k, n = re.size < in.size ? re.size : in.size;
            for (k = 0; k < n && re.data[k] == in.data[k]; k++) {}
            printf("DIFF %s (m%d): ours %u bytes, shipped %u, first difference at %u;"
                   " leeway %u/%u chunks %u/%u\n", argv[i], in.data[3], re.size, in.size, k,
                   re.size > 16 ? re.data[16] : 0, in.data[16], re.size > 17 ? re.data[17] : 0, in.data[17]);
            differ++;
        }
        Sg_BufFree(&re);

        /* round trip, both methods */
        for (m = 1; m <= 2; m++) {
            SgBuf p, u;
            if (Sg_RncPack(&un, m, &p, err)) { printf("FAIL %s m%d: pack: %s\n", argv[i], m, err); rtFail++; continue; }
            if (p.size >= un.size) {
                printf("FAIL %s m%d: packed %u >= %u\n", argv[i], m, p.size, un.size);
                rtFail++;
            } else if (Sg_RncUnpack(&p, &u, err)) {
                printf("FAIL %s m%d: unpack: %s\n", argv[i], m, err);
                rtFail++;
            } else {
                if (u.size == un.size && !memcmp(u.data, un.data, un.size)) rtOk++;
                else { printf("FAIL %s m%d: round trip differs\n", argv[i], m); rtFail++; }
                Sg_BufFree(&u);
            }
            Sg_BufFree(&p);
        }
        Sg_BufFree(&un);
        Sg_BufFree(&in);
    }
    printf("%d packed files: %d identical to the shipped file, %d differ; round trips %d ok, %d failed\n",
           (int)files, (int)same, (int)differ, (int)rtOk, (int)rtFail);
    return rtFail ? 1 : 0;
}
