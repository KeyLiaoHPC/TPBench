/**
 * tpbk_roofline_rocm_main.cpp
 * Description: PLI executable for roofline_rocm kernel
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
#include "tpbench.h"

extern int _tpbk_register_roofline_rocm(void);
extern int _tpbk_run_roofline_rocm(void);
}

int
main(int argc, char **argv)
{
    int err;

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

    err = _tpbk_register_roofline_rocm();
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

    tpb_printf(TPBM_PRTN_M_DIRECT, "## Kernel logs\n");
    err = _tpbk_run_roofline_rocm();
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "Kernel roofline_rocm failed: %d\n", err);
        return err;
    }

    tpb_cliout_results(&handle);
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Kernel roofline_rocm finished successfully.\n");

    tpb_driver_clean_handle(&handle);

    return 0;
}
