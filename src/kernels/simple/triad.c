/*
 * =================================================================================
 * TPBench - A high-precision throughputs benchmarking tool for scientific computing
 * 
 * Copyright (C) 2024 Key Liao (Liao Qiucheng)
 * 
 * This program is free software: you can redistribute it and/or modify it under the
 *  terms of the GNU General Public License as published by the Free Software 
 * Foundation, either version 3 of the License, or (at your option) any later 
 * version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY 
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with 
 * this program. If not, see https://www.gnu.org/licenses/.
 * 
 * =================================================================================
 * triad.c
 * Description: a[i] = b[i] + s * c[i]
 * Author: Key Liao
 * Modified: May. 21st, 2024
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <float.h>
#include "tptimer.h"
#include "tpmpi.h"
#include "../../tpb-types.h"
#include "../../tpb-impl.h"
#include "../../tpb-driver.h"

#ifdef KP_SVE
#include "arm_sve.h"
#endif

#if defined(AVX512) || defined(AVX2)
#include <immintrin.h>
#endif

#define MALLOC(_A, _NARR)  (_A) = (double *)aligned_alloc(64, sizeof(double) * _NARR);   \
                            if((_A) == NULL) {                                  \
                                return  TPBE_MALLOC_FAIL;                            \
                            }

static double epsilon = 1.e-8;

// Forward declarations
int register_triad(void);
int run_triad(void);
static int d_triad(tpb_timer_t *timer, int ntest, double kib, int64_t *tot_time, int64_t *step_time, uint64_t *real_memsize, double *bw);
static int check_d_triad(int narr, int ntest, double *a, double *b, double *c, double s, double epsilon, double *errval);

int
register_triad(void)
{
    int err;
    
    /* Register to TPBench */
    err = tpb_k_register("triad", "STREAM Triad: a[i] = b[i] + s * c[i]");
    if(err != 0) return err;

    /* Kernel input parameters */
    err = tpb_k_add_parm("ntest", "Number of test iterations", "10",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)1, (int64_t)100000);
    if(err != 0) return err;
    err = tpb_k_add_parm("memsize", "Memory size in KiB", "32",
                         TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_RANGE,
                         0.0009765625, DBL_MAX);
    if(err != 0) return err;

    /* Kernel outputs */
    err = tpb_k_add_output("tot_time", "Measured runtime of the outer loop (all steps).", 
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_SHAPE_POINT);
    if(err != 0) return err;
    err = tpb_k_add_output("step_time", "Measured runtime of per loop step.", 
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_SHAPE_1D);
    if(err != 0) return err;
    err = tpb_k_add_output("real_memsize", "Actual memory footprint of three triad arrays.",
                           TPB_UINT64_T, TPB_UNIT_B | TPB_UATTR_CAST_Y | TPB_UATTR_SHAPE_POINT );
    if(err != 0) return err;
    // Set runner function.
    err = tpb_k_add_runner(run_triad);
    if(err != 0) return err;
    
    return 0;
}

int
run_triad(void)
{
    int tpberr;
    /* Input */
    int ntest;
    TPB_UNIT_T tpb_uname;
    tpb_timer_t timer;
    double memsize;
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

    /* Malloc callbacks for kernel's outputs */
    tpberr = tpb_k_alloc_output("tot_time", 1, &tot_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("step_time", ntest, &step_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("real_memsize", 1, &real_memsize);
    if (tpberr) return tpberr;

    /* Measured data throughput rate is a derived metrics, adding at run-time */
    tpb_uname = timer.unit & TPB_UNAME_MASK;
    if (tpb_uname == TPB_UNAME_WALLTIME) {
        tpb_k_add_output("bw_walltime", "Measured sustainable memory bandwidth in decimal based MB/s.", 
                         TPB_DOUBLE_T, TPB_UNIT_MBPS | TPB_UATTR_CAST_N | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("bw_walltime", ntest, &bw);
        if (tpberr) return tpberr;
    } else if (tpb_uname == TPB_UNAME_PHYSTIME) {
        tpb_k_add_output("bw_phystime", "Measured sustainable memory bandwidth in binay based Byte/cy.", 
                         TPB_DOUBLE_T, TPB_UNIT_BYTEPCY | TPB_UATTR_CAST_N | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("bw_phystime", ntest, &bw);
        if (tpberr) return tpberr;
    } else {
        tpb_printf(TPBM_PRTN_M_DIRECT, "In kernel triad: unknown timer unit name %llx", tpb_uname);
        return TPBE_KERN_ARG_FAIL;
    }

    /* Call the actual kernel implementation */
    tpberr = d_triad(&timer, ntest, memsize, tot_time, step_time, real_memsize, bw);

    return tpberr;
}

static int
d_triad(tpb_timer_t *timer, int ntest, double kib, int64_t *tot_time, int64_t *step_time, uint64_t *real_memsize, double *bw) {
    int narr, err;
    double *a, *b, *c;
    double s = 0.42;
    uint64_t t0, t1;

#ifdef KP_SVE
    svfloat64_t tmp;
    uint64_t vec_len = svlen_f64(tmp);
#endif 
    err = 0;
    narr = (int)(kib * 1024 / sizeof(double) / 3);
    *real_memsize = narr * sizeof(double) * 3;

#ifdef AVX512
    narr = ((narr + 7) / 8) * 8;
#elif defined(AVX2)
    narr = ((narr + 3) / 4) * 4;
#endif

    MALLOC(a, narr);
    MALLOC(b, narr);
    MALLOC(c, narr);

    for(int i = 0; i < narr; i ++) {
        a[i] = 1.0;//s;
        b[i] = 2.0;//s;// + i;
        c[i] = 3.0;//s;//i;
    }

    // kernel warm
    struct timespec wts;
    uint64_t wns0, wns1;

    clock_gettime(CLOCK_MONOTONIC, &wts);
    wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    wns1 = wns0 + 1e9;
    while(wns0 < wns1) {
#ifdef KP_SVE
        #pragma omp parallel for shared(a, b, c, s, narr)
        for (int j = 0; j < narr; j += vec_len) {
            svbool_t predicate = svwhilelt_b64_s32(j, narr);
            svfloat64_t vec_b = svld1_f64(predicate, b + j);
            svfloat64_t vec_c = svld1_f64(predicate, c + j);

            svfloat64_t vec_a = svmla_n_f64_z(predicate, vec_b, vec_c, s);
            svst1_f64(predicate, a + j, vec_a);
        }
#elif defined(AVX512)
        #pragma omp parallel for shared(a, b, c, s, narr)
        for (int j = 0; j < narr; j += 8) {
            __m512d v_b = _mm512_load_pd(b + j);
            __m512d v_c = _mm512_load_pd(c + j);
            __m512d v_s = _mm512_set1_pd(s);
            __m512d v_a = _mm512_fmadd_pd(v_s, v_c, v_b);
            _mm512_store_pd(a + j, v_a);
        }
#elif defined(AVX2)
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
        for(int j = 0; j < narr; j ++){
            a[j] = b[j] + s * c[j];
        }
#endif
        clock_gettime(CLOCK_MONOTONIC, &wts);
        wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    }

    timer->init();

    // kernel start
    timer->tick(tot_time);
    for(int i = 0; i < ntest; i ++){
        tpmpi_dbarrier();
        timer->tick(step_time + i);
#ifdef KP_SVE
        #pragma omp parallel for shared(a, b, c, s, narr)
        for (int j = 0; j < narr; j += vec_len) {
            svbool_t predicate = svwhilelt_b64_s32(j, narr);
            svfloat64_t vec_b = svld1_f64(predicate, b + j);
            svfloat64_t vec_c = svld1_f64(predicate, c + j);

            svfloat64_t vec_a = svmla_n_f64_z(predicate, vec_b, vec_c, s);
            svst1_f64(predicate, a + j, vec_a);
        }
#elif defined(AVX512)
        #pragma omp parallel for shared(a, b, c, s, narr)
        for (int j = 0; j < narr; j += 8) {
            __m512d v_b = _mm512_load_pd(b + j);
            __m512d v_c = _mm512_load_pd(c + j);
            __m512d v_s = _mm512_set1_pd(s);
            __m512d v_a = _mm512_fmadd_pd(v_s, v_c, v_b);
            _mm512_store_pd(a + j, v_a);
        }
#elif defined(AVX2)
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
        for(int j = 0; j < narr; j ++){
            a[j] = b[j] + s * c[j];
        }
#endif
        #pragma omp barrier
        timer->tock(step_time+i);
    }
    timer->tock(tot_time);
    for (int i = 0; i < ntest; i ++) {
        bw[i] = ((double)(*real_memsize) * 1e-6)  / ((double)(step_time[i]) * 1e-9);
    }
    /* Verify results. */
    double errval;
    err = check_d_triad(narr, ntest, a, b, c, s, epsilon, &errval);
    tpb_printf(TPBM_PRTN_M_DIRECT, "Triad error: %lf\n", errval);
    // kernel end
    
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