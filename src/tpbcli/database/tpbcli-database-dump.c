/*
 * tpbcli-database-dump.c
 * `tpbcli database dump` — CSV-style dump of rafdb .tpbr/.tpbe files.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#ifdef __linux__
#include <linux/limits.h>
#else
#include <limits.h>
#endif

#include "tpb-public.h"
#include "tpbcli-database.h"

#define TPBCLI_DB_DUMP_DEFAULT_COUNT 20

/*
 * Iterate .tpbe window indices: latest (-n, step=-1) starts at hi;
 * oldest (-N, step=1) starts at lo. Matches tpbcli-database-ls.c.
 */
#define TPBCLI_DUMP_TPBE_FOREACH(i, lo, hi, step, k)                          \
    for ((i) = ((step) < 0 ? (hi) : (lo)), (k) = 0;                          \
         (step) > 0 ? ((i) <= (hi)) : ((i) >= (lo));                          \
         (i) += (step), (k)++)

/* Local Function Prototypes */

static void compute_window(int total, int count, int from_oldest,
                           int *lo, int *hi, int *step, int *nshow);
static int parse_id_hex_arg(const char *in, char *out, size_t outsz,
                            size_t *out_len);
static int parse_rtenv_id_arg(const char *in, int32_t *id_out);
static int scan_tpbr_prefix_matches(const char *workspace,
                                    const char *prefix, size_t prefix_len,
                                    uint8_t domain_filter,
                                    tpb_raf_id_match_t **matches_out,
                                    int *nmatch_out);
static void print_ambiguous_id_table(const char *prefix,
                                     tpb_raf_id_match_t *matches, int nmatch,
                                     const char *workspace);
static int dump_tpbr_by_domain(const char *workspace, uint8_t domain,
                               const unsigned char id[20]);
static int resolve_hex_arg_and_dump(const char *workspace, uint8_t domain,
                                    const char *arg);
static void dump_print_kv_u64(const char *key, uint64_t v);
static void dump_print_kv_u32(const char *key, uint32_t v);
static void dump_print_kv_i32(const char *key, int32_t v);
static void dump_print_kv_str(const char *key, const char *val);
static void dump_print_kv_hex20(const char *key, const unsigned char id[20]);
static void dump_print_kv_build_datetime_utc(const char *key,
                                             tpb_dtbits_t utc_bits);
static void dump_headers(const tpb_meta_header_t *hdrs, uint32_t n);
static size_t dtype_elem_size_from_typebits(uint32_t type_bits);
static void dump_header_record_lines(const tpb_meta_header_t *h,
                                     const uint8_t *blob, uint32_t nd,
                                     uint64_t mult[TPBM_DATA_NDIM_MAX],
                                     uint64_t idx[TPBM_DATA_NDIM_MAX], int lev,
                                     size_t elem_size);
static void print_elem_csv(const uint8_t *p, uint32_t type_bits,
                           size_t elem_size);
static void dump_record_data(const tpb_meta_header_t *hdrs, uint32_t nheader,
                             const void *data, uint64_t datasize);
static int dump_tpbr_tbatch(const char *workspace,
                            const unsigned char id[20]);
static int dump_tpbr_kernel(const char *workspace,
                            const unsigned char id[20]);
static int dump_tpbr_task(const char *workspace,
                          const unsigned char id[20]);
static int dump_tpbr_rtenv(const char *workspace, int32_t id);
static int dump_tpbe_domain(const char *workspace, uint8_t domain,
                            int count, int from_oldest);

static void
compute_window(int total, int count, int from_oldest,
               int *lo, int *hi, int *step, int *nshow)
{
    int lim;

    if (total <= 0) {
        if (lo != NULL) {
            *lo = 0;
        }
        if (hi != NULL) {
            *hi = -1;
        }
        if (step != NULL) {
            *step = 1;
        }
        if (nshow != NULL) {
            *nshow = 0;
        }
        return;
    }

    if (count <= 0) {
        count = TPBCLI_DB_DUMP_DEFAULT_COUNT;
    }
    lim = (total < count) ? total : count;
    if (nshow != NULL) {
        *nshow = lim;
    }
    if (from_oldest) {
        if (lo != NULL) {
            *lo = 0;
        }
        if (hi != NULL) {
            *hi = lim - 1;
        }
        if (step != NULL) {
            *step = 1;
        }
    } else {
        if (lo != NULL) {
            *lo = total - lim;
        }
        if (hi != NULL) {
            *hi = total - 1;
        }
        if (step != NULL) {
            *step = -1;
        }
    }
}

static void
dump_print_kv_u64(const char *key, uint64_t v)
{
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "%s, %" PRIu64 "\n", key, v);
}

static void
dump_print_kv_u32(const char *key, uint32_t v)
{
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "%s, %" PRIu32 "\n", key, v);
}

static void
dump_print_kv_i32(const char *key, int32_t v)
{
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "%s, %" PRId32 "\n", key, v);
}

static void
dump_print_kv_str(const char *key, const char *val)
{
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "%s, %s\n", key, val ? val : "");
}

static void
dump_print_kv_hex20(const char *key, const unsigned char id[20])
{
    char hex[41];

    tpb_raf_id_to_hex(id, hex);
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "%s, %s\n", key, hex);
}

static void
dump_print_kv_build_datetime_utc(const char *key, tpb_dtbits_t utc_bits)
{
    tpb_datetime_str_t ts;

    if (utc_bits == 0) {
        dump_print_kv_str(key, "N/A");
        return;
    }
    if (tpb_ts_bits_to_isoutc(utc_bits, &ts) == 0) {
        dump_print_kv_str(key, ts.str);
    } else {
        dump_print_kv_u64(key, utc_bits);
    }
}

static int
parse_id_hex_arg(const char *in, char *out, size_t outsz, size_t *out_len)
{
    const char *a;
    const char *z;
    size_t n, i;

    if (!in || !out || !out_len || outsz < 42) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    a = in;
    while (*a != '\0' && isspace((unsigned char)*a)) {
        a++;
    }
    z = a + strlen(a);
    while (z > a && isspace((unsigned char)z[-1])) {
        z--;
    }
    n = (size_t)(z - a);
    if (n < 4 || n > 40) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    memcpy(out, a, n);
    out[n] = '\0';
    for (i = 0; i < n; i++) {
        if (!isxdigit((unsigned char)out[i])) {
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        out[i] = (char)tolower((unsigned char)out[i]);
    }
    *out_len = n;
    return TPBE_SUCCESS;
}

static int
parse_rtenv_id_arg(const char *in, int32_t *id_out)
{
    char *end;
    long v;

    if (in == NULL || id_out == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    while (*in != '\0' && isspace((unsigned char)*in)) {
        in++;
    }
    if (*in == '\0') {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    v = strtol(in, &end, 10);
    if (end == in || *end != '\0' || v < 0 || v > INT32_MAX) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "Invalid runtime_environment id (non-negative decimal required): %s\n",
                   in);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    *id_out = (int32_t)v;
    return TPBE_SUCCESS;
}

static int
scan_tpbr_prefix_matches(const char *workspace, const char *prefix,
                         size_t prefix_len, uint8_t domain_filter,
                         tpb_raf_id_match_t **matches_out, int *nmatch_out)
{
    return tpb_raf_scan_records_by_id_prefix(workspace, prefix, prefix_len,
                                             domain_filter,
                                             matches_out, nmatch_out);
}

static void
print_ambiguous_id_table(const char *prefix, tpb_raf_id_match_t *matches,
                         int nmatch, const char *workspace)
{
    int i;

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "The ID %s maps following records, use longer ID for "
               "precise searching:\n",
               prefix);
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "FullID, Domain, Start UTC, Duration\n");

    for (i = 0; i < nmatch; i++) {
        unsigned char id[20];
        tpb_datetime_str_t ts;
        const char *utc_col = "N/A";
        char tsbuf[sizeof(ts.str)];
        double dur_sec = 0.0;
        int have_dur = 0;

        if (tpb_raf_hex_to_id(matches[i].id_hex, id) != TPBE_SUCCESS) {
            continue;
        }

        tsbuf[0] = '\0';
        if (matches[i].domain == TPB_RAF_DOM_TBATCH) {
            tbatch_attr_t attr;
            memset(&attr, 0, sizeof(attr));
            if (tpb_raf_record_read_tbatch(workspace, id, &attr,
                                              NULL, NULL) == TPBE_SUCCESS) {
                if (tpb_ts_bits_to_isoutc(attr.utc_bits, &ts) == 0) {
                    snprintf(tsbuf, sizeof(tsbuf), "%s", ts.str);
                    utc_col = tsbuf;
                }
                dur_sec = (double)attr.duration / 1e9;
                have_dur = 1;
                tpb_raf_free_headers(attr.headers, attr.nheader);
            }
        } else if (matches[i].domain == TPB_RAF_DOM_TASK) {
            task_attr_t attr;
            memset(&attr, 0, sizeof(attr));
            if (tpb_raf_record_read_task(workspace, id, &attr,
                                           NULL, NULL) == TPBE_SUCCESS) {
                if (tpb_ts_bits_to_isoutc(attr.utc_bits, &ts) == 0) {
                    snprintf(tsbuf, sizeof(tsbuf), "%s", ts.str);
                    utc_col = tsbuf;
                }
                dur_sec = (double)attr.duration / 1e9;
                have_dur = 1;
                tpb_raf_free_headers(attr.headers, attr.nheader);
            }
        } else if (matches[i].domain == TPB_RAF_DOM_KERNEL) {
            kernel_attr_t attr;
            memset(&attr, 0, sizeof(attr));
            if (tpb_raf_record_read_kernel(workspace, id, &attr,
                                           NULL, NULL) == TPBE_SUCCESS) {
                if (attr.utc_bits != 0 &&
                    tpb_ts_bits_to_isoutc(attr.utc_bits, &ts) == 0) {
                    snprintf(tsbuf, sizeof(tsbuf), "%s", ts.str);
                    utc_col = tsbuf;
                }
                tpb_raf_free_headers(attr.headers, attr.nheader);
            }
        } else {
            utc_col = "N/A";
        }

        {
            const char *dname =
                (matches[i].domain == TPB_RAF_DOM_TBATCH) ? "task_batch" :
                (matches[i].domain == TPB_RAF_DOM_KERNEL) ? "kernel" : "task";

            if (have_dur) {
                tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                           TPBLOG_FLAG_DIRECT,
                           "%s, %s, %s, %.3f\n",
                           matches[i].id_hex, dname, utc_col, dur_sec);
            } else {
                tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                           TPBLOG_FLAG_DIRECT,
                           "%s, %s, %s, N/A\n",
                           matches[i].id_hex, dname, utc_col);
            }
        }
    }
}

static int
dump_tpbr_by_domain(const char *workspace, uint8_t domain,
                    const unsigned char id[20])
{
    if (domain == TPB_RAF_DOM_TBATCH) {
        return dump_tpbr_tbatch(workspace, id);
    }
    if (domain == TPB_RAF_DOM_KERNEL) {
        return dump_tpbr_kernel(workspace, id);
    }
    if (domain == TPB_RAF_DOM_TASK) {
        return dump_tpbr_task(workspace, id);
    }
    TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
}

static int
resolve_hex_arg_and_dump(const char *workspace, uint8_t domain,
                         const char *arg)
{
    char norm[48];
    size_t plen;
    unsigned char id[20];
    int err;

    if (domain != TPB_RAF_DOM_TBATCH && domain != TPB_RAF_DOM_KERNEL &&
        domain != TPB_RAF_DOM_TASK) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }

    if (parse_id_hex_arg(arg, norm, sizeof(norm), &plen) != TPBE_SUCCESS) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "Invalid id (need 4-40 hex digits, no 0x): %s\n", arg);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }

    if (plen == 40) {
        if (tpb_raf_hex_to_id(norm, id) != TPBE_SUCCESS) {
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        return dump_tpbr_by_domain(workspace, domain, id);
    }

    {
        tpb_raf_id_match_t *matches = NULL;
        int nmatch = 0;

        err = scan_tpbr_prefix_matches(workspace, norm, plen, domain,
                                       &matches, &nmatch);
        if (err != TPBE_SUCCESS) {
            tpb_raf_free_id_matches(matches);
            TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, "scan_tpbr_prefix_matches");
        }

        if (nmatch == 0) {
            tpb_raf_free_id_matches(matches);
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                       "No .tpbr matches id prefix %s in workspace.\n", norm);
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_FILE_IO_FAIL, NULL);
        }
        if (nmatch > 1) {
            print_ambiguous_id_table(norm, matches, nmatch, workspace);
            tpb_raf_free_id_matches(matches);
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }

        if (tpb_raf_hex_to_id(matches[0].id_hex, id) != TPBE_SUCCESS) {
            tpb_raf_free_id_matches(matches);
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        tpb_raf_free_id_matches(matches);
        return dump_tpbr_by_domain(workspace, domain, id);
    }
}

static void
dump_headers(const tpb_meta_header_t *hdrs, uint32_t n)
{
    uint32_t i, j;

    for (i = 0; i < n; i++) {
        const tpb_meta_header_t *h = &hdrs[i];
        char key[320];

        snprintf(key, sizeof(key), "header[%" PRIu32 "].block_size", i);
        dump_print_kv_u32(key, h->block_size);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].ndim", i);
        dump_print_kv_u32(key, h->ndim);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].data_size", i);
        dump_print_kv_u64(key, h->data_size);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].type_bits", i);
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "%s, 0x%08" PRIx32 "\n", key, h->type_bits);
        snprintf(key, sizeof(key), "header[%" PRIu32 "]._reserve", i);
        dump_print_kv_u32(key, h->_reserve);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].uattr_bits", i);
        dump_print_kv_u64(key, h->uattr_bits);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].name", i);
        dump_print_kv_str(key, h->name);
        snprintf(key, sizeof(key), "header[%" PRIu32 "].note", i);
        dump_print_kv_str(key, h->note);

        for (j = 0; j < (uint32_t)TPBM_DATA_NDIM_MAX; j++) {
            snprintf(key, sizeof(key), "header[%" PRIu32 "].dimsizes[%" PRIu32 "]",
                     i, j);
            dump_print_kv_u64(key, h->dimsizes[j]);
        }
        for (j = 0; j < (uint32_t)TPBM_DATA_NDIM_MAX; j++) {
            snprintf(key, sizeof(key), "header[%" PRIu32 "].dimnames[%" PRIu32 "]",
                     i, j);
            dump_print_kv_str(key, h->dimnames[j]);
        }
    }
}

static size_t
dtype_elem_size_from_typebits(uint32_t type_bits)
{
    TPB_DTYPE dt = (TPB_DTYPE)(type_bits & TPB_PARM_TYPE_MASK);
    uint32_t code = (uint32_t)(dt & 0xFFFFu);
    uint32_t nbytes = (code >> 8) & 0xFFu;

    if (nbytes == 0) {
        nbytes = 8;
    }
    return (size_t)nbytes;
}

static void
print_elem_csv(const uint8_t *p, uint32_t type_bits, size_t elem_size)
{
    TPB_DTYPE dt = (TPB_DTYPE)(type_bits & TPB_PARM_TYPE_MASK);

    if (elem_size == 20u) {
        char hx[41];
        tpb_raf_id_to_hex(p, hx);
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "%s", hx);
        return;
    }

    switch (dt) {
    case TPB_INT8_T:
    case TPB_UINT8_T:
    case TPB_CHAR_T:
    case TPB_UNSIGNED_CHAR_T:
    case TPB_SIGNED_CHAR_T:
    case TPB_BYTE_T:
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "%d", (int)*p);
        break;
    case TPB_INT16_T:
    case TPB_UINT16_T:
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "%d", (int)*(const int16_t *)(const void *)p);
        break;
    case TPB_INT32_T:
    case TPB_UINT32_T:
    case TPB_INT_T:
    case TPB_UNSIGNED_T:
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "%d", (int)*(const int32_t *)(const void *)p);
        break;
    case TPB_INT64_T:
    case TPB_UINT64_T:
    case TPB_LONG_T:
    case TPB_UNSIGNED_LONG_T:
    case TPB_LONG_LONG_T:
    case TPB_UNSIGNED_LONG_LONG_T:
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "%" PRId64, (int64_t)*(const int64_t *)(const void *)p);
        break;
    case TPB_FLOAT_T:
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "%.8g", (double)*(const float *)(const void *)p);
        break;
    case TPB_DOUBLE_T:
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "%.16g", *(const double *)(const void *)p);
        break;
    case TPB_LONG_DOUBLE_T:
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "%.16Lg", (long double)*(const long double *)(const void *)p);
        break;
    default:
        if (elem_size > 0) {
            size_t k;
            for (k = 0; k < elem_size; k++) {
                tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                           TPBLOG_FLAG_DIRECT, "%s%02x",
                           k ? "" : "0x", (unsigned)p[k]);
            }
        } else {
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                       "0");
        }
        break;
    }
}

static void
dump_header_record_lines(const tpb_meta_header_t *h, const uint8_t *blob,
                         uint32_t nd, uint64_t mult[TPBM_DATA_NDIM_MAX],
                         uint64_t idx[TPBM_DATA_NDIM_MAX], int lev,
                         size_t elem_size)
{
    if (lev == 0) {
        uint64_t i0;
        uint64_t fix = 0;
        uint32_t k;

        for (k = 1; k < nd; k++) {
            fix += idx[k] * mult[k];
        }
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "%s", h->name);
        for (k = nd; k-- > 1u; ) {
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                       "[%" PRIu64 "]", idx[k]);
        }
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "[], ");
        for (i0 = 0; i0 < h->dimsizes[0]; i0++) {
            uint64_t L = fix + i0;
            if (i0 > 0) {
                tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                           TPBLOG_FLAG_DIRECT, ", ");
            }
            print_elem_csv(blob + (size_t)(L * elem_size),
                           h->type_bits, elem_size);
        }
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "\n");
        return;
    }
    {
        uint64_t t;
        uint64_t kmax = h->dimsizes[lev];
        for (t = 0; t < kmax; t++) {
            idx[lev] = t;
            dump_header_record_lines(h, blob, nd, mult, idx,
                                     lev - 1, elem_size);
        }
    }
}

static void
dump_record_data(const tpb_meta_header_t *hdrs, uint32_t nheader,
                 const void *data, uint64_t datasize)
{
    const uint8_t *base = (const uint8_t *)data;
    uint64_t off = 0;
    uint32_t hi;

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "Record Data\n");

    for (hi = 0; hi < nheader; hi++) {
        const tpb_meta_header_t *h = &hdrs[hi];
        uint64_t dsz = h->data_size;
        uint32_t nd = h->ndim;
        uint64_t total_el = 1;
        uint32_t j;
        uint64_t mult[TPBM_DATA_NDIM_MAX];
        size_t elem_size;
        const uint8_t *blob;

        if (dsz == 0) {
            continue;
        }
        if (off + dsz > datasize) {
            break;
        }
        blob = base + off;
        off += dsz;

        if (nd > TPBM_DATA_NDIM_MAX) {
            nd = TPBM_DATA_NDIM_MAX;
        }

        for (j = 0; j < nd; j++) {
            uint64_t d = h->dimsizes[j];
            if (d == 0) {
                total_el = 0;
                break;
            }
            if (total_el > UINT64_MAX / d) {
                total_el = 0;
                break;
            }
            total_el *= d;
        }

        if (total_el == 0) {
            continue;
        }

        elem_size = (size_t)(dsz / total_el);
        if (elem_size == 0 || (uint64_t)elem_size * total_el != dsz) {
            elem_size = dtype_elem_size_from_typebits(h->type_bits);
            if (elem_size == 0 || (uint64_t)elem_size * total_el != dsz) {
                elem_size = 1;
            }
        }

        mult[0] = 1u;
        for (j = 1; j < nd; j++) {
            mult[j] = mult[j - 1] * h->dimsizes[j - 1];
        }

        if (nd <= 1) {
            uint64_t d0 = nd == 0 ? 1u : h->dimsizes[0];
            uint64_t i0;
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                       "%s[], ", h->name);
            for (i0 = 0; i0 < d0; i0++) {
                if (i0 > 0) {
                    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                               TPBLOG_FLAG_DIRECT, ", ");
                }
                print_elem_csv(blob + (size_t)(i0 * elem_size),
                               h->type_bits, elem_size);
            }
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                       "\n");
            continue;
        }

        {
            uint64_t idx[TPBM_DATA_NDIM_MAX];
            memset(idx, 0, sizeof(idx));
            dump_header_record_lines(h, blob, nd, mult, idx,
                                     (int)nd - 1, elem_size);
        }
    }
}

static int
dump_tpbr_tbatch(const char *workspace, const unsigned char id[20])
{
    tbatch_attr_t attr;
    void *data = NULL;
    uint64_t datasize = 0;
    tpb_datetime_str_t ts;
    int err;

    memset(&attr, 0, sizeof(attr));
    err = tpb_raf_record_read_tbatch(workspace, id, &attr,
                                        &data, &datasize);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "Metadata\n");
    dump_print_kv_hex20("tbatch_id", attr.tbatch_id);
    dump_print_kv_hex20("derive_to", attr.derive_to);
    dump_print_kv_hex20("inherit_from", attr.inherit_from);
    if (tpb_ts_bits_to_isoutc(attr.utc_bits, &ts) == 0) {
        dump_print_kv_str("start_utc", ts.str);
    } else {
        dump_print_kv_u64("utc_bits", attr.utc_bits);
    }
    dump_print_kv_u64("btime", attr.btime);
    dump_print_kv_u64("duration", attr.duration);
    dump_print_kv_str("hostname", attr.hostname);
    dump_print_kv_str("username", attr.username);
    dump_print_kv_u32("front_pid", attr.front_pid);
    dump_print_kv_u32("nkernel", attr.nkernel);
    dump_print_kv_u32("ntask", attr.ntask);
    dump_print_kv_u32("nscore", attr.nscore);
    dump_print_kv_u32("batch_type", attr.batch_type);
    dump_print_kv_u32("nheader", attr.nheader);

    dump_headers(attr.headers, attr.nheader);
    dump_record_data(attr.headers, attr.nheader, data, datasize);

    free(data);
    tpb_raf_free_headers(attr.headers, attr.nheader);
    return TPBE_SUCCESS;
}

static int
dump_tpbr_kernel(const char *workspace, const unsigned char id[20])
{
    kernel_attr_t attr;
    void *data = NULL;
    uint64_t datasize = 0;
    int err;

    memset(&attr, 0, sizeof(attr));
    err = tpb_raf_record_read_kernel(workspace, id, &attr,
                                       &data, &datasize);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "Metadata\n");
    dump_print_kv_hex20("kernel_id", attr.kernel_id);
    dump_print_kv_hex20("derive_to", attr.derive_to);
    dump_print_kv_hex20("inherit_from", attr.inherit_from);
    dump_print_kv_str("kernel_name", attr.kernel_name);
    dump_print_kv_str("version", attr.version);
    dump_print_kv_str("description", attr.description);
    dump_print_kv_u32("nparm", attr.nparm);
    dump_print_kv_u32("nmetric", attr.nmetric);
    dump_print_kv_u32("kctrl", attr.kctrl);
    dump_print_kv_u32("nheader", attr.nheader);
    dump_print_kv_u32("active", attr.active);
    dump_print_kv_build_datetime_utc("Build datetime (UTC)", attr.utc_bits);

    dump_headers(attr.headers, attr.nheader);
    dump_record_data(attr.headers, attr.nheader, data, datasize);

    free(data);
    tpb_raf_free_headers(attr.headers, attr.nheader);
    return TPBE_SUCCESS;
}

static int
dump_tpbr_task(const char *workspace, const unsigned char id[20])
{
    task_attr_t attr;
    void *data = NULL;
    uint64_t datasize = 0;
    tpb_datetime_str_t ts;
    int err;

    memset(&attr, 0, sizeof(attr));
    err = tpb_raf_record_read_task(workspace, id, &attr,
                                     &data, &datasize);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "Metadata\n");
    dump_print_kv_hex20("task_record_id", attr.task_record_id);
    dump_print_kv_hex20("derive_to", attr.derive_to);
    dump_print_kv_hex20("inherit_from", attr.inherit_from);
    dump_print_kv_hex20("tbatch_id", attr.tbatch_id);
    dump_print_kv_hex20("kernel_id", attr.kernel_id);
    if (tpb_ts_bits_to_isoutc(attr.utc_bits, &ts) == 0) {
        dump_print_kv_str("start_utc", ts.str);
    } else {
        dump_print_kv_u64("utc_bits", attr.utc_bits);
    }
    dump_print_kv_u64("btime", attr.btime);
    dump_print_kv_u64("duration", attr.duration);
    dump_print_kv_u32("exit_code", attr.exit_code);
    dump_print_kv_u32("handle_index", attr.handle_index);
    dump_print_kv_u32("pid", attr.pid);
    dump_print_kv_u32("tid", attr.tid);
    dump_print_kv_u32("ninput", attr.ninput);
    dump_print_kv_u32("noutput", attr.noutput);
    dump_print_kv_u32("nheader", attr.nheader);
    dump_print_kv_u32("reserve", attr.reserve);

    dump_headers(attr.headers, attr.nheader);
    dump_record_data(attr.headers, attr.nheader, data, datasize);

    free(data);
    tpb_raf_free_headers(attr.headers, attr.nheader);
    return TPBE_SUCCESS;
}

static int
dump_tpbr_rtenv(const char *workspace, int32_t id)
{
    tpb_raf_rtenv_attr_t attr;
    tpb_meta_header_t *hdrs = NULL;
    void *data = NULL;
    uint64_t datasize = 0;
    tpb_datetime_str_t ts;
    int err;

    memset(&attr, 0, sizeof(attr));
    err = tpb_raf_record_read_rtenv(workspace, id, &attr, &hdrs, &data, &datasize);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "Metadata\n");
    dump_print_kv_i32("id", attr.id);
    dump_print_kv_str("name", attr.name);
    dump_print_kv_str("hostname", attr.hostname);
    if (tpb_ts_bits_to_isoutc(attr.utc_bits, &ts) == 0) {
        dump_print_kv_str("start_utc", ts.str);
    } else {
        dump_print_kv_u64("utc_bits", attr.utc_bits);
    }
    dump_print_kv_i32("inherit_from", attr.inherit_from);
    dump_print_kv_i32("derive_to", attr.derive_to);
    dump_print_kv_u32("ntask", attr.ntask);
    dump_print_kv_u32("ntbatch", attr.ntbatch);
    dump_print_kv_str("note", attr.note);
    dump_print_kv_u32("napp", attr.napp);
    dump_print_kv_u32("nenv", attr.nenv);
    dump_print_kv_u32("nheader", attr.nheader);

    dump_headers(hdrs, attr.nheader);
    dump_record_data(hdrs, attr.nheader, data, datasize);

    free(data);
    tpb_raf_free_headers(hdrs, attr.nheader);
    return TPBE_SUCCESS;
}

static int
dump_tpbe_domain(const char *workspace, uint8_t domain,
                 int count, int from_oldest)
{
    int err;
    int n;
    int lo, hi, step, nshow;
    int i, k;
    const char *fname = NULL;

    if (domain == TPB_RAF_DOM_KERNEL) {
        fname = "kernel.tpbe";
    } else if (domain == TPB_RAF_DOM_TASK) {
        fname = "task.tpbe";
    } else if (domain == TPB_RAF_DOM_RTENV) {
        fname = "runtime_environment.tpbe";
    } else {
        fname = "task_batch.tpbe";
    }

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "Entry File: %s\n", fname);

    if (domain == TPB_RAF_DOM_TBATCH) {
        tbatch_entry_t *e = NULL;

        err = tpb_raf_entry_list_tbatch(workspace, &e, &n);
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
        compute_window(n, count, from_oldest, &lo, &hi, &step, &nshow);
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "(%d of %d entries)\n", nshow, n);
        TPBCLI_DUMP_TPBE_FOREACH(i, lo, hi, step, k) {
            char p[80];
            tpb_datetime_str_t ts;

            snprintf(p, sizeof(p), "entry[%d].tbatch_id", k);
            dump_print_kv_hex20(p, e[i].tbatch_id);
            snprintf(p, sizeof(p), "entry[%d].inherit_from", k);
            dump_print_kv_hex20(p, e[i].inherit_from);
            snprintf(p, sizeof(p), "entry[%d].start_utc", k);
            if (tpb_ts_bits_to_isoutc(e[i].start_utc_bits, &ts) == 0) {
                dump_print_kv_str(p, ts.str);
            } else {
                snprintf(p, sizeof(p), "entry[%d].start_utc_bits", k);
                dump_print_kv_u64(p, e[i].start_utc_bits);
            }
            snprintf(p, sizeof(p), "entry[%d].duration", k);
            dump_print_kv_u64(p, e[i].duration);
            snprintf(p, sizeof(p), "entry[%d].hostname", k);
            dump_print_kv_str(p, e[i].hostname);
            snprintf(p, sizeof(p), "entry[%d].nkernel", k);
            dump_print_kv_u32(p, e[i].nkernel);
            snprintf(p, sizeof(p), "entry[%d].ntask", k);
            dump_print_kv_u32(p, e[i].ntask);
            snprintf(p, sizeof(p), "entry[%d].nscore", k);
            dump_print_kv_u32(p, e[i].nscore);
            snprintf(p, sizeof(p), "entry[%d].batch_type", k);
            dump_print_kv_u32(p, e[i].batch_type);
        }
        free(e);
        return TPBE_SUCCESS;
    }

    if (domain == TPB_RAF_DOM_KERNEL) {
        kernel_entry_t *e = NULL;

        err = tpb_raf_entry_list_kernel(workspace, &e, &n);
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
        compute_window(n, count, from_oldest, &lo, &hi, &step, &nshow);
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "(%d of %d entries)\n", nshow, n);
        TPBCLI_DUMP_TPBE_FOREACH(i, lo, hi, step, k) {
            char p[80];

            snprintf(p, sizeof(p), "entry[%d].kernel_id", k);
            dump_print_kv_hex20(p, e[i].kernel_id);
            snprintf(p, sizeof(p), "entry[%d].inherit_from", k);
            dump_print_kv_hex20(p, e[i].inherit_from);
            snprintf(p, sizeof(p), "entry[%d].kernel_name", k);
            dump_print_kv_str(p, e[i].kernel_name);
            snprintf(p, sizeof(p), "entry[%d].kctrl", k);
            dump_print_kv_u32(p, e[i].kctrl);
            snprintf(p, sizeof(p), "entry[%d].nparm", k);
            dump_print_kv_u32(p, e[i].nparm);
            snprintf(p, sizeof(p), "entry[%d].nmetric", k);
            dump_print_kv_u32(p, e[i].nmetric);
            snprintf(p, sizeof(p), "entry[%d].active", k);
            dump_print_kv_u32(p, e[i].active);
            snprintf(p, sizeof(p), "entry[%d].Build datetime (UTC)", k);
            dump_print_kv_build_datetime_utc(p, e[i].utc_bits);
        }
        free(e);
        return TPBE_SUCCESS;
    }

    if (domain == TPB_RAF_DOM_RTENV) {
        tpb_raf_rtenv_entry_t *e = NULL;

        err = tpb_raf_entry_list_rtenv(workspace, &e, &n);
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
        compute_window(n, count, from_oldest, &lo, &hi, &step, &nshow);
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "(%d of %d entries)\n", nshow, n);
        TPBCLI_DUMP_TPBE_FOREACH(i, lo, hi, step, k) {
            char p[96];
            tpb_datetime_str_t ts;

            snprintf(p, sizeof(p), "entry[%d].id", k);
            dump_print_kv_i32(p, e[i].id);
            snprintf(p, sizeof(p), "entry[%d].name", k);
            dump_print_kv_str(p, e[i].name);
            snprintf(p, sizeof(p), "entry[%d].hostname", k);
            dump_print_kv_str(p, e[i].hostname);
            snprintf(p, sizeof(p), "entry[%d].start_utc", k);
            if (tpb_ts_bits_to_isoutc(e[i].utc_bits, &ts) == 0) {
                dump_print_kv_str(p, ts.str);
            } else {
                snprintf(p, sizeof(p), "entry[%d].utc_bits", k);
                dump_print_kv_u64(p, e[i].utc_bits);
            }
            snprintf(p, sizeof(p), "entry[%d].inherit_from", k);
            dump_print_kv_i32(p, e[i].inherit_from);
            snprintf(p, sizeof(p), "entry[%d].derive_to", k);
            dump_print_kv_i32(p, e[i].derive_to);
            snprintf(p, sizeof(p), "entry[%d].ntask", k);
            dump_print_kv_u32(p, e[i].ntask);
            snprintf(p, sizeof(p), "entry[%d].ntbatch", k);
            dump_print_kv_u32(p, e[i].ntbatch);
            snprintf(p, sizeof(p), "entry[%d].note", k);
            dump_print_kv_str(p, e[i].note);
            snprintf(p, sizeof(p), "entry[%d].napp", k);
            dump_print_kv_u32(p, e[i].napp);
            snprintf(p, sizeof(p), "entry[%d].nenv", k);
            dump_print_kv_u32(p, e[i].nenv);
        }
        free(e);
        return TPBE_SUCCESS;
    }

    {
        task_entry_t *e = NULL;
        int *vis_idx = NULL;
        unsigned char z20[20] = {0};
        int nvis;
        int vi;

        err = tpb_raf_entry_list_task(workspace, &e, &n);
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
        nvis = 0;
        for (i = 0; i < n; i++) {
            if (memcmp(e[i].derive_to, z20, 20) == 0) {
                nvis++;
            }
        }
        if (nvis > 0) {
            vis_idx = (int *)malloc((size_t)nvis * sizeof(int));
            if (vis_idx == NULL) {
                free(e);
                TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
            }
            vi = 0;
            for (i = 0; i < n; i++) {
                if (memcmp(e[i].derive_to, z20, 20) == 0) {
                    vis_idx[vi++] = i;
                }
            }
        }
        compute_window(nvis, count, from_oldest, &lo, &hi, &step, &nshow);
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "(%d task entry points shown / %d total / %d rows)\n",
                   nshow, nvis, n);
        TPBCLI_DUMP_TPBE_FOREACH(i, lo, hi, step, k) {
            char p[96];
            tpb_datetime_str_t ts;
            int src;

            if (vis_idx == NULL) {
                break;
            }
            src = vis_idx[i];

            snprintf(p, sizeof(p), "entry[%d].task_record_id", k);
            dump_print_kv_hex20(p, e[src].task_record_id);
            snprintf(p, sizeof(p), "entry[%d].inherit_from", k);
            dump_print_kv_hex20(p, e[src].inherit_from);
            snprintf(p, sizeof(p), "entry[%d].derive_to", k);
            dump_print_kv_hex20(p, e[src].derive_to);
            snprintf(p, sizeof(p), "entry[%d].tbatch_id", k);
            dump_print_kv_hex20(p, e[src].tbatch_id);
            snprintf(p, sizeof(p), "entry[%d].kernel_id", k);
            dump_print_kv_hex20(p, e[src].kernel_id);
            snprintf(p, sizeof(p), "entry[%d].start_utc", k);
            if (tpb_ts_bits_to_isoutc(e[src].utc_bits, &ts) == 0) {
                dump_print_kv_str(p, ts.str);
            } else {
                snprintf(p, sizeof(p), "entry[%d].utc_bits", k);
                dump_print_kv_u64(p, e[src].utc_bits);
            }
            snprintf(p, sizeof(p), "entry[%d].duration", k);
            dump_print_kv_u64(p, e[src].duration);
            snprintf(p, sizeof(p), "entry[%d].exit_code", k);
            dump_print_kv_u32(p, e[src].exit_code);
            snprintf(p, sizeof(p), "entry[%d].handle_index", k);
            dump_print_kv_u32(p, e[src].handle_index);
        }
        free(vis_idx);
        free(e);
    }
    return TPBE_SUCCESS;
}

/**
 * @brief Run database dump from resolved domain and mode flags.
 */
int
tpbcli_database_dump_resolved(const char *workspace,
                              uint8_t domain,
                              int entry_mode,
                              const char *id_value,
                              int count,
                              int from_oldest)
{
    int32_t rtenv_id;
    int err;

    if (workspace == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }

    if (entry_mode) {
        return dump_tpbe_domain(workspace, domain, count, from_oldest);
    }

    if (id_value == NULL || id_value[0] == '\0') {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "Usage: tpbcli database dump "
                   "[-dT|-dt|-dk|-dr | --domain <name>] "
                   "(-i|--id <id> | -e) [-n <N> | -N <N>]\n");
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }

    if (domain == TPB_RAF_DOM_RTENV) {
        err = parse_rtenv_id_arg(id_value, &rtenv_id);
        if (err != TPBE_SUCCESS) {
            return err;
        }
        return dump_tpbr_rtenv(workspace, rtenv_id);
    }

    return resolve_hex_arg_and_dump(workspace, domain, id_value);
}
