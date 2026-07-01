/*
 * tpbcli-kernel-registry.c
 * Parse kernel_list.cmake.in and scan kernel source entry files.
 */

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef __linux__
#include <linux/limits.h>
#else
#include <limits.h>
#endif

#include "tpb-public.h"
#include "tpbcli-kernel-home.h"
#include "tpbcli-kernel-registry.h"

#define TPBCLI_KERNEL_REG_LIST_FILE  "src/kernels/kernel_list.cmake.in"
/* Installed per-kernel link overrides: NAME|EXTRA_LINK_LIBS rows */
#define TPBCLI_KERNEL_REG_LINK_FILE  "src/kernels/kernel_link_defs.txt"
#define TPBCLI_KERNEL_REG_PREFIX     "tpbk_"
#define TPBCLI_KERNEL_REG_SUFFIX     ".c"
#define TPBCLI_KERNEL_REG_SCAN_DEPTH 8

/* Local Function Prototypes */
static int _sf_add_entry(tpbcli_kernel_reg_list_t *list,
                         const char *name, const char *tags,
                         const char *path, int from_registry);
static int _sf_parse_registry_file(const char *path,
                                   tpbcli_kernel_reg_list_t *list);
static void _sf_scan_dir(const char *root, const char *rel,
                         tpbcli_kernel_reg_list_t *list, int depth);
static int _sf_tag_in_csv(const char *tags_csv, const char *tag);
static void _sf_trim(char *s);
static int _sf_name_from_source(const char *fname, char *name_out,
                                size_t name_len);

static void
_sf_trim(char *s)
{
    char *start;
    char *end;

    if (s == NULL) {
        return;
    }
    start = s;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        start++;
    }
    if (start != s) {
        memmove(s, start, strlen(start) + 1U);
    }
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) {
        end--;
    }
    *end = '\0';
}

static int
_sf_name_from_source(const char *fname, char *name_out, size_t name_len)
{
    size_t pre;
    size_t flen;
    size_t suf;

    if (fname == NULL || name_out == NULL || name_len == 0) {
        return TPBE_NULLPTR_ARG;
    }
    pre = strlen(TPBCLI_KERNEL_REG_PREFIX);
    suf = strlen(TPBCLI_KERNEL_REG_SUFFIX);
    flen = strlen(fname);
    if (flen <= pre + suf) {
        return TPBE_CLI_FAIL;
    }
    if (strncmp(fname, TPBCLI_KERNEL_REG_PREFIX, pre) != 0) {
        return TPBE_CLI_FAIL;
    }
    if (strcmp(fname + flen - suf, TPBCLI_KERNEL_REG_SUFFIX) != 0) {
        return TPBE_CLI_FAIL;
    }
    if (flen - pre - suf >= name_len) {
        return TPBE_FILE_IO_FAIL;
    }
    memcpy(name_out, fname + pre, flen - pre - suf);
    name_out[flen - pre - suf] = '\0';
    if (!tpbcli_kernel_name_valid(name_out)) {
        return TPBE_CLI_FAIL;
    }
    return TPBE_SUCCESS;
}

static int
_sf_add_entry(tpbcli_kernel_reg_list_t *list,
              const char *name, const char *tags,
              const char *path, int from_registry)
{
    int i;

    if (list == NULL || name == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    for (i = 0; i < list->count; i++) {
        if (strcmp(list->entries[i].name, name) == 0) {
            if (from_registry) {
                if (tags != NULL) {
                    snprintf(list->entries[i].tags,
                             sizeof(list->entries[i].tags), "%s", tags);
                }
                if (path != NULL && path[0] != '\0') {
                    snprintf(list->entries[i].path,
                             sizeof(list->entries[i].path), "%s", path);
                }
                list->entries[i].from_registry = 1;
            }
            return TPBE_SUCCESS;
        }
    }
    if (list->count >= TPBCLI_KERNEL_REG_MAX) {
        return TPBE_CLI_FAIL;
    }
    snprintf(list->entries[list->count].name,
             sizeof(list->entries[list->count].name), "%s", name);
    if (tags != NULL) {
        snprintf(list->entries[list->count].tags,
                 sizeof(list->entries[list->count].tags), "%s", tags);
    } else {
        list->entries[list->count].tags[0] = '\0';
    }
    if (path != NULL) {
        snprintf(list->entries[list->count].path,
                 sizeof(list->entries[list->count].path), "%s", path);
    } else {
        list->entries[list->count].path[0] = '\0';
    }
    list->entries[list->count].from_registry = from_registry;
    list->count++;
    return TPBE_SUCCESS;
}

static int
_sf_parse_registry_file(const char *path, tpbcli_kernel_reg_list_t *list)
{
    FILE *fp;
    char line[512];
    char name[TPBCLI_KERNEL_REG_NAME_MAX];
    char tags[TPBCLI_KERNEL_REG_TAGS_MAX];
    char path_rel[TPBCLI_KERNEL_REG_PATH_MAX];

    fp = fopen(path, "r");
    if (fp == NULL) {
        return TPBE_FILE_IO_FAIL;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *p;
        char *tok;

        _sf_trim(line);
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        p = line;
        tok = strsep(&p, "|");
        if (tok == NULL) {
            continue;
        }
        snprintf(name, sizeof(name), "%s", tok);
        _sf_trim(name);
        tok = strsep(&p, "|");
        if (tok == NULL) {
            continue;
        }
        snprintf(tags, sizeof(tags), "%s", tok);
        _sf_trim(tags);
        if (p == NULL) {
            continue;
        }
        snprintf(path_rel, sizeof(path_rel), "%s", p);
        _sf_trim(path_rel);
        if (name[0] == '\0' || path_rel[0] == '\0') {
            continue;
        }
        (void)_sf_add_entry(list, name, tags, path_rel, 1);
    }
    fclose(fp);
    return TPBE_SUCCESS;
}

static void
_sf_scan_dir(const char *root, const char *rel,
             tpbcli_kernel_reg_list_t *list, int depth)
{
    char dirpath[PATH_MAX];
    DIR *dp;
    struct dirent *de;
    char name[TPBCLI_KERNEL_REG_NAME_MAX];

    if (depth > TPBCLI_KERNEL_REG_SCAN_DEPTH) {
        return;
    }
    if (rel[0] == '\0') {
        snprintf(dirpath, sizeof(dirpath), "%s", root);
    } else {
        snprintf(dirpath, sizeof(dirpath), "%s/%s", root, rel);
    }
    dp = opendir(dirpath);
    if (dp == NULL) {
        return;
    }
    while ((de = readdir(dp)) != NULL) {
        char child_rel[PATH_MAX];

        if (de->d_name[0] == '.') {
            continue;
        }
        if (rel[0] == '\0') {
            snprintf(child_rel, sizeof(child_rel), "%s", de->d_name);
        } else {
            snprintf(child_rel, sizeof(child_rel), "%s/%s", rel, de->d_name);
        }
        if (de->d_type == DT_DIR || de->d_type == DT_UNKNOWN) {
            char subpath[PATH_MAX];
            struct stat st;

            snprintf(subpath, sizeof(subpath), "%s/%s", dirpath, de->d_name);
            if (stat(subpath, &st) == 0 && S_ISDIR(st.st_mode)) {
                _sf_scan_dir(root, child_rel, list, depth + 1);
            }
            continue;
        }
        if (_sf_name_from_source(de->d_name, name, sizeof(name)) !=
            TPBE_SUCCESS) {
            continue;
        }
        (void)_sf_add_entry(list, name, "", rel, 0);
    }
    closedir(dp);
}

static int
_sf_tag_in_csv(const char *tags_csv, const char *tag)
{
    char buf[TPBCLI_KERNEL_REG_TAGS_MAX];
    char *tokens[32];
    char scratch[TPBCLI_KERNEL_REG_TAGS_MAX];
    int n;
    int i;

    if (tags_csv == NULL || tag == NULL || tag[0] == '\0') {
        return 0;
    }
    snprintf(buf, sizeof(buf), "%s", tags_csv);
    n = tpbcli_kernel_reg_split_csv(buf, tokens, 32,
                                    scratch, sizeof(scratch));
    if (n < 0) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (strcmp(tokens[i], tag) == 0) {
            return 1;
        }
    }
    return 0;
}

int
tpbcli_kernel_reg_split_csv(const char *value,
                            char **tokens_out,
                            int tokens_max,
                            char *scratch,
                            size_t scratch_len)
{
    char *p;
    char *tok;
    int count;
    size_t vlen;
    char quote;

    if (value == NULL || tokens_out == NULL || scratch == NULL ||
        tokens_max <= 0 || scratch_len == 0) {
        return TPBE_NULLPTR_ARG;
    }
    vlen = strlen(value);
    if (vlen + 1U > scratch_len) {
        return TPBE_FILE_IO_FAIL;
    }
    memcpy(scratch, value, vlen + 1U);

    quote = '\0';
    if (vlen >= 2U) {
        if ((scratch[0] == '\'' && scratch[vlen - 1U] == '\'') ||
            (scratch[0] == '"' && scratch[vlen - 1U] == '"')) {
            quote = scratch[0];
            scratch[vlen - 1U] = '\0';
            memmove(scratch, scratch + 1, vlen - 1U);
        } else if (scratch[0] == '\'' || scratch[0] == '"' ||
                   scratch[vlen - 1U] == '\'' || scratch[vlen - 1U] == '"') {
            return TPBE_CLI_FAIL;
        }
    }
    (void)quote;

    count = 0;
    p = scratch;
    while ((tok = strsep(&p, ",")) != NULL) {
        _sf_trim(tok);
        if (tok[0] == '\0') {
            continue;
        }
        if (count >= tokens_max) {
            return TPBE_CLI_FAIL;
        }
        tokens_out[count++] = tok;
    }
    return count;
}

int
tpbcli_kernel_reg_load(const char *tpb_home, tpbcli_kernel_reg_list_t *out)
{
    char reg_path[PATH_MAX];
    char scan_root[PATH_MAX];
    int err;

    if (tpb_home == NULL || out == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    memset(out, 0, sizeof(*out));

    if (snprintf(reg_path, sizeof(reg_path), "%s/%s",
                 tpb_home, TPBCLI_KERNEL_REG_LIST_FILE) >= (int)sizeof(reg_path)) {
        return TPBE_FILE_IO_FAIL;
    }
    err = _sf_parse_registry_file(reg_path, out);
    if (err != TPBE_SUCCESS) {
        /* registry file optional for scan-only fallback */
    }

    if (snprintf(scan_root, sizeof(scan_root), "%s/src/kernels",
                 tpb_home) >= (int)sizeof(scan_root)) {
        return TPBE_FILE_IO_FAIL;
    }
    _sf_scan_dir(scan_root, "", out, 0);
    return TPBE_SUCCESS;
}

const tpbcli_kernel_reg_entry_t *
tpbcli_kernel_reg_find(const tpbcli_kernel_reg_list_t *list,
                       const char *name)
{
    int i;

    if (list == NULL || name == NULL) {
        return NULL;
    }
    for (i = 0; i < list->count; i++) {
        if (strcmp(list->entries[i].name, name) == 0) {
            return &list->entries[i];
        }
    }
    return NULL;
}

int
tpbcli_kernel_reg_expand_tags(const tpbcli_kernel_reg_list_t *list,
                              const char *tag_csv,
                              const char **names_out,
                              int names_max)
{
    char scratch[512];
    char *req_tags[32];
    char req_scratch[512];
    int nreq;
    int i;
    int j;
    int out_count;

    if (list == NULL || tag_csv == NULL || names_out == NULL ||
        names_max <= 0) {
        return TPBE_NULLPTR_ARG;
    }
    snprintf(scratch, sizeof(scratch), "%s", tag_csv);
    nreq = tpbcli_kernel_reg_split_csv(scratch, req_tags, 32,
                                       req_scratch, sizeof(req_scratch));
    if (nreq <= 0) {
        return TPBE_CLI_FAIL;
    }

    out_count = 0;
    for (i = 0; i < list->count; i++) {
        for (j = 0; j < nreq; j++) {
            if (_sf_tag_in_csv(list->entries[i].tags, req_tags[j])) {
                int k;
                int dup = 0;

                for (k = 0; k < out_count; k++) {
                    if (strcmp(names_out[k], list->entries[i].name) == 0) {
                        dup = 1;
                        break;
                    }
                }
                if (!dup && out_count < names_max) {
                    names_out[out_count++] = list->entries[i].name;
                }
                break;
            }
        }
    }
    return out_count;
}

void
tpbcli_kernel_reg_all_tags(const tpbcli_kernel_reg_list_t *list,
                           char *tags_out, size_t tags_len)
{
    char seen[TPBCLI_KERNEL_REG_MAX][64];
    int seen_count = 0;
    int i;

    if (tags_out == NULL || tags_len == 0) {
        return;
    }
    tags_out[0] = '\0';
    if (list == NULL) {
        return;
    }
    for (i = 0; i < list->count; i++) {
        char buf[TPBCLI_KERNEL_REG_TAGS_MAX];
        char *tokens[32];
        char scratch[TPBCLI_KERNEL_REG_TAGS_MAX];
        int n;
        int j;

        snprintf(buf, sizeof(buf), "%s", list->entries[i].tags);
        n = tpbcli_kernel_reg_split_csv(buf, tokens, 32,
                                        scratch, sizeof(scratch));
        for (j = 0; j < n; j++) {
            int k;
            int found = 0;

            for (k = 0; k < seen_count; k++) {
                if (strcmp(seen[k], tokens[j]) == 0) {
                    found = 1;
                    break;
                }
            }
            if (found || seen_count >= TPBCLI_KERNEL_REG_MAX) {
                continue;
            }
            snprintf(seen[seen_count], sizeof(seen[seen_count]), "%s",
                     tokens[j]);
            seen_count++;
            if (tags_out[0] != '\0') {
                strncat(tags_out, ", ", tags_len - strlen(tags_out) - 1U);
            }
            strncat(tags_out, tokens[j], tags_len - strlen(tags_out) - 1U);
        }
    }
}

/**
 * @brief Look up extra link libraries for a kernel from kernel_link_defs.txt.
 */
int
tpbcli_kernel_reg_link_libs(const char *tpb_home,
                            const char *name,
                            char *out,
                            size_t outlen)
{
    char path[PATH_MAX];
    FILE *fp;
    char line[256];
    char kname[TPBCLI_KERNEL_REG_NAME_MAX];
    char libs[TPBCLI_KERNEL_REG_LINK_MAX];

    if (tpb_home == NULL || name == NULL || out == NULL || outlen == 0) {
        return TPBE_NULLPTR_ARG;
    }
    out[0] = '\0';

    if (snprintf(path, sizeof(path), "%s/%s",
                 tpb_home, TPBCLI_KERNEL_REG_LINK_FILE) >= (int)sizeof(path)) {
        return TPBE_FILE_IO_FAIL;
    }
    fp = fopen(path, "r");
    if (fp == NULL) {
        /* Missing file is OK: most kernels have no extra link libraries */
        return TPBE_SUCCESS;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *p;
        char *tok;

        _sf_trim(line);
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        p = line;
        tok = strsep(&p, "|");
        if (tok == NULL) {
            continue;
        }
        snprintf(kname, sizeof(kname), "%s", tok);
        _sf_trim(kname);
        if (p == NULL) {
            continue;
        }
        snprintf(libs, sizeof(libs), "%s", p);
        _sf_trim(libs);
        if (strcmp(kname, name) == 0) {
            snprintf(out, outlen, "%s", libs);
            fclose(fp);
            return TPBE_SUCCESS;
        }
    }
    fclose(fp);
    return TPBE_SUCCESS;
}
