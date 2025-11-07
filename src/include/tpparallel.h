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
 * @file tpparallel.h
 * @version 0.1
 * @brief Unified parallel interface supporting both MPI and pthread backends
 * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
 * @date 2024-11-07
 */

#ifndef __TPPARALLEL_H
#define __TPPARALLEL_H

#include <stdint.h>

// Backend type enumeration
typedef enum {
    PARALLEL_MPI,
    PARALLEL_PTHREAD
} parallel_backend_t;

// Unified parallel info structure
struct tpparallel_info_t {
    // Common fields
    int32_t nrank;           // Number of ranks/threads
    int32_t myrank;          // Current rank/thread ID
    int32_t pcpu;            // Physical CPU
    int32_t tcpu;            // Thread CPU (for pthread)
    int32_t numa_node;       // NUMA node
    
    // Backend-specific fields
    parallel_backend_t backend;
    
    // MPI-specific
    int32_t nthread;         // Number of threads (for MPI + OpenMP)
    int32_t mythread;        // Thread ID (for MPI + OpenMP)
    
    // Pthread-specific
    void *thread_handles;    // Thread handles
};

extern struct tpparallel_info_t tpparallel_info;

// Core parallel interface functions
int tpparallel_init(int nthreads, parallel_backend_t backend);
void tpparallel_barrier();
void tpparallel_dbarrier();
void tpparallel_exit();

// Backend selection and querying
parallel_backend_t tpparallel_get_backend();
int tpparallel_is_mpi_available();
int tpparallel_is_pthread_available();

// NUMA-aware memory allocation (unified interface)
void* tpparallel_numa_alloc(size_t size, int numa_node);
void tpparallel_numa_free(void *ptr, size_t size);

// Thread/rank information
int tpparallel_get_size();
int tpparallel_get_rank();
int tpparallel_get_numa_node();

// Thread affinity (pthread backend only)
int tpparallel_set_affinity(int thread_id, int cpu);

// Function type for thread work functions (pthread backend)
typedef void* (*parallel_func_t)(void *arg);

// Thread creation and management (pthread backend only)
int tpparallel_create_threads(parallel_func_t func, void **args);
int tpparallel_join_threads();

// Timestamp synchronization
void tpparallel_sync_timestamp();

#endif // __TPPARALLEL_H