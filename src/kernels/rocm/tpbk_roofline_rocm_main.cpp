/**
 * tpbk_roofline_rocm_main.cpp
 * PLI entry and debug main wrapper for roofline_rocm kernel.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
#include "tpbench.h"

extern int tpbk_pli_register_roofline_rocm(void);
extern int _tpbk_run_roofline_rocm(void);
}

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

extern "C" int
tpbk_roofline_rocm_entry(int argc, char **argv)
{
    int err;

    MAIN_DBG("=== tpbk_roofline_rocm_entry() START, argc=%d ===", argc);
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
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "Error: Timer not specified (argv[1] or TPBENCH_TIMER)\n");
        return TPBE_CLI_FAIL;
    }
    MAIN_DBG("timer_name = %s", timer_name);

    MAIN_DBG("Calling tpb_k_pli_set_timer...");
    err = tpb_k_pli_set_timer(timer_name);
    if (err != 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "Error: Failed to set timer '%s'\n", timer_name);
        return err;
    }
    MAIN_DBG("tpb_k_pli_set_timer OK");

    MAIN_DBG("Calling tpbk_pli_register_roofline_rocm...");
    err = tpbk_pli_register_roofline_rocm();
    if (err != 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "Error: Failed to register kernel\n");
        return err;
    }
    MAIN_DBG("tpbk_pli_register_roofline_rocm OK");

    MAIN_DBG("Building handle...");
    tpb_k_rthdl_t handle;
    err = tpb_k_pli_build_handle(&handle, argc - 2, argv + 2);
    if (err != 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "Error: Failed to build handle\n");
        return err;
    }
    MAIN_DBG("Handle built OK");

    MAIN_DBG("Calling tpb_cliout_args...");
    tpb_cliout_args(&handle);
    MAIN_DBG("tpb_cliout_args OK");

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "## Kernel logs\n");
    MAIN_DBG("Calling _tpbk_run_roofline_rocm...");
    err = _tpbk_run_roofline_rocm();
    if (err != 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                        "Kernel roofline_rocm failed: %d\n", err);
        return err;
    }
    MAIN_DBG("_tpbk_run_roofline_rocm OK");

    MAIN_DBG("Outputting results...");
    tpb_cliout_results(&handle);
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG,
                    "Kernel roofline_rocm finished successfully.\n");

    MAIN_DBG("Cleaning up...");
    tpb_driver_clean_handle(&handle);

    MAIN_DBG("=== tpbk_roofline_rocm_entry() END ===");
    return 0;
}

int
main(int argc, char **argv)
{
    return tpbk_roofline_rocm_entry(argc, argv);
}
