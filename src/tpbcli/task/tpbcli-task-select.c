/*
 * tpbcli-task-select.c
 * RIDMAP, capsule confirmation, and shared selection helpers for task CLI.
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unistd.h>
#ifdef __linux__
#include <linux/limits.h>
#endif

#include "tpb-public.h"
#include "tpbcli-task-internal.h"

/* Local Function Prototypes */
static int _sf_ensure_tmp_dir(const char *workspace);
static int _sf_is_all_ff(const unsigned char id[20]);
static int _sf_is_all_zero(const unsigned char id[20]);

static int
_sf_ensure_tmp_dir(const char *workspace)
{
    char path[PATH_MAX];
    int n;

    if (workspace == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    n = snprintf(path, sizeof(path), "%s/.tmp", workspace);
    if (n < 0 || (size_t)n >= sizeof(path)) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    if (mkdir(path, 0700) != 0 && errno != EEXIST) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: cannot create '%s'\n", path);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_FILE_IO_FAIL, NULL);
    }
    return TPBE_SUCCESS;
}

static int
_sf_is_all_ff(const unsigned char id[20])
{
    int i;

    for (i = 0; i < 20; i++) {
        if (id[i] != 0xFFu) {
            return 0;
        }
    }
    return 1;
}

static int
_sf_is_all_zero(const unsigned char id[20])
{
    static const unsigned char z20[20] = {0};

    return memcmp(id, z20, 20) == 0;
}

void
tpbcli_task_format_id_prefix6(const unsigned char id[20], char *out,
                              size_t outlen)
{
    char hex[41];

    if (out == NULL || outlen < 8) {
        return;
    }
    tpb_raf_id_to_hex(id, hex);
    snprintf(out, outlen, "%.6s*", hex);
}

int
tpbcli_task_ridmap_path(const char *workspace, char *out, size_t outlen)
{
    int n;

    if (workspace == NULL || out == NULL || outlen == 0) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    n = snprintf(out, outlen, "%s/.tmp/%s", workspace, TPBCLI_TASK_RIDMAP_NAME);
    if (n < 0 || (size_t)n >= outlen) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    return TPBE_SUCCESS;
}

int
tpbcli_task_ridmap_write_atomic(const char *workspace,
                                const unsigned char (*ids)[20],
                                int nids)
{
    char final_path[PATH_MAX];
    char tmp_path[PATH_MAX];
    FILE *fp = NULL;
    int fd = -1;
    int i;
    int err;
    int n;

    if (workspace == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    if (nids < 0 || (nids > 0 && ids == NULL)) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    if (nids == 0) {
        return TPBE_SUCCESS;
    }

    err = _sf_ensure_tmp_dir(workspace);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    err = tpbcli_task_ridmap_path(workspace, final_path, sizeof(final_path));
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%d", final_path,
                 (int)getpid());
    if (n < 0 || (size_t)n >= sizeof(tmp_path)) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }

    fd = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: cannot create RIDMAP temp file\n");
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_FILE_IO_FAIL, NULL);
    }
    fp = fdopen(fd, "w");
    if (fp == NULL) {
        close(fd);
        unlink(tmp_path);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_FILE_IO_FAIL, NULL);
    }
    fd = -1;

    for (i = 0; i < nids; i++) {
        char hex[41];

        tpb_raf_id_to_hex(ids[i], hex);
        if (fprintf(fp, "%s\n", hex) < 0) {
            fclose(fp);
            unlink(tmp_path);
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_FILE_IO_FAIL, NULL);
        }
    }
    if (fflush(fp) != 0 || fsync(fileno(fp)) != 0) {
        fclose(fp);
        unlink(tmp_path);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_FILE_IO_FAIL, NULL);
    }
    if (fclose(fp) != 0) {
        unlink(tmp_path);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_FILE_IO_FAIL, NULL);
    }
    if (rename(tmp_path, final_path) != 0) {
        unlink(tmp_path);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_FILE_IO_FAIL, NULL);
    }
    return TPBE_SUCCESS;
}

int
tpbcli_task_ridmap_read(const char *workspace,
                        unsigned char (**ids_out)[20],
                        int *nids_out)
{
    char path[PATH_MAX];
    FILE *fp;
    char line[128];
    unsigned char (*ids)[20] = NULL;
    int n = 0;
    int cap = 0;
    int err;

    if (ids_out == NULL || nids_out == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    *ids_out = NULL;
    *nids_out = 0;

    err = tpbcli_task_ridmap_path(workspace, path, sizeof(path));
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    fp = fopen(path, "r");
    if (fp == NULL) {
        if (errno == ENOENT) {
            return TPBE_SUCCESS;
        }
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_FILE_IO_FAIL, NULL);
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *p = line;
        size_t len;
        unsigned char id[20];
        unsigned char (*grown)[20];

        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0' || *p == '\n' || *p == '#') {
            continue;
        }
        len = strlen(p);
        while (len > 0 &&
               (p[len - 1] == '\n' || p[len - 1] == '\r' ||
                p[len - 1] == ' ' || p[len - 1] == '\t')) {
            p[--len] = '\0';
        }
        if (len != 40 || tpb_raf_hex_to_id(p, id) != 0) {
            fclose(fp);
            free(ids);
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT,
                            "error: invalid RIDMAP line '%s'\n", p);
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        if (n == cap) {
            int ncap = (cap == 0) ? 16 : cap * 2;

            if (ncap < cap) {
                fclose(fp);
                free(ids);
                TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
            }
            grown = (unsigned char (*)[20])realloc(
                ids, (size_t)ncap * sizeof(*grown));
            if (grown == NULL) {
                fclose(fp);
                free(ids);
                TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
            }
            ids = grown;
            cap = ncap;
        }
        memcpy(ids[n], id, 20);
        n++;
    }
    fclose(fp);
    *ids_out = ids;
    *nids_out = n;
    return TPBE_SUCCESS;
}

int
tpbcli_task_confirm_capsule(const task_attr_t *attr, const void *data,
                            uint64_t datasize)
{
    const tpb_meta_header_t *h;
    uint64_t nbytes = 0;
    const void *ptr = NULL;
    int err;

    if (attr == NULL) {
        return 0;
    }
    if (!_sf_is_all_ff(attr->inherit_from) || !_sf_is_all_zero(attr->derive_to)) {
        return 0;
    }
    if (attr->ninput != 0 || attr->noutput != 0 || attr->nheader != 1 ||
        attr->headers == NULL) {
        return 0;
    }
    h = &attr->headers[0];
    if (strcmp(h->name, "TaskID") != 0 ||
        strcmp(h->tag, TPB_TAG_LINK) != 0) {
        return 0;
    }
    if (datasize % 20u != 0) {
        return 0;
    }
    if (h->dimsizes[0] != datasize / 20u) {
        return 0;
    }
    err = tpb_raf_header_data_ptr(attr->headers, attr->nheader, data, datasize,
                                  0, &ptr, &nbytes);
    if (err != TPBE_SUCCESS || ptr == NULL || nbytes != datasize) {
        return 0;
    }
    return 1;
}

int
tpbcli_task_read_capsule_members(const task_attr_t *attr, const void *data,
                                 uint64_t datasize,
                                 unsigned char (**ids_out)[20],
                                 int *nmembers_out)
{
    const void *ptr = NULL;
    uint64_t nbytes = 0;
    unsigned char (*ids)[20] = NULL;
    int n;
    int i;
    int err;

    if (ids_out == NULL || nmembers_out == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    *ids_out = NULL;
    *nmembers_out = 0;
    if (!tpbcli_task_confirm_capsule(attr, data, datasize)) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    err = tpb_raf_header_data_ptr(attr->headers, attr->nheader, data, datasize,
                                  0, &ptr, &nbytes);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    n = (int)(nbytes / 20u);
    if (n < 0 || (uint64_t)n * 20u != nbytes) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    if (n == 0) {
        return TPBE_SUCCESS;
    }
    ids = (unsigned char (*)[20])calloc((size_t)n, sizeof(*ids));
    if (ids == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
    }
    for (i = 0; i < n; i++) {
        memcpy(ids[i], (const unsigned char *)ptr + (size_t)i * 20u, 20);
    }
    *ids_out = ids;
    *nmembers_out = n;
    return TPBE_SUCCESS;
}

static int
_sf_is_all_zero20(const unsigned char id[20])
{
    static const unsigned char z[20] = {0};

    return memcmp(id, z, 20) == 0;
}

static int
_sf_hex_prefix_valid(const char *prefix, size_t *len_out)
{
    size_t n;
    size_t i;

    if (prefix == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    n = strlen(prefix);
    if (n < 6 || n > 20) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: task ID prefix must be 6-20 hex digits\n");
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    for (i = 0; i < n; i++) {
        char c = prefix[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT,
                            "error: invalid hex in task ID prefix '%s'\n",
                            prefix);
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
    }
    if (len_out != NULL) {
        *len_out = n;
    }
    return TPBE_SUCCESS;
}

static int
_sf_append_name_unique(char ***names, int *n, int *cap, const char *name)
{
    char **arr;
    int i;

    if (name == NULL || name[0] == '\0') {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: empty name in name list\n");
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    for (i = 0; i < *n; i++) {
        if (strcmp((*names)[i], name) == 0) {
            return TPBE_SUCCESS;
        }
    }
    if (*n >= *cap) {
        int ncap = (*cap == 0) ? 8 : *cap * 2;
        arr = (char **)realloc(*names, (size_t)ncap * sizeof(*arr));
        if (arr == NULL) {
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
        }
        *names = arr;
        *cap = ncap;
    }
    (*names)[*n] = strdup(name);
    if ((*names)[*n] == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
    }
    (*n)++;
    return TPBE_SUCCESS;
}

int
tpbcli_task_parse_name_csv(const char *text, char ***names_out, int *nnames_out)
{
    const char *p;
    char **names = NULL;
    int n = 0;
    int cap = 0;
    int err;

    if (names_out == NULL || nnames_out == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    *names_out = NULL;
    *nnames_out = 0;
    if (text == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }

    p = text;
    while (*p != '\0') {
        char token[TPBM_NAME_STR_MAX_LEN];
        size_t ti = 0;

        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (*p == ',') {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT,
                            "error: empty entry in name list\n");
            goto fail;
        }
        if (*p == '"') {
            p++;
            while (*p != '\0') {
                if (*p == '"') {
                    if (p[1] == '"') {
                        if (ti + 1 >= sizeof(token)) {
                            goto token_too_long;
                        }
                        token[ti++] = '"';
                        p += 2;
                        continue;
                    }
                    p++;
                    if (*p != '\0' && *p != ',') {
                        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                                        TPBLOG_FLAG_DIRECT,
                                        "error: trailing characters after "
                                        "quoted name\n");
                        goto fail;
                    }
                    break;
                }
                if (ti + 1 >= sizeof(token)) {
                    goto token_too_long;
                }
                token[ti++] = *p++;
            }
            if (*p == '"') {
                tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                                TPBLOG_FLAG_DIRECT,
                                "error: unclosed quote in name list\n");
                goto fail;
            }
        } else {
            while (*p != '\0' && *p != ',') {
                if (ti + 1 >= sizeof(token)) {
                    goto token_too_long;
                }
                token[ti++] = *p++;
            }
            while (ti > 0 && (token[ti - 1] == ' ' || token[ti - 1] == '\t')) {
                ti--;
            }
            size_t lead = 0;
            while (lead < ti && (token[lead] == ' ' || token[lead] == '\t')) {
                lead++;
            }
            if (lead > 0) {
                memmove(token, token + lead, ti - lead);
                ti -= lead;
            }
        }
        token[ti] = '\0';
        if (token[0] == '\0') {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT,
                            "error: empty entry in name list\n");
            goto fail;
        }
        err = _sf_append_name_unique(&names, &n, &cap, token);
        if (err != TPBE_SUCCESS) {
            goto fail;
        }
        if (*p == ',') {
            p++;
        }
    }

    *names_out = names;
    *nnames_out = n;
    return TPBE_SUCCESS;

token_too_long:
    tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                    "error: name too long in name list\n");
fail:
    for (int i = 0; i < n; i++) {
        free(names[i]);
    }
    free(names);
    TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
}

static int
_sf_parse_nonneg_rid_token(const char *tok, int *out)
{
    char *end;
    long v;

    if (tok == NULL || tok[0] == '\0' || out == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    v = strtol(tok, &end, 10);
    if (end == tok || *end != '\0' || v < 0 || v > INT_MAX) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: invalid rid '%s'\n", tok);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    *out = (int)v;
    return TPBE_SUCCESS;
}

static int
_sf_append_rid_unique(int **rids, int *n, int *cap, int rid)
{
    int *arr;
    int i;

    for (i = 0; i < *n; i++) {
        if ((*rids)[i] == rid) {
            return TPBE_SUCCESS;
        }
    }
    if (*n >= *cap) {
        int ncap = (*cap == 0) ? 8 : *cap * 2;
        arr = (int *)realloc(*rids, (size_t)ncap * sizeof(*arr));
        if (arr == NULL) {
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
        }
        *rids = arr;
        *cap = ncap;
    }
    (*rids)[(*n)++] = rid;
    return TPBE_SUCCESS;
}

int
tpbcli_task_parse_rid_list(const char *workspace, const char *spec,
                           int **rids_out, int *nrids_out)
{
    unsigned char (*map_ids)[20] = NULL;
    int nmap = 0;
    char *copy = NULL;
    char *p;
    char *save = NULL;
    int *rids = NULL;
    int nr = 0;
    int cap = 0;
    int err;
    int i;

    (void)workspace;
    if (rids_out == NULL || nrids_out == NULL || spec == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    *rids_out = NULL;
    *nrids_out = 0;

    err = tpbcli_task_ridmap_read(workspace, &map_ids, &nmap);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    if (nmap == 0) {
        free(map_ids);
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: RIDMAP is empty; run 'tpbcli task ls' first\n");
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }

    copy = strdup(spec);
    if (copy == NULL) {
        free(map_ids);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
    }

    p = copy;
    while (p != NULL && *p != '\0') {
        char *comma = strchr(p, ',');
        char *dash;
        char seg[64];
        size_t seglen;

        if (comma != NULL) {
            *comma = '\0';
        }
        seglen = strlen(p);
        while (seglen > 0 && (p[seglen - 1] == ' ' || p[seglen - 1] == '\t')) {
            p[--seglen] = '\0';
        }
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        snprintf(seg, sizeof(seg), "%s", p);
        dash = strchr(seg, '-');
        if (dash != NULL) {
            int lo;
            int hi;
            int j;

            *dash = '\0';
            err = _sf_parse_nonneg_rid_token(seg, &lo);
            if (err != TPBE_SUCCESS) {
                goto fail;
            }
            err = _sf_parse_nonneg_rid_token(dash + 1, &hi);
            if (err != TPBE_SUCCESS) {
                goto fail;
            }
            if (lo > hi) {
                tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                                TPBLOG_FLAG_DIRECT,
                                "error: invalid rid range '%s'\n", seg);
                err = TPBE_CLI_FAIL;
                goto fail;
            }
            for (j = lo; j <= hi; j++) {
                if (j >= nmap) {
                    tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                                    TPBLOG_FLAG_DIRECT,
                                    "error: rid %d not in RIDMAP (have %d)\n",
                                    j, nmap);
                    err = TPBE_CLI_FAIL;
                    goto fail;
                }
                err = _sf_append_rid_unique(&rids, &nr, &cap, j);
                if (err != TPBE_SUCCESS) {
                    goto fail;
                }
            }
        } else {
            int rid;

            err = _sf_parse_nonneg_rid_token(seg, &rid);
            if (err != TPBE_SUCCESS) {
                goto fail;
            }
            if (rid >= nmap) {
                tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                                TPBLOG_FLAG_DIRECT,
                                "error: rid %d not in RIDMAP (have %d)\n", rid,
                                nmap);
                err = TPBE_CLI_FAIL;
                goto fail;
            }
            err = _sf_append_rid_unique(&rids, &nr, &cap, rid);
            if (err != TPBE_SUCCESS) {
                goto fail;
            }
        }
        if (comma == NULL) {
            break;
        }
        p = comma + 1;
    }

    free(copy);
    free(map_ids);
    if (nr == 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: empty rid list\n");
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    *rids_out = rids;
    *nrids_out = nr;
    return TPBE_SUCCESS;

fail:
    free(copy);
    free(map_ids);
    free(rids);
    return err;
}

int
tpbcli_task_resolve_id_prefix(const char *workspace, const char *prefix,
                              unsigned char id_out[20])
{
    size_t plen = 0;
    char lower[21];
    tpb_raf_id_match_t *matches = NULL;
    int nmatch = 0;
    int err;
    size_t i;

    if (workspace == NULL || prefix == NULL || id_out == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    err = _sf_hex_prefix_valid(prefix, &plen);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    if (plen >= sizeof(lower)) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    for (i = 0; i < plen; i++) {
        char c = prefix[i];
        if (c >= 'A' && c <= 'F') {
            c = (char)(c - 'A' + 'a');
        }
        lower[i] = c;
    }
    lower[plen] = '\0';

    err = tpb_raf_scan_records_by_id_prefix(workspace, lower, plen,
                                            TPB_RAF_DOM_TASK, &matches,
                                            &nmatch);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    if (nmatch == 0) {
        tpb_raf_free_id_matches(matches);
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: no task record matches prefix '%s'\n", lower);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    if (nmatch > 1) {
        int j;
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: prefix '%s' matches %d task records:\n", lower,
                        nmatch);
        for (j = 0; j < nmatch; j++) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT, "  %s\n", matches[j].id_hex);
        }
        tpb_raf_free_id_matches(matches);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    if (tpb_raf_hex_to_id(matches[0].id_hex, id_out) != 0) {
        tpb_raf_free_id_matches(matches);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    tpb_raf_free_id_matches(matches);
    return TPBE_SUCCESS;
}

int
tpbcli_task_follow_derive(const char *workspace, const unsigned char start[20],
                          unsigned char root_out[20])
{
    unsigned char chain[TPBCLI_TASK_DERIVE_MAX_HOPS + 1][20];
    int nhops = 0;
    int i;
    int j;
    int err;

    if (workspace == NULL || start == NULL || root_out == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    memcpy(chain[0], start, 20);
    nhops = 0;
  hop_loop:
    {
        task_attr_t attr;
        void *data = NULL;
        uint64_t ds = 0;

        memset(&attr, 0, sizeof(attr));
        err = tpb_raf_record_read_task(workspace, chain[nhops], &attr, &data,
                                       &ds);
        if (err != TPBE_SUCCESS) {
            char hx[41];
            tpb_raf_id_to_hex(chain[nhops], hx);
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT,
                            "error: cannot read task record %s along derive_to "
                            "chain\n",
                            hx);
            free(data);
            TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
        }
        if (_sf_is_all_zero20(attr.derive_to)) {
            memcpy(root_out, chain[nhops], 20);
            tpb_raf_free_headers(attr.headers, attr.nheader);
            free(data);
            return TPBE_SUCCESS;
        }
        if (nhops >= TPBCLI_TASK_DERIVE_MAX_HOPS) {
            tpb_raf_free_headers(attr.headers, attr.nheader);
            free(data);
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT,
                            "error: derive_to chain exceeds %d hops\n",
                            TPBCLI_TASK_DERIVE_MAX_HOPS);
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        nhops++;
        memcpy(chain[nhops], attr.derive_to, 20);
        tpb_raf_free_headers(attr.headers, attr.nheader);
        free(data);
    }
    for (j = 0; j < nhops; j++) {
        if (memcmp(chain[j], chain[nhops], 20) == 0) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT,
                            "error: derive_to cycle detected\n");
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
    }
    goto hop_loop;
}

static int
_sf_load_member(const char *workspace, const unsigned char id[20], int subrank,
                tpbcli_task_member_rec_t *out)
{
    int err;

    memset(out, 0, sizeof(*out));
    memcpy(out->id, id, 20);
    out->subrank = subrank;
    err = tpb_raf_record_read_task(workspace, id, &out->attr, &out->data,
                                   &out->datasize);
    if (err == TPBE_SUCCESS) {
        out->readable = 1;
        return TPBE_SUCCESS;
    }
    out->readable = 0;
    return TPBE_SUCCESS;
}

static int
_sf_logical_from_root(const char *workspace, const unsigned char root[20],
                      const char *ref, int order,
                      tpbcli_task_logical_t *out)
{
    task_attr_t attr;
    void *data = NULL;
    uint64_t ds = 0;
    int err;

    memset(out, 0, sizeof(*out));
    memcpy(out->root_id, root, 20);
    snprintf(out->ref, sizeof(out->ref), "%s", ref);
    out->input_order = order;

    memset(&attr, 0, sizeof(attr));
    err = tpb_raf_record_read_task(workspace, root, &attr, &data, &ds);
    if (err != TPBE_SUCCESS) {
        char hx[41];
        tpb_raf_id_to_hex(root, hx);
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: cannot read task record %s\n", hx);
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    }

    if (tpbcli_task_confirm_capsule(&attr, data, ds)) {
        unsigned char (*ids)[20] = NULL;
        int nmem = 0;
        int i;

        out->is_capsule = 1;
        err = tpbcli_task_read_capsule_members(&attr, data, ds, &ids, &nmem);
        if (err != TPBE_SUCCESS) {
            tpb_raf_free_headers(attr.headers, attr.nheader);
            free(data);
            TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
        }
        out->nmember = nmem;
        if (nmem > 0) {
            out->members = (tpbcli_task_member_rec_t *)calloc(
                (size_t)nmem, sizeof(*out->members));
            if (out->members == NULL) {
                free(ids);
                tpb_raf_free_headers(attr.headers, attr.nheader);
                free(data);
                TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
            }
            for (i = 0; i < nmem; i++) {
                (void)_sf_load_member(workspace, ids[i], i, &out->members[i]);
            }
        }
        free(ids);
    } else {
        out->is_capsule = 0;
        out->nmember = 1;
        out->members = (tpbcli_task_member_rec_t *)calloc(1, sizeof(*out->members));
        if (out->members == NULL) {
            tpb_raf_free_headers(attr.headers, attr.nheader);
            free(data);
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
        }
        out->members[0].subrank = 0;
        out->members[0].readable = 1;
        memcpy(out->members[0].id, root, 20);
        out->members[0].attr = attr;
        out->members[0].data = data;
        out->members[0].datasize = ds;
        data = NULL;
    }

    if (out->is_capsule) {
        tpb_raf_free_headers(attr.headers, attr.nheader);
        free(data);
    }
    return TPBE_SUCCESS;
}

void
tpbcli_task_free_logical_tasks(tpbcli_task_logical_t *tasks, int ntasks)
{
    int i;
    int m;

    if (tasks == NULL) {
        return;
    }
    for (i = 0; i < ntasks; i++) {
        if (tasks[i].members != NULL) {
            for (m = 0; m < tasks[i].nmember; m++) {
                if (tasks[i].members[m].readable) {
                    tpb_raf_free_headers(tasks[i].members[m].attr.headers,
                                         tasks[i].members[m].attr.nheader);
                    free(tasks[i].members[m].data);
                }
            }
            free(tasks[i].members);
        }
    }
    free(tasks);
}

int
tpbcli_task_build_logical_tasks(const char *workspace,
                                const tpbcli_task_init_sel_t *sels, int nsels,
                                tpbcli_task_logical_t **tasks_out, int *ntasks_out)
{
    tpbcli_task_logical_t *tasks = NULL;
    int ntasks = 0;
    int cap = 0;
    int i;
    int err;

    if (workspace == NULL || sels == NULL || tasks_out == NULL ||
        ntasks_out == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    *tasks_out = NULL;
    *ntasks_out = 0;

    for (i = 0; i < nsels; i++) {
        unsigned char root[20];
        int found = 0;
        int j;

        err = tpbcli_task_follow_derive(workspace, sels[i].id, root);
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

        for (j = 0; j < ntasks; j++) {
            if (memcmp(tasks[j].root_id, root, 20) == 0) {
                found = 1;
                break;
            }
        }
        if (found) {
            continue;
        }
        if (ntasks >= cap) {
            int ncap = (cap == 0) ? 4 : cap * 2;
            tpbcli_task_logical_t *grown = (tpbcli_task_logical_t *)realloc(
                tasks, (size_t)ncap * sizeof(*grown));
            if (grown == NULL) {
                tpbcli_task_free_logical_tasks(tasks, ntasks);
                TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
            }
            tasks = grown;
            cap = ncap;
        }
        err = _sf_logical_from_root(workspace, root, sels[i].ref, sels[i].order,
                                    &tasks[ntasks]);
        if (err != TPBE_SUCCESS) {
            tpbcli_task_free_logical_tasks(tasks, ntasks);
            TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
        }
        ntasks++;
    }

    if (ntasks == 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: no logical tasks resolved\n");
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }

    *tasks_out = tasks;
    *ntasks_out = ntasks;
    return TPBE_SUCCESS;
}

static int
_sf_export_hex_prefix_valid(const char *prefix, size_t *len_out)
{
    size_t n;
    size_t i;

    if (prefix == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    n = strlen(prefix);
    if (n < 4 || n > 40) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: task ID prefix must be 4-40 hex digits\n");
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    for (i = 0; i < n; i++) {
        char c = prefix[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT,
                            "error: invalid hex in task ID prefix '%s'\n",
                            prefix);
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
    }
    if (len_out != NULL) {
        *len_out = n;
    }
    return TPBE_SUCCESS;
}

int
tpbcli_task_resolve_id_prefix_export(const char *workspace, const char *prefix,
                                     unsigned char id_out[20])
{
    size_t plen = 0;
    char lower[41];
    tpb_raf_id_match_t *matches = NULL;
    int nmatch = 0;
    int err;
    size_t i;

    if (workspace == NULL || prefix == NULL || id_out == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    err = _sf_export_hex_prefix_valid(prefix, &plen);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    if (plen >= sizeof(lower)) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    for (i = 0; i < plen; i++) {
        char c = prefix[i];
        if (c >= 'A' && c <= 'F') {
            c = (char)(c - 'A' + 'a');
        }
        lower[i] = c;
    }
    lower[plen] = '\0';

    err = tpb_raf_scan_records_by_id_prefix(workspace, lower, plen,
                                            TPB_RAF_DOM_TASK, &matches,
                                            &nmatch);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    if (nmatch == 0) {
        tpb_raf_free_id_matches(matches);
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: no task record matches prefix '%s'\n", lower);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    if (nmatch > 1) {
        int j;
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: prefix '%s' matches %d task records:\n", lower,
                        nmatch);
        for (j = 0; j < nmatch; j++) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT, "  %s\n", matches[j].id_hex);
        }
        tpb_raf_free_id_matches(matches);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    if (tpb_raf_hex_to_id(matches[0].id_hex, id_out) != 0) {
        tpb_raf_free_id_matches(matches);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    tpb_raf_free_id_matches(matches);
    return TPBE_SUCCESS;
}

static int
_sf_export_append_subrank(int **vals, int *n, int *cap, int v)
{
    int *arr;
    int i;

    for (i = 0; i < *n; i++) {
        if ((*vals)[i] == v) {
            return TPBE_SUCCESS;
        }
    }
    if (*n >= *cap) {
        int ncap = (*cap == 0) ? 8 : *cap * 2;
        arr = (int *)realloc(*vals, (size_t)ncap * sizeof(*arr));
        if (arr == NULL) {
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
        }
        *vals = arr;
        *cap = ncap;
    }
    (*vals)[(*n)++] = v;
    return TPBE_SUCCESS;
}

int
tpbcli_task_export_parse_filter(const char *text,
                                  tpbcli_task_export_filter_t *out)
{
    const char *eq;
    char key[32];
    size_t klen;
    char *copy = NULL;
    char *p;
    char *save = NULL;
    int err;

    if (text == NULL || out == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    memset(out, 0, sizeof(*out));
    eq = strchr(text, '=');
    if (eq == NULL || strchr(text, '>') != NULL || strchr(text, '<') != NULL) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: export member filter must use '=' only\n");
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    klen = (size_t)(eq - text);
    if (klen == 0 || klen >= sizeof(key)) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    memcpy(key, text, klen);
    key[klen] = '\0';
    if (eq[1] == '\0') {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    copy = strdup(eq + 1);
    if (copy == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
    }

    if (strcmp(key, "subrank") == 0) {
        int cap = 0;
        out->key = TPBCLI_TASK_EXP_FKEY_SUBRANK;
        p = copy;
        while (p != NULL && *p != '\0') {
            char *comma = strchr(p, ',');
            char *dash;
            char seg[64];

            if (comma != NULL) {
                *comma = '\0';
            }
            snprintf(seg, sizeof(seg), "%s", p);
            dash = strchr(seg, '-');
            if (dash != NULL) {
                int lo;
                int hi;
                int j;

                *dash = '\0';
                err = _sf_parse_nonneg_rid_token(seg, &lo);
                if (err != TPBE_SUCCESS) {
                    goto fail;
                }
                err = _sf_parse_nonneg_rid_token(dash + 1, &hi);
                if (err != TPBE_SUCCESS) {
                    goto fail;
                }
                if (lo > hi) {
                    err = TPBE_CLI_FAIL;
                    goto fail;
                }
                for (j = lo; j <= hi; j++) {
                    err = _sf_export_append_subrank(&out->subrank, &out->nsubrank,
                                                    &cap, j);
                    if (err != TPBE_SUCCESS) {
                        goto fail;
                    }
                }
            } else {
                int v;
                err = _sf_parse_nonneg_rid_token(seg, &v);
                if (err != TPBE_SUCCESS) {
                    goto fail;
                }
                err = _sf_export_append_subrank(&out->subrank, &out->nsubrank, &cap,
                                                v);
                if (err != TPBE_SUCCESS) {
                    goto fail;
                }
            }
            if (comma == NULL) {
                break;
            }
            p = comma + 1;
        }
        free(copy);
        return TPBE_SUCCESS;
    }

    if (strcmp(key, "subtid") == 0) {
        out->key = TPBCLI_TASK_EXP_FKEY_SUBTID;
        p = copy;
        while (p != NULL && *p != '\0') {
            char *comma = strchr(p, ',');
            size_t n;
            size_t i;

            if (comma != NULL) {
                *comma = '\0';
            }
            while (*p == ' ' || *p == '\t') {
                p++;
            }
            n = strlen(p);
            while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t')) {
                p[--n] = '\0';
            }
            if (n < 4 || n > 8) {
                tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                                TPBLOG_FLAG_DIRECT,
                                "error: subtid filter value must be 4-8 hex "
                                "digits\n");
                err = TPBE_CLI_FAIL;
                goto fail;
            }
            for (i = 0; i < n; i++) {
                char c = p[i];
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                      (c >= 'A' && c <= 'F'))) {
                    err = TPBE_CLI_FAIL;
                    goto fail;
                }
            }
            if (out->nsubtid >= TPBCLI_TASK_FILTER_MAX) {
                err = TPBE_CLI_FAIL;
                goto fail;
            }
            snprintf(out->subtid[out->nsubtid], sizeof(out->subtid[0]), "%s", p);
            for (i = 0; out->subtid[out->nsubtid][i]; i++) {
                if (out->subtid[out->nsubtid][i] >= 'A' &&
                    out->subtid[out->nsubtid][i] <= 'F') {
                    out->subtid[out->nsubtid][i] =
                        (char)(out->subtid[out->nsubtid][i] - 'A' + 'a');
                }
            }
            out->nsubtid++;
            if (comma == NULL) {
                break;
            }
            p = comma + 1;
        }
        free(copy);
        return TPBE_SUCCESS;
    }

    tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                    "error: filter key '%s' is not valid for task export "
                    "(supported: subrank, subtid)\n",
                    key);
    err = TPBE_CLI_FAIL;

fail:
    free(copy);
    free(out->subrank);
    memset(out, 0, sizeof(*out));
    return err;
}

void
tpbcli_task_export_free_filters(tpbcli_task_export_opts_t *opts)
{
    int i;

    if (opts == NULL) {
        return;
    }
    for (i = 0; i < opts->nfilter; i++) {
        free(opts->filters[i].subrank);
        opts->filters[i].subrank = NULL;
        opts->filters[i].nsubrank = 0;
    }
}

void
tpbcli_task_export_free_works(tpbcli_task_export_work_t *works, int nworks)
{
    (void)nworks;
    free(works);
}

static int
_sf_export_resolve_init_sels(const char *workspace,
                            const tpbcli_task_export_opts_t *opts,
                            tpbcli_task_init_sel_t **sels_out, int *nsels_out)
{
    tpbcli_task_init_sel_t *sels = NULL;
    int ns = 0;
    int cap = 0;
    int err;
    int i;

    *sels_out = NULL;
    *nsels_out = 0;

    if (opts->sel_mode == TPBCLI_TASK_EXP_SEL_FROM_LS) {
        unsigned char (*map_ids)[20] = NULL;
        int nmap = 0;

        err = tpbcli_task_ridmap_read(workspace, &map_ids, &nmap);
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
        if (nmap == 0) {
            free(map_ids);
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                            "error: RIDMAP has no valid task IDs\n");
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        sels = (tpbcli_task_init_sel_t *)calloc((size_t)nmap, sizeof(*sels));
        if (sels == NULL) {
            free(map_ids);
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
        }
        for (i = 0; i < nmap; i++) {
            memcpy(sels[i].id, map_ids[i], 20);
            snprintf(sels[i].ref, sizeof(sels[i].ref), "r%d", i);
            sels[i].sel_kind = TPBCLI_TASK_SEL_RID;
            sels[i].order = i;
        }
        ns = nmap;
        free(map_ids);
    } else if (opts->sel_mode == TPBCLI_TASK_EXP_SEL_RID) {
        int *rids = NULL;
        int nr = 0;
        unsigned char (*map_ids)[20] = NULL;
        int nmap = 0;

        if (opts->rid_spec == NULL) {
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        err = tpbcli_task_parse_rid_list(workspace, opts->rid_spec, &rids, &nr);
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
        err = tpbcli_task_ridmap_read(workspace, &map_ids, &nmap);
        if (err != TPBE_SUCCESS) {
            free(rids);
            TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
        }
        for (i = 0; i < nr; i++) {
            if (ns >= cap) {
                int ncap = (cap == 0) ? 4 : cap * 2;
                tpbcli_task_init_sel_t *g = (tpbcli_task_init_sel_t *)realloc(
                    sels, (size_t)ncap * sizeof(*g));
                if (g == NULL) {
                    free(rids);
                    free(map_ids);
                    free(sels);
                    TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
                }
                sels = g;
                cap = ncap;
            }
            memcpy(sels[ns].id, map_ids[rids[i]], 20);
            snprintf(sels[ns].ref, sizeof(sels[ns].ref), "r%d", rids[i]);
            sels[ns].sel_kind = TPBCLI_TASK_SEL_RID;
            sels[ns].order = i;
            ns++;
        }
        free(rids);
        free(map_ids);
    } else if (opts->sel_mode == TPBCLI_TASK_EXP_SEL_TASK_ID) {
        char *copy;
        char *p;
        char *save = NULL;
        int ord = 0;

        if (opts->task_id_spec == NULL) {
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        copy = strdup(opts->task_id_spec);
        if (copy == NULL) {
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
        }
        p = copy;
        while (p != NULL) {
            char *tok = strtok_r(p, ",", &save);
            if (tok == NULL) {
                break;
            }
            p = NULL;
            while (*tok == ' ' || *tok == '\t') {
                tok++;
            }
            if (*tok == '\0') {
                continue;
            }
            if (ns >= cap) {
                int ncap = (cap == 0) ? 4 : cap * 2;
                tpbcli_task_init_sel_t *g = (tpbcli_task_init_sel_t *)realloc(
                    sels, (size_t)ncap * sizeof(*g));
                if (g == NULL) {
                    free(copy);
                    free(sels);
                    TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
                }
                sels = g;
                cap = ncap;
            }
            err = tpbcli_task_resolve_id_prefix_export(workspace, tok, sels[ns].id);
            if (err != TPBE_SUCCESS) {
                free(copy);
                free(sels);
                TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
            }
            snprintf(sels[ns].ref, sizeof(sels[ns].ref), "i%d", ord);
            sels[ns].sel_kind = TPBCLI_TASK_SEL_TASK_ID;
            sels[ns].order = ord;
            ns++;
            ord++;
        }
        free(copy);
    } else {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }

    if (ns == 0) {
        free(sels);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    *sels_out = sels;
    *nsels_out = ns;
    return TPBE_SUCCESS;
}

static int
_sf_export_ask_trace(const unsigned char cur[20], const unsigned char next[20])
{
    char cur_hx[41];
    char next_hx[41];
    char line[128];
    char *nl;

    tpb_raf_id_to_hex(cur, cur_hx);
    tpb_raf_id_to_hex(next, next_hx);
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "Trace task %s to entry %s? [y/N] ", cur_hx, next_hx);
    if (fgets(line, sizeof(line), stdin) == NULL) {
        return 0;
    }
    nl = strchr(line, '\n');
    if (nl != NULL) {
        *nl = '\0';
    }
    return (line[0] == 'y' || line[0] == 'Y');
}

static int
_sf_export_trace_one(const char *workspace, const unsigned char start[20],
                   tpbcli_task_export_trace_t trace, int auto_trace_y,
                   unsigned char root_out[20], int *keep_single_out,
                   unsigned char single_out[20])
{
    unsigned char cur[20];
    int err;

    (void)auto_trace_y;
    memcpy(cur, start, 20);
    *keep_single_out = 0;
    memset(single_out, 0, 20);

    if (trace == TPBCLI_TASK_EXP_TRACE_TO_ENTRY) {
        err = tpbcli_task_follow_derive(workspace, start, root_out);
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
        return TPBE_SUCCESS;
    }

    if (trace == TPBCLI_TASK_EXP_TRACE_KEEP_CURRENT) {
        memcpy(root_out, cur, 20);
        {
            task_attr_t attr;
            void *data = NULL;
            uint64_t ds = 0;

            memset(&attr, 0, sizeof(attr));
            err = tpb_raf_record_read_task(workspace, cur, &attr, &data, &ds);
            if (err != TPBE_SUCCESS) {
                TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
            }
            if (!_sf_is_all_zero20(attr.derive_to)) {
                *keep_single_out = 1;
                memcpy(single_out, cur, 20);
            }
            tpb_raf_free_headers(attr.headers, attr.nheader);
            free(data);
        }
        return TPBE_SUCCESS;
    }

    if (!isatty(STDIN_FILENO)) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                        "note: stdin is not a TTY; keeping each selected task "
                        "record (--keep-current)\n");
        return _sf_export_trace_one(workspace, start,
                                    TPBCLI_TASK_EXP_TRACE_KEEP_CURRENT, 0,
                                    root_out, keep_single_out, single_out);
    }

    {
        task_attr_t attr;
        void *data = NULL;
        uint64_t ds = 0;

        memset(&attr, 0, sizeof(attr));
        err = tpb_raf_record_read_task(workspace, cur, &attr, &data, &ds);
        if (err != TPBE_SUCCESS) {
            TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
        }
        if (_sf_is_all_zero20(attr.derive_to)) {
            memcpy(root_out, cur, 20);
            tpb_raf_free_headers(attr.headers, attr.nheader);
            free(data);
            return TPBE_SUCCESS;
        }
        if (_sf_export_ask_trace(cur, attr.derive_to)) {
            tpb_raf_free_headers(attr.headers, attr.nheader);
            free(data);
            err = tpbcli_task_follow_derive(workspace, start, root_out);
            TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
            return TPBE_SUCCESS;
        }
        memcpy(root_out, cur, 20);
        *keep_single_out = 1;
        memcpy(single_out, cur, 20);
        tpb_raf_free_headers(attr.headers, attr.nheader);
        free(data);
    }
    return TPBE_SUCCESS;
}

int
tpbcli_task_export_build_works(const char *workspace,
                                const tpbcli_task_export_opts_t *opts,
                                tpbcli_task_export_work_t **works_out,
                                int *nworks_out)
{
    tpbcli_task_init_sel_t *sels = NULL;
    int nsels = 0;
    tpbcli_task_export_work_t *works = NULL;
    int nworks = 0;
    int cap = 0;
    int err;
    int i;

    if (workspace == NULL || opts == NULL || works_out == NULL ||
        nworks_out == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    *works_out = NULL;
    *nworks_out = 0;

    err = _sf_export_resolve_init_sels(workspace, opts, &sels, &nsels);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    for (i = 0; i < nsels; i++) {
        unsigned char root[20];
        unsigned char single[20];
        int keep_single = 0;
        int found = 0;
        int j;

        err = _sf_export_trace_one(workspace, sels[i].id, opts->trace, 0, root,
                                   &keep_single, single);
        if (err != TPBE_SUCCESS) {
            free(sels);
            tpbcli_task_export_free_works(works, nworks);
            TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
        }
        for (j = 0; j < nworks; j++) {
            if (memcmp(works[j].root_id, root, 20) == 0 &&
                works[j].keep_single_member == keep_single &&
                (!keep_single ||
                 memcmp(works[j].single_member_id, single, 20) == 0)) {
                found = 1;
                break;
            }
        }
        if (found) {
            continue;
        }
        if (nworks >= cap) {
            int ncap = (cap == 0) ? 4 : cap * 2;
            tpbcli_task_export_work_t *g = (tpbcli_task_export_work_t *)realloc(
                works, (size_t)ncap * sizeof(*g));
            if (g == NULL) {
                free(sels);
                tpbcli_task_export_free_works(works, nworks);
                TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
            }
            works = g;
            cap = ncap;
        }
        memcpy(works[nworks].root_id, root, 20);
        snprintf(works[nworks].ref, sizeof(works[nworks].ref), "%s",
                 sels[i].ref);
        works[nworks].keep_single_member = keep_single;
        if (keep_single) {
            memcpy(works[nworks].single_member_id, single, 20);
        }
        works[nworks].input_order = sels[i].order;
        nworks++;
    }

    free(sels);
    if (nworks == 0) {
        tpbcli_task_export_free_works(works, 0);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    *works_out = works;
    *nworks_out = nworks;
    return TPBE_SUCCESS;
}
