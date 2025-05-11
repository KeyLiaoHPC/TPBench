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
 * ldr.c
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

#ifdef __aarch64__

#ifdef KP_SVE
#include "arm_sve.h"
#else
#include "arm_neon.h"
#endif

#elif defined __x86_64__

#include <immintrin.h>

#endif

#define DUP_2(x) x x
#define DUP_4(x) x x x x
#define DUP_8(x) x x x x x x x x 
#define DUP_16(x) DUP_8(x) DUP_8(x)
#define DUP_32(x) DUP_16(x) DUP_16(x)

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MALLOC(_A, _NSIZE)  (_A) = (double *)aligned_alloc(64, sizeof(double) * _NSIZE);   \
                            if((_A) == NULL) {                                  \
                                return  MALLOC_FAIL;                            \
                            }


static double *a;
static size_t repeat = 1;

static int init_kernel_data(int nsize) {
    MALLOC(a, nsize);
    for (int i = 0; i < nsize; i++) {
        a[i] = 1.23;
    }
}

static void free_kernel_data() {
    free(a);
}

/* change this macro to adjust the Compute:Load ratio */
// #define FL_RATIO_F1L4 1
// #define FL_RATIO_F1L2 1
#define FL_RATIO_F8L1 1
// #define FL_RATIO_F2L1 1
// #define FL_RATIO_F4L1 1
// #define FL_RATIO_F8L1 1

#ifdef __aarch64__
#ifdef KP_SVE
int simd_width = 1;
const char *SIMD_NAME = "SVE";
#define INIT_REGISTERS  \
    z1 = svdup_f64(1.23); \
    z2 = svdup_f64(1.23); \
    z3 = svdup_f64(1.23); \
    z4 = svdup_f64(1.23); \
    z5 = svdup_f64(1.23); \
    z6 = svdup_f64(1.23); \
    z7 = svdup_f64(1.23); \
    z8 = svdup_f64(1.23); \
    w1 = svdup_f64(1.23); \
    w2 = svdup_f64(1.23); \
    w3 = svdup_f64(1.23); \
    w4 = svdup_f64(1.23); \
    w5 = svdup_f64(1.23); \
    w6 = svdup_f64(1.23); \
    w7 = svdup_f64(1.23); \
    w8 = svdup_f64(1.23);


#if defined(FL_RATIO_F1L4)
static const double operation_intensity=0.25 / 4; // 2 * simd_width / (4 * simd_width * 8)
static const size_t flops_per_cache_line=16 / 4;  // 2 * simd_width / (4 * simd_width / 8)
static void run_kernel_once(int nsize) {
    svbool_t predicate = svwhilelt_b64_s32(0, simd_width);
    svfloat64_t z0, z1, z2, z3, z4, z5, z6, z7, z8, 
                w1, w2, w3, w4, w5, w6, w7, w8;

    INIT_REGISTERS
    
    for (int r = 0; r < repeat; r++) {
        for (int i = 0; i < nsize; i += 16 * simd_width) {
            z0 = svld1_f64(predicate, &a[i]);
            z1 = svld1_f64(predicate, &a[i + simd_width]);
            z2 = svld1_f64(predicate, &a[i + 2 * simd_width]);
            z3 = svld1_f64(predicate, &a[i + 3 * simd_width]);
            z4 = svmad_f64_x(predicate, z4, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 4 * simd_width]);
            z5 = svld1_f64(predicate, &a[i + 5 * simd_width]);
            z6 = svld1_f64(predicate, &a[i + 6 * simd_width]);
            z7 = svld1_f64(predicate, &a[i + 7 * simd_width]);
            z8 = svmad_f64_x(predicate, z8, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 8 * simd_width]);
            w1 = svld1_f64(predicate, &a[i + 9 * simd_width]);
            w2 = svld1_f64(predicate, &a[i + 10 * simd_width]);
            w3 = svld1_f64(predicate, &a[i + 11 * simd_width]);
            w4 = svmad_f64_x(predicate, w4, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 12 * simd_width]);
            w5 = svld1_f64(predicate, &a[i + 13 * simd_width]);
            w6 = svld1_f64(predicate, &a[i + 14 * simd_width]);
            w7 = svld1_f64(predicate, &a[i + 15 * simd_width]);
            w8 = svmad_f64_x(predicate, w8, z0, z0);
            asm volatile (
                "\n\t":: "w" (z0), "w" (z1), "w" (z2), "w" (z3), "w" (z4), 
                         "w" (z5), "w" (z6), "w" (z7), "w" (z8),
                         "w" (w1), "w" (w2), "w" (w3), "w" (w4), 
                         "w" (w5), "w" (w6), "w" (w7), "w" (w8):
            );
        }
    }
} 
#elif defined(FL_RATIO_F1L2)
static const double operation_intensity=0.25 / 2;
static const size_t flops_per_cache_line=16 / 2;
static void run_kernel_once(int nsize) {
    svbool_t predicate = svwhilelt_b64_s32(0, simd_width);
    svfloat64_t z0, z1, z2, z3, z4, z5, z6, z7, z8, 
                w1, w2, w3, w4, w5, w6, w7, w8;

    INIT_REGISTERS
    
    for (int r = 0; r < repeat; r++) {
        for (int i = 0; i < nsize; i += 16 * simd_width) {
            z0 = svld1_f64(predicate, &a[i]);
            z1 = svld1_f64(predicate, &a[i + 1 * simd_width]);
            z2 = svmad_f64_x(predicate, z2, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 2 * simd_width]);
            z3 = svld1_f64(predicate, &a[i + 3 * simd_width]);
            z4 = svmad_f64_x(predicate, z4, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 4 * simd_width]);
            z5 = svld1_f64(predicate, &a[i + 5 * simd_width]);
            z6 = svmad_f64_x(predicate, z6, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 6 * simd_width]);
            z7 = svld1_f64(predicate, &a[i + 7 * simd_width]);
            z8 = svmad_f64_x(predicate, z8, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 8 * simd_width]);
            w1 = svld1_f64(predicate, &a[i + 9 * simd_width]);
            w2 = svmad_f64_x(predicate, w2, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 10 * simd_width]);
            w3 = svld1_f64(predicate, &a[i + 11 * simd_width]);
            w4 = svmad_f64_x(predicate, w4, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 12 * simd_width]);
            w5 = svld1_f64(predicate, &a[i + 13 * simd_width]);
            w6 = svmad_f64_x(predicate, w6, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 14 * simd_width]);
            w7 = svld1_f64(predicate, &a[i + 15 * simd_width]);
            w8 = svmad_f64_x(predicate, w8, z0, z0);
            asm volatile (
                "\n\t":: "w" (z0), "w" (z1), "w" (z2), "w" (z3), "w" (z4), 
                         "w" (z5), "w" (z6), "w" (z7), "w" (z8),
                         "w" (w1), "w" (w2), "w" (w3), "w" (w4), 
                         "w" (w5), "w" (w6), "w" (w7), "w" (w8):
            );
        }
    }
} 
#elif defined(FL_RATIO_F1L1)
static const double operation_intensity=0.25;
static const size_t flops_per_cache_line=16;
static void run_kernel_once(int nsize) {
    svbool_t predicate = svwhilelt_b64_s32(0, simd_width);
    svfloat64_t z0, z1, z2, z3, z4, z5, z6, z7, z8, 
                w1, w2, w3, w4, w5, w6, w7, w8;

    INIT_REGISTERS
    
    for (int r = 0; r < repeat; r++) {
        for (int i = 0; i < nsize; i += 16 * simd_width) {
            z0 = svld1_f64(predicate, &a[i]);
            z1 = svmad_f64_x(predicate, z1, z0, z0);
            z0 = svld1_f64(predicate, &a[i + simd_width]);
            z2 = svmad_f64_x(predicate, z2, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 2 * simd_width]);
            z3 = svmad_f64_x(predicate, z3, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 3 * simd_width]);
            z4 = svmad_f64_x(predicate, z4, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 4 * simd_width]);
            z5 = svmad_f64_x(predicate, z5, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 5 * simd_width]);
            z6 = svmad_f64_x(predicate, z6, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 6 * simd_width]);
            z7 = svmad_f64_x(predicate, z7, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 7 * simd_width]);
            z8 = svmad_f64_x(predicate, z8, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 8 * simd_width]);
            w1 = svmad_f64_x(predicate, w1, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 9 * simd_width]);
            w2 = svmad_f64_x(predicate, w2, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 10 * simd_width]);
            w3 = svmad_f64_x(predicate, w3, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 11 * simd_width]);
            w4 = svmad_f64_x(predicate, w4, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 12 * simd_width]);
            w5 = svmad_f64_x(predicate, w5, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 13  * simd_width]);
            w6 = svmad_f64_x(predicate, w6, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 14  * simd_width]);
            w7 = svmad_f64_x(predicate, w7, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 15  * simd_width]);
            w8 = svmad_f64_x(predicate, w8, z0, z0);
            asm volatile (
                "\n\t":: "w" (z0), "w" (z1), "w" (z2), "w" (z3), "w" (z4), 
                         "w" (z5), "w" (z6), "w" (z7), "w" (z8),
                         "w" (w1), "w" (w2), "w" (w3), "w" (w4), 
                         "w" (w5), "w" (w6), "w" (w7), "w" (w8):
            );
        }
    }
} 
#elif defined(FL_RATIO_F2L1)
static const double operation_intensity=0.5;
static const size_t flops_per_cache_line=32;
static void run_kernel_once(int nsize) {
    svbool_t predicate = svwhilelt_b64_s32(0, simd_width);
    svfloat64_t z0, z1, z2, z3, z4, z5, z6, z7, z8, 
                w1, w2, w3, w4, w5, w6, w7, w8;

    INIT_REGISTERS
    
    for (int r = 0; r < repeat; r++) {
        for (int i = 0; i < nsize; i += 8 * simd_width) {
            z0 = svld1_f64(predicate, &a[i]);
            z1 = svmad_f64_x(predicate, z1, z0, z0);
            z2 = svmad_f64_x(predicate, z2, z0, z0);
            z0 = svld1_f64(predicate, &a[i + simd_width]);
            z3 = svmad_f64_x(predicate, z3, z0, z0);
            z4 = svmad_f64_x(predicate, z4, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 2 * simd_width]);
            z5 = svmad_f64_x(predicate, z5, z0, z0);
            z6 = svmad_f64_x(predicate, z6, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 3 * simd_width]);
            z7 = svmad_f64_x(predicate, z7, z0, z0);
            z8 = svmad_f64_x(predicate, z8, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 4 * simd_width]);
            w1 = svmad_f64_x(predicate, w1, z0, z0);
            w2 = svmad_f64_x(predicate, w2, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 5 * simd_width]);
            w3 = svmad_f64_x(predicate, w3, z0, z0);
            w4 = svmad_f64_x(predicate, w4, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 6 * simd_width]);
            w5 = svmad_f64_x(predicate, w5, z0, z0);
            w6 = svmad_f64_x(predicate, w6, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 7 * simd_width]);
            w7 = svmad_f64_x(predicate, w7, z0, z0);
            w8 = svmad_f64_x(predicate, w8, z0, z0);
            asm volatile (
                "\n\t":: "w" (z0), "w" (z1), "w" (z2), "w" (z3), "w" (z4), 
                         "w" (z5), "w" (z6), "w" (z7), "w" (z8),
                         "w" (w1), "w" (w2), "w" (w3), "w" (w4), 
                         "w" (w5), "w" (w6), "w" (w7), "w" (w8):
            );
        }
    }
}
#elif defined(FL_RATIO_F4L1)
static const double operation_intensity=1;
static const size_t flops_per_cache_line=64;
static void run_kernel_once(int nsize) {
    svbool_t predicate = svwhilelt_b64_s32(0, simd_width);
    svfloat64_t z0, z1, z2, z3, z4, z5, z6, z7, z8, 
                w1, w2, w3, w4, w5, w6, w7, w8;

    INIT_REGISTERS
    
    for (int r = 0; r < repeat; r++) {
        for (int i = 0; i < nsize; i += 4 * simd_width) {
            z0 = svld1_f64(predicate, &a[i]);
            z1 = svmad_f64_x(predicate, z1, z0, z0);
            z2 = svmad_f64_x(predicate, z2, z0, z0);
            z3 = svmad_f64_x(predicate, z3, z0, z0);
            z4 = svmad_f64_x(predicate, z4, z0, z0);
            z0 = svld1_f64(predicate, &a[i + simd_width]);
            z5 = svmad_f64_x(predicate, z5, z0, z0);
            z6 = svmad_f64_x(predicate, z6, z0, z0);
            z7 = svmad_f64_x(predicate, z7, z0, z0);
            z8 = svmad_f64_x(predicate, z8, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 2 * simd_width]);
            w1 = svmad_f64_x(predicate, w1, z0, z0);
            w2 = svmad_f64_x(predicate, w2, z0, z0);
            w3 = svmad_f64_x(predicate, w3, z0, z0);
            w4 = svmad_f64_x(predicate, w4, z0, z0);
            z0 = svld1_f64(predicate, &a[i + 3 * simd_width]);
            w5 = svmad_f64_x(predicate, w5, z0, z0);
            w6 = svmad_f64_x(predicate, w6, z0, z0);
            w7 = svmad_f64_x(predicate, w7, z0, z0);
            w8 = svmad_f64_x(predicate, w8, z0, z0);
            asm volatile (
                "\n\t":: "w" (z0), "w" (z1), "w" (z2), "w" (z3), "w" (z4), 
                         "w" (z5), "w" (z6), "w" (z7), "w" (z8),
                         "w" (w1), "w" (w2), "w" (w3), "w" (w4), 
                         "w" (w5), "w" (w6), "w" (w7), "w" (w8):
            );
        }
    }
}
#elif defined(FL_RATIO_F8L1)
static const double operation_intensity=2;
static const size_t flops_per_cache_line=128;
static void run_kernel_once(int nsize) {
    svbool_t predicate = svwhilelt_b64_s32(0, simd_width);
    svfloat64_t z0, z1, z2, z3, z4, z5, z6, z7, z8, 
                w1, w2, w3, w4, w5, w6, w7, w8;

    INIT_REGISTERS
    
    for (int r = 0; r < repeat; r++) {
        for (int i = 0; i < nsize; i += 2 * simd_width) {
            z0 = svld1_f64(predicate, &a[i]);
            z1 = svmad_f64_x(predicate, z1, z0, z0);
            z2 = svmad_f64_x(predicate, z2, z0, z0);
            z3 = svmad_f64_x(predicate, z3, z0, z0);
            z4 = svmad_f64_x(predicate, z4, z0, z0);
            z5 = svmad_f64_x(predicate, z5, z0, z0);
            z6 = svmad_f64_x(predicate, z6, z0, z0);
            z7 = svmad_f64_x(predicate, z7, z0, z0);
            z8 = svmad_f64_x(predicate, z8, z0, z0);
            z0 = svld1_f64(predicate, &a[i + simd_width]);
            w1 = svmad_f64_x(predicate, w1, z0, z0);
            w2 = svmad_f64_x(predicate, w2, z0, z0);
            w3 = svmad_f64_x(predicate, w3, z0, z0);
            w4 = svmad_f64_x(predicate, w4, z0, z0);
            w5 = svmad_f64_x(predicate, w5, z0, z0);
            w6 = svmad_f64_x(predicate, w6, z0, z0);
            w7 = svmad_f64_x(predicate, w7, z0, z0);
            w8 = svmad_f64_x(predicate, w8, z0, z0);
            asm volatile (
                "\n\t":: "w" (z0), "w" (z1), "w" (z2), "w" (z3), "w" (z4), 
                         "w" (z5), "w" (z6), "w" (z7), "w" (z8),
                         "w" (w1), "w" (w2), "w" (w3), "w" (w4), 
                         "w" (w5), "w" (w6), "w" (w7), "w" (w8):
            );
        }
    }
}
#endif
#else // #ifdef KP_SVE
const char *SIMD_NAME = "NEON";
#define INIT_REGISTERS  \
    z1 = vdupq_n_f64(1.23); \
    z2 = vdupq_n_f64(1.23); \
    z3 = vdupq_n_f64(1.23); \
    z4 = vdupq_n_f64(1.23); \
    z5 = vdupq_n_f64(1.23); \
    z6 = vdupq_n_f64(1.23); \
    z7 = vdupq_n_f64(1.23); \
    z8 = vdupq_n_f64(1.23); \
    w1 = vdupq_n_f64(1.23); \
    w2 = vdupq_n_f64(1.23); \
    w3 = vdupq_n_f64(1.23); \
    w4 = vdupq_n_f64(1.23); \
    w5 = vdupq_n_f64(1.23); \
    w6 = vdupq_n_f64(1.23); \
    w7 = vdupq_n_f64(1.23); \
    w8 = vdupq_n_f64(1.23);

#if defined(FL_RATIO_F1L4)
static const double operation_intensity=0.25 / 4;
static const size_t flops_per_cache_line=16 / 4;
static void run_kernel_once(int nsize) {
    float64x2_t z0, z1, z2, z3, z4, z5, z6, z7, z8, 
                w1, w2, w3, w4, w5, w6, w7, w8;

    INIT_REGISTERS
    
    for (int r = 0; r < repeat; r++) {
        for (int i = 0; i < nsize; i += 32) { // 处理4x8=32个double元素
            z0 = vld1q_f64(&a[i]);
            z1 = vld1q_f64(&a[i + 2]);
            z2 = vld1q_f64(&a[i + 4]);
            z3 = vld1q_f64(&a[i + 6]);
            z4 = vfmaq_f64(z4, z0, z0);
            
            z0 = vld1q_f64(&a[i + 8]);
            z5 = vld1q_f64(&a[i + 10]);
            z6 = vld1q_f64(&a[i + 12]);
            z7 = vld1q_f64(&a[i + 14]);
            z8 = vfmaq_f64(z8, z0, z0);
            
            z0 = vld1q_f64(&a[i + 16]);
            w1 = vld1q_f64(&a[i + 18]);
            w2 = vld1q_f64(&a[i + 20]);
            w3 = vld1q_f64(&a[i + 22]);
            w4 = vfmaq_f64(w4, z0, z0);
            
            z0 = vld1q_f64(&a[i + 24]);
            w5 = vld1q_f64(&a[i + 26]);
            w6 = vld1q_f64(&a[i + 28]);
            w7 = vld1q_f64(&a[i + 30]);
            w8 = vfmaq_f64(w8, z0, z0);

            asm volatile (
                "\n\t":: "w" (z0), "w" (z1), "w" (z2), "w" (z3), "w" (z4),
                        "w" (z5), "w" (z6), "w" (z7), "w" (z8),
                        "w" (w1), "w" (w2), "w" (w3), "w" (w4),
                        "w" (w5), "w" (w6), "w" (w7), "w" (w8):
            );
        }
    }
} 
#elif defined(FL_RATIO_F1L2)
static const double operation_intensity=0.25 / 2;
static const size_t flops_per_cache_line=16 / 2;
static void run_kernel_once(int nsize) {
    float64x2_t z0, z1, z2, z3, z4, z5, z6, z7, z8, 
                w1, w2, w3, w4, w5, w6, w7, w8;

    INIT_REGISTERS
    
    for (int r = 0; r < repeat; r++) {
        for (int i = 0; i < nsize; i += 32) {
            z0 = vld1q_f64(&a[i]);
            z1 = vld1q_f64(&a[i + 2]);
            z2 = vfmaq_f64(z2, z0, z0);
            z0 = vld1q_f64(&a[i + 4]);
            z3 = vld1q_f64(&a[i + 6]);
            z4 = vfmaq_f64(z4, z0, z0);
            z0 = vld1q_f64(&a[i + 8]);
            z5 = vld1q_f64(&a[i + 10]);
            z6 = vfmaq_f64(z6, z0, z0);
            z0 = vld1q_f64(&a[i + 12]);
            z7 = vld1q_f64(&a[i + 14]);
            z8 = vfmaq_f64(z8, z0, z0);

            z0 = vld1q_f64(&a[i + 16]);
            w1 = vld1q_f64(&a[i + 18]);
            w2 = vfmaq_f64(w2, z0, z0);
            z0 = vld1q_f64(&a[i + 20]);
            w3 = vld1q_f64(&a[i + 22]);
            w4 = vfmaq_f64(w4, z0, z0);
            z0 = vld1q_f64(&a[i + 24]);
            w5 = vld1q_f64(&a[i + 26]);
            w6 = vfmaq_f64(w6, z0, z0);
            z0 = vld1q_f64(&a[i + 28]);
            w7 = vld1q_f64(&a[i + 30]);
            w8 = vfmaq_f64(w8, z0, z0);
            asm volatile (
                "\n\t":: "w" (z0), "w" (z1), "w" (z2), "w" (z3), "w" (z4), 
                         "w" (z5), "w" (z6), "w" (z7), "w" (z8),
                         "w" (w1), "w" (w2), "w" (w3), "w" (w4), 
                         "w" (w5), "w" (w6), "w" (w7), "w" (w8):
            );
        }
    }
} 
#elif defined(FL_RATIO_F1L1)
static const double operation_intensity=0.25;
static const size_t flops_per_cache_line=16;
static void run_kernel_once(int nsize) {
    float64x2_t z0, z1, z2, z3, z4, z5, z6, z7, z8,
            w1, w2, w3, w4, w5, w6, w7, w8;

    INIT_REGISTERS

    for (int r = 0; r < repeat; r++) {
        for (int i = 0; i < nsize; i += 32) { // NEON每次处理2个double
            z0 = vld1q_f64(&a[i]);
            z1 = vfmaq_f64(z1, z0, z0);
            
            z0 = vld1q_f64(&a[i + 2]);
            z2 = vfmaq_f64(z2, z0, z0);
            
            z0 = vld1q_f64(&a[i + 4]);
            z3 = vfmaq_f64(z3, z0, z0);
            
            z0 = vld1q_f64(&a[i + 6]);
            z4 = vfmaq_f64(z4, z0, z0);
            
            z0 = vld1q_f64(&a[i + 8]);
            z5 = vfmaq_f64(z5, z0, z0);
            
            z0 = vld1q_f64(&a[i + 10]);
            z6 = vfmaq_f64(z6, z0, z0);
            
            z0 = vld1q_f64(&a[i + 12]);
            z7 = vfmaq_f64(z7, z0, z0);
            
            z0 = vld1q_f64(&a[i + 14]);
            z8 = vfmaq_f64(z8, z0, z0);
            
            z0 = vld1q_f64(&a[i + 16]);
            w1 = vfmaq_f64(w1, z0, z0);
            
            z0 = vld1q_f64(&a[i + 18]);
            w2 = vfmaq_f64(w2, z0, z0);
            
            z0 = vld1q_f64(&a[i + 20]);
            w3 = vfmaq_f64(w3, z0, z0);
            
            z0 = vld1q_f64(&a[i + 22]);
            w4 = vfmaq_f64(w4, z0, z0);
            
            z0 = vld1q_f64(&a[i + 24]);
            w5 = vfmaq_f64(w5, z0, z0);
            
            z0 = vld1q_f64(&a[i + 26]);
            w6 = vfmaq_f64(w6, z0, z0);
            
            z0 = vld1q_f64(&a[i + 28]);
            w7 = vfmaq_f64(w7, z0, z0);
            
            z0 = vld1q_f64(&a[i + 30]);
            w8 = vfmaq_f64(w8, z0, z0);

            asm volatile (
                "\n\t":: "w" (z0), "w" (z1), "w" (z2), "w" (z3), "w" (z4),
                        "w" (z5), "w" (z6), "w" (z7), "w" (z8),
                        "w" (w1), "w" (w2), "w" (w3), "w" (w4),
                        "w" (w5), "w" (w6), "w" (w7), "w" (w8):
            );
        }
    }
} 
#elif defined(FL_RATIO_F2L1)
static const double operation_intensity=0.5;
static const size_t flops_per_cache_line=32;
static void run_kernel_once(int nsize) {
    float64x2_t z0, z1, z2, z3, z4, z5, z6, z7, z8, 
                w1, w2, w3, w4, w5, w6, w7, w8;

    INIT_REGISTERS
    
    for (int r = 0; r < repeat; r++) {
        for (int i = 0; i < nsize; i += 16) {
            z0 = vld1q_f64(&a[i]);
            z1 = vfmaq_f64(z1, z0, z0);
            z2 = vfmaq_f64(z2, z0, z0);
            
            z0 = vld1q_f64(&a[i + 2]);
            z3 = vfmaq_f64(z3, z0, z0);
            z4 = vfmaq_f64(z4, z0, z0);
            
            z0 = vld1q_f64(&a[i + 4]);
            z5 = vfmaq_f64(z5, z0, z0);
            z6 = vfmaq_f64(z6, z0, z0);
            
            z0 = vld1q_f64(&a[i + 6]);
            z7 = vfmaq_f64(z7, z0, z0);
            z8 = vfmaq_f64(z8, z0, z0);
            
            z0 = vld1q_f64(&a[i + 8]);
            w1 = vfmaq_f64(w1, z0, z0);
            w2 = vfmaq_f64(w2, z0, z0);
            
            z0 = vld1q_f64(&a[i + 10]);
            w3 = vfmaq_f64(w3, z0, z0);
            w4 = vfmaq_f64(w4, z0, z0);
            
            z0 = vld1q_f64(&a[i + 12]);
            w5 = vfmaq_f64(w5, z0, z0);
            w6 = vfmaq_f64(w6, z0, z0);
            
            z0 = vld1q_f64(&a[i + 14]);
            w7 = vfmaq_f64(w7, z0, z0);
            w8 = vfmaq_f64(w8, z0, z0);
            asm volatile (
                "\n\t":: "w" (z0), "w" (z1), "w" (z2), "w" (z3), "w" (z4), 
                         "w" (z5), "w" (z6), "w" (z7), "w" (z8),
                         "w" (w1), "w" (w2), "w" (w3), "w" (w4), 
                         "w" (w5), "w" (w6), "w" (w7), "w" (w8):
            );
        }
    }
}
#elif defined(FL_RATIO_F4L1)
static const double operation_intensity=1;
static const size_t flops_per_cache_line=64;
static void run_kernel_once(int nsize) {
    float64x2_t z0, z1, z2, z3, z4, z5, z6, z7, z8, 
                w1, w2, w3, w4, w5, w6, w7, w8;

    INIT_REGISTERS
    
    for (int r = 0; r < repeat; r++) {
        for (int i = 0; i < nsize; i += 8) {
            z0 = vld1q_f64(&a[i]);
            z1 = vfmaq_f64(z1, z0, z0);
            z2 = vfmaq_f64(z2, z0, z0);
            z3 = vfmaq_f64(z3, z0, z0);
            z4 = vfmaq_f64(z4, z0, z0);
            
            z0 = vld1q_f64(&a[i + 2]);
            z5 = vfmaq_f64(z5, z0, z0);
            z6 = vfmaq_f64(z6, z0, z0);
            z7 = vfmaq_f64(z7, z0, z0);
            z8 = vfmaq_f64(z8, z0, z0);
            
            z0 = vld1q_f64(&a[i + 4]);
            w1 = vfmaq_f64(w1, z0, z0);
            w2 = vfmaq_f64(w2, z0, z0);
            w3 = vfmaq_f64(w3, z0, z0);
            w4 = vfmaq_f64(w4, z0, z0);
            
            z0 = vld1q_f64(&a[i + 6]);
            w5 = vfmaq_f64(w5, z0, z0);
            w6 = vfmaq_f64(w6, z0, z0);
            w7 = vfmaq_f64(w7, z0, z0);
            w8 = vfmaq_f64(w8, z0, z0);
            asm volatile (
                "\n\t":: "w" (z0), "w" (z1), "w" (z2), "w" (z3), "w" (z4), 
                         "w" (z5), "w" (z6), "w" (z7), "w" (z8),
                         "w" (w1), "w" (w2), "w" (w3), "w" (w4), 
                         "w" (w5), "w" (w6), "w" (w7), "w" (w8):
            );
        }
    }
}
#elif defined(FL_RATIO_F8L1)
static const double operation_intensity=2;
static const size_t flops_per_cache_line=128;
static void run_kernel_once(int nsize) {
    float64x2_t z0, z1, z2, z3, z4, z5, z6, z7, z8, 
                w1, w2, w3, w4, w5, w6, w7, w8;

    INIT_REGISTERS
    
    for (int r = 0; r < repeat; r++) {
        for (int i = 0; i < nsize; i += 4) {
            z0 = vld1q_f64(&a[i]);
            z1 = vfmaq_f64(z1, z0, z0);
            z2 = vfmaq_f64(z2, z0, z0);
            z3 = vfmaq_f64(z3, z0, z0);
            z4 = vfmaq_f64(z4, z0, z0);
            z5 = vfmaq_f64(z5, z0, z0);
            z6 = vfmaq_f64(z6, z0, z0);
            z7 = vfmaq_f64(z7, z0, z0);
            z8 = vfmaq_f64(z8, z0, z0);
            
            z0 = vld1q_f64(&a[i + 2]);
            w1 = vfmaq_f64(w1, z0, z0);
            w2 = vfmaq_f64(w2, z0, z0);
            w3 = vfmaq_f64(w3, z0, z0);
            w4 = vfmaq_f64(w4, z0, z0);
            w5 = vfmaq_f64(w5, z0, z0);
            w6 = vfmaq_f64(w6, z0, z0);
            w7 = vfmaq_f64(w7, z0, z0);
            w8 = vfmaq_f64(w8, z0, z0);
            asm volatile (
                "\n\t":: "w" (z0), "w" (z1), "w" (z2), "w" (z3), "w" (z4), 
                         "w" (z5), "w" (z6), "w" (z7), "w" (z8),
                         "w" (w1), "w" (w2), "w" (w3), "w" (w4), 
                         "w" (w5), "w" (w6), "w" (w7), "w" (w8):
            );
        }
    }
}
#endif
#endif // #ifdef KP_SVE
#endif // #ifdef __aarch64__

#ifdef __x86_64__
#ifdef AVX512
const char *SIMD_NAME = "AVX-512";
#if defined(FL_RATIO_F1L4)
static const double operation_intensity=0.25 / 4.0;
static const size_t flops_per_cache_line=16 / 4.0;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 8
        for (int i = 0; i < nsize; i += 64) {
            asm volatile (
                "vmovapd %[a_i0], %%zmm0\n\t"
                "vmovapd %[a_i1], %%zmm0\n\t"
                "vmovapd %[a_i2], %%zmm0\n\t"
                "vmovapd %[a_i3], %%zmm0\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm4\n\t"
                "vmovapd %[a_i4], %%zmm0\n\t"
                "vmovapd %[a_i5], %%zmm0\n\t"
                "vmovapd %[a_i6], %%zmm0\n\t"
                "vmovapd %[a_i7], %%zmm0\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm8\n\t"
                :: [a_i0] "m" (a[i]), 
                   [a_i1] "m" (a[i + 8]), 
                   [a_i2] "m" (a[i + 16]),
                   [a_i3] "m" (a[i + 24]),
                   [a_i4] "m" (a[i + 32]),
                   [a_i5] "m" (a[i + 40]),
                   [a_i6] "m" (a[i + 48]),
                   [a_i7] "m" (a[i + 56])
                : "%zmm0", "%zmm1", "%zmm2", "%zmm3", 
                  "%zmm4", "%zmm5", "%zmm6", "%zmm7", "%zmm8"  
            );
        }
    }
}
#elif defined(FL_RATIO_F1L2)
static const double operation_intensity=0.25 / 2.0;
static const size_t flops_per_cache_line=16 / 2.0;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 8
        for (int i = 0; i < nsize; i += 64) {
            asm volatile (
                "vmovapd %[a_i0], %%zmm0\n\t"
                "vmovapd %[a_i1], %%zmm0\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm2\n\t"
                "vmovapd %[a_i2], %%zmm0\n\t"
                "vmovapd %[a_i3], %%zmm0\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm4\n\t"
                "vmovapd %[a_i4], %%zmm0\n\t"
                "vmovapd %[a_i5], %%zmm0\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm6\n\t"
                "vmovapd %[a_i6], %%zmm0\n\t"
                "vmovapd %[a_i7], %%zmm0\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm8\n\t"
                :: [a_i0] "m" (a[i]), 
                   [a_i1] "m" (a[i + 8]), 
                   [a_i2] "m" (a[i + 16]),
                   [a_i3] "m" (a[i + 24]),
                   [a_i4] "m" (a[i + 32]),
                   [a_i5] "m" (a[i + 40]),
                   [a_i6] "m" (a[i + 48]),
                   [a_i7] "m" (a[i + 56])
                : "%zmm0", "%zmm1", "%zmm2", "%zmm3", 
                  "%zmm4", "%zmm5", "%zmm6", "%zmm7", "%zmm8"  
            );
        }
    }
}
#elif defined(FL_RATIO_F1L1)
static const double operation_intensity=0.25;
static const size_t flops_per_cache_line=16;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 64) {
            asm volatile (
                "vmovapd %[a_i0], %%zmm0\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmovapd %[a_i1], %%zmm0\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm2\n\t"
                "vmovapd %[a_i2], %%zmm0\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm3\n\t"
                "vmovapd %[a_i3], %%zmm0\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm4\n\t"
                "vmovapd %[a_i4], %%zmm0\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm5\n\t"
                "vmovapd %[a_i5], %%zmm0\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm6\n\t"
                "vmovapd %[a_i6], %%zmm0\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm7\n\t"
                "vmovapd %[a_i7], %%zmm0\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm8\n\t"
                :: [a_i0] "m" (a[i]), 
                   [a_i1] "m" (a[i + 8]), 
                   [a_i2] "m" (a[i + 16]),
                   [a_i3] "m" (a[i + 24]),
                   [a_i4] "m" (a[i + 32]),
                   [a_i5] "m" (a[i + 40]),
                   [a_i6] "m" (a[i + 48]),
                   [a_i7] "m" (a[i + 56])
                : "%zmm0", "%zmm1", "%zmm2", "%zmm3", 
                  "%zmm4", "%zmm5", "%zmm6", "%zmm7", "%zmm8"  
            );
        }
    }
}
#elif defined(FL_RATIO_F2L1)
static const double operation_intensity=0.5;
static const size_t flops_per_cache_line=32;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 32) {
            asm volatile (
                "vmovapd %[a_i0], %%zmm0\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm2\n\t"
                "vmovapd %[a_i1], %%zmm0\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm3\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm4\n\t"
                "vmovapd %[a_i2], %%zmm0\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm5\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm6\n\t"
                "vmovapd %[a_i3], %%zmm0\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm7\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm8\n\t"
                :: [a_i0] "m" (a[i]), 
                   [a_i1] "m" (a[i + 8]), 
                   [a_i2] "m" (a[i + 16]),
                   [a_i3] "m" (a[i + 24])
                : "%zmm0", "%zmm1", "%zmm2", "%zmm3", 
                  "%zmm4", "%zmm5", "%zmm6", "%zmm7", "%zmm8"  
            );
        }
    }
}
#elif defined(FL_RATIO_F4L1)
static const double operation_intensity=1;
static const size_t flops_per_cache_line=64;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 16) {
            asm volatile (
                "vmovapd %[a_i0], %%zmm0\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm2\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm3\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm4\n\t"
                "vmovapd %[a_i1], %%zmm0\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm5\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm6\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm7\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm8\n\t"
                :: [a_i0] "m" (a[i]), 
                   [a_i1] "m" (a[i + 8])
                : "%zmm0", "%zmm1", "%zmm2", "%zmm3", 
                  "%zmm4", "%zmm5", "%zmm6", "%zmm7", "%zmm8"  
            );
        }
    }
}
#elif defined(FL_RATIO_F8L1)
static const double operation_intensity=2;
static const size_t flops_per_cache_line=128;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 8) {
            asm volatile (
                "vmovapd %[a_i0], %%zmm0\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm2\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm3\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm4\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm5\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm6\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm7\n\t"
                "vfmadd231pd %%zmm0, %%zmm0, %%zmm8\n\t"
                :: [a_i0] "m" (a[i])
                : "%zmm0", "%zmm1", "%zmm2", "%zmm3", 
                  "%zmm4", "%zmm5", "%zmm6", "%zmm7", "%zmm8"  
            );
        }
    }
}
#else 
#include "FL_RATIO_MACRO_ERROR_NOT_IDENTIFIED.h"
#endif // FL_RATIO_FxLx

#else // #ifdef AVX512

const char *SIMD_NAME = "AVX-256";
#if defined(FL_RATIO_F1L4)
static const double operation_intensity=0.25 / 4;
static const size_t flops_per_cache_line=16 / 4;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 16
        for (int i = 0; i < nsize; i += 32) {
            asm volatile (
                "vmovapd %[a_i0], %%ymm0\n\t"
                "vmovapd %[a_i1], %%ymm0\n\t"
                "vmovapd %[a_i2], %%ymm0\n\t"
                "vmovapd %[a_i3], %%ymm0\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm4\n\t"
                "vmovapd %[a_i4], %%ymm0\n\t"
                "vmovapd %[a_i5], %%ymm0\n\t"
                "vmovapd %[a_i6], %%ymm0\n\t"
                "vmovapd %[a_i7], %%ymm0\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm8\n\t"
                :: [a_i0] "m" (a[i]), 
                   [a_i1] "m" (a[i + 4]), 
                   [a_i2] "m" (a[i + 8]),
                   [a_i3] "m" (a[i + 12]),
                   [a_i4] "m" (a[i + 16]),
                   [a_i5] "m" (a[i + 20]),
                   [a_i6] "m" (a[i + 24]),
                   [a_i7] "m" (a[i + 28])
                : "%ymm0", "%ymm1", "%ymm2", "%ymm3", 
                  "%ymm4", "%ymm5", "%ymm6", "%ymm7", "%ymm8"  
            );
        }
    }
}
#elif defined(FL_RATIO_F1L2)
static const double operation_intensity=0.25 / 2;
static const size_t flops_per_cache_line=16 / 2;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 16
        for (int i = 0; i < nsize; i += 32) {
            asm volatile (
                "vmovapd %[a_i0], %%ymm0\n\t"
                "vmovapd %[a_i1], %%ymm0\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm2\n\t"
                "vmovapd %[a_i2], %%ymm0\n\t"
                "vmovapd %[a_i3], %%ymm0\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm4\n\t"
                "vmovapd %[a_i4], %%ymm0\n\t"
                "vmovapd %[a_i5], %%ymm0\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm6\n\t"
                "vmovapd %[a_i6], %%ymm0\n\t"
                "vmovapd %[a_i7], %%ymm0\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm8\n\t"
                :: [a_i0] "m" (a[i]), 
                   [a_i1] "m" (a[i + 4]), 
                   [a_i2] "m" (a[i + 8]),
                   [a_i3] "m" (a[i + 12]),
                   [a_i4] "m" (a[i + 16]),
                   [a_i5] "m" (a[i + 20]),
                   [a_i6] "m" (a[i + 24]),
                   [a_i7] "m" (a[i + 28])
                : "%ymm0", "%ymm1", "%ymm2", "%ymm3", 
                  "%ymm4", "%ymm5", "%ymm6", "%ymm7", "%ymm8"  
            );
        }
    }
}
#elif defined(FL_RATIO_F1L1)
static const double operation_intensity=0.25;
static const size_t flops_per_cache_line=16;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 32) {
            asm volatile (
                "vmovapd %[a_i0], %%ymm0\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmovapd %[a_i1], %%ymm0\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm2\n\t"
                "vmovapd %[a_i2], %%ymm0\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm3\n\t"
                "vmovapd %[a_i3], %%ymm0\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm4\n\t"
                "vmovapd %[a_i4], %%ymm0\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm5\n\t"
                "vmovapd %[a_i5], %%ymm0\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm6\n\t"
                "vmovapd %[a_i6], %%ymm0\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm7\n\t"
                "vmovapd %[a_i7], %%ymm0\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm8\n\t"
                :: [a_i0] "m" (a[i]), 
                   [a_i1] "m" (a[i + 4]), 
                   [a_i2] "m" (a[i + 8]),
                   [a_i3] "m" (a[i + 12]),
                   [a_i4] "m" (a[i + 16]),
                   [a_i5] "m" (a[i + 20]),
                   [a_i6] "m" (a[i + 24]),
                   [a_i7] "m" (a[i + 28])
                : "%ymm0", "%ymm1", "%ymm2", "%ymm3", 
                  "%ymm4", "%ymm5", "%ymm6", "%ymm7", "%ymm8"  
            );
        }
    }
}
#elif defined(FL_RATIO_F2L1)
static const double operation_intensity=0.5;
static const size_t flops_per_cache_line=32;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 16) {
            asm volatile (
                "vmovapd %[a_i0], %%ymm0\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm2\n\t"
                "vmovapd %[a_i1], %%ymm0\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm3\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm4\n\t"
                "vmovapd %[a_i2], %%ymm0\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm5\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm6\n\t"
                "vmovapd %[a_i3], %%ymm0\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm7\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm8\n\t"
                :: [a_i0] "m" (a[i]), 
                   [a_i1] "m" (a[i + 4]), 
                   [a_i2] "m" (a[i + 8]),
                   [a_i3] "m" (a[i + 12])
                : "%ymm0", "%ymm1", "%ymm2", "%ymm3", 
                  "%ymm4", "%ymm5", "%ymm6", "%ymm7", "%ymm8"  
            );
        }
    }
}
#elif defined(FL_RATIO_F4L1)
static const double operation_intensity=1;
static const size_t flops_per_cache_line=64;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 8) {
            asm volatile (
                "vmovapd %[a_i0], %%ymm0\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm2\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm3\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm4\n\t"
                "vmovapd %[a_i1], %%ymm0\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm5\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm6\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm7\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm8\n\t"
                :: [a_i0] "m" (a[i]), 
                   [a_i1] "m" (a[i + 4])
                : "%ymm0", "%ymm1", "%ymm2", "%ymm3", 
                  "%ymm4", "%ymm5", "%ymm6", "%ymm7", "%ymm8"  
            );
        }
    }
}
#elif defined(FL_RATIO_F8L1)
static const double operation_intensity=2;
static const size_t flops_per_cache_line=128;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 4) {
            asm volatile (
                "vmovapd %[a_i0], %%ymm0\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm2\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm3\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm4\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm5\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm6\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm7\n\t"
                "vfmadd231pd %%ymm0, %%ymm0, %%ymm8\n\t"
                :: [a_i0] "m" (a[i])
                : "%ymm0", "%ymm1", "%ymm2", "%ymm3", 
                  "%ymm4", "%ymm5", "%ymm6", "%ymm7", "%ymm8"  
            );
        }
    }
}
#else 
#include "FL_RATIO_MACRO_ERROR_NOT_IDENTIFIED.h"
#endif

#endif // #ifdef AVX512
#endif // #ifdef __x86_64__

int
d_fmaldr(int ntest, uint64_t *ns, uint64_t *cy, uint64_t kib, ...) {
    int nsize, err;

    nsize = kib * 1024 / sizeof(double);
    nsize = ((nsize + 7) / 8) * 8;
    kib = nsize * sizeof(double) / 1024;
    repeat = ((size_t) 1e8) / (nsize / 8) / MAX(1, (operation_intensity / 0.125));
    repeat = MAX(repeat, 1);

#ifdef __aarch64__
#ifdef KP_SVE
    simd_width = svcntd();
#endif
#endif

    tpprintf(0, 0, 0, "SIMD Instr: %s\n", SIMD_NAME);
    tpprintf(0, 0, 0, "Working set size: %dKB.\n", kib);
    tpprintf(0, 0, 0, "repeat times: %lu.\n", repeat);
    tpprintf(0, 0, 0, "Operation intensity: %f.\n", operation_intensity);

    init_kernel_data(nsize);

    // kernel warm
    struct timespec wts;
    uint64_t wns0, wns1;
    __getns(wts, wns1);
    wns0 = wns1 + 1e9;
    while(wns1 < wns0) {
        run_kernel_once(nsize);
        __getns(wts, wns1);
    }

    __getcy_init;
    __getns_init;

    // kernel start
    for(int i = 0; i < ntest; i ++){
        tpmpi_dbarrier();
        __getns_1d_st(i);
        __getcy_1d_st(i);

        run_kernel_once(nsize);

        __getcy_1d_en(i);
        __getns_1d_en(i);
    }
    // kernel end

    // overall result
    int nskip = 0, freq=1;
    dpipe_k0(ns, cy, nskip, ntest, freq, flops_per_cache_line, repeat * nsize / 8);
    
    free_kernel_data();

    return 0;
}
