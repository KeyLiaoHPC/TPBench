/*
 * tpb-raf-scan.c
 * rafdb record path resolution and ID prefix scanning.
 */

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../../include/tpb-public.h"
#include "tpb-raf-types.h"

/* Local Function Prototypes */

static int _sf_cmp_id_match(const void *a, const void *b);

/* Local Function Implementations */

static int
_sf_cmp_id_match(const void *a, const void *b)
{
    const tpb_raf_id_match_t *x = (const tpb_raf_id_match_t *)a;
    const tpb_raf_id_match_t *y = (const tpb_raf_id_match_t *)b;
    int c;

    c = strcmp(x->id_hex, y->id_hex);
    if (c != 0) {
        return c;
    }
    return (int)x->domain - (int)y->domain;
}

/**
 * @brief Resolve a user file path to an existing rafdb record file.
 */
int
tpb_raf_resolve_record_file(const char *workspace, const char *inpath,
                            char *resolved, size_t resolved_cap)
{
    struct stat st;
    const char *base;
    char trybuf[TPB_RAF_PATH_MAX];

    if (workspace == NULL || inpath == NULL || resolved == NULL ||
        resolved_cap == 0) {
        return TPBE_NULLPTR_ARG;
    }

    if (stat(inpath, &st) == 0 && S_ISREG(st.st_mode)) {
        snprintf(resolved, resolved_cap, "%s", inpath);
        return TPBE_SUCCESS;
    }

    base = strrchr(inpath, '/');
    base = base ? base + 1 : inpath;

    snprintf(trybuf, sizeof(trybuf), "%s/%s/%s",
             workspace, TPB_RAF_TBATCH_DIR, base);
    if (stat(trybuf, &st) == 0 && S_ISREG(st.st_mode)) {
        snprintf(resolved, resolved_cap, "%s", trybuf);
        return TPBE_SUCCESS;
    }
    snprintf(trybuf, sizeof(trybuf), "%s/%s/%s",
             workspace, TPB_RAF_KERNEL_DIR, base);
    if (stat(trybuf, &st) == 0 && S_ISREG(st.st_mode)) {
        snprintf(resolved, resolved_cap, "%s", trybuf);
        return TPBE_SUCCESS;
    }
    snprintf(trybuf, sizeof(trybuf), "%s/%s/%s",
             workspace, TPB_RAF_TASK_DIR, base);
    if (stat(trybuf, &st) == 0 && S_ISREG(st.st_mode)) {
        snprintf(resolved, resolved_cap, "%s", trybuf);
        return TPBE_SUCCESS;
    }

    return TPBE_FILE_IO_FAIL;
}

/**
 * @brief Scan .tpbr files whose record ID hex starts with a prefix.
 */
int
tpb_raf_scan_records_by_id_prefix(const char *workspace,
                                  const char *id_hex_prefix,
                                  size_t prefix_len,
                                  uint8_t domain_filter,
                                  tpb_raf_id_match_t **matches_out,
                                  int *nmatch_out)
{
    static const struct {
        const char *reldir;
        uint8_t     domain;
    } dirs[3] = {
        { TPB_RAF_TBATCH_DIR, TPB_RAF_DOM_TBATCH },
        { TPB_RAF_KERNEL_DIR, TPB_RAF_DOM_KERNEL },
        { TPB_RAF_TASK_DIR,   TPB_RAF_DOM_TASK },
    };
    char dirpath[TPB_RAF_PATH_MAX];
    tpb_raf_id_match_t *matches = NULL;
    int n = 0;
    int cap = 0;
    size_t di;

    if (workspace == NULL || id_hex_prefix == NULL || matches_out == NULL ||
        nmatch_out == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    *matches_out = NULL;
    *nmatch_out = 0;

    for (di = 0; di < sizeof(dirs) / sizeof(dirs[0]); di++) {
        DIR *dp;
        struct dirent *ent;

        if (domain_filter != TPB_RAF_DOM_ALL &&
            domain_filter != dirs[di].domain) {
            continue;
        }
        snprintf(dirpath, sizeof(dirpath), "%s/%s",
                 workspace, dirs[di].reldir);
        dp = opendir(dirpath);
        if (!dp) {
            continue;
        }
        while ((ent = readdir(dp)) != NULL) {
            const char *name = ent->d_name;
            size_t len = strlen(name);
            char idpart[41];
            size_t k;

            if (len != 45 || strcmp(name + 40, ".tpbr") != 0) {
                continue;
            }
            memcpy(idpart, name, 40);
            idpart[40] = '\0';
            for (k = 0; k < 40; k++) {
                if (!isxdigit((unsigned char)idpart[k])) {
                    break;
                }
                idpart[k] = (char)tolower((unsigned char)idpart[k]);
            }
            if (k != 40 || strncmp(idpart, id_hex_prefix, prefix_len) != 0) {
                continue;
            }
            if (n >= cap) {
                int nc = cap ? cap * 2 : 16;
                tpb_raf_id_match_t *nm = (tpb_raf_id_match_t *)realloc(
                    matches, (size_t)nc * sizeof(*matches));
                if (!nm) {
                    closedir(dp);
                    free(matches);
                    return TPBE_MALLOC_FAIL;
                }
                matches = nm;
                cap = nc;
            }
            snprintf(matches[n].id_hex, sizeof(matches[n].id_hex),
                     "%s", idpart);
            matches[n].domain = dirs[di].domain;
            n++;
        }
        closedir(dp);
    }

    if (n > 1) {
        qsort(matches, (size_t)n, sizeof(*matches), _sf_cmp_id_match);
    }
    *matches_out = matches;
    *nmatch_out = n;
    return TPBE_SUCCESS;
}

/**
 * @brief Free array returned by tpb_raf_scan_records_by_id_prefix().
 */
void
tpb_raf_free_id_matches(tpb_raf_id_match_t *matches)
{
    free(matches);
}
