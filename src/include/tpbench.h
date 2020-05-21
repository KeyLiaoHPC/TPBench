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
 * tpbench.c
 * Description: Heraders for TPBench.
 * Author: Key Liao
 * Modified: May. 15th, 2020
 * Email: keyliao@sjtu.edu.cn
 * =================================================================================
 */
#include <stdint.h>
#include "error.h"
#include "kernels.h"
#include "groups.h"

// ERROR COD
#define DHLINE "================================================================================\n"
#define HLINE  "--------------------------------------------------------------------------------\n"



int
make_dir(const char *path);

/*
 * function: write_res
 * description: write a 2d data result to prefix_r?_c?_pofix.csv
 * ret: 
 * int, error code
 * args:
 *     char *dir [in]: data file
 *     uint64_t **data [in]: pointer to 2d array
 *     int ndim1 [in]: number of dim1 elements. (data[dim1][dim2])
 *     int ndim2 [in]: number of dim2 elements. (data[dim1][dim2])
 */ 
int
write_csv(char *path, uint64_t **data, int ndim1, int ndim2, char *headers);

void list_kern();
int init_kern(char *kernels, char *groups, int *p_kern, int *p_grp, int *nkern, int *ngrp);
int run_kernel(int id, int ntest, uint64_t **res_ns, uint64_t **res_cy, uint64_t kib);
int run_group(int gid, int ntest, uint64_t **res_ns, uint64_t **res_cy, uint64_t kib);
