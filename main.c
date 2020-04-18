/*
 * =================================================================================
 * Arm-BwBench - A bandwidth benchmarking suite for Armv8
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
 * Description: main entry. 
 * Author: Key Liao
 * Modified: Apr. 14th, 2020
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */

# define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sched.h>
#include <time.h>
#include <float.h>
#include <math.h>
#ifdef USE_MPI
#include "mpi.h"
#endif
#include "bench_var.h"

extern int make_dir(char *);

int
main(int argc, char **argv){
    int nrank = 1, myid = 0;
    int err, cpuid;
    uint64_t nbyte, narr, nkib;
    char fname[1024], dirname[1024]; // Cycle filename.
    TYPE s, eps[5] = {0,0,0,0,0}; // s

    double *a, *b, *c, *d; 
    TYPE sa, sb, sc, sd, ss;
    uint64_t cys[NTIMES][8];
    double tps[NTIMES][8], tp_mean[8], tp_max[8], tp_min[8]; // Throughputs
#ifdef USE_MPI
    double partps[NTIMES][8]; // cpu cys, parallel cys
#endif
    uint64_t cy0, cy1, t0, t1;
    FILE    *fp;
    
    Kern_Type kerns[8] = {
        {"Init", 1, 0},
        {"Sum ", 1, 1},
        {"Copy", 2, 0},
        {"Update", 2, 1},
        {"Triad", 3, 2},
        {"Daxpy", 3, 2},
        {"STriad", 4, 2},
        {"SDaxpy", 4, 2}
    };

    struct timespec ts;

// =============================================================================
// STAGE 1 - Initialization
// =============================================================================

    for(int i = 0; i < 8; i ++){
        tp_mean[i] = 0.0;
        tp_max[i] = 0.0;
        tp_min[i] = FLT_MAX;
    }
    nkib = KIB_SIZE;
    nbyte = nkib * 1024;
    narr = nbyte / sizeof(TYPE);
    // MPI init.
#ifdef USE_MPI
    err = MPI_Init(NULL, NULL);
    if (err != MPI_SUCCESS) {
       printf("EXIT: MPI Initialization failed. [errno: %d]\n", err);
       exit(1);
    }
    // if either of these fail there is something really screwed up!
    MPI_Comm_size(MPI_COMM_WORLD, &nrank);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);
    if(myid == 0){
        printf(DHLINE "TPBench v" VER "\n");
    }
#endif
    for(int i = 0; i < 8; i ++){
        kerns[i].nbyte = kerns[i].nbyte * nbyte;
    }
    cpuid = sched_getcpu();
    // array init.
#ifdef USE_MPI
    if(myid == 0){
        printf(DHLINE"MPI version, %d processes.\n", nrank);
        printf("Single thread memory: %llu bytes, narr = %d.\n", nbyte, narr);
    }
#else
    printf(DHLINE"Single-core version.\n");
    printf("Mem: %llu bytes, narr = %d.\n", nbyte, narr);
#endif
#ifdef ALIGN
    err = posix_memalign((void **)&a, ALIGN, nbyte);
    if(err){
        printf("EXIT: Failed on array a allocation. [errno: %d]\n", err);
        exit(1);
    }
    err = posix_memalign((void **)&b, ALIGN, nbyte);
    if(err){
        printf("EXIT: Failed on array b allocation. [errno: %d]\n", err);
        exit(1);
    }
    err = posix_memalign((void **)&c, ALIGN, nbyte);
    if(err){
        printf("EXIT: Failed on array c allocation. [errno: %d]\n", err);
        exit(1);
    }
    err = posix_memalign((void **)&d, ALIGN, nbyte);
    if(err){
        printf("EXIT: Failed on array d allocation. [errno: %d]\n", err);
        exit(1);
    }
#else
    a = (double *)malloc(nbyte);
    if(a == NULL){
        printf("EXIT: Failed on array a allocation. [errno: %d]\n", err);
        exit(1);
    }
    b = (double *)malloc(nbyte);
    if(b == NULL){
        printf("EXIT: Failed on array b allocation. [errno: %d]\n", err);
        exit(1);
    }
    c = (double *)malloc(nbyte);
    if(c == NULL){
        printf("EXIT: Failed on array c allocation. [errno: %d]\n", err);
        exit(1);
    }
    d = (double *)malloc(nbyte);
    if(d == NULL){
        printf("EXIT: Failed on array d allocation. [errno: %d]\n", err);
        exit(1);
    }

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
    sa = A;
    sb = B;
    sc = C;
    sd = D;
    ss = S;
    for(int i = 0 ; i < NTIMES; i ++){
        ss = S;
        sa = ss;
        for(int j = 0; j < narr; j ++){
            ss += sa;
        }
        ss /= (double) narr;
        sa = sb;
        sb = sb * ss;
        sb = sa + ss * sc;
        sa = sa + sb * ss;
        sb = sa + sc * sd;
        sd = sd + sb * sc;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts);
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
        if(myid == 0){
            printf("%d,%f,%f,%f,%f,%f\n", i, a[0],b[0],c[0],d[0],s);
        }
#ifdef USE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
#endif 
    }

// =============================================================================
// STAGE 4 - Verification and Results
// =============================================================================

    // Output throughputs for each core.
    sprintf(fname, "%s/%d_r%d_c%d_cy.csv", dirname, narr, myid, cpuid); 
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
    sprintf(fname, "%s/overall.csv", dirname); 
    fp = fopen(fname, "w");
    fprintf(fp, "kernel,narr,nbyte,mean,max,min\n");
    for(int i = 0; i < 8; i ++){
        fprintf(fp, "%s,%llu,%llu,%.4f,%.4f,%.4f\n", kerns[i].name, narr, nbyte, tp_mean[i], 
                tp_max[i], tp_min[i]);
    }
    fclose(fp);
    if(myid == 0){
        printf(DHLINE"\n%d Cores Overall Throuphputs:\n", nrank);
        for(int i = 0; i < 8; i ++){
            printf("%s:\t\tMean: %7.4f, Max: %7.4f, Min: %7.4f\n", 
                   kerns[i].name, tp_mean[i], tp_max[i], tp_min[i]);
        }
    }

    // Verification
    
    if(myid == 0){
        for(int i = 0; i < narr; i ++){
            eps[0] += abs(a[i] - sa);
            eps[1] += abs(b[i] - sb);
            eps[2] += abs(c[i] - sc);
            eps[3] += abs(d[i] - sd);
        }
        printf(HLINE"Verification:\neps_a = %.7f, eps_b = %.7f, eps_c = %.7f, eps_d = %.7f\n"DHLINE,
               eps[0], eps[1], eps[2], eps[3]);
    }

    free(a);
    free(b);
    free(c);
    free(d);
#ifdef USE_MPI
    MPI_Finalize();
#endif
    return 0;
}
