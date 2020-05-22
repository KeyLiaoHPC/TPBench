 /*
 * =================================================================================
 * TPBench - A high-precision throughputs benchmarking tool for scientific computing
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
 * 
 * =================================================================================
 * tptimer.h
 * Description: Timer
 * Author: Key Liao
 * Modified: May. 21st, 2020
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 *
 */
 
#define _GNU_SOURCE
#include <time.h>
#include <unistd.h>
#ifdef __aarch64__

#define CYCLE_ST(ID)    uint64_t cy_t;                          \
                        asm volatile(                           \
                            "mov x28, %0"           "\n\t"      \
                            "mrs x29, pmccntr_el0"              \
                            :                                   \
                            : "r" (&cy[ID])                     \
                            : "x28", "x29", "x30", "memory");

#define CYCLE_EN(ID)    asm volatile(                           \
                            "mrs x30, pmccntr_el0"  "\n\t"      \
                            "sub x30, x30, x29"     "\n\t"      \
                            "str x30, [x28]"                    \
                            :                                   \
                            :                                   \
                            : "x28", "x29", "x30","memory" );   
                    
#else

#define CYCLE_ST(ID)    uint64_t hi1, lo1, hi2, lo2;            \
                        asm volatile(                           \
                            "RDTSCP"        "\n\t"              \
                            "RDTSC"         "\n\t"              \
                            "CPUID"         "\n\t"              \
                            "RDTSCP"        "\n\t"              \
                            "mov %%rdx, %0" "\n\t"              \
                            "mov %%rax, %1" "\n\t"              \
                            :"=r" (hi1), "=r" (lo1)             \
                            :                                   \
                            :"%rax", "%rbx", "%rcx", "%rdx");

#define CYCLE_EN(ID)    asm volatile (                         \
                            "RDTSC"         "\n\t"              \
                            "mov %%rdx, %0" "\n\t"              \
                            "mov %%rax, %1" "\n\t"              \
                            "CPUID"         "\n\t"              \
                            :"=r" (hi2), "=r" (lo2)             \
                            :                                   \
                            :"%rax", "%rbx", "%rcx", "%rdx");   \
                        cy[ID] = ((hi2 << 32) | lo2) - ((hi1 << 32) | lo1);
#endif

#define GETTIME_INIT    struct timespec ts1, ts2;               \
                        clock_gettime(CLOCK_MONOTONIC, &ts1);   \
                        clock_gettime(CLOCK_MONOTONIC, &ts2); 

#define GETTIME_ST(ID)  clock_gettime(CLOCK_MONOTONIC, &ts1);   

#define GETTIME_EN(ID)  clock_gettime(CLOCK_MONOTONIC, &ts2);   \
                        ns[ID] = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
