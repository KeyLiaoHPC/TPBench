/*
 * =================================================================================
 * Arm-BwBench - A bandwidth benchmarking suite for Armv8
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
 * kernels.c
 * Description: Computing kernels for benchmarking.
 * Author: Key Liao
 * Modified: Apr. 14th, 2020
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */

#include <stdint.h>
#include <stdlib.h>
#include "bench_var.h"

/*
 * init
 * 1 Store + WA, 0 flops
 */

void
init(TYPE *a, TYPE s, int narr, uint64_t *cy){
    uint64_t cy0, cy1;
    asm volatile(
        "DMB NSH"    "\n\t"
        "mrs %0, pmccntr_el0" : "=r" (cy0) : : );

    for(int i = 0; i < narr; i ++){
        a[i] = s;
    }

    asm volatile(
        "DMB NSHST"    "\n\t"
        "mrs %0, pmccntr_el0" : "=r" (cy1) : : );

    *cy = cy1 - cy0;
}

/*
 * sum
 * 1 Load, 1 flops
 */
void
sum(TYPE *a, TYPE *s, int narr, uint64_t *cy){
    uint64_t cy0, cy1;
    asm volatile(
        "DMB NSH"    "\n\t"
        "mrs %0, pmccntr_el0" : "=r" (cy0) : : );

    for(int i = 0; i < narr; i ++){
        *s += a[i];
    }

    asm volatile(
        "DMB NSHST"    "\n\t"
        "mrs %0, pmccntr_el0" : "=r" (cy1) : : );

    *cy = cy1 - cy0;
}

/*
 * copy
 * 1 Load + 1 Store + WA, 0 flops
 */
void
copy(TYPE *a, TYPE *b, int narr, uint64_t *cy){
    uint64_t cy0, cy1;
    asm volatile(
        "DMB NSH"    "\n\t"
        "mrs %0, pmccntr_el0" : "=r" (cy0) : : );

    for(int i = 0; i < narr; i ++){
        a[i] = b[i];
    }

    asm volatile(
        "DMB NSHST"    "\n\t"
        "mrs %0, pmccntr_el0" : "=r" (cy1) : : );

    *cy = cy1 - cy0;
}

/*
 * update
 * 1Load + 1 Store, 1 flops
 */
void
update(TYPE *a, TYPE s, int narr, uint64_t *cy){
    uint64_t cy0, cy1;
    asm volatile(
        "DMB NSH"    "\n\t"
        "mrs %0, pmccntr_el0" : "=r" (cy0) : : );

    for(int i = 0; i < narr; i ++){
        a[i] = s * a[i];
    }

    asm volatile(
        "DMB NSHST"    "\n\t"
        "mrs %0, pmccntr_el0" : "=r" (cy1) : : );

    *cy = cy1 - cy0;
}

/*
 * triad
 * 2 Load + 1 Store + WA, 2 flops
 */
void
triad(TYPE *a, TYPE *b, TYPE *c, TYPE s, int narr, uint64_t *cy){
    uint64_t cy0, cy1;
    asm volatile(
        "DMB NSH"    "\n\t"
        "mrs %0, pmccntr_el0" : "=r" (cy0) : : );

    for(int i = 0; i < narr; i ++){
        //a[i] = b[i] + s * c[i];
        b[i] = a[i] + s * c[i];
    }

    asm volatile(
        "DMB NSHST"    "\n\t"
        "mrs %0, pmccntr_el0" : "=r" (cy1) : : );

    *cy = cy1 - cy0;
}

/*
 * daxpy
 * 2 Load + 1 Store, 2 flops
 */
void
daxpy(TYPE *a, TYPE *b, TYPE s, int narr, uint64_t *cy){
    uint64_t cy0, cy1;
    asm volatile(
        "DMB NSH"    "\n\t"
        "mrs %0, pmccntr_el0" : "=r" (cy0) : : );

    for(int i = 0; i < narr; i ++){
        a[i] = a[i] + b[i] * s;
    }

    asm volatile(
        "DMB NSHST"    "\n\t"
        "mrs %0, pmccntr_el0" : "=r" (cy1) : : );

    *cy = cy1 - cy0;
}

/*
 * striad
 * 3 Load + 1 Store + WA, 2 flops
 */
void
striad(TYPE *a, TYPE *b, TYPE *c, TYPE *d, int narr, uint64_t *cy){
    uint64_t cy0, cy1;
    asm volatile(
        "DMB NSH"    "\n\t"
        "mrs %0, pmccntr_el0" : "=r" (cy0) : : );

    for(int i = 0; i < narr; i ++){
       // a[i] = b[i] + c[i] * d[i];
       b[i] = a[i] + c[i] * d[i];
    }

    asm volatile(
        "DMB NSHST"    "\n\t"
        "mrs %0, pmccntr_el0" : "=r" (cy1) : : );

    *cy = cy1 - cy0;
}

/*
 * sdaxpy
 * 3Load + 1 Store                                                                                                                                                        flops
 */
void
sdaxpy(TYPE *a, TYPE *b, TYPE *c, int narr, uint64_t *cy){
    uint64_t cy0, cy1;
    asm volatile(
        "DMB NSH"    "\n\t"
        "mrs %0, pmccntr_el0" : "=r" (cy0) : : );

    for(int i = 0; i < narr; i ++){
        a[i] = a[i] + b[i] * c[i];
    }

    asm volatile(
        "DMB NSHST"    "\n\t"
        "mrs %0, pmccntr_el0" : "=r" (cy1) : : );

    *cy = cy1 - cy0;
}
