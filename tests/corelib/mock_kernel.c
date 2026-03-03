#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mock_kernel.h"
#include "mock_timer.h"
#include "corelib/tpb-driver.h"

/* ---- Mock FLI runners ---- */

static int64_t g_received_ntest = -1;

int
mock_fli_success(void)
{
    return TPBE_SUCCESS;
}

int
mock_fli_warn(void)
{
    return TPBE_KERN_VERIFY_FAIL;
}

int
mock_fli_fail(void)
{
    return TPBE_KERN_ARG_FAIL;
}

int
mock_fli_check_arg(void)
{
    int64_t ntest = -1;
    int err = tpb_k_get_arg("ntest", TPB_INT64_T, &ntest);
    if (err != 0)
        return err;
    g_received_ntest = ntest;
    return 0;
}

int64_t
mock_get_received_ntest(void)
{
    return g_received_ntest;
}

/* ---- Driver setup ---- */

int
mock_setup_driver(void)
{
    int err;

    tpb_driver_set_integ_mode(0 /* TPB_INTEG_MODE_FLI */);
    tpb_driver_set_timer(mock_get_timer());

    err = tpb_register_kernel();
    if (err) return err;

    tpb_driver_enable_kernel_reg();

    /* mock_fli_ok: simple success kernel with one param and one output */
    err = tpb_k_register("mock_fli_ok", "Mock FLI success kernel", TPB_KTYPE_FLI);
    if (err) return err;
    err = tpb_k_add_parm("ntest", "Number of iterations", "10",
                          TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_NOCHECK);
    if (err) return err;
    err = tpb_k_add_output("elapsed", "Elapsed time", TPB_INT64_T, TPB_UNIT_NS);
    if (err) return err;
    err = tpb_k_add_runner(mock_fli_success);
    if (err) return err;

    /* mock_fli_warn: returns WARN-level error */
    err = tpb_k_register("mock_fli_warn", "Mock FLI warn kernel", TPB_KTYPE_FLI);
    if (err) return err;
    err = tpb_k_add_parm("ntest", "Number of iterations", "10",
                          TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_NOCHECK);
    if (err) return err;
    err = tpb_k_add_runner(mock_fli_warn);
    if (err) return err;

    /* mock_fli_fail: returns FAIL-level error */
    err = tpb_k_register("mock_fli_fail", "Mock FLI fail kernel", TPB_KTYPE_FLI);
    if (err) return err;
    err = tpb_k_add_parm("ntest", "Number of iterations", "10",
                          TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_NOCHECK);
    if (err) return err;
    err = tpb_k_add_runner(mock_fli_fail);
    if (err) return err;

    /* mock_fli_getarg: retrieves param via tpb_k_get_arg */
    err = tpb_k_register("mock_fli_getarg", "Mock FLI getarg kernel", TPB_KTYPE_FLI);
    if (err) return err;
    err = tpb_k_add_parm("ntest", "Number of iterations", "10",
                          TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_NOCHECK);
    if (err) return err;
    err = tpb_k_add_runner(mock_fli_check_arg);
    if (err) return err;

    /* mock_pli_test: PLI kernel (no runner) */
    err = tpb_k_register("mock_pli_test", "Mock PLI kernel", TPB_KTYPE_PLI);
    if (err) return err;
    err = tpb_k_add_parm("ntest", "Number of iterations", "10",
                          TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_NOCHECK);
    if (err) return err;
    err = tpb_k_finalize_pli();
    if (err) return err;

    tpb_driver_disable_kernel_reg();
    return 0;
}

/* ---- Handle builder ---- */

int
mock_build_handle(const char *kernel_name, tpb_k_rthdl_t *hdl)
{
    tpb_kernel_t *kern = NULL;
    int err;

    if (hdl == NULL || kernel_name == NULL)
        return TPBE_NULLPTR_ARG;

    err = tpb_get_kernel(kernel_name, &kern);
    if (err) return err;

    memset(hdl, 0, sizeof(tpb_k_rthdl_t));
    memcpy(&hdl->kernel, kern, sizeof(tpb_kernel_t));

    hdl->argpack.n = kern->info.nparms;
    if (kern->info.nparms > 0) {
        hdl->argpack.args = (tpb_rt_parm_t *)malloc(
            sizeof(tpb_rt_parm_t) * kern->info.nparms);
        if (hdl->argpack.args == NULL)
            return TPBE_MALLOC_FAIL;
        for (int i = 0; i < kern->info.nparms; i++) {
            memcpy(&hdl->argpack.args[i], &kern->info.parms[i],
                   sizeof(tpb_rt_parm_t));
            hdl->argpack.args[i].value = kern->info.parms[i].default_value;
        }
    } else {
        hdl->argpack.args = NULL;
    }

    hdl->respack.n = 0;
    hdl->respack.outputs = NULL;
    hdl->envpack.n = 0;
    hdl->envpack.envs = NULL;
    hdl->mpipack.mpiargs = NULL;

    return 0;
}

/* ---- Test harness ---- */

int
run_pack(const char *pack, test_case_t *cases, int n)
{
    int pass = 0, fail = 0;

    printf("Running test pack %s (%d cases)\n", pack, n);
    printf("------------------------------------------------------\n");

    for (int i = 0; i < n; i++) {
        int result = cases[i].func();
        if (result == 0) {
            printf("[%s] %-40s PASS\n", cases[i].id, cases[i].name);
            pass++;
        } else {
            printf("[%s] %-40s FAIL\n", cases[i].id, cases[i].name);
            fail++;
        }
    }

    printf("------------------------------------------------------\n");
    printf("Pack %s: %d passed, %d failed\n\n", pack, pass, fail);
    return fail;
}
