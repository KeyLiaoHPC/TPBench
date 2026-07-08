/*
 * tpbcli-database.c
 * `tpbcli database` subcommand — argp tree, list/ls vs dump.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef __linux__
#include <linux/limits.h>
#else
#include <limits.h>
#endif

#include "tpb-public.h"
#include "tpbcli-argp.h"
#include "tpbcli-database.h"

#define TPBCLI_DB_LIST_DEFAULT_COUNT 20

/* Local Function Prototypes */

static tpbcli_argtree_t *_sf_build_database_argtree(void *ctx);
static void _sf_emit_database_help(const tpbcli_argnode_t *node, FILE *out);
static int _sf_parse_database_dump_cmd(tpbcli_argnode_t *node,
                                        const char *value);
static int _sf_parse_database_dump_opt(tpbcli_argnode_t *node,
                                       const char *value);
static int _sf_parse_database_list_cmd(tpbcli_argnode_t *node,
                                       const char *value);
static int _sf_parse_list_domain_flag(tpbcli_argnode_t *node,
                                      const char *value);
static int _sf_parse_list_domain_opt(tpbcli_argnode_t *node,
                                     const char *value);
static int _sf_parse_list_count_newest(tpbcli_argnode_t *node,
                                       const char *value);
static int _sf_parse_list_count_oldest(tpbcli_argnode_t *node,
                                       const char *value);
static int _sf_parse_positive_int(const char *value, int *out);
static int _sf_parse_domain_name(const char *value, uint8_t *domain_out);

typedef struct database_cli_ctx {
    int          want_list;
    int          want_dump;
    const char  *dump_selector;
    char         primary_buf[PATH_MAX];
    uint8_t      list_domain;
    int          list_count;
    int          list_from_oldest;
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
static const char *_sf_conf_list_not_dT[] = {
    "-dt", "-dk", "-dr", "--domain", NULL
};
static const char *_sf_conf_list_not_dt[] = {
    "-dT", "-dk", "-dr", "--domain", NULL
};
static const char *_sf_conf_list_not_dk[] = {
    "-dT", "-dt", "-dr", "--domain", NULL
};
static const char *_sf_conf_list_not_dr[] = {
    "-dT", "-dt", "-dk", "--domain", NULL
};
static const char *_sf_conf_list_not_domain[] = {
    "-dT", "-dt", "-dk", "-dr", NULL
};
static const char *_sf_conf_list_not_n[] = {
    "-N", NULL
};
static const char *_sf_conf_list_not_N[] = {
    "-n", NULL
};

static int
_sf_parse_positive_int(const char *value, int *out)
{
    char *end;
    long v;

    if (value == NULL || value[0] == '\0' || out == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    v = strtol(value, &end, 10);
    if (end == value || *end != '\0' || v <= 0 || v > INT_MAX) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                   "error: invalid record count '%s' (positive integer required)\n",
                   value);
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    *out = (int)v;
    return 0;
}

static int
_sf_parse_domain_name(const char *value, uint8_t *domain_out)
{
    if (value == NULL || domain_out == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    if (strcasecmp(value, "tbatch") == 0 ||
        strcasecmp(value, "task_batch") == 0) {
        *domain_out = TPB_RAF_DOM_TBATCH;
        return 0;
    }
    if (strcasecmp(value, "task") == 0) {
        *domain_out = TPB_RAF_DOM_TASK;
        return 0;
    }
    if (strcasecmp(value, "kernel") == 0) {
        *domain_out = TPB_RAF_DOM_KERNEL;
        return 0;
    }
    if (strcasecmp(value, "runtime_environment") == 0 ||
        strcasecmp(value, "rtenv") == 0) {
        *domain_out = TPB_RAF_DOM_RTENV;
        return 0;
    }
    tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
               "error: unknown domain '%s' "
               "(use tbatch, task, kernel, or runtime_environment)\n",
               value);
    TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
}

static int
_sf_parse_list_domain_flag(tpbcli_argnode_t *node, const char *value)
{
    database_cli_ctx_t *ctx = (database_cli_ctx_t *)node->user_data;
    uint8_t domain;

    (void)value;
    if (ctx == NULL || node->name == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    if (strcmp(node->name, "-dT") == 0) {
        domain = TPB_RAF_DOM_TBATCH;
    } else if (strcmp(node->name, "-dt") == 0) {
        domain = TPB_RAF_DOM_TASK;
    } else if (strcmp(node->name, "-dk") == 0) {
        domain = TPB_RAF_DOM_KERNEL;
    } else if (strcmp(node->name, "-dr") == 0) {
        domain = TPB_RAF_DOM_RTENV;
    } else {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    ctx->list_domain = domain;
    return 0;
}

static int
_sf_parse_list_domain_opt(tpbcli_argnode_t *node, const char *value)
{
    database_cli_ctx_t *ctx = (database_cli_ctx_t *)node->user_data;
    uint8_t domain;
    int err;

    (void)node;
    if (ctx == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    err = _sf_parse_domain_name(value, &domain);
    if (err != 0) {
        return err;
    }
    ctx->list_domain = domain;
    return 0;
}

static int
_sf_parse_list_count_newest(tpbcli_argnode_t *node, const char *value)
{
    database_cli_ctx_t *ctx = (database_cli_ctx_t *)node->user_data;
    int err;

    (void)node;
    if (ctx == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    err = _sf_parse_positive_int(value, &ctx->list_count);
    if (err != 0) {
        return err;
    }
    ctx->list_from_oldest = 0;
    return 0;
}

static int
_sf_parse_list_count_oldest(tpbcli_argnode_t *node, const char *value)
{
    database_cli_ctx_t *ctx = (database_cli_ctx_t *)node->user_data;
    int err;

    (void)node;
    if (ctx == NULL) {
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }
    err = _sf_parse_positive_int(value, &ctx->list_count);
    if (err != 0) {
        return err;
    }
    ctx->list_from_oldest = 1;
    return 0;
}

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
        .desc = "List rafdb index records (tbatch, task, kernel, or runtime_environment)",
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

#define ADD_LIST_FLAG(_nm, _desc, _conf)                                       \
    do {                                                                     \
        if (tpbcli_add_arg(list_cmd, &(tpbcli_argconf_t){                     \
                .name = (_nm),                                               \
                .desc = (_desc),                                             \
                .type = TPBCLI_ARG_FLAG,                                     \
                .max_chosen = 1,                                             \
                .conflict_opts = (_conf),                                    \
                .parse_fn = _sf_parse_list_domain_flag,                      \
                .user_data = ctx,                                            \
            }) == NULL) {                                                    \
            tpbcli_argtree_destroy(tree);                                    \
            return NULL;                                                     \
        }                                                                    \
    } while (0)

#define ADD_LIST_OPT(_nm, _desc, _conf, _fn)                                   \
    do {                                                                     \
        if (tpbcli_add_arg(list_cmd, &(tpbcli_argconf_t){                     \
                .name = (_nm),                                               \
                .desc = (_desc),                                             \
                .type = TPBCLI_ARG_OPT,                                      \
                .max_chosen = 1,                                             \
                .conflict_opts = (_conf),                                    \
                .parse_fn = (_fn),                                           \
                .user_data = ctx,                                            \
            }) == NULL) {                                                    \
            tpbcli_argtree_destroy(tree);                                    \
            return NULL;                                                     \
        }                                                                    \
    } while (0)

    ADD_LIST_FLAG("-dT", "List tbatch domain (default)", _sf_conf_list_not_dT);
    ADD_LIST_FLAG("-dt", "List task domain entry points", _sf_conf_list_not_dt);
    ADD_LIST_FLAG("-dk", "List kernel domain", _sf_conf_list_not_dk);
    ADD_LIST_FLAG("-dr", "List runtime_environment domain", _sf_conf_list_not_dr);
    ADD_LIST_OPT("--domain",
                 "Domain name: tbatch, task, kernel, or runtime_environment",
                 _sf_conf_list_not_domain, _sf_parse_list_domain_opt);
    ADD_LIST_OPT("-n", "Show latest N records", _sf_conf_list_not_n,
                 _sf_parse_list_count_newest);
    ADD_LIST_OPT("-N", "Show oldest N records", _sf_conf_list_not_N,
                 _sf_parse_list_count_oldest);
#undef ADD_LIST_FLAG
#undef ADD_LIST_OPT

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
    fprintf(out, "  list, ls    List rafdb index records (default: tbatch, latest 20).\n");
    fprintf(out, "              Options: -dT|-dt|-dk|-dr, --domain, -n, -N.\n");
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
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
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
    char workspace[PATH_MAX];
    int err;

    memset(&cli_ctx, 0, sizeof(cli_ctx));
    cli_ctx.list_domain = TPB_RAF_DOM_TBATCH;
    cli_ctx.list_count = TPBCLI_DB_LIST_DEFAULT_COUNT;
    cli_ctx.list_from_oldest = 0;

    tree = _sf_build_database_argtree(&cli_ctx);
    if (tree == NULL) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                   "Failed to allocate database argument tree.\n");
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_MALLOC_FAIL, NULL);
    }

    err = tpbcli_parse_args(tree, argc, argv);
    tpbcli_argtree_destroy(tree);

    if (err == TPBE_EXIT_ON_HELP) {
        return err;
    }
    TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, NULL);

    if (!cli_ctx.want_list && !cli_ctx.want_dump) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                   "error: missing required subcommand: list|ls or dump\n");
        TPB_FAIL(TPB_MOD_CLI_MISC, TPBE_CLI_FAIL, NULL);
    }

    err = tpb_raf_resolve_workspace(workspace, sizeof(workspace));
    if (err != TPBE_SUCCESS) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                   "Failed to resolve workspace.\n");
        TPB_PROPAGATE(TPB_MOD_CLI_MISC, err, "tpb_raf_resolve_workspace");
    }

    if (cli_ctx.want_list) {
        return tpbcli_database_ls(workspace, cli_ctx.list_domain,
                                  cli_ctx.list_count,
                                  cli_ctx.list_from_oldest);
    }

    return tpbcli_database_dump_resolved(workspace, cli_ctx.dump_selector,
                                         cli_ctx.dump_selector != NULL
                                             ? cli_ctx.primary_buf
                                             : NULL,
                                         NULL);
}
