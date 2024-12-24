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
 * gemm_bcast.c
 * Description: DGEMM + MPI-Broadcast kernel
 * =================================================================================
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "tplog.h"
#include "tptimer.h"
#include "tperror.h"
#include "tpdata.h"
#include "tpmpi.h"
#include "tpio.h"

#ifdef KP_SVE
#include "arm_sve.h"
#endif

#ifdef KP_SME
#include "arm_sme.h"
#endif

#if defined(AVX512) || defined(AVX2)
#include <immintrin.h>
#endif

#define A(i, j) (A[(i) * N + (j)])
#define B(i, j) (B[(i) * N + (j)])
#define C(i, j) (C[(i) * N + (j)])
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// kernel data
static double* A;
static double* B;
static double* C;


static int get_int_from_env(char *name, int default_value) {
    const char *s = getenv(name);
    if (NULL != s) {
        return atoi(s);
    }
    return default_value;
}

static void fill_random(double *arr, size_t size) {
    struct timespec tv;
    uint64_t sec, nsec;
    clock_gettime(CLOCK_MONOTONIC, &tv);

    sec = tv.tv_sec;
    nsec = tv.tv_nsec;
    nsec = sec * 1e9 + nsec;
    srand(nsec);
    for (size_t i = 0; i < size; i ++){
        arr[i] = (double)rand() / (double) RAND_MAX;
    }
}

static void init_kernel_data(int N) {
    A = (double *) aligned_alloc(64, N * N * sizeof(double));
    B = (double *) aligned_alloc(64, N * N * sizeof(double));
    C = (double *) aligned_alloc(64, N * N * sizeof(double));

    fill_random(A, N * N);
    fill_random(B, N * N);
}

static void free_kernel_data() {
    free(A);
    free(B);
    free(C);
}

static void gemm_scalar(int N) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            for (int k = 0; k < N; k++) {
                C(i, j) += A(i, k) * B(k, j);
            }
        }
    }
}

#ifdef KP_SVE
static void gemm_sve(int N) {
    const int vec_len = svcntd();
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j+=vec_len) {
            svfloat64_t acc = svdup_n_f64(0.0);
            svbool_t pred = svwhilelt_b64_s32(j, N);
            for (int k = 0; k < N; k++) {
                svfloat64_t v_a = svdup_n_f64(A(i, k));
                svfloat64_t v_b = svld1_f64(pred, &B(k, j));
                acc = svmla_f64_x(pred, acc, v_a, v_b);
            }
            svst1_f64(pred, &C(i, j), acc);
        }
    }
}
#endif

#ifdef KP_SME
__arm_new("za") __arm_streaming
static void gemm_sme(int N) {
    const int vec_len = svcntd();
    for (int i = 0; i < N; i+=vec_len) {
        svbool_t predi = svwhilelt_b64_s32(i, N);
        for (int j = 0; j < N; j+=vec_len) {
            svbool_t predj = svwhilelt_b64_s32(j, N);
            svzero_za();
            double tmp_a[16];

            for (int k = 0; k < N; k++) {
                for (int ii = 0; i + ii < MIN(N, i + vec_len); ii++) {
                    tmp_a[ii] = A(i + ii, k);
                }
                svfloat64_t v_a = svld1_f64(predi, tmp_a);
                svfloat64_t v_b = svld1_f64(predj, &B(k, j));
                svmopa_za64_f64_m(0, predi, predj, v_a, v_b);
            }

            for (int ii = 0; i + ii < MIN(N, i + vec_len); ii++) {
                svst1_hor_za64(0, ii, predj, &C(i + ii, j));
            }
        }
    }
}
#endif

#ifdef AVX512
static void gemm_avx512(int N) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j+=8) {
            __m512d acc = _mm512_set1_pd(0.0);
            for (int k = 0; k < N; k++) {
                __m512d v_a = _mm512_set1_pd(A(i, k));
                __m512d v_b = _mm512_load_pd(&B(k, j));
                acc = _mm512_fmadd_pd(v_a, v_b, acc);
            }
            _mm512_store_pd(&C(i, j), acc);
        }
    }
}
#elif defined(AVX2)
static void gemm_avx2(int N) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j+=4) {
            __m256d acc = _mm256_set1_pd(0.0);
            for (int k = 0; k < N; k++) {
                __m256d v_a = _mm256_set1_pd(A(i, k));
                __m256d v_b = _mm256_load_pd(&B(k, j));
                acc = _mm256_fmadd_pd(v_a, v_b, acc);
            }
            _mm256_store_pd(&C(i, j), acc);
        }
    }
}
#endif

static void gemm(int N) {
#ifdef KP_SME
    gemm_sme(N);
#elif defined KP_SVE
    gemm_sve(N);
#elif defined AVX512
    gemm_avx512(N);
#elif defined AVX2
    gemm_avx2(N);
#else 
    gemm_scalar(N);
#endif
    // asm volatile ("" ::"m"(*C):);
}

static void bcast(int N, int Nr) {
#ifdef USE_MPI
    MPI_Bcast(A, Nr * N, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif
}

int d_gemm_bcast(int ntest, int nepoch, uint64_t **ns, uint64_t **cy, uint64_t matrix_size) {
    int N = matrix_size;
    int err;

    int Nr = get_int_from_env("TPBENCH_GEMM_NR", MIN(N, 10));
    int skip_comp = get_int_from_env("TPBENCH_SKIP_COMP", 0);
    int skip_comm = get_int_from_env("TPBENCH_SKIP_COMM", 0);

    if (skip_comp == 1 && skip_comm == 1) {
        tpprintf(0, 0, 0, "Both computation and communication are skipped.\n");
        return 0;
    }

#ifdef AVX512
    N = ((N + 7) / 8) * 8;
#endif

    init_kernel_data(N);

    tpprintf(0, 0, 0, "Matrix size N=%d\n", N);
    tpprintf(0, 0, 0, "#Bcast rows Nr=%d\n", Nr);
    tpprintf(0, 0, 0, "Working set size: %.1f KB\n", 1.0 * ((double) N) * N * sizeof(double) * 3 / 1000);
    tpprintf(0, 0, 0, "Bcast %.1f KB messages.\n", 1.0 * N * Nr * sizeof(double) / 1000);
    if (skip_comp) {
        tpprintf(0, 0, 0, "COMM only: the computation will be skipped.\n");
    } else if (skip_comm) {
        tpprintf(0, 0, 0, "COMP only: the communication will be skipped.\n");
    }
    // kernel warm. the #warmups need to be the same among ranks because of the MPI_Bcast
    int nwarmups = MAX(1e9 / N / N / N, 1);
    for (int i = 0; i < nwarmups; i++) {
        gemm(N);
        bcast(N, Nr);
    }

    __getcy_init;
    __getns_init;

    struct timespec wts;
    uint64_t total_wall_time = 0, timer_buff = 0;
    tpmpi_barrier();
    __getns(wts, timer_buff);

    // kernel start
    for(int i = 0; i < ntest; i ++){
        tpmpi_dbarrier();
        __getns_2d_st(i, 0);
        __getcy_2d_st(i, 0);

        if (!skip_comp) {
            gemm(N);
        }

        __getcy_2d_en(i, 0);
        __getns_2d_en(i, 0);

        __getns_2d_st(i, 1);
        __getcy_2d_st(i, 1);

        if (!skip_comm) {
            bcast(N, Nr);
        }

        __getcy_2d_en(i, 1);
        __getns_2d_en(i, 1);
    }
    // kernel end

    tpmpi_barrier();
    __getns(wts, total_wall_time);
    total_wall_time = total_wall_time - timer_buff;
    
    free_kernel_data();

    // overall result
    int nskip = 10, freq=1;
    report_performance(ns, cy, total_wall_time, nskip, ntest, nepoch, N, Nr, skip_comp, skip_comm);

    char kernel_name[32] = "gemmbcast";
    log_step_info(ns, cy, kernel_name, ntest, nepoch, N, Nr, skip_comp, skip_comm);
    return 0;
}