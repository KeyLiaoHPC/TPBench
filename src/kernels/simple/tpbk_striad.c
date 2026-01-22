/**
 * tpbk_striad.c
 * Description: Stanza Triad: a[i] = b[i] + s * c[i], computing in stride S then
 *              skip jump elements.
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
int register_striad(void);
int _tpbk_register_striad(void);
int _tpbk_run_striad(void);
int tpbk_pli_register_striad(void);
static int d_striad(tpb_timer_t *timer, int ntest, double kib, uint32_t array_size,
                    int64_t twarm_ms, int stride, int jump,
                    int64_t *tot_time, int64_t *step_time, uint64_t *real_memsize,
                    uint32_t *array_size_out, double *bw);
static int check_d_striad(int narr, int ntest, int stride, int jump,
                          double *a, double *b, double *c, double s, double epsilon, double *errval);

int
register_striad(void)
{
    /* Wrapper function for backward compatibility */
    return _tpbk_register_striad();
}

/* PLI registration: register only name, note, and parameters (no outputs, no runner) */
int
tpbk_pli_register_striad(void)
{
    int err;

    /* Register to TPBench as PLI kernel */
    err = tpb_k_register("striad", "Stanza Triad: a[i] = b[i] + s * c[i] with stride/jump pattern", TPB_KTYPE_PLI);
    if (err != 0) return err;

    /* Kernel input parameters - same as FLI registration */
    err = tpb_k_add_parm("ntest", "Number of test iterations", "10",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)1, (int64_t)100000);
    if (err != 0) return err;
    err = tpb_k_add_parm("memsize", "Memory size in KiB", "32",
                         TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_RANGE,
                         0.0009765625, DBL_MAX);
    if (err != 0) return err;
    err = tpb_k_add_parm("array_size", "Number of elements per array (0 = use memsize)", "0",
                         TPB_PARM_CLI | TPB_UINT32_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)4294967295);
    if (err != 0) return err;
    err = tpb_k_add_parm("twarm", "Warm-up time in milliseconds, 0<=twarm<=600000, default 1", "1",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)600000);
    if (err != 0) return err;
    err = tpb_k_add_parm("stride", "Contiguous access element count", "8",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)1, (int64_t)1000000);
    if (err != 0) return err;
    err = tpb_k_add_parm("jump", "Skip element count between stanzas", "8",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)1000000);
    if (err != 0) return err;

    /* NO outputs registered here - kernel registers them in its own process */
    /* NO runner function - PLI uses exec */

    return tpb_k_finalize_pli();
}

int
_tpbk_register_striad(void)
{
    int err;
    
    /* Register to TPBench */
    err = tpb_k_register("striad", "Stanza Triad: a[i] = b[i] + s * c[i] with stride/jump pattern", TPB_KTYPE_FLI);
    if (err != 0) return err;

    /* Kernel input parameters */
    err = tpb_k_add_parm("ntest", "Number of test iterations", "10",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)1, (int64_t)100000);
    if (err != 0) return err;
    err = tpb_k_add_parm("memsize", "Memory size in KiB", "32",
                         TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_RANGE,
                         0.0009765625, DBL_MAX);
    if (err != 0) return err;
    err = tpb_k_add_parm("array_size", "Number of elements per array (0 = use memsize)", "0",
                         TPB_PARM_CLI | TPB_UINT32_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)4294967295);
    if (err != 0) return err;
    err = tpb_k_add_parm("twarm", "Warm-up time in milliseconds, 0<=twarm<=600000, default 1", "1",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)600000);
    if (err != 0) return err;
    err = tpb_k_add_parm("stride", "Contiguous access element count", "8",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)1, (int64_t)1000000);
    if (err != 0) return err;
    err = tpb_k_add_parm("jump", "Skip element count between stanzas", "8",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)1000000);
    if (err != 0) return err;

    /* Kernel outputs */
    err = tpb_k_add_output("tot_time", "Measured runtime of the outer loop (all steps).", 
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;
    err = tpb_k_add_output("step_time", "Measured runtime of per loop step.", 
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;
    err = tpb_k_add_output("real_memsize", "Actual memory footprint of three striad arrays.",
                           TPB_UINT64_T, TPB_UNIT_B | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;
    err = tpb_k_add_output("array_size", "Actual number of elements per array.",
                           TPB_UINT32_T, TPB_UNAME_UNDEF | TPB_UBASE_BASE | TPB_UATTR_CAST_N | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;
    // Set runner function.
    err = tpb_k_add_runner(_tpbk_run_striad);
    if (err != 0) return err;
    
    return 0;
}

int
_tpbk_run_striad(void)
{
    int tpberr;
    /* Input */
    int ntest;
    TPB_UNIT_T tpb_uname;
    tpb_timer_t timer;
    double memsize;
    uint32_t array_size;
    int64_t twarm_ms;
    int64_t stride, jump;
    /* Output */
    void *tot_time = NULL;
    void *step_time = NULL;
    uint64_t *real_memsize = NULL;
    double *bw = NULL;

    /* Get timer */
    tpberr = tpb_k_get_timer(&timer);
    if (tpberr) return tpberr;

    /* Get arguments by names */
    tpberr = tpb_k_get_arg("ntest", TPB_INT64_T, (void *)&ntest);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("memsize", TPB_DOUBLE_T, (void *)&memsize);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("array_size", TPB_UINT32_T, (void *)&array_size);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("twarm", TPB_INT64_T, (void *)&twarm_ms);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("stride", TPB_INT64_T, (void *)&stride);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("jump", TPB_INT64_T, (void *)&jump);
    if (tpberr) return tpberr;

    /* Malloc callbacks for kernel's outputs */
    tpberr = tpb_k_alloc_output("tot_time", 1, &tot_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("step_time", ntest, &step_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("real_memsize", 1, &real_memsize);
    if (tpberr) return tpberr;
    uint32_t *array_size_out = NULL;
    tpberr = tpb_k_alloc_output("array_size", 1, &array_size_out);
    if (tpberr) return tpberr;

    /* Measured data throughput rate is a derived metrics, adding at run-time */
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
        tpb_printf(TPBM_PRTN_M_DIRECT, "In kernel striad: unknown timer unit name %llx", tpb_uname);
        return TPBE_KERN_ARG_FAIL;
    }

    /* Call the actual kernel implementation */
    tpberr = d_striad(&timer, ntest, memsize, array_size, twarm_ms, (int)stride, (int)jump,
                      tot_time, step_time, real_memsize, array_size_out, bw);

    return tpberr;
}

static int
d_striad(tpb_timer_t *timer, int ntest, double kib, uint32_t array_size,
         int64_t twarm_ms, int stride, int jump,
         int64_t *tot_time, int64_t *step_time, uint64_t *real_memsize,
         uint32_t *array_size_out, double *bw) {
    int narr, err;
    int total_step, nb;
    double *a, *b, *c;
    double s = 0.42;

#ifdef TPB_USE_KP_SVE
    svfloat64_t tmp;
    uint64_t vec_len = svlen_f64(tmp);
#endif

    err = 0;
    /* Use array_size if specified (non-zero), otherwise use memsize */
    if (array_size > 0) {
        narr = array_size;
    } else {
        /* striad uses 3 arrays */
        narr = (int)(kib * 1024 / sizeof(double) / 3);
    }

#ifdef TPB_USE_AVX512
    narr = ((narr + 7) / 8) * 8;
#elif defined(TPB_USE_AVX2)
    narr = ((narr + 3) / 4) * 4;
#endif

    *real_memsize = narr * sizeof(double) * 3;
    *array_size_out = narr;

    MALLOC(a, narr);
    MALLOC(b, narr);
    MALLOC(c, narr);

    for (int i = 0; i < narr; i++) {
        a[i] = 1.0;
        b[i] = 2.0;
        c[i] = 3.0;
    }

    total_step = stride + jump;
    nb = narr / total_step;

    tpb_printf(TPBM_PRTN_M_DIRECT, "striad: stride=%d, jump=%d, nb=%d\n", stride, jump, nb);

    // kernel warm
    struct timespec wts;
    uint64_t wns0, wns1;

    clock_gettime(CLOCK_MONOTONIC, &wts);
    wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    wns1 = wns0 + (uint64_t)twarm_ms * 1000000ULL;
    while (wns0 < wns1) {
#ifdef TPB_USE_KP_SVE
        #pragma omp parallel for shared(a, b, c, s, stride, narr, total_step, nb)
        for (int j = 0; j < nb; j++) {
            for (int k = j * total_step; k < j * total_step + stride; k += vec_len) {
                svbool_t predicate = svwhilelt_b64_s32(k, j * total_step + stride);
                svfloat64_t vec_b = svld1_f64(predicate, b + k);
                svfloat64_t vec_c = svld1_f64(predicate, c + k);
                svfloat64_t vec_a = svmla_n_f64_z(predicate, vec_b, vec_c, s);
                svst1_f64(predicate, a + k, vec_a);
            }
        }
#elif defined(TPB_USE_AVX512)
        #pragma omp parallel for shared(a, b, c, s, stride, narr, total_step, nb)
        for (int j = 0; j < nb; j++) {
            for (int k = j * total_step; k < j * total_step + stride; k += 8) {
                __m512d v_b = _mm512_load_pd(b + k);
                __m512d v_c = _mm512_load_pd(c + k);
                __m512d v_s = _mm512_set1_pd(s);
                __m512d v_a = _mm512_fmadd_pd(v_s, v_c, v_b);
                _mm512_store_pd(a + k, v_a);
            }
        }
#elif defined(TPB_USE_AVX2)
        #pragma omp parallel for shared(a, b, c, s, stride, narr, total_step, nb)
        for (int j = 0; j < nb; j++) {
            for (int k = j * total_step; k < j * total_step + stride; k += 4) {
                __m256d v_b = _mm256_load_pd(b + k);
                __m256d v_c = _mm256_load_pd(c + k);
                __m256d v_s = _mm256_set1_pd(s);
                __m256d v_a = _mm256_fmadd_pd(v_s, v_c, v_b);
                _mm256_store_pd(a + k, v_a);
            }
        }
#else
        #pragma omp parallel for shared(a, b, c, s, stride, narr, total_step, nb)
        for (int j = 0; j < nb; j++) {
            for (int k = j * total_step; k < j * total_step + stride; k++) {
                a[k] = b[k] + s * c[k];
            }
        }
#endif
        clock_gettime(CLOCK_MONOTONIC, &wts);
        wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    }

    /* Reset arrays to initial values after warm-up for verification */
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
#ifdef TPB_USE_KP_SVE
        #pragma omp parallel for shared(a, b, c, s, stride, narr, total_step, nb)
        for (int j = 0; j < nb; j++) {
            for (int k = j * total_step; k < j * total_step + stride; k += vec_len) {
                svbool_t predicate = svwhilelt_b64_s32(k, j * total_step + stride);
                svfloat64_t vec_b = svld1_f64(predicate, b + k);
                svfloat64_t vec_c = svld1_f64(predicate, c + k);
                svfloat64_t vec_a = svmla_n_f64_z(predicate, vec_b, vec_c, s);
                svst1_f64(predicate, a + k, vec_a);
            }
        }
#elif defined(TPB_USE_AVX512)
        #pragma omp parallel for shared(a, b, c, s, stride, narr, total_step, nb)
        for (int j = 0; j < nb; j++) {
            for (int k = j * total_step; k < j * total_step + stride; k += 8) {
                __m512d v_b = _mm512_load_pd(b + k);
                __m512d v_c = _mm512_load_pd(c + k);
                __m512d v_s = _mm512_set1_pd(s);
                __m512d v_a = _mm512_fmadd_pd(v_s, v_c, v_b);
                _mm512_store_pd(a + k, v_a);
            }
        }
#elif defined(TPB_USE_AVX2)
        #pragma omp parallel for shared(a, b, c, s, stride, narr, total_step, nb)
        for (int j = 0; j < nb; j++) {
            for (int k = j * total_step; k < j * total_step + stride; k += 4) {
                __m256d v_b = _mm256_load_pd(b + k);
                __m256d v_c = _mm256_load_pd(c + k);
                __m256d v_s = _mm256_set1_pd(s);
                __m256d v_a = _mm256_fmadd_pd(v_s, v_c, v_b);
                _mm256_store_pd(a + k, v_a);
            }
        }
#else
        #pragma omp parallel for shared(a, b, c, s, stride, narr, total_step, nb)
        for (int j = 0; j < nb; j++) {
            for (int k = j * total_step; k < j * total_step + stride; k++) {
                a[k] = b[k] + s * c[k];
            }
        }
#endif
        #pragma omp barrier
        timer->tock(step_time + i);
    }
    timer->tock(tot_time);

    /* Calculate bandwidth: striad accesses 3 arrays (read b, read c, write a)
     * but only for stride elements per block */
    uint64_t accessed_bytes = (uint64_t)nb * stride * sizeof(double) * 3;
    for (int i = 0; i < ntest; i++) {
        bw[i] = ((double)accessed_bytes * 1e-6) / ((double)(step_time[i]) * 1e-9);
    }

    /* Verify results. */
    double errval;
    err = check_d_striad(narr, ntest, stride, jump, a, b, c, s, epsilon, &errval);
    tpb_printf(TPBM_PRTN_M_DIRECT, "striad error: %lf\n", errval);
    // kernel end
    
    free((void *)a);
    free((void *)b);
    free((void *)c);
    return err;
}

static int 
check_d_striad(int narr, int ntest, int stride, int jump,
               double *a, double *b, double *c, double s, double epsilon, double *errval)
{
    double a0 = 1.0;
    double b0 = 2.0;
    double c0 = 3.0;
    int total_step = stride + jump;
    int nb = narr / total_step;

    /* Simulate striad operation repeated ntest times */
    for (int i = 0; i < ntest; i++) {
        a0 = b0 + s * c0;
    }

    *errval = 0;
    /* Only check stride elements in each block */
    for (int j = 0; j < nb; j++) {
        for (int k = j * total_step; k < j * total_step + stride; k++) {
            *errval += (a[k] - a0) > 0 ? (a[k] - a0) : (a0 - a[k]);
        }
    }

    if (*errval > epsilon) return TPBE_KERN_VERIFY_FAIL;

    return 0;
}
