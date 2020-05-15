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
 * main.c
 * Description: Main entry of TPBench. 
 * Author: Key Liao
 * Modified: May. 9th, 2020
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */

#define _GNU_SOURCE
#define VER "0.3"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sched.h>
#include <time.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <argp.h>


#include "utils.h"
#include "kernels.h"


#ifdef USE_MPI
#include "mpi.h"
#endif

// =============================================================================
// START - argp area
// =============================================================================
const char *argp_program_version = "TPBench-v0.3, maintaining by Key Liao, "
    "Center for HPC, Shanghai Jiao Tong University";
const char *argp_program_bug_address = "keyliao@sjtu.edu.cn";
static char doc[] = "TPBench - A parallel high-precision throughputs "
    "benchmarking tool for High-Performance Computing.";
static char args_doc[] = "";

// Type for arugments.
struct TP_Args_t{
    uint64_t ntest, nloop; // [Optional] Number of outer loops and inner looops
    uint64_t kib_size; // [Mandatory] Number of ngrps and nkerns
    char *groups, *kernels, *data_dir; // [Mandatory] group and kernels name
    int list_flag, cons_flag; // [Optinal] flags for list mode and consecutive run
};

static struct argp_option options[] = {
    {"ntest", 'n', "# of test", 0, 
        "Overall number of tests."},
    {"nloop", 'l', "# of loop", OPTION_ARG_OPTIONAL, 
        "Optional. Number of inner loop. (e.g. instruction benchmark)."},
    {"size", 's', "kib_size",  0, 
         "Memory usage for a single test array, in KiB."},
    {"group", 'g', "group_list", 0, 
        "Group list. (e.g. -g g1_kernel,g2_kernel)."},
    {"kernel", 'k', "kernel_list", 0,
        "Kernel list.(e.g. -k init,sum). "},
    {"list", 'L', 0, 0,
        "List all group and kernels then exit." },
    {"consecutive", 'c', 0, 0,
        "Set this if you want to run kernels in group consecutively with data dependency."},
    {"data_dir", 'd', "data_dir", OPTION_ARG_OPTIONAL, "Optional. Data directory."},
    { 0 }
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state) {
    struct TP_Args_t *args = state->input;
    switch(key) {
        case 'n':
            args->ntest = atoi(arg);
            break;
        case 'l':
            args->nloop = atoi(arg);
            break;
        case 's':
            args->kib_size = atoi(arg);
            break;
        case 'g':
            args->groups = arg;
            break;
        case 'k':
            args->kernels = arg;
            break;
        case 'L':
            args->list_flag = 1;
            break;
        case 'c':
            args->cons_flag = 1;
        case 'o':
            args->data_dir = arg;
        case ARGP_KEY_ARG:
            argp_usage(state);
            break;
        case ARGP_KEY_END:
            break;
        default:
          return ARGP_ERR_UNKNOWN;
    }
    return 0;
}
// =============================================================================
// END - argp area
// =============================================================================

int
main(int argc, char **argv) {
    // process info
    int nrank, myid, err, cpuid;
    // iter variables
    int i, j, k;
    struct TP_Args_t tp_args;
    //char fname[1024], dirname[1024]; // Cycle filename.
    //TYPE s, eps[5] = {0,0,0,0,0}; // s

    // nkerns: # of standalone kernels.
    // ngrps: # of group only for consecutive run
    int n_tot_kern, n_tot_grp, nkern, ngrp; 
    // benchmark groups and kernels
    int *grps, *kerns;
    // double *a, *b, *c, *d; 
    // TYPE sa, sb, sc, sd, ss;
    // uint64_t cys[NTIMES][8];
    // double tps[NTIMES][8], tp_mean[8], tp_max[8], tp_min[8]; // Throughputs
#ifdef USE_MPI
    double **partps; // cpu cys, parallel cys
#endif
    uint64_t cy0, cy1, t0, t1;
    FILE    *fp;

    static struct argp argp = { options, parse_opt, args_doc, doc };
// =============================================================================
// Starting....
// =============================================================================
    // MPI initilization.
    nrank = -1;
    myid = -1;
#ifdef USE_MPI
    err = MPI_Init(NULL, NULL);
    if(err != MPI_SUCCESS) {
       printf("EXIT: MPI Initialization failed. [errno: %d]\n", err);
       exit(1);
    }
    // if either of these fail there is something really screwed up!
    MPI_Comm_size(MPI_COMM_WORLD, &nrank);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);
    if(myid == -1 || nrank == -1){
        printf("EXIT: Something wrong at acquiring MPI communicator.\n");
    }
#else
    nrank = 1;
    myid = 0;
#endif
    if(myid == 0){
        printf(DHLINE "TPBench v" VER "\n");
    }

    // Parse args
    tp_args.ntest = 0;
    tp_args.nloop = 24;
    tp_args.kib_size = 0;
    tp_args.list_flag = 0;
    tp_args.cons_flag = 0;
    tp_args.kernels = NULL;
    tp_args.groups = NULL;
    tp_args.data_dir = NULL;
    argp_parse (&argp, argc, argv, 0, 0, &tp_args);
    // List only.
    if(tp_args.list_flag){
        if(myid == 0){
            // TODO: list_kern() is still hard coded.
            list_kern();
        }
#ifdef USE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Finalize();
#endif
        exit(0);
    }
    // Check mandatory options
    if(tp_args.kernels == NULL  && tp_args.groups == NULL) {
        if(myid == 0) {
            printf(HLINE "EXIT: Invalid kernel/group list. See tpbench.x --help for help.\n" DHLINE);
        }
#ifdef USE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Finalize();
#endif
        exit(0);
    }
    if(tp_args.ntest == 0) {
        if(myid == 0) {
            printf(HLINE "EXIT: Invalid number of tests. See tpbench.x --help for help.\n" DHLINE);
        }
#ifdef USE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Finalize();
#endif
        exit(0);
    }
    if(tp_args.kib_size < 1) {
        if(myid == 0) {
            printf(HLINE "EXIT: Invalid array size. See tpbench.x --help for help.\n" DHLINE);
        }
#ifdef USE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Finalize();
#endif
        exit(0);
    }
    // Print arguments
    if(myid == 0) {
        printf(HLINE "Array size is set to %llu KiB.\nEach kernel will run for %llu times.\n",
               tp_args.kib_size, tp_args.ntest);
    }
    if(tp_args.data_dir == NULL) {
        tp_args.data_dir = (char *)malloc(sizeof(char) * 256);
        err = sprintf(tp_args.data_dir, "./data");
        if(err < 0) {
            if(myid == 0){
                printf("EXIT: Failed at setting output data directory.\n" DHLINE);
            }
#ifdef USE_MPI
            MPI_Barrier(MPI_COMM_WORLD);
            MPI_Finalize();
#endif
            exit(0);
        }
        
    }
    if(myid == 0) {
        printf("All csv files will be saved in %s.\n", tp_args.data_dir);
    }

// =============================================================================
// STAGE 1 - Initialization
// =============================================================================
    // Prepare kernels according to input arguments.
    // Using kern_list to run kernels one by one.
    grps = NULL;
    kerns = NULL;
    n_tot_kern = sizeof(kern_info) / sizeof(Kern_Info_t);
    n_tot_grp = grp_end;
    grps = (int *)malloc(n_tot_grp * sizeof(int));
    kerns = (int *)malloc(n_tot_kern * sizeof(int));
    // Get the array of gid and uid
    
    err = init_kern(tp_args.kernels, tp_args.groups, 
                    kerns, grps, tp_args.cons_flag, &nkern, &ngrp);
    if(err){
        printf("%s\n%s\n", tp_args.groups, tp_args.kernels);
        printf("EXIT: Fail at init_kern.[%d]\nTPBench Exit\n" DHLINE, err);
        return 0;
    }

    // Print kernel list.
    if(myid == 0) {
        printf(HLINE "Running %d groups and %d kernels.\n"
               "Group list: ", ngrp, nkern);
        for(i = 0; i < ngrp; i ++)
            printf("%s, ", g_names[grps[i]]);
        printf("\nKernel list: ");
        for(i = 0; i < nkern; i ++) {
            for(j = 0; j < n_tot_kern; j ++){
                if(kern_info[j].uid == kerns[i]) {
                    printf("%s, ", kern_info[j].name);
                }
            }
        }
        switch(tp_args.cons_flag){
            case 0:
                printf("\nGroup kernels run stand-alone.\n" HLINE);
                break;
            case 1:
                printf("\nGroup kernels run consecutively.\n" HLINE);
                break;
            default:
                printf("EXIT: Error on consecutive flag.\n");
                exit(0);
                break;
        }
        fflush(stdout);
    }

// =============================================================================
// STAGE 2 - Benchmarking
// =============================================================================
    for(int i = 0; i < ngrp; i ++) {

    }

    for(int i = 0; i < nkern; i ++) {

    }
// =============================================================================
// STAGE 3 - Data process and output.
// =============================================================================

#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
#endif
    printf("TPBench done\n" DHLINE);
    return 0;
}
