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
 * triad_pthread.c
 * Description: pthread-compatible version of triad kernel: a[i] = b[i] + s * c[i]
 * Author: Key Liao
 * Modified: Nov. 7th, 2024
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sched.h>
#include "tptimer.h"
#include "tperror.h"
#include "tpdata.h"
#include "tpparallel.h"

#ifdef KP_SVE
#include "arm_sve.h"
#endif

#if defined(AVX512) || defined(AVX2)
#include <immintrin.h>
#endif

// Thread data structure
typedef struct {
    int thread_id;
    int ntest;
    uint64_t *ns;
    uint64_t *cy;
    uint64_t kib;
    volatile double *a, *b, *c;
    double s;
    int nsize;
    int chunk_size;
} triad_thread_data_t;

// Thread function
void* triad_thread_func(void *arg) {
    triad_thread_data_t *data = (triad_thread_data_t*)arg;
    int thread_id = data->thread_id;
    int ntest = data->ntest;
    uint64_t *ns = data->ns;
    uint64_t *cy = data->cy;
    volatile double *a = data->a;
    volatile double *b = data->b;
    volatile double *c = data->c;
    double s = data->s;
    int nsize = data->nsize;
    int chunk_size = data->chunk_size;
    
    // Calculate thread boundaries
    int start = thread_id * chunk_size;
    int end = (thread_id == tpparallel_get_size() - 1) ? nsize : start + chunk_size;

#ifdef KP_SVE
    svfloat64_t tmp;
    uint64_t vec_len = svlen_f64(tmp);
#endif

    // Set thread affinity
    int cpu = sched_getcpu();
    tpparallel_set_affinity(thread_id, cpu);

    // Kernel warmup
    struct timespec wts;
    uint64_t wns0, wns1;
    __getns(wts, wns1);
    wns0 = wns1 + 1e9;
    while(wns1 < wns0) {
#ifdef KP_SVE
        for (int j = start; j < end; j += vec_len) {
            svbool_t predicate = svwhilelt_b64_s32(j, end);
            svfloat64_t vec_b = svld1_f64(predicate, b + j);
            svfloat64_t vec_c = svld1_f64(predicate, c + j);
            svfloat64_t vec_a = svmla_n_f64_z(predicate, vec_b, vec_c, s);
            svst1_f64(predicate, a + j, vec_a);
        }
#elif defined(AVX512)
        for (int j = start; j < end; j += 8) {
            __m512d v_b = _mm512_load_pd(b + j);
            __m512d v_c = _mm512_load_pd(c + j);
            __m512d v_s = _mm512_set1_pd(s);
            __m512d v_a = _mm512_fmadd_pd(v_s, v_c, v_b);
            _mm512_store_pd(a + j, v_a);
        }
#elif defined(AVX2)
        for (int j = start; j < end; j += 4) {
            __m256d v_b = _mm256_load_pd(b + j);
            __m256d v_c = _mm256_load_pd(c + j);
            __m256d v_s = _mm256_set1_pd(s);
            __m256d v_a = _mm256_fmadd_pd(v_s, v_c, v_b);
            _mm256_store_pd(a + j, v_a);
        }
#else
        for(int j = start; j < end; j ++){
            a[j] = b[j] + s * c[j];
        }
#endif
        __getns(wts, wns1);
    }

    __getcy_init;
    __getns_init;

    // Kernel execution
    for(int i = 0; i < ntest; i ++){
        tpparallel_dbarrier();
        
        // Timestamp synchronization
        tpparallel_sync_timestamp();
        
        __getns_1d_st(i);
        __getcy_1d_st(i);
        
#ifdef KP_SVE
        for (int j = start; j < end; j += vec_len) {
            svbool_t predicate = svwhilelt_b64_s32(j, end);
            svfloat64_t vec_b = svld1_f64(predicate, b + j);
            svfloat64_t vec_c = svld1_f64(predicate, c + j);
            svfloat64_t vec_a = svmla_n_f64_z(predicate, vec_b, vec_c, s);
            svst1_f64(predicate, a + j, vec_a);
        }
#elif defined(AVX512)
        for (int j = start; j < end; j += 8) {
            __m512d v_b = _mm512_load_pd(b + j);
            __m512d v_c = _mm512_load_pd(c + j);
            __m512d v_s = _mm512_set1_pd(s);
            __m512d v_a = _mm512_fmadd_pd(v_s, v_c, v_b);
            _mm512_store_pd(a + j, v_a);
        }
#elif defined(AVX2)
        for (int j = start; j < end; j += 4) {
            __m256d v_b = _mm256_load_pd(b + j);
            __m256d v_c = _mm256_load_pd(c + j);
            __m256d v_s = _mm256_set1_pd(s);
            __m256d v_a = _mm256_fmadd_pd(v_s, v_c, v_b);
            _mm256_store_pd(a + j, v_a);
        }
#else
        for(int j = start; j < end; j ++){
            a[j] = b[j] + s * c[j];
        }
#endif
        
        __getcy_1d_en(i);
        __getns_1d_en(i);
    }

    return NULL;
}

int
d_triad_pthread(int ntest, uint64_t *ns, uint64_t *cy, uint64_t kib, ...) {
    int nsize, err;
    volatile double *a, *b, *c;
    register double s = 0.42;
    int num_threads = tpparallel_get_size();

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

    // NUMA-aware memory allocation
    int numa_node = tpparallel_get_numa_node();
    a = (volatile double*)tpparallel_numa_alloc(sizeof(double) * nsize, numa_node);
    b = (volatile double*)tpparallel_numa_alloc(sizeof(double) * nsize, numa_node);
    c = (volatile double*)tpparallel_numa_alloc(sizeof(double) * nsize, numa_node);
    
    if (a == NULL || b == NULL || c == NULL) {
        return MALLOC_FAIL;
    }

    // Initialize arrays
    for(int i = 0; i < nsize; i ++) {
        a[i] = 1.0;
        b[i] = 2.0;
        c[i] = 3.0;
    }

    // Prepare thread data
    triad_thread_data_t *thread_data = (triad_thread_data_t*)malloc(num_threads * sizeof(triad_thread_data_t));
    if (thread_data == NULL) {
        tpparallel_numa_free((void*)a, sizeof(double) * nsize);
        tpparallel_numa_free((void*)b, sizeof(double) * nsize);
        tpparallel_numa_free((void*)c, sizeof(double) * nsize);
        return MALLOC_FAIL;
    }

    int chunk_size = (nsize + num_threads - 1) / num_threads;
    
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].ntest = ntest;
        thread_data[i].ns = ns;
        thread_data[i].cy = cy;
        thread_data[i].kib = kib;
        thread_data[i].a = a;
        thread_data[i].b = b;
        thread_data[i].c = c;
        thread_data[i].s = s;
        thread_data[i].nsize = nsize;
        thread_data[i].chunk_size = chunk_size;
    }

    // Create and run threads
    void **args = (void**)malloc(num_threads * sizeof(void*));
    for (int i = 0; i < num_threads; i++) {
        args[i] = &thread_data[i];
    }

    err = tpparallel_create_threads(triad_thread_func, args);
    if (err != NO_ERROR) {
        free(thread_data);
        free(args);
        tpparallel_numa_free((void*)a, sizeof(double) * nsize);
        tpparallel_numa_free((void*)b, sizeof(double) * nsize);
        tpparallel_numa_free((void*)c, sizeof(double) * nsize);
        return err;
    }

    err = tpparallel_join_threads();
    if (err != NO_ERROR) {
        free(thread_data);
        free(args);
        tpparallel_numa_free((void*)a, sizeof(double) * nsize);
        tpparallel_numa_free((void*)b, sizeof(double) * nsize);
        tpparallel_numa_free((void*)c, sizeof(double) * nsize);
        return err;
    }

    // Clean up
    free(thread_data);
    free(args);
    tpparallel_numa_free((void*)a, sizeof(double) * nsize);
    tpparallel_numa_free((void*)b, sizeof(double) * nsize);
    tpparallel_numa_free((void*)c, sizeof(double) * nsize);

    // Process results (same as original)
    int nskip = 10, freq=1;
    dpipe_k0(ns, cy, nskip, ntest, freq, 24, nsize);
    
    return 0;
}