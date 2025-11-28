/**
 * @file tsc_asym.c
 * @brief: Implementation of X86 asymmetric timer proposed by Intel Paoloni.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include "timers.h"

int
init_timer_tsc_asym(void)
{
    // No initialization needed for clock_gettime
    return 0;
}

int64_t
tick_tsc_asym(void)
{
    unsigned ch, cl;

    __asm__ volatile (  "CPUID" "\n\t"
                        "RDTSC" "\n\t"
                        "mov %%edx, %0" "\n\t"
                        "mov %%eax, %1" "\n\t"
                        : "=r" (ch), "=r" (cl)
                        :
                        : "%rax", "%rbx", "%rcx", "%rdx");
    return (int64_t)ch * 1000000000LL + (int64_t)cl;
}

int64_t 
tock_tsc_asym(void)
{
    unsigned ch, cl;

    __asm__ volatile (  "RDTSCP" "\n\t"
                        "mov %%edx, %0" "\n\t"
                        "mov %%eax, %1" "\n\t"
                        "CPUID" "\n\t"
                        : "=r" (ch), "=r" (cl)
                        :
                        : "%rax", "%rbx", "%rcx", "%rdx");
    return (int64_t)ch * 1000000000LL + (int64_t)cl;
}

int64_t
get_stamp_tsc_asym(void)
{
    unsigned ch, cl;

    __asm__ volatile (  "CPUID" "\n\t"
                        "RDTSC" "\n\t"
                        "mov %%edx, %0" "\n\t"
                        "mov %%eax, %1" "\n\t"
                        "CPUID" "\n\t"
                        : "=r" (ch), "=r" (cl)
                        :
                        : "%rax", "%rbx", "%rcx", "%rdx");
    return (int64_t)ch * 1000000000LL + (int64_t)cl;

}