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
#include "tpbench.h"

/*
 * function: run_group
 * description: 
 * ret: 
 * int, error code
 * args:
 *     char *dir [in]: data directory 
 *     uint64_t **data [in]: pointer to 2d array
 *     int ndim1 [in]: number of dim1 elements. (data[dim1][dim2])
 *     int ndim2 [in]: number of dim2 elements. (data[dim1][dim2])
 *     char *prefix [in]: prefix of filename. 
 *     char *posfix [in]: postfix of filename.
 */
int
run_group(int gid, int ntest, int nloops, uint64_t kib, uint64_t **res_ns, uint64_t **res_cy) {
    int i, j, k, err;

    err = 0;
    gname = gid;
    switch(gname){
        case g1_kernel:
            run_g1_kernel(ntest, kib, res_ns, res_cy);
            break;
        // case io_ins:
        //     run_io_ins(ntest, nloops, kib, res_ns, res_cy);
        //     break;
        // case arith_ins:
        //     run_arith_ins(ntest, nloops, kib, res_ns, res_cy);
        //     break;
        // case g1_kernel:
        //     run_g1_kernel(ntest, nloops, kib, res_ns, res_cy);
        //     break;
        // case g2_kernel:
        //     run_g2_kernel(ntest, nloops, kib, res_ns, res_cy);
        //     break;
        // case g3_kernel:
        //     run_g3_kernel(ntest, nloops, kib, res_ns, res_cy);
        //     break;
        // case user_kernel:
        //     run_user_kernel(ntest, nloops, kib, res_ns, res_cy);
        //     break;
        default:
            return KERN_NOT_MATCH;
    }
    
    return err;
}

/*
 * function: run_kernel
 * description: 
 * ret: 
 * int, error code
 * args:
 *     char *dir [in]: data directory 
 *     uint64_t **data [in]: pointer to 2d array
 *     int ndim1 [in]: number of dim1 elements. (data[dim1][dim2])
 *     int ndim2 [in]: number of dim2 elements. (data[dim1][dim2])
 *     char *prefix [in]: prefix of filename. 
 *     char *posfix [in]: postfix of filename.
 */
int
run_kernel(int uid, int kid, int ntest, int nloop, uint64_t kib, uint64_t **res_ns, uint64_t **res_cy) {
    int i, err;
    uint64_t nsize, nbyte;
    TYPE *a, *b, *c, *d;
    TYPE s;

    nbyte = kib * 1024;
    nsize = kib * 1024 / 8;

    a = (double *)malloc(nbyte);
    if(a == NULL) {
        return MEM_FAIL;
    }
    b = (double *)malloc(nbyte);
    if(b == NULL) {
        return MEM_FAIL;
    }
    c = (double *)malloc(nbyte);
    if(c == NULL) {
        return MEM_FAIL;
    }
    d = (double *)malloc(nbyte);
    if(d == NULL) {
        return MEM_FAIL;
    }

    err = 0;
    s = 0.05;
    kname = uid;
    switch(kname) {
        case init:
            for(i = 0; i < ntest; i ++) {
                run_init(a, s, nsize, &res_ns[i][kid], &res_cy[i][kid]);
            }
            break;
        case sum:
            for(i = 0; i < ntest; i ++) {
                run_sum(a, &s, nsize, &res_ns[i][kid], &res_cy[i][kid]);
            }
            break;
        case copy:
            for(i = 0; i < ntest; i ++) {
                run_copy(a, b, nsize, &res_ns[i][kid], &res_cy[i][kid]);
            } 
            break;
        case update:
            for(i = 0; i < ntest; i ++) {
                run_update(a, s, nsize, &res_ns[i][kid], &res_cy[i][kid]);
            }
            break;
        case triad:
            for(i = 0; i < ntest; i ++) {
                run_triad(a, b, c, s, nsize, &res_ns[i][kid], &res_cy[i][kid]);
            }
            break;
        case daxpy:
            for(i = 0; i < ntest; i ++) {
                run_daxpy(a, b, s, nsize, &res_ns[i][kid], &res_cy[i][kid]);
            }
            break;
        case striad:
            for(i = 0; i < ntest; i ++) {
                run_striad(a, b, c, d, nsize, &res_ns[i][kid], &res_cy[i][kid]);
            }
            break;
        case sdaxpy:
            for(i = 0; i < ntest; i ++) {
                run_sdaxpy(a, b, c, nsize, &res_ns[i][kid], &res_cy[i][kid]);
            }
            break;
        default:
            return KERN_NOT_MATCH;
    }
    
    free(a);
    free(b);
    free(c);
    free(d);
    return NO_ERROR;
}

// int 
// run_grp(int gid, int ntests, int narr, int arr_count, int parm_count, int wloop) {
    // int err;
// 
    // err = 0;
    // switch(gid) {
        // case io_ins:
        // case arith_ins:
        // case g1_kernel:
        // case g2_kernel:
        // case g3_kernel:
        // case user_kernel;
    // }
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

int 
init_kern(char *kernels, char *groups, int *p_kern, int *p_grp, 
          int *nkern, int *ngrp) {
    int st_id, en_id, i, j, id, ret; // tag a name
    int n_tot_kern, n_tot_grp, dup_flag; // total number, duplication flag
    char *sch, *ech, *gname; // start char, end char, grp name

    n_tot_grp = grp_end; 
    n_tot_kern = sizeof(kern_info) / sizeof(Kern_Info_t);
    sch = groups;
    ech = sch;
    
    st_id = 0;
    en_id = st_id;
    *nkern = 0;
    *ngrp = 0;
    
    // match group into *p_grp
    while(*sch != '\0' && sch != NULL) {
        
        ech ++;
        en_id ++;
        if(*ech == '\0' || *ech == ','){
            // splitter matched
            ret = 1;
            for(id = 0; id < n_tot_grp; id ++){
                ret = strncmp(sch, g_names[id], en_id - st_id);
                if(ret != 0) {
                    // not match
                    continue;
                }
                // group found
                dup_flag = 0;
                for(j = 0 ; j < *ngrp; j ++) {
                    if(p_grp[j] == id) {
                        // duplication found
                        dup_flag = 1;
                        break;
                    }
                }
                if(dup_flag == 0) {
                // no dup, append to list
                    p_grp[*ngrp] = id;
                    (*ngrp) ++;
                }
                break;
            }
            if(ret) {
                // wrong group name
                return GRP_NOT_MATCH;
            }
            if(*ech == '\0') {
            // already the last one.
                break;
            }
            sch = ech + 1;
            st_id = en_id + 1;
            while(*sch == ','){
                // in case of multiple commas
                sch ++;
                ech ++;
                st_id ++;
                en_id ++;
            }
        }
    }
    // match kernel into *p_kern
    st_id = 0;
    en_id = st_id;
    if(kernels == NULL){
        return 0;
    }
    sch = kernels;
    ech = sch;
    while(*sch != '\0' && sch != NULL) {
        ech ++;
        en_id ++;
        if(*ech == '\0' || *ech == ','){
            // splitter matched
            ret = 1;
            for(id = 0; id < n_tot_grp; id ++){
                ret = strncmp(sch, kern_info[id].name, en_id - st_id);
                if(ret != 0) {
                    // not match
                    continue;
                }
                // kernel found
                dup_flag = 0;
                for(j = 0 ; j <= *nkern; j ++) {
                    if(p_kern[j] == kern_info[id].uid) {
                        // duplication found
                        dup_flag = 1;
                        break;
                    }
                }
                if(dup_flag == 0) {
                // no dup, append to list
                    p_kern[*nkern] = kern_info[id].uid;
                    (*nkern) ++;
                }
                // move to next char
                sch = ech + 1;
                st_id = en_id + 1;
                break;
            }
            if(ret) {
                // wrong group name
                return KERN_NOT_MATCH;
            }
            if(*ech == '\0') {
            // already the last one.
                break;
            }
            sch = ech + 1;
            st_id = en_id + 1;
            while(*sch == ','){
                // in case of multiple commas
                sch ++;
                ech ++;
                st_id ++;
                en_id ++;
            }
        }
    }

    return 0;
}