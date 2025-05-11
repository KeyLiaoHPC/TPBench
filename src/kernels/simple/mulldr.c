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

#ifdef KP_SVE
#include <arm_sve.h>
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
// #define FL_RATIO_F3L1 1
// #define FL_RATIO_F4L1 1
// #define FL_RATIO_F8L1 1
// #define FL_RATIO_F16L1 1
// #define FL_RATIO_F32L1 1

#ifdef __aarch64__
#ifdef KP_SVE
extern int simd_width;
// extern const char *SIMD_NAME;
#if defined(FL_RATIO_F1L4)
static const double operation_intensity=0.125 / 4;
static const size_t flops_per_cache_line=8 / 4;
static void run_kernel_once(int nsize) {
    svbool_t predicate = svwhilelt_b64_s32(0, 8);
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 16
        for (int i = 0; i < nsize; i += 4*simd_width) {
            svfloat64_t reg0 = svld1_f64(predicate, &a[i]);
            svfloat64_t reg1 = svld1_f64(predicate, &a[i + simd_width]);
            svfloat64_t reg2 = svld1_f64(predicate, &a[i + 2*simd_width]);
            svfloat64_t reg3 = svld1_f64(predicate, &a[i + 4*simd_width]);
            asm volatile (
                "fmul z1.d, %[reg0].d, %[reg0].d\n\t"
                :: [reg0] "w" (reg0), [reg1] "w" (reg1), [reg2] "w" (reg2), [reg3] "w" (reg3)
                : "z1"
            );
        }
    }
}
#elif defined(FL_RATIO_F1L2)
static const double operation_intensity=0.125 / 2;
static const size_t flops_per_cache_line=8 / 2;
static void run_kernel_once(int nsize) {
    svbool_t predicate = svwhilelt_b64_s32(0, 8);
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 2*simd_width) {
            svfloat64_t reg0 = svld1_f64(predicate, &a[i]);
            svfloat64_t reg1 = svld1_f64(predicate, &a[i + simd_width]);
            asm volatile (
                "fmul z1.d, %[reg0].d, %[reg1].d\n\t"
                :: [reg0] "w" (reg0), [reg1] "w" (reg1)
                : "z1"
            );
        }
    }
}
#elif defined(FL_RATIO_F1L1)
static const double operation_intensity=0.125;
static const size_t flops_per_cache_line=8;
static void run_kernel_once(int nsize) {
    svbool_t predicate = svwhilelt_b64_s32(0, simd_width);
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += simd_width) {
            svfloat64_t reg = svld1_f64(predicate, &a[i]);
            asm volatile (
                "fmul z1.d, %[reg].d, %[reg].d\n\t"
                :: [reg] "w" (reg)
                : "z1"
            );
        }
    }
}
#elif defined(FL_RATIO_F2L1)
static const double operation_intensity=0.25;
static const size_t flops_per_cache_line=16;
static void run_kernel_once(int nsize) {
    svbool_t predicate = svwhilelt_b64_s32(0, simd_width);
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += simd_width) {
            svfloat64_t reg = svld1_f64(predicate, &a[i]);
            asm volatile (
                "fmul z1.d, %[reg].d, %[reg].d\n\t"
                "fmul z1.d, %[reg].d, %[reg].d\n\t"
                :: [reg] "w" (reg)
                : "z1"
            );
        }
    }
}
#elif defined(FL_RATIO_F4L1)
static const double operation_intensity=0.5;
static const size_t flops_per_cache_line=32;
static void run_kernel_once(int nsize) {
    svbool_t predicate = svwhilelt_b64_s32(0, simd_width);
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += simd_width) {
            svfloat64_t reg = svld1_f64(predicate, &a[i]);
            asm volatile (
                "fmul z1.d, %[reg].d, %[reg].d\n\t"
                "fmul z1.d, %[reg].d, %[reg].d\n\t"
                "fmul z1.d, %[reg].d, %[reg].d\n\t"
                "fmul z1.d, %[reg].d, %[reg].d\n\t"
                :: [reg] "w" (reg)
                : "z1"
            );
        }
    }
}
#elif defined(FL_RATIO_F8L1)
static const double operation_intensity=1;
static const size_t flops_per_cache_line=64;
static void run_kernel_once(int nsize) {
    svbool_t predicate = svwhilelt_b64_s32(0, simd_width);
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += simd_width) {
            svfloat64_t reg = svld1_f64(predicate, &a[i]);
            asm volatile (
                DUP_8("fmul z1.d, %[reg].d, %[reg].d\n\t")
                :: [reg] "w" (reg)
                : "z1"
            );
        }
    }
}
#elif defined(FL_RATIO_F16L1)
static const double operation_intensity=2;
static const size_t flops_per_cache_line=128;
static void run_kernel_once(int nsize) {
    svbool_t predicate = svwhilelt_b64_s32(0, simd_width);
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += simd_width) {
            svfloat64_t reg = svld1_f64(predicate, &a[i]);
            asm volatile (
                DUP_16("fmul z1.d, %[reg].d, %[reg].d\n\t")
                :: [reg] "w" (reg)
                : "z1"
            );
        }
    }
}
#elif defined(FL_RATIO_F32L1)
static const double operation_intensity=4;
static const size_t flops_per_cache_line=256;
static void run_kernel_once(int nsize) {
    svbool_t predicate = svwhilelt_b64_s32(0, simd_width);
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += simd_width) {
            svfloat64_t reg = svld1_f64(predicate, &a[i]);
            asm volatile (
                DUP_32("fmul z1.d, %[reg].d, %[reg].d\n\t")
                :: [reg] "w" (reg)
                : "z1"
            );
        }
    }
}
#endif
#else // #ifdef KP_SVE
#if defined(FL_RATIO_F1L4)
static const double operation_intensity=1 / 8.0 / 4.0;
static const size_t flops_per_cache_line=2;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 100
        for (int i = 0; i < nsize; i += 8) {
            asm volatile (
                "ldr q0, %[a_i0]\n\t"
                "ldr q0, %[a_i1]\n\t"
                "ldr q0, %[a_i2]\n\t"
                "ldr q0, %[a_i3]\n\t"
                "fmul v1.2d, v0.2d, v0.2d\n\t"
                :: [a_i0] "m" (a[i]),
                   [a_i1] "m" (a[i + 2]),
                   [a_i2] "m" (a[i + 4]),
                   [a_i3] "m" (a[i + 8])
                : "v0", "v1", "q0"
            );
        }
    }
}
#elif defined(FL_RATIO_F1L2)
static const double operation_intensity=1 / 8.0 / 2.0;
static const size_t flops_per_cache_line=4;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 100
        for (int i = 0; i < nsize; i += 4) {
            asm volatile (
                "ldr q0, %[a_i0]\n\t"
                "ldr q0, %[a_i1]\n\t"
                "fmul v1.2d, v0.2d, v0.2d\n\t"
                ::  [a_i0] "m" (a[i]),
                    [a_i1] "m" (a[i + 2])
                : "v0", "v1", "q0"
            );
        }
    }
}
#elif defined(FL_RATIO_F1L1)
static const double operation_intensity=0.125;
static const size_t flops_per_cache_line=8;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 100
        for (int i = 0; i < nsize; i += 2) {
            asm volatile (
                "ldr q0, %[a_i]\n\t"
                "fmul v1.2d, v0.2d, v0.2d\n\t"
                :: [a_i] "m" (a[i]): "v0", "v1", "q0"
            );
        }
    }
}
#elif defined(FL_RATIO_F2L1)
static const double operation_intensity=0.25;
static const size_t flops_per_cache_line=16;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 2) {
            asm volatile (
                "ldr q0, %[a_i]\n\t"
                "fmul v1.2d, v0.2d, v0.2d\n\t"
                "fmul v1.2d, v0.2d, v0.2d\n\t"
                :: [a_i] "m" (a[i]): "v0", "v1", "q0"
            );
        }
    }
}
#elif defined(FL_RATIO_F3L1)
static const double operation_intensity=0.375;
static const size_t flops_per_cache_line=24;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 2) {
            asm volatile (
                "ldr q0, %[a_i]\n\t"
                "fmul v1.2d, v0.2d, v0.2d\n\t"
                "fmul v1.2d, v0.2d, v0.2d\n\t"
                "fmul v1.2d, v0.2d, v0.2d\n\t"
                :: [a_i] "m" (a[i]): "v0", "v1", "q0"
            );
        }
    }
}
#elif defined(FL_RATIO_F4L1)
static const double operation_intensity=0.5;
static const size_t flops_per_cache_line=32;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 2) {
            asm volatile (
                "ldr q0, %[a_i]\n\t"
                "fmul v1.2d, v0.2d, v0.2d\n\t"
                "fmul v1.2d, v0.2d, v0.2d\n\t"
                "fmul v1.2d, v0.2d, v0.2d\n\t"
                "fmul v1.2d, v0.2d, v0.2d\n\t"
                :: [a_i] "m" (a[i]): "v0", "v1", "q0"
            );
        }
    }
}
#elif defined(FL_RATIO_F8L1)
static const double operation_intensity=1;
static const size_t flops_per_cache_line=64;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 2) {
            asm volatile (
                "ldr q0, %[a_i]\n\t"
                DUP_8("fmul v1.2d, v0.2d, v0.2d\n\t")
                :: [a_i] "m" (a[i]): "v0", "v1", "q0"
            );
        }
    }
}
#elif defined(FL_RATIO_F16L1)
static const double operation_intensity=2;
static const size_t flops_per_cache_line=128;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 2) {
            asm volatile (
                "ldr q0, %[a_i]\n\t"
                DUP_16("fmul v1.2d, v0.2d, v0.2d\n\t")
                :: [a_i] "m" (a[i]): "v0", "v1", "q0"
            );
        }
    }
}
#elif defined(FL_RATIO_F32L1)
static const double operation_intensity=4;
static const size_t flops_per_cache_line=256;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 2) {
            asm volatile (
                "ldr q0, %[a_i]\n\t"
                DUP_32("fmul v1.2d, v0.2d, v0.2d\n\t")
                :: [a_i] "m" (a[i]): "v0", "v1", "q0"
            );
        }
    }
}
#else 
#include "FL_RATIO_MACRO_ERROR_NOT_IDENTIFIED.h"
#endif
#endif // #ifdef KP_SVE
/* AVX-512 Part*/

#else // #ifdef __aarch64__
#ifdef AVX512       
// const char *SIMD_NAME = "AVX-512";
#if defined(FL_RATIO_F1L1)
static const double operation_intensity=0.125;
static const size_t flops_per_cache_line=8;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 100
        for (int i = 0; i < nsize; i += 8) {
            asm volatile (
                "vmovapd %[a_i], %%zmm0\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                :: [a_i] "m" (a[i]): "%zmm0", "%zmm1" 
            );
        }
    }
}
#elif defined(FL_RATIO_F2L1)
static const double operation_intensity=0.25;
static const size_t flops_per_cache_line=16;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 8) {
            asm volatile (
                "vmovapd %[a_i], %%zmm0\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                :: [a_i] "m" (a[i]): "%zmm0", "%zmm1" 
            );
        }
    }
}
#elif defined(FL_RATIO_F3L1)
static const double operation_intensity=0.375;
static const size_t flops_per_cache_line=24;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 8) {
            asm volatile (
                "vmovapd %[a_i], %%zmm0\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                :: [a_i] "m" (a[i]): "%zmm0", "%zmm1" 
            );
        }
    }
}
#elif defined(FL_RATIO_F4L1)
static const double operation_intensity=0.5;
static const size_t flops_per_cache_line=32;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 8) {
            asm volatile (
                "vmovapd %[a_i], %%zmm0\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                :: [a_i] "m" (a[i]): "%zmm0", "%zmm1" 
            );
        }
    }
}

#elif defined(FL_RATIO_F8L1)
static const double operation_intensity=1;
static const size_t flops_per_cache_line=64;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 8) {
            asm volatile (
                "vmovapd %[a_i], %%zmm0\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                :: [a_i] "m" (a[i]): "%zmm0", "%zmm1" 
            );
        }
    }
}

#elif defined(FL_RATIO_F16L1)
static const double operation_intensity=2;
static const size_t flops_per_cache_line=128;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 8) {
            asm volatile (
                "vmovapd %[a_i], %%zmm0\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                :: [a_i] "m" (a[i]): "%zmm0", "%zmm1" 
            );
        }
    }
}

#elif defined(FL_RATIO_F1L2)
static const double operation_intensity=1.0 / 8.0 / 2.0;
static const size_t flops_per_cache_line=4;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 16) {
            asm volatile (
                "vmovapd %[a_i0], %%zmm0\n\t"
                "vmovapd %[a_i1], %%zmm0\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                :: [a_i0] "m" (a[i]), 
                   [a_i1] "m" (a[i + 8])
                : "%zmm0", "%zmm1" 
            );
        }
    }
}

#elif defined(FL_RATIO_F1L4)
static const double operation_intensity=1.0 / 8.0 / 4.0;
static const size_t flops_per_cache_line=2;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 16
        for (int i = 0; i < nsize; i += 32) {
            asm volatile (
                "vmovapd %[a_i0], %%zmm0\n\t"
                "vmovapd %[a_i1], %%zmm0\n\t"
                "vmovapd %[a_i2], %%zmm0\n\t"
                "vmovapd %[a_i3], %%zmm0\n\t"
                "vmulpd %%zmm0, %%zmm0, %%zmm1\n\t"
                :: [a_i0] "m" (a[i]), 
                   [a_i1] "m" (a[i + 8]),
                   [a_i2] "m" (a[i + 16]),
                   [a_i3] "m" (a[i + 24])
                : "%zmm0", "%zmm1" 
            );
        }
    }
}
#else 
#include "FL_RATIO_MACRO_ERROR_NOT_IDENTIFIED.h"
#endif 

#else //#ifdef AVX512

// const char *SIMD_NAME = "AVX-256";
#if defined(FL_RATIO_F1L1)
static const double operation_intensity=0.125;
static const size_t flops_per_cache_line=8;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 100
        for (int i = 0; i < nsize; i += 8) {
            asm volatile (
                "vmovapd %[a_i], %%ymm0\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                :: [a_i] "m" (a[i]): "%ymm0", "%ymm1" 
            );
        }
    }
}
#elif defined(FL_RATIO_F2L1)
static const double operation_intensity=0.25;
static const size_t flops_per_cache_line=16;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 8) {
            asm volatile (
                "vmovapd %[a_i], %%ymm0\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                :: [a_i] "m" (a[i]): "%ymm0", "%ymm1" 
            );
        }
    }
}
#elif defined(FL_RATIO_F3L1)
static const double operation_intensity=0.375;
static const size_t flops_per_cache_line=24;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 8) {
            asm volatile (
                "vmovapd %[a_i], %%ymm0\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                :: [a_i] "m" (a[i]): "%ymm0", "%ymm1" 
            );
        }
    }
}
#elif defined(FL_RATIO_F4L1)
static const double operation_intensity=0.5;
static const size_t flops_per_cache_line=32;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 8) {
            asm volatile (
                "vmovapd %[a_i], %%ymm0\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                :: [a_i] "m" (a[i]): "%ymm0", "%ymm1" 
            );
        }
    }
}

#elif defined(FL_RATIO_F8L1)
static const double operation_intensity=1;
static const size_t flops_per_cache_line=64;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 8) {
            asm volatile (
                "vmovapd %[a_i], %%ymm0\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                :: [a_i] "m" (a[i]): "%ymm0", "%ymm1" 
            );
        }
    }
}

#elif defined(FL_RATIO_F16L1)
static const double operation_intensity=2;
static const size_t flops_per_cache_line=128;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 8) {
            asm volatile (
                "vmovapd %[a_i], %%ymm0\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                :: [a_i] "m" (a[i]): "%ymm0", "%ymm1"  
            );
        }
    }
}

#elif defined(FL_RATIO_F1L2)
static const double operation_intensity=1.0 / 8.0 / 2.0;
static const size_t flops_per_cache_line=4;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 32
        for (int i = 0; i < nsize; i += 16) {
            asm volatile (
                "vmovapd %[a_i0], %%ymm0\n\t"
                "vmovapd %[a_i1], %%ymm0\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                :: [a_i0] "m" (a[i]), 
                   [a_i1] "m" (a[i + 8])
                : "%ymm0", "%ymm1" 
            );
        }
    }
}

#elif defined(FL_RATIO_F1L4)
static const double operation_intensity=1.0 / 8.0 / 4.0;
static const size_t flops_per_cache_line=2;
static inline void run_kernel_once(int nsize) {
    for (int r = 0; r < repeat; r++) {
        #pragma GCC unroll 16
        for (int i = 0; i < nsize; i += 32) {
            asm volatile (
                "vmovapd %[a_i0], %%ymm0\n\t"
                "vmovapd %[a_i1], %%ymm0\n\t"
                "vmovapd %[a_i2], %%ymm0\n\t"
                "vmovapd %[a_i3], %%ymm0\n\t"
                "vmulpd %%ymm0, %%ymm0, %%ymm1\n\t"
                :: [a_i0] "m" (a[i]), 
                   [a_i1] "m" (a[i + 8]),
                   [a_i2] "m" (a[i + 16]),
                   [a_i3] "m" (a[i + 24])
                : "%ymm0", "%ymm1" 
            );
        }
    }
}
#else 
#include "FL_RATIO_MACRO_ERROR_NOT_IDENTIFIED.h"
#endif 

#endif //#ifdef AVX512   

#endif // #ifdef __aarch64__
int
d_mulldr(int ntest, uint64_t *ns, uint64_t *cy, uint64_t kib, ...) {
    int nsize, err;

    nsize = kib * 1024 / sizeof(double);
    nsize = ((nsize + 7) / 8) * 8;
    kib = nsize * sizeof(double) / 1024;
    repeat = ((size_t) 1e8) / (nsize / 8) / MAX(1, (operation_intensity / 0.125));
    repeat = MAX(repeat, 1);
    tpprintf(0, 0, 0, "Working set size: %dKB.\n", kib);
    tpprintf(0, 0, 0, "repeat times: %lu.\n", repeat);
    tpprintf(0, 0, 0, "Operation intensity: %f.\n", operation_intensity);

    #ifdef __aarch64__
    #ifdef KP_SVE
        simd_width = svcntd();
    #endif
    #endif

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
