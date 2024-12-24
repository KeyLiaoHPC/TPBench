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
 * jacobi5p.c
 * Description: 5 point jacobi stencil
 * =================================================================================
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include "tptimer.h"
#include "tperror.h"
#include "tpdata.h"
#include "tpmpi.h"
#include "tpio.h"

// kernel data
static volatile double **in, **out;
static double a = 0.21, b = 0.2;

static const int height = 100;

// blocking data
static int block_size;

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

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

static void init_kernel_data(int narr) {
    in = (double **)malloc(height * sizeof(double*));
    for (size_t i = 0; i < height; i ++) {
        in[i] = (double *)malloc(narr * sizeof(double));
    }

    out = (double **)malloc(height * sizeof(double*));
    for (size_t i = 0; i < height; i ++) {
        out[i] = (double *)malloc(narr * sizeof(double));
    }

    for (int i = 0; i < height; i++) {
        fill_random(in[i], narr);
        fill_random(out[i], narr);
    }

    // init from environment variable
    block_size = 0; 
    {
        const char *s = getenv("TPBENCH_BLOCK");
        if (NULL != s) {
            block_size = atoi(s);
        }
    }
}

static void free_2d_array(double **a, int narr) {
    for (int i = 0; i < height; i++) {
        free(a[i]);
    }
    free(a);
}

static void free_kernel_data(int narr) {
    free_2d_array(in, narr);
    free_2d_array(out, narr);
}

static void run_kernel_once(int narr) {
    if (block_size == 0) { 
        // no blocking
        for (int j = 1; j < height-1; j ++) {
            for (int k = 1; k < narr-1; k ++) {
                out[j][k] = a * in[j][k] + b * (in[j-1][k] + in[j+1][k] + in[j][k-1] + in[j][k+1]);
            }
        }
    }

    // blocking
    else {
        for (int bx = 1; bx < narr-1; bx += block_size) {
            for (int j = 1; j < height-1; j++) {
                for (int k = bx; k < MIN(bx + block_size, narr-1); k++) {
                    out[j][k] = a * in[j][k] + b * (in[j-1][k] + in[j+1][k] + in[j][k-1] + in[j][k+1]);
                }
            }
        }
    }

}

int
d_jacobi5p(int ntest, uint64_t *ns, uint64_t *cy, uint64_t kib, ...) {
    int nsize, err;

    nsize = kib; // for stencil kernel, the kib arg means the grid length.

    init_kernel_data(nsize);

    tpprintf(0, 0, 0, "(please ignore the above \"# of Elements ...\" line.)\n");
    tpprintf(0, 0, 0, "grid size: (%d, %d)\n", height, nsize);
    tpprintf(0, 0, 0, "block size in x dim: %d\n", block_size);
    tpprintf(0, 0, 0, "working set size: %.1f KB\n", ((double) nsize) * height * sizeof(double) * 2.0 / 1024);


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

    free_kernel_data(nsize);

    // overall result
    int nskip = 10, freq=1;
    dpipe_k0(ns, cy, nskip, ntest, freq, (6 - 2) * sizeof(double), (height - 2) * (nsize - 2));
    return 0;
}
