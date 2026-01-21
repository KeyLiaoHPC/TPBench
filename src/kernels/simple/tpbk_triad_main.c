/*
 * =================================================================================
 * TPBench - A high-precision throughputs benchmarking tool for scientific computing
 * 
 * Copyright (C) 2024 Key Liao (Liao Qiucheng)
 * 
 * This program is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software 
 * Foundation, either version 3 of the License, or (at your option) any later 
 * version.
 * 
 * =================================================================================
 * tpbk_triad_main.c
 * Description: PLI executable for triad kernel
 * =================================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../tpb-types.h"
#include "../../tpb-driver.h"
#include "../../tpb-io.h"

/* External declarations from tpbk_triad.c */
extern int _tpbk_register_triad(void);
extern int _tpbk_run_triad(void);

int
main(int argc, char **argv)
{
    int err;

    /* 1. Get timer name from argv[1] or TPBENCH_TIMER env */
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

    /* 2. Set timer */
    err = tpb_pli_set_timer(timer_name);
    if (err != 0) {
        fprintf(stderr, "Error: Failed to set timer '%s'\n", timer_name);
        return err;
    }

    /* 3. Register kernel (FLI registration for full functionality) */
    err = _tpbk_register_triad();
    if (err != 0) {
        fprintf(stderr, "Error: Failed to register kernel\n");
        return err;
    }

    /* 4. Build handle from positional arguments (argc-2, argv+2) */
    tpb_k_rthdl_t handle;
    err = tpb_pli_build_handle(&handle, argc - 2, argv + 2);
    if (err != 0) {
        fprintf(stderr, "Error: Failed to build handle\n");
        return err;
    }

    /* 5. Print arguments (tpb_cliout_args prints the header too) */
    tpb_cliout_args(&handle);

    /* 6. Run benchmark */
    tpb_printf(TPBM_PRTN_M_DIRECT, "## Kernel logs\n");
    err = _tpbk_run_triad();
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "Kernel triad failed: %d\n", err);
        return err;
    }

    /* 7. Print results */
    tpb_cliout_results(&handle);
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Kernel triad finished successfully.\n");

    /* 8. Clean up */
    tpb_driver_clean_handle(&handle);

    return 0;
}
