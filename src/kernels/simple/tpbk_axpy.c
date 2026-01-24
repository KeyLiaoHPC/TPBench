/**
 * tpbk_axpy.c
 * BLAS AXPY: a[i] = a[i] + s * b[i]
 */

#include <inttypes.h>
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

static double epsilon = 1.e-8;

// Forward declarations
int register_axpy(void);
int _tpbk_register_axpy(void);
int _tpbk_run_axpy(void);
int tpbk_pli_register_axpy(void);
static int d_axpy(tpb_timer_t *timer, int ntest, double kib, uint32_t array_size,
                  int64_t twarm_ms,
                  int64_t *tot_time, int64_t *step_time, uint64_t *real_total_memsize,
                  uint32_t *array_size_out, double *bw);
static int check_d_axpy(int narr, int ntest, double *a, double *b, double s,
                        double epsilon, double *errval);

int
register_axpy(void)
{
    return _tpbk_register_axpy();
}

/* PLI registration */
int
tpbk_pli_register_axpy(void)
{
    int err;

    err = tpb_k_register("axpy", "BLAS AXPY: a[i] = a[i] + s * b[i]", TPB_KTYPE_PLI);
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

    return tpb_k_finalize_pli();
}

int
_tpbk_register_axpy(void)
{
    int err;
    
    err = tpb_k_register("axpy", "BLAS AXPY: a[i] = a[i] + s * b[i]", TPB_KTYPE_FLI);
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

    err = tpb_k_add_output("tot_time", "Measured runtime of the outer loop (all steps).", 
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;
    err = tpb_k_add_output("step_time", "Measured runtime of per loop step.", 
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;
    err = tpb_k_add_output("real_total_memsize", "Actual memory footprint of two axpy arrays.",
                           TPB_UINT64_T, TPB_UNIT_B | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;
    err = tpb_k_add_output("array_size", "Actual number of elements per array.",
                           TPB_UINT32_T, TPB_UNAME_UNDEF | TPB_UBASE_BASE | TPB_UATTR_CAST_N | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_runner(_tpbk_run_axpy);
    if (err != 0) return err;
    
    return 0;
}

int
_tpbk_run_axpy(void)
{
    int tpberr;
    int ntest;
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

    tpberr = tpb_k_get_arg("ntest", TPB_INT64_T, (void *)&ntest);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("total_memsize", TPB_DOUBLE_T, (void *)&total_memsize);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("array_size", TPB_UINT32_T, (void *)&array_size);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("twarm", TPB_INT64_T, (void *)&twarm_ms);
    if (tpberr) return tpberr;

    tpberr = tpb_k_alloc_output("tot_time", 1, &tot_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("step_time", ntest, &step_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("real_total_memsize", 1, &real_total_memsize);
    if (tpberr) return tpberr;
    uint32_t *array_size_out = NULL;
    tpberr = tpb_k_alloc_output("array_size", 1, &array_size_out);
    if (tpberr) return tpberr;

    tpb_uname = timer.unit & TPB_UNAME_MASK;
    if (tpb_uname == TPB_UNAME_WALLTIME) {
        tpb_k_add_output("bw_walltime", "Measured sustainable memory bandwidth in decimal based MB/s.", 
                         TPB_DOUBLE_T, TPB_UNIT_MBPS | TPB_UATTR_CAST_N | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("bw_walltime", ntest, &bw);
        if (tpberr) return tpberr;
    } else if (tpb_uname == TPB_UNAME_PHYSTIME) {
        tpb_k_add_output("bw_phystime", "Measured sustainable memory bandwidth in binary based Byte/cy.", 
                         TPB_DOUBLE_T, TPB_UNIT_BYTEPCY | TPB_UATTR_CAST_N | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("bw_phystime", ntest, &bw);
        if (tpberr) return tpberr;
    } else {
        tpb_printf(TPBM_PRTN_M_DIRECT, "In kernel axpy: unknown timer unit name %llx", tpb_uname);
        return TPBE_KERN_ARG_FAIL;
    }

    tpberr = d_axpy(&timer, ntest, total_memsize, array_size, twarm_ms,
                    tot_time, step_time, real_total_memsize, array_size_out, bw);

    return tpberr;
}

static int
d_axpy(tpb_timer_t *timer, int ntest, double kib, uint32_t array_size,
       int64_t twarm_ms,
       int64_t *tot_time, int64_t *step_time, uint64_t *real_total_memsize,
       uint32_t *array_size_out, double *bw)
{
    int narr, err;
    double *a, *b;
    double s = 0.42;

#ifdef TPB_USE_KP_SVE
    svfloat64_t tmp;
    uint64_t vec_len = svlen_f64(tmp);
#endif

    err = 0;
    if (array_size > 0) {
        narr = array_size;
    } else {
        /* axpy uses 2 arrays */
        narr = (int)(kib * 1024 / sizeof(double) / 2);
    }

#ifdef TPB_USE_AVX512
    narr = ((narr + 7) / 8) * 8;
#elif defined(TPB_USE_AVX2)
    narr = ((narr + 3) / 4) * 4;
#endif

    *real_total_memsize = narr * sizeof(double) * 2;
    *array_size_out = narr;

    MALLOC(a, narr);
    MALLOC(b, narr);

    for (int i = 0; i < narr; i++) {
        a[i] = 1.0;
        b[i] = 2.0;
    }

    // kernel warm
    struct timespec wts;
    uint64_t wns0, wns1;

    clock_gettime(CLOCK_MONOTONIC, &wts);
    wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    wns1 = wns0 + (uint64_t)twarm_ms * 1000000ULL;
    while (wns0 < wns1) {
#ifdef TPB_USE_KP_SVE
        #pragma omp parallel for shared(a, b, s, narr)
        for (int j = 0; j < narr; j += vec_len) {
            svbool_t predicate = svwhilelt_b64_s32(j, narr);
            svfloat64_t vec_a = svld1_f64(predicate, a + j);
            svfloat64_t vec_b = svld1_f64(predicate, b + j);
            vec_a = svmla_n_f64_z(predicate, vec_a, vec_b, s);
            svst1_f64(predicate, a + j, vec_a);
        }
#elif defined(TPB_USE_AVX512)
        #pragma omp parallel for shared(a, b, s, narr)
        for (int j = 0; j < narr; j += 8) {
            __m512d v_a = _mm512_load_pd(a + j);
            __m512d v_b = _mm512_load_pd(b + j);
            __m512d v_s = _mm512_set1_pd(s);
            v_a = _mm512_fmadd_pd(v_s, v_b, v_a);
            _mm512_store_pd(a + j, v_a);
        }
#elif defined(TPB_USE_AVX2)
        #pragma omp parallel for shared(a, b, s, narr)
        for (int j = 0; j < narr; j += 4) {
            __m256d v_a = _mm256_load_pd(a + j);
            __m256d v_b = _mm256_load_pd(b + j);
            __m256d v_s = _mm256_set1_pd(s);
            v_a = _mm256_fmadd_pd(v_s, v_b, v_a);
            _mm256_store_pd(a + j, v_a);
        }
#else
        #pragma omp parallel for shared(a, b, s, narr)
        for (int j = 0; j < narr; j++) {
            a[j] = a[j] + s * b[j];
        }
#endif
        clock_gettime(CLOCK_MONOTONIC, &wts);
        wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    }

    /* Reset arrays for verification */
    for (int i = 0; i < narr; i++) {
        a[i] = 1.0;
        b[i] = 2.0;
    }

    timer->init();

    // kernel start
    timer->tick(tot_time);
    for (int i = 0; i < ntest; i++) {
        timer->tick(step_time + i);
#ifdef TPB_USE_KP_SVE
        #pragma omp parallel for shared(a, b, s, narr)
        for (int j = 0; j < narr; j += vec_len) {
            svbool_t predicate = svwhilelt_b64_s32(j, narr);
            svfloat64_t vec_a = svld1_f64(predicate, a + j);
            svfloat64_t vec_b = svld1_f64(predicate, b + j);
            vec_a = svmla_n_f64_z(predicate, vec_a, vec_b, s);
            svst1_f64(predicate, a + j, vec_a);
        }
#elif defined(TPB_USE_AVX512)
        #pragma omp parallel for shared(a, b, s, narr)
        for (int j = 0; j < narr; j += 8) {
            __m512d v_a = _mm512_load_pd(a + j);
            __m512d v_b = _mm512_load_pd(b + j);
            __m512d v_s = _mm512_set1_pd(s);
            v_a = _mm512_fmadd_pd(v_s, v_b, v_a);
            _mm512_store_pd(a + j, v_a);
        }
#elif defined(TPB_USE_AVX2)
        #pragma omp parallel for shared(a, b, s, narr)
        for (int j = 0; j < narr; j += 4) {
            __m256d v_a = _mm256_load_pd(a + j);
            __m256d v_b = _mm256_load_pd(b + j);
            __m256d v_s = _mm256_set1_pd(s);
            v_a = _mm256_fmadd_pd(v_s, v_b, v_a);
            _mm256_store_pd(a + j, v_a);
        }
#else
        #pragma omp parallel for shared(a, b, s, narr)
        for (int j = 0; j < narr; j++) {
            a[j] = a[j] + s * b[j];
        }
#endif
        #pragma omp barrier
        timer->tock(step_time + i);
    }
    timer->tock(tot_time);

    /* AXPY: read a, read b, write a = 3 memory operations on 2 arrays */
    for (int i = 0; i < ntest; i++) {
        uint64_t bytes = 3 * (uint64_t)narr * sizeof(double);
        bw[i] = ((double)bytes * 1e-6) / ((double)(step_time[i]) * 1e-9);
    }

    double errval;
    err = check_d_axpy(narr, ntest, a, b, s, epsilon, &errval);
    tpb_printf(TPBM_PRTN_M_DIRECT, "axpy error: %lf\n", errval);
    
    free((void *)a);
    free((void *)b);
    return err;
}

static int 
check_d_axpy(int narr, int ntest, double *a, double *b, double s,
             double epsilon, double *errval)
{
    double a0 = 1.0;
    double b0 = 2.0;

    for (int i = 0; i < ntest; i++) {
        a0 = a0 + s * b0;
    }

    *errval = 0;
    for (int i = 0; i < narr; i++) {
        *errval += (a[i] - a0) > 0 ? (a[i] - a0) : (a0 - a[i]);
    }

    if (*errval > epsilon) return TPBE_KERN_VERIFY_FAIL;

    return 0;
}
