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

// ERROR CODE
#define NO_ERROR        0
#define GRP_NOT_MATCH   1
#define KERN_NOT_MATCH  2
#define FILE_OPEN_FAIL  3
#define MEM_FAIL        4
#define TYPE    double

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

// Kernel Information
typedef struct {
    char *name; // Name of kernel
    int gid, uid; // group id, kernel id, unified id
    int narr, nparm; // necessary number of arr and parameters
    unsigned int bytes, wabytes, ops; // wabytes is not included in bytes
} Kern_Info_t;

// Group Information
typedef struct {
    char *name;
    int gid, nkern;
} Group_Info_t;

static Kern_Info_t kern_info[] = {
    {"init", 2, 0, 1, 1, 8, 8, 0},
    {"sum", 2, 1, 1, 1, 8, 0, 1},
    {"copy", 2, 2, 2, 1, 16, 8, 0},
    {"update", 2, 3, 1, 1, 16, 0, 1},
    {"triad", 2, 4, 3, 1, 24, 8, 2},
    {"daxpy", 2, 5, 2, 1, 24, 0, 2},
    {"striad", 2, 6, 4, 1, 32, 8, 2},
    {"sdaxpy", 2, 7, 3, 1, 32, 0, 2}
};

static Group_Info_t group_info[] = {
    //{"io_ins", 0, 0},
    //{"arith_ins", 1, 0},
    {"g1_kernel", 2, 8},
    //{"g2_kernel", 3, 0},
    //{"g3_kernel", 4, 0},
    //{"user_kernel", 5, 0},
};
// Group and kernels information
//const static char *g_names[] = {"io_ins", "arith_ins", "g1_kernel", "g2_kernel", 
//                                "g3_kernel", "user_kernel"};
const static char *k_names[] = {"init", "sum", "copy", "update", "triad", "daxpy",
                                "striad", "sdaxpy"};
//static enum gnames {io_ins, arith_ins, g1_kernel, g2_kernel, g3_kernel, 
//                    user_kernel, grp_end};
const static char *g_names[] = {"g1_kernel"};
//enum gnames gname;
static enum gnames {g1_kernel, grp_end};
static enum gnames gname;
//enum knames kname;
static enum knames {init, sum, copy, update, triad, daxpy, striad, sdaxpy};
static enum knames kname;

void list_kern();
int init_kern(char *kernels, char *groups, int *p_kern, int *p_grp, int *nkern, int *ngrp);
int run_kernel(int uid, int kid, int ntests, int nloops, uint64_t kib, uint64_t **res_ns, uint64_t **res_cy);
int run_group(int gid, int ntests, int nloops, uint64_t kib, uint64_t **res_ns, uint64_t **res_cy);

int run_g1_kernel(int ntests, uint64_t kib, uint64_t **res_ns, uint64_t **res_cy);

void run_init(TYPE *a,  TYPE s, int nsize,   uint64_t *ns,   uint64_t *cy);
void run_sum(TYPE *a,   TYPE *s, int nsize, uint64_t *ns, uint64_t *cy);
void run_copy(TYPE *a, TYPE *b, int nsize, uint64_t *ns, uint64_t *cy);
void run_update(TYPE *a, TYPE s, int nsize, uint64_t *ns, uint64_t *cy);
void run_triad(TYPE *a, TYPE *b, TYPE *c, TYPE s, int nsize, uint64_t *ns, uint64_t *cy);
void run_daxpy(TYPE *a, TYPE *b, TYPE s, int nsize, uint64_t *ns, uint64_t *cy);
void run_striad(TYPE *a, TYPE *b, TYPE *c, TYPE *d, int nsize, uint64_t *ns, uint64_t *cy);
void run_sdaxpy(TYPE *a, TYPE *b, TYPE *c, int nsize, uint64_t *ns, uint64_t *cy);