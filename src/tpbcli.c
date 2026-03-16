/*
 * tpbcli.c
 * TPBench CLI entry point and dispatcher.
 */

#include <string.h>
#include <unistd.h>
#include "tpbcli-run.h"
#include "tpbcli-list.h"
#include "tpbcli-benchmark.h"
#include "tpbcli-help.h"
#include "tpbcli-database.h"
#include "corelib/tpb-io.h"
#include "corelib/tpb-types.h"

/* Public Function Implementations */

int
tpbcli_main(int argc, char **argv)
{
    if (argc <= 1) {
        tpb_print_help_total();
        return TPBE_EXIT_ON_HELP;
    }

    tpb_printf(TPBM_PRTN_M_DIRECT, "TPBench v%g\n", TPB_VERSION);

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
               "Unsupported action: %s. Please use one of actions:\n"
               "run/r, benchmark/b, database/d, list/l, help/h.\n",
               argv[1]);
    tpb_print_help_total();
    return TPBE_CLI_FAIL;
}

int
main(int argc, char **argv)
{
    int rc;

    
    /* Initialize logging system */
    tpb_log_init();
    
    /* Execute main CLI logic */
    rc = tpbcli_main(argc, argv);
    
    /* Cleanup logging system */
    tpb_log_cleanup();
    
    if (rc == TPBE_EXIT_ON_HELP) {
        return 0;
    }
    return rc;
}
