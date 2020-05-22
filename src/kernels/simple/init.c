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

int
d_init(int ntest, uint64_t *ns, uint64_t *cy, uint64_t kib, ...) {
    int nsize, err;
    volatile double *a;
    register double s = 0.99;

    nsize = kib * 1024 / sizeof(double);
    a = (double *)malloc(sizeof(double) * nsize);
    if(a == NULL) {
        return MALLOC_FAIL;
    }
    GETTIME_INIT;
    for(int i = 0; i < ntest; i ++){
        GETTIME_ST(i);
        CYCLE_ST(i);
        for(int j = 0; j < nsize; j ++){
            a[j] = s;
        }
        CYCLE_EN(i);
        GETTIME_EN(i);
    }

    free((void *)a);
    return 0;
}

int
s_init(int ntest, uint64_t *ns, uint64_t *cy, uint64_t kib, ...) {
    int nsize, err;
    volatile float *a;
    register float s = 0.99;

    nsize = kib * 1024 / sizeof(float);
    a = (float *)malloc(sizeof(float) * nsize);
    if(a == NULL) {
        return MALLOC_FAIL;
    }
    GETTIME_INIT;
    for(int i = 0; i < ntest; i ++){
        GETTIME_ST(i);
        CYCLE_ST(i);
        for(int j = 0; j < nsize; j ++){
            a[j] = s;
        }
        CYCLE_EN(i);
        GETTIME_EN(i);
    }

    free((void *)a);
    return 0;
}

#ifdef __aarch64__
int
h_init(int ntest, uint64_t *ns, uint64_t *cy, uint64_t kib, ...) {
    int nsize, err;
    volatile __fp16 *a;
    register __fp16 s = 0.99;

    nsize = kib * 1024 / sizeof(__fp16);
    a = (__fp16 *)malloc(sizeof(__fp16) * nsize);
    if(a == NULL) {
        return MALLOC_FAIL;
    }
    GETTIME_INIT;
    for(int i = 0; i < ntest; i ++){
        GETTIME_ST(i);
        CYCLE_ST(i);
        for(int j = 0; j < nsize; j ++){
            a[j] = s;
        }
        CYCLE_EN(i);
        GETTIME_EN(i);
    }
    free((void *)a);
    return 0;
}
#endif //#ifdef __aarch64__

int
i_init(int ntest, uint64_t *ns, uint64_t *cy, uint64_t kib, ...) {
    int nsize, err;
    volatile int *a;
    register int s = 0.99;

    nsize = kib * 1024 / sizeof(int);
    a = (int *)malloc(sizeof(int) * nsize);
    if(a == NULL) {
        return MALLOC_FAIL;
    }
    GETTIME_INIT;
    for(int i = 0; i < ntest; i ++){
        GETTIME_ST(i);
        CYCLE_ST(i);
        for(int j = 0; j < nsize; j ++){
            a[j] = s;
        }
        CYCLE_EN(i);
        GETTIME_EN(i);
    }
    free((void *)a);
    return 0;
}