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
 * tpio.c
 * Description: some accessory functions. 
 * Author: Key Liao
 * Modified: May. 9th, 2024
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <errno.h>
#include <inttypes.h>
#include "tpb-io.h"
#include "tpb-stat.h"
#include "tpb-core.h"

int
tpb_mkdir(char *path) {


    int err;
    const size_t len = strlen(path);
    char _path[PATH_MAX];
    char *p; 

    errno = 0;

    // Copy string so its mutable
    if(len > sizeof(_path)-1) {
        errno = ENAMETOOLONG;
        return -1; 
    }   
    strcpy(_path, path);

    // Iterate the string
    for(p = _path + 1; *p; p++) {
        if (*p == '/') {
            // Temporarily truncate 
            *p = '\0';

            if(mkdir(_path, S_IRWXU) != 0) {
                if(errno != EEXIST)
                    return -1; 
            }

            *p = '/';
        }
    }   

    if(mkdir(_path, S_IRWXU) != 0){
        if(errno != EEXIST)
            return -1; 
    }   
    return 0;
}


int
tpb_writecsv(char *path, int64_t **data, int nrow, int ncol, char *header) {
#ifdef TPM_NO_RAW_DATA
    return 0;
#else
    int err, i, j;
    FILE *fp;    

    fp = fopen(path, "w");
    if(fp == NULL) {
        return FILE_OPEN_FAIL;
    }
    if (header != NULL && strlen(header) > 0) {
        fprintf(fp, "%s\n", header);
    }

    // data[col][row], for kernel benchmark
    for(i = 0; i < nrow; i ++) {
        for(j = 0; j < ncol - 1; j ++) {
            fprintf(fp, "%"PRId64",", data[j][i]);
        }
        fprintf(fp, "%"PRId64"\n", data[ncol-1][i]);
    }
    fflush(fp);
    fclose(fp);

    return 0;
#endif
}

// tpbench printf wrapper. 
void
tpb_printf(int err, int ts_flag, int tag_flag, char *fmt, ...) {
    int err_type = 0;
    time_t t = time(0);
    struct tm* lt = localtime(&t);
    char tag[5], err_msg[32];
    
    // print splitter directly.
    if(strcmp(fmt, HLINE) == 0 || strcmp(fmt, DHLINE) == 0) {
        // only allow rank 0 to print
        if(tpmpi_info.myrank == 0) printf("\n%s", fmt);
    }
    err_type = tpb_get_err_exit_flag(err);
    if(err_type == 0) {
        sprintf(tag, "NOTE");
    } else if(err_type == 1) {
        sprintf(tag, "WARN");
    } else if(err_type == 2) {
        sprintf(tag, "FAIL");
    } else {
        sprintf(tag, "UNKN");
    }
    printf("\n%04d-%02d-%02d %02d:%02d:%02d [%s] ", 
            lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, 
            lt->tm_hour, lt->tm_min, lt->tm_sec, tag);

    // transport va_arg list to vprintf
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void
tpb_print_help_total(void)
{
    printf("\n");
    printf("Usage: tpbench <action> <options>\n");
    printf("Action and available options: \n");
    printf("    run:  Run one or more benchmark kernels.\n");
    printf("        -n, --ntest <# of test>        Overall number of tests.\n");
    printf("        -s, --nkib <kib_size>           Memory usage for a single test array, in KiB.\n");
    printf("        -k, --kernel <kernel_list>      Kernel list. (e.g. -k d_init,d_sum)\n");
    printf("        -L, --list                     List all group and kernels then exit.\n");
    printf("        -d, --data_dir <PATH>           Optional. Data directory.\n");
    printf("        -m, --mode <MODE>               Run mode: Supported modes: Manual (Default), BenchScore,\n");
    printf("                                  BenchCompute, BenchMemory, BenchNetwork, BenchIO.\n");
    printf("        -t, --timer <timer_name>        Timer name: Supported timers: clock_gettime (default), tsc_asym.\n");
    printf("        -h, --help                      Print this help message and exit.\n");
    printf("    help: Print this help message and exit.\n");
    printf("\n");
}

void
tpb_list(){
    tpb_printf(0, 1, 1, "Listing supported kernel and groups.\n");
    tpb_printf(0, 0, 0, HLINE);
    tpb_printf(0, 0, 0, "Kernel          Routine         NOTE\n");
    tpb_printf(0, 0, 0, HLINE);
    for(int i = 0 ; i < nkrout; i ++) {
        tpb_printf(0, 0, 0, "%-12s    %-12s    %s\n", 
                   kern_info[i].kname, kern_info[i].rname, kern_info[i].note);
    }
    tpb_printf(0, 0, 0, DHLINE);
}

static void transpose(uint64_t *out, uint64_t **in, int m, int n) {
    for (int j = 0; j < n; j++) {
        for (int i = 0; i < m; i++) {
            out[j * m + i] = in[i][j];
        }
    }
}

int report_performance(uint64_t **ns, uint64_t **cy, uint64_t total_wall_time, int nskip, int ntest, int nepoch, int N, int Nr, int skip_comp, int skip_comm) {
    __ovl_t res;

    uint64_t* nst = (uint64_t *) malloc(sizeof(uint64_t) * ntest * (nepoch + 1));
    uint64_t* cyt = (uint64_t *) malloc(sizeof(uint64_t) * ntest * (nepoch + 1));
    transpose(nst, ns, ntest, nepoch);
    transpose(cyt, cy, ntest, nepoch);

    const int BUFLEN = 10240;
    char buf[10240];
    int offset = 0;
    offset += sprintf(&buf[offset], HLINE);
    offset += sprintf(&buf[offset], "rank %3d ", tpmpi_info.myrank);
    offset += sprintf(&buf[offset], OVL_QUANT_HEADER_EXT);

    if (!skip_comp) {
        calc_rate_quant(&nst[0 * ntest + nskip], ntest - nskip, 2.0*N*N*N, 1, &res);
        offset += sprintf(&buf[offset], "GEMM(GFLOPS)   %-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f\n",
            res.meantp, res.min, res.tp05, res.tp25, res.tp50, res.tp75, res.tp95, res.max);

        calc_period_quant(&nst[0 * ntest + nskip], ntest - nskip, 1, 1e-6, &res);
        offset += sprintf(&buf[offset], "GEMM(ms)       %-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f\n",
            res.meantp, res.min, res.tp05, res.tp25, res.tp50, res.tp75, res.tp95, res.max);
    }

    if (!skip_comm){
        calc_period_quant(&nst[1 * ntest + nskip], ntest - nskip, 1, 1e-3, &res);
        offset += sprintf(&buf[offset], "comm(us)       %-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f\n",
                res.meantp, res.min, res.tp05, res.tp25, res.tp50, res.tp75, res.tp95, res.max);
    }
    // gather output to rank 0 to avoid message interleaving among ranks
    char *gather_buf;
    if (tpmpi_info.myrank == 0) {
        gather_buf = (char *) malloc(BUFLEN * tpmpi_info.nrank);
    }
#ifdef USE_MPI
    MPI_Gather(buf, BUFLEN, MPI_CHAR, gather_buf, BUFLEN, MPI_CHAR, 0, MPI_COMM_WORLD);
#endif
    if (tpmpi_info.myrank == 0) {
        for (int i = 0; i < tpmpi_info.nrank; i++) {
                printf(gather_buf + i * BUFLEN);
        }
        double total_time = (double) total_wall_time * 1e-3;
        printf("\nTotal Wall Time(us): %-12.3f", total_time);
        free(gather_buf);
    }
    free(nst);
    free(cyt);
    return 0;
}


/***
 * Log every step's performance data into a csv file
 * the csv file will be named as "np${rank_size}_kernelname_ntest_N{N}.csv"
 * the csv headers are "rank0" ~ "rank${rank_size-1}"
 * each row is the performance data of a step
 * @param ns: the time data of each step
 * @param cy: the cycle data of each step
 * @param kernel_name: the name of the kernel
 * @param ntest: the number of steps
 * @param nepoch: the number of epochs
 * @param N: the matrix size
 * @param Nr: the number of rows to allreduce
 * @param skip_comp: whether to skip the computation
 * @return void
 */
int log_step_info(uint64_t **ns, uint64_t **cy, char *kernel_name, int ntest, int nepoch, int N, int Nr, int skip_comp, int skip_comm) {
    int err = 0;
    const int BUFLEN = (ntest + 1) * 20;
    char *headers = malloc(BUFLEN);
    char filename[4][100];
    char filedir[16] = "./result/log/";
    char filepath[4][120];
    uint64_t* nst = (uint64_t *) malloc(sizeof(uint64_t) * ntest * (nepoch + 1));
    uint64_t* cyt = (uint64_t *) malloc(sizeof(uint64_t) * ntest * (nepoch + 1));

    char *tpbench_run_mode = getenv("TPBENCH_RUN_MODE");
    char run_mode[24];
    if (tpbench_run_mode != NULL) {
        strcpy(run_mode, tpbench_run_mode);
    } else {
        if(skip_comp) {
            strcpy(run_mode, "commonly");
        } else {   
            strcpy(run_mode, "compcomm");
        } 
    }

    sprintf(filename[0], "np%d-%s-%s-GEMM(ns)-ntest%d-N%d-Nr%d.csv", tpmpi_info.nrank, kernel_name, run_mode, ntest, N, Nr);
    sprintf(filename[1], "np%d-%s-%s-comm(ns)-ntest%d-N%d-Nr%d.csv", tpmpi_info.nrank, kernel_name, run_mode, ntest, N, Nr);
    sprintf(filename[2], "np%d-%s-%s-GEMM(cy)-ntest%d-N%d-Nr%d.csv", tpmpi_info.nrank, kernel_name, run_mode, ntest, N, Nr);

    strcpy(filepath[0], filedir);
    strcpy(filepath[1], filedir);
    strcpy(filepath[2], filedir);


    strcat(filepath[0], filename[0]);
    strcat(filepath[1], filename[1]);
    strcat(filepath[2], filename[2]);

    tpb_mkdir(filedir);

    sprintf(headers , "rank, step0");
    #ifdef USE_MPI
    for (int i = 1; i < ntest; i++) {
        sprintf(headers + strlen(headers), ", step%d", i);
    }
    #endif

    transpose(nst, ns, ntest, nepoch);
    transpose(cyt, cy, ntest, nepoch);

    if (skip_comp == 0){
        err = tpmpi_writecsv(filepath[0], nst, ntest, headers);
        if (err) return err;
        err = tpmpi_writecsv(filepath[2], cyt, ntest, headers);
        if (err) return err;
    }
    if (skip_comm == 0) {
        err = tpmpi_writecsv(filepath[1], &nst[ntest], ntest, headers);
        if (err) return err;
    }
    
    free(nst);
    free(cyt);
    free(headers);

    return 0;
}