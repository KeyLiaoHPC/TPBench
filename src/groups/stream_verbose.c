/**
  * =================================================================================
  * TPBench - A high-precision throughputs benchmarking tool for HPC
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
  * =================================================================================
  * 
  * @file stream_verbose.c
  * @version 0.3
  * @brief   A STREAM benchmark to collect performance of each core.
  * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
  * @date 2020-05-30
  */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "tperror.h"
#include "tptimer.h"
#include "tpdata.h"
#include "tpio.h"
#include "tpmpi.h"


int
d_stream_verbose(int ntest, int nepoch, uint64_t **ns, uint64_t **cy, uint64_t nkib) {

    int epoch, i ;
    uint64_t narr;
    volatile double *a, *b, *c;
    register double s = 0.42;

    tpprintf(0, 0, 0, "Running STREAM Benchmark for verbose data.\n");
    tpprintf(0, 0, 0, "CAUTION: The overall data output here is calculated across every single test spot.\n");
    tpprintf(0, 0, 0, "TRUE kernel runtime is evaluted in every test spot, syncing is only for contorlling.\n");
    tpprintf(0, 0, 0, "kernels to start at the same time.\n");
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
        tpmpi_dbarrier();
        __getns_2d_st(n, 0);
        __getcy_grp_st(n);

        // kernel 1: Copy
        tpmpi_dbarrier();
        __getns_2d_st(n, 1);
        __getcy_2d_st(n, 1);
#pragma omp parallel for
		for (i = 0; i < narr; i ++) {
	        c[i] = a[i];
        }
        __getcy_2d_en(n, 1);
        __getns_2d_en(n, 1);

        // kernel 2: Scale
        tpmpi_dbarrier();
        __getns_2d_st(n, 2);
        __getcy_2d_st(n, 2);
#pragma omp parallel for
		for (i = 0; i < narr; i ++) {
            b[i] = s * c[i];
        }
        __getcy_2d_en(n, 2);
        __getns_2d_en(n, 2);

        // kernel 3: Add
        tpmpi_dbarrier();
        __getns_2d_st(n, 3);
        __getcy_2d_st(n, 3);
#pragma omp parallel for
        for (i = 0; i < narr; i ++) {
            c[i] = a[i] + b[i];
        }
        __getcy_2d_en(n, 3);
        __getns_2d_en(n, 3);

        // kernel 4: Triad
        tpmpi_dbarrier();
        __getns_2d_st(n, 4);
        __getcy_2d_st(n, 4);
#pragma omp parallel for
	    for (i = 0; i < narr; i ++) {
            a[i] = b[i] + s * c[i];
        }
        __getcy_2d_en(n, 4);
        __getns_2d_en(n, 4);

        // overall timing end.
        __getcy_grp_en(n);
        __getns_2d_en(n, 0);
    }

    free((void *)a);
    free((void *)b);
    free((void *)c);
    tpprintf(0, 0, 0, "Verbose STREAM Benchmark done, processing data.\n");

    // skip some tests in the front 
    int nskip = 10, freq=1;
    uint64_t nitem, snitem;
    MPI_Status stat;
    snitem = ntest - nskip;
    if(ntest <= 10) {
        nskip = 0;
    }
    if(tpmpi_info.myrank == 0) {
        uint64_t *all_ns, *all_cy;
        size_t bpi[] = {80, 16, 16, 24, 24};

        nitem = tpmpi_info.nrank * snitem;
        all_ns = (uint64_t *)malloc(sizeof(uint64_t) * nitem);
        __error_eq(all_ns, NULL, MALLOC_FAIL);
        all_cy = (uint64_t *)malloc(sizeof(uint64_t) * nitem);
        __error_eq(all_cy, NULL, MALLOC_FAIL);
        for(int eid = 0; eid < 5; eid ++) {
            for(int i = nskip; i < ntest; i ++) {
                all_ns[i-nskip] = ns[i][eid];
                all_cy[i-nskip] = cy[i][eid];
            }
            tpmpi_barrier();
            for(int i = 1; i < tpmpi_info.nrank; i ++) {
                MPI_Recv(all_ns + i * snitem, snitem, MPI_UINT64_T, i, 101, MPI_COMM_WORLD, &stat);
                MPI_Recv(all_cy + i * snitem, snitem, MPI_UINT64_T, i, 102, MPI_COMM_WORLD, &stat);
            }
            // process data.
            tpprintf(0, 0, 0, "Epoch %d: \n", eid);
            // process overall data
            dpipe_k0(all_ns, all_cy, 0, nitem, freq, bpi[eid], narr);
        }
        free(all_ns);
        free(all_cy);
    }
    else {
        uint64_t epoch_ns[snitem], epoch_cy[snitem];
        for(int eid = 0; eid < 5; eid ++) {
            for(int i = 0; i < snitem; i ++) {
                epoch_ns[i] = ns[i+nskip][eid];
                epoch_cy[i] = cy[i+nskip][eid];
            }
            tpmpi_barrier();
            MPI_Send(epoch_ns, snitem, MPI_UINT64_T, 0, 101, MPI_COMM_WORLD);
            MPI_Send(epoch_cy, snitem, MPI_UINT64_T, 0, 102, MPI_COMM_WORLD);
        }
    }
    
    // printf("STREAM Overall performance\n");
    // dpipe_g0(ns, cy, 0, nskip, ntest, freq, 80, narr);
    // 
    // printf("STREAM-Copy performance\n");
    // dpipe_g0(ns, cy, 1, nskip, ntest, freq, 16, narr);
    // 
    // printf("STREAM-Scale performance\n");
    // dpipe_g0(ns, cy, 2, nskip, ntest, freq, 16, narr);
    // 
    // printf("STREAM-Add performance\n");
    // dpipe_g0(ns, cy, 3, nskip, ntest, freq, 24, narr);
    // 
    // printf("STREAM-Triad performance\n");
    // dpipe_g0(ns, cy, 4, nskip, ntest, freq, 24, narr);


    return 0;
}
