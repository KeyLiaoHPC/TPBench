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
 * @file tpparallel.c
 * @version 0.1
 * @brief Implementation of unified parallel interface
 * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
 * @date 2024-11-07
 */

#define _GNU_SOURCE

#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "tpparallel.h"
#include "tpmpi.h"
#include "tppthread.h"
#include "tperror.h"
#include "tptimer.h"

// Include the error definitions from tperror.h
#ifndef MPI_NOT_AVAILABLE
#define MPI_NOT_AVAILABLE          151
#endif
#ifndef INVALID_BACKEND
#define INVALID_BACKEND            152
#endif
#ifndef OPERATION_NOT_SUPPORTED
#define OPERATION_NOT_SUPPORTED    153
#endif

// Global parallel info
struct tpparallel_info_t tpparallel_info = {0};

int
tpparallel_init(int nthreads, parallel_backend_t backend) {
    int err = NO_ERROR;
    
    tpparallel_info.backend = backend;
    
    switch (backend) {
        case PARALLEL_MPI:
#ifdef USE_MPI
            // Initialize MPI
            err = tpmpi_init();
            if (err != NO_ERROR) {
                return err;
            }
            
            // Copy MPI info to parallel info
            tpparallel_info.nrank = tpmpi_info.nrank;
            tpparallel_info.myrank = tpmpi_info.myrank;
            tpparallel_info.pcpu = tpmpi_info.pcpu;
            tpparallel_info.nthread = tpmpi_info.nthread;
            tpparallel_info.mythread = tpmpi_info.mythread;
            tpparallel_info.numa_node = 0;  // Default, can be enhanced
            
            // Set thread handles to NULL for MPI backend
            tpparallel_info.thread_handles = NULL;
#else
            return MPI_NOT_AVAILABLE;
#endif
            break;
            
        case PARALLEL_PTHREAD:
            // Initialize pthread backend
            err = tppthread_init(nthreads);
            if (err != NO_ERROR) {
                return err;
            }
            
            // Copy pthread info to parallel info
            tpparallel_info.nrank = tppthread_info.nthread;
            tpparallel_info.myrank = tppthread_info.mythread;
            tpparallel_info.pcpu = tppthread_info.pcpu;
            tpparallel_info.nthread = tppthread_info.nthread;
            tpparallel_info.mythread = tppthread_info.mythread;
            tpparallel_info.numa_node = tppthread_info.numa_node;
            tpparallel_info.thread_handles = tppthread_info.threads;
            break;
            
        default:
            return INVALID_BACKEND;
    }
    
    return NO_ERROR;
}

void
tpparallel_barrier() {
    switch (tpparallel_info.backend) {
        case PARALLEL_MPI:
#ifdef USE_MPI
            tpmpi_barrier();
#endif
            break;
        case PARALLEL_PTHREAD:
            tppthread_barrier();
            break;
    }
}

void
tpparallel_dbarrier() {
    switch (tpparallel_info.backend) {
        case PARALLEL_MPI:
#ifdef USE_MPI
            tpmpi_dbarrier();
#endif
            break;
        case PARALLEL_PTHREAD:
            tppthread_dbarrier();
            break;
    }
}

void
tpparallel_exit() {
    switch (tpparallel_info.backend) {
        case PARALLEL_MPI:
#ifdef USE_MPI
            tpmpi_exit();
#endif
            break;
        case PARALLEL_PTHREAD:
            tppthread_exit();
            break;
    }
    
    memset(&tpparallel_info, 0, sizeof(tpparallel_info));
}

parallel_backend_t
tpparallel_get_backend() {
    return tpparallel_info.backend;
}

int
tpparallel_is_mpi_available() {
#ifdef USE_MPI
    return 1;
#else
    return 0;
#endif
}

int
tpparallel_is_pthread_available() {
    return 1;  // pthread is always available on POSIX systems
}

void*
tpparallel_numa_alloc(size_t size, int numa_node) {
    switch (tpparallel_info.backend) {
        case PARALLEL_MPI:
#ifdef USE_MPI
            // For MPI, each process has its own memory space
            // Use standard aligned allocation for now
            // TODO: Enhance with NUMA awareness for MPI backend
            {
                void *ptr = aligned_alloc(64, size);
                if (ptr != NULL) {
                    memset(ptr, 0, size);
                }
                return ptr;
            }
#endif
            break;
        case PARALLEL_PTHREAD:
            return tppthread_numa_alloc(size, numa_node);
            break;
    }
    
    return NULL;
}

void
tpparallel_numa_free(void *ptr, size_t size) {
    switch (tpparallel_info.backend) {
        case PARALLEL_MPI:
#ifdef USE_MPI
            free(ptr);
#endif
            break;
        case PARALLEL_PTHREAD:
            tppthread_numa_free(ptr, size);
            break;
    }
}

int
tpparallel_get_size() {
    return tpparallel_info.nrank;
}

int
tpparallel_get_rank() {
    return tpparallel_info.myrank;
}

int
tpparallel_get_numa_node() {
    return tpparallel_info.numa_node;
}

int
tpparallel_set_affinity(int thread_id, int cpu) {
    if (tpparallel_info.backend == PARALLEL_PTHREAD) {
        return tppthread_set_affinity(thread_id, cpu);
    } else {
        // Thread affinity setting only makes sense for pthread backend
        return OPERATION_NOT_SUPPORTED;
    }
}

int
tpparallel_create_threads(parallel_func_t func, void **args) {
    if (tpparallel_info.backend == PARALLEL_PTHREAD) {
        return tppthread_create_threads(func, args);
    } else {
        // Thread creation only makes sense for pthread backend
        return OPERATION_NOT_SUPPORTED;
    }
}

int
tpparallel_join_threads() {
    if (tpparallel_info.backend == PARALLEL_PTHREAD) {
        return tppthread_join_threads();
    } else {
        // Thread joining only makes sense for pthread backend
        return OPERATION_NOT_SUPPORTED;
    }
}

void
tpparallel_sync_timestamp() {
    switch (tpparallel_info.backend) {
        case PARALLEL_MPI:
#ifdef USE_MPI
            // For MPI, use double barrier for synchronization
            tpmpi_dbarrier();
#endif
            break;
        case PARALLEL_PTHREAD:
            tppthread_sync_timestamp();
            break;
    }
}