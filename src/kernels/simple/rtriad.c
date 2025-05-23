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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "tptimer.h"
#include "tperror.h"
#include "tpdata.h"
#include "tpmpi.h"
#include "tpio.h"

#ifdef KP_SVE
#include "arm_sve.h"
static uint64_t vec_len = 8;
#endif

#if defined(AVX512) || defined(AVX2)
#include <immintrin.h>
#endif

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define MALLOC(_A, _NSIZE)  (_A) = (double *)aligned_alloc(64, sizeof(double) * _NSIZE);   \
                            if((_A) == NULL) {                                  \
                                return  MALLOC_FAIL;                            \
                            }

static inline void run_kernel_once(double *a, double *b, double *c, double s, int repeat, int nsize) {
    for (int r = 0; r < repeat; r++) {
#ifdef KP_SVE
        #pragma omp parallel for shared(a, b, c, s, nsize)
        for (int j = 0; j < nsize; j += vec_len) {
            svbool_t predicate = svwhilelt_b64_s32(j, nsize);
            svfloat64_t vec_b = svld1_f64(predicate, b + j);
            svfloat64_t vec_c = svld1_f64(predicate, c + j);
            svfloat64_t vec_a = svmla_n_f64_x(predicate, vec_b, vec_c, s);
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
    }
}

int
d_rtriad(int ntest, uint64_t *ns, uint64_t *cy, uint64_t kib, ...) {
    int nsize, err;
    volatile double *a, *b, *c;
    register double s = 0.42;

#ifdef KP_SVE
    svfloat64_t tmp;
    vec_len = svlen_f64(tmp);
#endif 

    nsize = kib * 1024 / sizeof(double);

#ifdef AVX512
    nsize = ((nsize + 7) / 8) * 8;
#endif
#ifdef AVX2
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

    int repeat = MAX(1e8 / (nsize / 8), 1);

    tpprintf(0, 0, 0, "Working set size: %dKB.\n", kib * 3);
    tpprintf(0, 0, 0, "repeat times: %lu.\n", repeat);

    // kernel warm
    struct timespec wts;
    uint64_t wns0, wns1;
    __getns(wts, wns1);
    wns0 = wns1 + 1e9;
    while(wns1 < wns0) {
        run_kernel_once(a, b, c, s, repeat, nsize);
        __getns(wts, wns1);
    }

    __getcy_init;
    __getns_init;

    // kernel start
    for(int i = 0; i < ntest; i ++){
        tpmpi_dbarrier();
        __getns_1d_st(i);
        __getcy_1d_st(i);

        run_kernel_once(a, b, c, s, repeat, nsize);

        __getcy_1d_en(i);
        __getns_1d_en(i);
    }
    // kernel end

    // overall result
    int nskip = 10, freq=1;
    dpipe_k0(ns, cy, nskip, ntest, freq, 24, ((size_t)repeat) * nsize);
    
    free((void *)a);
    free((void *)b);
    free((void *)c);
    return 0;
}
