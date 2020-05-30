/**
 * =================================================================================
 * TPBench - A throughputs benchmarking tool for high-performance computing
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
 * =================================================================================
 * @file tpmpi.c
 * @version 0.3
 * @brief Warpping mpi communication operations.
 * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
 * @date 2020-05-29
 */

#define _GNU_SOURCE

#include <sched.h>
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