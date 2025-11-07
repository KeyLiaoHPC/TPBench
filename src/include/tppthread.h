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
 * @file tppthread.h
 * @version 0.1
 * @brief Header for pthread-based parallel backend
 * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
 * @date 2024-11-07
 */

#ifndef __TPPTHREAD_H
#define __TPPTHREAD_H

#include <stdint.h>
#include <pthread.h>

#ifdef USE_NUMA
#include <numa.h>
#endif

// Thread information structure
struct tppthread_info_t {
    // Thread identification
    int32_t nthread;          // Total number of threads
    int32_t mythread;         // Current thread ID
    int32_t pcpu;             // Physical CPU this thread is running on
    int32_t numa_node;        // NUMA node this thread belongs to
    
    // Synchronization
    pthread_barrier_t barrier;
    volatile uint64_t sync_timestamp;
    
    // Thread handles
    pthread_t *threads;
    
    // Thread-specific data
    void **thread_data;
};

// Function type for thread work functions
typedef void* (*thread_func_t)(void *arg);

// Global thread info
extern struct tppthread_info_t tppthread_info;

// Core pthread functions
int tppthread_init(int nthreads);
void tppthread_barrier();
void tppthread_dbarrier();
void tppthread_exit();

// Thread creation and management
int tppthread_create_threads(thread_func_t func, void **args);
int tppthread_join_threads();

// NUMA-aware memory allocation
void* tppthread_numa_alloc(size_t size, int numa_node);
void tppthread_numa_free(void *ptr, size_t size);

// Timestamp synchronization
void tppthread_sync_timestamp();
uint64_t tppthread_get_timestamp();

// Thread affinity and NUMA placement
int tppthread_set_affinity(int thread_id, int cpu);
int tppthread_get_numa_node(int cpu);

// Thread ID management
void tppthread_set_thread_id(int thread_id);
int tppthread_get_thread_id();

#endif // __TPPTHREAD_H