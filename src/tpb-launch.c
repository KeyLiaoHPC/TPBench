/*
 * tpb-launch.c
 * Dispatch CLI actions.
 */

#include <string.h>
#include "tpb-launch.h"
#include "tpb-driver.h"
#include "tpb-io.h"
#include "tpb-types.h"
#include "kernels/kernels.h"

static int
tpb_list_action(void)
{
    int err;

    if (err != 0) {
        return err;
    }

    err = tpb_register_kernel();
    if (err == 0) {
        err = register_triad();
    }

    if (err == 0) {
        tpb_list();
    }

    return err;
}

int
tpb_launch(int argc, char **argv)
{
    if (argc <= 1) {
        tpb_print_help_total();
        return TPBE_EXIT_ON_HELP;
    }

    if (strcmp(argv[1], "run") == 0) {
        return tpb_run(argc, argv);
    }
    if (strcmp(argv[1], "benchmark") == 0) {
        return tpb_benchmark(argc, argv);
    }
    if (strcmp(argv[1], "list") == 0) {
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
