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
#include "tpbcli-list.h"
#include "tpbcli-benchmark.h"
#include "tpbcli-help.h"
#include "tpbcli-database.h"
#include "corelib/tpb-io.h"

/* Local Function Prototypes */
static int parse_global_cli_prefix(int argc, char **argv, const char **ws_out,
                                   int *first_action_index);

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
            fprintf(stderr, "tpbcli: unknown option \"%s\"\n", argv[i]);
            fprintf(stderr, "Set \"--workspace\" or one of sub applications. \n"
                            "r[un], b[enchmark], d[atabase], l[ist], h[elp].\n");
            return TPBE_CLI_FAIL;
        }
        break;
    }

    *first_action_index = i;
    return TPBE_SUCCESS;
}

/* Public Function Implementations */

int
tpbcli_main(int argc, char **argv)
{
    if (argc <= 1) {
        tpb_print_help_total();
        return TPBE_EXIT_ON_HELP;
    }

    if (strcmp(argv[1], "run") == 0 || strcmp(argv[1], "r") == 0) {
        return tpbcli_run(argc, argv);
    }
    if (strcmp(argv[1], "benchmark") == 0 || strcmp(argv[1], "b") == 0) {
        return tpbcli_benchmark(argc, argv);
    }
    if (strcmp(argv[1], "database") == 0 || strcmp(argv[1], "d") == 0) {
        return tpbcli_database(argc, argv);
    }
    if (strcmp(argv[1], "list") == 0 || strcmp(argv[1], "l") == 0) {
        return tpbcli_list(argc, argv);
    }
    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "h") == 0) {
        return tpbcli_help(argc, argv);
    }

    tpb_printf(TPBM_PRTN_M_DIRECT,
               "Unsupported action: %s. Please use one of sub applications:\n"
               "r[un], b[enchmark], d[atabase], l[ist], h[elp].\n", argv[1]);
    tpb_print_help_total();
    return TPBE_CLI_FAIL;
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
