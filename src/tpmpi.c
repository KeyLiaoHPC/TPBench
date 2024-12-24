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
 * @file tpmpi.c
 * @version 0.3
 * @brief Warpping mpi communication operations.
 * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
 * @date 2024-01-29
 */

#define _GNU_SOURCE

#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "tpmpi.h"
#include "tperror.h"

int
tpmpi_init() {
    int err;
    // init mpi rank info
    #ifdef USE_MPI
        err = MPI_Init(NULL, NULL);
        __error_ne(err, NO_ERROR, MPI_INIT_FAIL);
        MPI_Comm_size(MPI_COMM_WORLD, &tpmpi_info.nrank);
        MPI_Comm_rank(MPI_COMM_WORLD, &tpmpi_info.myrank);
    #else
        tpmpi_info.nrank = 1;
        tpmpi_info.myrank = 0;
    #endif //#ifdef USE_MPI
    tpmpi_info.pcpu = sched_getcpu();
    tpmpi_info.tcpu;

    return NO_ERROR;
}

// process synchronization
void
tpmpi_barrier() {
#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
#endif
    return;
}

// double mpi_barrier
void
tpmpi_dbarrier() {
#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);
#endif
    return;
}

// mpi exit
void 
tpmpi_exit() {
#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
#endif   
    return;
}

// mpi file write
int
tpmpi_writecsv(char *path, uint64_t *data, int ncol, char *header) {
#ifdef USE_MPI
    int err;
    MPI_File fh;
    MPI_Status status;
    err = MPI_File_open(MPI_COMM_WORLD, path, MPI_MODE_CREATE | MPI_MODE_RDWR, MPI_INFO_NULL, &fh);
    if (err != MPI_SUCCESS) {
        printf("无法打开文件\n");
        MPI_Abort(MPI_COMM_WORLD, err);
        return FILE_OPEN_FAIL;
    }

    // 创建缓冲区以存储即将写入的字符串
    char *buffer;
    if (tpmpi_info.myrank == 0) {
        buffer = malloc(2 * (ncol + 1) * 24);
    } else {
        buffer = malloc((ncol + 1) * 24);
    }
    buffer[0] = '\0';  // 初始化字符串

    // 写入表头
    if (tpmpi_info.myrank == 0) {
        strcat(buffer, header);
        strcat(buffer, "\n");
    }
    sprintf(buffer + strlen(buffer), "rank%d,", tpmpi_info.myrank);
    for (int j = 0; j < ncol - 1; j ++) {
        sprintf(buffer + strlen(buffer), "%llu,", data[j]);
    }
    sprintf(buffer + strlen(buffer), "%llu\n", data[ncol-1]);

    // 写入数据
    MPI_File_write_ordered(fh, buffer, strlen(buffer), MPI_CHAR, &status);

    // 清理
    MPI_File_close(&fh);

#else 
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        return FILE_OPEN_FAIL;
    }
    fprintf(fp, "%s\n", header);
    
    fprintf(fp, "rank%d,", tpmpi_info.myrank);
    for (int j = 0; j < ncol - 1; j ++) {
        fprintf(fp, "%llu,", data[j]);
    }
    fprintf(fp, "%llu\n", data[ncol-1]);
    
    fclose(fp);
#endif
    return NO_ERROR;
}
