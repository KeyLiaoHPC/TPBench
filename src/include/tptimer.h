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

#define __getcy_init        asm volatile("NOP" "\n\t":::);
#define __getcy_grp_init        asm volatile("NOP" "\n\t":::);

#define __getcy_1d_st(rid)  asm volatile(                           \
                                "mov x28, %0"           "\n\t"      \
                                "mrs x29, pmccntr_el0"              \
                                :                                   \
                                : "r" (&cy[(rid)])                  \
                                : "x28", "x29", "x30", "memory");

#define __getcy_2d_st(rid, eid) asm volatile(                           \
                                    "mov x28, %0"           "\n\t"      \
                                    "mrs x29, pmccntr_el0"              \
                                    :                                   \
                                    : "r" (&cy[(rid)][(eid)])           \
                                    : "x28", "x29", "x30", "memory");



#define __getcy_en_t    asm volatile(                           \
                            "mrs x30, pmccntr_el0"  "\n\t"      \
                            "sub x30, x30, x29"     "\n\t"      \
                            "str x30, [x28]"                    \
                            :                                   \
                            :                                   \
                            : "x28", "x29", "x30","memory" );   

#define __getcy_1d_en(rid)  __getcy_en_t
#define __getcy_2d_en(rid, eid)   __getcy_en_t

#define __getcy_grp_st(rid)  asm volatile(                          \
                                "mov x25, %0"           "\n\t"      \
                                "mrs x26, pmccntr_el0"              \
                                :                                   \
                                : "r" (&cy[(rid)][0])               \
                                : "x25", "x26", "x27", "memory");


#define __getcy_grp_en(rid) asm volatile(                           \
                                "mrs x27, pmccntr_el0"  "\n\t"      \
                                "sub x27, x27, x26"     "\n\t"      \
                                "str x27, [x25]"                    \
                                :                                   \
                                :                                   \
                                : "x25", "x26", "x27","memory" );                      
#else

#define __getcy_init        uint64_t hi1, lo1, hi2, lo2;
#define __getcy_grp_init    uint64_t hi1_g, lo1_g, hi2_g, lo2_g;

#define __getcy_st_t        asm volatile(                           \
                                "RDTSCP"        "\n\t"              \
                                "RDTSC"         "\n\t"              \
                                "CPUID"         "\n\t"              \
                                "RDTSCP"        "\n\t"              \
                                "mov %%rdx, %0" "\n\t"              \
                                "mov %%rax, %1" "\n\t"              \
                                :"=r" (hi1), "=r" (lo1)             \
                                :                                   \
                                :"%rax", "%rbx", "%rcx", "%rdx");

#define __getcy_1d_st(rid)  __getcy_st_t

#define __getcy_1d_en(rid)  asm volatile (                          \
                                "RDTSC"         "\n\t"              \
                                "mov %%rdx, %0" "\n\t"              \
                                "mov %%rax, %1" "\n\t"              \
                                "CPUID"         "\n\t"              \
                                :"=r" (hi2), "=r" (lo2)             \
                                :                                   \
                                :"%rax", "%rbx", "%rcx", "%rdx");   \
                            cy[(rid)] = ((hi2 << 32) | lo2) - ((hi1 << 32) | lo1);

#define __getcy_2d_st(rid, eid)     __getcy_st_t

#define __getcy_2d_en(rid, eid)     asm volatile (                          \
                                        "RDTSC"         "\n\t"              \
                                        "mov %%rdx, %0" "\n\t"              \
                                        "mov %%rax, %1" "\n\t"              \
                                        "CPUID"         "\n\t"              \
                                        :"=r" (hi2), "=r" (lo2)             \
                                        :                                   \
                                        :"%rax", "%rbx", "%rcx", "%rdx");   \
                                    cy[(rid)][(eid)] = ((hi2 << 32) | lo2) - ((hi1 << 32) | lo1);

#define __getcy_grp_st(rid) asm volatile(                           \
                                "RDTSCP"        "\n\t"              \
                                "RDTSC"         "\n\t"              \
                                "CPUID"         "\n\t"              \
                                "RDTSCP"        "\n\t"              \
                                "mov %%rdx, %0" "\n\t"              \
                                "mov %%rax, %1" "\n\t"              \
                                :"=r" (hi1_g), "=r" (lo1_g)         \
                                :                                   \
                                :"%rax", "%rbx", "%rcx", "%rdx");

#define __getcy_grp_en(rid) asm volatile (                          \
                                "RDTSC"         "\n\t"              \
                                "mov %%rdx, %0" "\n\t"              \
                                "mov %%rax, %1" "\n\t"              \
                                "CPUID"         "\n\t"              \
                                :"=r" (hi2_g), "=r" (lo2_g)         \
                                :                                   \
                                :"%rax", "%rbx", "%rcx", "%rdx");   \
                            cy[(rid)][0] = ((hi2_g << 32) | lo2_g) - ((hi1_g << 32) | lo1_g);
#endif

#define __getns_init  struct timespec ts1;               \
                      clock_gettime(CLOCK_MONOTONIC, &ts1);

#define __getns(_ts, _ns)   clock_gettime(CLOCK_MONOTONIC, &(_ts));     \
                            (_ns) = (_ts).tv_sec * 1e9, (_ts).tv_nsec;

#define __getns_st_t            clock_gettime(CLOCK_MONOTONIC, &ts1);   
#define __getns_1d_st(rid)      __getns_st_t; \
                                ns[(rid)] = ts1.tv_sec * 1e9 + ts1.tv_nsec;
#define __getns_2d_st(rid, eid) __getns_st_t; \
                                ns[(rid)][(eid)] = ts1.tv_sec * 1e9 + ts1.tv_nsec;

#define __getns_1d_en(rid)      clock_gettime(CLOCK_MONOTONIC, &ts1);   \
                                ns[(rid)] = ts1.tv_sec * 1e9 + ts1.tv_nsec - ns[(rid)];
#define __getns_2d_en(rid, eid) clock_gettime(CLOCK_MONOTONIC, &ts1);   \
                                ns[(rid)][(eid)] = ts1.tv_sec * 1e9 + ts1.tv_nsec - ns[(rid)][(eid)];
