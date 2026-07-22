/*
 * tpbcli-task-get-result.c
 * `tpbcli task get-result|gr` — meta/metric comparison tables.
 */

#include <inttypes.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tpb-public.h"
#include "tpbcli-data-fmt.h"
#include "tpbcli-task-internal.h"

/* Local Function Prototypes */
static int _sf_gr_resolve_sels(const char *workspace,
                               const tpbcli_task_gr_opts_t *opts,
                               tpbcli_task_init_sel_t **sels_out, int *nsels_out);
static void _sf_gr_free_warnings(char **warns, int n);
static int _sf_gr_append_warn(char ***warns, int *n, int *cap, const char *msg);
static int _sf_header_nelem(const tpb_meta_header_t *h, uint64_t *nelem_out);
static int _sf_dtype_statable(uint32_t type_bits);
static int _sf_uattr_schema_match(uint64_t a, uint64_t b);
static int _sf_find_output_by_name(const task_attr_t *attr, const char *name,
                                   int *idx_out, int *dup_out);
static int _sf_append_elem_as_double(const void *p, TPB_DTYPE dt, double **arr,
                                     size_t *n, size_t *cap);
static int _sf_collect_samples(const tpbcli_task_logical_t *lt,
                               const char *dname, int show_each, int subrank,
                               double **samples_out, size_t *nsamples_out,
                               uint32_t *type_bits_out, uint64_t *uattr_out,
                               int *dtype_mixed_out, char *warn, size_t warnlen);
static void _sf_format_num(double v, char *out, size_t outlen);
static int _sf_gr_print_name_report(const char *workspace,
                                    tpbcli_task_logical_t *tasks, int ntasks);
static int _sf_gr_kernel_name(const char *workspace, const unsigned char kid[20],
                              char *out, size_t outlen);
static int _sf_gr_batch_host(const char *workspace,
                             const unsigned char tbatch_id[20], char *out,
                             size_t outlen);
static int _sf_gr_meta_cell(const char *workspace, const tpbcli_task_logical_t *lt,
                            const char *key, int show_each, int subrank,
                            char *out, size_t outlen, char *warn, size_t warnlen);
static int _sf_gr_collect_all_data_names(const tpbcli_task_logical_t *tasks,
                                         int ntasks, char ***names_out,
                                         int *nnames_out);
static int _sf_gr_collect_shared_data_names(const tpbcli_task_logical_t *tasks,
                                            int ntasks, char ***names_out,
                                            int *nnames_out);

static const char *const s_default_meta[] = {
    "ref", "taskid", "kernel", "batch_host", "datetime", "nmember",
    "exit_code", "ninput", "noutput", "nheader"
};
static const int s_default_meta_n =
    (int)(sizeof(s_default_meta) / sizeof(s_default_meta[0]));

static double s_qtiles[6] = {0.25, 0.50, 0.75, 0.90, 0.95, 0.99};

static void
_sf_format_num(double v, char *out, size_t outlen)
{
    if (out == NULL || outlen == 0) {
        return;
    }
    if (!isfinite(v)) {
        snprintf(out, outlen, "N/A");
        return;
    }
    snprintf(out, outlen, "%.6g", v);
}

static int
_sf_gr_append_warn(char ***warns, int *n, int *cap, const char *msg)
{
    char **arr;

    if (msg == NULL) {
        return TPBE_SUCCESS;
    }
    if (*n >= *cap) {
        int ncap = (*cap == 0) ? 8 : *cap * 2;
        arr = (char **)realloc(*warns, (size_t)ncap * sizeof(*arr));
        if (arr == NULL) {
            return TPBE_MALLOC_FAIL;
        }
        *warns = arr;
        *cap = ncap;
    }
    (*warns)[*n] = strdup(msg);
    if ((*warns)[*n] == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    (*n)++;
    return TPBE_SUCCESS;
}

static void
_sf_gr_free_warnings(char **warns, int n)
{
    int i;

    if (warns == NULL) {
        return;
    }
    for (i = 0; i < n; i++) {
        free(warns[i]);
    }
    free(warns);
}

static int
_sf_header_nelem(const tpb_meta_header_t *h, uint64_t *nelem_out)
{
    uint64_t prod = 1;
    uint32_t d;

    if (h == NULL || nelem_out == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    if (h->ndim == 0) {
        *nelem_out = 1;
        return TPBE_SUCCESS;
    }
    for (d = 0; d < h->ndim && d < TPBM_DATA_NDIM_MAX; d++) {
        if (h->dimsizes[d] == 0) {
            *nelem_out = 0;
            return TPBE_SUCCESS;
        }
        if (prod > UINT64_MAX / (uint64_t)h->dimsizes[d]) {
            return TPBE_CLI_FAIL;
        }
        prod *= (uint64_t)h->dimsizes[d];
    }
    *nelem_out = prod;
    return TPBE_SUCCESS;
}

static int
_sf_dtype_statable(uint32_t type_bits)
{
    uint32_t typ = type_bits & (uint32_t)TPB_PARM_TYPE_MASK;

    switch (typ) {
    case (uint32_t)(TPB_INT_T & TPB_PARM_TYPE_MASK):
    case (uint32_t)(TPB_INT8_T & TPB_PARM_TYPE_MASK):
    case (uint32_t)(TPB_INT16_T & TPB_PARM_TYPE_MASK):
    case (uint32_t)(TPB_INT32_T & TPB_PARM_TYPE_MASK):
    case (uint32_t)(TPB_INT64_T & TPB_PARM_TYPE_MASK):
    case (uint32_t)(TPB_UINT8_T & TPB_PARM_TYPE_MASK):
    case (uint32_t)(TPB_UINT16_T & TPB_PARM_TYPE_MASK):
    case (uint32_t)(TPB_UINT32_T & TPB_PARM_TYPE_MASK):
    case (uint32_t)(TPB_UINT64_T & TPB_PARM_TYPE_MASK):
    case (uint32_t)(TPB_FLOAT_T & TPB_PARM_TYPE_MASK):
    case (uint32_t)(TPB_DOUBLE_T & TPB_PARM_TYPE_MASK):
        return 1;
    default:
        return 0;
    }
}

static int
_sf_uattr_schema_match(uint64_t a, uint64_t b)
{
    uint64_t mask = TPB_UATTR_SHAPE_MASK | TPB_UNIT_MASK;
    return ((a & mask) == (b & mask));
}

static int
_sf_find_output_by_name(const task_attr_t *attr, const char *name, int *idx_out,
                        int *dup_out)
{
    uint32_t lo;
    uint32_t hi;
    int found = -1;
    int dup = 0;

    if (attr == NULL || name == NULL) {
        return -1;
    }
    lo = attr->ninput;
    hi = attr->ninput + attr->noutput;
    for (uint32_t i = lo; i < hi && i < attr->nheader; i++) {
        if (strcmp(attr->headers[i].name, name) == 0) {
            if (found >= 0) {
                dup = 1;
            }
            found = (int)i;
        }
    }
    if (idx_out != NULL) {
        *idx_out = found;
    }
    if (dup_out != NULL) {
        *dup_out = dup;
    }
    return found;
}

static int
_sf_append_elem_as_double(const void *p, TPB_DTYPE dt, double **arr,
                          size_t *n, size_t *cap)
{
    double v = 0.0;
    double *grown;

    switch (dt) {
    case TPB_INT8_T:
        v = (double)*(const int8_t *)p;
        break;
    case TPB_UINT8_T:
        v = (double)*(const uint8_t *)p;
        break;
    case TPB_INT16_T:
        v = (double)*(const int16_t *)p;
        break;
    case TPB_UINT16_T:
        v = (double)*(const uint16_t *)p;
        break;
    case TPB_INT32_T:
    case TPB_INT_T:
        v = (double)*(const int32_t *)p;
        break;
    case TPB_UINT32_T:
        v = (double)*(const uint32_t *)p;
        break;
    case TPB_INT64_T:
        v = (double)*(const int64_t *)p;
        break;
    case TPB_UINT64_T:
        v = (double)*(const uint64_t *)p;
        break;
    case TPB_FLOAT_T:
        v = (double)*(const float *)p;
        break;
    case TPB_DOUBLE_T:
        v = *(const double *)p;
        break;
    default:
        return TPBE_CLI_FAIL;
    }
    if (*n >= *cap) {
        size_t ncap = (*cap == 0) ? 16 : *cap * 2;
        grown = (double *)realloc(*arr, ncap * sizeof(*grown));
        if (grown == NULL) {
            return TPBE_MALLOC_FAIL;
        }
        *arr = grown;
        *cap = ncap;
    }
    (*arr)[(*n)++] = v;
    return TPBE_SUCCESS;
}

static int
_sf_gr_resolve_sels(const char *workspace, const tpbcli_task_gr_opts_t *opts,
                    tpbcli_task_init_sel_t **sels_out, int *nsels_out)
{
    tpbcli_task_init_sel_t *sels = NULL;
    int ns = 0;
    int cap = 0;
    int err;
    int i;

    if (opts->sel_kind == TPBCLI_TASK_SEL_RID) {
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
    } else {
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
            err = tpbcli_task_resolve_id_prefix(workspace, tok, sels[ns].id);
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
_sf_gr_kernel_name(const char *workspace, const unsigned char kid[20],
                   char *out, size_t outlen)
{
    kernel_entry_t *entries = NULL;
    int n = 0;
    int i;
    int err;

    err = tpb_raf_entry_list_kernel(workspace, &entries, &n);
    if (err != TPBE_SUCCESS) {
        tpbcli_task_format_id_prefix6(kid, out, outlen);
        return err;
    }
    for (i = 0; i < n; i++) {
        if (memcmp(entries[i].kernel_id, kid, 20) == 0) {
            snprintf(out, outlen, "%s", entries[i].kernel_name);
            free(entries);
            return TPBE_SUCCESS;
        }
    }
    free(entries);
    tpbcli_task_format_id_prefix6(kid, out, outlen);
    return TPBE_SUCCESS;
}

static int
_sf_gr_batch_host(const char *workspace, const unsigned char tbatch_id[20],
                  char *out, size_t outlen)
{
    tbatch_attr_t attr;
    int err;

    memset(&attr, 0, sizeof(attr));
    err = tpb_raf_record_read_tbatch(workspace, tbatch_id, &attr, NULL, NULL);
    if (err != TPBE_SUCCESS || attr.hostname[0] == '\0') {
        snprintf(out, outlen, "N/A");
        return err;
    }
    snprintf(out, outlen, "%s", attr.hostname);
    tpb_raf_free_headers(attr.headers, attr.nheader);
    return TPBE_SUCCESS;
}

static int
_sf_gr_meta_cell(const char *workspace, const tpbcli_task_logical_t *lt,
                 const char *key, int show_each, int subrank, char *out,
                 size_t outlen, char *warn, size_t warnlen)
{
    const task_attr_t *a;
    int m;

    (void)warn;
    (void)warnlen;
    if (strcmp(key, "ref") == 0) {
        snprintf(out, outlen, "%s", lt->ref);
        return TPBE_SUCCESS;
    }
    if (show_each) {
        if (subrank < 0 || subrank >= lt->nmember) {
            snprintf(out, outlen, "N/A");
            return TPBE_SUCCESS;
        }
        if (!lt->members[subrank].readable) {
            snprintf(out, outlen, "N/A");
            return TPBE_SUCCESS;
        }
        a = &lt->members[subrank].attr;
    } else if (lt->is_capsule) {
        uint32_t first = 0;
        int mixed = 0;
        char tmp[128];

        for (m = 0; m < lt->nmember; m++) {
            if (!lt->members[m].readable) {
                continue;
            }
            if (_sf_gr_meta_cell(workspace, lt, key, 1, m, tmp, sizeof(tmp),
                                 NULL, 0) != TPBE_SUCCESS) {
                mixed = 1;
                break;
            }
            if (m == 0 || first == 0) {
                snprintf(out, outlen, "%s", tmp);
                first = 1;
            } else if (strcmp(out, tmp) != 0) {
                mixed = 1;
                break;
            }
        }
        if (!first) {
            snprintf(out, outlen, "N/A");
            return TPBE_SUCCESS;
        }
        if (mixed) {
            snprintf(out, outlen, "mixed");
        }
        return TPBE_SUCCESS;
    } else {
        if (!lt->members[0].readable) {
            snprintf(out, outlen, "N/A");
            return TPBE_SUCCESS;
        }
        a = &lt->members[0].attr;
    }

    if (strcmp(key, "taskid") == 0 || strcmp(key, "task_record_id") == 0) {
        if (strcmp(key, "taskid") == 0) {
            tpbcli_task_format_id_prefix6(lt->root_id, out, outlen);
        } else {
            char hx[41];
            tpb_raf_id_to_hex(lt->root_id, hx);
            snprintf(out, outlen, "%s", hx);
        }
        return TPBE_SUCCESS;
    }
    if (strcmp(key, "kernel") == 0) {
        return _sf_gr_kernel_name(workspace, a->kernel_id, out, outlen);
    }
    if (strcmp(key, "batch_host") == 0) {
        return _sf_gr_batch_host(workspace, a->tbatch_id, out, outlen);
    }
    if (strcmp(key, "datetime") == 0) {
        if (tpbcli_task_time_format_local(a->utc_bits, out, outlen) !=
            TPBE_SUCCESS) {
            snprintf(out, outlen, "N/A");
        }
        return TPBE_SUCCESS;
    }
    if (strcmp(key, "nmember") == 0) {
        snprintf(out, outlen, "%d", lt->is_capsule ? lt->nmember : 1);
        return TPBE_SUCCESS;
    }
    if (strcmp(key, "duration_seconds") == 0) {
        snprintf(out, outlen, "%.9f", (double)a->duration / 1e9);
        return TPBE_SUCCESS;
    }
    if (strcmp(key, "exit_code") == 0) {
        snprintf(out, outlen, "%u", a->exit_code);
        return TPBE_SUCCESS;
    }
    if (strcmp(key, "ninput") == 0) {
        snprintf(out, outlen, "%u", a->ninput);
        return TPBE_SUCCESS;
    }
    if (strcmp(key, "noutput") == 0) {
        snprintf(out, outlen, "%u", a->noutput);
        return TPBE_SUCCESS;
    }
    if (strcmp(key, "nheader") == 0) {
        snprintf(out, outlen, "%u", a->nheader);
        return TPBE_SUCCESS;
    }
    if (strcmp(key, "duration") == 0) {
        snprintf(out, outlen, "%" PRIu64, a->duration);
        return TPBE_SUCCESS;
    }
    if (strcmp(key, "utc_bits") == 0) {
        snprintf(out, outlen, "%" PRIu64, a->utc_bits);
        return TPBE_SUCCESS;
    }
    if (strcmp(key, "pid") == 0) {
        snprintf(out, outlen, "%u", a->pid);
        return TPBE_SUCCESS;
    }
    if (strcmp(key, "tid") == 0) {
        snprintf(out, outlen, "%u", a->tid);
        return TPBE_SUCCESS;
    }
    snprintf(out, outlen, "N/A");
    return TPBE_SUCCESS;
}

static int
_sf_name_in_list(const char *name, char **list, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        if (strcmp(list[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int
_sf_gr_collect_all_data_names(const tpbcli_task_logical_t *tasks, int ntasks,
                              char ***names_out, int *nnames_out)
{
    char **names = NULL;
    int n = 0;
    int cap = 0;
    int t;
    int m;

    for (t = 0; t < ntasks; t++) {
        for (m = 0; m < tasks[t].nmember; m++) {
            const task_attr_t *a;
            uint32_t i;

            if (!tasks[t].members[m].readable) {
                continue;
            }
            a = &tasks[t].members[m].attr;
            for (i = a->ninput; i < a->ninput + a->noutput && i < a->nheader;
                 i++) {
                if (!_sf_name_in_list(a->headers[i].name, names, n)) {
                    char *one;

                    if (n >= cap) {
                        int ncap = (cap == 0) ? 8 : cap * 2;
                        char **g = (char **)realloc(names,
                                                    (size_t)ncap * sizeof(*g));
                        if (g == NULL) {
                            goto fail;
                        }
                        names = g;
                        cap = ncap;
                    }
                    one = strdup(a->headers[i].name);
                    if (one == NULL) {
                        goto fail;
                    }
                    names[n++] = one;
                }
            }
        }
    }
    *names_out = names;
    *nnames_out = n;
    return TPBE_SUCCESS;
fail:
    for (int i = 0; i < n; i++) {
        free(names[i]);
    }
    free(names);
    return TPBE_MALLOC_FAIL;
}

static int
_sf_gr_data_compatible(const tpbcli_task_logical_t *lt, const char *name)
{
    int m;
    uint32_t tb = 0;
    uint64_t ua = 0;
    int have = 0;

    for (m = 0; m < lt->nmember; m++) {
        int idx;
        int dup;
        const tpb_meta_header_t *h;
        const void *ptr;
        uint64_t nbytes;
        uint64_t nelem;
        int err;

        if (!lt->members[m].readable) {
            continue;
        }
        if (_sf_find_output_by_name(&lt->members[m].attr, name, &idx, &dup) <
                0 ||
            dup) {
            return 0;
        }
        h = &lt->members[m].attr.headers[idx];
        if (!_sf_dtype_statable(h->type_bits)) {
            return 0;
        }
        err = tpb_raf_header_data_ptr(lt->members[m].attr.headers,
                                      lt->members[m].attr.nheader,
                                      lt->members[m].data, lt->members[m].datasize,
                                      (uint32_t)idx, &ptr, &nbytes);
        if (err != TPBE_SUCCESS) {
            return 0;
        }
        if (_sf_header_nelem(h, &nelem) != TPBE_SUCCESS || nelem == 0) {
            return 0;
        }
        if (!have) {
            tb = h->type_bits;
            ua = h->uattr_bits;
            have = 1;
        } else if (tb != h->type_bits ||
                   !_sf_uattr_schema_match(ua, h->uattr_bits)) {
            return 0;
        }
    }
    return have;
}

static int
_sf_gr_collect_shared_data_names(const tpbcli_task_logical_t *tasks,
                                 int ntasks, char ***names_out, int *nnames_out)
{
    char **all = NULL;
    int nall = 0;
    char **shared = NULL;
    int nsh = 0;
    int cap = 0;
    int i;
    int t;
    int err;

    err = _sf_gr_collect_all_data_names(tasks, ntasks, &all, &nall);
    if (err != TPBE_SUCCESS) {
        return err;
    }
    for (i = 0; i < nall; i++) {
        int ok = 1;
        for (t = 0; t < ntasks; t++) {
            if (!_sf_gr_data_compatible(&tasks[t], all[i])) {
                ok = 0;
                break;
            }
        }
        if (!ok) {
            continue;
        }
        if (nsh >= cap) {
            int ncap = (cap == 0) ? 8 : cap * 2;
            char **g = (char **)realloc(shared, (size_t)ncap * sizeof(*g));
            if (g == NULL) {
                err = TPBE_MALLOC_FAIL;
                goto done;
            }
            shared = g;
            cap = ncap;
        }
        shared[nsh] = strdup(all[i]);
        if (shared[nsh] == NULL) {
            err = TPBE_MALLOC_FAIL;
            goto done;
        }
        nsh++;
    }
    err = TPBE_SUCCESS;
done:
    for (i = 0; i < nall; i++) {
        free(all[i]);
    }
    free(all);
    if (err != TPBE_SUCCESS) {
        for (i = 0; i < nsh; i++) {
            free(shared[i]);
        }
        free(shared);
        return err;
    }
    *names_out = shared;
    *nnames_out = nsh;
    return TPBE_SUCCESS;
}

static int
_sf_gr_print_name_report(const char *workspace, tpbcli_task_logical_t *tasks,
                         int ntasks)
{
    char **shared_meta = NULL;
    char **shared_data = NULL;
    int nsm = 0;
    int nsd = 0;
    int t;
    int err;

    (void)workspace;
    shared_meta = (char **)calloc((size_t)s_default_meta_n, sizeof(*shared_meta));
    if (shared_meta == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    for (int i = 0; i < s_default_meta_n; i++) {
        shared_meta[nsm++] = strdup(s_default_meta[i]);
    }
    err = _sf_gr_collect_shared_data_names(tasks, ntasks, &shared_data, &nsd);
    if (err != TPBE_SUCCESS) {
        goto done;
    }

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "Shared meta name:");
    for (int i = 0; i < nsm; i++) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                        TPBLOG_FLAG_DIRECT, " %s", shared_meta[i]);
    }
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "\nShared data name:");
    for (int i = 0; i < nsd; i++) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                        TPBLOG_FLAG_DIRECT, " %s", shared_data[i]);
    }
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "\n");

    for (t = 0; t < ntasks; t++) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                        "Private data name (ref=%s):", tasks[t].ref);
        for (int m = 0; m < tasks[t].nmember; m++) {
            const task_attr_t *a;
            uint32_t hi;

            if (!tasks[t].members[m].readable) {
                continue;
            }
            a = &tasks[t].members[m].attr;
            for (hi = a->ninput; hi < a->ninput + a->noutput && hi < a->nheader;
                 hi++) {
                if (!_sf_name_in_list(a->headers[hi].name, shared_data, nsd)) {
                    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                                    TPBLOG_FLAG_DIRECT, " %s",
                                    a->headers[hi].name);
                }
            }
        }
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                        TPBLOG_FLAG_DIRECT, "\n");
    }

done:
    for (int i = 0; i < nsm; i++) {
        free(shared_meta[i]);
    }
    free(shared_meta);
    for (int i = 0; i < nsd; i++) {
        free(shared_data[i]);
    }
    free(shared_data);
    return err;
}

static int
_sf_collect_samples_member(const tpbcli_task_member_rec_t *mem, const char *dname,
                           double **samples, size_t *n, size_t *cap,
                           uint32_t *tb, uint64_t *ua, int *mixed_dtype,
                           char *warn, size_t warnlen)
{
    int idx;
    int dup;
    const tpb_meta_header_t *h;
    const void *ptr;
    uint64_t nbytes;
    uint64_t nelem;
    TPB_DTYPE dt;
    size_t esz;
    int err;
    size_t k;

    if (!mem->readable) {
        return TPBE_LIST_NOT_FOUND;
    }
    if (_sf_find_output_by_name(&mem->attr, dname, &idx, &dup) < 0) {
        return TPBE_LIST_NOT_FOUND;
    }
    if (dup) {
        snprintf(warn, warnlen,
                 "WARNING: duplicate output header '%s' in one member record.",
                 dname);
        return TPBE_CLI_FAIL;
    }
    h = &mem->attr.headers[idx];
    if (!_sf_dtype_statable(h->type_bits)) {
        return TPBE_CLI_FAIL;
    }
    err = tpb_raf_header_data_ptr(mem->attr.headers, mem->attr.nheader,
                                  mem->data, mem->datasize, (uint32_t)idx,
                                  &ptr, &nbytes);
    if (err != TPBE_SUCCESS) {
        return err;
    }
    err = _sf_header_nelem(h, &nelem);
    if (err != TPBE_SUCCESS || nelem == 0) {
        return TPBE_CLI_FAIL;
    }
    dt = (TPB_DTYPE)(h->type_bits & TPB_PARM_TYPE_MASK);
    esz = nbytes / nelem;
    if (esz == 0 || esz * nelem != nbytes) {
        return TPBE_CLI_FAIL;
    }
    if (*tb == 0) {
        *tb = h->type_bits;
        *ua = h->uattr_bits;
    } else if (*tb != h->type_bits) {
        *mixed_dtype = 1;
    }
    for (k = 0; k < nelem; k++) {
        const uint8_t *p = (const uint8_t *)ptr + k * esz;
        err = _sf_append_elem_as_double(p, dt, samples, n, cap);
        if (err != TPBE_SUCCESS) {
            return err;
        }
    }
    return TPBE_SUCCESS;
}

int
tpbcli_task_get_result(const char *workspace, const tpbcli_task_gr_opts_t *opts)
{
    tpbcli_task_init_sel_t *sels = NULL;
    int nsels = 0;
    tpbcli_task_logical_t *tasks = NULL;
    int ntasks = 0;
    char **meta_cols = NULL;
    int nmeta = 0;
    char **data_cols = NULL;
    int ndata = 0;
    char **warns = NULL;
    int nwarn = 0;
    int wcap = 0;
    int want_meta = 1;
    int want_data = 1;
    int data_ok = 0;
    int err;
    int i;

    if (workspace == NULL || opts == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }

    err = _sf_gr_resolve_sels(workspace, opts, &sels, &nsels);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    err = tpbcli_task_build_logical_tasks(workspace, sels, nsels, &tasks,
                                          &ntasks);
    free(sels);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    if (opts->data_name_help || opts->meta_name_help) {
        err = _sf_gr_print_name_report(workspace, tasks, ntasks);
        tpbcli_task_free_logical_tasks(tasks, ntasks);
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
        return TPBE_SUCCESS;
    }

    if (opts->ndata_names == 0 && opts->nmeta_names == 0) {
        meta_cols = (char **)calloc((size_t)s_default_meta_n, sizeof(*meta_cols));
        if (meta_cols == NULL) {
            tpbcli_task_free_logical_tasks(tasks, ntasks);
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
        }
        for (i = 0; i < s_default_meta_n; i++) {
            meta_cols[nmeta++] = strdup(s_default_meta[i]);
        }
        err = _sf_gr_collect_shared_data_names(tasks, ntasks, &data_cols, &ndata);
        if (err != TPBE_SUCCESS) {
            tpbcli_task_free_logical_tasks(tasks, ntasks);
            TPB_FAIL(TPB_MOD_CLI_MISC, err, NULL);
        }
        want_meta = 1;
        want_data = (ndata > 0);
    } else if (opts->ndata_names > 0 && opts->nmeta_names == 0) {
        for (i = 0; i < s_default_meta_n; i++) {
            meta_cols = (char **)realloc(meta_cols,
                                       (size_t)(nmeta + 1) * sizeof(*meta_cols));
            meta_cols[nmeta++] = strdup(s_default_meta[i]);
        }
        for (i = 0; i < opts->ndata_names; i++) {
            data_cols = (char **)realloc(data_cols,
                                         (size_t)(ndata + 1) * sizeof(*data_cols));
            data_cols[ndata++] = strdup(opts->data_names[i]);
        }
        want_data = 1;
    } else if (opts->ndata_names == 0 && opts->nmeta_names > 0) {
        for (i = 0; i < opts->nmeta_names; i++) {
            meta_cols = (char **)realloc(meta_cols,
                                         (size_t)(nmeta + 1) * sizeof(*meta_cols));
            meta_cols[nmeta++] = strdup(opts->meta_names[i]);
        }
        err = _sf_gr_collect_all_data_names(tasks, ntasks, &data_cols, &ndata);
        if (err != TPBE_SUCCESS) {
            tpbcli_task_free_logical_tasks(tasks, ntasks);
            TPB_FAIL(TPB_MOD_CLI_MISC, err, NULL);
        }
        want_data = (ndata > 0);
    } else {
        for (i = 0; i < opts->nmeta_names; i++) {
            meta_cols = (char **)realloc(meta_cols,
                                         (size_t)(nmeta + 1) * sizeof(*meta_cols));
            meta_cols[nmeta++] = strdup(opts->meta_names[i]);
        }
        for (i = 0; i < opts->ndata_names; i++) {
            data_cols = (char **)realloc(data_cols,
                                         (size_t)(ndata + 1) * sizeof(*data_cols));
            data_cols[ndata++] = strdup(opts->data_names[i]);
        }
        want_data = 1;
    }

    if (nmeta == 0 && !want_data) {
        tpbcli_task_free_logical_tasks(tasks, ntasks);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }

    {
        char line[512];
        int pos = 0;

        pos += snprintf(line + pos, sizeof(line) - (size_t)pos,
                        "Selected tasks:");
        for (i = 0; i < ntasks; i++) {
            char pfx[8];
            tpbcli_task_format_id_prefix6(tasks[i].root_id, pfx, sizeof(pfx));
            pos += snprintf(line + pos, sizeof(line) - (size_t)pos, " %s(%s)",
                            tasks[i].ref, pfx);
        }
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                        "%s\n", line);
    }
    tpblog_print_hline('-');

    if (want_meta && nmeta > 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                        "Meta Data\n");
        /* header row */
        if (opts->show_each_subrank) {
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                            TPBLOG_FLAG_DIRECT, "Ref Subrank ");
        } else {
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                            TPBLOG_FLAG_DIRECT, "Ref ");
        }
        for (i = 0; i < nmeta; i++) {
            char title[64];
            if (strcmp(meta_cols[i], "batch_host") == 0) {
                snprintf(title, sizeof(title), "BatchHost");
            } else if (strcmp(meta_cols[i], "nmember") == 0) {
                snprintf(title, sizeof(title), "NMember");
            } else if (strcmp(meta_cols[i], "exit_code") == 0) {
                snprintf(title, sizeof(title), "ExitCode");
            } else if (strcmp(meta_cols[i], "taskid") == 0) {
                snprintf(title, sizeof(title), "TaskID");
            } else {
                snprintf(title, sizeof(title), "%s", meta_cols[i]);
            }
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                            TPBLOG_FLAG_DIRECT, "%s ", title);
        }
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                        "\n");

        for (int ti = 0; ti < ntasks; ti++) {
            if (opts->show_each_subrank && tasks[ti].is_capsule) {
                for (int sr = 0; sr < tasks[ti].nmember; sr++) {
                    char cell[128];
                    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                                    TPBLOG_FLAG_DIRECT, "%s %d ",
                                    tasks[ti].ref, sr);
                    for (i = 0; i < nmeta; i++) {
                        _sf_gr_meta_cell(workspace, &tasks[ti], meta_cols[i], 1,
                                         sr, cell, sizeof(cell), NULL, 0);
                        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                                        TPBLOG_FLAG_DIRECT, "%s ", cell);
                    }
                    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                                    TPBLOG_FLAG_DIRECT, "\n");
                }
            } else {
                char cell[128];
                if (opts->show_each_subrank) {
                    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                                    TPBLOG_FLAG_DIRECT, "%s - ", tasks[ti].ref);
                } else {
                    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                                    TPBLOG_FLAG_DIRECT, "%s ", tasks[ti].ref);
                }
                for (i = 0; i < nmeta; i++) {
                    _sf_gr_meta_cell(workspace, &tasks[ti], meta_cols[i], 0, -1,
                                     cell, sizeof(cell), NULL, 0);
                    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                                    TPBLOG_FLAG_DIRECT, "%s ", cell);
                }
                tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                                TPBLOG_FLAG_DIRECT, "\n");
            }
        }
        tpblog_print_hline('-');
    }

    if (want_data && ndata > 0) {
        int any_capsule_agg = 0;

        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                        "Record Data\n");
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                        "Ref Name Type Unit Dim mean min max p25 p50 p75 p90 "
                        "p95 p99\n");

        for (i = 0; i < ndata; i++) {
            for (int ti = 0; ti < ntasks; ti++) {
                if (opts->show_each_subrank) {
                    for (int sr = 0; sr < tasks[ti].nmember; sr++) {
                        double *samples = NULL;
                        size_t ns = 0;
                        size_t cap = 0;
                        uint32_t tb = 0;
                        uint64_t ua = 0;
                        int mixed = 0;
                        char wmsg[256];
                        char typebuf[32];
                        char unitbuf[48];
                        char cells[14][32];
                        int row_ok;

                        wmsg[0] = '\0';
                        row_ok = (_sf_collect_samples_member(
                                      &tasks[ti].members[sr], data_cols[i],
                                      &samples, &ns, &cap, &tb, &ua, &mixed,
                                      wmsg, sizeof(wmsg)) == TPBE_SUCCESS);
                        if (!row_ok) {
                            snprintf(cells[0], sizeof(cells[0]), "N/A");
                            for (int c = 1; c < 14; c++) {
                                snprintf(cells[c], sizeof(cells[c]), "N/A");
                            }
                            if (wmsg[0] != '\0') {
                                (void)_sf_gr_append_warn(&warns, &nwarn, &wcap,
                                                         wmsg);
                            }
                        } else {
                            const char *tname = mixed ? "mixed"
                                : tpbcli_data_fmt_dtype_short(tb);
                            snprintf(typebuf, sizeof(typebuf), "%s", tname);
                            tpbcli_data_fmt_format_unit(ua, unitbuf,
                                                        sizeof(unitbuf));
                            snprintf(cells[0], sizeof(cells[0]), "%zu", ns);
                            if (ns <= 1) {
                                double mean = 0.0;
                                (void)tpb_stat_mean(samples, ns,
                                                    TPB_DOUBLE_T,
                                                    &mean);
                                _sf_format_num(mean, cells[1], sizeof(cells[1]));
                                for (int c = 2; c < 14; c++) {
                                    snprintf(cells[c], sizeof(cells[c]), "-");
                                }
                            } else {
                                double mean, mn, mx;
                                double qout[6];
                                (void)tpb_stat_mean(samples, ns,
                                                    TPB_DOUBLE_T,
                                                    &mean);
                                (void)tpb_stat_min(samples, ns,
                                                   TPB_DOUBLE_T,
                                                   &mn);
                                (void)tpb_stat_max(samples, ns,
                                                   TPB_DOUBLE_T,
                                                   &mx);
                                (void)tpb_stat_qtile_1d(
                                    samples, ns,
                                    TPB_DOUBLE_T,
                                    s_qtiles, 6, qout);
                                _sf_format_num(mean, cells[1], sizeof(cells[1]));
                                _sf_format_num(mn, cells[2], sizeof(cells[2]));
                                _sf_format_num(mx, cells[3], sizeof(cells[3]));
                                for (int c = 0; c < 6; c++) {
                                    _sf_format_num(qout[c], cells[4 + c],
                                                   sizeof(cells[4 + c]));
                                }
                            }
                            data_ok = 1;
                        }
                        tpblog_printf_f(
                            TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                            TPBLOG_FLAG_DIRECT,
                            "%s %d %s %s %s %s %s %s %s %s %s %s %s %s %s\n",
                            tasks[ti].ref, sr, data_cols[i],
                            row_ok ? typebuf : "N/A", row_ok ? unitbuf : "N/A",
                            cells[0], cells[1], cells[2], cells[3], cells[4],
                            cells[5], cells[6], cells[7], cells[8], cells[9]);
                        free(samples);
                    }
                } else {
                    double *samples = NULL;
                    size_t ns = 0;
                    size_t cap = 0;
                    uint32_t tb = 0;
                    uint64_t ua = 0;
                    int mixed = 0;
                    char wmsg[256];
                    char typebuf[32];
                    char unitbuf[48];
                    char cells[14][32];
                    int row_ok = 0;
                    int used = 0;
                    int had_member_warn = 0;

                    wmsg[0] = '\0';
                    if (tasks[ti].is_capsule) {
                        any_capsule_agg = 1;
                    }
                    for (int sr = 0; sr < tasks[ti].nmember; sr++) {
                        char mw[256];
                        mw[0] = '\0';
                        if (_sf_collect_samples_member(&tasks[ti].members[sr],
                                                       data_cols[i], &samples,
                                                       &ns, &cap, &tb, &ua,
                                                       &mixed, mw,
                                                       sizeof(mw)) ==
                            TPBE_SUCCESS) {
                            used++;
                        } else if (mw[0] != '\0') {
                            (void)_sf_gr_append_warn(&warns, &nwarn, &wcap, mw);
                            had_member_warn = 1;
                        }
                    }
                    if (used == 0 && !had_member_warn) {
                        char msg[160];
                        snprintf(msg, sizeof(msg),
                                 "WARNING: Data name %s does not exist in "
                                 "ref=%s.",
                                 data_cols[i], tasks[ti].ref);
                        (void)_sf_gr_append_warn(&warns, &nwarn, &wcap, msg);
                        row_ok = 0;
                    } else {
                        row_ok = 1;
                        data_ok = 1;
                    }
                    if (!row_ok) {
                        for (int c = 0; c < 14; c++) {
                            snprintf(cells[c], sizeof(cells[c]), "N/A");
                        }
                        snprintf(typebuf, sizeof(typebuf), "N/A");
                        snprintf(unitbuf, sizeof(unitbuf), "N/A");
                    } else {
                        const char *tname = mixed ? "mixed"
                            : tpbcli_data_fmt_dtype_short(tb);
                        snprintf(typebuf, sizeof(typebuf), "%s", tname);
                        tpbcli_data_fmt_format_unit(ua, unitbuf,
                                                    sizeof(unitbuf));
                        snprintf(cells[0], sizeof(cells[0]), "%zu", ns);
                        if (ns <= 1) {
                            double mean = 0.0;
                            (void)tpb_stat_mean(samples, ns,
                                                TPB_DOUBLE_T,
                                                &mean);
                            _sf_format_num(mean, cells[1], sizeof(cells[1]));
                            for (int c = 2; c < 14; c++) {
                                snprintf(cells[c], sizeof(cells[c]), "-");
                            }
                        } else {
                            double mean, mn, mx;
                            double qout[6];
                            (void)tpb_stat_mean(samples, ns,
                                                TPB_DOUBLE_T,
                                                &mean);
                            (void)tpb_stat_min(samples, ns,
                                               TPB_DOUBLE_T,
                                               &mn);
                            (void)tpb_stat_max(samples, ns,
                                               TPB_DOUBLE_T,
                                               &mx);
                            (void)tpb_stat_qtile_1d(
                                samples, ns,
                                TPB_DOUBLE_T, s_qtiles,
                                6, qout);
                            _sf_format_num(mean, cells[1], sizeof(cells[1]));
                            _sf_format_num(mn, cells[2], sizeof(cells[2]));
                            _sf_format_num(mx, cells[3], sizeof(cells[3]));
                            for (int c = 0; c < 6; c++) {
                                _sf_format_num(qout[c], cells[4 + c],
                                               sizeof(cells[4 + c]));
                            }
                        }
                    }
                    tpblog_printf_f(
                        TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                        "%s %s %s %s %s %s %s %s %s %s %s %s %s %s\n",
                        tasks[ti].ref, data_cols[i], typebuf, unitbuf,
                        cells[0], cells[1], cells[2], cells[3], cells[4],
                        cells[5], cells[6], cells[7], cells[8], cells[9]);
                    free(samples);
                }
            }
        }
        tpblog_print_hline('-');
        if (any_capsule_agg && !opts->show_each_subrank) {
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                            TPBLOG_FLAG_DIRECT,
                            "Note: capsule member results were aggregated; use "
                            "--show-each-subrank to display each member.\n");
        }
    }

    if (nwarn > 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                        "Warnings\n");
        for (i = 0; i < nwarn; i++) {
            tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN,
                            TPBLOG_FLAG_DIRECT, "%s\n", warns[i]);
        }
        tpblog_print_hline('-');
    }

    for (i = 0; i < nmeta; i++) {
        free(meta_cols[i]);
    }
    free(meta_cols);
    for (i = 0; i < ndata; i++) {
        free(data_cols[i]);
    }
    free(data_cols);
    _sf_gr_free_warnings(warns, nwarn);
    tpbcli_task_free_logical_tasks(tasks, ntasks);

    if (want_data && ndata > 0 && !data_ok) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_METRIC_MISSING, NULL);
    }
    return TPBE_SUCCESS;
}
