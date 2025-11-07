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
 * @file tppthread.c
 * @version 0.1
 * @brief Implementation of pthread-based parallel backend
 * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
 * @date 2024-11-07
 */

#define _GNU_SOURCE

#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/syscall.h>
#include <errno.h>
#include <pthread.h>
#include "tppthread.h"
#include "tperror.h"
#include "tptimer.h"

// Include the error definitions from tperror.h
#ifndef PTHREAD_BARRIER_INIT_FAIL
#define PTHREAD_BARRIER_INIT_FAIL  101
#endif
#ifndef PTHREAD_CREATE_FAIL
#define PTHREAD_CREATE_FAIL        102
#endif
#ifndef PTHREAD_JOIN_FAIL
#define PTHREAD_JOIN_FAIL          103
#endif
#ifndef PTHREAD_AFFINITY_FAIL
#define PTHREAD_AFFINITY_FAIL      104
#endif
#ifndef INVALID_THREAD_ID
#define INVALID_THREAD_ID          105
#endif

// Global thread info
struct tppthread_info_t tppthread_info = {0};

// Thread-local storage for thread ID
static __thread int thread_local_id = -1;

// Internal function for thread initialization
static void* thread_init_func(void *arg);

int
tppthread_init(int nthreads) {
    int err;
    
    if (nthreads <= 0) {
        nthreads = 1;
    }
    
    // Initialize thread info structure
    tppthread_info.nthread = nthreads;
    tppthread_info.mythread = 0;  // Main thread
    tppthread_info.pcpu = sched_getcpu();
    
#ifdef USE_NUMA
    if (numa_available() != -1) {
        tppthread_info.numa_node = numa_node_of_cpu(tppthread_info.pcpu);
    } else {
        tppthread_info.numa_node = 0;
    }
#else
    tppthread_info.numa_node = 0;
#endif
    
    // Initialize barrier
    err = pthread_barrier_init(&tppthread_info.barrier, NULL, nthreads);
    if (err != 0) {
        return PTHREAD_BARRIER_INIT_FAIL;
    }
    
    // Allocate thread handles
    tppthread_info.threads = (pthread_t*)malloc(nthreads * sizeof(pthread_t));
    if (tppthread_info.threads == NULL) {
        pthread_barrier_destroy(&tppthread_info.barrier);
        return MALLOC_FAIL;
    }
    
    // Allocate thread data array
    tppthread_info.thread_data = (void**)malloc(nthreads * sizeof(void*));
    if (tppthread_info.thread_data == NULL) {
        free(tppthread_info.threads);
        pthread_barrier_destroy(&tppthread_info.barrier);
        return MALLOC_FAIL;
    }
    
    // Initialize sync timestamp
    tppthread_info.sync_timestamp = 0;
    
    // Set affinity for main thread
    tppthread_set_affinity(0, tppthread_info.pcpu);
    
    return NO_ERROR;
}

void
tppthread_barrier() {
    pthread_barrier_wait(&tppthread_info.barrier);
}

void
tppthread_dbarrier() {
    // Double barrier for better synchronization
    pthread_barrier_wait(&tppthread_info.barrier);
    pthread_barrier_wait(&tppthread_info.barrier);
}

void
tppthread_exit() {
    if (tppthread_info.threads) {
        free(tppthread_info.threads);
        tppthread_info.threads = NULL;
    }
    
    if (tppthread_info.thread_data) {
        free(tppthread_info.thread_data);
        tppthread_info.thread_data = NULL;
    }
    
    pthread_barrier_destroy(&tppthread_info.barrier);
    
    memset(&tppthread_info, 0, sizeof(tppthread_info));
}

int
tppthread_create_threads(thread_func_t func, void **args) {
    int err;
    
    for (int i = 1; i < tppthread_info.nthread; i++) {
        tppthread_info.thread_data[i] = args ? args[i] : NULL;
        
        err = pthread_create(&tppthread_info.threads[i], NULL, func, 
                           (void*)(intptr_t)i);
        if (err != 0) {
            // Clean up already created threads
            for (int j = 1; j < i; j++) {
                pthread_cancel(tppthread_info.threads[j]);
                pthread_join(tppthread_info.threads[j], NULL);
            }
            return PTHREAD_CREATE_FAIL;
        }
    }
    
    // Main thread executes with ID 0
    thread_local_id = 0;
    tppthread_info.mythread = 0;
    if (args && args[0]) {
        func(args[0]);
    } else {
        func((void*)(intptr_t)0);
    }
    
    return NO_ERROR;
}

int
tppthread_join_threads() {
    int err = NO_ERROR;
    
    for (int i = 1; i < tppthread_info.nthread; i++) {
        void *retval;
        int join_err = pthread_join(tppthread_info.threads[i], &retval);
        if (join_err != 0) {
            err = PTHREAD_JOIN_FAIL;
        }
    }
    
    return err;
}

void*
tppthread_numa_alloc(size_t size, int numa_node) {
    void *ptr = NULL;
    
#ifdef USE_NUMA
    if (numa_available() != -1) {
        ptr = numa_alloc_onnode(size, numa_node);
    } else {
        ptr = aligned_alloc(64, size);
    }
#else
    ptr = aligned_alloc(64, size);
#endif
    
    if (ptr == NULL) {
        return NULL;
    }
    
    // Touch the memory to ensure it's allocated
    memset(ptr, 0, size);
    
    return ptr;
}

void
tppthread_numa_free(void *ptr, size_t size) {
#ifdef USE_NUMA
    if (numa_available() != -1) {
        numa_free(ptr, size);
    } else {
        free(ptr);
    }
#else
    free(ptr);
#endif
}

void
tppthread_sync_timestamp() {
    // Master thread (ID 0) sets the target timestamp
    if (tppthread_info.mythread == 0) {
        // Get current timestamp and add a small delay
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        tppthread_info.sync_timestamp = ts.tv_sec * 1000000000ULL + ts.tv_nsec + 1000000ULL; // 1ms delay
    }
    
    // Synchronize all threads
    tppthread_barrier();
    
    // All threads wait until the target timestamp
    struct timespec current_ts;
    uint64_t current_ns;
    
    do {
        clock_gettime(CLOCK_MONOTONIC, &current_ts);
        current_ns = current_ts.tv_sec * 1000000000ULL + current_ts.tv_nsec;
    } while (current_ns < tppthread_info.sync_timestamp);
    
    // Final synchronization
    tppthread_barrier();
}

uint64_t
tppthread_get_timestamp() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

int
tppthread_set_affinity(int thread_id, int cpu) {
    cpu_set_t cpuset;
    pthread_t current_thread;
    
    if (thread_id == 0) {
        current_thread = pthread_self();
    } else if (thread_id < tppthread_info.nthread) {
        current_thread = tppthread_info.threads[thread_id];
    } else {
        return INVALID_THREAD_ID;
    }
    
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    
    int err = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
    if (err != 0) {
        return PTHREAD_AFFINITY_FAIL;
    }
    
    return NO_ERROR;
}

int
tppthread_get_numa_node(int cpu) {
#ifdef USE_NUMA
    if (numa_available() != -1) {
        return numa_node_of_cpu(cpu);
    }
#endif
    return 0;  // Default to NUMA node 0
}

// Internal function to set thread local ID
void
tppthread_set_thread_id(int thread_id) {
    thread_local_id = thread_id;
    tppthread_info.mythread = thread_id;
    tppthread_info.pcpu = sched_getcpu();
    
#ifdef USE_NUMA
    if (numa_available() != -1) {
        tppthread_info.numa_node = numa_node_of_cpu(tppthread_info.pcpu);
    } else {
        tppthread_info.numa_node = 0;
    }
#else
    tppthread_info.numa_node = 0;
#endif
}

int
tppthread_get_thread_id() {
    return thread_local_id;
}