/**
 * tpbk_triad_main.c
 * PLI executable for triad kernel
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tpbench.h"

/* PLI registration and runner from tpbk_triad.c */
extern int tpbk_pli_register_triad(void);
extern int run_triad(void);

int
main(int argc, char **argv)
{
    int err;

    err = tpb_k_corelib_init(NULL);
    if (err != 0) {
        fprintf(stderr, "Error: tpb_k_corelib_init failed: %d\n", err);
        return err;
    }

    const char *timer_name = NULL;
    if (argc >= 2) {
        timer_name = argv[1];
    } else {
        timer_name = getenv("TPBENCH_TIMER");
    }
    if (timer_name == NULL) {
        fprintf(stderr, "Error: Timer not specified (argv[1] or TPBENCH_TIMER)\n");
        return TPBE_CLI_FAIL;
    }

    err = tpb_k_pli_set_timer(timer_name);
    if (err != 0) {
        fprintf(stderr, "Error: Failed to set timer '%s'\n", timer_name);
        return err;
    }

    err = tpbk_pli_register_triad();
    if (err != 0) {
        fprintf(stderr, "Error: Failed to register kernel\n");
        return err;
    }

    tpb_k_rthdl_t handle;
    err = tpb_k_pli_build_handle(&handle, argc - 2, argv + 2);
    if (err != 0) {
        fprintf(stderr, "Error: Failed to build handle\n");
        return err;
    }

    tpb_cliout_args(&handle);

    tpb_printf(TPBM_PRTN_M_DIRECT, "Kernel logs\n");
    err = run_triad();
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "Kernel triad failed: %d\n", err);
        return err;
    }

    tpb_cliout_results(&handle);
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Kernel triad finished successfully.\n");

    tpb_k_write_task(&handle, 0);

    tpb_driver_clean_handle(&handle);

    return 0;
}
