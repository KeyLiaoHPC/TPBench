/**
 * tpbk_triad.c
 * BLAS TRIAD: a[i] = b[i] + s * c[i]
 */

#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <float.h>
#include "tpbench.h"

#define MALLOC(_A, _NARR)  (_A) = (double *)aligned_alloc(64, sizeof(double) * _NARR);   \
                            if((_A) == NULL) {                                  \
                                return  TPBE_MALLOC_FAIL;                            \
                            }

static double epsilon = 1.e-8;

/* Local Function Prototypes */
int tpbk_pli_register_triad(void);
int run_triad(void);
static int d_triad(tpb_timer_t *timer, int ntest, double kib, uint32_t array_size, int64_t twarm_ms,
                   int64_t *tot_time, int64_t *step_time, uint64_t *real_total_memsize,
                   uint32_t *array_size_out, double *bw, int walltime_bw);
static int check_d_triad(int narr, int ntest, double *a, double *b, double *c, double s, double epsilon, double *errval);

/* PLI registration */
int
tpbk_pli_register_triad(void)
{
    int err;

    err = tpb_k_register("triad", "STREAM Triad: a[i] = b[i] + s * c[i]", TPB_KTYPE_PLI);
    if (err != 0) return err;

    err = tpb_k_add_parm("ntest", "Number of test iterations", "10",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)1, (int64_t)100000);
    if (err != 0) return err;
    err = tpb_k_add_parm("total_memsize", "Memory size in KiB", "32",
                         TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_RANGE,
                         0.0009765625, DBL_MAX);
    if (err != 0) return err;
    err = tpb_k_add_parm("array_size", "Number of elements per array (0 = use total_memsize)", "0",
                         TPB_PARM_CLI | TPB_UINT32_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)4294967295);
    if (err != 0) return err;
    err = tpb_k_add_parm("twarm", "Warm-up time in milliseconds", "1",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)600000);
    if (err != 0) return err;

    err = tpb_k_add_output("Allocated memory size", "Actual memory footprint of three triad arrays.",
                           TPB_UINT64_T, TPB_UNIT_B | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;
    err = tpb_k_add_output("TRIAD array size", "Actual number of elements per array.",
                           TPB_UINT32_T, TPB_UNAME_UNDEF | TPB_UBASE_BASE | TPB_UATTR_CAST_N | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;
    err = tpb_k_add_output("Time::Total", "Measured runtime of the outer loop (all steps).",
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;
    err = tpb_k_add_output("Time::Step", "Measured runtime of per loop step.",
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;
    err = tpb_k_add_output("Bandwidth::Walltime",
                           "Measured sustainable memory bandwidth in decimal based MB/s.",
                           TPB_DOUBLE_T, TPB_UNIT_MBPS | TPB_UATTR_CAST_N | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;
    err = tpb_k_add_output("Bandwidth::Phystime",
                           "Measured sustainable memory bandwidth in binary based Byte/cy.",
                           TPB_DOUBLE_T, TPB_UNIT_BYTEPCY | TPB_UATTR_CAST_N | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;

    return tpb_k_finalize_pli();
}

int
run_triad(void)
{
    int tpberr;
    int ntest;
    int64_t ntest64;
    TPB_UNIT_T tpb_uname;
    tpb_timer_t timer;
    double total_memsize;
    uint32_t array_size;
    int64_t twarm_ms;
    void *tot_time = NULL;
    void *step_time = NULL;
    uint64_t *real_total_memsize = NULL;
    double *bw = NULL;

    tpberr = tpb_k_get_timer(&timer);
    if (tpberr) return tpberr;

    tpberr = tpb_k_get_arg("ntest", TPB_INT64_T, (void *)&ntest64);
    if (tpberr) return tpberr;
    if (ntest64 < 1) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "triad: ntest must be >= 1, got %" PRId64 "\n", ntest64);
        return TPBE_KERN_ARG_FAIL;
    }
    if (ntest64 > (int64_t)INT_MAX) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "triad: ntest %" PRId64 " exceeds INT_MAX\n", ntest64);
        return TPBE_KERN_ARG_FAIL;
    }
    ntest = (int)ntest64;
    tpberr = tpb_k_get_arg("total_memsize", TPB_DOUBLE_T, (void *)&total_memsize);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("array_size", TPB_UINT32_T, (void *)&array_size);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("twarm", TPB_INT64_T, (void *)&twarm_ms);
    if (tpberr) return tpberr;

    tpberr = tpb_k_alloc_output("Time::Total", 1, &tot_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("Time::Step", ntest, &step_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("Allocated memory size", 1, &real_total_memsize);
    if (tpberr) return tpberr;
    uint32_t *array_size_out = NULL;
    tpberr = tpb_k_alloc_output("TRIAD array size", 1, &array_size_out);
    if (tpberr) return tpberr;

    tpb_uname = timer.unit & TPB_UNAME_MASK;
    if (tpb_uname == TPB_UNAME_WALLTIME) {
        tpberr = tpb_k_alloc_output("Bandwidth::Walltime", ntest, &bw);
        if (tpberr) return tpberr;
    } else if (tpb_uname == TPB_UNAME_PHYSTIME) {
        tpberr = tpb_k_alloc_output("Bandwidth::Phystime", ntest, &bw);
        if (tpberr) return tpberr;
    } else {
        tpb_printf(TPBM_PRTN_M_DIRECT, "In kernel triad: unknown timer unit name %llx", tpb_uname);
        return TPBE_KERN_ARG_FAIL;
    }

    tpberr = d_triad(&timer, ntest, total_memsize, array_size, twarm_ms,
                     (int64_t *)tot_time, (int64_t *)step_time, real_total_memsize, array_size_out, bw,
                     tpb_uname == TPB_UNAME_WALLTIME);

    return tpberr;
}

static int
d_triad(tpb_timer_t *timer, int ntest, double kib, uint32_t array_size, int64_t twarm_ms,
        int64_t *tot_time, int64_t *step_time, uint64_t *real_total_memsize,
        uint32_t *array_size_out, double *bw, int walltime_bw) {
    int narr, err;
    double *a, *b, *c;
    double s = 0.42;

    err = 0;
    /* Use array_size if specified (non-zero), otherwise use total_memsize */
    if (array_size > 0) {
        if (array_size > (uint32_t)INT_MAX) {
            tpb_printf(TPBM_PRTN_M_DIRECT,
                "triad: array_size %" PRIu32 " exceeds INT_MAX\n", array_size);
            return TPBE_KERN_ARG_FAIL;
        }
        narr = (int)array_size;
    } else {
        /* triad uses 3 arrays */
        narr = (int)(kib * 1024 / sizeof(double) / 3);
    }

    if (ntest < 1) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "triad: internal ntest=%d invalid\n", ntest);
        return TPBE_KERN_ARG_FAIL;
    }
    if (narr <= 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "triad: derived array length is %d (check total_memsize / array_size)\n", narr);
        return TPBE_KERN_ARG_FAIL;
    }

    *real_total_memsize = narr * sizeof(double) * 3;
    *array_size_out = (uint32_t)narr;

    MALLOC(a, narr);
    MALLOC(b, narr);
    MALLOC(c, narr);

    for(int i = 0; i < narr; i ++) {
        a[i] = 1.0;
        b[i] = 2.0;
        c[i] = 3.0;
    }

    // kernel warm
    struct timespec wts;
    uint64_t wns0, wns1;

    clock_gettime(CLOCK_MONOTONIC, &wts);
    wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    wns1 = wns0 + (uint64_t)twarm_ms * 1000000ULL;
    while(wns0 < wns1) {
        #pragma omp parallel for shared(a, b, c, s, narr)
        for(int j = 0; j < narr; j ++){
            a[j] = b[j] + s * c[j];
        }
        clock_gettime(CLOCK_MONOTONIC, &wts);
        wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    }

    /* Reset arrays to initial values after warm-up for verification */
    for(int i = 0; i < narr; i ++) {
        a[i] = 1.0;
        b[i] = 2.0;
        c[i] = 3.0;
    }

    timer->init();

    // kernel start
    timer->tick(tot_time);
    for(int i = 0; i < ntest; i ++){
        timer->tick(step_time + i);
        #pragma omp parallel for shared(a, b, c, s, narr)
        for(int j = 0; j < narr; j ++){
            a[j] = b[j] + s * c[j];
        }
        #pragma omp barrier
        timer->tock(step_time+i);
    }
    timer->tock(tot_time);

    // TRIAD: read b, read c, write a = 3 memory operations on 3 arrays
    for (int i = 0; i < ntest; i++) {
        uint64_t bytes = 3 * (uint64_t)narr * sizeof(double);
        if (step_time[i] <= 0) {
            bw[i] = 0.0;
        } else if (walltime_bw) {
            bw[i] = ((double)bytes * 1e-6) / ((double)(step_time[i]) * 1e-9);
        } else {
            // PHYSTIME: step_time[i] is cycle count; Byte/cy
            bw[i] = (double)bytes / (double)step_time[i];
        }
    }

    /* Verify results. */
    double errval;
    err = check_d_triad(narr, ntest, a, b, c, s, epsilon, &errval);
    tpb_printf(TPBM_PRTN_M_DIRECT, "triad error: %lf\n", errval);

    free((void *)a);
    free((void *)b);
    free((void *)c);
    return err;
}

static int
check_d_triad(int narr, int ntest, double *a, double *b, double *c, double s, double epsilon, double *errval)
{
    double a0 = 1.0;
    double b0 = 2.0;
    double c0 = 3.0;

    for (int i = 0; i < ntest; i++) {
        a0 = b0 + s * c0;
    }

    *errval = 0;
    for (int i = 0; i < narr; i ++) {
        *errval += (a[i] - a0) > 0 ? (a[i] - a0): (a0 - a[i]);
    }

    if (*errval > epsilon) return TPBE_KERN_VERIFY_FAIL;

    return 0;
}

/* =========================================================================
 * PLI executable entry point (only compiled into .tpbx, not into .so)
 * ========================================================================= */
#ifdef TPB_K_BUILD_MAIN
int
main(int argc, char **argv)
{
    int err;

    err = tpb_k_corelib_init(NULL);
    if (err != 0) {
        fprintf(stderr, "Error: tpb_k_corelib_init failed: %d\n", err);
        return err;
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

    err = tpb_k_pli_set_timer(timer_name);
    if (err != 0) {
        fprintf(stderr, "Error: Failed to set timer '%s'\n", timer_name);
        return err;
    }

    err = tpbk_pli_register_triad();
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

    tpb_printf(TPBM_PRTN_M_DIRECT, "Kernel logs\n");
    err = run_triad();
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "Kernel triad failed: %d\n", err);
        return err;
    }

    tpb_cliout_results(&handle);
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Kernel triad finished successfully.\n");

    tpb_k_write_task(&handle, 0);

    tpb_driver_clean_handle(&handle);

    return 0;
}
#endif /* TPB_K_BUILD_MAIN */
