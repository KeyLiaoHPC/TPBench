/**
  * =================================================================================
  * TPBench - A high-precision throughputs benchmarking tool for HPC
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
  * =================================================================================
  * 
  * @file stream.c
  * @version 0.3
  * @brief   A TPBench-port John McCalpin's STREAM benchmark (mpi version v1.8). 
  * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
  * @date 2024-01-30
  */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "tperror.h"
#include "tptimer.h"
#include "tpdata.h"
#include "tpmpi.h"
#include "tpio.h"


int
d_stream(int ntest, int nepoch, uint64_t **ns, uint64_t **cy, uint64_t nkib) {

    int epoch, i;
    uint64_t narr;
    volatile double *a, *b, *c;
    register double s = 0.42;

    tpprintf(0, 0, 0, "Running STREAM Benchmark\n");
    tpprintf(0, 0, 0, "Number of tests: %d\n", ntest);
    tpprintf(0, 0, 0, "Estimated memory allocation: %llu Kib\n", nkib * 3);
    tpprintf(0, 0, 0, "# of Elements per Array: %d\n", nkib * 1024 / sizeof(double));

    narr = nkib * 1024 / sizeof(double);
    a = (double *)malloc(narr * sizeof(double));
    b = (double *)malloc(narr * sizeof(double));
    c = (double *)malloc(narr * sizeof(double));

    if(a == NULL || b == NULL || c == NULL) {
        return MALLOC_FAIL;
    }

    for(int n = 0; n < narr; n ++) {
        a[n] = 1.0;
        b[n] = 2.0;
        c[n] = 3.0;
    }

    // kernel warm
    struct timespec wts;
    uint64_t wns0, wns1;
    __getns(wts, wns1);
    wns0 = wns1 + 1e9;
    while(wns1 < wns0) {
        for(int j = 0; j < narr; j ++){
            a[j] = a[j] + s * b[j];
        }
        __getns(wts, wns1);
    }
    for(int i = 0; i < narr; i ++) {
        a[i] = 1.0;
    }

    __getns_init;
    __getcy_init;
    __getcy_grp_init;

    // stream start
    for(int n = 0; n < ntest; n++) {
        tpmpi_barrier();
        __getns_2d_st(n, 0);
        __getcy_grp_st(n);

        // kernel 1: Copy
        tpmpi_barrier();
        __getns_2d_st(n, 1);
        __getcy_2d_st(n, 1);
        tpmpi_barrier();
#pragma omp parallel for
		for (i = 0; i < narr; i ++) {
	        c[i] = a[i];
        }
        tpmpi_barrier();
        __getcy_2d_en(n, 1);
        __getns_2d_en(n, 1);

        // kernel 2: Scale
        __getns_2d_st(n, 2);
        __getcy_2d_st(n, 2);
        tpmpi_barrier();
#pragma omp parallel for
		for (i = 0; i < narr; i ++) {
            b[i] = s * c[i];
        }
        tpmpi_barrier();
        __getcy_2d_en(n, 2);
        __getns_2d_en(n, 2);

        // kernel 3: Add
        __getns_2d_st(n, 3);
        __getcy_2d_st(n, 3);
        tpmpi_barrier();
#pragma omp parallel for
        for (i = 0; i < narr; i ++) {
            c[i] = a[i] + b[i];
        }
        tpmpi_barrier();
        __getcy_2d_en(n, 3);
        __getns_2d_en(n, 3);

        // kernel 4: Triad
        __getns_2d_st(n, 4);
        __getcy_2d_st(n, 4);
        tpmpi_barrier();
#pragma omp parallel for
	    for (i = 0; i < narr; i ++) {
            a[i] = b[i] + s * c[i];
        }
        tpmpi_barrier();
        __getcy_2d_en(n, 4);
        __getns_2d_en(n, 4);

        // overall timing end.
        tpmpi_barrier();
        __getcy_grp_en(n);
        __getns_2d_en(n, 0);
        tpmpi_barrier();
    }

    tpprintf(0, 0, 0, "STREAM Benchmark done, processing data.\n");

#ifdef USE_MPI
    uint64_t **all_ns, **all_cy;
    int nskip = 10, freq=1;

    if(ntest <= 10) {
        nskip = 0;
    }
    all_ns = (uint64_t **)malloc(sizeof(uint64_t *) * ntest);
    all_cy = (uint64_t **)malloc(sizeof(uint64_t *) * ntest);
    for(int i = 0; i < ntest; i ++) {
        all_ns[i] = (uint64_t *)malloc(sizeof(uint64_t) * (nepoch + 1));
        all_cy[i] = (uint64_t *)malloc(sizeof(uint64_t) * (nepoch + 1));
    }
    for(int i = 0; i < ntest; i ++) {
        MPI_Reduce(ns[i], all_ns[i], nepoch + 1, MPI_UINT64_T, MPI_MIN, 0, MPI_COMM_WORLD);
        MPI_Reduce(cy[i], all_cy[i], nepoch + 1, MPI_UINT64_T, MPI_MIN, 0, MPI_COMM_WORLD);
    }
    if(tpmpi_info.myrank == 0) {
        tpprintf(0, 0, 0, "STREAM Overall performance\n");
        dpipe_g0(all_ns, all_cy, 0, nskip, ntest, freq, tpmpi_info.nrank * 80, narr);
    
        tpprintf(0, 0, 0, "STREAM-Copy performance\n");
        dpipe_g0(all_ns, all_cy, 1, nskip, ntest, freq, tpmpi_info.nrank * 16, narr);
    
        tpprintf(0, 0, 0, "STREAM-Scale performance\n");
        dpipe_g0(all_ns, all_cy, 2, nskip, ntest, freq, tpmpi_info.nrank * 16, narr);
    
        tpprintf(0, 0, 0, "STREAM-Add performance\n");
        dpipe_g0(all_ns, all_cy, 3, nskip, ntest, freq, tpmpi_info.nrank * 24, narr);
    
        tpprintf(0, 0, 0, "STREAM-Triad performance\n");
        dpipe_g0(all_ns, all_cy, 4, nskip, ntest, freq, tpmpi_info.nrank * 24, narr);

        for(int i = 0 ; i < ntest; i ++) {
            free(all_ns[i]);
            free(all_cy[i]);
        }
        free(all_ns);
        free(all_cy);
    }

    tpmpi_barrier();
#else
    int nskip = 10, freq=1;
    if(ntest <= 10) {
        nskip = 0;
    }
    tpprintf(0, 0, 0, "STREAM Overall performance\n");
    dpipe_g0(ns, cy, 0, nskip, ntest, freq, 80, narr);
    
    tpprintf(0, 0, 0, "STREAM-Copy performance\n");
    dpipe_g0(ns, cy, 1, nskip, ntest, freq, 16, narr);
    
    tpprintf(0, 0, 0, "STREAM-Scale performance\n");
    dpipe_g0(ns, cy, 2, nskip, ntest, freq, 16, narr);
    
    tpprintf(0, 0, 0, "STREAM-Add performance\n");
    dpipe_g0(ns, cy, 3, nskip, ntest, freq, 24, narr);
    
    tpprintf(0, 0, 0, "STREAM-Triad performance\n");
    dpipe_g0(ns, cy, 4, nskip, ntest, freq, 24, narr);
#endif

    free((void *)a);
    free((void *)b);
    free((void *)c);
    return 0;
}