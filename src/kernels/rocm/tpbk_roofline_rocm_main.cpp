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

/*
 * Debug logging - enabled via:
 *   - CMake: -DTPB_SHOW_DEBUG=ON
 *   - Compiler: -DTPB_K_DEBUG=1
 */
#ifndef TPB_K_DEBUG
#define TPB_K_DEBUG 0
#endif

#if TPB_K_DEBUG
#define MAIN_DBG(fmt, ...) do { \
    fprintf(stderr, "[MAIN_DBG] " fmt "\n", ##__VA_ARGS__); \
    fflush(stderr); \
} while(0)
#else
#define MAIN_DBG(fmt, ...) ((void)0)
#endif

int
main(int argc, char **argv)
{
    int err;

    MAIN_DBG("=== main() START, argc=%d ===", argc);
    for (int i = 0; i < argc; i++) {
        MAIN_DBG("  argv[%d] = %s", i, argv[i]);
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
    MAIN_DBG("timer_name = %s", timer_name);

    MAIN_DBG("Calling tpb_k_pli_set_timer...");
    err = tpb_k_pli_set_timer(timer_name);
    if (err != 0) {
        fprintf(stderr, "Error: Failed to set timer '%s'\n", timer_name);
        return err;
    }
    MAIN_DBG("tpb_k_pli_set_timer OK");

    MAIN_DBG("Calling _tpbk_register_roofline_rocm...");
    err = _tpbk_register_roofline_rocm();
    if (err != 0) {
        fprintf(stderr, "Error: Failed to register kernel\n");
        return err;
    }
    MAIN_DBG("_tpbk_register_roofline_rocm OK");

    MAIN_DBG("Building handle...");
    tpb_k_rthdl_t handle;
    err = tpb_k_pli_build_handle(&handle, argc - 2, argv + 2);
    if (err != 0) {
        fprintf(stderr, "Error: Failed to build handle\n");
        return err;
    }
    MAIN_DBG("Handle built OK");

    MAIN_DBG("Calling tpb_cliout_args...");
    tpb_cliout_args(&handle);
    MAIN_DBG("tpb_cliout_args OK");

    tpb_printf(TPBM_PRTN_M_DIRECT, "## Kernel logs\n");
    MAIN_DBG("Calling _tpbk_run_roofline_rocm...");
    err = _tpbk_run_roofline_rocm();
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "Kernel roofline_rocm failed: %d\n", err);
        return err;
    }
    MAIN_DBG("_tpbk_run_roofline_rocm OK");

    MAIN_DBG("Outputting results...");
    tpb_cliout_results(&handle);
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Kernel roofline_rocm finished successfully.\n");

    MAIN_DBG("Cleaning up...");
    tpb_driver_clean_handle(&handle);

    MAIN_DBG("=== main() END ===");
    return 0;
}
