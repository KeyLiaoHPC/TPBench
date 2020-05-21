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
 * kernels.h
 * Description: Kernel informations.
 * Author: Key Liao
 * Modified: May. 9th, 2020
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */
#define _KERNELS_H
#include <stdint.h>
#include <stdarg.h>

// Kernel info type
typedef struct {
    char *name; // Name of kernel
    int uid; // kernel unique id
    uint64_t nbyte; // bytes through core per iteration
    uint64_t nop; // Arithmetic (FL)OPs per iteration.
    int (*pfun)(int, uint64_t *, uint64_t *, uint64_t, ...); //var args for extra args
} Kern_Info_t;

// Kernel declaration
// id 0
int d_init(int ntest,   uint64_t *ns, uint64_t *cy, uint64_t kib, ...);
int d_sum(int ntest,    uint64_t *ns, uint64_t *cy, uint64_t kib, ...);
int d_copy(int ntest,   uint64_t *ns, uint64_t *cy, uint64_t kib, ...);
int d_update(int ntest, uint64_t *ns, uint64_t *cy, uint64_t kib, ...);
int d_triad(int ntest,  uint64_t *ns, uint64_t *cy, uint64_t kib, ...);
int d_axpy(int ntest,   uint64_t *ns, uint64_t *cy, uint64_t kib, ...);
int d_striad(int ntest, uint64_t *ns, uint64_t *cy, uint64_t kib, ...);
int d_staxpy(int ntest, uint64_t *ns, uint64_t *cy, uint64_t kib, ...);
int d_scale(int ntest,  uint64_t *ns, uint64_t *cy, uint64_t kib, ...);

// name, id, nbyte, nop, pfun
// i_: integer
// h_: fp16, half precision
// d_: fp64, double
// s_: fp32, single
// m_: mix
static Kern_Info_t kern_info[] = {
    {"d_init",      0,  8,  0,  d_init},
    {"d_sum",       1,  8,  1,  d_sum},
    {"d_copy",      2,  16, 0,  d_copy},
    {"d_update",    3,  16, 1,  d_update},
    {"d_triad",     4,  24, 2,  d_triad},
    {"d_axpy",      5,  24, 2,  d_axpy},
    {"d_striad",    6,  32, 2,  d_striad},
    {"d_sdaxpy",    7,  32, 2,  d_staxpy},
    {"d_scale",     8,  16, 1,  d_scale}
};