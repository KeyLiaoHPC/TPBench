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
 * tl_cgw.c
 * Description: the cg_calc_w kernel from TeaLeaf
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

// cg_calc_w data
static volatile double **w;
static double **Di, **p, **Kx, **Ky;
static double rx = 0.11, ry = 0.22;
static volatile double pw;

static const int height = 100;

// blocking data
int block_x;

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static void
fill_random(double *arr, size_t size) {
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
    w = (double **)malloc(height * sizeof(double*));
    for (size_t i = 0; i < height; i ++) {
        w[i] = (double *)malloc(narr * sizeof(double));
    }
    Di = (double **)malloc(height * sizeof(double*));
    for (size_t i = 0; i < height; i ++) {
        Di[i] = (double *)malloc(narr * sizeof(double));
    }
    p = (double **)malloc(height * sizeof(double*));
    for (size_t i = 0; i < height; i ++) {
        p[i] = (double *)malloc(narr * sizeof(double));
    }
    Kx = (double **)malloc(height * sizeof(double*));
    for (size_t i = 0; i < height; i ++) {
        Kx[i] = (double *)malloc(narr * sizeof(double));
    }
    Ky = (double **)malloc(height * sizeof(double*));
    for (size_t i = 0; i < height; i ++) {
        Ky[i] = (double *)malloc(narr * sizeof(double));
    }

    for (int i = 0; i < height; i ++) {
        fill_random(w[i], narr);
        fill_random(Di[i], narr);
        fill_random(p[i], narr);
        fill_random(Kx[i], narr);
        fill_random(Ky[i], narr);
    }
    fill_random(&rx, 1);
    fill_random(&ry, 1);

    block_x = 0;
    {
        const char *s = getenv("TPBENCH_BLOCK");
        if (NULL != s) {
            block_x = atoi(s);
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
    free_2d_array(w, narr);
    free_2d_array(Di, narr);
    free_2d_array(p, narr);
    free_2d_array(Kx, narr);
    free_2d_array(Ky, narr);
}

static void run_kernel_once(int narr) {
    pw = 0;
    if (block_x == 0) { 
        // no blocking
        for (int i = 1; i < height-1; i ++) {
            for (int j = 1; j < narr-1; j ++) {
                w[i][j] = Di[i][j] * p[i][j]  \
                            - ry * (Ky[i+1][j] * p[i+1][j] + Ky[i][j] * p[i-1][j]) \
                            - rx * (Kx[i][j+1] * p[i][j+1] + Kx[i][j] * p[i][j-1]);
            }
            for (int j = 1; j < narr-1; j ++) {
                pw = pw + w[i][j] * p[i][j];
            }
        }
    }

    // blocking
    else {
        for (int bx = 0; bx < narr - 1; bx += block_x) {
            for (int i = 1; i < height - 1; i++) {
                for (int j = MAX(bx, 1); j < MIN(bx + block_x, narr - 1); j++) {
                    w[i][j] = Di[i][j] * p[i][j]  \
                                - ry * (Ky[i+1][j] * p[i+1][j] + Ky[i][j] * p[i-1][j]) \
                                - rx * (Kx[i][j+1] * p[i][j+1] + Kx[i][j] * p[i][j-1]);
                }
                for (int j = MAX(bx, 1); j < MIN(bx + block_x, narr - 1); j++) {
                    pw = pw + w[i][j] * p[i][j];
                }
            }
        }
    }

}


int
d_tl_cgw(int ntest, uint64_t *ns, uint64_t *cy, uint64_t nsize_uint64, ...) {
    int err;
    int nsize = nsize_uint64;

    init_kernel_data(nsize);

    tpprintf(0, 0, 0, "ignore the \"# of Elements ...\" line.\n");
    tpprintf(0, 0, 0, "grid size: (%d, %d)\n", height, nsize);
    if (block_x)
        tpprintf(0, 0, 0, "block size x: %d\n", block_x);
    tpprintf(0, 0, 0, "working set size: %.1f KB\n", 1.0 * ((double) height) * nsize * sizeof(double) * 5.0 / 1024);


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
    dpipe_k0(ns, cy, nskip, ntest, freq, (11 - 3) * sizeof(double), (height - 2) * (nsize - 2));
    return 0;
}
