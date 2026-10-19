/*
 * swosdata -- build OpenSWOS's game data from the player's own copy of the game,
 * from the command line. The setup app does the same through the library.
 *
 *   swosdata [--scan DIR]... [--adf FILE]... [--dos DIR] [--jag FILE]
 *            --out DIR [--res DIR]
 *   swosdata --compare OUT REF     every file in OUT equals REF's (RNC unpacked)
 */
#include "swosdata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#endif

static void on_log(void *user, const char *line)
{
    (void)user;
    printf("%s\n", line);
}

static int32_t usage(void)
{
    fprintf(stderr,
            "usage: swosdata [--scan DIR]... [--adf FILE]... [--dos DIR] [--jag FILE]\n"
            "                --out DIR [--res DIR]\n"
            "       swosdata --compare OUT REF\n");
    return 2;
}

#ifndef _WIN32
/* Compare every file under `out` with the same path under `ref`, both unpacked. */
static int32_t compare_tree(const char *out, const char *ref, const char *rel, int32_t *same, int32_t *diff)
{
    char dir[1100];
    Sd_PathJoin(dir, sizeof dir, out, rel);
    DIR *d = opendir(dir);
    if (!d)
        return -1;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.')
            continue;
        char sub[1100], po[1100], pr[1100];
        Sd_PathJoin(sub, sizeof sub, rel, e->d_name);
        Sd_PathJoin(po, sizeof po, out, sub);
        struct stat st;
        if (stat(po, &st) == 0 && S_ISDIR(st.st_mode)) {
            compare_tree(out, ref, sub, same, diff);
            continue;
        }
        Sd_PathJoin(pr, sizeof pr, ref, sub);
        char err[256];
        SdBuf a, b, ua, ub;
        if (Sd_ReadFile(po, &a, err) != 0 || Sd_ReadFile(pr, &b, err) != 0) {
            printf("MISSING %s\n", sub);
            (*diff)++;
            continue;
        }
        if (Sd_RncUnpack(&a, &ua, err) != 0 || Sd_RncUnpack(&b, &ub, err) != 0) {
            printf("BAD %s: %s\n", sub, err);
            (*diff)++;
        } else if (ua.size == ub.size && memcmp(ua.data, ub.data, ua.size) == 0) {
            (*same)++;
        } else {
            printf("DIFFERS %s (%u vs %u bytes unpacked)\n", sub, ua.size, ub.size);
            (*diff)++;
        }
        Sd_BufFree(&a);
        Sd_BufFree(&b);
        Sd_BufFree(&ua);
        Sd_BufFree(&ub);
    }
    closedir(d);
    return 0;
}
#endif

int main(int argc, char **argv)
{
    static SdJob job;
    char err[256];

    if (argc == 4 && strcmp(argv[1], "--compare") == 0) {
#ifdef _WIN32
        fprintf(stderr, "--compare is not built on Windows\n");
        return 2;
#else
        int32_t same = 0, diff = 0;
        compare_tree(argv[2], argv[3], "", &same, &diff);
        printf("%d identical, %d different\n", same, diff);
        return diff ? 1 : 0;
#endif
    }

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (!v)
            return usage();
        if (strcmp(a, "--scan") == 0)
            SdSources_Scan(&job.src, v);
        else if (strcmp(a, "--adf") == 0 || strcmp(a, "--jag") == 0) {
            if (SdSources_AddFile(&job.src, v) != 0) {
                fprintf(stderr, "not recognised: %s\n", v);
                return 1;
            }
        } else if (strcmp(a, "--dos") == 0) {
            if (SdSources_AddFile(&job.src, v) != 0) {
                fprintf(stderr, "not a PC Sensible World of Soccer folder: %s\n", v);
                return 1;
            }
        } else if (strcmp(a, "--out") == 0)
            snprintf(job.outDir, sizeof job.outDir, "%s", v);
        else if (strcmp(a, "--res") == 0)
            snprintf(job.resDir, sizeof job.resDir, "%s", v);
        else
            return usage();
        i++;
    }
    if (!job.outDir[0])
        return usage();

    printf("Amiga disk 1: %s\n", job.src.adf1[0] ? job.src.adf1 : "(not found)");
    printf("Amiga disk 2: %s\n", job.src.adf2[0] ? job.src.adf2 : "(not found)");
    printf("PC game:      %s\n", job.src.dosDir[0] ? job.src.dosDir : "(not found)");
    printf("Jaguar:       %s\n", job.src.jagRom[0] ? job.src.jagRom : "(not found)");

    job.log = on_log;
    if (SwosData_Run(&job, err) != 0) {
        fprintf(stderr, "failed: %s\n", err);
        return 1;
    }
    return 0;
}
