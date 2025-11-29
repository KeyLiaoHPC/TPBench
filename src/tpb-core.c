/*
 * =================================================================================
 * TPBench - A high-precision throughputs benchmarking tool for scientific computing
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
 * 
 * =================================================================================
 * kernels.c
 * Description: Main entry of benchmarking kernels.
 * Author: Key Liao
 * Modified: May. 9th, 2024
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "tpb-core.h"
#include "tpb-io.h"
#include "tpb-types.h"


/**
 * Basic information of tpbench benchmarking sets, init by tpb_init().
 */
// # of kernels, kernel routines, groups and group routines
int nkern, nkrout;

/**
 * @brief set number of kernels, groups, kernel routines, and grou routines.
 * @return int 
 */
int
tpb_init() {
    nkrout = sizeof(kern_info) / sizeof(tpb_kern_info_t);
    nkern = nkrout;

    return 0;
}

int
tpb_run_kernel(int id, tpb_timer_t *timer, int ntest, int64_t *time_arr, uint64_t nkib){
    int err;

    tpprintf(0, 0, 0, "Running Kernel %s - %s\n", kern_info[id].kname, kern_info[id].rname);
    tpprintf(0, 0, 0, "Number of tests: %d\n", ntest);
    tpprintf(0, 0, 0, "# of Elements per Array: %d\n", nkib * 1024 / sizeof(double));
    err = kern_info[id].pfun(timer, ntest, time_arr, nkib);
    tpprintf(0, 0, 0, HLINE);
    return err;
}