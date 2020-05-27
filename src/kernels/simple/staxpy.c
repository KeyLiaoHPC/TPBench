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
 * staxpy.c
 * Description: Stanza AXPY
 * Author: Key Liao
 * Modified: May. 21st, 2020
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "tptimer.h"
#include "tperror.h"
#include "tpdata.h"

#define MALLOC(_A, _NSIZE)  (_A) = (double *)malloc(sizeof(double) * _NSIZE);   \
                            if((_A) == NULL) {                                  \
                                return  MALLOC_FAIL;                            \
                            }

int
d_staxpy(int ntest, uint64_t *ns, uint64_t *cy, uint64_t kib) {
    int nsize, err;
    int stride, l;
    volatile double *a, *b;
    register double s = 0.11, jump;

    nsize = kib * 1024 / sizeof(double);
    MALLOC(a, nsize);
    MALLOC(b, nsize);
    for(int i = 0; i < nsize; i ++) {
        a[i] = stride;
        b[i] = stride + i;
    }
    stride = 8;
    l = 8;
    jump = stride + l;

    __getcy_init;
    __getns_init;
    for(int i = 0; i < ntest; i ++){
        __getns_1d_st(i);
        __getcy_1d_st(i);
        for(int j = 0; j < nsize; j += jump) {
            for(int k = j; k < stride; k ++) {
                a[k] = a[k] + s * b[k];
            }
        }
        __getcy_1d_en(i);
        __getns_1d_en(i);
    }

    // overall result
    int nskip = 10, freq=1;
    dpipe_k0(ns, cy, nskip, ntest, freq, 32, nsize);

    free((void *)a);
    free((void *)b);
    return 0;
}
