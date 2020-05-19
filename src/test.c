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
 * test.c
 * Description: Main entry of TPBench. 
 * Author: Key Liao
 * Modified: May. 9th, 2020
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */
#define _GNU_SOURCE
#define VER "0.3"

#include <stdio.h>
#include <stdlib.h>
#include "tpbench.h"

int
test_run_group() {
    int i, j, err;
    uint64_t **res_ns, **res_cy;

    res_ns = (uint64_t **)malloc(sizeof(uint64_t) * 10);
    res_cy = (uint64_t **)malloc(sizeof(uint64_t) * 10);
    for(i = 0; i < 10; i ++) {
        res_ns[i] = (uint64_t *)malloc(sizeof(uint64_t) * 8);
        res_cy[i] = (uint64_t *)malloc(sizeof(uint64_t) * 8);
    }
    err = run_group(0, 10, 1, 2048, res_ns, res_cy);
    for(i = 0; i < 10; i ++) {
        for(j = 0; j < 8; j ++) {
            printf("%llu, ", res_ns[i][j]) ;
        }
        printf("\n");
        for(j = 0; j < 8; j ++) {
            printf("%llu, ", res_cy[i][j]) ;
        }
        printf("\n");
    }
    for(i = 0; i < 10; i ++) {
        free(res_ns[i]);
        free(res_cy[i]);
    }
    free(res_ns);
    free(res_cy);
    return err;
}

int test_run_kernel() {
    int i, j;
    uint64_t **res_ns, **res_cy;

    res_ns = (uint64_t **)malloc(sizeof(uint64_t) * 10);
    res_cy = (uint64_t **)malloc(sizeof(uint64_t) * 10);
    for(i = 0; i < 10; i ++) {
        res_ns[i] = (uint64_t *)malloc(sizeof(uint64_t) * 8);
        res_cy[i] = (uint64_t *)malloc(sizeof(uint64_t) * 8);
    }

    for(i = 0; i < 8; i ++) {
        run_kernel(i, i, 10, 1, 2048, res_ns, res_cy);
    }
    for(i = 0; i < 10; i ++) {
        for(j = 0; j < 8; j ++) {
            printf("%llu, ", res_ns[i][j]) ;
        }
        printf("\n");
        for(j = 0; j < 8; j ++) {
            printf("%llu, ", res_cy[i][j]) ;
        }
        printf("\n");
    }

    for(i = 0; i < 10; i ++) {
        free(res_ns[i]);
        free(res_cy[i]);
    }
    free(res_ns);
    free(res_cy);
    return 0;
}

int
main(void) {
    int err;
    printf("TEST run_group\n");
    
    if(err = test_run_group()) {
        printf("FAIL run_group [%d]\n", err);
    }
    printf("TEST run_kernel\n");
    test_run_kernel();
    return 0;
}