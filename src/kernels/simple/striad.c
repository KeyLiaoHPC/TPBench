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
 * copy.c
 * Description: Stanza Triad, a[i] = b[i] + s * c[i], computing in stride S than 
 * skip l elements.
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

#define MALLOC(_A, _NSIZE)  (_A) = (double *)malloc(sizeof(double) * _NSIZE);   \
                            if((_A) == NULL) {                                  \
                                return  MALLOC_FAIL;                            \
                            }

int
d_striad(int ntest, uint64_t *ns, uint64_t *cy, uint64_t kib, ...) {
    int nsize, err;
    int stride, l;
    volatile double *a, *b, *c;
    register double s = 0.11, jump;

    nsize = kib * 1024 / sizeof(double);
    MALLOC(a, nsize);
    MALLOC(b, nsize);
    MALLOC(c, nsize);
    for(int i = 0; i < nsize; i ++) {
        a[i] = s;
        b[i] = s + i;
        c[i] = s * i;
    }
    stride = 8;
    l = 8;
    jump = stride + l;

    GETTIME_INIT;
    for(int i = 0; i < ntest; i ++){
        GETTIME_ST(i);
        CYCLE_ST(i);
        for(int j = 0; j < nsize; j += jump) {
            for(int k = j; k < stride; k ++) {
                a[k] = b[k] + s * c[k];
            }
        }
        CYCLE_EN(i);
        GETTIME_EN(i);
    }

    free((void *)a);
    free((void *)b);
    free((void *)c);
    return 0;
}
