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
  * @file stream.c
  * @version 0.3
  * @brief   A TPBench-port John McCalpin's STREAM benchmark.
  * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
  * @date 2020-05-22
  */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdint.h>
#include "tperror.h"
#include "tptimer.h"


int
d_stream(int ntest, int nepoch, uint64_t **ns, uint64_t **cy, uint64_t nkib) {

    int epoch, i ;
    uint64_t narr;
    volatile double *a, *b, *c;
    register double s = 0.42;


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
    __getcy_init;
    __getns_init;
    for(int n = 0; n < ntest; n++) {
        __getns_2d_st(n, 0);
        __getcy_2d_st(n, 0);
		    // kernel 1: Copy
        __getns_2d_st(n, 1);
        __getcy_2d_st(n, 1);
#pragma omp parallel for
		    for (i = 0; i < narr; i ++)
		    	  c[i] = a[i];
        __getcy_2d_en(n, 1);
        __getns_2d_en(n, 1);

		    // kernel 2: Scale
        __getns_2d_st(n, 2);
        __getcy_2d_st(n, 2);
#pragma omp parallel for
		    for (i = 0; i < narr; i ++)
            b[i] = s * c[i];
        __getcy_2d_en(n, 2);
        __getns_2d_en(n, 2);

        // kernel 3: Add
        __getns_2d_st(n, 3);
        __getcy_2d_st(n, 3);
#pragma omp parallel for
		    for (i = 0; i < narr; i ++)
            c[i] = a[i] + b[i];
        __getcy_2d_en(n, 3);
        __getns_2d_en(n, 3);

        // kernel 4: Triad
        __getns_2d_st(n, 4);
        __getcy_2d_st(n, 4);
#pragma omp parallel for
		    for (i = 0; i < narr; i ++)
            a[i] = b[i] + s * c[i];
        __getcy_2d_en(n, 4);
        __getns_2d_en(n, 4);
        __getcy_2d_en(n, 0);
        __getns_2d_en(n, 0);
    }


    free((void *)a);
    free((void *)b);
    free((void *)c);
    return 0;
}