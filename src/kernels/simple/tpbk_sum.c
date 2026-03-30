/**
 * tpbk_sum.c
 * Sum (Reduction): s += a[i]
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

#ifdef TPB_USE_KP_SVE
#include "arm_sve.h"
#endif

#if defined(TPB_USE_AVX512) || defined(TPB_USE_AVX2)
#include <immintrin.h>
#endif

#define MALLOC(_A, _NARR)  (_A) = (double *)aligned_alloc(64, sizeof(double) * _NARR);   \
                            if((_A) == NULL) {                                  \
                                return  TPBE_MALLOC_FAIL;                            \
                            }

static double epsilon = 1.e-6;

/* Local Function Prototypes */
int tpbk_pli_register_sum(void);
int run_sum(void);
static int d_sum(tpb_timer_t *timer, int ntest, double kib, uint32_t array_size,
                 int64_t twarm_ms,
                 int64_t *tot_time, int64_t *step_time, uint64_t *real_total_memsize,
                 uint32_t *array_size_out, double *result_sum, double *bw, int walltime_bw);
static int check_d_sum(int narr, int ntest, double *result_sum, double init_val,
                       double epsilon, double *errval);

/* PLI registration */
int
tpbk_pli_register_sum(void)
{
    int err;

    err = tpb_k_register("sum", "Sum (Reduction): s += a[i]", TPB_KTYPE_PLI);
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

    err = tpb_k_add_output("Allocated memory size", "Actual memory footprint of sum array.",
                           TPB_UINT64_T, TPB_UNIT_B | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;
    err = tpb_k_add_output("SUM array size", "Actual number of elements per array.",
                           TPB_UINT32_T, TPB_UNAME_UNDEF | TPB_UBASE_BASE | TPB_UATTR_CAST_N | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;
    err = tpb_k_add_output("Time::Total", "Measured runtime of the outer loop (all steps).",
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;
    err = tpb_k_add_output("Time::Step", "Measured runtime of per loop step.",
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;
    err = tpb_k_add_output("Result::Sum", "Sum result per iteration (for verification).",
                           TPB_DOUBLE_T, TPB_UNAME_UNDEF | TPB_UBASE_BASE | TPB_UATTR_CAST_N | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_1D);
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
run_sum(void)
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
    double *result_sum = NULL;
    double *bw = NULL;

    tpberr = tpb_k_get_timer(&timer);
    if (tpberr) return tpberr;

    tpberr = tpb_k_get_arg("ntest", TPB_INT64_T, (void *)&ntest64);
    if (tpberr) return tpberr;
    if (ntest64 < 1) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "sum: ntest must be >= 1, got %" PRId64 "\n", ntest64);
        return TPBE_KERN_ARG_FAIL;
    }
    if (ntest64 > (int64_t)INT_MAX) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "sum: ntest %" PRId64 " exceeds INT_MAX\n", ntest64);
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
    tpberr = tpb_k_alloc_output("SUM array size", 1, &array_size_out);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("Result::Sum", ntest, &result_sum);
    if (tpberr) return tpberr;

    tpb_uname = timer.unit & TPB_UNAME_MASK;
    if (tpb_uname == TPB_UNAME_WALLTIME) {
        tpberr = tpb_k_alloc_output("Bandwidth::Walltime", ntest, &bw);
        if (tpberr) return tpberr;
    } else if (tpb_uname == TPB_UNAME_PHYSTIME) {
        tpberr = tpb_k_alloc_output("Bandwidth::Phystime", ntest, &bw);
        if (tpberr) return tpberr;
    } else {
        tpb_printf(TPBM_PRTN_M_DIRECT, "In kernel sum: unknown timer unit name %llx", tpb_uname);
        return TPBE_KERN_ARG_FAIL;
    }

    tpberr = d_sum(&timer, ntest, total_memsize, array_size, twarm_ms,
                   (int64_t *)tot_time, (int64_t *)step_time, real_total_memsize, array_size_out, result_sum, bw,
                   tpb_uname == TPB_UNAME_WALLTIME);

    return tpberr;
}

static int
d_sum(tpb_timer_t *timer, int ntest, double kib, uint32_t array_size,
      int64_t twarm_ms,
      int64_t *tot_time, int64_t *step_time, uint64_t *real_total_memsize,
      uint32_t *array_size_out, double *result_sum, double *bw, int walltime_bw)
{
    int narr, err;
    double *a;
    double init_val = 0.11;
    volatile double s;

#ifdef TPB_USE_KP_SVE
    svfloat64_t tmp;
    uint64_t vec_len = svlen_f64(tmp);
#endif

    err = 0;
    if (array_size > 0) {
        if (array_size > (uint32_t)INT_MAX) {
            tpb_printf(TPBM_PRTN_M_DIRECT,
                "sum: array_size %" PRIu32 " exceeds INT_MAX\n", array_size);
            return TPBE_KERN_ARG_FAIL;
        }
        narr = (int)array_size;
    } else {
        /* sum uses 1 array */
        narr = (int)(kib * 1024 / sizeof(double));
    }

#ifdef TPB_USE_AVX512
    narr = ((narr + 7) / 8) * 8;
#elif defined(TPB_USE_AVX2)
    narr = ((narr + 3) / 4) * 4;
#endif

    if (ntest < 1) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "sum: internal ntest=%d invalid\n", ntest);
        return TPBE_KERN_ARG_FAIL;
    }
    if (narr <= 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "sum: derived array length is %d (check total_memsize / array_size)\n", narr);
        return TPBE_KERN_ARG_FAIL;
    }

    *real_total_memsize = narr * sizeof(double);
    *array_size_out = (uint32_t)narr;

    MALLOC(a, narr);

    for (int i = 0; i < narr; i++) {
        a[i] = init_val;
    }

    // kernel warm
    struct timespec wts;
    uint64_t wns0, wns1;
    s = 0.0;

    clock_gettime(CLOCK_MONOTONIC, &wts);
    wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    wns1 = wns0 + (uint64_t)twarm_ms * 1000000ULL;
    while (wns0 < wns1) {
#ifdef TPB_USE_KP_SVE
        svfloat64_t vec_s = svdup_f64(0);
        for (int j = 0; j < narr; j += vec_len) {
            svbool_t predicate = svwhilelt_b64_s32(j, narr);
            svfloat64_t vec_a = svld1_f64(predicate, a + j);
            vec_s = svadd_f64_z(predicate, vec_s, vec_a);
        }
        s += svaddv(svptrue_b64(), vec_s);
#elif defined(TPB_USE_AVX512)
        __m512d v_s = _mm512_setzero_pd();
        for (int j = 0; j < narr; j += 8) {
            __m512d v_a = _mm512_load_pd(a + j);
            v_s = _mm512_add_pd(v_s, v_a);
        }
        s += _mm512_reduce_add_pd(v_s);
#elif defined(TPB_USE_AVX2)
        __m256d v_s = _mm256_setzero_pd();
        for (int j = 0; j < narr; j += 4) {
            __m256d v_a = _mm256_load_pd(a + j);
            v_s = _mm256_add_pd(v_s, v_a);
        }
        __m256d v_hadd = _mm256_hadd_pd(v_s, v_s);
        s += ((double*)&v_hadd)[0] + ((double*)&v_hadd)[2];
#else
        double local_s = 0.0;
        #pragma omp parallel for shared(a, narr) reduction(+:local_s)
        for (int j = 0; j < narr; j++) {
            local_s += a[j];
        }
        s += local_s;
#endif
        /* Reset s to avoid overflow */
        s = s / (double)(narr + 1);
        clock_gettime(CLOCK_MONOTONIC, &wts);
        wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    }

    timer->init();

    // kernel start
    timer->tick(tot_time);
    for (int i = 0; i < ntest; i++) {
        s = 0.0;
        timer->tick(step_time + i);
#ifdef TPB_USE_KP_SVE
        svfloat64_t vec_s = svdup_f64(0);
        for (int j = 0; j < narr; j += vec_len) {
            svbool_t predicate = svwhilelt_b64_s32(j, narr);
            svfloat64_t vec_a = svld1_f64(predicate, a + j);
            vec_s = svadd_f64_z(predicate, vec_s, vec_a);
        }
        s = svaddv(svptrue_b64(), vec_s);
#elif defined(TPB_USE_AVX512)
        __m512d v_s = _mm512_setzero_pd();
        for (int j = 0; j < narr; j += 8) {
            __m512d v_a = _mm512_load_pd(a + j);
            v_s = _mm512_add_pd(v_s, v_a);
        }
        s = _mm512_reduce_add_pd(v_s);
#elif defined(TPB_USE_AVX2)
        __m256d v_s = _mm256_setzero_pd();
        for (int j = 0; j < narr; j += 4) {
            __m256d v_a = _mm256_load_pd(a + j);
            v_s = _mm256_add_pd(v_s, v_a);
        }
        __m256d v_hadd = _mm256_hadd_pd(v_s, v_s);
        s = ((double*)&v_hadd)[0] + ((double*)&v_hadd)[2];
#else
        double local_s = 0.0;
        #pragma omp parallel for shared(a, narr) reduction(+:local_s)
        for (int j = 0; j < narr; j++) {
            local_s += a[j];
        }
        s = local_s;
#endif
        timer->tock(step_time + i);
        result_sum[i] = s;
    }
    timer->tock(tot_time);

    /* Sum: read a = 1 array */
    for (int i = 0; i < ntest; i++) {
        uint64_t bytes = (uint64_t)narr * sizeof(double);
        if (step_time[i] <= 0) {
            bw[i] = 0.0;
        } else if (walltime_bw) {
            bw[i] = ((double)bytes * 1e-6) / ((double)(step_time[i]) * 1e-9);
        } else {
            // PHYSTIME: step_time[i] is cycle count; Byte/cy
            bw[i] = (double)bytes / (double)step_time[i];
        }
    }

    double errval;
    err = check_d_sum(narr, ntest, result_sum, init_val, epsilon, &errval);
    tpb_printf(TPBM_PRTN_M_DIRECT, "sum error: %lf\n", errval);

    free((void *)a);
    return err;
}

static int
check_d_sum(int narr, int ntest, double *result_sum, double init_val,
            double epsilon, double *errval)
{
    /* Expected sum: narr * init_val */
    double expected = (double)narr * init_val;

    *errval = 0;
    for (int i = 0; i < ntest; i++) {
        double diff = (result_sum[i] - expected);
        double rel_err = (diff > 0 ? diff : -diff) / expected;
        if (rel_err > *errval) *errval = rel_err;
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

    err = tpbk_pli_register_sum();
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
    err = run_sum();
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "Kernel sum failed: %d\n", err);
        return err;
    }

    tpb_cliout_results(&handle);
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Kernel sum finished successfully.\n");

    tpb_k_write_task(&handle, 0, NULL);

    tpb_driver_clean_handle(&handle);

    return 0;
}
#endif /* TPB_K_BUILD_MAIN */
