/* rnc_test -- unpack each file named on the command line to <file>.unp. */
#include "sd_util.h"
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv)
{
    char err[256], out[1024];
    for (int i = 1; i < argc; i++) {
        SdBuf in, un;
        if (Sd_ReadFile(argv[i], &in, err) || Sd_RncUnpack(&in, &un, err)) { printf("FAIL %s: %s\n", argv[i], err); return 1; }
        snprintf(out, sizeof out, "%s.unp", argv[i]);
        Sd_WriteFile(out, un.data, un.size, err);
        Sd_BufFree(&in); Sd_BufFree(&un);
    }
    return 0;
}
