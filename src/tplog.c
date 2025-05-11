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
 * @file tpgroup.c
 * @version 0.3
 * @brief 
 * @author Qizheng Lyu
 * @date 2024-12-11
 */

#define _GNU_SOURCE

#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "tpgroups.h"
#include "tpdata.h"
#include "tpmpi.h"
#include "tperror.h"
#include "tpio.h"



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
void log_step_info(uint64_t **ns, uint64_t **cy, char *kernel_name, int ntest, int nepoch, int N, int Nr, int skip_comp, int skip_comm) {
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
        __error_fun(err, "Writing ns csv failed.");
        err = tpmpi_writecsv(filepath[2], cyt, ntest, headers);
        __error_fun(err, "Writing ns csv failed.");
    }
    if (skip_comm == 0) {
        tpmpi_writecsv(filepath[1], &nst[ntest], ntest, headers);
        __error_fun(err, "Writing ns csv failed.");
    }
    
    free(nst);
    free(cyt);
    free(headers);
}

