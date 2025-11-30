/**
 * @file tsc_asym.c
 * @brief: Implementation of X86 asymmetric timer proposed by Intel Paoloni.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>

static int64_t ts0;

int
init_timer_tsc_asym(void)
{
    return 0;
}

int
tick_tsc_asym(int64_t *ts)
{
    unsigned ch, cl;

    __asm__ volatile (  "CPUID" "\n\t"
                        "RDTSC" "\n\t"
                        "mov %%edx, %0" "\n\t"
                        "mov %%eax, %1" "\n\t"
                        : "=r" (ch), "=r" (cl)
                        :
                        : "%rax", "%rbx", "%rcx", "%rdx");
    ts0 = (int64_t)(((uint64_t)ch << 32) | (uint64_t)cl);
    if (ts) *ts = ts0;

    return 0;
}

int
tock_tsc_asym(int64_t *ts)
{
    unsigned ch, cl;

    *ts = 0;
    __asm__ volatile (  "RDTSCP" "\n\t"
                        "mov %%edx, %0" "\n\t"
                        "mov %%eax, %1" "\n\t"
                        "CPUID" "\n\t"
                        : "=r" (ch), "=r" (cl)
                        :
                        : "%rax", "%rbx", "%rcx", "%rdx");
    *ts = (int64_t)(((uint64_t)ch << 32) | (uint64_t)cl) - ts0;

    return 0;
}

void
get_time_tsc_asym(int64_t *ts)
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
    *ts = (int64_t)(((uint64_t)ch << 32) | (uint64_t)cl);
}