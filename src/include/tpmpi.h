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
 * @file tpmpi.h
 * @version 0.3
 * @brief Header for tpbench data processor 
 * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
 * @date 2020-05-29
 */

// ====
#ifdef USE_MPI

#include <mpi.h>

#endif // #ifdef USE_MPI

#include <stdint.h>

// ====
#ifndef __PROC_VAR_H

#define __PROC_VAR_H
struct {
    // # of process, proc id, process core, thread core.
    // TODO: malicious naming space for parent process and spawned thread
    int32_t nrank, myrank, pcpu, tcpu;
    // thread info
    int32_t nthread, mythread;
} tpmpi_info;

#endif //#ifndef __PROC_VAR_H

// ====
int tpmpi_init();

void tpmpi_barrier();

void tpmpi_dbarrier();

void tpmpi_exit();