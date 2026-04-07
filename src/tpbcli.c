/*
 * tpbcli.c
 * TPBench CLI entry point and dispatcher.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <linux/limits.h>
#else
#include <limits.h>
#endif

#include "tpbcli-run.h"
#include "tpbcli-kernel.h"
#include "tpbcli-benchmark.h"
#include "tpbcli-help.h"
#include "tpbcli-database.h"
#include "tpbcli-argp.h"
#include "corelib/tpb-io.h"

/* Local Function Prototypes */
static int parse_global_cli_prefix(int argc, char **argv, const char **ws_out,
                                   int *first_action_index);

static int g_top_argc;
static char **g_top_argv;

static int _sf_dispatch_run(tpbcli_argnode_t *node, const char *value);
static int _sf_dispatch_benchmark(tpbcli_argnode_t *node, const char *value);
static int _sf_dispatch_database(tpbcli_argnode_t *node, const char *value);
static int _sf_dispatch_kernel(tpbcli_argnode_t *node, const char *value);
static int _sf_dispatch_help_cmd(tpbcli_argnode_t *node, const char *value);
static void _sf_top_help_flag(const tpbcli_argnode_t *node, FILE *out);
static int _sf_build_top_arg_tree(tpbcli_argtree_t *tree);

static int
parse_global_cli_prefix(int argc, char **argv, const char **ws_out,
                        int *first_action_index)
{
    int i = 1;

    *ws_out = NULL;
    *first_action_index = 1;

    while (i < argc) {
        if (strcmp(argv[i], "--workspace") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "tpbcli: --workspace requires a path\n");
                return TPBE_CLI_FAIL;
            }
            *ws_out = argv[i + 1];
            i += 2;
            continue;
        }
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
                break;
            }
            fprintf(stderr, "tpbcli: unknown option \"%s\"\n", argv[i]);
            fprintf(stderr, "Set \"--workspace\" or one of sub applications. \n"
                            "r[un], b[enchmark], d[atabase], k[ernel], h[elp].\n");
            return TPBE_CLI_FAIL;
        }
        break;
    }

    *first_action_index = i;
    return TPBE_SUCCESS;
}

static int
_sf_dispatch_run(tpbcli_argnode_t *node, const char *value)
{
    (void)node;
    (void)value;
    return tpbcli_run(g_top_argc, g_top_argv);
}

static int
_sf_dispatch_benchmark(tpbcli_argnode_t *node, const char *value)
{
    (void)node;
    (void)value;
    return tpbcli_benchmark(g_top_argc, g_top_argv);
}

static int
_sf_dispatch_database(tpbcli_argnode_t *node, const char *value)
{
    (void)node;
    (void)value;
    return tpbcli_database(g_top_argc, g_top_argv);
}

static int
_sf_dispatch_kernel(tpbcli_argnode_t *node, const char *value)
{
    (void)node;
    (void)value;
    return tpbcli_kernel(g_top_argc, g_top_argv);
}

static int
_sf_dispatch_help_cmd(tpbcli_argnode_t *node, const char *value)
{
    (void)node;
    (void)value;
    return tpbcli_help(g_top_argc, g_top_argv);
}

static void
_sf_top_help_flag(const tpbcli_argnode_t *node, FILE *out)
{
    (void)node;
    (void)out;
    tpb_print_help_total();
}

static int
_sf_build_top_arg_tree(tpbcli_argtree_t *tree)
{
    tpbcli_argnode_t *root;

    if (tree == NULL) {
        return TPBE_CLI_FAIL;
    }
    root = &tree->root;

    if (tpbcli_add_arg(root, &(tpbcli_argconf_t){
            .name = "run",
            .short_name = "r",
            .desc = "Run benchmark kernels",
            .type = TPBCLI_ARG_CMD,
            .flags = TPBCLI_ARGF_EXCLUSIVE | TPBCLI_ARGF_DELEGATE_SUBCMD,
            .max_chosen = 1,
            .parse_fn = _sf_dispatch_run,
        }) == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    if (tpbcli_add_arg(root, &(tpbcli_argconf_t){
            .name = "benchmark",
            .short_name = "b",
            .desc = "Run benchmark suite from YAML",
            .type = TPBCLI_ARG_CMD,
            .flags = TPBCLI_ARGF_EXCLUSIVE | TPBCLI_ARGF_DELEGATE_SUBCMD,
            .max_chosen = 1,
            .parse_fn = _sf_dispatch_benchmark,
        }) == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    if (tpbcli_add_arg(root, &(tpbcli_argconf_t){
            .name = "database",
            .short_name = "d",
            .desc = "Database operations (list, dump)",
            .type = TPBCLI_ARG_CMD,
            .flags = TPBCLI_ARGF_EXCLUSIVE | TPBCLI_ARGF_DELEGATE_SUBCMD,
            .max_chosen = 1,
            .parse_fn = _sf_dispatch_database,
        }) == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    if (tpbcli_add_arg(root, &(tpbcli_argconf_t){
            .name = "kernel",
            .short_name = "k",
            .desc = "Kernel management (list)",
            .type = TPBCLI_ARG_CMD,
            .flags = TPBCLI_ARGF_EXCLUSIVE | TPBCLI_ARGF_DELEGATE_SUBCMD,
            .max_chosen = 1,
            .parse_fn = _sf_dispatch_kernel,
        }) == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    if (tpbcli_add_arg(root, &(tpbcli_argconf_t){
            .name = "help",
            .short_name = "h",
            .desc = "Show help message",
            .type = TPBCLI_ARG_CMD,
            .flags = TPBCLI_ARGF_EXCLUSIVE | TPBCLI_ARGF_DELEGATE_SUBCMD,
            .max_chosen = 1,
            .parse_fn = _sf_dispatch_help_cmd,
        }) == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    if (tpbcli_add_arg(root, &(tpbcli_argconf_t){
            .name = "--help",
            .short_name = "-h",
            .desc = "Show help message",
            .type = TPBCLI_ARG_FLAG,
            .flags = TPBCLI_ARGF_EXCLUSIVE,
            .max_chosen = 0,
            .help_fn = _sf_top_help_flag,
        }) == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    return TPBE_SUCCESS;
}

/* Public Function Implementations */

int
tpbcli_main(int argc, char **argv)
{
    tpbcli_argtree_t *tree;
    int err;

    g_top_argc = argc;
    g_top_argv = argv;

    if (argc <= 1) {
        tpb_print_help_total();
        return TPBE_EXIT_ON_HELP;
    }

    tree = tpbcli_argtree_create(
        (argc > 0 && argv[0] != NULL) ? argv[0] : "tpbcli",
        "TPBench performance benchmarking CLI");
    if (tree == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    err = _sf_build_top_arg_tree(tree);
    if (err != TPBE_SUCCESS) {
        tpbcli_argtree_destroy(tree);
        return err;
    }
    err = tpbcli_parse_args(tree, argc, argv);
    tpbcli_argtree_destroy(tree);
    return err;
}

int
main(int argc, char **argv)
{
    const char *ws_opt;
    char ws_buf[PATH_MAX];
    const char *init_ws;
    int first_idx;
    int rc;
    int err;
    char **dispatch_argv;
    int dispatch_argc;
    int use_heap_argv;
    int k;

    err = parse_global_cli_prefix(argc, argv, &ws_opt, &first_idx);
    if (err != TPBE_SUCCESS) {
        return err;
    }

    init_ws = NULL;
    if (ws_opt != NULL) {
        if (snprintf(ws_buf, sizeof(ws_buf), "%s", ws_opt) >= (int)sizeof(ws_buf)) {
            fprintf(stderr, "tpbcli: workspace path too long\n");
            return TPBE_CLI_FAIL;
        }
        init_ws = ws_buf;
    }

    dispatch_argc = 1 + (argc - first_idx);
    use_heap_argv = (first_idx > 1);
    if (use_heap_argv) {
        dispatch_argv = (char **)malloc(sizeof(char *) * (size_t)(dispatch_argc + 1));
        if (dispatch_argv == NULL) {
            return TPBE_MALLOC_FAIL;
        }
        dispatch_argv[0] = argv[0];
        for (k = first_idx; k < argc; k++) {
            dispatch_argv[1 + (k - first_idx)] = argv[k];
        }
        dispatch_argv[dispatch_argc] = NULL;
    } else {
        dispatch_argv = argv;
    }

    rc = tpb_corelib_init(init_ws);
    if (rc != TPBE_SUCCESS) {
        if (use_heap_argv) {
            free(dispatch_argv);
        }
        return rc;
    }

    rc = tpbcli_main(dispatch_argc, dispatch_argv);

    if (use_heap_argv) {
        free(dispatch_argv);
    }

    tpb_log_cleanup();

    if (rc == TPBE_EXIT_ON_HELP) {
        return 0;
    }
    return rc;
}
