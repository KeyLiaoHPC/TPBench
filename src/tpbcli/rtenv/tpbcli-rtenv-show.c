/*
 * tpbcli-rtenv-show.c
 * `tpbcli rtenv show` — display merged runtime environment.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tpb-public.h"
#include "tpbcli-rtenv-template.h"

/* Local Function Prototypes */

static int _sf_parse_selector(int argc, char **argv, const char *workspace,
                              int32_t *id_out);
static const char *_sf_mode_name(uint32_t mode);

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
                TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_LIST_NOT_FOUND, "RTEnv name");
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
_sf_mode_name(uint32_t mode)
{
    switch (mode) {
    case 0: return "override";
    case 1: return "prepend";
    case 2: return "append";
    case 3: return "unset";
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
               "Applications (merged):\n");
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "Name\tVersion\tNote\n");
    for (i = 0; i < merged.napp; i++) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                   TPBLOG_FLAG_DIRECT, "%s\t%s\t%s\n",
                   merged.apps[i].name, merged.apps[i].version,
                   merged.apps[i].note);
    }

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "\nEnvironment variables (merged):\n");
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "Key\tMode\tValue\n");
    for (i = 0; i < merged.nenv; i++) {
        if (merged.vars[i].mode == 3) {
            continue;
        }
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO,
                   TPBLOG_FLAG_DIRECT, "%s\t%s\t%s\n",
                   merged.vars[i].key, _sf_mode_name(merged.vars[i].mode),
                   merged.vars[i].value);
    }

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
               "\nLoad with: eval \"$(tpbcli rtenv load -i %d)\"\n", (int)id);
    return TPBE_SUCCESS;
}
