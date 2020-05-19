/*
 * =================================================================================
 * TPBench - A high-precision throughputs benchmarking tool for scientific computing
 * 
 * Copyright (C) 2020 Key Liao (Liao Qiucheng)
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
 * g1_kernel.c
 * Description: Group 1 kernels, one-line kernel performing very simple operations.
 * Author: Key Liao
 * Modified: May. 9th, 2020
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "tpbench.h"

#define TYPE double

#ifdef __aarch64__

#define CYCLE_ST    uint64_t cy_t;                      \
                    asm volatile(                       \
                        "mov x28, %0"           "\n\t"  \
                        "mrs x29, pmccntr_el0"          \
                        :                               \
                        : "r" (cy)                      \
                        : "x28", "x29", "x30", "memory");

#define CYCLE_EN    asm volatile(                           \
                        "mrs x30, pmccntr_el0"  "\n\t"      \
                        "sub x30, x30, x29"     "\n\t"      \
                        "str x30, [x28]"                    \
                        :                                   \ 
                        :                                   \
                        : "x28", "x29", "x30","memory" );   
                    // *cy = cy_t;
                    
#else

#define CYCLE_ST uint64_t hi1, lo1, hi2, lo2;            \
                asm volatile(                           \
                    "RDTSCP"        "\n\t"              \
                    "RDTSC"         "\n\t"              \
                    "CPUID"         "\n\t"              \
                    "RDTSCP"        "\n\t"              \
                    "mov %%rdx, %0" "\n\t"              \
                    "mov %%rax, %1" "\n\t"              \
                    :"=r" (hi1), "=r" (lo1)             \
                    :                                   \
                    :"%rax", "%rbx", "%rcx", "%rdx");

#define CYCLE_EN asm volatile (                         \
                    "RDTSC"         "\n\t"              \
                    "mov %%rdx, %0" "\n\t"              \
                    "mov %%rax, %1" "\n\t"              \
                    "CPUID"         "\n\t"              \
                    :"=r" (hi2), "=r" (lo2)             \
                    :                                   \
                    :"%rax", "%rbx", "%rcx", "%rdx");   \
                *cy = ((hi2 << 32) | lo2) - ((hi1 << 32) | lo1);
#endif

/*
 * init
 * 1 Store + WA, 0 flops
 */
void
run_init(TYPE *a, TYPE s, int narr, uint64_t *ns, uint64_t *cy){
    struct timespec ts1, ts2;
    
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    CYCLE_ST;
    for(int i = 0; i < narr; i ++){
        a[i] = s;
    }
    CYCLE_EN;
    
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
}

/*
 * sum
 * 1 Load, 1 flops
 */
void
run_sum(TYPE *a, TYPE *s, int narr, uint64_t *ns, uint64_t *cy){
    struct timespec ts1, ts2;
    
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    CYCLE_ST;
    for(int i = 0; i < narr; i ++){
        *s += a[i];
    }
    CYCLE_EN;
    clock_gettime(CLOCK_MONOTONIC, &ts2);

    *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
}

/*
 * copy
 * 1 Load + 1 Store + WA, 0 flops
 */
void
run_copy(TYPE *a, TYPE *b, int narr, uint64_t *ns, uint64_t *cy){
    struct timespec ts1, ts2;
    
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    CYCLE_ST;
    for(int i = 0; i < narr; i ++){
        a[i] = b[i];
    }
    CYCLE_EN;
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
}

/*
 * update
 * 1Load + 1 Store, 1 flops
 */
void
run_update(TYPE *a, TYPE s, int narr, uint64_t *ns, uint64_t *cy){
    struct timespec ts1, ts2;
    
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    CYCLE_ST;
    for(int i = 0; i < narr; i ++){
        a[i] = s * a[i];
    }
    CYCLE_EN;
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
}

/*
 * triad
 * 2 Load + 1 Store + WA, 2 flops
 */
void
run_triad(TYPE *a, TYPE *b, TYPE *c, TYPE s, int narr, uint64_t *ns, uint64_t *cy){
    struct timespec ts1, ts2;
    
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    CYCLE_ST;
    for(int i = 0; i < narr; i ++){
        //a[i] = b[i] + s * c[i];
        a[i] = b[i] + s * c[i];
    }
    CYCLE_EN;
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
}

/*
 * daxpy
 * 2 Load + 1 Store, 2 flops
 */
void
run_daxpy(TYPE *a, TYPE *b, TYPE s, int narr, uint64_t *ns, uint64_t *cy){
    struct timespec ts1, ts2;
    
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    CYCLE_ST;
    for(int i = 0; i < narr; i ++){
        a[i] = a[i] + b[i] * s;
    }
    CYCLE_EN;
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
}

/*
 * striad
 * 3 Load + 1 Store + WA, 2 flops
 */
void
run_striad(TYPE *a, TYPE *b, TYPE *c, TYPE *d, int narr, uint64_t *ns, uint64_t *cy){
    struct timespec ts1, ts2;
    
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    CYCLE_ST;
    for(int i = 0; i < narr; i ++){
       a[i] = b[i] + c[i] * d[i];
    }
    CYCLE_EN;
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
}

/*
 * sdaxpy
 * 3Load + 1 Store                                                                                                                                                        flops
 */
void
run_sdaxpy(TYPE *a, TYPE *b, TYPE *c, int narr, uint64_t *ns, uint64_t *cy){
    struct timespec ts1, ts2;
    
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    CYCLE_ST;
    for(int i = 0; i < narr; i ++){
        a[i] = a[i] + b[i] * c[i];
    }
    CYCLE_EN;
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
}

/*
 * run_g1_group
 * group runner for g1 group
 * 
 * 
 */
int
run_g1_kernel(int ntest, uint64_t kib, uint64_t **res_ns, uint64_t **res_cy) {
    int nbyte, nsize, i;
    uint64_t t0, t1, ns[8], cy[8];
    struct timespec ts;
    double s = 0.5;
    TYPE *a, *b, *c, *d;

    nbyte = kib * 1024;
    nsize = nbyte / 8; // double

    a = (TYPE *)malloc(sizeof(TYPE) * nsize);
    if(a == NULL) {
        return MEM_FAIL;
    }
    b = (TYPE *)malloc(sizeof(TYPE) * nsize);
    if(b == NULL) {
        return MEM_FAIL;
    }
    c = (TYPE *)malloc(sizeof(TYPE) * nsize);
    if(c == NULL) {
        return MEM_FAIL;
    }
    d = (TYPE *)malloc(sizeof(TYPE) * nsize);
    if(d == NULL) {
        return MEM_FAIL;
    }
    clock_gettime(CLOCK_MONOTONIC, &ts);
    t0 = ts.tv_sec * 1e9 + ts.tv_nsec;
    t1 = t0 + 1e9;
    for(i = 0; i < nsize; i ++) {
        a[i] = 1.0;
        b[i] = 2.0;
        c[i] = 3.0;
        d[i] = 4.0;
    }
    while(t0 < t1) {
        run_init(a, s,  nsize,  &res_ns[0][0],  &res_cy[0][0]);
        run_sum(a,  &s, nsize,  &res_ns[0][1],  &res_cy[0][1]);
        run_copy(a, b,  nsize,  &res_ns[0][2],  &res_cy[0][2]);
        run_update(b,   s,  nsize,  &res_ns[0][3],  &res_cy[0][3]);
        run_triad(a, b, c, s, nsize, &res_ns[0][4], &res_cy[0][4]);
        run_daxpy(a, b, s, nsize, &res_ns[0][5], &res_cy[0][5]);
        run_striad(a, b, c, d, nsize, &res_ns[0][6], &res_cy[0][6]);
        run_sdaxpy(d, b, c, nsize, &res_ns[0][7], &res_cy[0][7]);
        clock_gettime(CLOCK_MONOTONIC, &ts);
        t0 = ts.tv_sec * 1e9 + ts.tv_nsec; 
    }

    for(i = 0; i < ntest; i ++) {
        run_init(a, s, nsize, &res_ns[i][0], &res_cy[i][0]);
        run_sum(a, &s, nsize, &res_ns[i][1], &res_cy[i][1]);
        run_copy(a, b, nsize, &res_ns[i][2], &res_cy[i][2]);
        run_update(b, s, nsize, &res_ns[i][3], &res_cy[i][3]);
        run_triad(a, b, c, s, nsize, &res_ns[i][4], &res_cy[i][4]);
        run_daxpy(a, b, s, nsize, &res_ns[i][5], &res_cy[i][5]);
        run_striad(a, b, c, d, nsize, &res_ns[i][6], &res_cy[i][6]);
        run_sdaxpy(d, b, c, nsize, &res_ns[i][7], &res_cy[i][7]);
    }

    free(a);
    free(b);
    free(c);
    free(d);
    return 0;
}