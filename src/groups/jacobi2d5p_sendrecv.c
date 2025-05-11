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
 * jacobi2d5p_sendrecv.c
 * Description: jacobi2d5p + MPI-sendrecv kernel
 * =================================================================================
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "tptimer.h"
#include "tperror.h"
#include "tpdata.h"
#include "tpmpi.h"
#include "tpio.h"

#ifdef KP_SVE
#include "arm_sve.h"
#endif

#ifdef KP_SME
#include "arm_sme.h"
#endif

#define Nplus2 (N + 2)
#define A(i, j) (A[(i) * Nplus2 + (j)])
#define B(i, j) (B[(i) * Nplus2 + (j)])
#define MAX(a, b) ((a) > (b) ? (a) : (b))
// kernel data
static double* A;
static double* B;


static int report_performance(uint64_t **ns, uint64_t **cy, int nskip, int ntest, int nepoch, int N, int skip_comp);
static void log_step_info(uint64_t **ns, uint64_t **cy, char *kernel_name, int ntest, int nepoch, int N, int skip_comp);


static int get_int_from_env(char *name, int default_value) {
    const char *s = getenv(name);
    if (NULL != s) {
        return atoi(s);
    }
    return default_value;
}

static void fill_random(double *arr, size_t size) {
    struct timespec tv;
    uint64_t sec, nsec;
    clock_gettime(CLOCK_MONOTONIC, &tv);

    sec = tv.tv_sec;
    nsec = tv.tv_nsec;
    nsec = sec * 1e9 + nsec;
    srand(nsec);
    for (size_t i = 0; i < size; i ++){
        arr[i] = (double)rand() / (double) RAND_MAX;
    }
}

static void init_kernel_data(int N) {
    A = (double *) aligned_alloc(64, Nplus2 * Nplus2 * sizeof(double));
    B = (double *) aligned_alloc(64, Nplus2 * Nplus2 * sizeof(double));

    fill_random(A, Nplus2 * Nplus2);
    fill_random(B, Nplus2 * Nplus2);
}

static void free_kernel_data() {
    free(A);
    free(B);
}


static void jacobi2d5p(int N) {
    for (int i = 1; i <= N; i++) {
        for (int j = 1; j <= N; j++) {
            B(i, j) = 1.23 * A(i, j) + 1.56 * (A(i-1,j) + A(i+1,j) + A(i,j-1) + A(i,j+1));
        }
    }
    // asm volatile ("" ::"m"(*B):);
}

static void sendrecv(int N) {
#ifdef USE_MPI
// int MPI_Sendrecv(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
//                 int dest, int sendtag,
//                 void *recvbuf, int recvcount, MPI_Datatype recvtype,
//                 int source, int recvtag,
//                 MPI_Comm comm, MPI_Status *status)
    int up = (tpmpi_info.myrank + tpmpi_info.nrank - 1) % tpmpi_info.nrank;
    int dn = (tpmpi_info.myrank + 1) % tpmpi_info.nrank;
    int tag=0;
    if (tpmpi_info.myrank%2 == 0) { // to avoid deadlock
        MPI_Sendrecv(&B(N, 1), N, MPI_DOUBLE, dn, tag, &A(N+1, 1), N, MPI_DOUBLE, dn, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Sendrecv(&B(1, 1), N, MPI_DOUBLE, up, tag, &A(0  , 1), N, MPI_DOUBLE, up, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    else {
        MPI_Sendrecv(&B(1, 1), N, MPI_DOUBLE, up, tag, &A(0  , 1), N, MPI_DOUBLE, up, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Sendrecv(&B(N, 1), N, MPI_DOUBLE, dn, tag, &A(N+1, 1), N, MPI_DOUBLE, dn, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

#endif
}

int d_jacobi2d5p_sendrecv(int ntest, int nepoch, uint64_t **ns, uint64_t **cy, uint64_t matrix_size) {
    int N = matrix_size;
    int err;

    int skip_comp = get_int_from_env("TPBENCH_SKIP_COMP", 0);
    init_kernel_data(N);

    tpprintf(0, 0, 0, "Matrix size (local domain) N=%d\n", N);
    tpprintf(0, 0, 0, "Working set size: %.1f KB\n", (1.0 * Nplus2) * Nplus2 * sizeof(double) * 2 / 1000);
    tpprintf(0, 0, 0, "Sendrecv %.1f KB messages.\n", 2.0 * N * sizeof(double) / 1000);
    if (skip_comp) {
        tpprintf(0, 0, 0, " COMM only: the computation will be skipped.\n");
    }
    // kernel warm. the #warmups need to be the same among ranks because of the MPI_Bcast
    int nwarmups = MAX(1e8 / N / N, 1);
    for (int i = 0; i < nwarmups; i++) {
        jacobi2d5p(N);
        sendrecv(N);
    }

    __getcy_init;
    __getns_init;

    // kernel start
    for(int i = 0; i < ntest; i ++){
        tpmpi_dbarrier();
        __getns_2d_st(i, 0);
        __getcy_2d_st(i, 0);

        if (!skip_comp) {
            jacobi2d5p(N);
        }

        __getcy_2d_en(i, 0);
        __getns_2d_en(i, 0);

        __getns_2d_st(i, 1);
        __getcy_2d_st(i, 1);

        sendrecv(N);

        __getcy_2d_en(i, 1);
        __getns_2d_en(i, 1);
    }
    // kernel end

    free_kernel_data();

    // overall result
    int nskip = 10, freq=1;
    report_performance(ns, cy, nskip, ntest, nepoch, N, skip_comp);
    char kernel_name[32] = "jacobi2d5p_sendrecv";
    log_step_info(ns, cy, kernel_name, ntest, nepoch, N, skip_comp);
    return 0;
}

static void transpose(uint64_t *out, uint64_t **in, int m, int n) {
    for (int j = 0; j < n; j++) {
        for (int i = 0; i < m; i++) {
            out[j * m + i] = in[i][j];
        }
    }
}

static int report_performance(uint64_t **ns, uint64_t **cy, int nskip, int ntest, int nepoch, int N, int skip_comp) {
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
        calc_rate_quant(&cyt[0 * ntest + nskip], ntest - nskip, 3*N*N, 1, &res);
        offset += sprintf(&buf[offset], "Jacobi(B/c)   %-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f\n",
            res.meantp, res.min,  res.min, res.tp05, res.tp25, res.tp50, res.tp75, res.tp95, res.max, res.max);

        calc_period_quant(&nst[0 * ntest + nskip], ntest - nskip, 1, 1e-6, &res);
        offset += sprintf(&buf[offset], "Jacobi(ms)    %-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f\n",
            res.meantp, res.min,  res.min, res.tp05, res.tp25, res.tp50, res.tp75, res.tp95, res.max, res.max);
    }

    calc_period_quant(&nst[1 * ntest + nskip], ntest - nskip, 1, 1e-3, &res);
    offset += sprintf(&buf[offset], "comm(us)      %-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f%-12.3f\n",
            res.meantp,  res.min, res.tp05, res.tp25, res.tp50, res.tp75, res.tp95, res.max);

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
 * @param skip_comp: whether to skip the computation
 * @return void
 */

static void log_step_info(uint64_t **ns, uint64_t **cy, char *kernel_name, int ntest, int nepoch, int N, int skip_comp) {
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


    sprintf(filename[0], "np%d-%s-%s-Jacobi(ns)-ntest%d-N%d.csv", tpmpi_info.nrank, kernel_name, run_mode, ntest, N);
    if (skip_comp) {
        sprintf(filename[1], "np%d-%s-%s-comm(ns)-ntest%d-N%d.csv", tpmpi_info.nrank, kernel_name, run_mode, ntest, N);
    } else {
        sprintf(filename[1], "np%d-%s-%s-comm(ns)-ntest%d-N%d.csv", tpmpi_info.nrank, kernel_name, run_mode, ntest, N);
    }
    sprintf(filename[2], "np%d-%s-%s-Jacobi(cy)-ntest%d-N%d.csv", tpmpi_info.nrank, kernel_name, run_mode, ntest, N);


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
    tpmpi_writecsv(filepath[1], &nst[ntest], ntest, headers);
    __error_fun(err, "Writing ns csv failed.");

    free(nst);
    free(cyt);
    free(headers);
}

