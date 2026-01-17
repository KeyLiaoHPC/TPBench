/*
 * tpb-run.c
 * Run subcommand implementation.
 */

#define _GNU_SOURCE
#define TPB_VER "0.71"

#include <stdlib.h>
#include "tpb-launch.h"
#include "tpb-cli.h"
#include "tpb-driver.h"
#include "tpb-impl.h"
#include "tpb-io.h"
#include "tpb-types.h"
#include "kernels/kernels.h"

int
tpb_run(int argc, char **argv)
{
    int err;
    tpb_args_t tpb_args;
    tpb_k_rthdl_t *kernel_handles = NULL;

    __tpbm_exit_on_error(err, "At tpb-run.c: tpmpi_init");
#ifdef USE_MPI
    tpb_printf(TPBM_PRTN_M_DIRECT, "TPBench-MPI v" TPB_VER "\n");
#else
    tpb_printf(TPBM_PRTN_M_DIRECT, "TPBench v" TPB_VER "\n");
#endif

    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Initializing TPBench kernels.\n");
    err = tpb_register_kernel();
    __tpbm_exit_on_error(err, "At tpb-run.c: tpb_register_kernel");
    err = register_triad();
    __tpbm_exit_on_error(err, "At tpb-run.c: register_triad");

    err = tpb_parse_args(argc, argv, &tpb_args, &kernel_handles);
    if (err == TPBE_EXIT_ON_HELP) {
        goto RUN_EXIT;
    } else {
        __tpbm_exit_on_error(err, "At tpb-run.c: tpb_parse_args");
    }

    if (tpb_args.nkern) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Kernels to run: ");
        for (int i = 0; i < tpb_args.nkern - 1; i++) {
            tpb_printf(TPBM_PRTN_M_DIRECT, "%s, ",
                       kernel_handles[i].kernel.info.name);
        }
        tpb_printf(TPBM_PRTN_M_DIRECT, "%s\n",
                   kernel_handles[tpb_args.nkern - 1].kernel.info.name);
    }

    tpb_driver_set_timer(tpb_args.timer);

    tpb_printf(TPBM_PRTN_M_DIRECT, DHLINE "\n");
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Start driver runner.\n");
    for (int i = 0; i < tpb_args.nkern; i++) {
        tpb_k_rthdl_t *handle = &kernel_handles[i];

        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Kernel %s started.\n",
                   handle->kernel.info.name);
        tpb_printf(TPBM_PRTN_M_DIRECT, "# Test %d/%d  \n",
                   i + 1, tpb_args.nkern);
        err = tpb_run_kernel(handle);
        __tpbm_exit_on_error(err, "At tpb-run.c: tpb_run_kernel");

        tpb_clean_output(handle);
    }

    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "TPBench exit.\n");

RUN_EXIT:
    if (kernel_handles != NULL) {
        for (int i = 0; i < tpb_args.nkern; i++) {
            free(kernel_handles[i].argpack.args);
        }
        free(kernel_handles);
    }

    return err;
}
