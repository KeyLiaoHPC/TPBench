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
 * tpkernels.h
 * Description: Kernel informations.
 * Author: Key Liao
 * Modified: May. 9th, 2024
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */
#ifndef _TPKERNELS_H
#define _TPKERNELS_H

#include <stdint.h>
#include <stdarg.h>

// Kernel info type
typedef struct {
    char *kname; // Name of kernel
    char *rname; // Name of routine
    int rid; // kernel routine id
    uint64_t nbyte; // bytes through core per iteration
    uint64_t nop; // Arithmetic (FL)OPs per iteration.
    int (*pfun)(int, uint64_t *, uint64_t *, uint64_t); //var args for extra args
    char *note;
} __kern_info_t;

// Kernel declaration
// id 0
int d_init(int ntest,   uint64_t *ns, uint64_t *cy, uint64_t kib);
int d_sum(int ntest,    uint64_t *ns, uint64_t *cy, uint64_t kib);
int d_copy(int ntest,   uint64_t *ns, uint64_t *cy, uint64_t kib);
int d_update(int ntest, uint64_t *ns, uint64_t *cy, uint64_t kib);
int d_triad(int ntest,  uint64_t *ns, uint64_t *cy, uint64_t kib);
int d_axpy(int ntest,   uint64_t *ns, uint64_t *cy, uint64_t kib);
int d_striad(int ntest, uint64_t *ns, uint64_t *cy, uint64_t kib);
int d_staxpy(int ntest, uint64_t *ns, uint64_t *cy, uint64_t kib);
int d_scale(int ntest,  uint64_t *ns, uint64_t *cy, uint64_t kib);
int d_tl_cgw(int ntest,  uint64_t *ns, uint64_t *cy, uint64_t kib);
int d_jacobi5p(int ntest,  uint64_t *ns, uint64_t *cy, uint64_t kib);
int d_mulldr(int ntest,  uint64_t *ns, uint64_t *cy, uint64_t kib);
int d_fmaldr(int ntest,  uint64_t *ns, uint64_t *cy, uint64_t kib);
int d_rtriad(int ntest, uint64_t *ns, uint64_t *cy, uint64_t kib);

// name, id, nbyte, nop, pfun
// i_: integer
// h_: fp16, half precision
// d_: fp64, double
// s_: fp32, single
// m_: mix
static __kern_info_t kern_info[] = {
    {"init",        "d_init",       0,  8,  0,  d_init,
      "FP64 init."},
    {"sum",         "d_sum",        1,  8,  1,  d_sum,
      "FP64 sum."},
    {"copy",        "d_copy",       2,  16, 0,  d_copy,
      "FP64 copy."},
    {"update",      "d_update",     3,  16, 1,  d_update,
      "FP64 update."},
    {"triad",       "d_triad",      4,  24, 2,  d_triad,
      "FP64 STREAM Triad."},
    {"axpy",        "d_axpy",       5,  24, 2,  d_axpy,
      "FP64 AXPY."},
    {"striad",      "d_striad",     6,  32, 2,  d_striad,
      "FP64 Stanza Triad."},
    {"staxpy",      "d_staxpy",     7,  32, 2,  d_staxpy,
      "FP64 Stanza AXPY"},
    {"scale",       "d_scale",      8,  16, 1,  d_scale,
      "FP64 STREAM scale."},
    {"tl_cgw",       "d_tl_cgw",    9,  11 * 8, 13,  d_tl_cgw,
      "TeaLeaf cg_calc_w kernel"},
    {"jacobi5p",     "d_jacobi5p",  10,  6 * 8,  6,  d_jacobi5p,
      "5 point jacobi stencil kernel"},
    {"mulldr",     "d_mulldr",      11,  8,  1,  d_mulldr,
      "mul + load kernel."},
    {"fmaldr",     "d_fmaldr",      12,  8,  1,  d_fmaldr,
      "fma + load kernel."},
    {"rtriad",      "d_rtriad",     13,  24, 2,  d_rtriad,
      "Repeated Triad kernel."},
};

#endif // #ifndef _TPKERNELS_H