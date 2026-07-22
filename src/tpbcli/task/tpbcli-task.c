/*
 * tpbcli-task.c
 * `tpbcli task` argp tree and subcommand dispatch.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#ifdef __linux__
#include <linux/limits.h>
#else
#include <limits.h>
#endif

#include "tpb-public.h"
#include "tpbcli-argp.h"
#include "tpbcli-task.h"
#include "tpbcli-task-internal.h"

/* Local Function Prototypes */
static tpbcli_argtree_t *_sf_build_task_argtree(void *ctx);
static int _sf_add_ls_options(tpbcli_argnode_t *parent, void *ctx);
static int _sf_add_gr_options(tpbcli_argnode_t *parent, void *ctx);
static int _sf_add_export_options(tpbcli_argnode_t *parent, void *ctx);
static void _sf_emit_export_help(const tpbcli_argnode_t *node, FILE *out);
static int _sf_parse_export_cmd(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_export_rid(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_export_task_id(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_export_from_ls(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_export_trace_entry(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_export_keep(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_export_filter(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_export_outdir(tpbcli_argnode_t *node, const char *value);
static void _sf_emit_task_help(const tpbcli_argnode_t *node, FILE *out);
static void _sf_emit_ls_help(const tpbcli_argnode_t *node, FILE *out);
static void _sf_emit_gr_help(const tpbcli_argnode_t *node, FILE *out);
static int _sf_parse_ls_cmd(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_gr_cmd(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_gr_rid(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_gr_task_id(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_gr_data_name(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_gr_meta_name(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_gr_show_subrank(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_gr_data_name_ctx_help(tpbcli_argnode_t *node,
                                           const char *value);
static int _sf_parse_gr_meta_name_ctx_help(tpbcli_argnode_t *node,
                                           const char *value);
static int _sf_parse_count_newest(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_count_oldest(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_filter_opt(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_nonneg_int(const char *value, int *out);
static int _sf_parse_one_filter(const char *text, tpbcli_task_filter_t *out);

typedef struct task_cli_ctx {
    int                         want_ls;
    int                         want_gr;
    int                         want_export;
    tpbcli_task_ls_opts_t       ls;
    tpbcli_task_gr_opts_t       gr;
    tpbcli_task_export_opts_t   export;
} task_cli_ctx_t;

static const char *_sf_conf_not_n[] = { "-N", NULL };
static const char *_sf_conf_not_N[] = { "-n", NULL };
static const char *_sf_gr_conf_not_rid[] = { "--task-id", NULL };
static const char *_sf_gr_conf_not_taskid[] = { "--rid", NULL };
static const char *_sf_exp_conf_not_rid[] = { "--id", "--from-ls", NULL };
static const char *_sf_exp_conf_not_id[] = { "--rid", "--from-ls", NULL };
static const char *_sf_exp_conf_not_from_ls[] = { "--rid", "--id", NULL };
static const char *_sf_conf_not_trace[] = { "--keep-current", NULL };
static const char *_sf_conf_not_keep[] = { "--trace-to-entry", NULL };

static int _sf_gr_merge_names(task_cli_ctx_t *ctx, char *dst[], int *ndst,
                              const char *value);

static int
_sf_parse_nonneg_int(const char *value, int *out)
{
    char *end;
    long v;

    if (value == NULL || value[0] == '\0' || out == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    v = strtol(value, &end, 10);
    if (end == value || *end != '\0' || v < 0 || v > INT_MAX) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: invalid count '%s' (non-negative integer required)\n",
                        value);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    *out = (int)v;
    return 0;
}

static int
_sf_parse_one_filter(const char *text, tpbcli_task_filter_t *out)
{
    const char *op;
    size_t key_len;
    char key[64];
    int err;

    if (text == NULL || out == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    memset(out, 0, sizeof(*out));

    if (strstr(text, ">=") != NULL) {
        op = strstr(text, ">=");
        out->op = TPBCLI_TASK_FOP_GE;
        key_len = (size_t)(op - text);
    } else if (strstr(text, "<=") != NULL) {
        op = strstr(text, "<=");
        out->op = TPBCLI_TASK_FOP_LE;
        key_len = (size_t)(op - text);
    } else if (strchr(text, '>') != NULL) {
        op = strchr(text, '>');
        out->op = TPBCLI_TASK_FOP_GT;
        key_len = (size_t)(op - text);
    } else if (strchr(text, '<') != NULL) {
        op = strchr(text, '<');
        out->op = TPBCLI_TASK_FOP_LT;
        key_len = (size_t)(op - text);
    } else if (strchr(text, '=') != NULL) {
        op = strchr(text, '=');
        out->op = TPBCLI_TASK_FOP_EQ;
        key_len = (size_t)(op - text);
    } else {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: invalid filter '%s' (missing operator)\n", text);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }

    if (key_len == 0 || key_len >= sizeof(key)) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: invalid filter key in '%s'\n", text);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    memcpy(key, text, key_len);
    key[key_len] = '\0';
    if (out->op == TPBCLI_TASK_FOP_GE || out->op == TPBCLI_TASK_FOP_LE) {
        snprintf(out->value, sizeof(out->value), "%s", op + 2);
    } else {
        snprintf(out->value, sizeof(out->value), "%s", op + 1);
    }
    if (out->value[0] == '\0') {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: empty filter value in '%s'\n", text);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }

    if (strcmp(key, "kernel_id") == 0) {
        size_t n = strlen(out->value);
        size_t i;

        if (out->op != TPBCLI_TASK_FOP_EQ) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT,
                            "error: kernel_id only supports '='\n");
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        if (n < 4 || n > 40) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT,
                            "error: kernel_id prefix must be 4-40 hex digits\n");
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        for (i = 0; i < n; i++) {
            if (!((out->value[i] >= '0' && out->value[i] <= '9') ||
                  (out->value[i] >= 'a' && out->value[i] <= 'f') ||
                  (out->value[i] >= 'A' && out->value[i] <= 'F'))) {
                tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                                TPBLOG_FLAG_DIRECT,
                                "error: kernel_id must be hexadecimal\n");
                TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
            }
        }
        out->key = TPBCLI_TASK_FKEY_KERNEL_ID;
        return 0;
    }
    if (strcmp(key, "kernel_name") == 0) {
        if (out->op != TPBCLI_TASK_FOP_EQ) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT,
                            "error: kernel_name only supports '='\n");
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        out->key = TPBCLI_TASK_FKEY_KERNEL_NAME;
        return 0;
    }
    if (strcmp(key, "tbatch_id") == 0) {
        size_t n = strlen(out->value);
        size_t i;

        if (out->op != TPBCLI_TASK_FOP_EQ) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT,
                            "error: tbatch_id only supports '='\n");
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        if (n < 4 || n > 40) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT,
                            "error: tbatch_id prefix must be 4-40 hex digits\n");
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        for (i = 0; i < n; i++) {
            if (!((out->value[i] >= '0' && out->value[i] <= '9') ||
                  (out->value[i] >= 'a' && out->value[i] <= 'f') ||
                  (out->value[i] >= 'A' && out->value[i] <= 'F'))) {
                tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                                TPBLOG_FLAG_DIRECT,
                                "error: tbatch_id must be hexadecimal\n");
                TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
            }
        }
        out->key = TPBCLI_TASK_FKEY_TBATCH_ID;
        return 0;
    }
    if (strcmp(key, "datetime") == 0) {
        out->key = TPBCLI_TASK_FKEY_DATETIME;
        err = tpbcli_task_time_parse_filter(out->value, &out->datetime_utc);
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
        return 0;
    }
    if (strcmp(key, "exit_code") == 0) {
        char *end;
        long v;

        if (out->op != TPBCLI_TASK_FOP_EQ) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT,
                            "error: exit_code only supports '='\n");
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        v = strtol(out->value, &end, 10);
        if (end == out->value || *end != '\0' || v < 0 ||
            v > (long)UINT32_MAX) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT,
                            "error: invalid exit_code '%s'\n", out->value);
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        out->key = TPBCLI_TASK_FKEY_EXIT_CODE;
        out->exit_code = (uint32_t)v;
        return 0;
    }

    if (strcmp(key, "duration") == 0 || strcmp(key, "subrank") == 0 ||
        strcmp(key, "subtid") == 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: filter key '%s' is not valid for task ls "
                        "(duration is unsupported; subrank/subtid belong to "
                        "task export)\n",
                        key);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }

    tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                    "error: unknown filter key '%s' for task ls "
                    "(supported: kernel_id, kernel_name, tbatch_id, datetime, "
                    "exit_code)\n",
                    key);
    TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
}

static int
_sf_parse_ls_cmd(tpbcli_argnode_t *node, const char *value)
{
    task_cli_ctx_t *ctx = (task_cli_ctx_t *)node->user_data;

    (void)value;
    if (ctx != NULL) {
        ctx->want_ls = 1;
    }
    return 0;
}

static int
_sf_parse_count_newest(tpbcli_argnode_t *node, const char *value)
{
    task_cli_ctx_t *ctx = (task_cli_ctx_t *)node->user_data;
    int n;
    int err;

    err = _sf_parse_nonneg_int(value, &n);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    if (ctx != NULL) {
        ctx->ls.count = n;
        ctx->ls.from_oldest = 0;
    }
    return 0;
}

static int
_sf_parse_count_oldest(tpbcli_argnode_t *node, const char *value)
{
    task_cli_ctx_t *ctx = (task_cli_ctx_t *)node->user_data;
    int n;
    int err;

    err = _sf_parse_nonneg_int(value, &n);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    if (ctx != NULL) {
        ctx->ls.count = n;
        ctx->ls.from_oldest = 1;
    }
    return 0;
}

static int
_sf_parse_filter_opt(tpbcli_argnode_t *node, const char *value)
{
    task_cli_ctx_t *ctx = (task_cli_ctx_t *)node->user_data;
    tpbcli_task_filter_t f;
    int err;

    if (ctx == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    if (ctx->ls.nfilter >= TPBCLI_TASK_FILTER_MAX) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: too many -f filters (max %d)\n",
                        TPBCLI_TASK_FILTER_MAX);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    err = _sf_parse_one_filter(value, &f);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    ctx->ls.filters[ctx->ls.nfilter++] = f;
    return 0;
}

static void
_sf_emit_ls_help(const tpbcli_argnode_t *node, FILE *out)
{
    (void)node;
    (void)out;
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "Usage: tpbcli task ls|list [-n N|-N N] "
                    "[-f|--filter KEYOPVALUE]...\n"
                    "  -n N     show newest N results (0 = unlimited)\n"
                    "  -N N     show oldest N results (0 = unlimited)\n"
                    "  -f KEYOPVALUE\n"
                    "           filter keys: kernel_id, kernel_name, tbatch_id,\n"
                    "           datetime, exit_code\n");
}

static void
_sf_emit_task_help(const tpbcli_argnode_t *node, FILE *out)
{
    (void)node;
    (void)out;
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "Usage: tpbcli [--workspace PATH] task <subcommand>\n"
                    "Subcommands:\n"
                    "  ls|list         List task entry points and refresh RIDMAP\n"
                    "  get-result|gr   Compare task meta and pooled metrics\n"
                    "  export          Export paired meta/data CSV files\n");
}

static int
_sf_add_ls_options(tpbcli_argnode_t *parent, void *ctx)
{
    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "-n",
            .short_name = NULL,
            .desc = "Show newest N results",
            .type = TPBCLI_ARG_OPT,
            .flags = 0,
            .max_chosen = 1,
            .parse_fn = _sf_parse_count_newest,
            .conflict_opts = _sf_conf_not_n,
            .user_data = ctx,
        }) == NULL) {
        return -1;
    }
    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "-N",
            .short_name = NULL,
            .desc = "Show oldest N results",
            .type = TPBCLI_ARG_OPT,
            .flags = 0,
            .max_chosen = 1,
            .parse_fn = _sf_parse_count_oldest,
            .conflict_opts = _sf_conf_not_N,
            .user_data = ctx,
        }) == NULL) {
        return -1;
    }
    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "--filter",
            .short_name = "-f",
            .desc = "Filter KEYOPVALUE",
            .type = TPBCLI_ARG_OPT,
            .flags = 0,
            .max_chosen = TPBCLI_TASK_FILTER_MAX,
            .parse_fn = _sf_parse_filter_opt,
            .user_data = ctx,
        }) == NULL) {
        return -1;
    }
    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "--help",
            .short_name = "-h",
            .desc = "Show ls help",
            .type = TPBCLI_ARG_FLAG,
            .max_chosen = 0,
            .help_fn = _sf_emit_ls_help,
        }) == NULL) {
        return -1;
    }
    return 0;
}

static int
_sf_gr_merge_names(task_cli_ctx_t *ctx, char *dst[], int *ndst, const char *value)
{
    char **parsed = NULL;
    int np = 0;
    int err;
    int i;
    int j;

    if (ctx == NULL || dst == NULL || ndst == NULL || value == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_NULLPTR_ARG, NULL);
    }
    if (strcmp(value, "--help") == 0 || strcmp(value, "-h") == 0) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_EXIT_ON_HELP, NULL);
    }
    err = tpbcli_task_parse_name_csv(value, &parsed, &np);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    for (i = 0; i < np; i++) {
        int dup = 0;
        for (j = 0; j < *ndst; j++) {
            if (strcmp(dst[j], parsed[i]) == 0) {
                dup = 1;
                break;
            }
        }
        if (dup) {
            free(parsed[i]);
            continue;
        }
        if (*ndst >= TPBCLI_TASK_GR_NAME_MAX) {
            for (j = i; j < np; j++) {
                free(parsed[j]);
            }
            free(parsed);
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT,
                            "error: too many name entries (max %d)\n",
                            TPBCLI_TASK_GR_NAME_MAX);
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        dst[*ndst] = parsed[i];
        (*ndst)++;
    }
    free(parsed);
    return 0;
}

static int
_sf_parse_gr_cmd(tpbcli_argnode_t *node, const char *value)
{
    task_cli_ctx_t *ctx = (task_cli_ctx_t *)node->user_data;

    (void)value;
    if (ctx != NULL) {
        ctx->want_gr = 1;
    }
    return 0;
}

static int
_sf_parse_gr_rid(tpbcli_argnode_t *node, const char *value)
{
    task_cli_ctx_t *ctx = (task_cli_ctx_t *)node->user_data;

    if (ctx == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    ctx->gr.sel_kind = TPBCLI_TASK_SEL_RID;
    ctx->gr.rid_spec = value;
    return 0;
}

static int
_sf_parse_gr_task_id(tpbcli_argnode_t *node, const char *value)
{
    task_cli_ctx_t *ctx = (task_cli_ctx_t *)node->user_data;

    if (ctx == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    ctx->gr.sel_kind = TPBCLI_TASK_SEL_TASK_ID;
    ctx->gr.task_id_spec = value;
    return 0;
}

static int
_sf_parse_gr_data_name(tpbcli_argnode_t *node, const char *value)
{
    task_cli_ctx_t *ctx = (task_cli_ctx_t *)node->user_data;
    int err;

    if (ctx == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    if (value != NULL &&
        (strcmp(value, "--help") == 0 || strcmp(value, "-h") == 0)) {
        ctx->gr.data_name_help = 1;
        return 0;
    }
    /* Empty '' means omit Record Data (design §7.1); still marks the option. */
    ctx->gr.data_name_given = 1;
    if (value == NULL || value[0] == '\0') {
        return 0;
    }
    err = _sf_gr_merge_names(ctx, ctx->gr.data_names, &ctx->gr.ndata_names,
                             value);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    return 0;
}

static int
_sf_parse_gr_meta_name(tpbcli_argnode_t *node, const char *value)
{
    task_cli_ctx_t *ctx = (task_cli_ctx_t *)node->user_data;
    int err;

    if (ctx == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    if (value != NULL &&
        (strcmp(value, "--help") == 0 || strcmp(value, "-h") == 0)) {
        ctx->gr.meta_name_help = 1;
        return 0;
    }
    err = _sf_gr_merge_names(ctx, ctx->gr.meta_names, &ctx->gr.nmeta_names,
                             value);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    return 0;
}

static int
_sf_parse_gr_data_name_ctx_help(tpbcli_argnode_t *node, const char *value)
{
    task_cli_ctx_t *ctx;

    (void)value;
    if (node != NULL && node->parent != NULL) {
        ctx = (task_cli_ctx_t *)node->parent->user_data;
        if (ctx != NULL) {
            ctx->gr.data_name_help = 1;
        }
    }
    return 0;
}

static int
_sf_parse_gr_meta_name_ctx_help(tpbcli_argnode_t *node, const char *value)
{
    task_cli_ctx_t *ctx;

    (void)value;
    if (node != NULL && node->parent != NULL) {
        ctx = (task_cli_ctx_t *)node->parent->user_data;
        if (ctx != NULL) {
            ctx->gr.meta_name_help = 1;
        }
    }
    return 0;
}

static int
_sf_parse_gr_show_subrank(tpbcli_argnode_t *node, const char *value)
{
    task_cli_ctx_t *ctx = (task_cli_ctx_t *)node->user_data;

    (void)value;
    if (ctx != NULL) {
        ctx->gr.show_each_subrank = 1;
    }
    return 0;
}

static void
_sf_emit_gr_help(const tpbcli_argnode_t *node, FILE *out)
{
    (void)node;
    (void)out;
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "Usage: tpbcli task get-result|gr "
                    "(-r|--rid LIST | -i|--task-id HASH_LIST) "
                    "[--data-name NAME_LIST]... "
                    "[--meta-name META_LIST]... "
                    "[--show-each-subrank]\n");
}

static int
_sf_add_gr_options(tpbcli_argnode_t *parent, void *ctx)
{
    tpbcli_argnode_t *data_name;
    tpbcli_argnode_t *meta_name;

    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "--rid",
            .short_name = "-r",
            .desc = "RIDMAP index list",
            .type = TPBCLI_ARG_OPT,
            .max_chosen = 1,
            .parse_fn = _sf_parse_gr_rid,
            .conflict_opts = _sf_gr_conf_not_rid,
            .user_data = ctx,
        }) == NULL) {
        return -1;
    }
    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "--task-id",
            .short_name = "-i",
            .desc = "TaskRecordID hex prefix list",
            .type = TPBCLI_ARG_OPT,
            .max_chosen = 1,
            .parse_fn = _sf_parse_gr_task_id,
            .conflict_opts = _sf_gr_conf_not_taskid,
            .user_data = ctx,
        }) == NULL) {
        return -1;
    }
    data_name = tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "--data-name",
            .short_name = NULL,
            .desc = "Output metric name CSV list",
            .type = TPBCLI_ARG_OPT,
            .max_chosen = TPBCLI_TASK_GR_NAME_MAX,
            .parse_fn = _sf_parse_gr_data_name,
            .user_data = ctx,
        });
    if (data_name == NULL) {
        return -1;
    }
    if (tpbcli_add_arg(data_name, &(tpbcli_argconf_t){
            .name = "--help",
            .short_name = "-h",
            .desc = "List shared/private data names for selection",
            .type = TPBCLI_ARG_FLAG,
            .max_chosen = 0,
            .parse_fn = _sf_parse_gr_data_name_ctx_help,
            .user_data = ctx,
        }) == NULL) {
        return -1;
    }
    meta_name = tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "--meta-name",
            .short_name = NULL,
            .desc = "Meta field name CSV list",
            .type = TPBCLI_ARG_OPT,
            .max_chosen = TPBCLI_TASK_GR_NAME_MAX,
            .parse_fn = _sf_parse_gr_meta_name,
            .user_data = ctx,
        });
    if (meta_name == NULL) {
        return -1;
    }
    if (tpbcli_add_arg(meta_name, &(tpbcli_argconf_t){
            .name = "--help",
            .short_name = "-h",
            .desc = "List shared/private meta and data names",
            .type = TPBCLI_ARG_FLAG,
            .max_chosen = 0,
            .parse_fn = _sf_parse_gr_meta_name_ctx_help,
            .user_data = ctx,
        }) == NULL) {
        return -1;
    }
    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "--show-each-subrank",
            .short_name = NULL,
            .desc = "Show each capsule member separately",
            .type = TPBCLI_ARG_FLAG,
            .max_chosen = 1,
            .parse_fn = _sf_parse_gr_show_subrank,
            .user_data = ctx,
        }) == NULL) {
        return -1;
    }
    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "--help",
            .short_name = "-h",
            .desc = "Show get-result help",
            .type = TPBCLI_ARG_FLAG,
            .max_chosen = 0,
            .help_fn = _sf_emit_gr_help,
        }) == NULL) {
        return -1;
    }
    return 0;
}

static void
_sf_emit_export_help(const tpbcli_argnode_t *node, FILE *out)
{
    (void)node;
    (void)out;
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "Usage: tpbcli task export "
                    "(-r|--rid LIST | -i|--id LIST | --from-ls) "
                    "[--trace-to-entry | --keep-current] "
                    "[-f|--filter subrank=...|subtid=...]... "
                    "[-o|--outdir DIR]\n");
}

static int
_sf_parse_export_cmd(tpbcli_argnode_t *node, const char *value)
{
    task_cli_ctx_t *ctx = (task_cli_ctx_t *)node->user_data;

    (void)value;
    if (ctx != NULL) {
        ctx->want_export = 1;
    }
    return 0;
}

static int
_sf_parse_export_rid(tpbcli_argnode_t *node, const char *value)
{
    task_cli_ctx_t *ctx = (task_cli_ctx_t *)node->user_data;

    if (ctx == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    ctx->export.sel_mode = TPBCLI_TASK_EXP_SEL_RID;
    ctx->export.rid_spec = value;
    return 0;
}

static int
_sf_parse_export_task_id(tpbcli_argnode_t *node, const char *value)
{
    task_cli_ctx_t *ctx = (task_cli_ctx_t *)node->user_data;

    if (ctx == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    ctx->export.sel_mode = TPBCLI_TASK_EXP_SEL_TASK_ID;
    ctx->export.task_id_spec = value;
    return 0;
}

static int
_sf_parse_export_from_ls(tpbcli_argnode_t *node, const char *value)
{
    task_cli_ctx_t *ctx = (task_cli_ctx_t *)node->user_data;

    (void)value;
    if (ctx == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    ctx->export.sel_mode = TPBCLI_TASK_EXP_SEL_FROM_LS;
    return 0;
}

static int
_sf_parse_export_trace_entry(tpbcli_argnode_t *node, const char *value)
{
    task_cli_ctx_t *ctx = (task_cli_ctx_t *)node->user_data;

    (void)value;
    if (ctx != NULL) {
        ctx->export.trace = TPBCLI_TASK_EXP_TRACE_TO_ENTRY;
    }
    return 0;
}

static int
_sf_parse_export_keep(tpbcli_argnode_t *node, const char *value)
{
    task_cli_ctx_t *ctx = (task_cli_ctx_t *)node->user_data;

    (void)value;
    if (ctx != NULL) {
        ctx->export.trace = TPBCLI_TASK_EXP_TRACE_KEEP_CURRENT;
    }
    return 0;
}

static int
_sf_parse_export_filter(tpbcli_argnode_t *node, const char *value)
{
    task_cli_ctx_t *ctx = (task_cli_ctx_t *)node->user_data;
    tpbcli_task_export_filter_t f;
    int err;

    if (ctx == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    if (ctx->export.nfilter >= TPBCLI_TASK_FILTER_MAX) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    err = tpbcli_task_export_parse_filter(value, &f);
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);
    ctx->export.filters[ctx->export.nfilter++] = f;
    return 0;
}

static int
_sf_parse_export_outdir(tpbcli_argnode_t *node, const char *value)
{
    task_cli_ctx_t *ctx = (task_cli_ctx_t *)node->user_data;

    if (ctx == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    ctx->export.outdir = value;
    return 0;
}

static int
_sf_add_export_options(tpbcli_argnode_t *parent, void *ctx)
{
    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "--rid",
            .short_name = "-r",
            .desc = "RIDMAP index list",
            .type = TPBCLI_ARG_OPT,
            .max_chosen = 1,
            .parse_fn = _sf_parse_export_rid,
            .conflict_opts = _sf_exp_conf_not_rid,
            .user_data = ctx,
        }) == NULL) {
        return -1;
    }
    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "--id",
            .short_name = "-i",
            .desc = "TaskRecordID hex prefix list",
            .type = TPBCLI_ARG_OPT,
            .max_chosen = 1,
            .parse_fn = _sf_parse_export_task_id,
            .conflict_opts = _sf_exp_conf_not_id,
            .user_data = ctx,
        }) == NULL) {
        return -1;
    }
    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "--from-ls",
            .short_name = NULL,
            .desc = "Export all IDs from RIDMAP",
            .type = TPBCLI_ARG_FLAG,
            .max_chosen = 1,
            .parse_fn = _sf_parse_export_from_ls,
            .conflict_opts = _sf_exp_conf_not_from_ls,
            .user_data = ctx,
        }) == NULL) {
        return -1;
    }
    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "--trace-to-entry",
            .short_name = NULL,
            .desc = "Follow derive_to to entry root",
            .type = TPBCLI_ARG_FLAG,
            .max_chosen = 1,
            .parse_fn = _sf_parse_export_trace_entry,
            .conflict_opts = _sf_conf_not_trace,
            .user_data = ctx,
        }) == NULL) {
        return -1;
    }
    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "--keep-current",
            .short_name = NULL,
            .desc = "Export selected record only",
            .type = TPBCLI_ARG_FLAG,
            .max_chosen = 1,
            .parse_fn = _sf_parse_export_keep,
            .conflict_opts = _sf_conf_not_keep,
            .user_data = ctx,
        }) == NULL) {
        return -1;
    }
    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "--filter",
            .short_name = "-f",
            .desc = "Member filter subrank= or subtid=",
            .type = TPBCLI_ARG_OPT,
            .max_chosen = TPBCLI_TASK_FILTER_MAX,
            .parse_fn = _sf_parse_export_filter,
            .user_data = ctx,
        }) == NULL) {
        return -1;
    }
    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "--outdir",
            .short_name = "-o",
            .desc = "Output root directory",
            .type = TPBCLI_ARG_OPT,
            .max_chosen = 1,
            .parse_fn = _sf_parse_export_outdir,
            .user_data = ctx,
        }) == NULL) {
        return -1;
    }
    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "--help",
            .short_name = "-h",
            .desc = "Show export help",
            .type = TPBCLI_ARG_FLAG,
            .max_chosen = 0,
            .help_fn = _sf_emit_export_help,
        }) == NULL) {
        return -1;
    }
    return 0;
}

static tpbcli_argtree_t *
_sf_build_task_argtree(void *ctx)
{
    tpbcli_argtree_t *tree;
    tpbcli_argnode_t *root;
    tpbcli_argnode_t *task_cmd;
    tpbcli_argnode_t *ls;
    tpbcli_argnode_t *list;
    tpbcli_argnode_t *gr;
    tpbcli_argnode_t *get_result;
    tpbcli_argnode_t *export_cmd;

    /* Rebuild full argv tree like database: argv[1] is still "task". */
    tree = tpbcli_argtree_create("tpbcli", "TPBench command-line interface");
    if (tree == NULL) {
        return NULL;
    }
    root = &tree->root;

    task_cmd = tpbcli_add_arg(root, &(tpbcli_argconf_t){
            .name = "task",
            .short_name = NULL,
            .desc = "Task listing, result comparison, and export",
            .type = TPBCLI_ARG_CMD,
            .flags = TPBCLI_ARGF_EXCLUSIVE,
            .max_chosen = 1,
            .help_fn = _sf_emit_task_help,
        });
    if (task_cmd == NULL) {
        tpbcli_argtree_destroy(tree);
        return NULL;
    }

    if (tpbcli_add_arg(task_cmd, &(tpbcli_argconf_t){
            .name = "--help",
            .short_name = "-h",
            .desc = "Show task help",
            .type = TPBCLI_ARG_FLAG,
            .max_chosen = 0,
            .help_fn = _sf_emit_task_help,
        }) == NULL) {
        tpbcli_argtree_destroy(tree);
        return NULL;
    }

    ls = tpbcli_add_arg(task_cmd, &(tpbcli_argconf_t){
            .name = "ls",
            .short_name = NULL,
            .desc = "List task entry points",
            .type = TPBCLI_ARG_CMD,
            .flags = TPBCLI_ARGF_EXCLUSIVE,
            .max_chosen = 1,
            .parse_fn = _sf_parse_ls_cmd,
            .help_fn = _sf_emit_ls_help,
            .user_data = ctx,
        });
    list = tpbcli_add_arg(task_cmd, &(tpbcli_argconf_t){
            .name = "list",
            .short_name = NULL,
            .desc = "Alias for ls",
            .type = TPBCLI_ARG_CMD,
            .flags = TPBCLI_ARGF_EXCLUSIVE,
            .max_chosen = 1,
            .parse_fn = _sf_parse_ls_cmd,
            .help_fn = _sf_emit_ls_help,
            .user_data = ctx,
        });
    if (ls == NULL || list == NULL) {
        tpbcli_argtree_destroy(tree);
        return NULL;
    }
    if (_sf_add_ls_options(ls, ctx) != 0 ||
        _sf_add_ls_options(list, ctx) != 0) {
        tpbcli_argtree_destroy(tree);
        return NULL;
    }

    get_result = tpbcli_add_arg(task_cmd, &(tpbcli_argconf_t){
            .name = "get-result",
            .short_name = NULL,
            .desc = "Compare task meta and metrics",
            .type = TPBCLI_ARG_CMD,
            .flags = TPBCLI_ARGF_EXCLUSIVE,
            .max_chosen = 1,
            .parse_fn = _sf_parse_gr_cmd,
            .help_fn = _sf_emit_gr_help,
            .user_data = ctx,
        });
    gr = tpbcli_add_arg(task_cmd, &(tpbcli_argconf_t){
            .name = "gr",
            .short_name = NULL,
            .desc = "Alias for get-result",
            .type = TPBCLI_ARG_CMD,
            .flags = TPBCLI_ARGF_EXCLUSIVE,
            .max_chosen = 1,
            .parse_fn = _sf_parse_gr_cmd,
            .help_fn = _sf_emit_gr_help,
            .user_data = ctx,
        });
    if (get_result == NULL || gr == NULL) {
        tpbcli_argtree_destroy(tree);
        return NULL;
    }
    if (_sf_add_gr_options(get_result, ctx) != 0 ||
        _sf_add_gr_options(gr, ctx) != 0) {
        tpbcli_argtree_destroy(tree);
        return NULL;
    }

    export_cmd = tpbcli_add_arg(task_cmd, &(tpbcli_argconf_t){
            .name = "export",
            .short_name = NULL,
            .desc = "Export task records as CSV files",
            .type = TPBCLI_ARG_CMD,
            .flags = TPBCLI_ARGF_EXCLUSIVE,
            .max_chosen = 1,
            .parse_fn = _sf_parse_export_cmd,
            .help_fn = _sf_emit_export_help,
            .user_data = ctx,
        });
    if (export_cmd == NULL) {
        tpbcli_argtree_destroy(tree);
        return NULL;
    }
    if (_sf_add_export_options(export_cmd, ctx) != 0) {
        tpbcli_argtree_destroy(tree);
        return NULL;
    }
    return tree;
}

/**
 * @brief Dispatch `tpbcli task` after argv[0..1].
 */
int
tpbcli_task(int argc, char **argv)
{
    task_cli_ctx_t cli_ctx;
    tpbcli_argtree_t *tree;
    char workspace[PATH_MAX];
    int err;

    memset(&cli_ctx, 0, sizeof(cli_ctx));
    cli_ctx.ls.count = 0; /* unlimited by default */
    cli_ctx.ls.from_oldest = 0;

    tree = _sf_build_task_argtree(&cli_ctx);
    if (tree == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
    }

    err = tpbcli_parse_args(tree, argc, argv);
    tpbcli_argtree_destroy(tree);
    if (err == TPBE_EXIT_ON_HELP) {
        return err;
    }
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    if (!cli_ctx.want_ls && !cli_ctx.want_gr && !cli_ctx.want_export) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                        "error: missing required subcommand: ls|list|get-result|gr|"
                        "export\n");
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }

    err = tpb_raf_resolve_workspace(workspace, sizeof(workspace));
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, "tpb_raf_resolve_workspace");
    }
    if (cli_ctx.want_ls) {
        return tpbcli_task_ls(workspace, &cli_ctx.ls);
    }
    if (cli_ctx.want_export) {
        if (cli_ctx.export.sel_mode == TPBCLI_TASK_EXP_SEL_NONE) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO,
                            TPBLOG_FLAG_DIRECT,
                            "error: export requires -r|--rid, -i|--id, or "
                            "--from-ls\n");
            TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
        }
        err = tpbcli_task_export(workspace, &cli_ctx.export);
        tpbcli_task_export_free_filters(&cli_ctx.export);
        return err;
    }
    if (!cli_ctx.want_gr) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    if (cli_ctx.gr.rid_spec == NULL && cli_ctx.gr.task_id_spec == NULL) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: get-result requires -r|--rid or -i|--task-id\n");
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    if (cli_ctx.gr.rid_spec != NULL && cli_ctx.gr.task_id_spec != NULL) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "error: -r|--rid and -i|--task-id are mutually exclusive\n");
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    if (cli_ctx.gr.ndata_names == 0 && cli_ctx.gr.nmeta_names == 0 &&
        !cli_ctx.gr.data_name_given && !cli_ctx.gr.data_name_help &&
        !cli_ctx.gr.meta_name_help) {
        /* default columns */
    } else if ((cli_ctx.gr.ndata_names == 0 && cli_ctx.gr.nmeta_names > 0 &&
                !cli_ctx.gr.meta_name_help) ||
               (cli_ctx.gr.data_name_given && cli_ctx.gr.ndata_names == 0 &&
                cli_ctx.gr.nmeta_names == 0 && !cli_ctx.gr.data_name_help) ||
               (cli_ctx.gr.ndata_names > 0 && cli_ctx.gr.nmeta_names == 0 &&
                !cli_ctx.gr.data_name_help) ||
               (cli_ctx.gr.ndata_names > 0 && cli_ctx.gr.nmeta_names > 0) ||
               (cli_ctx.gr.data_name_given && cli_ctx.gr.ndata_names == 0 &&
                cli_ctx.gr.nmeta_names > 0)) {
        /* valid explicit combinations, including --data-name '' */
    } else if (cli_ctx.gr.data_name_help || cli_ctx.gr.meta_name_help) {
        /* context help */
    }
    return tpbcli_task_get_result(workspace, &cli_ctx.gr);
}
