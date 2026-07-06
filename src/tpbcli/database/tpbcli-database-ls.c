/*
 * tpbcli-database-ls.c
 * `tpbcli database list` / `ls` — rafdb index listing by domain.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tpb-public.h"
#include "tpbcli-database.h"

#define TPBCLI_DB_LIST_DEFAULT_COUNT 20
#define TPBCLI_DB_LIST_COL_GAP       1

/* Local Function Prototypes */

static void format_id_prefix6(const unsigned char id[20], char *out,
                              size_t outlen);
static void compute_window(int total, int count, int from_oldest,
                           int *lo, int *hi, int *step, int *nshow);
static const char *lookup_kernel_name(const kernel_entry_t *kentries, int nk,
                                      const unsigned char kernel_id[20],
                                      char *fallback, size_t fallback_len);
static int filter_task_entrypoints(const task_entry_t *src, int nsrc,
                                   task_entry_t **filtered, int *nfiltered);
static int list_tbatch(const char *workspace, int count, int from_oldest);
static int list_task(const char *workspace, int count, int from_oldest);
static int list_kernel(const char *workspace, int count, int from_oldest);

static void
format_id_prefix6(const unsigned char id[20], char *out, size_t outlen)
{
    char hex[41];

    if (out == NULL || outlen < 8) {
        return;
    }
    tpb_raf_id_to_hex(id, hex);
    snprintf(out, outlen, "%.6s*", hex);
}

static void
compute_window(int total, int count, int from_oldest,
               int *lo, int *hi, int *step, int *nshow)
{
    int lim;

    if (count <= 0) {
        count = TPBCLI_DB_LIST_DEFAULT_COUNT;
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

static const char *
lookup_kernel_name(const kernel_entry_t *kentries, int nk,
                   const unsigned char kernel_id[20],
                   char *fallback, size_t fallback_len)
{
    int i;

    if (kentries == NULL || nk <= 0) {
        format_id_prefix6(kernel_id, fallback, fallback_len);
        return fallback;
    }
    for (i = 0; i < nk; i++) {
        if (memcmp(kentries[i].kernel_id, kernel_id, 20) == 0) {
            if (kentries[i].kernel_name[0] != '\0') {
                return kentries[i].kernel_name;
            }
            break;
        }
    }
    format_id_prefix6(kernel_id, fallback, fallback_len);
    return fallback;
}

static int
filter_task_entrypoints(const task_entry_t *src, int nsrc,
                        task_entry_t **filtered, int *nfiltered)
{
    static const unsigned char z20[20] = {0};
    task_entry_t *out;
    int i;
    int n = 0;

    if (filtered == NULL || nfiltered == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    *filtered = NULL;
    *nfiltered = 0;
    if (src == NULL || nsrc <= 0) {
        return TPBE_SUCCESS;
    }
    for (i = 0; i < nsrc; i++) {
        if (memcmp(src[i].derive_to, z20, 20) == 0) {
            n++;
        }
    }
    if (n == 0) {
        return TPBE_SUCCESS;
    }
    out = (task_entry_t *)calloc((size_t)n, sizeof(*out));
    if (out == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
    }
    n = 0;
    for (i = 0; i < nsrc; i++) {
        if (memcmp(src[i].derive_to, z20, 20) == 0) {
            out[n++] = src[i];
        }
    }
    *filtered = out;
    *nfiltered = n;
    return TPBE_SUCCESS;
}

static int
list_tbatch(const char *workspace, int count, int from_oldest)
{
    tbatch_entry_t *entries = NULL;
    int total = 0;
    int err;
    int lo;
    int hi;
    int step;
    int nshow;
    int i;
    const char *headers[7];
    const char *cells[7];
    float ratios[7] = {20.0f, 10.0f, 12.0f, 7.0f, 8.0f, 7.0f, 9.0f};
    char c0[32];
    char c1[16];
    char c2[32];
    char c3[16];
    char c4[16];
    char c5[16];
    char c6[8];
    tpb_datetime_str_t ts;

    err = tpb_raf_entry_list_tbatch(workspace, &entries, &total);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, "tpb_raf_entry_list_tbatch");
    }

    compute_window(total, count, from_oldest, &lo, &hi, &step, &nshow);
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG,
               "List of %s %d tbatch record%s\n",
               from_oldest ? "oldest" : "latest",
               nshow, nshow == 1 ? "" : "s");

    headers[0] = "Start Time (UTC)";
    headers[1] = "Type";
    headers[2] = "Duration(s)";
    headers[3] = "NTask";
    headers[4] = "NKernel";
    headers[5] = "NScore";
    headers[6] = "TBatch ID";
    tpblog_printf_c(ratios, 7, TPBCLI_DB_LIST_COL_GAP, headers);

    if (total == 0) {
        free(entries);
        return TPBE_SUCCESS;
    }

    if (step < 0) {
        for (i = hi; i >= lo; i--) {
            const tbatch_entry_t *e = &entries[i];
            double dur_sec = (double)e->duration / 1e9;

            if (tpb_ts_bits_to_isoutc(e->start_utc_bits, &ts) != 0) {
                snprintf(c0, sizeof(c0), "-");
            } else {
                snprintf(c0, sizeof(c0), "%s", ts.str);
            }
            snprintf(c1, sizeof(c1), "%s",
                     (e->batch_type == TPB_BATCH_TYPE_BENCHMARK)
                         ? "benchmark" : "run");
            snprintf(c2, sizeof(c2), "%.3f", dur_sec);
            snprintf(c3, sizeof(c3), "%u", e->ntask);
            snprintf(c4, sizeof(c4), "%u", e->nkernel);
            snprintf(c5, sizeof(c5), "%u", e->nscore);
            format_id_prefix6(e->tbatch_id, c6, sizeof(c6));

            cells[0] = c0;
            cells[1] = c1;
            cells[2] = c2;
            cells[3] = c3;
            cells[4] = c4;
            cells[5] = c5;
            cells[6] = c6;
            tpblog_printf_c(ratios, 7, TPBCLI_DB_LIST_COL_GAP, cells);
        }
    } else {
        for (i = lo; i <= hi; i++) {
            const tbatch_entry_t *e = &entries[i];
            double dur_sec = (double)e->duration / 1e9;

            if (tpb_ts_bits_to_isoutc(e->start_utc_bits, &ts) != 0) {
                snprintf(c0, sizeof(c0), "-");
            } else {
                snprintf(c0, sizeof(c0), "%s", ts.str);
            }
            snprintf(c1, sizeof(c1), "%s",
                     (e->batch_type == TPB_BATCH_TYPE_BENCHMARK)
                         ? "benchmark" : "run");
            snprintf(c2, sizeof(c2), "%.3f", dur_sec);
            snprintf(c3, sizeof(c3), "%u", e->ntask);
            snprintf(c4, sizeof(c4), "%u", e->nkernel);
            snprintf(c5, sizeof(c5), "%u", e->nscore);
            format_id_prefix6(e->tbatch_id, c6, sizeof(c6));

            cells[0] = c0;
            cells[1] = c1;
            cells[2] = c2;
            cells[3] = c3;
            cells[4] = c4;
            cells[5] = c5;
            cells[6] = c6;
            tpblog_printf_c(ratios, 7, TPBCLI_DB_LIST_COL_GAP, cells);
        }
    }

    free(entries);
    return TPBE_SUCCESS;
}

static int
list_task(const char *workspace, int count, int from_oldest)
{
    task_entry_t *all = NULL;
    task_entry_t *entries = NULL;
    kernel_entry_t *kentries = NULL;
    int nall = 0;
    int nk = 0;
    int total = 0;
    int err;
    int lo;
    int hi;
    int step;
    int nshow;
    int i;
    const char *headers[7];
    const char *cells[7];
    float ratios[7] = {19.0f, 16.0f, 6.0f, 12.0f, 7.0f, 9.0f, 9.0f};
    char c0[32];
    char c1[72];
    char c2[16];
    char c3[32];
    char c4[16];
    char c5[8];
    char c6[8];
    char kernel_fallback[8];
    tpb_datetime_str_t ts;

    err = tpb_raf_entry_list_task(workspace, &all, &nall);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, "tpb_raf_entry_list_task");
    }
    err = filter_task_entrypoints(all, nall, &entries, &total);
    free(all);
    if (err != TPBE_SUCCESS) {
        free(entries);
        return err;
    }
    (void)tpb_raf_entry_list_kernel(workspace, &kentries, &nk);

    compute_window(total, count, from_oldest, &lo, &hi, &step, &nshow);
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG,
               "List of %s %d task entry point%s\n",
               from_oldest ? "oldest" : "latest",
               nshow, nshow == 1 ? "" : "s");

    headers[0] = "Start Time (UTC)";
    headers[1] = "Kernel";
    headers[2] = "Exit";
    headers[3] = "Duration(s)";
    headers[4] = "Handle";
    headers[5] = "Task ID";
    headers[6] = "TBatch ID";
    tpblog_printf_c(ratios, 7, TPBCLI_DB_LIST_COL_GAP, headers);

    if (total == 0) {
        free(entries);
        free(kentries);
        return TPBE_SUCCESS;
    }

    if (step < 0) {
        for (i = hi; i >= lo; i--) {
            const task_entry_t *e = &entries[i];
            double dur_sec = (double)e->duration / 1e9;
            const char *kname;

            if (tpb_ts_bits_to_isoutc(e->utc_bits, &ts) != 0) {
                snprintf(c0, sizeof(c0), "-");
            } else {
                snprintf(c0, sizeof(c0), "%s", ts.str);
            }
            kname = lookup_kernel_name(kentries, nk, e->kernel_id,
                                       kernel_fallback, sizeof(kernel_fallback));
            snprintf(c1, sizeof(c1), "%s", kname);
            snprintf(c2, sizeof(c2), "%u", e->exit_code);
            snprintf(c3, sizeof(c3), "%.3f", dur_sec);
            snprintf(c4, sizeof(c4), "%u", e->handle_index);
            format_id_prefix6(e->task_record_id, c5, sizeof(c5));
            format_id_prefix6(e->tbatch_id, c6, sizeof(c6));

            cells[0] = c0;
            cells[1] = c1;
            cells[2] = c2;
            cells[3] = c3;
            cells[4] = c4;
            cells[5] = c5;
            cells[6] = c6;
            tpblog_printf_c(ratios, 7, TPBCLI_DB_LIST_COL_GAP, cells);
        }
    } else {
        for (i = lo; i <= hi; i++) {
            const task_entry_t *e = &entries[i];
            double dur_sec = (double)e->duration / 1e9;
            const char *kname;

            if (tpb_ts_bits_to_isoutc(e->utc_bits, &ts) != 0) {
                snprintf(c0, sizeof(c0), "-");
            } else {
                snprintf(c0, sizeof(c0), "%s", ts.str);
            }
            kname = lookup_kernel_name(kentries, nk, e->kernel_id,
                                       kernel_fallback, sizeof(kernel_fallback));
            snprintf(c1, sizeof(c1), "%s", kname);
            snprintf(c2, sizeof(c2), "%u", e->exit_code);
            snprintf(c3, sizeof(c3), "%.3f", dur_sec);
            snprintf(c4, sizeof(c4), "%u", e->handle_index);
            format_id_prefix6(e->task_record_id, c5, sizeof(c5));
            format_id_prefix6(e->tbatch_id, c6, sizeof(c6));

            cells[0] = c0;
            cells[1] = c1;
            cells[2] = c2;
            cells[3] = c3;
            cells[4] = c4;
            cells[5] = c5;
            cells[6] = c6;
            tpblog_printf_c(ratios, 7, TPBCLI_DB_LIST_COL_GAP, cells);
        }
    }

    free(entries);
    free(kentries);
    return TPBE_SUCCESS;
}

static int
list_kernel(const char *workspace, int count, int from_oldest)
{
    kernel_entry_t *entries = NULL;
    int total = 0;
    int err;
    int lo;
    int hi;
    int step;
    int nshow;
    int i;
    const char *headers[6];
    const char *cells[6];
    float ratios[6] = {18.0f, 8.0f, 8.0f, 9.0f, 20.0f, 9.0f};
    char c0[72];
    char c1[8];
    char c2[16];
    char c3[16];
    char c4[32];
    char c5[8];
    tpb_datetime_str_t ts;

    err = tpb_raf_entry_list_kernel(workspace, &entries, &total);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, "tpb_raf_entry_list_kernel");
    }

    compute_window(total, count, from_oldest, &lo, &hi, &step, &nshow);
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG,
               "List of %s %d kernel record%s\n",
               from_oldest ? "oldest" : "latest",
               nshow, nshow == 1 ? "" : "s");

    headers[0] = "Kernel Name";
    headers[1] = "Active";
    headers[2] = "NParm";
    headers[3] = "NMetric";
    headers[4] = "Build Time (UTC)";
    headers[5] = "Kernel ID";
    tpblog_printf_c(ratios, 6, TPBCLI_DB_LIST_COL_GAP, headers);

    if (total == 0) {
        free(entries);
        return TPBE_SUCCESS;
    }

    if (step < 0) {
        for (i = hi; i >= lo; i--) {
            const kernel_entry_t *e = &entries[i];

            snprintf(c0, sizeof(c0), "%s",
                     e->kernel_name[0] != '\0' ? e->kernel_name : "-");
            snprintf(c1, sizeof(c1), "%s", e->active ? "yes" : "no");
            snprintf(c2, sizeof(c2), "%u", e->nparm);
            snprintf(c3, sizeof(c3), "%u", e->nmetric);
            if (tpb_ts_bits_to_isoutc(e->utc_bits, &ts) != 0) {
                snprintf(c4, sizeof(c4), "-");
            } else {
                snprintf(c4, sizeof(c4), "%s", ts.str);
            }
            format_id_prefix6(e->kernel_id, c5, sizeof(c5));

            cells[0] = c0;
            cells[1] = c1;
            cells[2] = c2;
            cells[3] = c3;
            cells[4] = c4;
            cells[5] = c5;
            tpblog_printf_c(ratios, 6, TPBCLI_DB_LIST_COL_GAP, cells);
        }
    } else {
        for (i = lo; i <= hi; i++) {
            const kernel_entry_t *e = &entries[i];

            snprintf(c0, sizeof(c0), "%s",
                     e->kernel_name[0] != '\0' ? e->kernel_name : "-");
            snprintf(c1, sizeof(c1), "%s", e->active ? "yes" : "no");
            snprintf(c2, sizeof(c2), "%u", e->nparm);
            snprintf(c3, sizeof(c3), "%u", e->nmetric);
            if (tpb_ts_bits_to_isoutc(e->utc_bits, &ts) != 0) {
                snprintf(c4, sizeof(c4), "-");
            } else {
                snprintf(c4, sizeof(c4), "%s", ts.str);
            }
            format_id_prefix6(e->kernel_id, c5, sizeof(c5));

            cells[0] = c0;
            cells[1] = c1;
            cells[2] = c2;
            cells[3] = c3;
            cells[4] = c4;
            cells[5] = c5;
            tpblog_printf_c(ratios, 6, TPBCLI_DB_LIST_COL_GAP, cells);
        }
    }

    free(entries);
    return TPBE_SUCCESS;
}

/*
 * When: `database list` or `database ls` is selected (see tpbcli_database).
 * Input: workspace — path from tpb_raf_resolve_workspace; domain and window args.
 * Output: User-visible table; return TPBE_SUCCESS or rafdb error code.
 */
int
tpbcli_database_ls(const char *workspace, uint8_t domain,
                   int count, int from_oldest)
{
    int err;

    if (workspace == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG,
               "TPB_WORKSPACE: %s\n", workspace);
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG,
               "Reading records ... ");

    if (domain == TPB_RAF_DOM_TBATCH) {
        err = list_tbatch(workspace, count, from_oldest);
    } else if (domain == TPB_RAF_DOM_TASK) {
        err = list_task(workspace, count, from_oldest);
    } else if (domain == TPB_RAF_DOM_KERNEL) {
        err = list_kernel(workspace, count, from_oldest);
    } else {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "Failed\n");
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }

    if (err != TPBE_SUCCESS) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "Failed\n");
        return err;
    }
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "Done\n");
    return TPBE_SUCCESS;
}
