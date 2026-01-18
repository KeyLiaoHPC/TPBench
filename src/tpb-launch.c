/*
 * tpb-launch.c
 * Dispatch CLI actions.
 */

#include <string.h>
#include "tpb-launch.h"
#include "tpb-list.h"
#include "tpb-io.h"
#include "tpb-types.h"

int
tpb_launch(int argc, char **argv)
{
    if (argc <= 1) {
        tpb_print_help_total();
        return TPBE_EXIT_ON_HELP;
    }

    tpb_printf(TPBM_PRTN_M_DIRECT, "TPBench v%g\n", TPB_VERSION);

    if (strcmp(argv[1], "run") == 0 || strcmp(argv[1], "r") == 0) {
        return tpb_run(argc, argv);
    }
    if (strcmp(argv[1], "benchmark") == 0 || strcmp(argv[1], "bench") == 0) {
        return tpb_benchmark(argc, argv);
    }
    if (strcmp(argv[1], "list") == 0 || strcmp(argv[1], "ls") == 0) {
        return tpb_list_action();
    }
    if (strcmp(argv[1], "help") == 0) {
        return tpb_help(argc, argv);
    }

    tpb_printf(TPBM_PRTN_M_DIRECT,
               "Unsupported action: %s. Please use one of actions:\n"
               "run, benchmark, list, help.\n",
               argv[1]);
    tpb_print_help_total();
    return TPBE_CLI_FAIL;
}
