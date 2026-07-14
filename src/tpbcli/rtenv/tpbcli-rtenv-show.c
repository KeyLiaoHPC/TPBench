/*
 * tpbcli-rtenv-show.c
 * `tpbcli rtenv show` — display merged runtime environment.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tpb-public.h"
#include "tpbcli-rtenv-template.h"

#define TPBCLI_RTENV_SHOW_GAP 2

static const float s_app_col_ratios[] = { 0.08f, 0.30f, 0.22f, 0.40f };
static const float s_var_col_ratios[] = { 0.08f, 0.14f, 0.14f, 0.18f, 0.46f };

/* Local Function Prototypes */

static int _sf_parse_selector(int argc, char **argv, const char *workspace,
                              int32_t *id_out);
static const char *_sf_on_set_name(uint32_t v);
static const char *_sf_on_get_name(uint32_t v);

static int
_sf_parse_selector(int argc, char **argv, const char *workspace,
                   int32_t *id_out)
{
    int i;
    int have_id = 0;
    int32_t id = 0;

    for (i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--id") == 0) {
            if (i + 1 >= argc) {
                TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL, "missing -i value");
            }
            id = (int32_t)strtol(argv[++i], NULL, 10);
            have_id = 1;
            continue;
        }
        if (strcmp(argv[i], "--name") == 0) {
            if (i + 1 >= argc) {
                TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL, "missing --name");
            }
            if (tpbcli_rtenv_find_id_by_name(workspace, argv[++i], &id) !=
                TPBE_SUCCESS) {
                TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_LIST_NOT_FOUND, "RTEnv id");
            }
            have_id = 1;
        }
    }

    if (have_id) {
        if (!tpbcli_rtenv_id_exists(workspace, id)) {
            TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_LIST_NOT_FOUND, "RTEnv id");
        }
        *id_out = id;
        return TPBE_SUCCESS;
    }

    if (tpbcli_rtenv_resolve_active_id_cli(workspace, &id) == TPBE_SUCCESS &&
        tpbcli_rtenv_id_exists(workspace, id)) {
        *id_out = id;
        return TPBE_SUCCESS;
    }

    tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
               "error: no active runtime environment; use -i or "
               "tpbcli rtenv list\n");
    TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL, NULL);
}

static const char *
_sf_on_set_name(uint32_t v)
{
    switch (v) {
    case TPB_RTENV_ON_SET_IGNORE: return "ignore";
    case TPB_RTENV_ON_SET_OVERWRITE: return "overwrite";
    case TPB_RTENV_ON_SET_PREPEND: return "prepend";
    case TPB_RTENV_ON_SET_APPEND: return "append";
    default: return "?";
    }
}

static const char *
_sf_on_get_name(uint32_t v)
{
    switch (v) {
    case TPB_RTENV_ON_GET_IGNORE: return "ignore";
    case TPB_RTENV_ON_GET_WARN: return "warn";
    case TPB_RTENV_ON_GET_FAIL: return "fail";
    case TPB_RTENV_ON_GET_OVERWRITE: return "overwrite";
    default: return "?";
    }
}

int
tpbcli_rtenv_show(int argc, char **argv, const char *workspace)
{
    int32_t id;
    tpb_raf_rtenv_entry_t *entries = NULL;
    tpbcli_rtenv_merged_t merged;
    tpb_datetime_str_t ts;
    int count = 0;
    int i;
    int err;
    const char *hdrs[4];
    char idbuf[16];
    uint32_t wrap_flags[5];

    err = _sf_parse_selector(argc, argv, workspace, &id);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    err = tpbcli_rtenv_merge_chain(workspace, id, &merged);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_CLI_RTENV, err, "merge chain");
    }

    err = tpb_raf_entry_list_rtenv(workspace, &entries, &count);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_CLI_RTENV, err, "entry list");
    }

    for (i = 0; i < count; i++) {
        if (entries[i].id != id) {
            continue;
        }
        if (tpb_ts_bits_to_isoutc(entries[i].utc_bits, &ts) != TPBE_SUCCESS) {
            snprintf(ts.str, sizeof(ts.str), "%llu",
                     (unsigned long long)entries[i].utc_bits);
        }
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                   TPBLOG_FLAG_DIRECT, "ID: %d\n", (int)entries[i].id);
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                   TPBLOG_FLAG_DIRECT, "Name: %s\n", entries[i].name);
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                   TPBLOG_FLAG_DIRECT, "Hostname: %s\n", entries[i].hostname);
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                   TPBLOG_FLAG_DIRECT, "Created UTC: %s\n", ts.str);
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                   TPBLOG_FLAG_DIRECT, "Inherit from: %d\n",
                   (int)entries[i].inherit_from);
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                   TPBLOG_FLAG_DIRECT, "Derive to: %d\n",
                   (int)entries[i].derive_to);
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                   TPBLOG_FLAG_DIRECT, "NTask: %u NTBatch: %u\n",
                   entries[i].ntask, entries[i].ntbatch);
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                   TPBLOG_FLAG_DIRECT, "Note: %s\n\n", entries[i].note);
        break;
    }
    free(entries);

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "Applications (merged)\n");
    hdrs[0] = "ID";
    hdrs[1] = "Name";
    hdrs[2] = "Version";
    hdrs[3] = "Note";
    tpblog_printf_c(s_app_col_ratios, 4, TPBCLI_RTENV_SHOW_GAP, hdrs);
    for (i = 0; i < merged.napp; i++) {
        char row[4][256];

        snprintf(idbuf, sizeof(idbuf), "%d", i + 1);
        snprintf(row[0], sizeof(row[0]), "%5s", idbuf);
        snprintf(row[1], sizeof(row[1]), "%s", merged.apps[i].name);
        snprintf(row[2], sizeof(row[2]), "%s", merged.apps[i].version);
        snprintf(row[3], sizeof(row[3]), "%s", merged.apps[i].note);
        hdrs[0] = row[0];
        hdrs[1] = row[1];
        hdrs[2] = row[2];
        hdrs[3] = row[3];
        tpblog_printf_c(s_app_col_ratios, 4, TPBCLI_RTENV_SHOW_GAP, hdrs);
    }

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "\nEnvironment Variables (merged)\n");
    {
        const char *vhdrs[5];

        vhdrs[0] = "ID";
        vhdrs[1] = "On_set";
        vhdrs[2] = "On_get";
        vhdrs[3] = "Key";
        vhdrs[4] = "Value";
        tpblog_printf_c(s_var_col_ratios, 5, TPBCLI_RTENV_SHOW_GAP, vhdrs);
        wrap_flags[0] = TPBLOG_WRAP_HYPHEN;
        wrap_flags[1] = TPBLOG_WRAP_HYPHEN;
        wrap_flags[2] = TPBLOG_WRAP_HYPHEN;
        wrap_flags[3] = TPBLOG_WRAP_HYPHEN;
        wrap_flags[4] = TPBLOG_WRAP_NO_HYPHEN;
        for (i = 0; i < merged.nenv; i++) {
            char row[5][4096];

            snprintf(idbuf, sizeof(idbuf), "%d", i + 1);
            snprintf(row[0], sizeof(row[0]), "%5s", idbuf);
            snprintf(row[1], sizeof(row[1]), "%s",
                     _sf_on_set_name(merged.vars[i].on_set));
            snprintf(row[2], sizeof(row[2]), "%s",
                     _sf_on_get_name(merged.vars[i].on_get));
            snprintf(row[3], sizeof(row[3]), "%s", merged.vars[i].key);
            snprintf(row[4], sizeof(row[4]), "%s", merged.vars[i].value);
            vhdrs[0] = row[0];
            vhdrs[1] = row[1];
            vhdrs[2] = row[2];
            vhdrs[3] = row[3];
            vhdrs[4] = row[4];
            tpblog_printf_c_flags(s_var_col_ratios, 5, TPBCLI_RTENV_SHOW_GAP,
                                  vhdrs, wrap_flags);
        }
    }

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "\nLoad with: eval \"$(tpbcli rtenv load -i %d)\"\n", (int)id);
    return TPBE_SUCCESS;
}
