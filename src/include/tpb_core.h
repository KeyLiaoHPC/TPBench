/**
 * =================================================================================
 * TPBench - A throughputs benchmarking tool for high-performance computing
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
 * =================================================================================
 * @file tpb_core.h
 * @version 0.3
 * @brief Header for tpb-core library 
 * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
 * @date 2020-05-22
 */

#define _GNU_SOURCE

#include "tperror.h"
#include "tpkernels.h"
#include "tpgroups.h"
#include "tptimer.h"

/**
 * @brief 
 */
void tpb_list_kernel();

/**
 * @brief 
 * @param kernels 
 * @param groups 
 * @param p_kern 
 * @param p_grp 
 * @param nkern 
 * @param ngrp 
 * @return int 
 */
int tpb_init_kernel(char *kernels, char *groups, int *p_kern, int *p_grp, int *nkern, int *ngrp);

/**
 * @brief 
 * @param id 
 * @param ntest 
 * @param res_ns 
 * @param res_cy 
 * @param kib 
 * @return int 
 */
int tpb_run_kernel(int id, int ntest, uint64_t **res_ns, uint64_t **res_cy, uint64_t kib);

/**
 * @brief 
 * @param gid 
 * @param ntest 
 * @param res_ns 
 * @param res_cy 
 * @param kib 
 * @return int 
 */
int tpb_run_group(int gid, int ntest, uint64_t **res_ns, uint64_t **res_cy, uint64_t kib);
