/*
 * tpbcli-rtenv-list.c
 * `tpbcli rtenv list|ls` — list runtime environment entries.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tpb-public.h"
#include "tpbcli-rtenv-template.h"

#define TPBCLI_RTENV_LIST_COL_GAP 1

static const float s_col_ratios[] = {
    6.0f, 18.0f, 14.0f, 20.0f, 8.0f, 8.0f, 6.0f, 6.0f, 14.0f
};

int
tpbcli_rtenv_list(int argc, char **argv, const char *workspace)
{
    tpb_raf_rtenv_entry_t *entries = NULL;
    int count = 0;
    int32_t active_id = -1;
    int active_found = 0;
    int i;
    int err;

    (void)argc;
    (void)argv;

    err = tpbcli_rtenv_resolve_list_active(workspace, &active_id,
                                           &active_found);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_CLI_RTENV, err, "resolve list active");
    }

    if (active_found) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                   TPBLOG_FLAG_DIRECT,
                   "Activated runtime environment ID: %d\n\n",
                   (int)active_id);
    } else {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                   TPBLOG_FLAG_DIRECT,
                   "Activated runtime environment ID: N/A\n\n");
    }

    err = tpb_raf_entry_list_rtenv(workspace, &entries, &count);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_CLI_RTENV, err, "tpb_raf_entry_list_rtenv");
    }

    {
        const char *headers[] = {
            "ID", "Name", "Hostname", "Created UTC", "NTask", "NTBatch",
            "NApp", "NEnv", "Note"
        };

        tpblog_printf_c(s_col_ratios, 9, TPBCLI_RTENV_LIST_COL_GAP, headers);
    }

    for (i = 0; i < count; i++) {
        tpb_datetime_str_t ts;
        char idbuf[16];
        char ntask[16];
        char nbatch[16];
        char napp[16];
        char nenv[16];
        const char *cells[9];

        snprintf(idbuf, sizeof(idbuf), "%d", (int)entries[i].id);
        if (tpb_ts_bits_to_isoutc(entries[i].utc_bits, &ts) != TPBE_SUCCESS) {
            snprintf(ts.str, sizeof(ts.str), "%llu",
                     (unsigned long long)entries[i].utc_bits);
        }
        snprintf(ntask, sizeof(ntask), "%u", entries[i].ntask);
        snprintf(nbatch, sizeof(nbatch), "%u", entries[i].ntbatch);
        snprintf(napp, sizeof(napp), "%u", entries[i].napp);
        snprintf(nenv, sizeof(nenv), "%u", entries[i].nenv);

        cells[0] = idbuf;
        cells[1] = entries[i].name;
        cells[2] = entries[i].hostname;
        cells[3] = ts.str;
        cells[4] = ntask;
        cells[5] = nbatch;
        cells[6] = napp;
        cells[7] = nenv;
        cells[8] = entries[i].note;
        tpblog_printf_c(s_col_ratios, 9, TPBCLI_RTENV_LIST_COL_GAP, cells);
    }

    free(entries);
    return TPBE_SUCCESS;
}
