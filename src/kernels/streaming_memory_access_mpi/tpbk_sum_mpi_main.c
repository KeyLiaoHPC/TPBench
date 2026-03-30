/**
 * tpbk_sum_mpi_main.c
 * Description: PLI executable for sum_mpi kernel
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include "tpbench.h"

extern int _tpbk_register_sum_mpi(void);
extern int _tpbk_run_sum_mpi(void);

int
main(int argc, char **argv)
{
    int err;
    int rank;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

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

    err = tpb_k_pli_set_timer(timer_name);
    if (err != 0) {
        if (rank == 0) {
            fprintf(stderr, "Error: Failed to set timer '%s'\n", timer_name);
        }
        MPI_Finalize();
        return err;
    }

    err = _tpbk_register_sum_mpi();
    if (err != 0) {
        if (rank == 0) {
            fprintf(stderr, "Error: Failed to register kernel\n");
        }
        MPI_Finalize();
        return err;
    }

    tpb_k_rthdl_t handle;
    err = tpb_k_pli_build_handle(&handle, argc - 2, argv + 2);
    if (err != 0) {
        if (rank == 0) {
            fprintf(stderr, "Error: Failed to build handle\n");
        }
        MPI_Finalize();
        return err;
    }

    if (rank == 0) {
        tpb_cliout_args(&handle);
        tpb_printf(TPBM_PRTN_M_DIRECT, "## Kernel logs\n");
    }

    err = _tpbk_run_sum_mpi();
    if (err != 0) {
        if (rank == 0) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "Kernel sum_mpi failed: %d\n", err);
        }
        MPI_Finalize();
        return err;
    }

    if (rank == 0) {
        tpb_cliout_results(&handle);
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Kernel sum_mpi finished successfully.\n");
    }

    tpb_driver_clean_handle(&handle);

    MPI_Finalize();
    return 0;
}
