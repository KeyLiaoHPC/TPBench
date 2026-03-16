/*
 * tpbcli-database.c
 * Front-end for `tpbcli database` subcommand.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "corelib/tpb-types.h"
#include "corelib/tpb-io.h"
#include "corelib/strftime.h"
#include "corelib/raw_db/tpb-rawdb-types.h"
#include "tpbcli-database.h"

/* Local Function Prototypes */
static int record_list(const char *workspace);
static void print_list_header(void);
static void print_list_row(const tbatch_entry_t *e);

#define RECORD_LIST_MAX 20

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
 * List the latest RECORD_LIST_MAX tbatch entries.
 */
static int
record_list(const char *workspace)
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

int
tpbcli_database(int argc, char **argv)
{
    char workspace[TPB_RAWDB_PATH_MAX];
    int err;

    err = tpb_rawdb_resolve_workspace(workspace,
                                      sizeof(workspace));
    if (err != TPBE_SUCCESS) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "Failed to resolve workspace.\n");
        return err;
    }

    if (argc < 3) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Usage: tpbcli log <list|ls>\n");
        return TPBE_CLI_FAIL;
    }

    if (strcmp(argv[2], "list") == 0 ||
        strcmp(argv[2], "ls") == 0) {
        return record_list(workspace);
    }

    tpb_printf(TPBM_PRTN_M_DIRECT,
               "Unknown log action: %s\n"
               "Usage: tpbcli log <list|ls>\n",
               argv[2]);
    return TPBE_CLI_FAIL;
}
