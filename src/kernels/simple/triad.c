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
#include <time.h>
#include "tptimer.h"
#include "tpmpi.h"
#include "../../tpb-types.h"
#include "../../tpb-impl.h"
#include "../../tpb-stat.h"

#ifdef KP_SVE
#include "arm_sve.h"
#endif

#if defined(AVX512) || defined(AVX2)
#include <immintrin.h>
#endif

#define MALLOC(_A, _NSIZE)  (_A) = (double *)aligned_alloc(64, sizeof(double) * _NSIZE);   \
                            if((_A) == NULL) {                                  \
                                return  MALLOC_FAIL;                            \
                            }

int
d_triad(tpb_timer_t *timer, int ntest, int64_t *time_arr, uint64_t kib, ...) {
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

    // overall result
    int nskip = 10, freq=1;
    dpipe_k0(time_arr, nskip, ntest, freq, 24, nsize);
    
    free((void *)a);
    free((void *)b);
    free((void *)c);
    return 0;
}
