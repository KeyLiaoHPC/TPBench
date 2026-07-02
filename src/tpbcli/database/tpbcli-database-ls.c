/*
 * tpbcli-database-ls.c
 * `tpbcli database list` / `ls` — tbatch entry listing.
 */

#include <stdlib.h>
#include <string.h>

#include "tpb-public.h"
#include "tpbcli-database.h"

#define RECORD_LIST_MAX 20

/* Local Function Prototypes */

/*
 * When: Before printing the tbatch table body.
 * Input: None.
 * Output: Writes column header line to stdout via tpblog_printf_f.
 */
static void print_list_header(void);

/*
 * When: For each tbatch entry row in the list.
 * Input: e — pointer to one tbatch_entry_t from rafdb.
 * Output: Writes one formatted data row to stdout.
 */
static void print_list_row(const tbatch_entry_t *e);

static void
print_list_header(void)
{
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
        "%-20s %-40s %-10s %5s %7s %6s %13s\n",
        "Start Time (UTC)", "TBatch ID", "Type",
        "NTask", "NKernel", "NScore", "Duration (s)");
}

static void
print_list_row(const tbatch_entry_t *e)
{
    char hex[41];
    tpb_datetime_str_t ts;
    const char *type_str;
    double dur_sec;

    tpb_raf_id_to_hex(e->tbatch_id, hex);
    tpb_ts_bits_to_isoutc(e->start_utc_bits, &ts);

    type_str = (e->batch_type == TPB_BATCH_TYPE_BENCHMARK)
             ? "benchmark" : "run";

    dur_sec = (double)e->duration / 1e9;

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
        "%-20s %-40s %-10s %5u %7u %6u %13.3f\n",
        ts.str, hex, type_str,
        e->ntask, e->nkernel, e->nscore, dur_sec);
}

/*
 * When: `database list` or `database ls` is selected (see tpbcli_database).
 * Input: workspace — path from tpb_raf_resolve_workspace.
 * Output: User-visible table; return TPBE_SUCCESS or rafdb error code.
 */
int
tpbcli_database_ls(const char *workspace)
{
    tbatch_entry_t *entries = NULL;
    int count = 0;
    int err;
    int i, start;

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG,
               "TPB_WORKSPACE: %s\n", workspace);
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG,
               "Reading records ... ");

    err = tpb_raf_entry_list_tbatch(workspace, &entries,
                                      &count);
    if (err != TPBE_SUCCESS) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, "Failed\n");
        return err;
    }
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, "Done\n");

    if (count == 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG,
                   "No tbatch records found.\n");
        return TPBE_SUCCESS;
    }

    start = (count > RECORD_LIST_MAX)
          ? count - RECORD_LIST_MAX : 0;

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG,
               "List of latest %d tbatch records\n",
               count - start);
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, "===\n");
    print_list_header();

    for (i = count - 1; i >= start; i--) {
        print_list_row(&entries[i]);
    }

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, "===\n");

    free(entries);
    return TPBE_SUCCESS;
}
