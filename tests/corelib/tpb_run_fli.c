/*
 * tpb_run_fli.c
 * Test pack A1: Unit tests for tpb_run_fli (FLI kernel runner).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mock_kernel.h"

static int g_setup_done = 0;

static int
ensure_setup(void)
{
    if (!g_setup_done) {
        int err = mock_setup_driver();
        if (err) {
            fprintf(stderr, "FATAL: mock_setup_driver failed with %d\n", err);
            return err;
        }
        g_setup_done = 1;
    }
    return 0;
}

/* A1.1: Kernel runs successfully and returns 0 */
static int
test_basic_success(void)
{
    if (ensure_setup()) return 1;

    tpb_k_rthdl_t hdl;
    int err = mock_build_handle("mock_fli_ok", &hdl);
    if (err) return 1;

    err = tpb_run_fli(&hdl);
    tpb_driver_clean_handle(&hdl);
    free(hdl.argpack.args);
    return (err == TPBE_SUCCESS) ? 0 : 1;
}

/* A1.2: NULL handle returns TPBE_NULLPTR_ARG */
static int
test_null_handle(void)
{
    int err = tpb_run_fli(NULL);
    return (err == TPBE_NULLPTR_ARG) ? 0 : 1;
}

/* A1.3: Handle with k_run=NULL returns TPBE_KERN_ARG_FAIL */
static int
test_empty_runner(void)
{
    if (ensure_setup()) return 1;

    tpb_k_rthdl_t hdl;
    int err = mock_build_handle("mock_fli_ok", &hdl);
    if (err) return 1;

    hdl.kernel.func.k_run = NULL;

    err = tpb_run_fli(&hdl);
    tpb_driver_clean_handle(&hdl);
    free(hdl.argpack.args);
    return (err == TPBE_KERN_ARG_FAIL) ? 0 : 1;
}

/* A1.4: Kernel returns WARN-level error, function still completes */
static int
test_kernel_warn(void)
{
    if (ensure_setup()) return 1;

    tpb_k_rthdl_t hdl;
    int err = mock_build_handle("mock_fli_warn", &hdl);
    if (err) return 1;

    err = tpb_run_fli(&hdl);
    tpb_driver_clean_handle(&hdl);
    free(hdl.argpack.args);
    return (err == TPBE_KERN_VERIFY_FAIL) ? 0 : 1;
}

/* A1.5: Kernel returns FAIL-level error, function aborts early */
static int
test_kernel_fail(void)
{
    if (ensure_setup()) return 1;

    tpb_k_rthdl_t hdl;
    int err = mock_build_handle("mock_fli_fail", &hdl);
    if (err) return 1;

    err = tpb_run_fli(&hdl);
    tpb_driver_clean_handle(&hdl);
    free(hdl.argpack.args);
    return (err == TPBE_KERN_ARG_FAIL) ? 0 : 1;
}

/* A1.6: Kernel retrieves parameter via tpb_k_get_arg, value matches default */
static int
test_param_retrieval(void)
{
    if (ensure_setup()) return 1;

    tpb_k_rthdl_t hdl;
    int err = mock_build_handle("mock_fli_getarg", &hdl);
    if (err) return 1;

    err = tpb_run_fli(&hdl);
    tpb_driver_clean_handle(&hdl);
    free(hdl.argpack.args);

    if (err != 0) return 1;
    if (mock_get_received_ntest() != 10) return 1;
    return 0;
}

int
main(void)
{
    test_case_t cases[] = {
        { "A1.1", "basic_success",   test_basic_success   },
        { "A1.2", "null_handle",     test_null_handle     },
        { "A1.3", "empty_runner",    test_empty_runner    },
        { "A1.4", "kernel_warn",     test_kernel_warn     },
        { "A1.5", "kernel_fail",     test_kernel_fail     },
        { "A1.6", "param_retrieval", test_param_retrieval },
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    int fail = run_pack("A1", cases, n);
    return (fail > 0) ? 1 : 0;
}
