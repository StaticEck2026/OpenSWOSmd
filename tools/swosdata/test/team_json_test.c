/* team_json_test -- convert every team database file of <in dir> (TEAM.nnn, *.TMD,
 * POOLPLYR.DAT; packed or not) to <out dir>/<name>.json. With a third argument,
 * compare each with the file of the same name there (bin/data_json/teams).
 * Exit 1 if a conversion failed or a compared file differs. */
#include "team_json.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cmp_names(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

int main(int argc, char **argv)
{
    char err[256], path[1024], name[128];
    char *names[1024];
    int n = 0, i, fails = 0, same = 0, differ = 0, unmatched = 0;
    DIR *d;
    struct dirent *e;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <in dir> <out dir> [reference dir]\n", argv[0]);
        return 2;
    }
    if (!(d = opendir(argv[1]))) {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    while ((e = readdir(d)) && n < 1024)
        if (Sd_TeamFileType(e->d_name) != SD_TEAMFILE_NONE) {
            names[n] = (char *)malloc(strlen(e->d_name) + 1);
            strcpy(names[n++], e->d_name);
        }
    closedir(d);
    qsort(names, (size_t)n, sizeof names[0], cmp_names);

    for (i = 0; i < n; i++) {
        SdBuf raw, json, ref;
        Sd_PathJoin(path, sizeof path, argv[1], names[i]);
        Sd_TeamJsonName(names[i], name, sizeof name);
        if (Sd_ReadFile(path, &raw, err) || Sd_TeamFileToJson(names[i], &raw, &json, err)) {
            printf("FAIL %-14s %s\n", names[i], err);
            fails++;
            continue;
        }
        Sd_BufFree(&raw);
        Sd_PathJoin(path, sizeof path, argv[2], name);
        if (Sd_WriteFile(path, json.data, json.size, err)) {
            printf("FAIL %-14s %s\n", names[i], err);
            fails++;
        }
        if (argc > 3) {
            Sd_PathJoin(path, sizeof path, argv[3], name);
            if (Sd_ReadFile(path, &ref, err)) {
                printf("  %-14s -> %-24s no reference\n", names[i], name);
                unmatched++;
            } else {
                int ok = ref.size == json.size && !memcmp(ref.data, json.data, json.size);
                printf("  %-14s -> %-24s %s\n", names[i], name, ok ? "identical" : "DIFFERS");
                if (ok) same++; else differ++;
                Sd_BufFree(&ref);
            }
        }
        Sd_BufFree(&json);
        free(names[i]);
    }
    printf("%d files: %d failed", n, fails);
    if (argc > 3)
        printf(", %d identical, %d differ, %d with no reference", same, differ, unmatched);
    printf("\n");
    return (fails || differ) ? 1 : 0;
}
