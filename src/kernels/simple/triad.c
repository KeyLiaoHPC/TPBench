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

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "tptimer.h"
#include "tpmpi.h"
#include "../../tpb-types.h"
#include "../../tpb-impl.h"
#include "../../tpb-stat.h"
#include "../../tpb-driver.h"

#ifdef KP_SVE
#include "arm_sve.h"
#endif

#if defined(AVX512) || defined(AVX2)
#include <immintrin.h>
#endif

#define MALLOC(_A, _NSIZE)  (_A) = (double *)aligned_alloc(64, sizeof(double) * _NSIZE);   \
                            if((_A) == NULL) {                                  \
                                return  TPBE_MALLOC_FAIL;                            \
                            }

// Forward declarations
int register_triad(tpb_kernel_t *kernel);
int run_triad(tpb_rt_handle_t *handle);
int d_triad(tpb_timer_t *timer, int ntest, int64_t *time_arr, uint64_t kib);

int
register_triad(tpb_kernel_t *kernel)
{
    int err;
    
    // Register kernel with name
    err = tpb_k_register("triad");
    if(err != 0) return err;
    
    // Set description
    err = tpb_k_set_note("STREAM Triad: a[i] = b[i] + s * c[i]");
    if(err != 0) return err;
    
    // Add runtime parameters with validation
    err = tpb_k_add_parm("ntest", "10", "Number of test iterations",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)1, (int64_t)100000);
    if(err != 0) return err;
    
    err = tpb_k_add_parm("nskip", "2", "Number of initial iterations to skip",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)1000);
    if(err != 0) return err;
    
    err = tpb_k_add_parm("twarm", "100", "Warmup time in milliseconds",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)10000);
    if(err != 0) return err;
    
    err = tpb_k_add_parm("memsize", "32", "Memory size in KiB",
                         TPB_PARM_CLI | TPB_UINT64_T | TPB_PARM_RANGE,
                         (uint64_t)1, (uint64_t)1048576);
    if(err != 0) return err;
    
    err = tpb_k_add_parm("s", "0.42", "Scalar multiplier",
                         TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_NOCHECK);
    if(err != 0) return err;
    
    // Set runner function
    err = tpb_k_add_runner(run_triad);
    if(err != 0) return err;
    
    // Set dimension (1D timing array)
    err = tpb_k_set_dim(1);
    if(err != 0) return err;
    
    // Set bytes per iteration (24 bytes for triad: 3 arrays * 8 bytes)
    err = tpb_k_set_nbyte(24);
    if(err != 0) return err;
    
    return 0;
}

int
run_triad(tpb_rt_handle_t *handle)
{
    if(handle == NULL) {
        return TPBE_KERN_ARG_FAIL;
    }
    
    // Extract parameters from handle
    tpb_parm_value_t *ntest_val = tpb_rt_get_parm(handle, "ntest");
    tpb_parm_value_t *memsize_val = tpb_rt_get_parm(handle, "memsize");
    
    int ntest = (ntest_val != NULL) ? (int)ntest_val->i64 : 10;
    uint64_t memsize = (memsize_val != NULL) ? memsize_val->u64 : 32;
    
    // Call the actual kernel implementation
    return d_triad(handle->timer, ntest, handle->respack->time_arr, memsize);
}

int
d_triad(tpb_timer_t *timer, int ntest, int64_t *time_arr, uint64_t kib) {
    int nsize, err;
    volatile double *a, *b, *c;
    register double s = 0.42;

#ifdef KP_SVE
    svfloat64_t tmp;
    uint64_t vec_len = svlen_f64(tmp);
#endif 

    nsize = kib * 1024 / sizeof(double);

#ifdef AVX512
    nsize = ((nsize + 7) / 8) * 8;
#elif defined(AVX2)
    nsize = ((nsize + 3) / 4) * 4;
#endif

    MALLOC(a, nsize);
    MALLOC(b, nsize);
    MALLOC(c, nsize);

    for(int i = 0; i < nsize; i ++) {
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
        #pragma omp parallel for shared(a, b, c, s, nsize)
        for (int j = 0; j < nsize; j += vec_len) {
            svbool_t predicate = svwhilelt_b64_s32(j, nsize);
            svfloat64_t vec_b = svld1_f64(predicate, b + j);
            svfloat64_t vec_c = svld1_f64(predicate, c + j);

            svfloat64_t vec_a = svmla_n_f64_z(predicate, vec_b, vec_c, s);
            svst1_f64(predicate, a + j, vec_a);
        }
#elif defined(AVX512)
        #pragma omp parallel for shared(a, b, c, s, nsize)
        for (int j = 0; j < nsize; j += 8) {
            __m512d v_b = _mm512_load_pd(b + j);
            __m512d v_c = _mm512_load_pd(c + j);
            __m512d v_s = _mm512_set1_pd(s);
            __m512d v_a = _mm512_fmadd_pd(v_s, v_c, v_b);
            _mm512_store_pd(a + j, v_a);
        }
#elif defined(AVX2)
        #pragma omp parallel for shared(a, b, c, s, nsize)
        for (int j = 0; j < nsize; j += 4) {
            __m256d v_b = _mm256_load_pd(b + j);
            __m256d v_c = _mm256_load_pd(c + j);
            __m256d v_s = _mm256_set1_pd(s);
            __m256d v_a = _mm256_fmadd_pd(v_s, v_c, v_b);
            _mm256_store_pd(a + j, v_a);
        }
#else
        #pragma omp parallel for shared(a, b, c, s, nsize)
        for(int j = 0; j < nsize; j ++){
            a[j] = b[j] + s * c[j];
        }
#endif
        clock_gettime(CLOCK_MONOTONIC, &wts);
        wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    }

    timer->init();

    // kernel start
    for(int i = 0; i < ntest; i ++){
        tpmpi_dbarrier();
        timer->tick(NULL);
#ifdef KP_SVE
        #pragma omp parallel for shared(a, b, c, s, nsize)
        for (int j = 0; j < nsize; j += vec_len) {
            svbool_t predicate = svwhilelt_b64_s32(j, nsize);
            svfloat64_t vec_b = svld1_f64(predicate, b + j);
            svfloat64_t vec_c = svld1_f64(predicate, c + j);

            svfloat64_t vec_a = svmla_n_f64_z(predicate, vec_b, vec_c, s);
            svst1_f64(predicate, a + j, vec_a);
        }
#elif defined(AVX512)
        #pragma omp parallel for shared(a, b, c, s, nsize)
        for (int j = 0; j < nsize; j += 8) {
            __m512d v_b = _mm512_load_pd(b + j);
            __m512d v_c = _mm512_load_pd(c + j);
            __m512d v_s = _mm512_set1_pd(s);
            __m512d v_a = _mm512_fmadd_pd(v_s, v_c, v_b);
            _mm512_store_pd(a + j, v_a);
        }
#elif defined(AVX2)
        #pragma omp parallel for shared(a, b, c, s, nsize)
        for (int j = 0; j < nsize; j += 4) {
            __m256d v_b = _mm256_load_pd(b + j);
            __m256d v_c = _mm256_load_pd(c + j);
            __m256d v_s = _mm256_set1_pd(s);
            __m256d v_a = _mm256_fmadd_pd(v_s, v_c, v_b);
            _mm256_store_pd(a + j, v_a);
        }
#else
        #pragma omp parallel for shared(a, b, c, s, nsize)
        for(int j = 0; j < nsize; j ++){
            a[j] = b[j] + s * c[j];
        }
#endif
        #pragma omp barrier
        timer->tock(time_arr+i);
    }
    // kernel end
    
    free((void *)a);
    free((void *)b);
    free((void *)c);
    return 0;
}
