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

int tpb_init() {
    // count # of routines.
    nkrout = sizeof(kern_info) / sizeof(__kern_info_t);
    ngrout = sizeof(grp_info) / sizeof(__grp_info_t);
    // count # of kernel and groups.
    // TODO: equals to # of routine in current moment.
    nkern = nkrout;
    ngrp = ngrout;

    return 0;
}

/*
 * function: run_group
 * description: 
 * ret: 
 * int, error code
 * args:
 */
// int
// run_group(int gid, int ntest, uint64_t **res_ns, uint64_t **res_cy, uint64_t kib) {
//     int i, j, k, err;

//     err = 0;
    
//     return NO_ERROR;
// }

/*
 * function: run_kernel
 * description: 
 * ret: 
 * int, error code
 * args:
 */
// int
// run_kernel(int *ulist, int nkern, int ntest, uint64_t **res_ns, uint64_t **res_cy, uint64_t kib){
//     int err, n_all_kern;
//     int klist[nkern];
//     uint64_t ns[ntest], cy[ntest];
    
//     n_all_kern = sizeof(kern_info) / sizeof(Kern_Info_t);

//     for(int i = 0; i < nkern; i ++) {
//         for(int j = 0; j < n_all_kern; j ++) {
//             if(kern_info[i] == kern_info[j].rid) {
//                 klist[i] = j;
//             }
//         }
//     }

//     for(int i = 0; i < nkern; i ++) {
//         err = kern_info[klist[i]].pfun(ntest, ns, cy, kib);
//         if(err) {
//             return err;
//         }
//         for(int t = 0; t < ntest; t ++) {
//             res_ns[t][i] = ns[t];
//             res_cy[t][i] = cy[t];
//         }
//     }
//     return NO_ERROR;
// }

void
list_kern(){
    //printf("Supported group and kernels:\n");
    //printf("Group: io_ins\n");
    //printf("Group: arith_ins\n");
    //printf("Group: g1_kernel\n");
    //printf("Group: g2_kernel\n");
    //printf("Group: g3_kernel\n");
    //printf("Group: user_kernel\n");
}

