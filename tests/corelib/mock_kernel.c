#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mock_kernel.h"
#include "mock_timer.h"
#include "corelib/tpb-driver.h"
#include "include/tpb-public.h"

/* ---- Driver setup ---- */

int
mock_setup_driver(void)
{
    int err;

    err = tpb_corelib_init(NULL);
    if (err != 0) {
        return err;
    }

    tpb_driver_set_timer(mock_get_timer());

    err = tpb_register_kernel();
    if (err) return err;

    tpb_driver_enable_kernel_reg();

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

    if (hdl == NULL || kernel_name == NULL)
        return TPBE_NULLPTR_ARG;

    /* Query kernel - allocates isolated copy */
    tpb_query_kernel(-1, kernel_name, &kern);
    if (kern == NULL)
        return TPBE_LIST_NOT_FOUND;

    memset(hdl, 0, sizeof(tpb_k_rthdl_t));

    /* Transfer ownership of allocated data from kern to hdl->kernel. */
    hdl->kernel = *kern;
    free(kern);

    /* Build argpack with separate allocations (deep copy plims) */
    hdl->argpack.n = hdl->kernel.info.nparms;
    if (hdl->kernel.info.nparms > 0) {
        hdl->argpack.args = (tpb_rt_parm_t *)malloc(
            sizeof(tpb_rt_parm_t) * hdl->kernel.info.nparms);
        if (hdl->argpack.args == NULL)
            return TPBE_MALLOC_FAIL;
        for (int i = 0; i < hdl->kernel.info.nparms; i++) {
            memcpy(&hdl->argpack.args[i], &hdl->kernel.info.parms[i],
                   sizeof(tpb_rt_parm_t));

            if (hdl->kernel.info.parms[i].plims != NULL &&
                hdl->kernel.info.parms[i].nlims > 0) {
                hdl->argpack.args[i].plims = (tpb_parm_value_t *)malloc(
                    sizeof(tpb_parm_value_t) * hdl->kernel.info.parms[i].nlims);
                if (hdl->argpack.args[i].plims == NULL) {
                    for (int j = 0; j < i; j++) {
                        if (hdl->argpack.args[j].plims != NULL) {
                            free(hdl->argpack.args[j].plims);
                            hdl->argpack.args[j].plims = NULL;
                        }
                    }
                    free(hdl->argpack.args);
                    hdl->argpack.args = NULL;
                    return TPBE_MALLOC_FAIL;
                }
                for (int j = 0; j < hdl->kernel.info.parms[i].nlims; j++) {
                    hdl->argpack.args[i].plims[j] =
                        hdl->kernel.info.parms[i].plims[j];
                }
            } else {
                hdl->argpack.args[i].plims = NULL;
            }
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
run_pack(const char *pack, test_case_t *cases, int n, const char *filter)
{
    int pass = 0, fail = 0, run = 0;

    if (filter == NULL)
        printf("Running test pack %s (%d cases)\n", pack, n);
    printf("------------------------------------------------------\n");

    for (int i = 0; i < n; i++) {
        if (filter != NULL && strcmp(filter, cases[i].id) != 0)
            continue;
        run++;
        int result = cases[i].func();
        if (result == 0) {
            printf("[%s] %-40s PASS\n", cases[i].id, cases[i].name);
            pass++;
        } else {
            printf("[%s] %-40s FAIL\n", cases[i].id, cases[i].name);
            fail++;
        }
    }

    if (filter != NULL && run == 0) {
        fprintf(stderr, "No case matching '%s' in pack %s\n", filter, pack);
        return 1;
    }

    printf("------------------------------------------------------\n");
    if (filter == NULL)
        printf("Pack %s: %d passed, %d failed\n\n", pack, pass, fail);
    return fail;
}
