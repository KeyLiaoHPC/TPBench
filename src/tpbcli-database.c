/*
 * tpbcli-database.c
 * `tpbcli database` subcommand — argp tree, list/ls vs dump.
 */

#include <stdio.h>
#include <string.h>

#include "corelib/rafdb/tpb-raf-types.h"
#include "tpbcli-argp.h"
#include "tpbcli-database.h"

/* Local Function Prototypes */

static tpbcli_argtree_t *_sf_build_database_argtree(void *ctx);
static void _sf_emit_database_help(const tpbcli_argnode_t *node, FILE *out);
static int _sf_parse_database_dump_cmd(tpbcli_argnode_t *node,
                                        const char *value);
static int _sf_parse_database_dump_opt(tpbcli_argnode_t *node,
                                       const char *value);
static int _sf_parse_database_list_cmd(tpbcli_argnode_t *node,
                                       const char *value);

typedef struct database_cli_ctx {
    int          want_list;
    int          want_dump;
    const char  *dump_selector;
    char         primary_buf[TPB_RAF_PATH_MAX];
} database_cli_ctx_t;

static const char *_sf_conf_not_id[] = {
    "--tbatch-id", "--kernel-id", "--task-id", "--score-id",
    "--file", "--entry", NULL
};
static const char *_sf_conf_not_tbatch[] = {
    "--id", "--kernel-id", "--task-id", "--score-id",
    "--file", "--entry", NULL
};
static const char *_sf_conf_not_kernel[] = {
    "--id", "--tbatch-id", "--task-id", "--score-id",
    "--file", "--entry", NULL
};
static const char *_sf_conf_not_task[] = {
    "--id", "--tbatch-id", "--kernel-id", "--score-id",
    "--file", "--entry", NULL
};
static const char *_sf_conf_not_score[] = {
    "--id", "--tbatch-id", "--kernel-id", "--task-id",
    "--file", "--entry", NULL
};
static const char *_sf_conf_not_file[] = {
    "--id", "--tbatch-id", "--kernel-id", "--task-id",
    "--score-id", "--entry", NULL
};
static const char *_sf_conf_not_entry[] = {
    "--id", "--tbatch-id", "--kernel-id", "--task-id",
    "--score-id", "--file", NULL
};

static tpbcli_argtree_t *
_sf_build_database_argtree(void *ctx_void)
{
    database_cli_ctx_t *ctx = (database_cli_ctx_t *)ctx_void;
    tpbcli_argtree_t *tree;
    tpbcli_argnode_t *root;
    tpbcli_argnode_t *db_cmd;
    tpbcli_argnode_t *list_cmd;
    tpbcli_argnode_t *dump_cmd;

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

    db_cmd = tpbcli_add_arg(root, &(tpbcli_argconf_t){
        .name = "database",
        .short_name = "db",
        .desc = "Database operations for TPBench rafdb results",
        .type = TPBCLI_ARG_CMD,
        .flags = TPBCLI_ARGF_EXCLUSIVE,
        .max_chosen = 1,
    });
    if (db_cmd == NULL) {
        tpbcli_argtree_destroy(tree);
        return NULL;
    }

    if (tpbcli_add_arg(db_cmd, &(tpbcli_argconf_t){
            .name = "--help",
            .short_name = "-h",
            .desc = "Show help for database subcommand",
            .type = TPBCLI_ARG_FLAG,
            .max_chosen = 0,
            .help_fn = _sf_emit_database_help,
        }) == NULL) {
        tpbcli_argtree_destroy(tree);
        return NULL;
    }

    list_cmd = tpbcli_add_arg(db_cmd, &(tpbcli_argconf_t){
        .name = "list",
        .short_name = "ls",
        .desc = "List tbatch records in the workspace index",
        .type = TPBCLI_ARG_CMD,
        .flags = TPBCLI_ARGF_EXCLUSIVE,
        .max_chosen = 1,
        .parse_fn = _sf_parse_database_list_cmd,
        .user_data = ctx,
    });
    if (list_cmd == NULL) {
        tpbcli_argtree_destroy(tree);
        return NULL;
    }

    if (tpbcli_add_arg(list_cmd, &(tpbcli_argconf_t){
            .name = "--help",
            .short_name = "-h",
            .desc = "Show help for list",
            .type = TPBCLI_ARG_FLAG,
            .max_chosen = 0,
            .help_fn = tpbcli_default_help,
        }) == NULL) {
        tpbcli_argtree_destroy(tree);
        return NULL;
    }

    dump_cmd = tpbcli_add_arg(db_cmd, &(tpbcli_argconf_t){
        .name = "dump",
        .desc = "Dump one .tpbr/.tpbe record or domain as CSV-style lines",
        .type = TPBCLI_ARG_CMD,
        .flags = TPBCLI_ARGF_EXCLUSIVE,
        .max_chosen = 1,
        .parse_fn = _sf_parse_database_dump_cmd,
        .user_data = ctx,
    });
    if (dump_cmd == NULL) {
        tpbcli_argtree_destroy(tree);
        return NULL;
    }

    if (tpbcli_add_arg(dump_cmd, &(tpbcli_argconf_t){
            .name = "--help",
            .short_name = "-h",
            .desc = "Show dump options",
            .type = TPBCLI_ARG_FLAG,
            .max_chosen = 0,
            .help_fn = tpbcli_default_help,
        }) == NULL) {
        tpbcli_argtree_destroy(tree);
        return NULL;
    }

#define ADD_DUMP_OPT(_nm, _desc, _conf)                                        \
    do {                                                                     \
        if (tpbcli_add_arg(dump_cmd, &(tpbcli_argconf_t){                     \
                .name = (_nm),                                               \
                .desc = (_desc),                                             \
                .type = TPBCLI_ARG_OPT,                                      \
                .max_chosen = 1,                                             \
                .conflict_opts = (_conf),                                    \
                .parse_fn = _sf_parse_database_dump_opt,                       \
                .user_data = ctx,                                            \
            }) == NULL) {                                                    \
            tpbcli_argtree_destroy(tree);                                    \
            return NULL;                                                     \
        }                                                                    \
    } while (0)

    ADD_DUMP_OPT("--id", "Global id search (SHA-1 hex, 4-40 digits)",
                 _sf_conf_not_id);
    ADD_DUMP_OPT("--tbatch-id", "Tbatch record id (SHA-1 hex)",
                 _sf_conf_not_tbatch);
    ADD_DUMP_OPT("--kernel-id", "Kernel record id (SHA-1 hex)",
                 _sf_conf_not_kernel);
    ADD_DUMP_OPT("--task-id", "Task record id (SHA-1 hex)",
                 _sf_conf_not_task);
    ADD_DUMP_OPT("--score-id", "Score record id (not implemented in rafdb)",
                 _sf_conf_not_score);
    ADD_DUMP_OPT("--file", "Path to a .tpbr/.tpbe file (full path or basename)",
                 _sf_conf_not_file);
    ADD_DUMP_OPT("--entry", "Dump domain index: task_batch, kernel, or task",
                 _sf_conf_not_entry);
#undef ADD_DUMP_OPT

    return tree;
}

static void
_sf_emit_database_help(const tpbcli_argnode_t *node, FILE *out)
{
    (void)node;
    fprintf(out, "Usage: tpbcli database|db <list|ls|dump> ...\n\n");
    fprintf(out, "Database operations for TPBench rafdb results.\n\n");
    fprintf(out, "Commands:\n");
    fprintf(out, "  list, ls    List tbatch records in the workspace index.\n");
    fprintf(out, "  dump        Dump one record or domain as CSV-style lines.\n");
    fprintf(out, "              Brief selectors: --id, --tbatch-id, "
                   "--kernel-id,\n");
    fprintf(out, "              --task-id, --score-id, --file <path>, "
                   "--entry <name>.\n");
    fprintf(out, "              Use \"tpbcli database dump --help\" for full option help.\n\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  --help, -h    Show this help.\n\n");
    fprintf(out, "Use \"tpbcli database <list|ls|dump> --help\" for more information.\n");
}

static int
_sf_parse_database_dump_cmd(tpbcli_argnode_t *node, const char *value)
{
    database_cli_ctx_t *ctx = (database_cli_ctx_t *)node->user_data;

    (void)value;
    if (ctx != NULL) {
        ctx->want_dump = 1;
    }
    return 0;
}

static int
_sf_parse_database_dump_opt(tpbcli_argnode_t *node, const char *value)
{
    database_cli_ctx_t *ctx = (database_cli_ctx_t *)node->user_data;

    if (ctx == NULL || node->name == NULL || value == NULL) {
        return TPBE_CLI_FAIL;
    }
    ctx->dump_selector = node->name;
    snprintf(ctx->primary_buf, sizeof(ctx->primary_buf), "%s", value);
    return 0;
}

static int
_sf_parse_database_list_cmd(tpbcli_argnode_t *node, const char *value)
{
    database_cli_ctx_t *ctx = (database_cli_ctx_t *)node->user_data;

    (void)value;
    if (ctx != NULL) {
        ctx->want_list = 1;
    }
    return 0;
}

/**
 * @brief Dispatches `tpbcli database` after argv[0..1] (`tpbcli`, `database`).
 */
int
tpbcli_database(int argc, char **argv)
{
    database_cli_ctx_t cli_ctx;
    tpbcli_argtree_t *tree;
    char workspace[TPB_RAF_PATH_MAX];
    int err;

    memset(&cli_ctx, 0, sizeof(cli_ctx));

    tree = _sf_build_database_argtree(&cli_ctx);
    if (tree == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "Failed to allocate database argument tree.\n");
        return TPBE_MALLOC_FAIL;
    }

    err = tpbcli_parse_args(tree, argc, argv);
    tpbcli_argtree_destroy(tree);

    if (err == TPBE_EXIT_ON_HELP) {
        return err;
    }
    if (err != TPBE_SUCCESS) {
        return err;
    }

    if (!cli_ctx.want_list && !cli_ctx.want_dump) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "error: missing required subcommand: list|ls or dump\n");
        return TPBE_CLI_FAIL;
    }

    err = tpb_raf_resolve_workspace(workspace, sizeof(workspace));
    if (err != TPBE_SUCCESS) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "Failed to resolve workspace.\n");
        return err;
    }

    if (cli_ctx.want_list) {
        return tpbcli_database_ls(workspace);
    }

    return tpbcli_database_dump_resolved(workspace, cli_ctx.dump_selector,
                                         cli_ctx.dump_selector != NULL
                                             ? cli_ctx.primary_buf
                                             : NULL,
                                         NULL);
}
