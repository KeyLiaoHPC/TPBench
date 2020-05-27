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
 * kernels.c
 * Description: Main entry of benchmarking kernels.
 * Author: Key Liao
 * Modified: May. 9th, 2020
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "tpb_core.h"

/**
 * @brief set number of kernels, groups, kernel routines, and grou routines.
 * @return int 
 */
int
tpb_init() {
    // count # of routines.
    nkrout = sizeof(kern_info) / sizeof(__kern_info_t);
    ngrout = sizeof(grp_info) / sizeof(__grp_info_t);
    // count # of kernel and groups.
    // TODO: equals to # of routine in current moment.
    nkern = nkrout;
    ngrp = ngrout;

    return 0;
}

int
tpb_run_kernel(int id, int ntest, uint64_t *res_ns, uint64_t *res_cy, uint64_t nkib){
    printf("Running Kernel %s - %s\n", kern_info[id].kname, kern_info[id].rname);
    printf("Number of tests: %d\n", ntest);
    printf("# of Elements per Array: %d\n", nkib * 1024 / sizeof(double));

    return kern_info[id].pfun(ntest, res_ns, res_cy, nkib);
}

int
tpb_run_group(int id, int ntest, uint64_t **res_ns, uint64_t **res_cy, uint64_t nkib){
    return grp_info[id].pfun(ntest, grp_info[id].nepoch+1, res_ns, res_cy, nkib);
}
