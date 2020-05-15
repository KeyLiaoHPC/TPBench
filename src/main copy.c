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


#include "bench_var.h"
#include "kernels.h"


#ifdef USE_MPI
#include "mpi.h"
#endif

// =============================================================================
// START - argp area
// =============================================================================
const char *argp_program_version = "TPBench-v0.3, maintaining by Key Liao, "
    "Center for HPC, Shanghai Jiao Tong University";
const char *argp_program_bug_address = "keyliaohpc@gmail.com";
static char doc[] = "TPBench - A parallel high-precision throughputs "
    "benchmarking tool for High-Performance Computing.";
static char args_doc[] = "";

static struct argp_option options[] = {
    {"ntests", 'n', "ntests", 0, 
        "Overall number of tests."},
    {"nloops", 'l', "nloops", 0, 
        "Number of loops for tests whose number of inner loop "
        "do not depend on the array size (e.g. instruction benchmark)."},
    {"kib_size", 's', "kib_size",  0, 
         "Memory usage for a single test array, in KiB."},
    {"groups", 'g', "groups", 0, 
        "Name of kernel groups, using comma to split multiple groups. "
        "For group list or customization group please refer to README.md." },
    {"kernels", 'k', "kernels", 0,
        "Name of specific kernel, using comma to split multiple kernels. "
        "For kernel list or customization group please refer to README.md." },
    {"list", 'L', 0, 0,
        "List all kernels." },
    {"consecutive", 'c', 0, 0,
        "Name of specific kernel, using comma to split multiple kernels. "
        "For kernel list or customization group please refer to README.md." },
    { 0 }
};


static error_t
parse_opt (int key, char *arg, struct argp_state *state) {
    TP_Args_t *args = state->input;
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
        case ARGP_KEY_ARG:
            if(state->arg_num < 1){
                args->out_dir = arg;
            }
            else{
                argp_usage(state);
            }
            break;
        case ARGP_KEY_END:
            if(state->arg_num < 1){
                args->out_dir = "./data";
            }
            break;
        default:
          return ARGP_ERR_UNKNOWN;
    }
    return 0;
}
// argp struct itself
static struct argp argp = { options, parse_opt, args_doc, doc };
// =============================================================================
// END - argp area
// =============================================================================

int
main(int argc, char **argv) {
    int nrank, myid;
    int err, cpuid;
    uint64_t nbyte, narr, nkib, ntimes = NTIMES;
    TP_Args_t tp_args;
    char fname[1024], dirname[1024]; // Cycle filename.
    TYPE s, eps[5] = {0,0,0,0,0}; // s

    // nkerns: # of standalone kernels.
    // ngrps: # of group only for consecutive run
    int n_tot_kern, n_tot_grp, nkern, ngrp; 
    int *p_grp, *p_kern;
    double *a, *b, *c, *d; 
    TYPE sa, sb, sc, sd, ss;
    uint64_t cys[NTIMES][8];
    double tps[NTIMES][8], tp_mean[8], tp_max[8], tp_min[8]; // Throughputs
#ifdef USE_MPI
    double partps[NTIMES][8]; // cpu cys, parallel cys
#endif
    uint64_t cy0, cy1, t0, t1;
    FILE    *fp;
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

    // Handling args
    tp_args.list_flag = 0;
    tp_args.kernels = NULL;
    tp_args.groups = NULL;
    argp_parse (&argp, argc, argv, 0, 0, &tp_args);
    // List only.
    if(myid == 0){
        if(tp_args.list_flag){
            // TODO: list_kern() is still hard coded.
            list_kern();
            exit(0);
        }
    }
    else{

    }
// =============================================================================
// STAGE 1 - Initialization
// =============================================================================
    // Prepare kernels according to input arguments.
    // Using kern_list to run kernels one by one.
    p_kern = NULL;
    p_grp = NULL;
    n_tot_kern = sizeof(kern_info) / sizeof(Kern_Info_t);
    n_tot_grp = N_TOT_GRP + 1;
    p_kern = (int *)malloc(n_tot_kern * sizeof(int));
    p_grp = (int *)malloc(n_tot_grp * sizeof(int));
    // Get the array of gid and uid
    err = init_kern(tp_args.kernels, tp_args.groups, 
                    p_kern, p_grp, tp_args.cons_flag, &nkern, &ngrp);
    if(err){
        printf("EXIT: Fail at init_kern.\nTPBench Exit\n");
        return 0;
    }
    if(myid == 0) {
        printf(DHLINE "Running %d groups and %d kernels.\n"
               "Group list: ", ngrp, nkern);
        for(i = 0; i < ngrp; i ++)
            printf("%s, ", g_names[p_grp[i]]);
        printf("\nKernel list: ");
        for(i = 0; i < nkern; i ++)
            printf("%s, ", kern_info[p_kern[i]].name);
        printf("\n");
        fflush();
    }
    // Run kernels according to p_kern and p_grp
    if(tp_args.cons_flag) {
        for(int i = 0; i < ngrp; i ++){
            run_grp(p_grp[i]);
        }
    } 
    for(int i = 0; i < nkern; i ++){
        run_kern(p_kern[i], tp_args.ntest, tp_args.nloop, narr, nparm, tp_args.kib_size,
            wloop)
    }


    struct timespec ts;


    for(int i = 0; i < 8; i ++){
        tp_mean[i] = 0.0;
        tp_max[i] = 0.0;
        tp_min[i] = FLT_MAX;
    }
    nkib = KIB_SIZE;
    nbyte = nkib * 1024;
    narr = nbyte / sizeof(TYPE);
    // MPI init.

    for(int i = 0; i < 8; i ++){
        kerns[i].nbyte = kerns[i].nbyte * nbyte;
    }
    cpuid = sched_getcpu();
    // array init.
#ifdef USE_MPI
    if(myid == 0){
        printf(DHLINE"MPI version, %d processes.\n", nrank);
        printf("Single thread memory: %llu bytes, narr = %d.\n", nbyte, narr);
        printf("NTIMES = %llu\n", ntimes);
    }
#else
    printf(DHLINE"Running on single core, core No. %d\n", cpuid);
    printf("Mem: %llu bytes, narr = %d.\n", nbyte, narr);
#endif

    // Result dir
    sprintf(dirname, "./data/%llu", nkib);
    if(myid == 0){
        printf("Detail results are stored in %s.\n", dirname);
    }
    err = make_dir(dirname);
    if(err){
        printf("Failed when create results directory. [errno: %d]\n", err);
        exit(1);
    }
    for(int i = 0; i < narr; i ++){
        a[i] = A;
        b[i] = B;
        c[i] = C;
        d[i] = D;
    }

// =============================================================================
// STAGE 2 - Timer init and warming up.
// =============================================================================
    t0 = ts.tv_sec * 1e9 + ts.tv_nsec;
    t1 = t0 + 1e9 * TWARM;
    while(t0 < t1){
        s = S;
        init(a, s, narr, &cys[0][0]);
        sum(a, &s, narr, &cys[0][1]);
        s /= (double)narr;
        copy(a, b, narr, &cys[0][2]);
        update(b, s, narr, &cys[0][3]);
        triad(a, b, c, s, narr, &cys[0][4]);
        daxpy(a, b, s, narr, &cys[0][5]);
        striad(a, b, c, d, narr, &cys[0][6]);
        sdaxpy(d, b, c, narr, &cys[0][7]);
        clock_gettime(CLOCK_MONOTONIC, &ts);
        t0 = ts.tv_sec * 1e9 + ts.tv_nsec;
    }
    

// =============================================================================
// STAGE 3 - Benchmarking
// =============================================================================
    if(myid == 0){
        printf("Start benchmarking.\n");
    }
    s = S;
    for(int i = 0; i < narr; i ++){
        a[i] = A;
        b[i] = B;
        c[i] = C;
        d[i] = D;
    }
    for(int i = 0; i < NTIMES; i ++){
        s = S;
#ifdef USE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
#endif 
#ifdef TUNE
        tune_init(a, s, narr, &cys[i][0]);
        tune_sum(a, &s, narr, &cys[i][1]);
        s /= (double)narr;
        tune_copy(a, b, narr, &cys[i][2]);
        tune_update(b, s, narr, &cys[i][3]);
        tune_triad(a, b, c, s, narr, &cys[i][4]);
        tune_daxpy(a, b, s, narr, &cys[i][5]);
        tune_striad(a, b, c, d, narr, &cys[i][6]);
        tune_sdaxpy(d, b, c, narr, &cys[i][7]);

#else
        init(a, s, narr, &cys[i][0]);
        sum(a, &s, narr, &cys[i][1]);
        s /= (double)narr;
        copy(a, b, narr, &cys[i][2]);
        update(b, s, narr, &cys[i][3]);
        triad(a, b, c, s, narr, &cys[i][4]);
        daxpy(a, b, s, narr, &cys[i][5]);
        striad(a, b, c, d, narr, &cys[i][6]);
        sdaxpy(d, b, c, narr, &cys[i][7]);
#endif
        // if(myid == 0){
        //     printf("%d,%f,%f,%f,%f,%f\n", i, a[0],b[0],c[0],d[0],s);
        // }
#ifdef USE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
#endif 
    }

// =============================================================================
// STAGE 4 - Verification and Results
// =============================================================================
    if(myid == 0){
        printf("Benchmark finished, writing results.\n");
    }
    // Output throughputs for each core.
    sprintf(fname, "%s/%d_r%d_c%d_cy.csv", dirname, narr, myid, cpuid); 
    // printf("%d: %s\n", myid, fname);
    fp = fopen(fname, "w");
    for(int i = 0; i < NTIMES; i ++){
        for(int j = 0; j < 7; j ++){
            tps[i][j] = (double)kerns[j].nbyte / (double)cys[i][j];
            fprintf(fp, "%f,", tps[i][j]);
        }
        tps[i][7] = (double)kerns[7].nbyte / (double)cys[i][7];
        fprintf(fp, "%f\n", tps[i][7]);
    }
    fclose(fp);

#ifdef USE_MPI
    MPI_Reduce(tps, partps, 8*NTIMES, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    for(int i = 0; i < 8; i ++){
        for(int n = 0; n < NTIMES; n ++){
            tp_mean[i] += partps[n][i];
            tp_max[i] = MAX(tp_max[i], partps[n][i]);
            tp_min[i] = MIN(tp_min[i], partps[n][i]);
        }
        tp_mean[i] /= NTIMES;
    }
#else
    for(int i = 0; i < 8; i ++){
        for(int n = 0; n < NTIMES; n ++){
            tp_mean[i] += tps[n][i];
            tp_max[i] = MAX(tp_max[i], tps[n][i]);
            tp_min[i] = MIN(tp_min[i], tps[n][i]);
        }
        tp_mean[i] /= NTIMES;
    }
#endif
   
    // Output overall results
    if(myid == 0){
        sprintf(fname, "%s/overall.csv", dirname); 
        fp = fopen(fname, "w");
        fprintf(fp, "kernel,narr,nbyte,mean,max,min\n");
        for(int i = 0; i < 8; i ++){
            fprintf(fp, "%s,%llu,%llu,%.4f,%.4f,%.4f\n", kerns[i].name, narr, nbyte, tp_mean[i], 
                    tp_max[i], tp_min[i]);
        }
        fclose(fp);
    }
    if(myid == 0){
        printf(DHLINE"\n%d Cores Overall Throuphputs:\n", nrank);
        for(int i = 0; i < 8; i ++){
            printf("%s:\t\tMean: %7.4f, Max: %7.4f, Min: %7.4f\n", 
                   kerns[i].name, tp_mean[i], tp_max[i], tp_min[i]);
        }
    }

#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
#endif
    return 0;
}