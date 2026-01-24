/**
 * tpbk_roofline_rocm.hip
 * ROCm/HIP Roofline Model Benchmark Kernel
 *
 * Measures GPU performance at different arithmetic intensities (AI)
 * to construct a roofline model for AMD GPUs.
 */

#include <hip/hip_runtime.h>
#include <hip/hip_bfloat16.h>
#include <hip/hip_fp16.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <float.h>

extern "C" {
#include "tpbench.h"
}

/* Number of arithmetic intensity points to measure */
#define N_AI_POINTS 23

/* Target arithmetic intensities (FLOP/Byte) */
static const double ai_values[N_AI_POINTS] = {
    0.1, 0.25, 0.5, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 15.0, 20.0, 25.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 90.0
};

/* AI index: 0-12 same as before, 13=15.0, 14=20.0, 15=25.0, 16=30.0 */

/* HIP error checking macro - uses TPBE_KERN_ARG_FAIL for HIP runtime errors */
#define HIP_CHECK(call)                                                        \
    do {                                                                       \
        hipError_t err = call;                                                 \
        if (err != hipSuccess) {                                               \
            fprintf(stderr, "HIP error at %s:%d: %s\n", __FILE__, __LINE__,    \
                    hipGetErrorString(err));                                   \
            return TPBE_KERN_ARG_FAIL;                                         \
        }                                                                      \
    } while (0)

/* Forward declarations */
extern "C" {
int register_roofline_rocm(void);
int _tpbk_register_roofline_rocm(void);
int _tpbk_run_roofline_rocm(void);
int tpbk_pli_register_roofline_rocm(void);
}

static int d_roofline_rocm(tpb_timer_t *timer, int64_t type_code, int64_t op_code,
                           double total_memsize_kib, int64_t ntest, int64_t twarm_ms,
                           int64_t nloop_timing, double *flops_out);

/*
 * ============================================================================
 * HIP Kernels for FP32 Vector Operations at Different Arithmetic Intensities
 * ============================================================================
 *
 * Each kernel reads data from memory, performs a controlled number of FLOPs,
 * and writes back. The arithmetic intensity is controlled by the number of
 * FMA operations per memory access.
 *
 * For FP32: 1 element = 4 bytes, read + write = 8 bytes total memory traffic
 * FMA = 2 FLOPs (multiply + add)
 */

/* AI = 0.1: Very memory-bound (1 FMA per 20 bytes effective)
 * Each thread processes 4 elements for better memory coalescing */
__global__ void roofline_fp32_vec_ai_0p1(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = data[idx];
        float v1 = data[idx + 1];
        float v2 = data[idx + 2];
        float v3 = data[idx + 3];
        
        v0 = fmaf(v0, 1.0001f, 0.0001f);
        v1 = fmaf(v1, 1.0001f, 0.0001f);
        v2 = fmaf(v2, 1.0001f, 0.0001f);
        v3 = fmaf(v3, 1.0001f, 0.0001f);
        
        data[idx] = v0;
        data[idx + 1] = v1;
        data[idx + 2] = v2;
        data[idx + 3] = v3;
    }
}

/* AI = 0.25: Memory-bound (2 FLOPs per 8 bytes)
 * Each thread processes 4 elements for better memory coalescing */
__global__ void roofline_fp32_vec_ai_0p25(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = data[idx];
        float v1 = data[idx + 1];
        float v2 = data[idx + 2];
        float v3 = data[idx + 3];
        
        v0 = fmaf(v0, 1.0001f, 0.0001f);
        v1 = fmaf(v1, 1.0001f, 0.0001f);
        v2 = fmaf(v2, 1.0001f, 0.0001f);
        v3 = fmaf(v3, 1.0001f, 0.0001f);
        
        data[idx] = v0;
        data[idx + 1] = v1;
        data[idx + 2] = v2;
        data[idx + 3] = v3;
    }
}

/* AI = 0.5: Memory-bound (4 FLOPs per 8 bytes)
 * Each thread processes 4 elements with 2 FMAs each */
__global__ void roofline_fp32_vec_ai_0p5(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = data[idx];
        float v1 = data[idx + 1];
        float v2 = data[idx + 2];
        float v3 = data[idx + 3];
        
        v0 = fmaf(v0, 1.0001f, 0.0001f);
        v1 = fmaf(v1, 1.0001f, 0.0001f);
        v2 = fmaf(v2, 1.0001f, 0.0001f);
        v3 = fmaf(v3, 1.0001f, 0.0001f);
        v0 = fmaf(v0, 1.0001f, 0.0001f);
        v1 = fmaf(v1, 1.0001f, 0.0001f);
        v2 = fmaf(v2, 1.0001f, 0.0001f);
        v3 = fmaf(v3, 1.0001f, 0.0001f);
        
        data[idx] = v0;
        data[idx + 1] = v1;
        data[idx + 2] = v2;
        data[idx + 3] = v3;
    }
}

/* AI = 1.0: Balanced (8 FLOPs per 8 bytes)
 * Each thread processes 4 elements with 4 FMAs each */
__global__ void roofline_fp32_vec_ai_1p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = data[idx];
        float v1 = data[idx + 1];
        float v2 = data[idx + 2];
        float v3 = data[idx + 3];
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f);
            v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f);
            v3 = fmaf(v3, 1.0001f, 0.0001f);
        }
        
        data[idx] = v0;
        data[idx + 1] = v1;
        data[idx + 2] = v2;
        data[idx + 3] = v3;
    }
}

/* AI = 2.0: Compute-bound (16 FLOPs per 8 bytes)
 * Each thread processes 2 elements with 4 independent chains for ILP */
__global__ void roofline_fp32_vec_ai_2p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        float v0 = data[idx];
        float v1 = data[idx + 1];
        float a0 = v0, a1 = v1;
        
        /* 32 FMAs / 2 elements = 16 FMAs per element = 32 FLOPs per element
         * But we want 16 FLOPs per 8 bytes, so 8 FMAs per element */
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f);
            v1 = fmaf(v1, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f);
            a1 = fmaf(a1, 1.0001f, 0.0001f);
        }
        
        data[idx] = v0 + a0;
        data[idx + 1] = v1 + a1;
    }
}

/* AI = 3.0: Compute-bound (24 FLOPs per 8 bytes)
 * Each thread processes 2 elements with 4 independent chains for ILP */
__global__ void roofline_fp32_vec_ai_3p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        float v0 = data[idx];
        float v1 = data[idx + 1];
        float a0 = v0, a1 = v1;
        
        /* 48 FMAs / 2 elements = 24 FMAs per element = 48 FLOPs per element
         * But we want 24 FLOPs per 8 bytes, so 12 FMAs per element */
        #pragma unroll
        for (int i = 0; i < 6; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f);
            v1 = fmaf(v1, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f);
            a1 = fmaf(a1, 1.0001f, 0.0001f);
        }
        
        data[idx] = v0 + a0;
        data[idx + 1] = v1 + a1;
    }
}

/* AI = 4.0: Compute-bound (32 FLOPs per 8 bytes)
 * Each thread processes 2 elements with 4 independent chains for ILP */
__global__ void roofline_fp32_vec_ai_4p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        float v0 = data[idx];
        float v1 = data[idx + 1];
        float a0 = v0, a1 = v1;
        
        /* 64 FMAs / 2 elements = 32 FMAs per element = 64 FLOPs per element
         * But we want 32 FLOPs per 8 bytes, so 16 FMAs per element */
        #pragma unroll
        for (int i = 0; i < 8; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f);
            v1 = fmaf(v1, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f);
            a1 = fmaf(a1, 1.0001f, 0.0001f);
        }
        
        data[idx] = v0 + a0;
        data[idx + 1] = v1 + a1;
    }
}

/* AI = 5.0: Compute-bound (40 FLOPs per 8 bytes) */
__global__ void roofline_fp32_vec_ai_5p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        float v0 = data[idx];
        float v1 = data[idx + 1];
        float a0 = v0, a1 = v1;
        
        #pragma unroll
        for (int i = 0; i < 10; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f);
            v1 = fmaf(v1, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f);
            a1 = fmaf(a1, 1.0001f, 0.0001f);
        }
        
        data[idx] = v0 + a0;
        data[idx + 1] = v1 + a1;
    }
}

/* AI = 6.0: Compute-bound (48 FLOPs per 8 bytes) */
__global__ void roofline_fp32_vec_ai_6p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        float v0 = data[idx];
        float v1 = data[idx + 1];
        float a0 = v0, a1 = v1;
        
        #pragma unroll
        for (int i = 0; i < 12; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f);
            v1 = fmaf(v1, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f);
            a1 = fmaf(a1, 1.0001f, 0.0001f);
        }
        
        data[idx] = v0 + a0;
        data[idx + 1] = v1 + a1;
    }
}

/* AI = 7.0: Compute-bound (56 FLOPs per 8 bytes) */
__global__ void roofline_fp32_vec_ai_7p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        float v0 = data[idx];
        float v1 = data[idx + 1];
        float a0 = v0, a1 = v1;
        
        #pragma unroll
        for (int i = 0; i < 14; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f);
            v1 = fmaf(v1, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f);
            a1 = fmaf(a1, 1.0001f, 0.0001f);
        }
        
        data[idx] = v0 + a0;
        data[idx + 1] = v1 + a1;
    }
}

/* AI = 8.0: (64 FLOPs per 8 bytes) → 8 iterations */
__global__ void roofline_fp32_vec_ai_8p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = data[idx], v1 = data[idx + 1], v2 = data[idx + 2], v3 = data[idx + 3];
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3;
        float c0 = v0, c1 = v1, c2 = v2, c3 = v3;
        #pragma unroll
        for (int i = 0; i < 8; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f);
            b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            c0 = fmaf(c0, 1.0001f, 0.0001f); c1 = fmaf(c1, 1.0001f, 0.0001f);
            c2 = fmaf(c2, 1.0001f, 0.0001f); c3 = fmaf(c3, 1.0001f, 0.0001f);
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
        data[idx + 2] = v2 + a2 + b2 + c2; data[idx + 3] = v3 + a3 + b3 + c3;
    }
}

/* AI = 9.0: (72 FLOPs per 8 bytes) → 9 iterations */
__global__ void roofline_fp32_vec_ai_9p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = data[idx], v1 = data[idx + 1], v2 = data[idx + 2], v3 = data[idx + 3];
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3;
        float c0 = v0, c1 = v1, c2 = v2, c3 = v3;
        #pragma unroll
        for (int i = 0; i < 9; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f);
            b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            c0 = fmaf(c0, 1.0001f, 0.0001f); c1 = fmaf(c1, 1.0001f, 0.0001f);
            c2 = fmaf(c2, 1.0001f, 0.0001f); c3 = fmaf(c3, 1.0001f, 0.0001f);
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
        data[idx + 2] = v2 + a2 + b2 + c2; data[idx + 3] = v3 + a3 + b3 + c3;
    }
}

/* AI = 10.0: (80 FLOPs per 8 bytes) → 10 iterations */
__global__ void roofline_fp32_vec_ai_10p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = data[idx], v1 = data[idx + 1], v2 = data[idx + 2], v3 = data[idx + 3];
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3;
        float c0 = v0, c1 = v1, c2 = v2, c3 = v3;
        #pragma unroll
        for (int i = 0; i < 10; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f);
            b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            c0 = fmaf(c0, 1.0001f, 0.0001f); c1 = fmaf(c1, 1.0001f, 0.0001f);
            c2 = fmaf(c2, 1.0001f, 0.0001f); c3 = fmaf(c3, 1.0001f, 0.0001f);
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
        data[idx + 2] = v2 + a2 + b2 + c2; data[idx + 3] = v3 + a3 + b3 + c3;
    }
}

/* AI = 15.0: Highly compute-bound (120 FLOPs per 8 bytes)
 * 4 elements, 4 chains, 16 FMAs/iter = 8 FLOPs/elem/iter → 15 iterations */
__global__ void roofline_fp32_vec_ai_15p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = data[idx], v1 = data[idx + 1], v2 = data[idx + 2], v3 = data[idx + 3];
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3;
        float c0 = v0, c1 = v1, c2 = v2, c3 = v3;
        for (int i = 0; i < 15; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f);
            b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            c0 = fmaf(c0, 1.0001f, 0.0001f); c1 = fmaf(c1, 1.0001f, 0.0001f);
            asm volatile("");
            c2 = fmaf(c2, 1.0001f, 0.0001f); c3 = fmaf(c3, 1.0001f, 0.0001f);
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
        data[idx + 2] = v2 + a2 + b2 + c2; data[idx + 3] = v3 + a3 + b3 + c3;
    }
}

/* AI = 20.0: Highly compute-bound (160 FLOPs per 8 bytes) → 20 iterations */
__global__ void roofline_fp32_vec_ai_20p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = data[idx], v1 = data[idx + 1], v2 = data[idx + 2], v3 = data[idx + 3];
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3;
        float c0 = v0, c1 = v1, c2 = v2, c3 = v3;
        for (int i = 0; i < 20; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f);
            b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            c0 = fmaf(c0, 1.0001f, 0.0001f); c1 = fmaf(c1, 1.0001f, 0.0001f);
            asm volatile("");
            c2 = fmaf(c2, 1.0001f, 0.0001f); c3 = fmaf(c3, 1.0001f, 0.0001f);
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
        data[idx + 2] = v2 + a2 + b2 + c2; data[idx + 3] = v3 + a3 + b3 + c3;
    }
}

/* AI = 25.0: Highly compute-bound (200 FLOPs per 8 bytes) → 25 iterations */
__global__ void roofline_fp32_vec_ai_25p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = data[idx], v1 = data[idx + 1], v2 = data[idx + 2], v3 = data[idx + 3];
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3;
        float c0 = v0, c1 = v1, c2 = v2, c3 = v3;
        for (int i = 0; i < 25; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f);
            b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            c0 = fmaf(c0, 1.0001f, 0.0001f); c1 = fmaf(c1, 1.0001f, 0.0001f);
            asm volatile("");
            c2 = fmaf(c2, 1.0001f, 0.0001f); c3 = fmaf(c3, 1.0001f, 0.0001f);
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
        data[idx + 2] = v2 + a2 + b2 + c2; data[idx + 3] = v3 + a3 + b3 + c3;
    }
}

/* AI = 30.0: Highly compute-bound (240 FLOPs per 8 bytes) → 30 iterations */
__global__ void roofline_fp32_vec_ai_30p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = data[idx], v1 = data[idx + 1], v2 = data[idx + 2], v3 = data[idx + 3];
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3;
        float c0 = v0, c1 = v1, c2 = v2, c3 = v3;
        for (int i = 0; i < 30; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f);
            b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            c0 = fmaf(c0, 1.0001f, 0.0001f); c1 = fmaf(c1, 1.0001f, 0.0001f);
            asm volatile("");
            c2 = fmaf(c2, 1.0001f, 0.0001f); c3 = fmaf(c3, 1.0001f, 0.0001f);
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
        data[idx + 2] = v2 + a2 + b2 + c2; data[idx + 3] = v3 + a3 + b3 + c3;
    }
}

/* AI = 40.0: (320 FLOPs per 8 bytes) → 40 iterations */
__global__ void roofline_fp32_vec_ai_40p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = data[idx], v1 = data[idx + 1], v2 = data[idx + 2], v3 = data[idx + 3];
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3;
        float c0 = v0, c1 = v1, c2 = v2, c3 = v3;
        for (int i = 0; i < 40; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f);
            b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            c0 = fmaf(c0, 1.0001f, 0.0001f); c1 = fmaf(c1, 1.0001f, 0.0001f);
            c2 = fmaf(c2, 1.0001f, 0.0001f); c3 = fmaf(c3, 1.0001f, 0.0001f);
            asm volatile("");
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
        data[idx + 2] = v2 + a2 + b2 + c2; data[idx + 3] = v3 + a3 + b3 + c3;
    }
}

/* AI = 50.0: (400 FLOPs per 8 bytes) → 50 iterations */
__global__ void roofline_fp32_vec_ai_50p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = data[idx], v1 = data[idx + 1], v2 = data[idx + 2], v3 = data[idx + 3];
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3;
        float c0 = v0, c1 = v1, c2 = v2, c3 = v3;
        for (int i = 0; i < 50; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f);
            b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            c0 = fmaf(c0, 1.0001f, 0.0001f); c1 = fmaf(c1, 1.0001f, 0.0001f);
            c2 = fmaf(c2, 1.0001f, 0.0001f); c3 = fmaf(c3, 1.0001f, 0.0001f);
            asm volatile("");
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
        data[idx + 2] = v2 + a2 + b2 + c2; data[idx + 3] = v3 + a3 + b3 + c3;
    }
}

/* AI = 60.0: (480 FLOPs per 8 bytes) → 60 iterations */
__global__ void roofline_fp32_vec_ai_60p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = data[idx], v1 = data[idx + 1], v2 = data[idx + 2], v3 = data[idx + 3];
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3;
        float c0 = v0, c1 = v1, c2 = v2, c3 = v3;
        for (int i = 0; i < 60; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f);
            b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            c0 = fmaf(c0, 1.0001f, 0.0001f); c1 = fmaf(c1, 1.0001f, 0.0001f);
            c2 = fmaf(c2, 1.0001f, 0.0001f); c3 = fmaf(c3, 1.0001f, 0.0001f);
            asm volatile("");
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
        data[idx + 2] = v2 + a2 + b2 + c2; data[idx + 3] = v3 + a3 + b3 + c3;
    }
}

/* AI = 70.0: (560 FLOPs per 8 bytes) → 70 iterations with 4 chains */
__global__ void roofline_fp32_vec_ai_70p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = data[idx], v1 = data[idx + 1], v2 = data[idx + 2], v3 = data[idx + 3];
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3;
        float c0 = v0, c1 = v1, c2 = v2, c3 = v3;
        for (int i = 0; i < 70; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f);
            b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            c0 = fmaf(c0, 1.0001f, 0.0001f); c1 = fmaf(c1, 1.0001f, 0.0001f);
            c2 = fmaf(c2, 1.0001f, 0.0001f); c3 = fmaf(c3, 1.0001f, 0.0001f);
            asm volatile("");
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
        data[idx + 2] = v2 + a2 + b2 + c2; data[idx + 3] = v3 + a3 + b3 + c3;
    }
}

/* AI = 80.0: (640 FLOPs per 8 bytes) → 80 iterations with 4 chains */
__global__ void roofline_fp32_vec_ai_80p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = data[idx], v1 = data[idx + 1], v2 = data[idx + 2], v3 = data[idx + 3];
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3;
        float c0 = v0, c1 = v1, c2 = v2, c3 = v3;
        for (int i = 0; i < 80; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f);
            b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            c0 = fmaf(c0, 1.0001f, 0.0001f); c1 = fmaf(c1, 1.0001f, 0.0001f);
            c2 = fmaf(c2, 1.0001f, 0.0001f); c3 = fmaf(c3, 1.0001f, 0.0001f);
            asm volatile("");
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
        data[idx + 2] = v2 + a2 + b2 + c2; data[idx + 3] = v3 + a3 + b3 + c3;
    }
}

/* AI = 90.0: (720 FLOPs per 8 bytes) → 90 iterations with 4 chains */
__global__ void roofline_fp32_vec_ai_90p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = data[idx], v1 = data[idx + 1], v2 = data[idx + 2], v3 = data[idx + 3];
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3;
        float c0 = v0, c1 = v1, c2 = v2, c3 = v3;
        for (int i = 0; i < 90; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f);
            b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            c0 = fmaf(c0, 1.0001f, 0.0001f); c1 = fmaf(c1, 1.0001f, 0.0001f);
            c2 = fmaf(c2, 1.0001f, 0.0001f); c3 = fmaf(c3, 1.0001f, 0.0001f);
            asm volatile("");
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
        data[idx + 2] = v2 + a2 + b2 + c2; data[idx + 3] = v3 + a3 + b3 + c3;
    }
}

/*
 * ============================================================================
 * HIP Kernels for BF16 Vector Operations at Different Arithmetic Intensities
 * ============================================================================
 *
 * For BF16: 1 element = 2 bytes, read + write = 4 bytes total memory traffic
 */

/* AI = 0.1: Very memory-bound
 * Each thread processes 8 elements for better memory coalescing */
__global__ void roofline_bf16_vec_ai_0p1(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = float(data[idx]);
        float v1 = float(data[idx + 1]);
        float v2 = float(data[idx + 2]);
        float v3 = float(data[idx + 3]);
        float v4 = float(data[idx + 4]);
        float v5 = float(data[idx + 5]);
        float v6 = float(data[idx + 6]);
        float v7 = float(data[idx + 7]);
        
        v0 = fmaf(v0, 1.0001f, 0.0001f);
        v1 = fmaf(v1, 1.0001f, 0.0001f);
        v2 = fmaf(v2, 1.0001f, 0.0001f);
        v3 = fmaf(v3, 1.0001f, 0.0001f);
        v4 = fmaf(v4, 1.0001f, 0.0001f);
        v5 = fmaf(v5, 1.0001f, 0.0001f);
        v6 = fmaf(v6, 1.0001f, 0.0001f);
        v7 = fmaf(v7, 1.0001f, 0.0001f);
        
        data[idx] = hip_bfloat16(v0);
        data[idx + 1] = hip_bfloat16(v1);
        data[idx + 2] = hip_bfloat16(v2);
        data[idx + 3] = hip_bfloat16(v3);
        data[idx + 4] = hip_bfloat16(v4);
        data[idx + 5] = hip_bfloat16(v5);
        data[idx + 6] = hip_bfloat16(v6);
        data[idx + 7] = hip_bfloat16(v7);
    }
}

/* AI = 0.25: Memory-bound (1 FLOP per 4 bytes)
 * Each thread processes 8 elements for better memory coalescing */
__global__ void roofline_bf16_vec_ai_0p25(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = float(data[idx]);
        float v1 = float(data[idx + 1]);
        float v2 = float(data[idx + 2]);
        float v3 = float(data[idx + 3]);
        float v4 = float(data[idx + 4]);
        float v5 = float(data[idx + 5]);
        float v6 = float(data[idx + 6]);
        float v7 = float(data[idx + 7]);
        
        v0 = fmaf(v0, 1.0001f, 0.0001f);
        v1 = fmaf(v1, 1.0001f, 0.0001f);
        v2 = fmaf(v2, 1.0001f, 0.0001f);
        v3 = fmaf(v3, 1.0001f, 0.0001f);
        v4 = fmaf(v4, 1.0001f, 0.0001f);
        v5 = fmaf(v5, 1.0001f, 0.0001f);
        v6 = fmaf(v6, 1.0001f, 0.0001f);
        v7 = fmaf(v7, 1.0001f, 0.0001f);
        
        data[idx] = hip_bfloat16(v0);
        data[idx + 1] = hip_bfloat16(v1);
        data[idx + 2] = hip_bfloat16(v2);
        data[idx + 3] = hip_bfloat16(v3);
        data[idx + 4] = hip_bfloat16(v4);
        data[idx + 5] = hip_bfloat16(v5);
        data[idx + 6] = hip_bfloat16(v6);
        data[idx + 7] = hip_bfloat16(v7);
    }
}

/* AI = 0.5: Memory-bound (2 FLOPs per 4 bytes)
 * Each thread processes 8 elements */
__global__ void roofline_bf16_vec_ai_0p5(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = float(data[idx]);
        float v1 = float(data[idx + 1]);
        float v2 = float(data[idx + 2]);
        float v3 = float(data[idx + 3]);
        float v4 = float(data[idx + 4]);
        float v5 = float(data[idx + 5]);
        float v6 = float(data[idx + 6]);
        float v7 = float(data[idx + 7]);
        
        v0 = fmaf(v0, 1.0001f, 0.0001f);
        v1 = fmaf(v1, 1.0001f, 0.0001f);
        v2 = fmaf(v2, 1.0001f, 0.0001f);
        v3 = fmaf(v3, 1.0001f, 0.0001f);
        v4 = fmaf(v4, 1.0001f, 0.0001f);
        v5 = fmaf(v5, 1.0001f, 0.0001f);
        v6 = fmaf(v6, 1.0001f, 0.0001f);
        v7 = fmaf(v7, 1.0001f, 0.0001f);
        
        data[idx] = hip_bfloat16(v0);
        data[idx + 1] = hip_bfloat16(v1);
        data[idx + 2] = hip_bfloat16(v2);
        data[idx + 3] = hip_bfloat16(v3);
        data[idx + 4] = hip_bfloat16(v4);
        data[idx + 5] = hip_bfloat16(v5);
        data[idx + 6] = hip_bfloat16(v6);
        data[idx + 7] = hip_bfloat16(v7);
    }
}

/* AI = 1.0: Balanced (4 FLOPs per 4 bytes)
 * Each thread processes 8 elements with 2 FMAs each */
__global__ void roofline_bf16_vec_ai_1p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = float(data[idx]);
        float v1 = float(data[idx + 1]);
        float v2 = float(data[idx + 2]);
        float v3 = float(data[idx + 3]);
        float v4 = float(data[idx + 4]);
        float v5 = float(data[idx + 5]);
        float v6 = float(data[idx + 6]);
        float v7 = float(data[idx + 7]);
        
        #pragma unroll
        for (int i = 0; i < 2; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f);
            v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f);
            v3 = fmaf(v3, 1.0001f, 0.0001f);
            v4 = fmaf(v4, 1.0001f, 0.0001f);
            v5 = fmaf(v5, 1.0001f, 0.0001f);
            v6 = fmaf(v6, 1.0001f, 0.0001f);
            v7 = fmaf(v7, 1.0001f, 0.0001f);
        }
        
        data[idx] = hip_bfloat16(v0);
        data[idx + 1] = hip_bfloat16(v1);
        data[idx + 2] = hip_bfloat16(v2);
        data[idx + 3] = hip_bfloat16(v3);
        data[idx + 4] = hip_bfloat16(v4);
        data[idx + 5] = hip_bfloat16(v5);
        data[idx + 6] = hip_bfloat16(v6);
        data[idx + 7] = hip_bfloat16(v7);
    }
}

/* AI = 2.0: Compute-bound (8 FLOPs per 4 bytes)
 * Each thread processes 4 elements with 8 independent chains for ILP */
__global__ void roofline_bf16_vec_ai_2p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = float(data[idx]);
        float v1 = float(data[idx + 1]);
        float v2 = float(data[idx + 2]);
        float v3 = float(data[idx + 3]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        
        /* 32 FMAs / 4 elements = 8 FMAs per element = 16 FLOPs per element
         * But we want 8 FLOPs per 4 bytes, so 4 FMAs per element */
        #pragma unroll
        for (int i = 0; i < 2; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f);
            v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f);
            v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f);
            a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f);
            a3 = fmaf(a3, 1.0001f, 0.0001f);
        }
        
        data[idx] = hip_bfloat16(v0 + a0);
        data[idx + 1] = hip_bfloat16(v1 + a1);
        data[idx + 2] = hip_bfloat16(v2 + a2);
        data[idx + 3] = hip_bfloat16(v3 + a3);
    }
}

/* AI = 3.0: Compute-bound (12 FLOPs per 4 bytes)
 * Each thread processes 4 elements with 8 independent chains for ILP */
__global__ void roofline_bf16_vec_ai_3p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = float(data[idx]);
        float v1 = float(data[idx + 1]);
        float v2 = float(data[idx + 2]);
        float v3 = float(data[idx + 3]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        
        /* 48 FMAs / 4 elements = 12 FMAs per element = 24 FLOPs per element
         * But we want 12 FLOPs per 4 bytes, so 6 FMAs per element */
        #pragma unroll
        for (int i = 0; i < 3; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f);
            v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f);
            v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f);
            a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f);
            a3 = fmaf(a3, 1.0001f, 0.0001f);
        }
        
        data[idx] = hip_bfloat16(v0 + a0);
        data[idx + 1] = hip_bfloat16(v1 + a1);
        data[idx + 2] = hip_bfloat16(v2 + a2);
        data[idx + 3] = hip_bfloat16(v3 + a3);
    }
}

/* AI = 4.0: Compute-bound (16 FLOPs per 4 bytes)
 * Each thread processes 4 elements with 8 independent chains for ILP */
__global__ void roofline_bf16_vec_ai_4p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = float(data[idx]);
        float v1 = float(data[idx + 1]);
        float v2 = float(data[idx + 2]);
        float v3 = float(data[idx + 3]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        
        /* 64 FMAs / 4 elements = 16 FMAs per element = 32 FLOPs per element
         * But we want 16 FLOPs per 4 bytes, so 8 FMAs per element */
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f);
            v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f);
            v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f);
            a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f);
            a3 = fmaf(a3, 1.0001f, 0.0001f);
        }
        
        data[idx] = hip_bfloat16(v0 + a0);
        data[idx + 1] = hip_bfloat16(v1 + a1);
        data[idx + 2] = hip_bfloat16(v2 + a2);
        data[idx + 3] = hip_bfloat16(v3 + a3);
    }
}

/* AI = 8.0: (32 FLOPs per 4 bytes) → 8 iterations */
__global__ void roofline_bf16_vec_ai_8p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = float(data[idx]), v1 = float(data[idx + 1]);
        float v2 = float(data[idx + 2]), v3 = float(data[idx + 3]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        #pragma unroll
        for (int i = 0; i < 8; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
        }
        data[idx] = hip_bfloat16(v0 + a0); data[idx + 1] = hip_bfloat16(v1 + a1);
        data[idx + 2] = hip_bfloat16(v2 + a2); data[idx + 3] = hip_bfloat16(v3 + a3);
    }
}

/* BF16 AI = 5.0, 6.0, 7.0 kernels - same pattern as AI 2,3,4 */
__global__ void roofline_bf16_vec_ai_5p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = float(data[idx]), v1 = float(data[idx + 1]);
        float v2 = float(data[idx + 2]), v3 = float(data[idx + 3]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        #pragma unroll
        for (int i = 0; i < 5; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
        }
        data[idx] = hip_bfloat16(v0 + a0); data[idx + 1] = hip_bfloat16(v1 + a1);
        data[idx + 2] = hip_bfloat16(v2 + a2); data[idx + 3] = hip_bfloat16(v3 + a3);
    }
}

__global__ void roofline_bf16_vec_ai_6p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = float(data[idx]), v1 = float(data[idx + 1]);
        float v2 = float(data[idx + 2]), v3 = float(data[idx + 3]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        #pragma unroll
        for (int i = 0; i < 6; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
        }
        data[idx] = hip_bfloat16(v0 + a0); data[idx + 1] = hip_bfloat16(v1 + a1);
        data[idx + 2] = hip_bfloat16(v2 + a2); data[idx + 3] = hip_bfloat16(v3 + a3);
    }
}

__global__ void roofline_bf16_vec_ai_7p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = float(data[idx]), v1 = float(data[idx + 1]);
        float v2 = float(data[idx + 2]), v3 = float(data[idx + 3]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        #pragma unroll
        for (int i = 0; i < 7; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
        }
        data[idx] = hip_bfloat16(v0 + a0); data[idx + 1] = hip_bfloat16(v1 + a1);
        data[idx + 2] = hip_bfloat16(v2 + a2); data[idx + 3] = hip_bfloat16(v3 + a3);
    }
}

/* AI = 9.0: (36 FLOPs per 4 bytes) → 9 iterations */
__global__ void roofline_bf16_vec_ai_9p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = float(data[idx]), v1 = float(data[idx + 1]);
        float v2 = float(data[idx + 2]), v3 = float(data[idx + 3]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        #pragma unroll
        for (int i = 0; i < 9; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
        }
        data[idx] = hip_bfloat16(v0 + a0); data[idx + 1] = hip_bfloat16(v1 + a1);
        data[idx + 2] = hip_bfloat16(v2 + a2); data[idx + 3] = hip_bfloat16(v3 + a3);
    }
}

/* AI = 10.0: (40 FLOPs per 4 bytes) → 10 iterations */
__global__ void roofline_bf16_vec_ai_10p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = float(data[idx]), v1 = float(data[idx + 1]);
        float v2 = float(data[idx + 2]), v3 = float(data[idx + 3]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        #pragma unroll
        for (int i = 0; i < 10; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
        }
        data[idx] = hip_bfloat16(v0 + a0); data[idx + 1] = hip_bfloat16(v1 + a1);
        data[idx + 2] = hip_bfloat16(v2 + a2); data[idx + 3] = hip_bfloat16(v3 + a3);
    }
}

/* BF16 AI = 15.0, 20.0, 25.0, 30.0 kernels
 * 8 elements, 3 chains, 24 FMAs/iter = 6 FLOPs/elem/iter */
__global__ void roofline_bf16_vec_ai_15p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = float(data[idx]), v1 = float(data[idx + 1]), v2 = float(data[idx + 2]), v3 = float(data[idx + 3]);
        float v4 = float(data[idx + 4]), v5 = float(data[idx + 5]), v6 = float(data[idx + 6]), v7 = float(data[idx + 7]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3, a4 = v4, a5 = v5, a6 = v6, a7 = v7;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3, b4 = v4, b5 = v5, b6 = v6, b7 = v7;
        #pragma unroll
        for (int i = 0; i < 10; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f); v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            v4 = fmaf(v4, 1.0001f, 0.0001f); v5 = fmaf(v5, 1.0001f, 0.0001f); v6 = fmaf(v6, 1.0001f, 0.0001f); v7 = fmaf(v7, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f); a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            a4 = fmaf(a4, 1.0001f, 0.0001f); a5 = fmaf(a5, 1.0001f, 0.0001f); a6 = fmaf(a6, 1.0001f, 0.0001f); a7 = fmaf(a7, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f); b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            b4 = fmaf(b4, 1.0001f, 0.0001f); b5 = fmaf(b5, 1.0001f, 0.0001f); b6 = fmaf(b6, 1.0001f, 0.0001f); b7 = fmaf(b7, 1.0001f, 0.0001f);
        }
        data[idx] = hip_bfloat16(v0 + a0 + b0); data[idx + 1] = hip_bfloat16(v1 + a1 + b1);
        data[idx + 2] = hip_bfloat16(v2 + a2 + b2); data[idx + 3] = hip_bfloat16(v3 + a3 + b3);
        data[idx + 4] = hip_bfloat16(v4 + a4 + b4); data[idx + 5] = hip_bfloat16(v5 + a5 + b5);
        data[idx + 6] = hip_bfloat16(v6 + a6 + b6); data[idx + 7] = hip_bfloat16(v7 + a7 + b7);
    }
}

__global__ void roofline_bf16_vec_ai_20p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = float(data[idx]), v1 = float(data[idx + 1]), v2 = float(data[idx + 2]), v3 = float(data[idx + 3]);
        float v4 = float(data[idx + 4]), v5 = float(data[idx + 5]), v6 = float(data[idx + 6]), v7 = float(data[idx + 7]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3, a4 = v4, a5 = v5, a6 = v6, a7 = v7;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3, b4 = v4, b5 = v5, b6 = v6, b7 = v7;
        #pragma unroll
        for (int i = 0; i < 14; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f); v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            v4 = fmaf(v4, 1.0001f, 0.0001f); v5 = fmaf(v5, 1.0001f, 0.0001f); v6 = fmaf(v6, 1.0001f, 0.0001f); v7 = fmaf(v7, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f); a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            a4 = fmaf(a4, 1.0001f, 0.0001f); a5 = fmaf(a5, 1.0001f, 0.0001f); a6 = fmaf(a6, 1.0001f, 0.0001f); a7 = fmaf(a7, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f); b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            b4 = fmaf(b4, 1.0001f, 0.0001f); b5 = fmaf(b5, 1.0001f, 0.0001f); b6 = fmaf(b6, 1.0001f, 0.0001f); b7 = fmaf(b7, 1.0001f, 0.0001f);
        }
        data[idx] = hip_bfloat16(v0 + a0 + b0); data[idx + 1] = hip_bfloat16(v1 + a1 + b1);
        data[idx + 2] = hip_bfloat16(v2 + a2 + b2); data[idx + 3] = hip_bfloat16(v3 + a3 + b3);
        data[idx + 4] = hip_bfloat16(v4 + a4 + b4); data[idx + 5] = hip_bfloat16(v5 + a5 + b5);
        data[idx + 6] = hip_bfloat16(v6 + a6 + b6); data[idx + 7] = hip_bfloat16(v7 + a7 + b7);
    }
}

__global__ void roofline_bf16_vec_ai_25p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = float(data[idx]), v1 = float(data[idx + 1]), v2 = float(data[idx + 2]), v3 = float(data[idx + 3]);
        float v4 = float(data[idx + 4]), v5 = float(data[idx + 5]), v6 = float(data[idx + 6]), v7 = float(data[idx + 7]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3, a4 = v4, a5 = v5, a6 = v6, a7 = v7;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3, b4 = v4, b5 = v5, b6 = v6, b7 = v7;
        #pragma unroll
        for (int i = 0; i < 17; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f); v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            v4 = fmaf(v4, 1.0001f, 0.0001f); v5 = fmaf(v5, 1.0001f, 0.0001f); v6 = fmaf(v6, 1.0001f, 0.0001f); v7 = fmaf(v7, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f); a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            a4 = fmaf(a4, 1.0001f, 0.0001f); a5 = fmaf(a5, 1.0001f, 0.0001f); a6 = fmaf(a6, 1.0001f, 0.0001f); a7 = fmaf(a7, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f); b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            b4 = fmaf(b4, 1.0001f, 0.0001f); b5 = fmaf(b5, 1.0001f, 0.0001f); b6 = fmaf(b6, 1.0001f, 0.0001f); b7 = fmaf(b7, 1.0001f, 0.0001f);
        }
        data[idx] = hip_bfloat16(v0 + a0 + b0); data[idx + 1] = hip_bfloat16(v1 + a1 + b1);
        data[idx + 2] = hip_bfloat16(v2 + a2 + b2); data[idx + 3] = hip_bfloat16(v3 + a3 + b3);
        data[idx + 4] = hip_bfloat16(v4 + a4 + b4); data[idx + 5] = hip_bfloat16(v5 + a5 + b5);
        data[idx + 6] = hip_bfloat16(v6 + a6 + b6); data[idx + 7] = hip_bfloat16(v7 + a7 + b7);
    }
}

__global__ void roofline_bf16_vec_ai_30p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = float(data[idx]), v1 = float(data[idx + 1]), v2 = float(data[idx + 2]), v3 = float(data[idx + 3]);
        float v4 = float(data[idx + 4]), v5 = float(data[idx + 5]), v6 = float(data[idx + 6]), v7 = float(data[idx + 7]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3, a4 = v4, a5 = v5, a6 = v6, a7 = v7;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3, b4 = v4, b5 = v5, b6 = v6, b7 = v7;
        #pragma unroll
        for (int i = 0; i < 20; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f); v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            v4 = fmaf(v4, 1.0001f, 0.0001f); v5 = fmaf(v5, 1.0001f, 0.0001f); v6 = fmaf(v6, 1.0001f, 0.0001f); v7 = fmaf(v7, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f); a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            a4 = fmaf(a4, 1.0001f, 0.0001f); a5 = fmaf(a5, 1.0001f, 0.0001f); a6 = fmaf(a6, 1.0001f, 0.0001f); a7 = fmaf(a7, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f); b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            b4 = fmaf(b4, 1.0001f, 0.0001f); b5 = fmaf(b5, 1.0001f, 0.0001f); b6 = fmaf(b6, 1.0001f, 0.0001f); b7 = fmaf(b7, 1.0001f, 0.0001f);
        }
        data[idx] = hip_bfloat16(v0 + a0 + b0); data[idx + 1] = hip_bfloat16(v1 + a1 + b1);
        data[idx + 2] = hip_bfloat16(v2 + a2 + b2); data[idx + 3] = hip_bfloat16(v3 + a3 + b3);
        data[idx + 4] = hip_bfloat16(v4 + a4 + b4); data[idx + 5] = hip_bfloat16(v5 + a5 + b5);
        data[idx + 6] = hip_bfloat16(v6 + a6 + b6); data[idx + 7] = hip_bfloat16(v7 + a7 + b7);
    }
}

/* AI=40: 27 iterations → 162 FLOPs/elem */
__global__ void roofline_bf16_vec_ai_40p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = float(data[idx]), v1 = float(data[idx + 1]), v2 = float(data[idx + 2]), v3 = float(data[idx + 3]);
        float v4 = float(data[idx + 4]), v5 = float(data[idx + 5]), v6 = float(data[idx + 6]), v7 = float(data[idx + 7]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3, a4 = v4, a5 = v5, a6 = v6, a7 = v7;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3, b4 = v4, b5 = v5, b6 = v6, b7 = v7;
        #pragma unroll
        for (int i = 0; i < 27; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f); v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            v4 = fmaf(v4, 1.0001f, 0.0001f); v5 = fmaf(v5, 1.0001f, 0.0001f); v6 = fmaf(v6, 1.0001f, 0.0001f); v7 = fmaf(v7, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f); a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            a4 = fmaf(a4, 1.0001f, 0.0001f); a5 = fmaf(a5, 1.0001f, 0.0001f); a6 = fmaf(a6, 1.0001f, 0.0001f); a7 = fmaf(a7, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f); b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            b4 = fmaf(b4, 1.0001f, 0.0001f); b5 = fmaf(b5, 1.0001f, 0.0001f); b6 = fmaf(b6, 1.0001f, 0.0001f); b7 = fmaf(b7, 1.0001f, 0.0001f);
        }
        data[idx] = hip_bfloat16(v0 + a0 + b0); data[idx + 1] = hip_bfloat16(v1 + a1 + b1);
        data[idx + 2] = hip_bfloat16(v2 + a2 + b2); data[idx + 3] = hip_bfloat16(v3 + a3 + b3);
        data[idx + 4] = hip_bfloat16(v4 + a4 + b4); data[idx + 5] = hip_bfloat16(v5 + a5 + b5);
        data[idx + 6] = hip_bfloat16(v6 + a6 + b6); data[idx + 7] = hip_bfloat16(v7 + a7 + b7);
    }
}

/* AI=50: 33 iterations → 198 FLOPs/elem */
__global__ void roofline_bf16_vec_ai_50p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = float(data[idx]), v1 = float(data[idx + 1]), v2 = float(data[idx + 2]), v3 = float(data[idx + 3]);
        float v4 = float(data[idx + 4]), v5 = float(data[idx + 5]), v6 = float(data[idx + 6]), v7 = float(data[idx + 7]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3, a4 = v4, a5 = v5, a6 = v6, a7 = v7;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3, b4 = v4, b5 = v5, b6 = v6, b7 = v7;
        #pragma unroll
        for (int i = 0; i < 33; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f); v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            v4 = fmaf(v4, 1.0001f, 0.0001f); v5 = fmaf(v5, 1.0001f, 0.0001f); v6 = fmaf(v6, 1.0001f, 0.0001f); v7 = fmaf(v7, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f); a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            a4 = fmaf(a4, 1.0001f, 0.0001f); a5 = fmaf(a5, 1.0001f, 0.0001f); a6 = fmaf(a6, 1.0001f, 0.0001f); a7 = fmaf(a7, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f); b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            b4 = fmaf(b4, 1.0001f, 0.0001f); b5 = fmaf(b5, 1.0001f, 0.0001f); b6 = fmaf(b6, 1.0001f, 0.0001f); b7 = fmaf(b7, 1.0001f, 0.0001f);
        }
        data[idx] = hip_bfloat16(v0 + a0 + b0); data[idx + 1] = hip_bfloat16(v1 + a1 + b1);
        data[idx + 2] = hip_bfloat16(v2 + a2 + b2); data[idx + 3] = hip_bfloat16(v3 + a3 + b3);
        data[idx + 4] = hip_bfloat16(v4 + a4 + b4); data[idx + 5] = hip_bfloat16(v5 + a5 + b5);
        data[idx + 6] = hip_bfloat16(v6 + a6 + b6); data[idx + 7] = hip_bfloat16(v7 + a7 + b7);
    }
}

/* AI=60: 40 iterations → 240 FLOPs/elem */
__global__ void roofline_bf16_vec_ai_60p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = float(data[idx]), v1 = float(data[idx + 1]), v2 = float(data[idx + 2]), v3 = float(data[idx + 3]);
        float v4 = float(data[idx + 4]), v5 = float(data[idx + 5]), v6 = float(data[idx + 6]), v7 = float(data[idx + 7]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3, a4 = v4, a5 = v5, a6 = v6, a7 = v7;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3, b4 = v4, b5 = v5, b6 = v6, b7 = v7;
        #pragma unroll
        for (int i = 0; i < 40; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f); v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            v4 = fmaf(v4, 1.0001f, 0.0001f); v5 = fmaf(v5, 1.0001f, 0.0001f); v6 = fmaf(v6, 1.0001f, 0.0001f); v7 = fmaf(v7, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f); a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            a4 = fmaf(a4, 1.0001f, 0.0001f); a5 = fmaf(a5, 1.0001f, 0.0001f); a6 = fmaf(a6, 1.0001f, 0.0001f); a7 = fmaf(a7, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f); b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            b4 = fmaf(b4, 1.0001f, 0.0001f); b5 = fmaf(b5, 1.0001f, 0.0001f); b6 = fmaf(b6, 1.0001f, 0.0001f); b7 = fmaf(b7, 1.0001f, 0.0001f);
        }
        data[idx] = hip_bfloat16(v0 + a0 + b0); data[idx + 1] = hip_bfloat16(v1 + a1 + b1);
        data[idx + 2] = hip_bfloat16(v2 + a2 + b2); data[idx + 3] = hip_bfloat16(v3 + a3 + b3);
        data[idx + 4] = hip_bfloat16(v4 + a4 + b4); data[idx + 5] = hip_bfloat16(v5 + a5 + b5);
        data[idx + 6] = hip_bfloat16(v6 + a6 + b6); data[idx + 7] = hip_bfloat16(v7 + a7 + b7);
    }
}

/* AI=70: 47 iterations → 282 FLOPs/elem */
__global__ void roofline_bf16_vec_ai_70p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = float(data[idx]), v1 = float(data[idx + 1]), v2 = float(data[idx + 2]), v3 = float(data[idx + 3]);
        float v4 = float(data[idx + 4]), v5 = float(data[idx + 5]), v6 = float(data[idx + 6]), v7 = float(data[idx + 7]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3, a4 = v4, a5 = v5, a6 = v6, a7 = v7;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3, b4 = v4, b5 = v5, b6 = v6, b7 = v7;
        #pragma unroll
        for (int i = 0; i < 47; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f); v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            v4 = fmaf(v4, 1.0001f, 0.0001f); v5 = fmaf(v5, 1.0001f, 0.0001f); v6 = fmaf(v6, 1.0001f, 0.0001f); v7 = fmaf(v7, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f); a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            a4 = fmaf(a4, 1.0001f, 0.0001f); a5 = fmaf(a5, 1.0001f, 0.0001f); a6 = fmaf(a6, 1.0001f, 0.0001f); a7 = fmaf(a7, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f); b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            b4 = fmaf(b4, 1.0001f, 0.0001f); b5 = fmaf(b5, 1.0001f, 0.0001f); b6 = fmaf(b6, 1.0001f, 0.0001f); b7 = fmaf(b7, 1.0001f, 0.0001f);
        }
        data[idx] = hip_bfloat16(v0 + a0 + b0); data[idx + 1] = hip_bfloat16(v1 + a1 + b1);
        data[idx + 2] = hip_bfloat16(v2 + a2 + b2); data[idx + 3] = hip_bfloat16(v3 + a3 + b3);
        data[idx + 4] = hip_bfloat16(v4 + a4 + b4); data[idx + 5] = hip_bfloat16(v5 + a5 + b5);
        data[idx + 6] = hip_bfloat16(v6 + a6 + b6); data[idx + 7] = hip_bfloat16(v7 + a7 + b7);
    }
}

/* AI=80: 53 iterations → 318 FLOPs/elem */
__global__ void roofline_bf16_vec_ai_80p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = float(data[idx]), v1 = float(data[idx + 1]), v2 = float(data[idx + 2]), v3 = float(data[idx + 3]);
        float v4 = float(data[idx + 4]), v5 = float(data[idx + 5]), v6 = float(data[idx + 6]), v7 = float(data[idx + 7]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3, a4 = v4, a5 = v5, a6 = v6, a7 = v7;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3, b4 = v4, b5 = v5, b6 = v6, b7 = v7;
        #pragma unroll
        for (int i = 0; i < 53; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f); v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            v4 = fmaf(v4, 1.0001f, 0.0001f); v5 = fmaf(v5, 1.0001f, 0.0001f); v6 = fmaf(v6, 1.0001f, 0.0001f); v7 = fmaf(v7, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f); a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            a4 = fmaf(a4, 1.0001f, 0.0001f); a5 = fmaf(a5, 1.0001f, 0.0001f); a6 = fmaf(a6, 1.0001f, 0.0001f); a7 = fmaf(a7, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f); b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            b4 = fmaf(b4, 1.0001f, 0.0001f); b5 = fmaf(b5, 1.0001f, 0.0001f); b6 = fmaf(b6, 1.0001f, 0.0001f); b7 = fmaf(b7, 1.0001f, 0.0001f);
        }
        data[idx] = hip_bfloat16(v0 + a0 + b0); data[idx + 1] = hip_bfloat16(v1 + a1 + b1);
        data[idx + 2] = hip_bfloat16(v2 + a2 + b2); data[idx + 3] = hip_bfloat16(v3 + a3 + b3);
        data[idx + 4] = hip_bfloat16(v4 + a4 + b4); data[idx + 5] = hip_bfloat16(v5 + a5 + b5);
        data[idx + 6] = hip_bfloat16(v6 + a6 + b6); data[idx + 7] = hip_bfloat16(v7 + a7 + b7);
    }
}

/* AI=90: 60 iterations → 360 FLOPs/elem */
__global__ void roofline_bf16_vec_ai_90p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = float(data[idx]), v1 = float(data[idx + 1]), v2 = float(data[idx + 2]), v3 = float(data[idx + 3]);
        float v4 = float(data[idx + 4]), v5 = float(data[idx + 5]), v6 = float(data[idx + 6]), v7 = float(data[idx + 7]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3, a4 = v4, a5 = v5, a6 = v6, a7 = v7;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3, b4 = v4, b5 = v5, b6 = v6, b7 = v7;
        #pragma unroll
        for (int i = 0; i < 60; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f); v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            v4 = fmaf(v4, 1.0001f, 0.0001f); v5 = fmaf(v5, 1.0001f, 0.0001f); v6 = fmaf(v6, 1.0001f, 0.0001f); v7 = fmaf(v7, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f); a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            a4 = fmaf(a4, 1.0001f, 0.0001f); a5 = fmaf(a5, 1.0001f, 0.0001f); a6 = fmaf(a6, 1.0001f, 0.0001f); a7 = fmaf(a7, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f); b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            b4 = fmaf(b4, 1.0001f, 0.0001f); b5 = fmaf(b5, 1.0001f, 0.0001f); b6 = fmaf(b6, 1.0001f, 0.0001f); b7 = fmaf(b7, 1.0001f, 0.0001f);
        }
        data[idx] = hip_bfloat16(v0 + a0 + b0); data[idx + 1] = hip_bfloat16(v1 + a1 + b1);
        data[idx + 2] = hip_bfloat16(v2 + a2 + b2); data[idx + 3] = hip_bfloat16(v3 + a3 + b3);
        data[idx + 4] = hip_bfloat16(v4 + a4 + b4); data[idx + 5] = hip_bfloat16(v5 + a5 + b5);
        data[idx + 6] = hip_bfloat16(v6 + a6 + b6); data[idx + 7] = hip_bfloat16(v7 + a7 + b7);
    }
}

/*
 * ============================================================================
 * HIP Kernels for FP16 Vector Operations at Different Arithmetic Intensities
 * ============================================================================
 *
 * For FP16: 1 element = 2 bytes, read + write = 4 bytes total memory traffic
 * Using __half type from hip_fp16.h
 */

/* AI = 0.1: Very memory-bound
 * Each thread processes 8 elements for better memory coalescing */
__global__ void roofline_fp16_vec_ai_0p1(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = __half2float(data[idx]);
        float v1 = __half2float(data[idx + 1]);
        float v2 = __half2float(data[idx + 2]);
        float v3 = __half2float(data[idx + 3]);
        float v4 = __half2float(data[idx + 4]);
        float v5 = __half2float(data[idx + 5]);
        float v6 = __half2float(data[idx + 6]);
        float v7 = __half2float(data[idx + 7]);
        
        v0 = fmaf(v0, 1.0001f, 0.0001f);
        v1 = fmaf(v1, 1.0001f, 0.0001f);
        v2 = fmaf(v2, 1.0001f, 0.0001f);
        v3 = fmaf(v3, 1.0001f, 0.0001f);
        v4 = fmaf(v4, 1.0001f, 0.0001f);
        v5 = fmaf(v5, 1.0001f, 0.0001f);
        v6 = fmaf(v6, 1.0001f, 0.0001f);
        v7 = fmaf(v7, 1.0001f, 0.0001f);
        
        data[idx] = __float2half(v0);
        data[idx + 1] = __float2half(v1);
        data[idx + 2] = __float2half(v2);
        data[idx + 3] = __float2half(v3);
        data[idx + 4] = __float2half(v4);
        data[idx + 5] = __float2half(v5);
        data[idx + 6] = __float2half(v6);
        data[idx + 7] = __float2half(v7);
    }
}

/* AI = 0.25: Memory-bound
 * Each thread processes 8 elements for better memory coalescing */
__global__ void roofline_fp16_vec_ai_0p25(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = __half2float(data[idx]);
        float v1 = __half2float(data[idx + 1]);
        float v2 = __half2float(data[idx + 2]);
        float v3 = __half2float(data[idx + 3]);
        float v4 = __half2float(data[idx + 4]);
        float v5 = __half2float(data[idx + 5]);
        float v6 = __half2float(data[idx + 6]);
        float v7 = __half2float(data[idx + 7]);
        
        v0 = fmaf(v0, 1.0001f, 0.0001f);
        v1 = fmaf(v1, 1.0001f, 0.0001f);
        v2 = fmaf(v2, 1.0001f, 0.0001f);
        v3 = fmaf(v3, 1.0001f, 0.0001f);
        v4 = fmaf(v4, 1.0001f, 0.0001f);
        v5 = fmaf(v5, 1.0001f, 0.0001f);
        v6 = fmaf(v6, 1.0001f, 0.0001f);
        v7 = fmaf(v7, 1.0001f, 0.0001f);
        
        data[idx] = __float2half(v0);
        data[idx + 1] = __float2half(v1);
        data[idx + 2] = __float2half(v2);
        data[idx + 3] = __float2half(v3);
        data[idx + 4] = __float2half(v4);
        data[idx + 5] = __float2half(v5);
        data[idx + 6] = __float2half(v6);
        data[idx + 7] = __float2half(v7);
    }
}

/* AI = 0.5: Memory-bound
 * Each thread processes 8 elements */
__global__ void roofline_fp16_vec_ai_0p5(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = __half2float(data[idx]);
        float v1 = __half2float(data[idx + 1]);
        float v2 = __half2float(data[idx + 2]);
        float v3 = __half2float(data[idx + 3]);
        float v4 = __half2float(data[idx + 4]);
        float v5 = __half2float(data[idx + 5]);
        float v6 = __half2float(data[idx + 6]);
        float v7 = __half2float(data[idx + 7]);
        
        v0 = fmaf(v0, 1.0001f, 0.0001f);
        v1 = fmaf(v1, 1.0001f, 0.0001f);
        v2 = fmaf(v2, 1.0001f, 0.0001f);
        v3 = fmaf(v3, 1.0001f, 0.0001f);
        v4 = fmaf(v4, 1.0001f, 0.0001f);
        v5 = fmaf(v5, 1.0001f, 0.0001f);
        v6 = fmaf(v6, 1.0001f, 0.0001f);
        v7 = fmaf(v7, 1.0001f, 0.0001f);
        
        data[idx] = __float2half(v0);
        data[idx + 1] = __float2half(v1);
        data[idx + 2] = __float2half(v2);
        data[idx + 3] = __float2half(v3);
        data[idx + 4] = __float2half(v4);
        data[idx + 5] = __float2half(v5);
        data[idx + 6] = __float2half(v6);
        data[idx + 7] = __float2half(v7);
    }
}

/* AI = 1.0: Balanced (4 FLOPs per 4 bytes)
 * Each thread processes 8 elements with 2 FMAs each */
__global__ void roofline_fp16_vec_ai_1p0(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = __half2float(data[idx]);
        float v1 = __half2float(data[idx + 1]);
        float v2 = __half2float(data[idx + 2]);
        float v3 = __half2float(data[idx + 3]);
        float v4 = __half2float(data[idx + 4]);
        float v5 = __half2float(data[idx + 5]);
        float v6 = __half2float(data[idx + 6]);
        float v7 = __half2float(data[idx + 7]);
        
        #pragma unroll
        for (int i = 0; i < 2; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f);
            v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f);
            v3 = fmaf(v3, 1.0001f, 0.0001f);
            v4 = fmaf(v4, 1.0001f, 0.0001f);
            v5 = fmaf(v5, 1.0001f, 0.0001f);
            v6 = fmaf(v6, 1.0001f, 0.0001f);
            v7 = fmaf(v7, 1.0001f, 0.0001f);
        }
        
        data[idx] = __float2half(v0);
        data[idx + 1] = __float2half(v1);
        data[idx + 2] = __float2half(v2);
        data[idx + 3] = __float2half(v3);
        data[idx + 4] = __float2half(v4);
        data[idx + 5] = __float2half(v5);
        data[idx + 6] = __float2half(v6);
        data[idx + 7] = __float2half(v7);
    }
}

/* AI = 2.0: Compute-bound (8 FLOPs per 4 bytes)
 * Each thread processes 4 elements with 8 independent chains for ILP */
__global__ void roofline_fp16_vec_ai_2p0(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = __half2float(data[idx]);
        float v1 = __half2float(data[idx + 1]);
        float v2 = __half2float(data[idx + 2]);
        float v3 = __half2float(data[idx + 3]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        
        #pragma unroll
        for (int i = 0; i < 2; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f);
            v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f);
            v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f);
            a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f);
            a3 = fmaf(a3, 1.0001f, 0.0001f);
        }
        
        data[idx] = __float2half(v0 + a0);
        data[idx + 1] = __float2half(v1 + a1);
        data[idx + 2] = __float2half(v2 + a2);
        data[idx + 3] = __float2half(v3 + a3);
    }
}

/* AI = 3.0: Compute-bound (12 FLOPs per 4 bytes)
 * Each thread processes 4 elements with 8 independent chains for ILP */
__global__ void roofline_fp16_vec_ai_3p0(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = __half2float(data[idx]);
        float v1 = __half2float(data[idx + 1]);
        float v2 = __half2float(data[idx + 2]);
        float v3 = __half2float(data[idx + 3]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        
        #pragma unroll
        for (int i = 0; i < 3; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f);
            v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f);
            v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f);
            a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f);
            a3 = fmaf(a3, 1.0001f, 0.0001f);
        }
        
        data[idx] = __float2half(v0 + a0);
        data[idx + 1] = __float2half(v1 + a1);
        data[idx + 2] = __float2half(v2 + a2);
        data[idx + 3] = __float2half(v3 + a3);
    }
}

/* AI = 4.0: Compute-bound (16 FLOPs per 4 bytes)
 * Each thread processes 4 elements with 8 independent chains for ILP */
__global__ void roofline_fp16_vec_ai_4p0(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = __half2float(data[idx]);
        float v1 = __half2float(data[idx + 1]);
        float v2 = __half2float(data[idx + 2]);
        float v3 = __half2float(data[idx + 3]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f);
            v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f);
            v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f);
            a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f);
            a3 = fmaf(a3, 1.0001f, 0.0001f);
        }
        
        data[idx] = __float2half(v0 + a0);
        data[idx + 1] = __float2half(v1 + a1);
        data[idx + 2] = __float2half(v2 + a2);
        data[idx + 3] = __float2half(v3 + a3);
    }
}

/* AI = 8.0: (32 FLOPs per 4 bytes) → 8 iterations */
__global__ void roofline_fp16_vec_ai_8p0(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = __half2float(data[idx]), v1 = __half2float(data[idx + 1]);
        float v2 = __half2float(data[idx + 2]), v3 = __half2float(data[idx + 3]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        #pragma unroll
        for (int i = 0; i < 8; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
        }
        data[idx] = __float2half(v0 + a0); data[idx + 1] = __float2half(v1 + a1);
        data[idx + 2] = __float2half(v2 + a2); data[idx + 3] = __float2half(v3 + a3);
    }
}

/* FP16 AI = 5.0, 6.0, 7.0 kernels */
__global__ void roofline_fp16_vec_ai_5p0(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = __half2float(data[idx]), v1 = __half2float(data[idx + 1]);
        float v2 = __half2float(data[idx + 2]), v3 = __half2float(data[idx + 3]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        #pragma unroll
        for (int i = 0; i < 5; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
        }
        data[idx] = __float2half(v0 + a0); data[idx + 1] = __float2half(v1 + a1);
        data[idx + 2] = __float2half(v2 + a2); data[idx + 3] = __float2half(v3 + a3);
    }
}

__global__ void roofline_fp16_vec_ai_6p0(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = __half2float(data[idx]), v1 = __half2float(data[idx + 1]);
        float v2 = __half2float(data[idx + 2]), v3 = __half2float(data[idx + 3]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        #pragma unroll
        for (int i = 0; i < 6; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
        }
        data[idx] = __float2half(v0 + a0); data[idx + 1] = __float2half(v1 + a1);
        data[idx + 2] = __float2half(v2 + a2); data[idx + 3] = __float2half(v3 + a3);
    }
}

__global__ void roofline_fp16_vec_ai_7p0(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = __half2float(data[idx]), v1 = __half2float(data[idx + 1]);
        float v2 = __half2float(data[idx + 2]), v3 = __half2float(data[idx + 3]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        #pragma unroll
        for (int i = 0; i < 7; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
        }
        data[idx] = __float2half(v0 + a0); data[idx + 1] = __float2half(v1 + a1);
        data[idx + 2] = __float2half(v2 + a2); data[idx + 3] = __float2half(v3 + a3);
    }
}

/* AI = 9.0: (36 FLOPs per 4 bytes) → 9 iterations */
__global__ void roofline_fp16_vec_ai_9p0(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = __half2float(data[idx]), v1 = __half2float(data[idx + 1]);
        float v2 = __half2float(data[idx + 2]), v3 = __half2float(data[idx + 3]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        #pragma unroll
        for (int i = 0; i < 9; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
        }
        data[idx] = __float2half(v0 + a0); data[idx + 1] = __float2half(v1 + a1);
        data[idx + 2] = __float2half(v2 + a2); data[idx + 3] = __float2half(v3 + a3);
    }
}

/* AI = 10.0: (40 FLOPs per 4 bytes) → 10 iterations */
__global__ void roofline_fp16_vec_ai_10p0(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float v0 = __half2float(data[idx]), v1 = __half2float(data[idx + 1]);
        float v2 = __half2float(data[idx + 2]), v3 = __half2float(data[idx + 3]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3;
        #pragma unroll
        for (int i = 0; i < 10; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f);
            v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f);
            a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
        }
        data[idx] = __float2half(v0 + a0); data[idx + 1] = __float2half(v1 + a1);
        data[idx + 2] = __float2half(v2 + a2); data[idx + 3] = __float2half(v3 + a3);
    }
}

/* FP16 AI = 15.0, 20.0, 25.0, 30.0 kernels
 * 8 elements, 3 chains, 24 FMAs/iter = 6 FLOPs/elem/iter */
__global__ void roofline_fp16_vec_ai_15p0(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = __half2float(data[idx]), v1 = __half2float(data[idx + 1]), v2 = __half2float(data[idx + 2]), v3 = __half2float(data[idx + 3]);
        float v4 = __half2float(data[idx + 4]), v5 = __half2float(data[idx + 5]), v6 = __half2float(data[idx + 6]), v7 = __half2float(data[idx + 7]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3, a4 = v4, a5 = v5, a6 = v6, a7 = v7;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3, b4 = v4, b5 = v5, b6 = v6, b7 = v7;
        #pragma unroll
        for (int i = 0; i < 10; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f); v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            v4 = fmaf(v4, 1.0001f, 0.0001f); v5 = fmaf(v5, 1.0001f, 0.0001f); v6 = fmaf(v6, 1.0001f, 0.0001f); v7 = fmaf(v7, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f); a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            a4 = fmaf(a4, 1.0001f, 0.0001f); a5 = fmaf(a5, 1.0001f, 0.0001f); a6 = fmaf(a6, 1.0001f, 0.0001f); a7 = fmaf(a7, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f); b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            b4 = fmaf(b4, 1.0001f, 0.0001f); b5 = fmaf(b5, 1.0001f, 0.0001f); b6 = fmaf(b6, 1.0001f, 0.0001f); b7 = fmaf(b7, 1.0001f, 0.0001f);
        }
        data[idx] = __float2half(v0 + a0 + b0); data[idx + 1] = __float2half(v1 + a1 + b1);
        data[idx + 2] = __float2half(v2 + a2 + b2); data[idx + 3] = __float2half(v3 + a3 + b3);
        data[idx + 4] = __float2half(v4 + a4 + b4); data[idx + 5] = __float2half(v5 + a5 + b5);
        data[idx + 6] = __float2half(v6 + a6 + b6); data[idx + 7] = __float2half(v7 + a7 + b7);
    }
}

__global__ void roofline_fp16_vec_ai_20p0(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = __half2float(data[idx]), v1 = __half2float(data[idx + 1]), v2 = __half2float(data[idx + 2]), v3 = __half2float(data[idx + 3]);
        float v4 = __half2float(data[idx + 4]), v5 = __half2float(data[idx + 5]), v6 = __half2float(data[idx + 6]), v7 = __half2float(data[idx + 7]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3, a4 = v4, a5 = v5, a6 = v6, a7 = v7;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3, b4 = v4, b5 = v5, b6 = v6, b7 = v7;
        #pragma unroll
        for (int i = 0; i < 14; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f); v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            v4 = fmaf(v4, 1.0001f, 0.0001f); v5 = fmaf(v5, 1.0001f, 0.0001f); v6 = fmaf(v6, 1.0001f, 0.0001f); v7 = fmaf(v7, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f); a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            a4 = fmaf(a4, 1.0001f, 0.0001f); a5 = fmaf(a5, 1.0001f, 0.0001f); a6 = fmaf(a6, 1.0001f, 0.0001f); a7 = fmaf(a7, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f); b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            b4 = fmaf(b4, 1.0001f, 0.0001f); b5 = fmaf(b5, 1.0001f, 0.0001f); b6 = fmaf(b6, 1.0001f, 0.0001f); b7 = fmaf(b7, 1.0001f, 0.0001f);
        }
        data[idx] = __float2half(v0 + a0 + b0); data[idx + 1] = __float2half(v1 + a1 + b1);
        data[idx + 2] = __float2half(v2 + a2 + b2); data[idx + 3] = __float2half(v3 + a3 + b3);
        data[idx + 4] = __float2half(v4 + a4 + b4); data[idx + 5] = __float2half(v5 + a5 + b5);
        data[idx + 6] = __float2half(v6 + a6 + b6); data[idx + 7] = __float2half(v7 + a7 + b7);
    }
}

__global__ void roofline_fp16_vec_ai_25p0(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = __half2float(data[idx]), v1 = __half2float(data[idx + 1]), v2 = __half2float(data[idx + 2]), v3 = __half2float(data[idx + 3]);
        float v4 = __half2float(data[idx + 4]), v5 = __half2float(data[idx + 5]), v6 = __half2float(data[idx + 6]), v7 = __half2float(data[idx + 7]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3, a4 = v4, a5 = v5, a6 = v6, a7 = v7;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3, b4 = v4, b5 = v5, b6 = v6, b7 = v7;
        #pragma unroll
        for (int i = 0; i < 17; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f); v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            v4 = fmaf(v4, 1.0001f, 0.0001f); v5 = fmaf(v5, 1.0001f, 0.0001f); v6 = fmaf(v6, 1.0001f, 0.0001f); v7 = fmaf(v7, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f); a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            a4 = fmaf(a4, 1.0001f, 0.0001f); a5 = fmaf(a5, 1.0001f, 0.0001f); a6 = fmaf(a6, 1.0001f, 0.0001f); a7 = fmaf(a7, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f); b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            b4 = fmaf(b4, 1.0001f, 0.0001f); b5 = fmaf(b5, 1.0001f, 0.0001f); b6 = fmaf(b6, 1.0001f, 0.0001f); b7 = fmaf(b7, 1.0001f, 0.0001f);
        }
        data[idx] = __float2half(v0 + a0 + b0); data[idx + 1] = __float2half(v1 + a1 + b1);
        data[idx + 2] = __float2half(v2 + a2 + b2); data[idx + 3] = __float2half(v3 + a3 + b3);
        data[idx + 4] = __float2half(v4 + a4 + b4); data[idx + 5] = __float2half(v5 + a5 + b5);
        data[idx + 6] = __float2half(v6 + a6 + b6); data[idx + 7] = __float2half(v7 + a7 + b7);
    }
}

__global__ void roofline_fp16_vec_ai_30p0(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = __half2float(data[idx]), v1 = __half2float(data[idx + 1]), v2 = __half2float(data[idx + 2]), v3 = __half2float(data[idx + 3]);
        float v4 = __half2float(data[idx + 4]), v5 = __half2float(data[idx + 5]), v6 = __half2float(data[idx + 6]), v7 = __half2float(data[idx + 7]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3, a4 = v4, a5 = v5, a6 = v6, a7 = v7;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3, b4 = v4, b5 = v5, b6 = v6, b7 = v7;
        #pragma unroll
        for (int i = 0; i < 20; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f); v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            v4 = fmaf(v4, 1.0001f, 0.0001f); v5 = fmaf(v5, 1.0001f, 0.0001f); v6 = fmaf(v6, 1.0001f, 0.0001f); v7 = fmaf(v7, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f); a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            a4 = fmaf(a4, 1.0001f, 0.0001f); a5 = fmaf(a5, 1.0001f, 0.0001f); a6 = fmaf(a6, 1.0001f, 0.0001f); a7 = fmaf(a7, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f); b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            b4 = fmaf(b4, 1.0001f, 0.0001f); b5 = fmaf(b5, 1.0001f, 0.0001f); b6 = fmaf(b6, 1.0001f, 0.0001f); b7 = fmaf(b7, 1.0001f, 0.0001f);
        }
        data[idx] = __float2half(v0 + a0 + b0); data[idx + 1] = __float2half(v1 + a1 + b1);
        data[idx + 2] = __float2half(v2 + a2 + b2); data[idx + 3] = __float2half(v3 + a3 + b3);
        data[idx + 4] = __float2half(v4 + a4 + b4); data[idx + 5] = __float2half(v5 + a5 + b5);
        data[idx + 6] = __float2half(v6 + a6 + b6); data[idx + 7] = __float2half(v7 + a7 + b7);
    }
}

/* AI=40: 27 iterations → 162 FLOPs/elem */
__global__ void roofline_fp16_vec_ai_40p0(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = __half2float(data[idx]), v1 = __half2float(data[idx + 1]), v2 = __half2float(data[idx + 2]), v3 = __half2float(data[idx + 3]);
        float v4 = __half2float(data[idx + 4]), v5 = __half2float(data[idx + 5]), v6 = __half2float(data[idx + 6]), v7 = __half2float(data[idx + 7]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3, a4 = v4, a5 = v5, a6 = v6, a7 = v7;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3, b4 = v4, b5 = v5, b6 = v6, b7 = v7;
        #pragma unroll
        for (int i = 0; i < 27; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f); v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            v4 = fmaf(v4, 1.0001f, 0.0001f); v5 = fmaf(v5, 1.0001f, 0.0001f); v6 = fmaf(v6, 1.0001f, 0.0001f); v7 = fmaf(v7, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f); a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            a4 = fmaf(a4, 1.0001f, 0.0001f); a5 = fmaf(a5, 1.0001f, 0.0001f); a6 = fmaf(a6, 1.0001f, 0.0001f); a7 = fmaf(a7, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f); b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            b4 = fmaf(b4, 1.0001f, 0.0001f); b5 = fmaf(b5, 1.0001f, 0.0001f); b6 = fmaf(b6, 1.0001f, 0.0001f); b7 = fmaf(b7, 1.0001f, 0.0001f);
        }
        data[idx] = __float2half(v0 + a0 + b0); data[idx + 1] = __float2half(v1 + a1 + b1);
        data[idx + 2] = __float2half(v2 + a2 + b2); data[idx + 3] = __float2half(v3 + a3 + b3);
        data[idx + 4] = __float2half(v4 + a4 + b4); data[idx + 5] = __float2half(v5 + a5 + b5);
        data[idx + 6] = __float2half(v6 + a6 + b6); data[idx + 7] = __float2half(v7 + a7 + b7);
    }
}

/* AI=50: 33 iterations → 198 FLOPs/elem */
__global__ void roofline_fp16_vec_ai_50p0(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = __half2float(data[idx]), v1 = __half2float(data[idx + 1]), v2 = __half2float(data[idx + 2]), v3 = __half2float(data[idx + 3]);
        float v4 = __half2float(data[idx + 4]), v5 = __half2float(data[idx + 5]), v6 = __half2float(data[idx + 6]), v7 = __half2float(data[idx + 7]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3, a4 = v4, a5 = v5, a6 = v6, a7 = v7;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3, b4 = v4, b5 = v5, b6 = v6, b7 = v7;
        #pragma unroll
        for (int i = 0; i < 33; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f); v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            v4 = fmaf(v4, 1.0001f, 0.0001f); v5 = fmaf(v5, 1.0001f, 0.0001f); v6 = fmaf(v6, 1.0001f, 0.0001f); v7 = fmaf(v7, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f); a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            a4 = fmaf(a4, 1.0001f, 0.0001f); a5 = fmaf(a5, 1.0001f, 0.0001f); a6 = fmaf(a6, 1.0001f, 0.0001f); a7 = fmaf(a7, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f); b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            b4 = fmaf(b4, 1.0001f, 0.0001f); b5 = fmaf(b5, 1.0001f, 0.0001f); b6 = fmaf(b6, 1.0001f, 0.0001f); b7 = fmaf(b7, 1.0001f, 0.0001f);
        }
        data[idx] = __float2half(v0 + a0 + b0); data[idx + 1] = __float2half(v1 + a1 + b1);
        data[idx + 2] = __float2half(v2 + a2 + b2); data[idx + 3] = __float2half(v3 + a3 + b3);
        data[idx + 4] = __float2half(v4 + a4 + b4); data[idx + 5] = __float2half(v5 + a5 + b5);
        data[idx + 6] = __float2half(v6 + a6 + b6); data[idx + 7] = __float2half(v7 + a7 + b7);
    }
}

/* AI=60: 40 iterations → 240 FLOPs/elem */
__global__ void roofline_fp16_vec_ai_60p0(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = __half2float(data[idx]), v1 = __half2float(data[idx + 1]), v2 = __half2float(data[idx + 2]), v3 = __half2float(data[idx + 3]);
        float v4 = __half2float(data[idx + 4]), v5 = __half2float(data[idx + 5]), v6 = __half2float(data[idx + 6]), v7 = __half2float(data[idx + 7]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3, a4 = v4, a5 = v5, a6 = v6, a7 = v7;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3, b4 = v4, b5 = v5, b6 = v6, b7 = v7;
        #pragma unroll
        for (int i = 0; i < 40; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f); v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            v4 = fmaf(v4, 1.0001f, 0.0001f); v5 = fmaf(v5, 1.0001f, 0.0001f); v6 = fmaf(v6, 1.0001f, 0.0001f); v7 = fmaf(v7, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f); a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            a4 = fmaf(a4, 1.0001f, 0.0001f); a5 = fmaf(a5, 1.0001f, 0.0001f); a6 = fmaf(a6, 1.0001f, 0.0001f); a7 = fmaf(a7, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f); b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            b4 = fmaf(b4, 1.0001f, 0.0001f); b5 = fmaf(b5, 1.0001f, 0.0001f); b6 = fmaf(b6, 1.0001f, 0.0001f); b7 = fmaf(b7, 1.0001f, 0.0001f);
        }
        data[idx] = __float2half(v0 + a0 + b0); data[idx + 1] = __float2half(v1 + a1 + b1);
        data[idx + 2] = __float2half(v2 + a2 + b2); data[idx + 3] = __float2half(v3 + a3 + b3);
        data[idx + 4] = __float2half(v4 + a4 + b4); data[idx + 5] = __float2half(v5 + a5 + b5);
        data[idx + 6] = __float2half(v6 + a6 + b6); data[idx + 7] = __float2half(v7 + a7 + b7);
    }
}

/* AI=70: 47 iterations → 282 FLOPs/elem */
__global__ void roofline_fp16_vec_ai_70p0(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = __half2float(data[idx]), v1 = __half2float(data[idx + 1]), v2 = __half2float(data[idx + 2]), v3 = __half2float(data[idx + 3]);
        float v4 = __half2float(data[idx + 4]), v5 = __half2float(data[idx + 5]), v6 = __half2float(data[idx + 6]), v7 = __half2float(data[idx + 7]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3, a4 = v4, a5 = v5, a6 = v6, a7 = v7;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3, b4 = v4, b5 = v5, b6 = v6, b7 = v7;
        #pragma unroll
        for (int i = 0; i < 47; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f); v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            v4 = fmaf(v4, 1.0001f, 0.0001f); v5 = fmaf(v5, 1.0001f, 0.0001f); v6 = fmaf(v6, 1.0001f, 0.0001f); v7 = fmaf(v7, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f); a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            a4 = fmaf(a4, 1.0001f, 0.0001f); a5 = fmaf(a5, 1.0001f, 0.0001f); a6 = fmaf(a6, 1.0001f, 0.0001f); a7 = fmaf(a7, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f); b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            b4 = fmaf(b4, 1.0001f, 0.0001f); b5 = fmaf(b5, 1.0001f, 0.0001f); b6 = fmaf(b6, 1.0001f, 0.0001f); b7 = fmaf(b7, 1.0001f, 0.0001f);
        }
        data[idx] = __float2half(v0 + a0 + b0); data[idx + 1] = __float2half(v1 + a1 + b1);
        data[idx + 2] = __float2half(v2 + a2 + b2); data[idx + 3] = __float2half(v3 + a3 + b3);
        data[idx + 4] = __float2half(v4 + a4 + b4); data[idx + 5] = __float2half(v5 + a5 + b5);
        data[idx + 6] = __float2half(v6 + a6 + b6); data[idx + 7] = __float2half(v7 + a7 + b7);
    }
}

/* AI=80: 53 iterations → 318 FLOPs/elem */
__global__ void roofline_fp16_vec_ai_80p0(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = __half2float(data[idx]), v1 = __half2float(data[idx + 1]), v2 = __half2float(data[idx + 2]), v3 = __half2float(data[idx + 3]);
        float v4 = __half2float(data[idx + 4]), v5 = __half2float(data[idx + 5]), v6 = __half2float(data[idx + 6]), v7 = __half2float(data[idx + 7]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3, a4 = v4, a5 = v5, a6 = v6, a7 = v7;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3, b4 = v4, b5 = v5, b6 = v6, b7 = v7;
        #pragma unroll
        for (int i = 0; i < 53; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f); v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            v4 = fmaf(v4, 1.0001f, 0.0001f); v5 = fmaf(v5, 1.0001f, 0.0001f); v6 = fmaf(v6, 1.0001f, 0.0001f); v7 = fmaf(v7, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f); a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            a4 = fmaf(a4, 1.0001f, 0.0001f); a5 = fmaf(a5, 1.0001f, 0.0001f); a6 = fmaf(a6, 1.0001f, 0.0001f); a7 = fmaf(a7, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f); b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            b4 = fmaf(b4, 1.0001f, 0.0001f); b5 = fmaf(b5, 1.0001f, 0.0001f); b6 = fmaf(b6, 1.0001f, 0.0001f); b7 = fmaf(b7, 1.0001f, 0.0001f);
        }
        data[idx] = __float2half(v0 + a0 + b0); data[idx + 1] = __float2half(v1 + a1 + b1);
        data[idx + 2] = __float2half(v2 + a2 + b2); data[idx + 3] = __float2half(v3 + a3 + b3);
        data[idx + 4] = __float2half(v4 + a4 + b4); data[idx + 5] = __float2half(v5 + a5 + b5);
        data[idx + 6] = __float2half(v6 + a6 + b6); data[idx + 7] = __float2half(v7 + a7 + b7);
    }
}

/* AI=90: 60 iterations → 360 FLOPs/elem */
__global__ void roofline_fp16_vec_ai_90p0(__half *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 8;
    if (idx + 7 < n) {
        float v0 = __half2float(data[idx]), v1 = __half2float(data[idx + 1]), v2 = __half2float(data[idx + 2]), v3 = __half2float(data[idx + 3]);
        float v4 = __half2float(data[idx + 4]), v5 = __half2float(data[idx + 5]), v6 = __half2float(data[idx + 6]), v7 = __half2float(data[idx + 7]);
        float a0 = v0, a1 = v1, a2 = v2, a3 = v3, a4 = v4, a5 = v5, a6 = v6, a7 = v7;
        float b0 = v0, b1 = v1, b2 = v2, b3 = v3, b4 = v4, b5 = v5, b6 = v6, b7 = v7;
        #pragma unroll
        for (int i = 0; i < 60; i++) {
            v0 = fmaf(v0, 1.0001f, 0.0001f); v1 = fmaf(v1, 1.0001f, 0.0001f); v2 = fmaf(v2, 1.0001f, 0.0001f); v3 = fmaf(v3, 1.0001f, 0.0001f);
            v4 = fmaf(v4, 1.0001f, 0.0001f); v5 = fmaf(v5, 1.0001f, 0.0001f); v6 = fmaf(v6, 1.0001f, 0.0001f); v7 = fmaf(v7, 1.0001f, 0.0001f);
            a0 = fmaf(a0, 1.0001f, 0.0001f); a1 = fmaf(a1, 1.0001f, 0.0001f); a2 = fmaf(a2, 1.0001f, 0.0001f); a3 = fmaf(a3, 1.0001f, 0.0001f);
            a4 = fmaf(a4, 1.0001f, 0.0001f); a5 = fmaf(a5, 1.0001f, 0.0001f); a6 = fmaf(a6, 1.0001f, 0.0001f); a7 = fmaf(a7, 1.0001f, 0.0001f);
            b0 = fmaf(b0, 1.0001f, 0.0001f); b1 = fmaf(b1, 1.0001f, 0.0001f); b2 = fmaf(b2, 1.0001f, 0.0001f); b3 = fmaf(b3, 1.0001f, 0.0001f);
            b4 = fmaf(b4, 1.0001f, 0.0001f); b5 = fmaf(b5, 1.0001f, 0.0001f); b6 = fmaf(b6, 1.0001f, 0.0001f); b7 = fmaf(b7, 1.0001f, 0.0001f);
        }
        data[idx] = __float2half(v0 + a0 + b0); data[idx + 1] = __float2half(v1 + a1 + b1);
        data[idx + 2] = __float2half(v2 + a2 + b2); data[idx + 3] = __float2half(v3 + a3 + b3);
        data[idx + 4] = __float2half(v4 + a4 + b4); data[idx + 5] = __float2half(v5 + a5 + b5);
        data[idx + 6] = __float2half(v6 + a6 + b6); data[idx + 7] = __float2half(v7 + a7 + b7);
    }
}

/*
 * ============================================================================
 * HIP Kernels for FP64 Vector Operations at Different Arithmetic Intensities
 * ============================================================================
 *
 * For FP64: 1 element = 8 bytes, read + write = 16 bytes total memory traffic
 */

/* FP64 AI = 0.1 to 1.0: Memory-bound kernels */
__global__ void roofline_fp64_vec_ai_0p1(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1];
        v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001);
        data[idx] = v0; data[idx + 1] = v1;
    }
}

__global__ void roofline_fp64_vec_ai_0p25(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1];
        v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001);
        v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001);
        data[idx] = v0; data[idx + 1] = v1;
    }
}

__global__ void roofline_fp64_vec_ai_0p5(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1];
        #pragma unroll
        for (int i = 0; i < 4; i++) { v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001); }
        data[idx] = v0; data[idx + 1] = v1;
    }
}

__global__ void roofline_fp64_vec_ai_1p0(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1];
        #pragma unroll
        for (int i = 0; i < 8; i++) { v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001); }
        data[idx] = v0; data[idx + 1] = v1;
    }
}

/* FP64 AI = 2.0 to 7.0: Compute-bound kernels */
__global__ void roofline_fp64_vec_ai_2p0(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1], a0 = v0, a1 = v1;
        #pragma unroll
        for (int i = 0; i < 8; i++) { v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001); a0 = fma(a0, 1.0001, 0.0001); a1 = fma(a1, 1.0001, 0.0001); }
        data[idx] = v0 + a0; data[idx + 1] = v1 + a1;
    }
}

__global__ void roofline_fp64_vec_ai_3p0(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1], a0 = v0, a1 = v1;
        #pragma unroll
        for (int i = 0; i < 12; i++) { v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001); a0 = fma(a0, 1.0001, 0.0001); a1 = fma(a1, 1.0001, 0.0001); }
        data[idx] = v0 + a0; data[idx + 1] = v1 + a1;
    }
}

__global__ void roofline_fp64_vec_ai_4p0(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1], a0 = v0, a1 = v1;
        #pragma unroll
        for (int i = 0; i < 16; i++) { v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001); a0 = fma(a0, 1.0001, 0.0001); a1 = fma(a1, 1.0001, 0.0001); }
        data[idx] = v0 + a0; data[idx + 1] = v1 + a1;
    }
}

__global__ void roofline_fp64_vec_ai_5p0(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1], a0 = v0, a1 = v1;
        #pragma unroll
        for (int i = 0; i < 20; i++) { v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001); a0 = fma(a0, 1.0001, 0.0001); a1 = fma(a1, 1.0001, 0.0001); }
        data[idx] = v0 + a0; data[idx + 1] = v1 + a1;
    }
}

__global__ void roofline_fp64_vec_ai_6p0(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1], a0 = v0, a1 = v1;
        #pragma unroll
        for (int i = 0; i < 24; i++) { v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001); a0 = fma(a0, 1.0001, 0.0001); a1 = fma(a1, 1.0001, 0.0001); }
        data[idx] = v0 + a0; data[idx + 1] = v1 + a1;
    }
}

__global__ void roofline_fp64_vec_ai_7p0(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1], a0 = v0, a1 = v1;
        #pragma unroll
        for (int i = 0; i < 28; i++) { v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001); a0 = fma(a0, 1.0001, 0.0001); a1 = fma(a1, 1.0001, 0.0001); }
        data[idx] = v0 + a0; data[idx + 1] = v1 + a1;
    }
}

/* AI = 8.0: (128 FLOPs per 16 bytes) → 16 iterations */
__global__ void roofline_fp64_vec_ai_8p0(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1], a0 = v0, a1 = v1, b0 = v0, b1 = v1, c0 = v0, c1 = v1;
        #pragma unroll
        for (int i = 0; i < 16; i++) {
            v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001);
            a0 = fma(a0, 1.0001, 0.0001); a1 = fma(a1, 1.0001, 0.0001);
            b0 = fma(b0, 1.0001, 0.0001); b1 = fma(b1, 1.0001, 0.0001);
            c0 = fma(c0, 1.0001, 0.0001); c1 = fma(c1, 1.0001, 0.0001);
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
    }
}

/* AI = 9.0: (144 FLOPs per 16 bytes) → 18 iterations */
__global__ void roofline_fp64_vec_ai_9p0(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1], a0 = v0, a1 = v1, b0 = v0, b1 = v1, c0 = v0, c1 = v1;
        #pragma unroll
        for (int i = 0; i < 18; i++) {
            v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001);
            a0 = fma(a0, 1.0001, 0.0001); a1 = fma(a1, 1.0001, 0.0001);
            b0 = fma(b0, 1.0001, 0.0001); b1 = fma(b1, 1.0001, 0.0001);
            c0 = fma(c0, 1.0001, 0.0001); c1 = fma(c1, 1.0001, 0.0001);
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
    }
}

/* AI = 10.0: (160 FLOPs per 16 bytes) → 20 iterations */
__global__ void roofline_fp64_vec_ai_10p0(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1], a0 = v0, a1 = v1, b0 = v0, b1 = v1, c0 = v0, c1 = v1;
        #pragma unroll
        for (int i = 0; i < 20; i++) {
            v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001);
            a0 = fma(a0, 1.0001, 0.0001); a1 = fma(a1, 1.0001, 0.0001);
            b0 = fma(b0, 1.0001, 0.0001); b1 = fma(b1, 1.0001, 0.0001);
            c0 = fma(c0, 1.0001, 0.0001); c1 = fma(c1, 1.0001, 0.0001);
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
    }
}

/* FP64 AI = 15.0, 20.0, 25.0, 30.0 kernels */
__global__ void roofline_fp64_vec_ai_15p0(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1], a0 = v0, a1 = v1, b0 = v0, b1 = v1, c0 = v0, c1 = v1;
        #pragma unroll
        for (int i = 0; i < 60; i++) {
            v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001);
            a0 = fma(a0, 1.0001, 0.0001); a1 = fma(a1, 1.0001, 0.0001);
            b0 = fma(b0, 1.0001, 0.0001); b1 = fma(b1, 1.0001, 0.0001);
            c0 = fma(c0, 1.0001, 0.0001); c1 = fma(c1, 1.0001, 0.0001);
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
    }
}

__global__ void roofline_fp64_vec_ai_20p0(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1], a0 = v0, a1 = v1, b0 = v0, b1 = v1, c0 = v0, c1 = v1;
        #pragma unroll
        for (int i = 0; i < 80; i++) {
            v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001);
            a0 = fma(a0, 1.0001, 0.0001); a1 = fma(a1, 1.0001, 0.0001);
            b0 = fma(b0, 1.0001, 0.0001); b1 = fma(b1, 1.0001, 0.0001);
            c0 = fma(c0, 1.0001, 0.0001); c1 = fma(c1, 1.0001, 0.0001);
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
    }
}

__global__ void roofline_fp64_vec_ai_25p0(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1], a0 = v0, a1 = v1, b0 = v0, b1 = v1, c0 = v0, c1 = v1;
        #pragma unroll
        for (int i = 0; i < 100; i++) {
            v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001);
            a0 = fma(a0, 1.0001, 0.0001); a1 = fma(a1, 1.0001, 0.0001);
            b0 = fma(b0, 1.0001, 0.0001); b1 = fma(b1, 1.0001, 0.0001);
            c0 = fma(c0, 1.0001, 0.0001); c1 = fma(c1, 1.0001, 0.0001);
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
    }
}

__global__ void roofline_fp64_vec_ai_30p0(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1], a0 = v0, a1 = v1, b0 = v0, b1 = v1, c0 = v0, c1 = v1;
        #pragma unroll
        for (int i = 0; i < 120; i++) {
            v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001);
            a0 = fma(a0, 1.0001, 0.0001); a1 = fma(a1, 1.0001, 0.0001);
            b0 = fma(b0, 1.0001, 0.0001); b1 = fma(b1, 1.0001, 0.0001);
            c0 = fma(c0, 1.0001, 0.0001); c1 = fma(c1, 1.0001, 0.0001);
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
    }
}

/* AI=40: 80 iterations → 640 FLOPs/elem */
__global__ void roofline_fp64_vec_ai_40p0(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1], a0 = v0, a1 = v1, b0 = v0, b1 = v1, c0 = v0, c1 = v1;
        #pragma unroll
        for (int i = 0; i < 80; i++) {
            v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001);
            a0 = fma(a0, 1.0001, 0.0001); a1 = fma(a1, 1.0001, 0.0001);
            b0 = fma(b0, 1.0001, 0.0001); b1 = fma(b1, 1.0001, 0.0001);
            c0 = fma(c0, 1.0001, 0.0001); c1 = fma(c1, 1.0001, 0.0001);
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
    }
}

/* AI=50: 100 iterations → 800 FLOPs/elem */
__global__ void roofline_fp64_vec_ai_50p0(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1], a0 = v0, a1 = v1, b0 = v0, b1 = v1, c0 = v0, c1 = v1;
        #pragma unroll
        for (int i = 0; i < 100; i++) {
            v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001);
            a0 = fma(a0, 1.0001, 0.0001); a1 = fma(a1, 1.0001, 0.0001);
            b0 = fma(b0, 1.0001, 0.0001); b1 = fma(b1, 1.0001, 0.0001);
            c0 = fma(c0, 1.0001, 0.0001); c1 = fma(c1, 1.0001, 0.0001);
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
    }
}

/* AI=60: 120 iterations → 960 FLOPs/elem */
__global__ void roofline_fp64_vec_ai_60p0(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1], a0 = v0, a1 = v1, b0 = v0, b1 = v1, c0 = v0, c1 = v1;
        #pragma unroll
        for (int i = 0; i < 120; i++) {
            v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001);
            a0 = fma(a0, 1.0001, 0.0001); a1 = fma(a1, 1.0001, 0.0001);
            b0 = fma(b0, 1.0001, 0.0001); b1 = fma(b1, 1.0001, 0.0001);
            c0 = fma(c0, 1.0001, 0.0001); c1 = fma(c1, 1.0001, 0.0001);
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
    }
}

/* AI=70: 140 iterations → 1120 FLOPs/elem */
__global__ void roofline_fp64_vec_ai_70p0(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1], a0 = v0, a1 = v1, b0 = v0, b1 = v1, c0 = v0, c1 = v1;
        #pragma unroll
        for (int i = 0; i < 140; i++) {
            v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001);
            a0 = fma(a0, 1.0001, 0.0001); a1 = fma(a1, 1.0001, 0.0001);
            b0 = fma(b0, 1.0001, 0.0001); b1 = fma(b1, 1.0001, 0.0001);
            c0 = fma(c0, 1.0001, 0.0001); c1 = fma(c1, 1.0001, 0.0001);
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
    }
}

/* AI=80: 160 iterations → 1280 FLOPs/elem */
__global__ void roofline_fp64_vec_ai_80p0(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1], a0 = v0, a1 = v1, b0 = v0, b1 = v1, c0 = v0, c1 = v1;
        #pragma unroll
        for (int i = 0; i < 160; i++) {
            v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001);
            a0 = fma(a0, 1.0001, 0.0001); a1 = fma(a1, 1.0001, 0.0001);
            b0 = fma(b0, 1.0001, 0.0001); b1 = fma(b1, 1.0001, 0.0001);
            c0 = fma(c0, 1.0001, 0.0001); c1 = fma(c1, 1.0001, 0.0001);
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
    }
}

/* AI=90: 180 iterations → 1440 FLOPs/elem */
__global__ void roofline_fp64_vec_ai_90p0(double *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    if (idx + 1 < n) {
        double v0 = data[idx], v1 = data[idx + 1], a0 = v0, a1 = v1, b0 = v0, b1 = v1, c0 = v0, c1 = v1;
        #pragma unroll
        for (int i = 0; i < 180; i++) {
            v0 = fma(v0, 1.0001, 0.0001); v1 = fma(v1, 1.0001, 0.0001);
            a0 = fma(a0, 1.0001, 0.0001); a1 = fma(a1, 1.0001, 0.0001);
            b0 = fma(b0, 1.0001, 0.0001); b1 = fma(b1, 1.0001, 0.0001);
            c0 = fma(c0, 1.0001, 0.0001); c1 = fma(c1, 1.0001, 0.0001);
        }
        data[idx] = v0 + a0 + b0 + c0; data[idx + 1] = v1 + a1 + b1 + c1;
    }
}

/*
 * ============================================================================
 * HIP Kernels for FP32 Tensor/Matrix Operations (MFMA-like)
 * ============================================================================
 *
 * These kernels simulate tensor core operations by performing matrix-like
 * multiply-accumulate patterns. On AMD GPUs with Matrix Cores (CDNA),
 * these would use MFMA instructions.
 */

/* Tensor operation helper - simulates 4x4 matrix multiply-accumulate */
__device__ __forceinline__ void tensor_mma_fp32_4x4(
    float acc[4][4], const float a[4], const float b[4]
) {
    #pragma unroll
    for (int i = 0; i < 4; i++) {
        #pragma unroll
        for (int j = 0; j < 4; j++) {
            acc[i][j] = fmaf(a[i], b[j], acc[i][j]);
        }
    }
}

/* AI = 0.1: Very memory-bound tensor op */
__global__ void roofline_fp32_tensor_ai_0p1(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float a[4], b[4];
        float acc[4][4] = {{0}};
        
        /* Load 8 floats = 32 bytes */
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            a[i] = data[idx + i];
            b[i] = data[idx + i];
        }
        
        /* 1 tensor op = 32 FLOPs, for AI ~0.1 we need ~3.2 FLOPs per 32 bytes */
        acc[0][0] = fmaf(a[0], b[0], acc[0][0]);
        
        /* Store 4 floats */
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            data[idx + i] = acc[0][i];
        }
    }
}

/* AI = 0.25: Memory-bound tensor op */
__global__ void roofline_fp32_tensor_ai_0p25(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float a[4], b[4];
        float acc[4][4] = {{0}};
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            a[i] = data[idx + i];
            b[i] = data[idx + i];
        }
        
        /* 4 FMAs = 8 FLOPs for 32 bytes = 0.25 FLOP/Byte */
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            acc[0][i] = fmaf(a[0], b[i], acc[0][i]);
        }
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            data[idx + i] = acc[0][i];
        }
    }
}

/* AI = 0.5: Memory-bound tensor op */
__global__ void roofline_fp32_tensor_ai_0p5(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float a[4], b[4];
        float acc[4][4] = {{0}};
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            a[i] = data[idx + i];
            b[i] = data[idx + i];
        }
        
        /* 8 FMAs = 16 FLOPs for 32 bytes = 0.5 FLOP/Byte */
        #pragma unroll
        for (int i = 0; i < 2; i++) {
            #pragma unroll
            for (int j = 0; j < 4; j++) {
                acc[i][j] = fmaf(a[i], b[j], acc[i][j]);
            }
        }
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            data[idx + i] = acc[0][i] + acc[1][i];
        }
    }
}

/* AI = 1.0: Balanced tensor op */
__global__ void roofline_fp32_tensor_ai_1p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float a[4], b[4];
        float acc[4][4] = {{0}};
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            a[i] = data[idx + i];
            b[i] = data[idx + i];
        }
        
        /* 16 FMAs = 32 FLOPs for 32 bytes = 1.0 FLOP/Byte */
        tensor_mma_fp32_4x4(acc, a, b);
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            data[idx + i] = acc[0][i] + acc[1][i] + acc[2][i] + acc[3][i];
        }
    }
}

/* AI = 2.0: Compute-bound tensor op */
__global__ void roofline_fp32_tensor_ai_2p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float a[4], b[4];
        float acc[4][4] = {{0}};
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            a[i] = data[idx + i];
            b[i] = data[idx + i];
        }
        
        /* 2 x 16 FMAs = 64 FLOPs for 32 bytes = 2.0 FLOP/Byte */
        tensor_mma_fp32_4x4(acc, a, b);
        tensor_mma_fp32_4x4(acc, a, b);
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            data[idx + i] = acc[0][i] + acc[1][i] + acc[2][i] + acc[3][i];
        }
    }
}

/* AI = 3.0: Compute-bound tensor op */
__global__ void roofline_fp32_tensor_ai_3p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float a[4], b[4];
        float acc[4][4] = {{0}};
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            a[i] = data[idx + i];
            b[i] = data[idx + i];
        }
        
        /* 3 x 16 FMAs = 96 FLOPs for 32 bytes = 3.0 FLOP/Byte */
        tensor_mma_fp32_4x4(acc, a, b);
        tensor_mma_fp32_4x4(acc, a, b);
        tensor_mma_fp32_4x4(acc, a, b);
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            data[idx + i] = acc[0][i] + acc[1][i] + acc[2][i] + acc[3][i];
        }
    }
}

/* AI = 4.0: Compute-bound tensor op */
__global__ void roofline_fp32_tensor_ai_4p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float a[4], b[4];
        float acc[4][4] = {{0}};
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            a[i] = data[idx + i];
            b[i] = data[idx + i];
        }
        
        /* 4 x 16 FMAs = 128 FLOPs for 32 bytes = 4.0 FLOP/Byte */
        #pragma unroll
        for (int k = 0; k < 4; k++) {
            tensor_mma_fp32_4x4(acc, a, b);
        }
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            data[idx + i] = acc[0][i] + acc[1][i] + acc[2][i] + acc[3][i];
        }
    }
}

/* AI = 8.0: Highly compute-bound tensor op */
__global__ void roofline_fp32_tensor_ai_8p0(float *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float a[4], b[4];
        float acc[4][4] = {{0}};
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            a[i] = data[idx + i];
            b[i] = data[idx + i];
        }
        
        /* 8 x 16 FMAs = 256 FLOPs for 32 bytes = 8.0 FLOP/Byte */
        #pragma unroll
        for (int k = 0; k < 8; k++) {
            tensor_mma_fp32_4x4(acc, a, b);
        }
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            data[idx + i] = acc[0][i] + acc[1][i] + acc[2][i] + acc[3][i];
        }
    }
}

/*
 * ============================================================================
 * HIP Kernels for BF16 Tensor/Matrix Operations
 * ============================================================================
 */

/* Tensor operation helper for BF16 */
__device__ __forceinline__ void tensor_mma_bf16_4x4(
    float acc[4][4], const float a[4], const float b[4]
) {
    #pragma unroll
    for (int i = 0; i < 4; i++) {
        #pragma unroll
        for (int j = 0; j < 4; j++) {
            acc[i][j] = fmaf(a[i], b[j], acc[i][j]);
        }
    }
}

/* AI = 0.1: Very memory-bound BF16 tensor op */
__global__ void roofline_bf16_tensor_ai_0p1(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float a[4], b[4];
        float acc[4][4] = {{0}};
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            a[i] = float(data[idx + i]);
            b[i] = a[i];
        }
        
        acc[0][0] = fmaf(a[0], b[0], acc[0][0]);
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            data[idx + i] = hip_bfloat16(acc[0][i]);
        }
    }
}

/* AI = 0.25: Memory-bound BF16 tensor op */
__global__ void roofline_bf16_tensor_ai_0p25(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float a[4], b[4];
        float acc[4][4] = {{0}};
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            a[i] = float(data[idx + i]);
            b[i] = a[i];
        }
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            acc[0][i] = fmaf(a[0], b[i], acc[0][i]);
        }
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            data[idx + i] = hip_bfloat16(acc[0][i]);
        }
    }
}

/* AI = 0.5: Memory-bound BF16 tensor op */
__global__ void roofline_bf16_tensor_ai_0p5(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float a[4], b[4];
        float acc[4][4] = {{0}};
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            a[i] = float(data[idx + i]);
            b[i] = a[i];
        }
        
        #pragma unroll
        for (int i = 0; i < 2; i++) {
            #pragma unroll
            for (int j = 0; j < 4; j++) {
                acc[i][j] = fmaf(a[i], b[j], acc[i][j]);
            }
        }
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            data[idx + i] = hip_bfloat16(acc[0][i] + acc[1][i]);
        }
    }
}

/* AI = 1.0: Balanced BF16 tensor op */
__global__ void roofline_bf16_tensor_ai_1p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float a[4], b[4];
        float acc[4][4] = {{0}};
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            a[i] = float(data[idx + i]);
            b[i] = a[i];
        }
        
        tensor_mma_bf16_4x4(acc, a, b);
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            data[idx + i] = hip_bfloat16(acc[0][i] + acc[1][i] + acc[2][i] + acc[3][i]);
        }
    }
}

/* AI = 2.0: Compute-bound BF16 tensor op */
__global__ void roofline_bf16_tensor_ai_2p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float a[4], b[4];
        float acc[4][4] = {{0}};
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            a[i] = float(data[idx + i]);
            b[i] = a[i];
        }
        
        tensor_mma_bf16_4x4(acc, a, b);
        tensor_mma_bf16_4x4(acc, a, b);
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            data[idx + i] = hip_bfloat16(acc[0][i] + acc[1][i] + acc[2][i] + acc[3][i]);
        }
    }
}

/* AI = 3.0: Compute-bound BF16 tensor op */
__global__ void roofline_bf16_tensor_ai_3p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float a[4], b[4];
        float acc[4][4] = {{0}};
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            a[i] = float(data[idx + i]);
            b[i] = a[i];
        }
        
        tensor_mma_bf16_4x4(acc, a, b);
        tensor_mma_bf16_4x4(acc, a, b);
        tensor_mma_bf16_4x4(acc, a, b);
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            data[idx + i] = hip_bfloat16(acc[0][i] + acc[1][i] + acc[2][i] + acc[3][i]);
        }
    }
}

/* AI = 4.0: Compute-bound BF16 tensor op */
__global__ void roofline_bf16_tensor_ai_4p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float a[4], b[4];
        float acc[4][4] = {{0}};
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            a[i] = float(data[idx + i]);
            b[i] = a[i];
        }
        
        #pragma unroll
        for (int k = 0; k < 4; k++) {
            tensor_mma_bf16_4x4(acc, a, b);
        }
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            data[idx + i] = hip_bfloat16(acc[0][i] + acc[1][i] + acc[2][i] + acc[3][i]);
        }
    }
}

/* AI = 8.0: Highly compute-bound BF16 tensor op */
__global__ void roofline_bf16_tensor_ai_8p0(hip_bfloat16 *data, size_t n) {
    size_t idx = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    if (idx + 3 < n) {
        float a[4], b[4];
        float acc[4][4] = {{0}};
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            a[i] = float(data[idx + i]);
            b[i] = a[i];
        }
        
        #pragma unroll
        for (int k = 0; k < 8; k++) {
            tensor_mma_bf16_4x4(acc, a, b);
        }
        
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            data[idx + i] = hip_bfloat16(acc[0][i] + acc[1][i] + acc[2][i] + acc[3][i]);
        }
    }
}

/*
 * ============================================================================
 * Kernel Function Pointer Types and Tables
 * ============================================================================
 */

typedef void (*fp16_vec_kernel_t)(__half *, size_t);
typedef void (*bf16_vec_kernel_t)(hip_bfloat16 *, size_t);
typedef void (*fp32_vec_kernel_t)(float *, size_t);
typedef void (*fp64_vec_kernel_t)(double *, size_t);
typedef void (*fp32_tensor_kernel_t)(float *, size_t);
typedef void (*bf16_tensor_kernel_t)(hip_bfloat16 *, size_t);

/* type_code 0: FP16 */
static fp16_vec_kernel_t fp16_vec_kernels[N_AI_POINTS] = {
    roofline_fp16_vec_ai_0p1,  roofline_fp16_vec_ai_0p25, roofline_fp16_vec_ai_0p5,
    roofline_fp16_vec_ai_1p0,  roofline_fp16_vec_ai_2p0,  roofline_fp16_vec_ai_3p0,
    roofline_fp16_vec_ai_4p0,  roofline_fp16_vec_ai_5p0,  roofline_fp16_vec_ai_6p0,
    roofline_fp16_vec_ai_7p0,  roofline_fp16_vec_ai_8p0,  roofline_fp16_vec_ai_9p0,
    roofline_fp16_vec_ai_10p0, roofline_fp16_vec_ai_15p0, roofline_fp16_vec_ai_20p0,
    roofline_fp16_vec_ai_25p0, roofline_fp16_vec_ai_30p0, roofline_fp16_vec_ai_40p0,
    roofline_fp16_vec_ai_50p0, roofline_fp16_vec_ai_60p0, roofline_fp16_vec_ai_70p0,
    roofline_fp16_vec_ai_80p0, roofline_fp16_vec_ai_90p0
};

/* type_code 1: BF16 */
static bf16_vec_kernel_t bf16_vec_kernels[N_AI_POINTS] = {
    roofline_bf16_vec_ai_0p1,  roofline_bf16_vec_ai_0p25, roofline_bf16_vec_ai_0p5,
    roofline_bf16_vec_ai_1p0,  roofline_bf16_vec_ai_2p0,  roofline_bf16_vec_ai_3p0,
    roofline_bf16_vec_ai_4p0,  roofline_bf16_vec_ai_5p0,  roofline_bf16_vec_ai_6p0,
    roofline_bf16_vec_ai_7p0,  roofline_bf16_vec_ai_8p0,  roofline_bf16_vec_ai_9p0,
    roofline_bf16_vec_ai_10p0, roofline_bf16_vec_ai_15p0, roofline_bf16_vec_ai_20p0,
    roofline_bf16_vec_ai_25p0, roofline_bf16_vec_ai_30p0, roofline_bf16_vec_ai_40p0,
    roofline_bf16_vec_ai_50p0, roofline_bf16_vec_ai_60p0, roofline_bf16_vec_ai_70p0,
    roofline_bf16_vec_ai_80p0, roofline_bf16_vec_ai_90p0
};

/* type_code 2: FP32 */
static fp32_vec_kernel_t fp32_vec_kernels[N_AI_POINTS] = {
    roofline_fp32_vec_ai_0p1,  roofline_fp32_vec_ai_0p25, roofline_fp32_vec_ai_0p5,
    roofline_fp32_vec_ai_1p0,  roofline_fp32_vec_ai_2p0,  roofline_fp32_vec_ai_3p0,
    roofline_fp32_vec_ai_4p0,  roofline_fp32_vec_ai_5p0,  roofline_fp32_vec_ai_6p0,
    roofline_fp32_vec_ai_7p0,  roofline_fp32_vec_ai_8p0,  roofline_fp32_vec_ai_9p0,
    roofline_fp32_vec_ai_10p0, roofline_fp32_vec_ai_15p0, roofline_fp32_vec_ai_20p0,
    roofline_fp32_vec_ai_25p0, roofline_fp32_vec_ai_30p0, roofline_fp32_vec_ai_40p0,
    roofline_fp32_vec_ai_50p0, roofline_fp32_vec_ai_60p0, roofline_fp32_vec_ai_70p0,
    roofline_fp32_vec_ai_80p0, roofline_fp32_vec_ai_90p0
};

/* type_code 3: FP64 */
static fp64_vec_kernel_t fp64_vec_kernels[N_AI_POINTS] = {
    roofline_fp64_vec_ai_0p1,  roofline_fp64_vec_ai_0p25, roofline_fp64_vec_ai_0p5,
    roofline_fp64_vec_ai_1p0,  roofline_fp64_vec_ai_2p0,  roofline_fp64_vec_ai_3p0,
    roofline_fp64_vec_ai_4p0,  roofline_fp64_vec_ai_5p0,  roofline_fp64_vec_ai_6p0,
    roofline_fp64_vec_ai_7p0,  roofline_fp64_vec_ai_8p0,  roofline_fp64_vec_ai_9p0,
    roofline_fp64_vec_ai_10p0, roofline_fp64_vec_ai_15p0, roofline_fp64_vec_ai_20p0,
    roofline_fp64_vec_ai_25p0, roofline_fp64_vec_ai_30p0, roofline_fp64_vec_ai_40p0,
    roofline_fp64_vec_ai_50p0, roofline_fp64_vec_ai_60p0, roofline_fp64_vec_ai_70p0,
    roofline_fp64_vec_ai_80p0, roofline_fp64_vec_ai_90p0
};

/* Tensor kernels - only available for BF16/FP32 (use AI=8 for high AI points) */
static fp32_tensor_kernel_t fp32_tensor_kernels[N_AI_POINTS] = {
    roofline_fp32_tensor_ai_0p1,  roofline_fp32_tensor_ai_0p25, roofline_fp32_tensor_ai_0p5,
    roofline_fp32_tensor_ai_1p0,  roofline_fp32_tensor_ai_2p0,  roofline_fp32_tensor_ai_3p0,
    roofline_fp32_tensor_ai_4p0,  roofline_fp32_tensor_ai_8p0,  roofline_fp32_tensor_ai_8p0,
    roofline_fp32_tensor_ai_8p0,  roofline_fp32_tensor_ai_8p0,  roofline_fp32_tensor_ai_8p0,
    roofline_fp32_tensor_ai_8p0,  roofline_fp32_tensor_ai_8p0,  roofline_fp32_tensor_ai_8p0,
    roofline_fp32_tensor_ai_8p0,  roofline_fp32_tensor_ai_8p0,  roofline_fp32_tensor_ai_8p0,
    roofline_fp32_tensor_ai_8p0,  roofline_fp32_tensor_ai_8p0,  roofline_fp32_tensor_ai_8p0,
    roofline_fp32_tensor_ai_8p0,  roofline_fp32_tensor_ai_8p0
};

static bf16_tensor_kernel_t bf16_tensor_kernels[N_AI_POINTS] = {
    roofline_bf16_tensor_ai_0p1,  roofline_bf16_tensor_ai_0p25, roofline_bf16_tensor_ai_0p5,
    roofline_bf16_tensor_ai_1p0,  roofline_bf16_tensor_ai_2p0,  roofline_bf16_tensor_ai_3p0,
    roofline_bf16_tensor_ai_4p0,  roofline_bf16_tensor_ai_8p0,  roofline_bf16_tensor_ai_8p0,
    roofline_bf16_tensor_ai_8p0,  roofline_bf16_tensor_ai_8p0,  roofline_bf16_tensor_ai_8p0,
    roofline_bf16_tensor_ai_8p0,  roofline_bf16_tensor_ai_8p0,  roofline_bf16_tensor_ai_8p0,
    roofline_bf16_tensor_ai_8p0,  roofline_bf16_tensor_ai_8p0,  roofline_bf16_tensor_ai_8p0,
    roofline_bf16_tensor_ai_8p0,  roofline_bf16_tensor_ai_8p0,  roofline_bf16_tensor_ai_8p0,
    roofline_bf16_tensor_ai_8p0,  roofline_bf16_tensor_ai_8p0
};

/* FLOPs per element for FP16 vector (4 bytes R/W)
 * FLOPs = AI × bytes_per_elem = AI × 4 (counting FMA as 2 ops)
 * 4 elements, 2 chains, 8 FMAs/iter × 2 = 4 FLOPs/elem/iter */
static const int flops_per_elem_fp16_vec[N_AI_POINTS] = {
    1, 1, 2, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 60, 80, 100, 120, 160, 200, 240, 280, 320, 360
};

/* FLOPs per element for BF16 vector (4 bytes R/W) - same as FP16 */
static const int flops_per_elem_bf16_vec[N_AI_POINTS] = {
    1, 1, 2, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 60, 80, 100, 120, 160, 200, 240, 280, 320, 360
};

/* FLOPs per element for FP32 vector (8 bytes R/W)
 * FLOPs = AI × bytes_per_elem = AI × 8 (counting FMA as 2 ops)
 * 4 elements, 4 chains, 16 FMAs/iter × 2 = 8 FLOPs/elem/iter */
static const int flops_per_elem_fp32_vec[N_AI_POINTS] = {
    1, 2, 4, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 120, 160, 200, 240, 320, 400, 480, 560, 640, 720
};

/* FLOPs per element for FP64 vector (16 bytes R/W)
 * FLOPs = AI × bytes_per_elem = AI × 16 (counting FMA as 2 ops)
 * 2 elements, 4 chains, 8 FMAs/iter × 2 = 8 FLOPs/elem/iter */
static const int flops_per_elem_fp64_vec[N_AI_POINTS] = {
    2, 4, 8, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 240, 320, 400, 480, 640, 800, 960, 1120, 1280, 1440
};

/* FLOPs per 4-element group for tensor operations (32 bytes R/W for FP32) */
static const int flops_per_group_fp32_tensor[N_AI_POINTS] = {
    2, 8, 16, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 480, 640, 800, 960, 1280, 1600, 1920, 2240, 2560, 2880
};

/* FLOPs per 4-element group for BF16 tensor (16 bytes R/W) */
static const int flops_per_group_bf16_tensor[N_AI_POINTS] = {
    2, 8, 16, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 480, 640, 800, 960, 1280, 1600, 1920, 2240, 2560, 2880
};

/*
 * ============================================================================
 * PLI Registration Functions
 * ============================================================================
 */

extern "C" int
register_roofline_rocm(void)
{
    return _tpbk_register_roofline_rocm();
}

extern "C" int
tpbk_pli_register_roofline_rocm(void)
{
    int err;

    err = tpb_k_register("roofline_rocm",
                         "ROCm GPU Roofline Model Benchmark",
                         TPB_KTYPE_PLI);
    if (err != 0) return err;

    err = tpb_k_add_parm("type_code",
                         "Data type: 0=FP16, 1=BF16, 2=FP32, 3=FP64",
                         "0",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)3);
    if (err != 0) return err;

    err = tpb_k_add_parm("op_code",
                         "Operation type: 0=vector, 1=tensor (tensor only for BF16/FP32)",
                         "0",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)1);
    if (err != 0) return err;

    err = tpb_k_add_parm("total_memsize",
                         "Memory size in KiB",
                         "131072",
                         TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_RANGE,
                         0.0009765625, DBL_MAX);
    if (err != 0) return err;

    err = tpb_k_add_parm("ntest",
                         "Number of test iterations",
                         "100",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)1, (int64_t)1000000);
    if (err != 0) return err;

    err = tpb_k_add_parm("twarm",
                         "Warm-up time in milliseconds",
                         "100",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)600000);
    if (err != 0) return err;

    err = tpb_k_add_parm("nloop_timing",
                         "Repeat factor for timing accuracy",
                         "1",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)1, (int64_t)1000);
    if (err != 0) return err;

    return tpb_k_finalize_pli();
}

extern "C" int
_tpbk_register_roofline_rocm(void)
{
    int err;

    err = tpb_k_register("roofline_rocm",
                         "ROCm GPU Roofline Model Benchmark",
                         TPB_KTYPE_FLI);
    if (err != 0) return err;

    err = tpb_k_add_parm("type_code",
                         "Data type: 0=FP16, 1=BF16, 2=FP32, 3=FP64",
                         "0",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)3);
    if (err != 0) return err;

    err = tpb_k_add_parm("op_code",
                         "Operation type: 0=vector, 1=tensor (tensor only for BF16/FP32)",
                         "0",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)1);
    if (err != 0) return err;

    err = tpb_k_add_parm("total_memsize",
                         "Memory size in KiB",
                         "131072",
                         TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_RANGE,
                         0.0009765625, DBL_MAX);
    if (err != 0) return err;

    err = tpb_k_add_parm("ntest",
                         "Number of test iterations",
                         "100",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)1, (int64_t)1000000);
    if (err != 0) return err;

    err = tpb_k_add_parm("twarm",
                         "Warm-up time in milliseconds",
                         "100",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)600000);
    if (err != 0) return err;

    err = tpb_k_add_parm("nloop_timing",
                         "Repeat factor for timing accuracy",
                         "1",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)1, (int64_t)1000);
    if (err != 0) return err;

    /* Output: FLOP/s at each arithmetic intensity */
    err = tpb_k_add_output("flops_ai_0p1",
                           "FLOP/s at AI=0.1",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_output("flops_ai_0p25",
                           "FLOP/s at AI=0.25",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_output("flops_ai_0p5",
                           "FLOP/s at AI=0.5",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_output("flops_ai_1p0",
                           "FLOP/s at AI=1.0",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_output("flops_ai_2p0",
                           "FLOP/s at AI=2.0",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_output("flops_ai_3p0",
                           "FLOP/s at AI=3.0",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_output("flops_ai_4p0",
                           "FLOP/s at AI=4.0",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_output("flops_ai_5p0",
                           "FLOP/s at AI=5.0",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_output("flops_ai_6p0",
                           "FLOP/s at AI=6.0",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_output("flops_ai_7p0",
                           "FLOP/s at AI=7.0",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_output("flops_ai_8p0",
                           "FLOP/s at AI=8.0",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_output("flops_ai_9p0",
                           "FLOP/s at AI=9.0",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_output("flops_ai_10p0",
                           "FLOP/s at AI=10.0",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_output("flops_ai_15p0",
                           "FLOP/s at AI=15.0",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_output("flops_ai_20p0",
                           "FLOP/s at AI=20.0",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_output("flops_ai_25p0",
                           "FLOP/s at AI=25.0",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_output("flops_ai_30p0",
                           "FLOP/s at AI=30.0",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_output("flops_ai_40p0",
                           "FLOP/s at AI=40.0",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_output("flops_ai_50p0",
                           "FLOP/s at AI=50.0",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_output("flops_ai_60p0",
                           "FLOP/s at AI=60.0",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_output("flops_ai_70p0",
                           "FLOP/s at AI=70.0",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_output("flops_ai_80p0",
                           "FLOP/s at AI=80.0",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_output("flops_ai_90p0",
                           "FLOP/s at AI=90.0",
                           TPB_DOUBLE_T,
                           TPB_UNIT_FLOPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_runner(_tpbk_run_roofline_rocm);
    if (err != 0) return err;

    return 0;
}

/*
 * ============================================================================
 * Runner Function
 * ============================================================================
 */

extern "C" int
_tpbk_run_roofline_rocm(void)
{
    int tpberr;
    tpb_timer_t timer;
    int64_t type_code, op_code, ntest, twarm_ms, nloop_timing;
    double total_memsize;
    double *flops_out[N_AI_POINTS];

    tpberr = tpb_k_get_timer(&timer);
    if (tpberr) return tpberr;

    tpberr = tpb_k_get_arg("type_code", TPB_INT64_T, (void *)&type_code);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("op_code", TPB_INT64_T, (void *)&op_code);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("total_memsize", TPB_DOUBLE_T, (void *)&total_memsize);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("ntest", TPB_INT64_T, (void *)&ntest);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("twarm", TPB_INT64_T, (void *)&twarm_ms);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("nloop_timing", TPB_INT64_T, (void *)&nloop_timing);
    if (tpberr) return tpberr;

    /* Allocate outputs for each AI point */
    const char *output_names[N_AI_POINTS] = {
        "flops_ai_0p1", "flops_ai_0p25", "flops_ai_0p5", "flops_ai_1p0",
        "flops_ai_2p0", "flops_ai_3p0", "flops_ai_4p0", "flops_ai_5p0",
        "flops_ai_6p0", "flops_ai_7p0", "flops_ai_8p0", "flops_ai_9p0",
        "flops_ai_10p0", "flops_ai_15p0", "flops_ai_20p0", "flops_ai_25p0",
        "flops_ai_30p0", "flops_ai_40p0", "flops_ai_50p0", "flops_ai_60p0",
        "flops_ai_70p0", "flops_ai_80p0", "flops_ai_90p0"
    };

    for (int i = 0; i < N_AI_POINTS; i++) {
        tpberr = tpb_k_alloc_output(output_names[i], 1, &flops_out[i]);
        if (tpberr) return tpberr;
    }

    /* Flatten output pointers into a single array for the core function */
    double flops_results[N_AI_POINTS];

    tpberr = d_roofline_rocm(&timer, type_code, op_code, total_memsize,
                             ntest, twarm_ms, nloop_timing, flops_results);

    /* Copy results to allocated outputs */
    for (int i = 0; i < N_AI_POINTS; i++) {
        *flops_out[i] = flops_results[i];
    }

    return tpberr;
}

/*
 * ============================================================================
 * Core Measurement Function
 * ============================================================================
 */

static int
d_roofline_rocm(tpb_timer_t *timer, int64_t type_code, int64_t op_code,
                double total_memsize_kib, int64_t ntest, int64_t twarm_ms,
                int64_t nloop_timing, double *flops_out)
{
    hipError_t hip_err;
    int device_id = 0;
    
    /* Check for device selection via environment variable */
    const char *hip_visible = getenv("HIP_VISIBLE_DEVICES");
    if (hip_visible == NULL) {
        hip_visible = getenv("ROCR_VISIBLE_DEVICES");
    }
    /* If env var is set, HIP runtime will handle device remapping,
       so we always use device 0 in that case */
    
    /* Initialize HIP and select device */
    HIP_CHECK(hipSetDevice(device_id));

    /* Get device properties for info */
    hipDeviceProp_t props;
    HIP_CHECK(hipGetDeviceProperties(&props, device_id));
    tpb_printf(TPBM_PRTN_M_DIRECT, "Using GPU: %s\n", props.name);
    tpb_printf(TPBM_PRTN_M_DIRECT, "  Compute capability: %d.%d\n",
               props.major, props.minor);
    tpb_printf(TPBM_PRTN_M_DIRECT, "  Total memory: %.2f GB\n",
               props.totalGlobalMem / (1024.0 * 1024.0 * 1024.0));

    /* Calculate array size based on memory size and data type */
    /* type_code: 0=FP16, 1=BF16, 2=FP32, 3=FP64 */
    size_t elem_size;
    const char *type_name;
    if (type_code == 0) {
        elem_size = sizeof(__half);
        type_name = "FP16";
    } else if (type_code == 1) {
        elem_size = sizeof(hip_bfloat16);
        type_name = "BF16";
    } else if (type_code == 2) {
        elem_size = sizeof(float);
        type_name = "FP32";
    } else {
        elem_size = sizeof(double);
        type_name = "FP64";
    }
    size_t total_bytes = (size_t)(total_memsize_kib * 1024.0);
    size_t n_elements = total_bytes / elem_size;

    /* Ensure alignment for tensor operations (multiple of 8 for half types) */
    n_elements = (n_elements / 8) * 8;
    if (n_elements < 256) n_elements = 256;

    tpb_printf(TPBM_PRTN_M_DIRECT, "Configuration:\n");
    tpb_printf(TPBM_PRTN_M_DIRECT, "  Data type: %s\n", type_name);
    tpb_printf(TPBM_PRTN_M_DIRECT, "  Operation: %s\n",
               op_code == 0 ? "vector" : "tensor");
    tpb_printf(TPBM_PRTN_M_DIRECT, "  Elements: %zu\n", n_elements);
    tpb_printf(TPBM_PRTN_M_DIRECT, "  Memory: %.2f KiB\n",
               (double)(n_elements * elem_size) / 1024.0);
    tpb_printf(TPBM_PRTN_M_DIRECT, "  Iterations: %" PRId64 "\n", ntest);

    /* Allocate device memory */
    void *d_data;
    HIP_CHECK(hipMalloc(&d_data, n_elements * elem_size));

    /* Initialize device memory */
    HIP_CHECK(hipMemset(d_data, 0, n_elements * elem_size));

    /* Create events for timing */
    hipEvent_t start, stop;
    HIP_CHECK(hipEventCreate(&start));
    HIP_CHECK(hipEventCreate(&stop));

    /* Calculate grid and block dimensions */
    int block_size = 1024;
    int grid_size_tensor = ((n_elements / 4) + block_size - 1) / block_size;
    
    /* Grid sizes for kernels that process multiple elements per thread */
    /* FP16/BF16: 8 elem/thread for low AI, 4 elem/thread for mid AI */
    int grid_size_half_8elem = ((n_elements / 8) + block_size - 1) / block_size;
    int grid_size_half_4elem = ((n_elements / 4) + block_size - 1) / block_size;
    /* FP32: 4 elem/thread for low AI, 2 elem/thread for mid AI */
    int grid_size_fp32_4elem = ((n_elements / 4) + block_size - 1) / block_size;
    int grid_size_fp32_2elem = ((n_elements / 2) + block_size - 1) / block_size;
    /* FP64: 2 elem/thread */
    int grid_size_fp64_2elem = ((n_elements / 2) + block_size - 1) / block_size;

    /* Warm-up phase */
    tpb_printf(TPBM_PRTN_M_DIRECT, "Warming up for %" PRId64 " ms...\n", twarm_ms);
    
    struct timespec wts;
    uint64_t wns0, wns1;
    clock_gettime(CLOCK_MONOTONIC, &wts);
    wns0 = wts.tv_sec * 1000000000ULL + wts.tv_nsec;
    wns1 = wns0 + (uint64_t)twarm_ms * 1000000ULL;

    while (wns0 < wns1) {
        /* type_code: 0=FP16, 1=BF16, 2=FP32, 3=FP64 */
        if (type_code == 0) {
            /* FP16 */
            hipLaunchKernelGGL(fp16_vec_kernels[3], dim3(grid_size_half_8elem),
                               dim3(block_size), 0, 0,
                               (__half *)d_data, n_elements);
        } else if (type_code == 1) {
            /* BF16 */
            if (op_code == 0) {
                hipLaunchKernelGGL(bf16_vec_kernels[3], dim3(grid_size_half_8elem),
                                   dim3(block_size), 0, 0,
                                   (hip_bfloat16 *)d_data, n_elements);
            } else {
                hipLaunchKernelGGL(bf16_tensor_kernels[3], dim3(grid_size_tensor),
                                   dim3(block_size), 0, 0,
                                   (hip_bfloat16 *)d_data, n_elements);
            }
        } else if (type_code == 2) {
            /* FP32 */
            if (op_code == 0) {
                hipLaunchKernelGGL(fp32_vec_kernels[3], dim3(grid_size_fp32_4elem),
                                   dim3(block_size), 0, 0,
                                   (float *)d_data, n_elements);
            } else {
                hipLaunchKernelGGL(fp32_tensor_kernels[3], dim3(grid_size_tensor),
                                   dim3(block_size), 0, 0,
                                   (float *)d_data, n_elements);
            }
        } else {
            /* FP64 */
            hipLaunchKernelGGL(fp64_vec_kernels[3], dim3(grid_size_fp64_2elem),
                               dim3(block_size), 0, 0,
                               (double *)d_data, n_elements);
        }
        HIP_CHECK(hipDeviceSynchronize());
        clock_gettime(CLOCK_MONOTONIC, &wts);
        wns0 = wts.tv_sec * 1000000000ULL + wts.tv_nsec;
    }

    /* Measure each arithmetic intensity point */
    tpb_printf(TPBM_PRTN_M_DIRECT, "\nMeasuring roofline points...\n");
    tpb_printf(TPBM_PRTN_M_DIRECT, "%-8s %15s %15s\n", "AI", "Time (ms)", "GFLOP/s");
    tpb_printf(TPBM_PRTN_M_DIRECT, "-------- --------------- ---------------\n");

    for (int ai_idx = 0; ai_idx < N_AI_POINTS; ai_idx++) {
        float elapsed_ms = 0.0f;
        
        HIP_CHECK(hipEventRecord(start, 0));
        
        /* nloop_timing repeats the entire ntest loop for better timing accuracy */
        for (int64_t timing_loop = 0; timing_loop < nloop_timing; timing_loop++) {
            for (int64_t iter = 0; iter < ntest; iter++) {
                /* type_code: 0=FP16, 1=BF16, 2=FP32, 3=FP64 */
                int cur_grid_size;
                if (type_code == 0) {
                    /* FP16 */
                    /* AI 0-3 use 8 elem/thread, AI 4-12 use 4 elem/thread, AI 13+ use 8 elem/thread */
                    if (ai_idx <= 3 || ai_idx >= 13) {
                        cur_grid_size = grid_size_half_8elem;
                    } else {
                        cur_grid_size = grid_size_half_4elem;
                    }
                    hipLaunchKernelGGL(fp16_vec_kernels[ai_idx], dim3(cur_grid_size),
                                       dim3(block_size), 0, 0,
                                       (__half *)d_data, n_elements);
                } else if (type_code == 1) {
                    /* BF16 */
                    if (op_code == 0) {
                        if (ai_idx <= 3 || ai_idx >= 13) {
                            cur_grid_size = grid_size_half_8elem;
                        } else {
                            cur_grid_size = grid_size_half_4elem;
                        }
                        hipLaunchKernelGGL(bf16_vec_kernels[ai_idx], dim3(cur_grid_size),
                                           dim3(block_size), 0, 0,
                                           (hip_bfloat16 *)d_data, n_elements);
                    } else {
                        hipLaunchKernelGGL(bf16_tensor_kernels[ai_idx], dim3(grid_size_tensor),
                                           dim3(block_size), 0, 0,
                                           (hip_bfloat16 *)d_data, n_elements);
                    }
                } else if (type_code == 2) {
                    /* FP32 */
                    if (op_code == 0) {
                        /* AI 0-3 and 10+ use 4 elem/thread, AI 4-9 use 2 elem/thread */
                        if (ai_idx <= 3 || ai_idx >= 10) {
                            cur_grid_size = grid_size_fp32_4elem;
                        } else {
                            cur_grid_size = grid_size_fp32_2elem;
                        }
                        hipLaunchKernelGGL(fp32_vec_kernels[ai_idx], dim3(cur_grid_size),
                                           dim3(block_size), 0, 0,
                                           (float *)d_data, n_elements);
                    } else {
                        hipLaunchKernelGGL(fp32_tensor_kernels[ai_idx], dim3(grid_size_tensor),
                                           dim3(block_size), 0, 0,
                                           (float *)d_data, n_elements);
                    }
                } else {
                    /* FP64 */
                    cur_grid_size = grid_size_fp64_2elem;
                    hipLaunchKernelGGL(fp64_vec_kernels[ai_idx], dim3(cur_grid_size),
                                       dim3(block_size), 0, 0,
                                       (double *)d_data, n_elements);
                }
            }
        }
        
        HIP_CHECK(hipEventRecord(stop, 0));
        HIP_CHECK(hipEventSynchronize(stop));
        HIP_CHECK(hipEventElapsedTime(&elapsed_ms, start, stop));

        /* Calculate total FLOPs */
        /* type_code: 0=FP16, 1=BF16, 2=FP32, 3=FP64 */
        uint64_t total_flops;
        if (op_code == 0) {
            /* Vector operations */
            int flops_per_elem;
            if (type_code == 0) {
                flops_per_elem = flops_per_elem_fp16_vec[ai_idx];
            } else if (type_code == 1) {
                flops_per_elem = flops_per_elem_bf16_vec[ai_idx];
            } else if (type_code == 2) {
                flops_per_elem = flops_per_elem_fp32_vec[ai_idx];
            } else {
                flops_per_elem = flops_per_elem_fp64_vec[ai_idx];
            }
            total_flops = (uint64_t)n_elements * flops_per_elem * ntest * nloop_timing;
        } else {
            /* Tensor operations (4 elements per group) - only for BF16/FP32 */
            int flops_per_group = (type_code == 1) ?
                flops_per_group_bf16_tensor[ai_idx] :
                flops_per_group_fp32_tensor[ai_idx];
            total_flops = (uint64_t)(n_elements / 4) * flops_per_group * ntest * nloop_timing;
        }

        /* Calculate FLOP/s */
        double elapsed_sec = elapsed_ms / 1000.0;
        double flops_per_sec = (double)total_flops / elapsed_sec;
        flops_out[ai_idx] = flops_per_sec;

        tpb_printf(TPBM_PRTN_M_DIRECT, "%-8.2f %15.3f %15.3f\n",
                   ai_values[ai_idx], elapsed_ms, flops_per_sec / 1e9);
    }

    /* Cleanup */
    HIP_CHECK(hipEventDestroy(start));
    HIP_CHECK(hipEventDestroy(stop));
    HIP_CHECK(hipFree(d_data));

    tpb_printf(TPBM_PRTN_M_DIRECT, "\nRoofline measurement completed.\n");

    return 0;
}
