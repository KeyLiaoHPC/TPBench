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
 */
int
run_group(int gid, int ntest, uint64_t **res_ns, uint64_t **res_cy, uint64_t kib) {
    int i, j, k, err;

    err = 0;
    
    return NO_ERROR;
}

/*
 * function: run_kernel
 * description: 
 * ret: 
 * int, error code
 * args:
 */
int
run_kernel(int *ulist, int nkern, int ntest, uint64_t **res_ns, uint64_t **res_cy, uint64_t kib){
    int err, n_all_kern;
    int klist[nkern];
    uint64_t ns[ntest], cy[ntest];
    
    n_all_kern = sizeof(kern_info) / sizeof(Kern_Info_t);

    for(int i = 0; i < nkern; i ++) {
        for(int j = 0; j < n_all_kern; j ++) {
            if(kern_info[i] == kern_info[j].uid) {
                klist[i] = j;
            }
        }
    }

    for(int i = 0; i < nkern; i ++) {
        err = kern_info[klist[i]].pfun(ntest, ns, cy, kib);
        if(err) {
            return err;
        }
        for(int t = 0; t < ntest; t ++) {
            res_ns[t][i] = ns[t];
            res_cy[t][i] = cy[t];
        }
    }
    return NO_ERROR;
}

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
            for(i = 0; i < n_tot_grp; i ++){
                ret = strncmp(sch, group_info[i].name, strlen(group_info[i].name));
                if(ret != 0) {
                    // not match
                    continue;
                }
                // group found
                dup_flag = 0;
                for(j = 0 ; j < *ngrp; j ++) {
                    if(p_grp[j] == group_info[i].gid) {
                        // duplication found
                        dup_flag = 1;
                        break;
                    }
                }
                if(dup_flag == 0) {
                // no dup, append to list
                    p_grp[*ngrp] = group_info[i].gid;
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
                ret = strncmp(sch, kern_info[id].name, strlen(kern_info[id].name));
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