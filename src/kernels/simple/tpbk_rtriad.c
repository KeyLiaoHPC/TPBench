/**
 * tpbk_rtriad.c
 * Repeat Triad: a[i] = b[i] + s * c[i] with repeat factor for bandwidth testing
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
#include "tpb-public.h"
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

#define MAX(a, b) ((a) > (b) ? (a) : (b))

static double epsilon = 1.e-8;

/* Local Function Prototypes */
int tpbk_pli_register_rtriad(void);
int run_rtriad(void);
static int d_rtriad(tpb_timer_t *timer, int ntest, double kib, uint32_t array_size,
                    int64_t twarm_ms, int64_t repeat,
                    int64_t *tot_time, int64_t *step_time, uint64_t *real_total_memsize,
                    uint32_t *array_size_out, int64_t *repeat_out, double *bw);
static int check_d_rtriad(int narr, int ntest, int64_t repeat, double *a, double *b, double *c,
                          double s, double epsilon, double *errval);

/* PLI registration */
int
tpbk_pli_register_rtriad(void)
{
    int err;

    err = tpb_k_register("rtriad", "Repeat Triad: a[i] = b[i] + s * c[i] with repeat factor", TPB_KTYPE_PLI);
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
    err = tpb_k_add_parm("repeat", "Repeat factor per step (0 = auto)", "0",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)1000000000);
    if (err != 0) return err;

    /* Kernel outputs */
    err = tpb_k_add_output("INPARM::Allocated memory size", "Actual memory footprint of three rtriad arrays.",
                           TPB_UINT64_T, TPB_UNIT_B | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;
    err = tpb_k_add_output("INPARM::RTriad array size", "Actual number of elements per array.",
                           TPB_UINT32_T, TPB_UNAME_UNDEF | TPB_UBASE_BASE | TPB_UATTR_CAST_N | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;
    err = tpb_k_add_output("INPARM::Repeat factor", "Actual repeat factor used.",
                           TPB_INT64_T, TPB_UNAME_UNDEF | TPB_UBASE_BASE | TPB_UATTR_CAST_N | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;
    err = tpb_k_add_output("EVENT,TIME::Total", "Measured runtime of the outer loop (all steps).",
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;
    err = tpb_k_add_output("EVENT,TIME::Step", "Measured runtime of per loop step.",
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;
    err = tpb_k_add_output("FOM,BANDWIDTH::Walltime",
                           "Measured sustainable memory bandwidth in decimal based MB/s.",
                           TPB_DOUBLE_T, TPB_UNIT_MBPS | TPB_UATTR_CAST_N | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;
    err = tpb_k_add_output("FOM,BANDWIDTH::Phystime",
                           "Measured sustainable memory bandwidth in binary based Byte/cy.",
                           TPB_DOUBLE_T, TPB_UNIT_BYTEPCY | TPB_UATTR_CAST_N | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;

    return tpb_k_finalize_pli();
}

int
run_rtriad(void)
{
    int tpberr;
    int ntest;
    int64_t ntest64;
    TPB_UNIT_T tpb_uname;
    tpb_timer_t timer;
    double total_memsize;
    uint32_t array_size;
    int64_t twarm_ms;
    int64_t repeat;
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
            "rtriad: ntest must be >= 1, got %" PRId64 "\n", ntest64);
        return TPBE_KERN_ARG_FAIL;
    }
    if (ntest64 > (int64_t)INT_MAX) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "rtriad: ntest %" PRId64 " exceeds INT_MAX\n", ntest64);
        return TPBE_KERN_ARG_FAIL;
    }
    ntest = (int)ntest64;
    tpberr = tpb_k_get_arg("total_memsize", TPB_DOUBLE_T, (void *)&total_memsize);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("array_size", TPB_UINT32_T, (void *)&array_size);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("twarm", TPB_INT64_T, (void *)&twarm_ms);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("repeat", TPB_INT64_T, (void *)&repeat);
    if (tpberr) return tpberr;

    tpberr = tpb_k_alloc_output("EVENT,TIME::Total", 1, &tot_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("EVENT,TIME::Step", ntest, &step_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("INPARM::Allocated memory size", 1, &real_total_memsize);
    if (tpberr) return tpberr;
    uint32_t *array_size_out = NULL;
    tpberr = tpb_k_alloc_output("INPARM::RTriad array size", 1, &array_size_out);
    if (tpberr) return tpberr;
    int64_t *repeat_out = NULL;
    tpberr = tpb_k_alloc_output("INPARM::Repeat factor", 1, &repeat_out);
    if (tpberr) return tpberr;

    tpb_uname = timer.unit & TPB_UNAME_MASK;
    if (tpb_uname == TPB_UNAME_WALLTIME) {
        tpberr = tpb_k_alloc_output("FOM,BANDWIDTH::Walltime", ntest, &bw);
        if (tpberr) return tpberr;
    } else if (tpb_uname == TPB_UNAME_PHYSTIME) {
        tpberr = tpb_k_alloc_output("FOM,BANDWIDTH::Phystime", ntest, &bw);
        if (tpberr) return tpberr;
    } else {
        tpb_printf(TPBM_PRTN_M_DIRECT, "In kernel rtriad: unknown timer unit name %llx", tpb_uname);
        return TPBE_KERN_ARG_FAIL;
    }

    tpberr = d_rtriad(&timer, ntest, total_memsize, array_size, twarm_ms, repeat,
                      (int64_t *)tot_time, (int64_t *)step_time, real_total_memsize, array_size_out, repeat_out, bw);

    return tpberr;
}

static inline void run_kernel_once(double *a, double *b, double *c, double s, int64_t repeat, int narr) {
#ifdef TPB_USE_KP_SVE
    svfloat64_t tmp;
    uint64_t vec_len = svlen_f64(tmp);
#endif
    for (int64_t r = 0; r < repeat; r++) {
#ifdef TPB_USE_KP_SVE
        #pragma omp parallel for shared(a, b, c, s, narr)
        for (int j = 0; j < narr; j += vec_len) {
            svbool_t predicate = svwhilelt_b64_s32(j, narr);
            svfloat64_t vec_b = svld1_f64(predicate, b + j);
            svfloat64_t vec_c = svld1_f64(predicate, c + j);
            svfloat64_t vec_a = svmla_n_f64_x(predicate, vec_b, vec_c, s);
            svst1_f64(predicate, a + j, vec_a);
        }
#elif defined(TPB_USE_AVX512)
        #pragma omp parallel for shared(a, b, c, s, narr)
        for (int j = 0; j < narr; j += 8) {
            __m512d v_b = _mm512_load_pd(b + j);
            __m512d v_c = _mm512_load_pd(c + j);
            __m512d v_s = _mm512_set1_pd(s);
            __m512d v_a = _mm512_fmadd_pd(v_s, v_c, v_b);
            _mm512_store_pd(a + j, v_a);
        }
#elif defined(TPB_USE_AVX2)
        #pragma omp parallel for shared(a, b, c, s, narr)
        for (int j = 0; j < narr; j += 4) {
            __m256d v_b = _mm256_load_pd(b + j);
            __m256d v_c = _mm256_load_pd(c + j);
            __m256d v_s = _mm256_set1_pd(s);
            __m256d v_a = _mm256_fmadd_pd(v_s, v_c, v_b);
            _mm256_store_pd(a + j, v_a);
        }
#else
        #pragma omp parallel for shared(a, b, c, s, narr)
        for (int j = 0; j < narr; j++) {
            a[j] = b[j] + s * c[j];
        }
#endif
    }
}

static int
d_rtriad(tpb_timer_t *timer, int ntest, double kib, uint32_t array_size,
         int64_t twarm_ms, int64_t repeat,
         int64_t *tot_time, int64_t *step_time, uint64_t *real_total_memsize,
         uint32_t *array_size_out, int64_t *repeat_out, double *bw)
{
    int narr, err;
    double *a, *b, *c;
    double s = 0.42;

    err = 0;
    if (array_size > 0) {
        narr = array_size;
    } else {
        /* rtriad uses 3 arrays */
        narr = (int)(kib * 1024 / sizeof(double) / 3);
    }

#ifdef TPB_USE_AVX512
    narr = ((narr + 7) / 8) * 8;
#elif defined(TPB_USE_AVX2)
    narr = ((narr + 3) / 4) * 4;
#endif

    /* Auto-calculate repeat factor if not specified */
    if (repeat <= 0) {
        repeat = MAX((int64_t)(1e8 / ((double)narr / 8.0)), 1);
    }

    *real_total_memsize = narr * sizeof(double) * 3;
    *array_size_out = narr;
    *repeat_out = repeat;

    MALLOC(a, narr);
    MALLOC(b, narr);
    MALLOC(c, narr);

    for (int i = 0; i < narr; i++) {
        a[i] = 1.0;
        b[i] = 2.0;
        c[i] = 3.0;
    }

    tpb_printf(TPBM_PRTN_M_DIRECT, "Working set size: %.2f KiB\n", (double)narr * sizeof(double) * 3 / 1024.0);
    tpb_printf(TPBM_PRTN_M_DIRECT, "Repeat factor: %ld\n", (long)repeat);

    // kernel warm
    struct timespec wts;
    uint64_t wns0, wns1;

    clock_gettime(CLOCK_MONOTONIC, &wts);
    wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    wns1 = wns0 + (uint64_t)twarm_ms * 1000000ULL;
    while (wns0 < wns1) {
        run_kernel_once(a, b, c, s, repeat, narr);
        clock_gettime(CLOCK_MONOTONIC, &wts);
        wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    }

    /* Reset arrays for verification */
    for (int i = 0; i < narr; i++) {
        a[i] = 1.0;
        b[i] = 2.0;
        c[i] = 3.0;
    }

    timer->init();

    // kernel start
    timer->tick(tot_time);
    for (int i = 0; i < ntest; i++) {
        timer->tick(step_time + i);
        run_kernel_once(a, b, c, s, repeat, narr);
        #pragma omp barrier
        timer->tock(step_time + i);
    }
    timer->tock(tot_time);

    /* Triad: read b, read c, write a = 3 arrays * repeat times */
    for (int i = 0; i < ntest; i++) {
        uint64_t bytes = 3 * (uint64_t)narr * sizeof(double) * repeat;
        bw[i] = ((double)bytes * 1e-6) / ((double)(step_time[i]) * 1e-9);
    }

    double errval;
    err = check_d_rtriad(narr, ntest, repeat, a, b, c, s, epsilon, &errval);
    tpb_printf(TPBM_PRTN_M_DIRECT, "rtriad error: %lf\n", errval);
    
    free((void *)a);
    free((void *)b);
    free((void *)c);
    return err;
}

static int 
check_d_rtriad(int narr, int ntest, int64_t repeat, double *a, double *b, double *c,
               double s, double epsilon, double *errval)
{
    /* After ntest iterations, each with 'repeat' repetitions, 
       the expected value is: a = b + s * c = 2.0 + 0.42 * 3.0 = 3.26 */
    double expected_a = 2.0 + s * 3.0;

    *errval = 0;
    for (int i = 0; i < narr; i++) {
        double diff = (a[i] - expected_a);
        *errval += diff > 0 ? diff : -diff;
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

    err = tpbk_pli_register_rtriad();
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
    err = run_rtriad();
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "Kernel rtriad failed: %d\n", err);
        return err;
    }

    tpb_cliout_results(&handle);
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Kernel rtriad finished successfully.\n");

    tpb_k_write_task(&handle, 0, NULL);

    tpb_driver_clean_handle(&handle);

    return 0;
}
#endif /* TPB_K_BUILD_MAIN */
