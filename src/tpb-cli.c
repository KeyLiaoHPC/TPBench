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
 * @file tpb-cli.c
 * @version 0.71
 * @brief tpbench command line parser 
 * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
 * @date 2024-01-22
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include "tpb-cli.h"
#include "timers/timers.h"
#include "tpb-impl.h"
#include "tpb-core.h"

int
tpb_check_count(int *n, char *strarg)
{
    int len;
    char *ch;
    
    if(strarg[0] == '\0') {
        *n = 0;
        return 0;
    }
    len = strlen(strarg);
    if(strarg[len-1] == ',') {
        strarg[len-1] = '\0';
    }
    *n = 1;
    ch = strarg;
    if(!isalnum(*ch)) {
        return SYNTAX_ERROR;
    }
    while(*ch != '\0') {
        if(!isalnum(*ch) && *ch != ',' && *ch != '_') {
            return SYNTAX_ERROR;
        }
        if(*ch == ',' && *(ch+1) == ',') {
            return SYNTAX_ERROR;
        }
        if(*ch == ',') {
            (*n) += 1;
        }
        ch++;
    }
    return 0;
}

int
tpb_parse_klist(tpb_args_t *tp_args)
{
    int matched;
    char *ch, *che;

    ch = tp_args->kstr;
    for(int seg = 0; seg < tp_args->nkern; seg++) {
        matched = 0;
        che = ch;
        while(1) {
            if(*che == ',') {
                *che = '\0';
                break;
            }
            if(*che == '\0') {
                break;
            }
            che++;
        }
        for(int rid = 0; rid < nkrout; rid++) {
            if(strcmp(ch, kern_info[rid].rname) == 0) {
                tp_args->klist[seg] = rid;
                matched = 1;
                break;
            }
        }
        if(matched == 0) {
            return KERN_NE;
        }
        ch = che + 1;
    }
    return 0;
}

int
tpb_init_list(tpb_args_t *tp_args)
{
    int err;

    if(tp_args->mode == 1) {
        // BenchScore mode: including compute/memory/network.
        // Compute subsystem: fmaldr, mulldr.
        // Memory subsystem: init, copy, scale, striad.
        // Network subsystem: MPI_Send/Recv, MPI_Allreduce.
    } else if(tp_args->mode == 2) {
        // BenchCompute mode
    } else if(tp_args->mode == 3) {
        // BenchMemory mode
    } else if(tp_args->mode == 4) {
        // BenchNetwork mode
    } else if(tp_args->mode == 5) {
        // BenchIO mode
    } else {
        if((err = tpb_check_count(&(tp_args->nkern), tp_args->kstr))) {
            return err;
        }
        tp_args->klist = (int *)malloc(sizeof(int) * tp_args->nkern);
        if(tp_args->klist == NULL) {
            return MALLOC_FAIL;
        }
        err = tpb_parse_klist(tp_args);
        if(tp_args->nkern && err) {
            return err;
        }
        return 0;
    }
    return 0;
}

static int
tpb_set_mode(tpb_args_t *args, const char *arg)
{
    if(strcmp(arg, "BenchScore") == 0) {
        args->mode = 1;
    } else if(strcmp(arg, "BenchCompute") == 0) {
        args->mode = 2;
    } else if(strcmp(arg, "BenchMemory") == 0) {
        args->mode = 3;
    } else if(strcmp(arg, "BenchNetwork") == 0) {
        args->mode = 4;
    } else if(strcmp(arg, "BenchIO") == 0) {
        args->mode = 5;
    } else {
        return SYNTAX_ERROR;
    }
    return 0;
}

static int
tpb_set_timer(tpb_args_t *args, const char *arg)
{
    if(strcmp(arg, "clock_gettime") == 0) {
        sprintf(args->timer, "clock_gettime");
    } else if(strcmp(arg, "tsc_asym") == 0) {
        sprintf(args->timer, "tsc_asym");
    } else {
        return SYNTAX_ERROR;
    }
    return 0;
}

static void
tpb_set_timer_funcs(tpb_timer_t *timer, const char *timer_name)
{
    if(strcmp(timer_name, "clock_gettime") == 0) {
        timer->init = init_timer_clock_gettime;
        timer->tick = tick_clock_gettime;
        timer->tock = tock_clock_gettime;
        timer->get_stamp = get_time_clock_gettime;
    } else if(strcmp(timer_name, "tsc_asym") == 0) {
        timer->init = init_timer_tsc_asym;
        timer->tick = tick_tsc_asym;
        timer->tock = tock_tsc_asym;
        timer->get_stamp = get_time_tsc_asym;
    }
}

void
tpb_print_help(const char *progname)
{
    printf("\n");
    printf("Usage: %s [OPTIONS]\n", progname);
    printf("\n");
    printf("Options:\n");
    printf("  -n, --ntest <# of test>        Overall number of tests.\n");
    printf("  -s, --nkib <kib_size>           Memory usage for a single test array, in KiB.\n");
    printf("  -k, --kernel <kernel_list>      Kernel list. (e.g. -k d_init,d_sum)\n");
    printf("  -L, --list                     List all group and kernels then exit.\n");
    printf("  -d, --data_dir <PATH>           Optional. Data directory.\n");
    printf("  -m, --mode <MODE>               Run mode: Supported modes: Manual (Default), BenchScore,\n");
    printf("                                  BenchCompute, BenchMemory, BenchNetwork, BenchIO.\n");
    printf("  -t, --timer <timer_name>        Timer name: Supported timers: clock_gettime (default), tsc_asym.\n");
    printf("  -h, --help                      Print this help message and exit.\n");
    printf("\n");
}

int
tpb_parse_args(int argc, char **argv, tpb_args_t *tp_args, tpb_timer_t *timer)
{
    int opt;
    int err = 0;

    if(argc <= 1) {
        return SYNTAX_ERROR;
    }

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--help") == 0) {
            tpb_print_help(argv[0]);
            exit(0);
        }
    }

    tp_args->mode = 0;
    tp_args->ntest = 0;
    tp_args->nkib = 0;
    tp_args->nkern = 0;
    tp_args->list_only_flag = 0;
    tp_args->klist = NULL;
    tp_args->data_dir[0] = '\0';
    sprintf(tp_args->timer, "clock_gettime");

    while((opt = getopt(argc, argv, "n:s:k:Ld:m:t:h")) != -1) {
        switch(opt) {
        case 'n':
            tp_args->ntest = atoi(optarg);
            break;
        case 's':
            tp_args->nkib = atoi(optarg);
            break;
        case 'k':
            if(strlen(optarg) > 1023) {
                return SYNTAX_ERROR;
            }
            sprintf(tp_args->kstr, "%s", optarg);
            break;
        case 'L':
            tp_args->list_only_flag = 1;
            break;
        case 'd':
            if(strlen(optarg) > 1023) {
                return SYNTAX_ERROR;
            }
            sprintf(tp_args->data_dir, "%s", optarg);
            break;
        case 'm':
            err = tpb_set_mode(tp_args, optarg);
            if(err) {
                return err;
            }
            break;
        case 't':
            err = tpb_set_timer(tp_args, optarg);
            if(err) {
                return err;
            }
            break;
        case 'h':
            tpb_print_help(argv[0]);
            exit(0);
        case '?':
            tpb_print_help(argv[0]);
            return SYNTAX_ERROR;
        default:
            return SYNTAX_ERROR;
        }
    }

    if(optind < argc) {
        return SYNTAX_ERROR;
    }

    tpb_set_timer_funcs(timer, tp_args->timer);

    if(tp_args->list_only_flag) {
        return 0;
    }

    if(tp_args->ntest == 0 || tp_args->nkib == 0) {
        return ARGS_MISS;
    }

    if(strlen(tp_args->data_dir) == 0) {
        sprintf(tp_args->data_dir, "./data");
        err = DEFAULT_DIR;
    }

    err = tpb_init_list(tp_args);
    return err;
}
