/*
 * tpbcli-database-dump.c
 * `tpbcli database dump` — human-readable .tpbr and CSV-style .tpbe output.
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
#include "tpbcli-database-dump-fmt.h"

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

static int
dump_tpbr_tbatch(const char *workspace, const unsigned char id[20])
{
    unsigned char start_magic[TPB_RAF_MAGIC_LEN];
    unsigned char split_magic[TPB_RAF_MAGIC_LEN];
    unsigned char end_magic[TPB_RAF_MAGIC_LEN];
    tbatch_attr_t attr;
    tpbcli_dump_kv_row_t rows[16];
    void *data = NULL;
    uint64_t datasize = 0;
    tpb_datetime_str_t ts;
    char id_hex[41];
    int nr = 0;
    int err;

    err = tpb_raf_record_peek_magics(workspace, TPB_RAF_DOM_TBATCH, id, 0,
                                     start_magic, split_magic, end_magic);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    memset(&attr, 0, sizeof(attr));
    err = tpb_raf_record_read_tbatch(workspace, id, &attr, &data, &datasize);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    tpb_raf_id_to_hex(id, id_hex);
    tpbcli_dump_fmt_print_domain_banner(TPB_RAF_DOM_TBATCH, id_hex);
    tpbcli_dump_fmt_print_magic_line(start_magic);
    tpbcli_dump_fmt_print_section_banner("Metadata");

    tpbcli_dump_fmt_print_subsection("Attributes");
    tpbcli_dump_fmt_kv_set_hex20(&rows[nr++], "tbatch_id", attr.tbatch_id);
    tpbcli_dump_fmt_kv_set_hex20(&rows[nr++], "derive_to", attr.derive_to);
    tpbcli_dump_fmt_kv_set_hex20(&rows[nr++], "inherit_from", attr.inherit_from);
    if (tpb_ts_bits_to_isoutc(attr.utc_bits, &ts) == 0) {
        tpbcli_dump_fmt_kv_set_str(&rows[nr++], "start_utc", ts.str);
    } else {
        tpbcli_dump_fmt_kv_set_u64(&rows[nr++], "utc_bits", attr.utc_bits);
    }
    tpbcli_dump_fmt_kv_set_u64(&rows[nr++], "btime", attr.btime);
    tpbcli_dump_fmt_kv_set_u64(&rows[nr++], "duration", attr.duration);
    tpbcli_dump_fmt_kv_set_str(&rows[nr++], "hostname", attr.hostname);
    tpbcli_dump_fmt_kv_set_str(&rows[nr++], "username", attr.username);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "front_pid", attr.front_pid);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "nkernel", attr.nkernel);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "ntask", attr.ntask);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "nscore", attr.nscore);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "batch_type", attr.batch_type);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "nheader", attr.nheader);
    tpbcli_dump_fmt_print_kv_block(rows, nr);

    tpbcli_dump_fmt_print_headers(attr.headers, attr.nheader);

    tpbcli_dump_fmt_print_magic_line(split_magic);
    tpbcli_dump_fmt_print_section_banner("Record Data");
    tpbcli_dump_fmt_print_record_data(attr.headers, attr.nheader, data, datasize);
    tpbcli_dump_fmt_print_file_end(end_magic);

    free(data);
    tpb_raf_free_headers(attr.headers, attr.nheader);
    return TPBE_SUCCESS;
}

static int
dump_tpbr_kernel(const char *workspace, const unsigned char id[20])
{
    unsigned char start_magic[TPB_RAF_MAGIC_LEN];
    unsigned char split_magic[TPB_RAF_MAGIC_LEN];
    unsigned char end_magic[TPB_RAF_MAGIC_LEN];
    kernel_attr_t attr;
    tpbcli_dump_kv_row_t rows[16];
    void *data = NULL;
    uint64_t datasize = 0;
    tpb_datetime_str_t ts;
    char id_hex[41];
    int nr = 0;
    int err;

    err = tpb_raf_record_peek_magics(workspace, TPB_RAF_DOM_KERNEL, id, 0,
                                     start_magic, split_magic, end_magic);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    memset(&attr, 0, sizeof(attr));
    err = tpb_raf_record_read_kernel(workspace, id, &attr, &data, &datasize);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    tpb_raf_id_to_hex(id, id_hex);
    tpbcli_dump_fmt_print_domain_banner(TPB_RAF_DOM_KERNEL, id_hex);
    tpbcli_dump_fmt_print_magic_line(start_magic);
    tpbcli_dump_fmt_print_section_banner("Metadata");

    tpbcli_dump_fmt_print_subsection("Attributes");
    tpbcli_dump_fmt_kv_set_hex20(&rows[nr++], "kernel_id", attr.kernel_id);
    tpbcli_dump_fmt_kv_set_hex20(&rows[nr++], "derive_to", attr.derive_to);
    tpbcli_dump_fmt_kv_set_hex20(&rows[nr++], "inherit_from", attr.inherit_from);
    tpbcli_dump_fmt_kv_set_str(&rows[nr++], "kernel_name", attr.kernel_name);
    tpbcli_dump_fmt_kv_set_str(&rows[nr++], "version", attr.version);
    tpbcli_dump_fmt_kv_set_str(&rows[nr++], "description", attr.description);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "narg", attr.narg);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "nmetric", attr.nmetric);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "kctrl", attr.kctrl);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "nheader", attr.nheader);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "active", attr.active);
    tpbcli_dump_fmt_kv_set_build_datetime(&rows[nr++], "Build datetime (UTC)",
                                          attr.utc_bits);
    tpbcli_dump_fmt_print_kv_block(rows, nr);

    tpbcli_dump_fmt_print_headers(attr.headers, attr.nheader);

    tpbcli_dump_fmt_print_magic_line(split_magic);
    tpbcli_dump_fmt_print_section_banner("Record Data");
    tpbcli_dump_fmt_print_record_data(attr.headers, attr.nheader, data, datasize);
    tpbcli_dump_fmt_print_file_end(end_magic);

    free(data);
    tpb_raf_free_headers(attr.headers, attr.nheader);
    return TPBE_SUCCESS;
}

static int
dump_tpbr_task(const char *workspace, const unsigned char id[20])
{
    unsigned char start_magic[TPB_RAF_MAGIC_LEN];
    unsigned char split_magic[TPB_RAF_MAGIC_LEN];
    unsigned char end_magic[TPB_RAF_MAGIC_LEN];
    task_attr_t attr;
    tpbcli_dump_kv_row_t rows[20];
    void *data = NULL;
    uint64_t datasize = 0;
    tpb_datetime_str_t ts;
    char id_hex[41];
    int nr = 0;
    int err;

    err = tpb_raf_record_peek_magics(workspace, TPB_RAF_DOM_TASK, id, 0,
                                     start_magic, split_magic, end_magic);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    memset(&attr, 0, sizeof(attr));
    err = tpb_raf_record_read_task(workspace, id, &attr, &data, &datasize);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    tpb_raf_id_to_hex(id, id_hex);
    tpbcli_dump_fmt_print_domain_banner(TPB_RAF_DOM_TASK, id_hex);
    tpbcli_dump_fmt_print_magic_line(start_magic);
    tpbcli_dump_fmt_print_section_banner("Metadata");

    tpbcli_dump_fmt_print_subsection("Attributes");
    tpbcli_dump_fmt_kv_set_hex20(&rows[nr++], "task_record_id",
                                 attr.task_record_id);
    tpbcli_dump_fmt_kv_set_hex20(&rows[nr++], "derive_to", attr.derive_to);
    tpbcli_dump_fmt_kv_set_hex20(&rows[nr++], "inherit_from", attr.inherit_from);
    tpbcli_dump_fmt_kv_set_hex20(&rows[nr++], "tbatch_id", attr.tbatch_id);
    tpbcli_dump_fmt_kv_set_hex20(&rows[nr++], "kernel_id", attr.kernel_id);
    if (tpb_ts_bits_to_isoutc(attr.utc_bits, &ts) == 0) {
        tpbcli_dump_fmt_kv_set_str(&rows[nr++], "start_utc", ts.str);
    } else {
        tpbcli_dump_fmt_kv_set_u64(&rows[nr++], "utc_bits", attr.utc_bits);
    }
    tpbcli_dump_fmt_kv_set_u64(&rows[nr++], "btime", attr.btime);
    tpbcli_dump_fmt_kv_set_u64(&rows[nr++], "duration", attr.duration);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "exit_code", attr.exit_code);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "handle_index", attr.handle_index);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "pid", attr.pid);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "tid", attr.tid);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "ninput", attr.ninput);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "noutput", attr.noutput);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "nheader", attr.nheader);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "reserve", attr.reserve);
    tpbcli_dump_fmt_print_kv_block(rows, nr);

    tpbcli_dump_fmt_print_headers(attr.headers, attr.nheader);

    tpbcli_dump_fmt_print_magic_line(split_magic);
    tpbcli_dump_fmt_print_section_banner("Record Data");
    tpbcli_dump_fmt_print_record_data(attr.headers, attr.nheader, data, datasize);
    tpbcli_dump_fmt_print_file_end(end_magic);

    free(data);
    tpb_raf_free_headers(attr.headers, attr.nheader);
    return TPBE_SUCCESS;
}

static int
dump_tpbr_rtenv(const char *workspace, int32_t id)
{
    unsigned char start_magic[TPB_RAF_MAGIC_LEN];
    unsigned char split_magic[TPB_RAF_MAGIC_LEN];
    unsigned char end_magic[TPB_RAF_MAGIC_LEN];
    tpb_raf_rtenv_attr_t attr;
    tpb_meta_header_t *hdrs = NULL;
    tpbcli_dump_kv_row_t rows[16];
    void *data = NULL;
    uint64_t datasize = 0;
    tpb_datetime_str_t ts;
    char id_buf[32];
    int nr = 0;
    int err;

    err = tpb_raf_record_peek_magics(workspace, TPB_RAF_DOM_RTENV, NULL, id,
                                     start_magic, split_magic, end_magic);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    memset(&attr, 0, sizeof(attr));
    err = tpb_raf_record_read_rtenv(workspace, id, &attr, &hdrs, &data,
                                    &datasize);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    snprintf(id_buf, sizeof(id_buf), "%" PRId32, id);
    tpbcli_dump_fmt_print_domain_banner(TPB_RAF_DOM_RTENV, id_buf);
    tpbcli_dump_fmt_print_magic_line(start_magic);
    tpbcli_dump_fmt_print_section_banner("Metadata");

    tpbcli_dump_fmt_print_subsection("Attributes");
    tpbcli_dump_fmt_kv_set_i32(&rows[nr++], "id", attr.id);
    tpbcli_dump_fmt_kv_set_str(&rows[nr++], "name", attr.name);
    tpbcli_dump_fmt_kv_set_str(&rows[nr++], "hostname", attr.hostname);
    if (tpb_ts_bits_to_isoutc(attr.utc_bits, &ts) == 0) {
        tpbcli_dump_fmt_kv_set_str(&rows[nr++], "start_utc", ts.str);
    } else {
        tpbcli_dump_fmt_kv_set_u64(&rows[nr++], "utc_bits", attr.utc_bits);
    }
    tpbcli_dump_fmt_kv_set_i32(&rows[nr++], "inherit_from", attr.inherit_from);
    tpbcli_dump_fmt_kv_set_i32(&rows[nr++], "derive_to", attr.derive_to);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "ntask", attr.ntask);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "ntbatch", attr.ntbatch);
    tpbcli_dump_fmt_kv_set_str(&rows[nr++], "note", attr.note);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "napp", attr.napp);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "nenv", attr.nenv);
    tpbcli_dump_fmt_kv_set_u32(&rows[nr++], "nheader", attr.nheader);
    tpbcli_dump_fmt_print_kv_block(rows, nr);

    tpbcli_dump_fmt_print_headers(hdrs, attr.nheader);

    tpbcli_dump_fmt_print_magic_line(split_magic);
    tpbcli_dump_fmt_print_section_banner("Record Data");
    tpbcli_dump_fmt_print_record_data(hdrs, attr.nheader, data, datasize);
    tpbcli_dump_fmt_print_file_end(end_magic);

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
            snprintf(p, sizeof(p), "entry[%d].narg", k);
            dump_print_kv_u32(p, e[i].narg);
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
