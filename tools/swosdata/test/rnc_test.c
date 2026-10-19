/* rnc_test -- unpack each file named on the command line to <file>.unp. */
#include "sg_util.h"
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv)
{
    char err[256], out[1024];
    for (int i = 1; i < argc; i++) {
        SgBuf in, un;
        if (Sg_ReadFile(argv[i], &in, err) || Sg_RncUnpack(&in, &un, err)) { printf("FAIL %s: %s\n", argv[i], err); return 1; }
        snprintf(out, sizeof out, "%s.unp", argv[i]);
        Sg_WriteFile(out, un.data, un.size, err);
        Sg_BufFree(&in); Sg_BufFree(&un);
    }
    return 0;
}
