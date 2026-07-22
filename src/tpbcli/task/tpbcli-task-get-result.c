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

/* Kind of a resolved `--meta-name` key (design docs/design/tpbcli_task.md §7.4). */
typedef enum tpbcli_gr_mkey_kind {
    TPBCLI_GR_MK_DERIVED = 0,   /**< ref, taskid, kernel, batch_host, nmember */
    TPBCLI_GR_MK_ATTR,         /**< task_attr_t scalar field */
    TPBCLI_GR_MK_HEADER,       /**< header[INDEX].FIELD */
    TPBCLI_GR_MK_INVALID       /**< Unknown key or malformed header syntax */
} tpbcli_gr_mkey_kind_t;

/* One `header[INDEX].FIELD` sub-field (design §7.4). */
typedef enum tpbcli_gr_hfield {
    TPBCLI_GR_HF_BLOCK_SIZE = 0,
    TPBCLI_GR_HF_NDIM,
    TPBCLI_GR_HF_DATA_SIZE,
    TPBCLI_GR_HF_TYPE_BITS,
    TPBCLI_GR_HF_TYPE_BITS_SOURCE,
    TPBCLI_GR_HF_TYPE_BITS_CHECK,
    TPBCLI_GR_HF_TYPE_BITS_TYPE,
    TPBCLI_GR_HF_RESERVE,
    TPBCLI_GR_HF_UATTR_BITS,
    TPBCLI_GR_HF_UATTR_CAST,
    TPBCLI_GR_HF_UATTR_SHAPE,
    TPBCLI_GR_HF_UATTR_TRIM,
    TPBCLI_GR_HF_UATTR_KIND,
    TPBCLI_GR_HF_UATTR_NAME,
    TPBCLI_GR_HF_UATTR_BASE,
    TPBCLI_GR_HF_UATTR_VALUE,
    TPBCLI_GR_HF_NAME,
    TPBCLI_GR_HF_TAG,
    TPBCLI_GR_HF_NOTE,
    TPBCLI_GR_HF_DIMSIZES,
    TPBCLI_GR_HF_DIMNAMES
} tpbcli_gr_hfield_t;

/* Local Function Prototypes */
static int _sf_append_elem_as_double(const void *p, TPB_DTYPE dt, double **arr,
                                     size_t *n, size_t *cap);
static int _sf_collect_samples_member(const tpbcli_task_member_rec_t *mem,
                                      const char *dname, double **samples,
                                      size_t *n, size_t *cap, uint32_t *tb,
                                      uint64_t *ua, int *mixed_dtype,
                                      int *flattened, char *warn,
                                      size_t warnlen);
static int _sf_dtype_statable(uint32_t type_bits);
static int _sf_find_output_by_name(const task_attr_t *attr, const char *name,
                                   int *idx_out, int *dup_out);
static void _sf_format_num(double v, uint64_t uattr_bits, char *out,
                           size_t outlen);
static int _sf_gr_append_warn(char ***warns, int *n, int *cap,
                              const char *msg);
static int _sf_gr_append_warn_unique(char ***warns, int *n, int *cap,
                                     const char *msg);
static int _sf_gr_attr_cell(const task_attr_t *a, const char *key, char *out,
                            size_t outlen);
static int _sf_gr_batch_host(const char *workspace,
                             const unsigned char tbatch_id[20], char *out,
                             size_t outlen, char *warn, size_t warnlen);
static int _sf_gr_collect_all_data_names(const tpbcli_task_logical_t *tasks,
                                         int ntasks, char ***names_out,
                                         int *nnames_out);
static int _sf_gr_collect_shared_data_names(const tpbcli_task_logical_t *tasks,
                                            int ntasks, char ***names_out,
                                            int *nnames_out);
static int _sf_gr_data_compatible(const tpbcli_task_logical_t *lt,
                                  const char *name);
static void _sf_gr_free_warnings(char **warns, int n);
static int _sf_gr_header_cell(const task_attr_t *a, long idx,
                              tpbcli_gr_hfield_t field, char *out,
                              size_t outlen);
static int _sf_gr_hfield_from_str(const char *s, tpbcli_gr_hfield_t *out);
static int _sf_gr_kernel_name(const char *workspace,
                              const unsigned char kid[20], char *out,
                              size_t outlen);
static int _sf_gr_meta_cell(const char *workspace,
                            const tpbcli_task_logical_t *lt, const char *key,
                            int show_each, int subrank, char *out,
                            size_t outlen, char *warn, size_t warnlen);
static int _sf_gr_meta_cell_one(const char *workspace,
                                const tpbcli_task_logical_t *lt,
                                const char *key, int member_idx, char *out,
                                size_t outlen, char *warn, size_t warnlen);
static tpbcli_gr_mkey_kind_t _sf_gr_meta_key_kind(const char *key,
                                                  long *hidx_out,
                                                  tpbcli_gr_hfield_t
                                                      *hfield_out);
static int _sf_gr_parse_header_key(const char *key, long *idx_out,
                                   tpbcli_gr_hfield_t *field_out);
static int _sf_gr_print_name_report(const char *workspace,
                                    tpbcli_task_logical_t *tasks, int ntasks);
static int _sf_gr_resolve_sels(const char *workspace,
                               const tpbcli_task_gr_opts_t *opts,
                               tpbcli_task_init_sel_t **sels_out,
                               int *nsels_out);
static void _sf_gr_subrank_presence(const tpbcli_task_logical_t *lt,
                                    const char *name, char *out,
                                    size_t outlen);
static int _sf_gr_task_max_nheader(const tpbcli_task_logical_t *lt);
static int _sf_gr_validate_meta_names(char *const *names, int n);
static int _sf_header_nelem(const tpb_meta_header_t *h, uint64_t *nelem_out);
static int _sf_name_in_list(const char *name, char **list, int n);
static int _sf_uattr_schema_match(uint64_t a, uint64_t b);

static const char *const s_default_meta[] = {
    "ref", "taskid", "kernel", "batch_host", "datetime", "nmember",
    "exit_code", "ninput", "noutput", "nheader"
};
static const int s_default_meta_n =
    (int)(sizeof(s_default_meta) / sizeof(s_default_meta[0]));

/* Derived meta keys (design §7.4); resolved from the logical task itself. */
static const char *const s_derived_keys[] = {
    "ref", "taskid", "kernel", "batch_host", "nmember"
};
static const int s_derived_keys_n =
    (int)(sizeof(s_derived_keys) / sizeof(s_derived_keys[0]));

/* task_attr_t scalar meta keys (design §7.4). */
static const char *const s_attr_keys[] = {
    "task_record_id", "derive_to", "inherit_from", "tbatch_id", "kernel_id",
    "datetime", "utc_bits", "btime", "duration", "duration_seconds",
    "exit_code", "handle_index", "pid", "tid", "ninput", "noutput",
    "nheader", "reserve"
};
static const int s_attr_keys_n =
    (int)(sizeof(s_attr_keys) / sizeof(s_attr_keys[0]));

/* `header[INDEX].FIELD` sub-field names, parallel to tpbcli_gr_hfield_t. */
static const char *const s_hfield_names[] = {
    "block_size", "ndim", "data_size", "type_bits", "type_bits.source",
    "type_bits.check", "type_bits.type", "_reserve", "uattr_bits",
    "uattr_bits.cast", "uattr_bits.shape", "uattr_bits.trim",
    "uattr_bits.kind", "uattr_bits.name", "uattr_bits.base",
    "uattr_bits.value", "name", "tag", "note", "dimsizes", "dimnames"
};
static const int s_hfield_n =
    (int)(sizeof(s_hfield_names) / sizeof(s_hfield_names[0]));

static double s_qtiles[6] = {0.25, 0.50, 0.75, 0.90, 0.95, 0.99};

static void
_sf_format_num(double v, uint64_t uattr_bits, char *out, size_t outlen)
{
    int trim_disabled;

    if (out == NULL || outlen == 0) {
        return;
    }
    if (!isfinite(v)) {
        snprintf(out, outlen, "N/A");
        return;
    }
    trim_disabled =
        ((uattr_bits & TPB_UATTR_TRIM_MASK) == TPB_UATTR_TRIM_N);
    if (!trim_disabled) {
        (void)tpb_format_value(v, out, outlen, 5, 0);
        return;
    }
    if (v == floor(v)) {
        snprintf(out, outlen, "%.0f", v);
        return;
    }
    {
        int len = snprintf(out, outlen, "%f", v);

        while (len > 0 && out[len - 1] == '0') {
            out[--len] = '\0';
        }
        if (len > 0 && out[len - 1] == '.') {
            out[--len] = '\0';
        }
    }
}

static int
_sf_gr_append_warn(char ***warns, int *n, int *cap, const char *msg)
{
    char **arr;

    if (msg == NULL || msg[0] == '\0') {
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

/*
 * Append a warning unless an identical string is already present, so the
 * same condition observed across several members or keys is only shown once.
 */
static int
_sf_gr_append_warn_unique(char ***warns, int *n, int *cap, const char *msg)
{
    int i;

    if (msg == NULL || msg[0] == '\0') {
        return TPBE_SUCCESS;
    }
    for (i = 0; i < *n; i++) {
        if (strcmp((*warns)[i], msg) == 0) {
            return TPBE_SUCCESS;
        }
    }
    return _sf_gr_append_warn(warns, n, cap, msg);
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

    /*
     * A missing kernel index or an unregistered kernel are both documented
     * fallbacks (design §7.4): show the 6-hex prefix. Neither is a resolution
     * failure, so this always returns TPBE_SUCCESS.
     */
    if (tpb_raf_entry_list_kernel(workspace, &entries, &n) != TPBE_SUCCESS) {
        tpbcli_task_format_id_prefix6(kid, out, outlen);
        return TPBE_SUCCESS;
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

/*
 * Resolve BatchHost from the associated tbatch record. An unreadable tbatch
 * or an empty hostname are documented as "N/A" plus a warning (design §7.4)
 * rather than an error, so consensus does not treat this as a mismatch.
 */
static int
_sf_gr_batch_host(const char *workspace, const unsigned char tbatch_id[20],
                  char *out, size_t outlen, char *warn, size_t warnlen)
{
    tbatch_attr_t attr;
    int err;

    memset(&attr, 0, sizeof(attr));
    err = tpb_raf_record_read_tbatch(workspace, tbatch_id, &attr, NULL, NULL);
    if (err != TPBE_SUCCESS || attr.hostname[0] == '\0') {
        snprintf(out, outlen, "N/A");
        if (warn != NULL && warnlen > 0) {
            snprintf(warn, warnlen,
                     "WARNING: tbatch record unreadable; BatchHost N/A.");
        }
        if (err == TPBE_SUCCESS) {
            tpb_raf_free_headers(attr.headers, attr.nheader);
        }
        return TPBE_SUCCESS;
    }
    snprintf(out, outlen, "%s", attr.hostname);
    tpb_raf_free_headers(attr.headers, attr.nheader);
    return TPBE_SUCCESS;
}

/*
 * Parse strict `header[INDEX].FIELD` syntax. INDEX must be plain decimal
 * digits. Returns TPBE_LIST_NOT_FOUND when the key does not even start with
 * "header[" (not a header key at all), TPBE_CLI_FAIL for malformed header
 * syntax, or TPBE_SUCCESS with idx_out and field_out filled in.
 */
static int
_sf_gr_parse_header_key(const char *key, long *idx_out,
                        tpbcli_gr_hfield_t *field_out)
{
    const char *p;
    const char *digits;
    char numbuf[24];
    size_t len;
    char *end;
    long idx;

    if (strncmp(key, "header[", 7) != 0) {
        return TPBE_LIST_NOT_FOUND;
    }
    p = key + 7;
    digits = p;
    if (!isdigit((unsigned char)*p)) {
        return TPBE_CLI_FAIL;
    }
    while (isdigit((unsigned char)*p)) {
        p++;
    }
    if (*p != ']') {
        return TPBE_CLI_FAIL;
    }
    len = (size_t)(p - digits);
    if (len == 0 || len >= sizeof(numbuf)) {
        return TPBE_CLI_FAIL;
    }
    memcpy(numbuf, digits, len);
    numbuf[len] = '\0';
    idx = strtol(numbuf, &end, 10);
    if (*end != '\0' || idx < 0) {
        return TPBE_CLI_FAIL;
    }
    p++; /* skip ']' */
    if (*p != '.' || p[1] == '\0') {
        return TPBE_CLI_FAIL;
    }
    p++;
    if (!_sf_gr_hfield_from_str(p, field_out)) {
        return TPBE_CLI_FAIL;
    }
    *idx_out = idx;
    return TPBE_SUCCESS;
}

static int
_sf_gr_hfield_from_str(const char *s, tpbcli_gr_hfield_t *out)
{
    int i;

    for (i = 0; i < s_hfield_n; i++) {
        if (strcmp(s, s_hfield_names[i]) == 0) {
            *out = (tpbcli_gr_hfield_t)i;
            return 1;
        }
    }
    return 0;
}

/* Classify a `--meta-name` key into derived / attr / header / invalid. */
static tpbcli_gr_mkey_kind_t
_sf_gr_meta_key_kind(const char *key, long *hidx_out,
                     tpbcli_gr_hfield_t *hfield_out)
{
    int i;

    for (i = 0; i < s_derived_keys_n; i++) {
        if (strcmp(key, s_derived_keys[i]) == 0) {
            return TPBCLI_GR_MK_DERIVED;
        }
    }
    for (i = 0; i < s_attr_keys_n; i++) {
        if (strcmp(key, s_attr_keys[i]) == 0) {
            return TPBCLI_GR_MK_ATTR;
        }
    }
    if (_sf_gr_parse_header_key(key, hidx_out, hfield_out) == TPBE_SUCCESS) {
        return TPBCLI_GR_MK_HEADER;
    }
    return TPBCLI_GR_MK_INVALID;
}

/*
 * Validate all explicit --meta-name keys before any table is printed
 * (design §7.4/§7.8): an unknown key name or malformed header[INDEX].FIELD
 * syntax is a parameter error, not a per-row N/A.
 */
static int
_sf_gr_validate_meta_names(char *const *names, int n)
{
    long hidx;
    tpbcli_gr_hfield_t hfield;
    int i;

    for (i = 0; i < n; i++) {
        if (_sf_gr_meta_key_kind(names[i], &hidx, &hfield) ==
            TPBCLI_GR_MK_INVALID) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT,
                            "error: unknown --meta-name key '%s'\n",
                            names[i]);
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
    }
    return TPBE_SUCCESS;
}

/* Resolve one task_attr_t scalar meta key (design §7.4). */
static int
_sf_gr_attr_cell(const task_attr_t *a, const char *key, char *out,
                 size_t outlen)
{
    char hx[41];

    if (strcmp(key, "task_record_id") == 0) {
        tpb_raf_id_to_hex(a->task_record_id, hx);
        snprintf(out, outlen, "%s", hx);
    } else if (strcmp(key, "derive_to") == 0) {
        tpb_raf_id_to_hex(a->derive_to, hx);
        snprintf(out, outlen, "%s", hx);
    } else if (strcmp(key, "inherit_from") == 0) {
        tpb_raf_id_to_hex(a->inherit_from, hx);
        snprintf(out, outlen, "%s", hx);
    } else if (strcmp(key, "tbatch_id") == 0) {
        tpb_raf_id_to_hex(a->tbatch_id, hx);
        snprintf(out, outlen, "%s", hx);
    } else if (strcmp(key, "kernel_id") == 0) {
        tpb_raf_id_to_hex(a->kernel_id, hx);
        snprintf(out, outlen, "%s", hx);
    } else if (strcmp(key, "datetime") == 0) {
        if (tpbcli_task_time_format_utc(a->utc_bits, out, outlen) !=
            TPBE_SUCCESS) {
            snprintf(out, outlen, "N/A");
        }
    } else if (strcmp(key, "utc_bits") == 0) {
        snprintf(out, outlen, "%" PRIu64, (uint64_t)a->utc_bits);
    } else if (strcmp(key, "btime") == 0) {
        snprintf(out, outlen, "%" PRIu64, a->btime);
    } else if (strcmp(key, "duration") == 0) {
        snprintf(out, outlen, "%" PRIu64, a->duration);
    } else if (strcmp(key, "duration_seconds") == 0) {
        snprintf(out, outlen, "%.9f", (double)a->duration / 1e9);
    } else if (strcmp(key, "exit_code") == 0) {
        snprintf(out, outlen, "%u", a->exit_code);
    } else if (strcmp(key, "handle_index") == 0) {
        snprintf(out, outlen, "%u", a->handle_index);
    } else if (strcmp(key, "pid") == 0) {
        snprintf(out, outlen, "%u", a->pid);
    } else if (strcmp(key, "tid") == 0) {
        snprintf(out, outlen, "%u", a->tid);
    } else if (strcmp(key, "ninput") == 0) {
        snprintf(out, outlen, "%u", a->ninput);
    } else if (strcmp(key, "noutput") == 0) {
        snprintf(out, outlen, "%u", a->noutput);
    } else if (strcmp(key, "nheader") == 0) {
        snprintf(out, outlen, "%u", a->nheader);
    } else if (strcmp(key, "reserve") == 0) {
        snprintf(out, outlen, "%u", a->reserve);
    } else {
        return TPBE_LIST_NOT_FOUND;
    }
    return TPBE_SUCCESS;
}

/*
 * Resolve one `header[idx].field` meta key. Bit-field values and recognized
 * sub-fields show `0xHEX (symbolic)` like `db dump`; unrecognized or
 * out-of-range indices are TPBE_LIST_NOT_FOUND so the caller can show N/A
 * plus a warning.
 */
static int
_sf_gr_header_cell(const task_attr_t *a, long idx, tpbcli_gr_hfield_t field,
                   char *out, size_t outlen)
{
    const tpb_meta_header_t *h;
    char decode[512];

    if (idx < 0 || a->headers == NULL || (uint32_t)idx >= a->nheader) {
        return TPBE_LIST_NOT_FOUND;
    }
    h = &a->headers[idx];
    switch (field) {
    case TPBCLI_GR_HF_BLOCK_SIZE:
        snprintf(out, outlen, "%" PRIu32, h->block_size);
        break;
    case TPBCLI_GR_HF_NDIM:
        snprintf(out, outlen, "%" PRIu32, h->ndim);
        break;
    case TPBCLI_GR_HF_DATA_SIZE:
        snprintf(out, outlen, "%" PRIu64, h->data_size);
        break;
    case TPBCLI_GR_HF_TYPE_BITS:
        tpbcli_data_fmt_decode_type_bits(h->type_bits, decode, sizeof(decode));
        snprintf(out, outlen, "0x%08" PRIX32 " (%s)", h->type_bits, decode);
        break;
    case TPBCLI_GR_HF_TYPE_BITS_SOURCE:
        {
            uint32_t m = h->type_bits & (uint32_t)TPB_PARM_SOURCE_MASK;

            tpbcli_data_fmt_decode_type_bits(m, decode, sizeof(decode));
            snprintf(out, outlen, "0x%08" PRIX32 " (%s)", m, decode);
        }
        break;
    case TPBCLI_GR_HF_TYPE_BITS_CHECK:
        {
            uint32_t m = h->type_bits & (uint32_t)TPB_PARM_CHECK_MASK;

            tpbcli_data_fmt_decode_type_bits(m, decode, sizeof(decode));
            snprintf(out, outlen, "0x%08" PRIX32 " (%s)", m, decode);
        }
        break;
    case TPBCLI_GR_HF_TYPE_BITS_TYPE:
        {
            uint32_t m = h->type_bits & (uint32_t)TPB_PARM_TYPE_MASK;

            tpbcli_data_fmt_decode_type_bits(m, decode, sizeof(decode));
            snprintf(out, outlen, "0x%08" PRIX32 " (%s)", m, decode);
        }
        break;
    case TPBCLI_GR_HF_RESERVE:
        snprintf(out, outlen, "%" PRIu32, h->_reserve);
        break;
    case TPBCLI_GR_HF_UATTR_BITS:
        tpbcli_data_fmt_decode_uattr_bits(h->uattr_bits, decode,
                                          sizeof(decode));
        snprintf(out, outlen, "0x%016" PRIX64 " (%s)",
                 (uint64_t)h->uattr_bits, decode);
        break;
    case TPBCLI_GR_HF_UATTR_CAST:
        {
            uint64_t m = (uint64_t)(h->uattr_bits & TPB_UATTR_CAST_MASK);

            tpbcli_data_fmt_decode_uattr_bits(m, decode, sizeof(decode));
            snprintf(out, outlen, "0x%016" PRIX64 " (%s)", m, decode);
        }
        break;
    case TPBCLI_GR_HF_UATTR_SHAPE:
        {
            uint64_t m = (uint64_t)(h->uattr_bits & TPB_UATTR_SHAPE_MASK);

            tpbcli_data_fmt_decode_uattr_bits(m, decode, sizeof(decode));
            snprintf(out, outlen, "0x%016" PRIX64 " (%s)", m, decode);
        }
        break;
    case TPBCLI_GR_HF_UATTR_TRIM:
        {
            uint64_t m = (uint64_t)(h->uattr_bits & TPB_UATTR_TRIM_MASK);

            tpbcli_data_fmt_decode_uattr_bits(m, decode, sizeof(decode));
            snprintf(out, outlen, "0x%016" PRIX64 " (%s)", m, decode);
        }
        break;
    case TPBCLI_GR_HF_UATTR_KIND:
        {
            uint64_t m = (uint64_t)(h->uattr_bits & TPB_UKIND_MASK);

            tpbcli_data_fmt_decode_uattr_bits(m, decode, sizeof(decode));
            snprintf(out, outlen, "0x%016" PRIX64 " (%s)", m, decode);
        }
        break;
    case TPBCLI_GR_HF_UATTR_NAME:
        {
            uint64_t m = (uint64_t)(h->uattr_bits & TPB_UNAME_MASK);

            tpbcli_data_fmt_decode_uattr_bits(m, decode, sizeof(decode));
            snprintf(out, outlen, "0x%016" PRIX64 " (%s)", m, decode);
        }
        break;
    case TPBCLI_GR_HF_UATTR_BASE:
        {
            uint64_t m = (uint64_t)(h->uattr_bits & TPB_UBASE_MASK);

            tpbcli_data_fmt_decode_uattr_bits(m, decode, sizeof(decode));
            snprintf(out, outlen, "0x%016" PRIX64 " (%s)", m, decode);
        }
        break;
    case TPBCLI_GR_HF_UATTR_VALUE:
        {
            uint64_t m = (uint64_t)(h->uattr_bits & TPB_UNIT_MASK);

            tpbcli_data_fmt_decode_uattr_bits(m, decode, sizeof(decode));
            snprintf(out, outlen, "0x%016" PRIX64 " (%s)", m, decode);
        }
        break;
    case TPBCLI_GR_HF_NAME:
        snprintf(out, outlen, "%s", h->name);
        break;
    case TPBCLI_GR_HF_TAG:
        snprintf(out, outlen, "%s", h->tag);
        break;
    case TPBCLI_GR_HF_NOTE:
        snprintf(out, outlen, "%s", h->note);
        break;
    case TPBCLI_GR_HF_DIMSIZES:
        {
            char dsz[256];
            uint32_t j;

            dsz[0] = '\0';
            for (j = 0; j < (uint32_t)TPBM_DATA_NDIM_MAX; j++) {
                char part[32];

                snprintf(part, sizeof(part), "%s%" PRIu64, j ? "," : "",
                         h->dimsizes[j]);
                if (strlen(dsz) + strlen(part) + 1 < sizeof(dsz)) {
                    strcat(dsz, part);
                }
            }
            snprintf(out, outlen, "%s", dsz);
        }
        break;
    case TPBCLI_GR_HF_DIMNAMES:
        {
            char dnm[512];
            uint32_t j;

            dnm[0] = '\0';
            for (j = 0; j < (uint32_t)TPBM_DATA_NDIM_MAX; j++) {
                char part[80];

                snprintf(part, sizeof(part), "%s%s", j ? "," : "",
                         h->dimnames[j]);
                if (strlen(dnm) + strlen(part) + 1 < sizeof(dnm)) {
                    strcat(dnm, part);
                }
            }
            snprintf(out, outlen, "%s", dnm);
        }
        break;
    default:
        return TPBE_LIST_NOT_FOUND;
    }
    return TPBE_SUCCESS;
}

/*
 * Resolve one meta key for one member of a logical task. `ref`/`taskid`/
 * `nmember` are properties of the logical task and never need a readable
 * member. All other keys need lt->members[member_idx] to be readable.
 */
static int
_sf_gr_meta_cell_one(const char *workspace, const tpbcli_task_logical_t *lt,
                     const char *key, int member_idx, char *out,
                     size_t outlen, char *warn, size_t warnlen)
{
    const task_attr_t *a;
    tpbcli_gr_mkey_kind_t kind;
    long hidx = 0;
    tpbcli_gr_hfield_t hfield = TPBCLI_GR_HF_NAME;
    int err;

    if (strcmp(key, "ref") == 0) {
        snprintf(out, outlen, "%s", lt->ref);
        return TPBE_SUCCESS;
    }
    if (strcmp(key, "taskid") == 0) {
        tpbcli_task_format_id_prefix6(lt->root_id, out, outlen);
        return TPBE_SUCCESS;
    }
    if (strcmp(key, "nmember") == 0) {
        snprintf(out, outlen, "%d", lt->is_capsule ? lt->nmember : 1);
        return TPBE_SUCCESS;
    }
    if (member_idx < 0 || member_idx >= lt->nmember ||
        !lt->members[member_idx].readable) {
        snprintf(out, outlen, "N/A");
        return TPBE_LIST_NOT_FOUND;
    }
    a = &lt->members[member_idx].attr;
    if (strcmp(key, "kernel") == 0) {
        return _sf_gr_kernel_name(workspace, a->kernel_id, out, outlen);
    }
    if (strcmp(key, "batch_host") == 0) {
        return _sf_gr_batch_host(workspace, a->tbatch_id, out, outlen, warn,
                                 warnlen);
    }
    kind = _sf_gr_meta_key_kind(key, &hidx, &hfield);
    if (kind == TPBCLI_GR_MK_ATTR) {
        err = _sf_gr_attr_cell(a, key, out, outlen);
        if (err != TPBE_SUCCESS) {
            snprintf(out, outlen, "N/A");
        }
        return err;
    }
    if (kind == TPBCLI_GR_MK_HEADER) {
        err = _sf_gr_header_cell(a, hidx, hfield, out, outlen);
        if (err != TPBE_SUCCESS) {
            snprintf(out, outlen, "N/A");
            if (warn != NULL && warnlen > 0) {
                snprintf(warn, warnlen,
                         "WARNING: header[%ld] out of range for ref=%s.",
                         hidx, lt->ref);
            }
        }
        return err;
    }
    snprintf(out, outlen, "N/A");
    return TPBE_LIST_NOT_FOUND;
}

/*
 * Resolve one meta key for a whole logical task: a single member row when
 * --show-each-subrank is active, or the member consensus otherwise (design
 * §7.4). Capsule datetime/utc_bits/btime use the work-root (capsule entry)
 * in aggregate mode. Consensus is computed only from readable members; the
 * first per-member warning (e.g. an unreadable tbatch) is forwarded to the
 * caller.
 */
static int
_sf_gr_meta_cell(const char *workspace, const tpbcli_task_logical_t *lt,
                 const char *key, int show_each, int subrank, char *out,
                 size_t outlen, char *warn, size_t warnlen)
{
    char tmp[192];
    char mwarn[192];
    int mixed = 0;
    int first = 1;
    int used = 0;
    int m;

    if (show_each) {
        (void)_sf_gr_meta_cell_one(workspace, lt, key, subrank, out, outlen,
                                   warn, warnlen);
        return TPBE_SUCCESS;
    }
    if (!lt->is_capsule || strcmp(key, "ref") == 0 ||
        strcmp(key, "taskid") == 0 || strcmp(key, "nmember") == 0) {
        (void)_sf_gr_meta_cell_one(workspace, lt, key, 0, out, outlen, warn,
                                   warnlen);
        return TPBE_SUCCESS;
    }
    /* Work-root time fields: capsule entry, not member consensus (§7.4). */
    if (strcmp(key, "datetime") == 0) {
        if (tpbcli_task_time_format_utc(lt->root_utc_bits, out, outlen) !=
            TPBE_SUCCESS) {
            snprintf(out, outlen, "N/A");
        }
        return TPBE_SUCCESS;
    }
    if (strcmp(key, "utc_bits") == 0) {
        snprintf(out, outlen, "%" PRIu64, (uint64_t)lt->root_utc_bits);
        return TPBE_SUCCESS;
    }
    if (strcmp(key, "btime") == 0) {
        snprintf(out, outlen, "%" PRIu64, lt->root_btime);
        return TPBE_SUCCESS;
    }
    for (m = 0; m < lt->nmember; m++) {
        if (!lt->members[m].readable) {
            continue;
        }
        mwarn[0] = '\0';
        if (_sf_gr_meta_cell_one(workspace, lt, key, m, tmp, sizeof(tmp),
                                 mwarn, sizeof(mwarn)) != TPBE_SUCCESS) {
            continue;
        }
        used++;
        if (mwarn[0] != '\0' && warn != NULL && warnlen > 0 &&
            warn[0] == '\0') {
            snprintf(warn, warnlen, "%s", mwarn);
        }
        if (first) {
            snprintf(out, outlen, "%s", tmp);
            first = 0;
        } else if (strcmp(out, tmp) != 0) {
            mixed = 1;
        }
    }
    if (used == 0) {
        snprintf(out, outlen, "N/A");
        return TPBE_SUCCESS;
    }
    if (mixed) {
        snprintf(out, outlen, "mixed");
    }
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

/* Highest nheader among this task's readable members (0 when none). */
static int
_sf_gr_task_max_nheader(const tpbcli_task_logical_t *lt)
{
    int m;
    uint32_t mx = 0;

    for (m = 0; m < lt->nmember; m++) {
        if (lt->members[m].readable && lt->members[m].attr.nheader > mx) {
            mx = lt->members[m].attr.nheader;
        }
    }
    return (int)mx;
}

/*
 * Format the subrank range where an output header name is present, e.g.
 * "[subrank=0,2-3]" (design §7.3). Returns an empty string when the name is
 * present in none or all readable members (no partial-presence note needed).
 */
static void
_sf_gr_subrank_presence(const tpbcli_task_logical_t *lt, const char *name,
                        char *out, size_t outlen)
{
    int *present;
    int npresent = 0;
    int nreadable = 0;
    int m;
    int pos;
    int run_start = -1;
    int prev = -1;
    int first = 1;

    out[0] = '\0';
    if (!lt->is_capsule || lt->nmember <= 0) {
        return;
    }
    present = (int *)calloc((size_t)lt->nmember, sizeof(int));
    if (present == NULL) {
        return;
    }
    for (m = 0; m < lt->nmember; m++) {
        int idx;
        int dup;

        if (!lt->members[m].readable) {
            continue;
        }
        nreadable++;
        if (_sf_find_output_by_name(&lt->members[m].attr, name, &idx, &dup) >=
                0 &&
            !dup) {
            present[m] = 1;
            npresent++;
        }
    }
    if (npresent == 0 || npresent == nreadable) {
        free(present);
        return;
    }
    pos = snprintf(out, outlen, "[subrank=");
    for (m = 0; m <= lt->nmember; m++) {
        int is_in = (m < lt->nmember) && present[m];

        if (is_in) {
            if (run_start < 0) {
                run_start = m;
            }
            prev = m;
            continue;
        }
        if (run_start < 0) {
            continue;
        }
        if (!first && pos > 0 && (size_t)pos < outlen) {
            pos += snprintf(out + pos, outlen - (size_t)pos, ",");
        }
        first = 0;
        if (run_start == prev) {
            pos += snprintf(out + pos, outlen - (size_t)pos, "%d", run_start);
        } else {
            pos += snprintf(out + pos, outlen - (size_t)pos, "%d-%d",
                            run_start, prev);
        }
        run_start = -1;
    }
    if (pos > 0 && (size_t)pos < outlen) {
        snprintf(out + pos, outlen - (size_t)pos, "]");
    }
    free(present);
}

/*
 * Print the `--data-name --help` / `--meta-name --help` context report
 * (design §7.3): Shared meta/data across all selected logical tasks, then
 * each task's private meta/data. Derived and task_attr_t scalar meta keys
 * always resolve, so they are always shared; only header[idx].field keys and
 * output data names can differ per logical task.
 */
static int
_sf_gr_print_name_report(const char *workspace, tpbcli_task_logical_t *tasks,
                         int ntasks)
{
    char **shared_data = NULL;
    int nsd = 0;
    int t;
    int tt;
    int idx;
    int f;
    int err;

    (void)workspace;

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "Shared meta name:");
    for (idx = 0; idx < s_derived_keys_n; idx++) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                        TPBLOG_FLAG_DIRECT, " %s", s_derived_keys[idx]);
    }
    for (idx = 0; idx < s_attr_keys_n; idx++) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                        TPBLOG_FLAG_DIRECT, " %s", s_attr_keys[idx]);
    }
    if (ntasks > 0) {
        int max0 = _sf_gr_task_max_nheader(&tasks[0]);

        for (idx = 0; idx < max0; idx++) {
            int shared_here = 1;

            for (tt = 1; tt < ntasks; tt++) {
                if (idx >= _sf_gr_task_max_nheader(&tasks[tt])) {
                    shared_here = 0;
                    break;
                }
            }
            if (!shared_here) {
                continue;
            }
            for (f = 0; f < s_hfield_n; f++) {
                tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                                TPBLOG_FLAG_DIRECT, " header[%d].%s", idx,
                                s_hfield_names[f]);
            }
        }
    }
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "\n");

    err = _sf_gr_collect_shared_data_names(tasks, ntasks, &shared_data, &nsd);
    if (err != TPBE_SUCCESS) {
        return err;
    }
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "Shared data name:");
    for (idx = 0; idx < nsd; idx++) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                        TPBLOG_FLAG_DIRECT, " %s", shared_data[idx]);
    }
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "\n");

    for (t = 0; t < ntasks; t++) {
        int max_t = _sf_gr_task_max_nheader(&tasks[t]);
        char **allnames = NULL;
        int nall = 0;

        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                        TPBLOG_FLAG_DIRECT, "Private meta name (ref=%s):",
                        tasks[t].ref);
        for (idx = 0; idx < max_t; idx++) {
            int shared_here = 1;

            for (tt = 0; tt < ntasks; tt++) {
                if (idx >= _sf_gr_task_max_nheader(&tasks[tt])) {
                    shared_here = 0;
                    break;
                }
            }
            if (shared_here) {
                continue;
            }
            for (f = 0; f < s_hfield_n; f++) {
                tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                                TPBLOG_FLAG_DIRECT, " header[%d].%s", idx,
                                s_hfield_names[f]);
            }
        }
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                        TPBLOG_FLAG_DIRECT, "\n");

        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                        TPBLOG_FLAG_DIRECT, "Private data name (ref=%s):",
                        tasks[t].ref);
        if (_sf_gr_collect_all_data_names(&tasks[t], 1, &allnames, &nall) ==
            TPBE_SUCCESS) {
            for (idx = 0; idx < nall; idx++) {
                char rangebuf[128];

                if (_sf_name_in_list(allnames[idx], shared_data, nsd)) {
                    continue;
                }
                _sf_gr_subrank_presence(&tasks[t], allnames[idx], rangebuf,
                                        sizeof(rangebuf));
                tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                                TPBLOG_FLAG_DIRECT, " %s%s", allnames[idx],
                                rangebuf);
            }
            for (idx = 0; idx < nall; idx++) {
                free(allnames[idx]);
            }
            free(allnames);
        }
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                        TPBLOG_FLAG_DIRECT, "\n");
    }

    for (idx = 0; idx < nsd; idx++) {
        free(shared_data[idx]);
    }
    free(shared_data);
    return TPBE_SUCCESS;
}

/*
 * Collect one member's samples for a data name into the shared (samples, n,
 * cap) accumulator. On the first successful member, tb and ua record the
 * established type/unit schema. Later members must match that schema
 * (design §7.5): a unit/shape mismatch is rejected without appending any
 * samples, while a different-but-statable numeric type is allowed and marks
 * *mixed_dtype. A multi-dimensional header sets *flattened so the caller can
 * warn once per ref/name that it flattened storage order.
 */
static int
_sf_collect_samples_member(const tpbcli_task_member_rec_t *mem,
                           const char *dname, double **samples, size_t *n,
                           size_t *cap, uint32_t *tb, uint64_t *ua,
                           int *mixed_dtype, int *flattened, char *warn,
                           size_t warnlen)
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
    if (*n == 0) {
        *tb = h->type_bits;
        *ua = h->uattr_bits;
    } else {
        if (!_sf_uattr_schema_match(*ua, h->uattr_bits)) {
            snprintf(warn, warnlen,
                     "WARNING: unit/shape mismatch for data '%s' across "
                     "members.",
                     dname);
            return TPBE_CLI_FAIL;
        }
        if (*tb != h->type_bits) {
            *mixed_dtype = 1;
        }
    }
    if (flattened != NULL && h->ndim > 1) {
        *flattened = 1;
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

/**
 * @brief Implement `tpbcli task get-result|gr`.
 */
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

    err = _sf_gr_validate_meta_names(opts->meta_names, opts->nmeta_names);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    err = _sf_gr_resolve_sels(workspace, opts, &sels, &nsels);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    err = tpbcli_task_build_logical_tasks(workspace, sels, nsels, &tasks,
                                          &ntasks);
    free(sels);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    {
        int navail = 0;
        int ti;
        int m;
        int j;

        for (ti = 0; ti < ntasks; ti++) {
            int nreadable = 0;

            for (m = 0; m < tasks[ti].nmember; m++) {
                if (tasks[ti].members[m].readable) {
                    nreadable++;
                }
            }
            if (nreadable > 0) {
                navail++;
            } else if (tasks[ti].is_capsule) {
                char msg[192];

                snprintf(msg, sizeof(msg),
                         "WARNING: used 0/%d members for ref=%s; logical "
                         "task unavailable.",
                         tasks[ti].nmember, tasks[ti].ref);
                (void)_sf_gr_append_warn_unique(&warns, &nwarn, &wcap, msg);
            }
            if (tasks[ti].is_capsule) {
                for (m = 0; m < tasks[ti].nmember; m++) {
                    for (j = 0; j < m; j++) {
                        if (memcmp(tasks[ti].members[m].id,
                                   tasks[ti].members[j].id, 20) == 0) {
                            char msg[224];

                            snprintf(msg, sizeof(msg),
                                     "WARNING: duplicate member TaskRecordID "
                                     "at subrank %d (same as subrank %d) for "
                                     "ref=%s.",
                                     m, j, tasks[ti].ref);
                            (void)_sf_gr_append_warn_unique(&warns, &nwarn,
                                                            &wcap, msg);
                            break;
                        }
                    }
                }
            }
        }
        if (navail == 0) {
            if (nwarn > 0) {
                tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                                TPBLOG_FLAG_DIRECT, "Warnings\n");
                for (i = 0; i < nwarn; i++) {
                    tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN,
                                    TPBLOG_FLAG_DIRECT, "%s\n", warns[i]);
                }
            }
            _sf_gr_free_warnings(warns, nwarn);
            tpbcli_task_free_logical_tasks(tasks, ntasks);
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT,
                            "error: all logical tasks unavailable\n");
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
    }

    if (opts->data_name_help || opts->meta_name_help) {
        err = _sf_gr_print_name_report(workspace, tasks, ntasks);
        _sf_gr_free_warnings(warns, nwarn);
        tpbcli_task_free_logical_tasks(tasks, ntasks);
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
        return TPBE_SUCCESS;
    }

    if (opts->ndata_names == 0 && opts->nmeta_names == 0 &&
        !opts->data_name_given) {
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
    } else if (opts->ndata_names == 0) {
        /* No --data-name, or --data-name '': Meta only, omit Record Data. */
        if (opts->nmeta_names > 0) {
            for (i = 0; i < opts->nmeta_names; i++) {
                meta_cols = (char **)realloc(
                    meta_cols, (size_t)(nmeta + 1) * sizeof(*meta_cols));
                meta_cols[nmeta++] = strdup(opts->meta_names[i]);
            }
        } else {
            for (i = 0; i < s_default_meta_n; i++) {
                meta_cols = (char **)realloc(
                    meta_cols, (size_t)(nmeta + 1) * sizeof(*meta_cols));
                meta_cols[nmeta++] = strdup(s_default_meta[i]);
            }
        }
        want_data = 0;
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
        for (int ti = 0; ti < ntasks; ti++) {
            int nreadable = 0;
            int m;

            if (!tasks[ti].is_capsule) {
                continue;
            }
            for (m = 0; m < tasks[ti].nmember; m++) {
                if (tasks[ti].members[m].readable) {
                    nreadable++;
                }
            }
            if (nreadable == 0) {
                char msg[192];
                snprintf(msg, sizeof(msg),
                         "WARNING: used 0/%d members for ref=%s meta.",
                         tasks[ti].nmember, tasks[ti].ref);
                (void)_sf_gr_append_warn_unique(&warns, &nwarn, &wcap, msg);
            } else if (nreadable < tasks[ti].nmember) {
                char msg[160];
                snprintf(msg, sizeof(msg),
                         "WARNING: used %d/%d members for ref=%s meta.",
                         nreadable, tasks[ti].nmember, tasks[ti].ref);
                (void)_sf_gr_append_warn_unique(&warns, &nwarn, &wcap, msg);
            }
        }

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
                    char cell[512];
                    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                                    TPBLOG_FLAG_DIRECT, "%s %d ",
                                    tasks[ti].ref, sr);
                    for (i = 0; i < nmeta; i++) {
                        char cwarn[192];
                        cwarn[0] = '\0';
                        _sf_gr_meta_cell(workspace, &tasks[ti], meta_cols[i],
                                         1, sr, cell, sizeof(cell), cwarn,
                                         sizeof(cwarn));
                        if (cwarn[0] != '\0') {
                            (void)_sf_gr_append_warn_unique(&warns, &nwarn,
                                                            &wcap, cwarn);
                        }
                        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                                        TPBLOG_FLAG_DIRECT, "%s ", cell);
                    }
                    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                                    TPBLOG_FLAG_DIRECT, "\n");
                }
            } else {
                char cell[512];
                if (opts->show_each_subrank) {
                    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                                    TPBLOG_FLAG_DIRECT, "%s - ", tasks[ti].ref);
                } else {
                    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                                    TPBLOG_FLAG_DIRECT, "%s ", tasks[ti].ref);
                }
                for (i = 0; i < nmeta; i++) {
                    char cwarn[192];
                    cwarn[0] = '\0';
                    _sf_gr_meta_cell(workspace, &tasks[ti], meta_cols[i], 0,
                                     -1, cell, sizeof(cell), cwarn,
                                     sizeof(cwarn));
                    if (cwarn[0] != '\0') {
                        (void)_sf_gr_append_warn_unique(&warns, &nwarn, &wcap,
                                                        cwarn);
                    }
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
                        int flat_ignore = 0;
                        char wmsg[256];
                        char typebuf[32];
                        char unitbuf[48];
                        char cells[14][32];
                        int row_ok;

                        wmsg[0] = '\0';
                        row_ok = (_sf_collect_samples_member(
                                      &tasks[ti].members[sr], data_cols[i],
                                      &samples, &ns, &cap, &tb, &ua, &mixed,
                                      &flat_ignore, wmsg,
                                      sizeof(wmsg)) == TPBE_SUCCESS);
                        if (!row_ok) {
                            snprintf(cells[0], sizeof(cells[0]), "N/A");
                            for (int c = 1; c < 14; c++) {
                                snprintf(cells[c], sizeof(cells[c]), "N/A");
                            }
                            if (wmsg[0] != '\0') {
                                (void)_sf_gr_append_warn_unique(&warns, &nwarn,
                                                                &wcap, wmsg);
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
                                _sf_format_num(mean, ua, cells[1], sizeof(cells[1]));
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
                                _sf_format_num(mean, ua, cells[1], sizeof(cells[1]));
                                _sf_format_num(mn, ua, cells[2], sizeof(cells[2]));
                                _sf_format_num(mx, ua, cells[3], sizeof(cells[3]));
                                for (int c = 0; c < 6; c++) {
                                    _sf_format_num(qout[c], ua, cells[4 + c],
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
                    int flattened = 0;
                    char typebuf[32];
                    char unitbuf[48];
                    char cells[14][32];
                    int row_ok;
                    int used = 0;
                    int n_readable = 0;
                    int had_member_warn = 0;

                    if (tasks[ti].is_capsule) {
                        any_capsule_agg = 1;
                    }
                    for (int sr = 0; sr < tasks[ti].nmember; sr++) {
                        char mw[256];
                        int mflat = 0;

                        if (!tasks[ti].members[sr].readable) {
                            continue;
                        }
                        n_readable++;
                        mw[0] = '\0';
                        if (_sf_collect_samples_member(
                                &tasks[ti].members[sr], data_cols[i],
                                &samples, &ns, &cap, &tb, &ua, &mixed, &mflat,
                                mw, sizeof(mw)) == TPBE_SUCCESS) {
                            used++;
                            if (mflat) {
                                flattened = 1;
                            }
                        } else if (mw[0] != '\0') {
                            (void)_sf_gr_append_warn_unique(&warns, &nwarn,
                                                            &wcap, mw);
                            had_member_warn = 1;
                        }
                    }
                    /* Success requires both a used member and samples;
                     * had_member_warn alone never counts as success. */
                    row_ok = (used > 0 && ns > 0);
                    if (!row_ok) {
                        if (!had_member_warn) {
                            char msg[160];
                            snprintf(msg, sizeof(msg),
                                     "WARNING: Data name %s does not exist "
                                     "in ref=%s.",
                                     data_cols[i], tasks[ti].ref);
                            (void)_sf_gr_append_warn_unique(&warns, &nwarn,
                                                            &wcap, msg);
                        }
                        for (int c = 0; c < 14; c++) {
                            snprintf(cells[c], sizeof(cells[c]), "N/A");
                        }
                        snprintf(typebuf, sizeof(typebuf), "N/A");
                        snprintf(unitbuf, sizeof(unitbuf), "N/A");
                    } else {
                        const char *tname = mixed ? "mixed"
                            : tpbcli_data_fmt_dtype_short(tb);

                        data_ok = 1;
                        if (used < n_readable) {
                            char msg[160];
                            snprintf(msg, sizeof(msg),
                                     "WARNING: used %d/%d members for ref=%s "
                                     "data %s.",
                                     used, n_readable, tasks[ti].ref,
                                     data_cols[i]);
                            (void)_sf_gr_append_warn_unique(&warns, &nwarn,
                                                            &wcap, msg);
                        }
                        if (flattened) {
                            char msg[192];
                            snprintf(msg, sizeof(msg),
                                     "WARNING: ref=%s data %s has "
                                     "multi-dimensional output; flattened by "
                                     "storage order.",
                                     tasks[ti].ref, data_cols[i]);
                            (void)_sf_gr_append_warn_unique(&warns, &nwarn,
                                                            &wcap, msg);
                        }
                        snprintf(typebuf, sizeof(typebuf), "%s", tname);
                        tpbcli_data_fmt_format_unit(ua, unitbuf,
                                                    sizeof(unitbuf));
                        snprintf(cells[0], sizeof(cells[0]), "%zu", ns);
                        if (ns <= 1) {
                            double mean = 0.0;
                            (void)tpb_stat_mean(samples, ns,
                                                TPB_DOUBLE_T,
                                                &mean);
                            _sf_format_num(mean, ua, cells[1], sizeof(cells[1]));
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
                            _sf_format_num(mean, ua, cells[1], sizeof(cells[1]));
                            _sf_format_num(mn, ua, cells[2], sizeof(cells[2]));
                            _sf_format_num(mx, ua, cells[3], sizeof(cells[3]));
                            for (int c = 0; c < 6; c++) {
                                _sf_format_num(qout[c], ua, cells[4 + c],
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
