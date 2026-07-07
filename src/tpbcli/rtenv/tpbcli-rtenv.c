/*
 * tpbcli-rtenv.c
 * `tpbcli rtenv` — runtime environment management subcommands.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <linux/limits.h>
#else
#include <limits.h>
#endif

#include "tpb-public.h"
#include "tpbcli-argp.h"
#include "tpbcli-rtenv.h"
#include "tpbcli-rtenv-template.h"

/* Local Function Prototypes */

static tpbcli_argtree_t *_sf_build_rtenv_argtree(void *ctx);
static void _sf_emit_rtenv_help(const tpbcli_argnode_t *node, FILE *out);
static int _sf_parse_rtenv_new_cmd(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_rtenv_list_cmd(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_rtenv_show_cmd(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_rtenv_load_cmd(tpbcli_argnode_t *node, const char *value);
static int _sf_parse_rtenv_init_base_cmd(tpbcli_argnode_t *node, const char *value);

static int
_sf_add_rtenv_subcommands(tpbcli_argnode_t *parent, void *ctx)
{
    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "--help",
            .short_name = "-h",
            .desc = "Show rtenv help",
            .type = TPBCLI_ARG_FLAG,
            .max_chosen = 0,
            .help_fn = _sf_emit_rtenv_help,
        }) == NULL) {
        return -1;
    }
    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "new",
            .desc = "Create a runtime environment",
            .type = TPBCLI_ARG_CMD,
            .flags = TPBCLI_ARGF_EXCLUSIVE | TPBCLI_ARGF_DELEGATE_SUBCMD,
            .max_chosen = 1,
            .parse_fn = _sf_parse_rtenv_new_cmd,
            .user_data = ctx,
        }) == NULL) {
        return -1;
    }
    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "create",
            .desc = "Alias for new",
            .type = TPBCLI_ARG_CMD,
            .flags = TPBCLI_ARGF_EXCLUSIVE | TPBCLI_ARGF_DELEGATE_SUBCMD,
            .max_chosen = 1,
            .parse_fn = _sf_parse_rtenv_new_cmd,
            .user_data = ctx,
        }) == NULL) {
        return -1;
    }
    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "list",
            .short_name = "ls",
            .desc = "List runtime environments",
            .type = TPBCLI_ARG_CMD,
            .flags = TPBCLI_ARGF_EXCLUSIVE,
            .max_chosen = 1,
            .parse_fn = _sf_parse_rtenv_list_cmd,
            .user_data = ctx,
        }) == NULL) {
        return -1;
    }
    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "show",
            .desc = "Show merged runtime environment",
            .type = TPBCLI_ARG_CMD,
            .flags = TPBCLI_ARGF_EXCLUSIVE | TPBCLI_ARGF_DELEGATE_SUBCMD,
            .max_chosen = 1,
            .parse_fn = _sf_parse_rtenv_show_cmd,
            .user_data = ctx,
        }) == NULL) {
        return -1;
    }
    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "load",
            .desc = "Output shell fragment to load environment",
            .type = TPBCLI_ARG_CMD,
            .flags = TPBCLI_ARGF_EXCLUSIVE | TPBCLI_ARGF_DELEGATE_SUBCMD,
            .max_chosen = 1,
            .parse_fn = _sf_parse_rtenv_load_cmd,
            .user_data = ctx,
        }) == NULL) {
        return -1;
    }
    if (tpbcli_add_arg(parent, &(tpbcli_argconf_t){
            .name = "init-base",
            .desc = "Create base RTEnv id=0 when domain is empty",
            .type = TPBCLI_ARG_CMD,
            .flags = TPBCLI_ARGF_EXCLUSIVE,
            .max_chosen = 1,
            .parse_fn = _sf_parse_rtenv_init_base_cmd,
            .user_data = ctx,
        }) == NULL) {
        return -1;
    }
    return 0;
}

typedef struct rtenv_cli_ctx {
    int want_new;
    int want_list;
    int want_show;
    int want_load;
    int want_init_base;
} rtenv_cli_ctx_t;

static int
_sf_parse_rtenv_new_cmd(tpbcli_argnode_t *node, const char *value)
{
    rtenv_cli_ctx_t *ctx = (rtenv_cli_ctx_t *)node->user_data;

    (void)value;
    if (ctx != NULL) {
        ctx->want_new = 1;
    }
    return 0;
}

static int
_sf_parse_rtenv_list_cmd(tpbcli_argnode_t *node, const char *value)
{
    rtenv_cli_ctx_t *ctx = (rtenv_cli_ctx_t *)node->user_data;

    (void)value;
    if (ctx != NULL) {
        ctx->want_list = 1;
    }
    return 0;
}

static int
_sf_parse_rtenv_show_cmd(tpbcli_argnode_t *node, const char *value)
{
    rtenv_cli_ctx_t *ctx = (rtenv_cli_ctx_t *)node->user_data;

    (void)value;
    if (ctx != NULL) {
        ctx->want_show = 1;
    }
    return 0;
}

static int
_sf_parse_rtenv_load_cmd(tpbcli_argnode_t *node, const char *value)
{
    rtenv_cli_ctx_t *ctx = (rtenv_cli_ctx_t *)node->user_data;

    (void)value;
    if (ctx != NULL) {
        ctx->want_load = 1;
    }
    return 0;
}

static int
_sf_parse_rtenv_init_base_cmd(tpbcli_argnode_t *node, const char *value)
{
    rtenv_cli_ctx_t *ctx = (rtenv_cli_ctx_t *)node->user_data;

    (void)value;
    if (ctx != NULL) {
        ctx->want_init_base = 1;
    }
    return 0;
}

static tpbcli_argtree_t *
_sf_build_rtenv_argtree(void *ctx)
{
    tpbcli_argtree_t *tree;
    tpbcli_argnode_t *root;
    tpbcli_argnode_t *rtenv_cmd;
    tpbcli_argnode_t *alias_cmd;

    tree = tpbcli_argtree_create("tpbcli", "TPBench command-line interface");
    if (tree == NULL) {
        return NULL;
    }
    root = &tree->root;

    if (tpbcli_add_arg(root, &(tpbcli_argconf_t){
            .name = "--help",
            .short_name = "-h",
            .desc = "Show help for tpbcli",
            .type = TPBCLI_ARG_FLAG,
            .max_chosen = 0,
            .help_fn = tpbcli_default_help,
        }) == NULL) {
        tpbcli_argtree_destroy(tree);
        return NULL;
    }

    rtenv_cmd = tpbcli_add_arg(root, &(tpbcli_argconf_t){
            .name = "rtenv",
            .desc = "Runtime environment management",
            .type = TPBCLI_ARG_CMD,
            .flags = TPBCLI_ARGF_EXCLUSIVE,
            .max_chosen = 1,
        });
    if (rtenv_cmd == NULL) {
        tpbcli_argtree_destroy(tree);
        return NULL;
    }

    alias_cmd = tpbcli_add_arg(root, &(tpbcli_argconf_t){
            .name = "runtime-environment",
            .desc = "Alias for rtenv",
            .type = TPBCLI_ARG_CMD,
            .flags = TPBCLI_ARGF_EXCLUSIVE,
            .max_chosen = 1,
        });
    if (alias_cmd == NULL) {
        tpbcli_argtree_destroy(tree);
        return NULL;
    }

    if (_sf_add_rtenv_subcommands(rtenv_cmd, ctx) != 0 ||
        _sf_add_rtenv_subcommands(alias_cmd, ctx) != 0) {
        tpbcli_argtree_destroy(tree);
        return NULL;
    }
    return tree;
}

static void
_sf_emit_rtenv_help(const tpbcli_argnode_t *node, FILE *out)
{
    (void)node;
    fprintf(out, "Usage: tpbcli rtenv <new|create|list|ls|show|load> ...\n\n");
    fprintf(out, "Manage TPBench runtime environments (applications and shell variables).\n\n");
    fprintf(out, "Commands:\n");
    fprintf(out, "  new, create  Create a runtime environment (must inherit active RTEnv).\n");
    fprintf(out, "  list, ls     List RTEnv entries and show activated ID.\n");
    fprintf(out, "  show         Show merged attributes, applications, and variables.\n");
    fprintf(out, "  load         Output shell exports for eval/source.\n\n");
    fprintf(out, "Use \"tpbcli rtenv <command> --help\" for command-specific help.\n");
}

/**
 * @brief Execute the rtenv subcommand.
 */
int
tpbcli_rtenv(int argc, char **argv)
{
    rtenv_cli_ctx_t cli_ctx;
    tpbcli_argtree_t *tree;
    char workspace[PATH_MAX];
    int err;

    memset(&cli_ctx, 0, sizeof(cli_ctx));

    tree = _sf_build_rtenv_argtree(&cli_ctx);
    if (tree == NULL) {
        TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_MALLOC_FAIL, NULL);
    }

    err = tpbcli_parse_args(tree, argc, argv);
    tpbcli_argtree_destroy(tree);

    if (err == TPBE_EXIT_ON_HELP) {
        return err;
    }
    TPB_PROPAGATE(TPB_MOD_CLI_RTENV, err, NULL);

    if (!cli_ctx.want_new && !cli_ctx.want_list && !cli_ctx.want_show &&
        !cli_ctx.want_load && !cli_ctx.want_init_base) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "error: missing required subcommand: new|list|show|load\n");
        TPB_FAIL(TPB_MOD_CLI_RTENV, TPBE_CLI_FAIL, NULL);
    }

    err = tpb_raf_resolve_workspace(workspace, sizeof(workspace));
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_CLI_RTENV, err, "tpb_raf_resolve_workspace");
    }

    if (cli_ctx.want_init_base) {
        int32_t id;
        err = tpb_rtenv_ensure_base_env(workspace, &id);
        TPB_PROPAGATE(TPB_MOD_CLI_RTENV, err, "tpb_rtenv_ensure_base_env");
        return TPBE_SUCCESS;
    }
    if (cli_ctx.want_new) {
        return tpbcli_rtenv_new(argc, argv, workspace);
    }
    if (cli_ctx.want_list) {
        return tpbcli_rtenv_list(argc, argv, workspace);
    }
    if (cli_ctx.want_show) {
        return tpbcli_rtenv_show(argc, argv, workspace);
    }
    return tpbcli_rtenv_load(argc, argv, workspace);
}
