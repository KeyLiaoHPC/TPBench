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
 * tpbk_stream_mpi_main.c
 * Description: PLI executable for MPI stream kernel
 * =================================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include "tpbench.h"

/* External declarations from tpbk_stream_mpi.c */
extern int _tpbk_register_stream_mpi(void);
extern int _tpbk_run_stream_mpi(void);

int
main(int argc, char **argv)
{
    int err;
    int rank;

    /* Initialize MPI */
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /* 1. Get timer name from argv[1] or TPBENCH_TIMER env */
    const char *timer_name = NULL;
    if (argc >= 2) {
        timer_name = argv[1];
    } else {
        timer_name = getenv("TPBENCH_TIMER");
    }
    if (timer_name == NULL) {
        if (rank == 0) {
            fprintf(stderr, "Error: Timer not specified (argv[1] or TPBENCH_TIMER)\n");
        }
        MPI_Finalize();
        return TPBE_CLI_FAIL;
    }

    /* 2. Set timer */
    err = tpb_k_pli_set_timer(timer_name);
    if (err != 0) {
        if (rank == 0) {
            fprintf(stderr, "Error: Failed to set timer '%s'\n", timer_name);
        }
        MPI_Finalize();
        return err;
    }

    /* 3. Register kernel (FLI registration for full functionality) */
    err = _tpbk_register_stream_mpi();
    if (err != 0) {
        if (rank == 0) {
            fprintf(stderr, "Error: Failed to register kernel\n");
        }
        MPI_Finalize();
        return err;
    }

    /* 4. Build handle from positional arguments (argc-2, argv+2) */
    tpb_k_rthdl_t handle;
    err = tpb_k_pli_build_handle(&handle, argc - 2, argv + 2);
    if (err != 0) {
        if (rank == 0) {
            fprintf(stderr, "Error: Failed to build handle\n");
        }
        MPI_Finalize();
        return err;
    }

    /* 5. Print arguments (only rank 0) */
    if (rank == 0) {
        tpb_cliout_args(&handle);
    }

    /* 6. Run benchmark */
    if (rank == 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "## Kernel logs\n");
    }
    err = _tpbk_run_stream_mpi();
    if (err != 0) {
        if (rank == 0) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "Kernel stream_mpi failed: %d\n", err);
        }
        MPI_Finalize();
        return err;
    }

    /* 7. Print results (only rank 0) */
    if (rank == 0) {
        tpb_cliout_results(&handle);
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Kernel stream_mpi finished successfully.\n");
    }

    /* 8. Clean up */
    tpb_driver_clean_handle(&handle);

    MPI_Finalize();
    return 0;
}
