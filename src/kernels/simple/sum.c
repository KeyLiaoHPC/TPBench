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
 * init.c
 * Description: init
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
#include "tpmpi.h"

#define MALLOC(_A, _NSIZE)  (_A) = (double *)malloc(sizeof(double) * _NSIZE);   \
                            if((_A) == NULL) {                                  \
                                return  MALLOC_FAIL;                            \
                            }

int
d_sum(int ntest, uint64_t *ns, uint64_t *cy, uint64_t kib, ...) {
    int nsize, err;
    volatile double *a;
    register double s = 0.11;

    nsize = kib * 1024 / sizeof(double);
    a = (double *)malloc(sizeof(double) * nsize);
    MALLOC(a, nsize);
    for(int i = 0; i < nsize; i ++) {
        a[i] = s;
    }

    // kernel warm
    struct timespec wts;
    uint64_t wns0, wns1;
    __getns(wts, wns1);
    wns0 = wns1 + 1e9;
    while(wns1 < wns0) {
        for(int j = 0; j < nsize; j ++){
            s += a[j];
        }
        __getns(wts, wns1);
    }

    __getcy_init;
    __getns_init;
    for(int i = 0; i < ntest; i ++){
        tpmpi_dbarrier();
        __getns_1d_st(i);
        __getcy_1d_st(i);
        for(int j = 0; j < nsize; j ++){
            s += a[j];
        }
        __getcy_1d_en(i);
        __getns_1d_en(i);
        s = s / a[nsize-1] / nsize;
    }

    // overall result
    int nskip = 10, freq=1;

    dpipe_k0(ns, cy, nskip, ntest, freq, 8, nsize);
    free((void *)a);
    return 0;
}