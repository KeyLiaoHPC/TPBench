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
 * init.c
 * Description: init
 * Author: Key Liao
 * Modified: May. 21st, 2024
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "tptimer.h"
#include "tperror.h"
#include "tpdata.h"
#include "tpmpi.h"

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
d_sum(int ntest, uint64_t *ns, uint64_t *cy, uint64_t kib, ...) {
    int nsize, err;
    volatile double *a;
    volatile double s = 0.11;

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
    
    for(int i = 0; i < nsize; i ++) {
        a[i] = s;
    }

    // kernel warm
    struct timespec wts;
    uint64_t wns0, wns1;
    __getns(wts, wns1);
    wns0 = wns1 + 1e9;
    while(wns1 < wns0) {
#ifdef KP_SVE
        svfloat64_t vec_s = svdup_f64(0);
        for(int j = 0; j < nsize; j += vec_len){
            svbool_t predicate = svwhilelt_b64_s32(j, nsize);
            svfloat64_t vec_a = svld1_f64(predicate, a + j);
            vec_s = svadd_f64_z(predicate, vec_s, vec_a);
        }
        s += svaddv(svptrue_b64(), vec_s);
#elif defined(AVX512)
        __m512d v_s = _mm512_setzero_pd();
        for (int j = 0; j < nsize; j += 8) {
            __m512d v_a = _mm512_load_pd(a + j);
            v_s = _mm512_add_pd(v_s, v_a);
        }
        s += _mm512_reduce_add_pd(v_s);
#elif defined(AVX2)
        __m256d v_s = _mm256_setzero_pd();
        for (int j = 0; j < nsize; j += 4) {
            __m256d v_a = _mm256_load_pd(a + j);
            v_s = _mm256_add_pd(v_s, v_a);
        }
        __m256d v_hadd = _mm256_hadd_pd(v_s, v_s);
        s += ((double*)&v_hadd)[0] + ((double*)&v_hadd)[2];
#else    
        #pragma omp parallel for shared(a,nsize) reduction(+:s)
        for(int j = 0; j < nsize; j ++){
            s += a[j];
        }
#endif
        __getns(wts, wns1);
        // Reset s to 0.11
        s = s / (double)(nsize + 1);
    }

    __getcy_init;
    __getns_init;
    for(int i = 0; i < ntest; i ++){
        tpmpi_dbarrier();
        __getns_1d_st(i);
        __getcy_1d_st(i);
#ifdef KP_SVE
        svfloat64_t vec_s = svdup_f64(0);
        for(int j = 0; j < nsize; j += vec_len){
            svbool_t predicate = svwhilelt_b64_s32(j, nsize);
            svfloat64_t vec_a = svld1_f64(predicate, a + j);
            vec_s = svadd_f64_z(predicate, vec_s, vec_a);
        }
        s += svaddv(svptrue_b64(), vec_s);
#elif defined(AVX512)
        __m512d v_s = _mm512_setzero_pd();
        for (int j = 0; j < nsize; j += 8) {
            __m512d v_a = _mm512_load_pd(a + j);
            v_s = _mm512_add_pd(v_s, v_a);
        }
        s += _mm512_reduce_add_pd(v_s);
#elif defined(AVX2)
        __m256d v_s = _mm256_setzero_pd();
        for (int j = 0; j < nsize; j += 4) {
            __m256d v_a = _mm256_load_pd(a + j);
            v_s = _mm256_add_pd(v_s, v_a);
        }
        __m256d v_hadd = _mm256_hadd_pd(v_s, v_s);
        s += ((double*)&v_hadd)[0] + ((double*)&v_hadd)[2];
#else    
        #pragma omp parallel for shared(a,nsize) reduction(+:s)
        for(int j = 0; j < nsize; j ++){
            s += a[j];
        }
#endif
        __getcy_1d_en(i);
        __getns_1d_en(i);
        // Reset s to 0.11
        s = s / (double)(nsize + 1);
    }

    // overall result
    int nskip = 10, freq=1;

    dpipe_k0(ns, cy, nskip, ntest, freq, 8, nsize);
    free((void *)a);
    return 0;
}