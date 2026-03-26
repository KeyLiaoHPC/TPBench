/*
 * tpbcli-database-ls.c
 * `tpbcli database list` / `ls` — tbatch entry listing.
 */

#include <stdlib.h>
#include <string.h>

#include "corelib/raw_db/tpb-rawdb-types.h"
#include "corelib/strftime.h"
#include "tpbcli-database.h"

#define RECORD_LIST_MAX 20

/* Local Function Prototypes */

/*
 * When: Before printing the tbatch table body.
 * Input: None.
 * Output: Writes column header line to stdout via tpb_printf.
 */
static void print_list_header(void);

/*
 * When: For each tbatch entry row in the list.
 * Input: e — pointer to one tbatch_entry_t from rawdb.
 * Output: Writes one formatted data row to stdout.
 */
static void print_list_row(const tbatch_entry_t *e);

static void
print_list_header(void)
{
    tpb_printf(TPBM_PRTN_M_DIRECT,
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

    tpb_rawdb_id_to_hex(e->tbatch_id, hex);
    tpb_ts_bits_to_isoutc(e->start_utc_bits, &ts);

    type_str = (e->batch_type == TPB_BATCH_TYPE_BENCHMARK)
             ? "benchmark" : "run";

    dur_sec = (double)e->duration / 1e9;

    tpb_printf(TPBM_PRTN_M_DIRECT,
        "%-20s %-40s %-10s %5u %7u %6u %13.3f\n",
        ts.str, hex, type_str,
        e->ntask, e->nkernel, e->nscore, dur_sec);
}

/*
 * When: `database list` or `database ls` is selected (see tpbcli_database).
 * Input: workspace — path from tpb_rawdb_resolve_workspace.
 * Output: User-visible table; return TPBE_SUCCESS or rawdb error code.
 */
int
tpbcli_database_ls(const char *workspace)
{
    tbatch_entry_t *entries = NULL;
    int count = 0;
    int err;
    int i, start;

    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
               "TPB_WORKSPACE: %s\n", workspace);
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
               "Reading records ... ");

    err = tpb_rawdb_entry_list_tbatch(workspace, &entries,
                                      &count);
    if (err != TPBE_SUCCESS) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "Failed\n");
        return err;
    }
    tpb_printf(TPBM_PRTN_M_DIRECT, "Done\n");

    if (count == 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
                   "No tbatch records found.\n");
        return TPBE_SUCCESS;
    }

    start = (count > RECORD_LIST_MAX)
          ? count - RECORD_LIST_MAX : 0;

    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
               "List of latest %d tbatch records\n",
               count - start);
    tpb_printf(TPBM_PRTN_M_DIRECT, "===\n");
    print_list_header();

    for (i = count - 1; i >= start; i--) {
        print_list_row(&entries[i]);
    }

    tpb_printf(TPBM_PRTN_M_DIRECT, "===\n");

    free(entries);
    return TPBE_SUCCESS;
}
