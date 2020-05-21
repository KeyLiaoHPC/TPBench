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
 * groups.h
 * Description: Kernel informations.
 * Author: Key Liao
 * Modified: May. 9th, 2020
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */
#define _GROUPS_H
#include <stdint.h>
#include <stdarg.h>

// Group infor type
typedef struct {
    char *name;
    int gid, nepoch; // group id, # of epochs
    int (*pfun)(int, int, uint64_t **, uint64_t **, uint64_t, ...);//var args for extra args
} Group_Info_t;

// Group declaration
int d_stream(int ntest, int nepoch, uint64_t **ns, uint64_t **cy, uint64_t kib, ...);

// Group info list
static Group_Info_t group_info[] = {
    {   "stream",      0,  4,
        d_stream}
};

