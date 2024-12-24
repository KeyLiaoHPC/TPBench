/**
 * =================================================================================
 * TPBench - A throughputs benchmarking tool for high-performance computing
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
 * =================================================================================
 * @file cli_parser.h
 * @version 0.3
 * @brief Header for tpbench command line parser 
 * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
 * @date 2024-01-22
 */

#define _GNU_SOURCE

#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "argp.h"

/**
 * @brief tpbench argument struct
 */
typedef struct {
    uint64_t ntest; // [Mandatory] Number of tests per job.
    uint64_t nkib; // [Mandatory] Number of ngrps and nkerns
    char kstr[1024], gstr[1024], data_dir[1024]; // [Mandatory] group and kernels name
    int *klist, *glist; 
    int nkern, ngrp;
    int list_only_flag; // [Optinal] flags for list mode and consecutive run
} __tp_args_t;


// argp option
static struct argp_option options[] = {
    {"ntest",       'n',    "# of test",    0, 
        "Overall number of tests."},
    {"nkib",        's',    "kib_size",     0, 
        "Memory usage for a single test array, in KiB."},
    {"group",       'g',    "group_list",   0, 
        "Group list. (e.g. -g g1_kernel,g2_kernel)."},
    {"kernel",      'k',    "kernel_list",  0,
        "Kernel list.(e.g. -k d_init,d_sum). "},
    {"list",        'L',    0,              0,
        "List all group and kernels then exit." },
    {"data_dir",    'd',    "PATH",         OPTION_ARG_OPTIONAL,
        "Optional. Data directory."},
    { 0 }
};

/**
 * @brief parse cli arguments 
 * @param argc int, argc of main()
 * @param argv char**, argv of main()
 * @param tp_args __tp_args_t*，pointer to argument data struct
 * @return int 
 */
int parse_args(int argc, char **argv, __tp_args_t *tp_args);