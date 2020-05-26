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
 * tpgroups.h
 * Description: Kernel informations.
 * Author: Key Liao
 * Modified: May. 9th, 2020
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */
#ifndef _TPGROUPS_H
#define _TPGROUPS_H
#include <stdint.h>
#include <stdarg.h>

// Group infor type
/**
 * @brief 
 */
typedef struct {
    char *gname;
    char *rname;
    int rid, nepoch; // group routine id, # of epochs
    int (*pfun)(int, int, uint64_t **, uint64_t **, uint64_t);//var args for extra args
    char *note;
    char *header; // a string split by comma of names of each epoch
} __grp_info_t;


// Group declaration
int d_stream(int ntest, int nepoch, uint64_t **ns, uint64_t **cy, uint64_t kib);

// Group info list
static __grp_info_t grp_info[] = {
    {"stream",       "d_stream",      0,  4,     d_stream,
     "FP64 STREAM Bemchmark.",
     "copy,scale,add,triad"}
};

#endif // #ifndef _TPGROUPS_H