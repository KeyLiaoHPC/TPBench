#ifndef TIMERS_H
#define TIMERS_H

#include <stddef.h>
#include <stdint.h>

enum timer_name {
    TIMER_CLOCK_GETTIME = 0,
    TIMER_MPI_WTIME,
#if defined(__x86_64__) || defined(_M_X64)
    TIMER_TSC_ASYM,
    TIMER_RDTSCP,
#endif
#ifdef __aarch64__
    TIMER_CNTVCT_EL0,
#endif
    TPB_TIMER_COUNT
};

int init_timer_clock_gettime(void);
void tick_clock_gettime(int64_t *ts);
void tock_clock_gettime(int64_t *ts);
void get_time_clock_gettime(int64_t *ts);

int init_timer_mpi_wtime(void);
void tick_mpi_wtime(int64_t *ts);
void tock_mpi_wtime(int64_t *ts);
void get_time_mpi_wtime(int64_t *ts);

#if defined(__x86_64__) || defined(_M_X64)
int init_timer_tsc_asym(void);
void tick_tsc_asym(int64_t *ts);
void tock_tsc_asym(int64_t *ts);
void get_time_tsc_asym(int64_t *ts);

int init_timer_rdtscp(void);
void tick_rdtscp(int64_t *ts);
void tock_rdtscp(int64_t *ts);
void get_time_rdtscp(int64_t *ts);
double tpb_rdtscp_ns_per_tick(void);
#endif

#ifdef __aarch64__
int init_timer_cntvct_el0(void);
void tick_cntvct_el0(int64_t *ts);
void tock_cntvct_el0(int64_t *ts);
void get_time_cntvct_el0(int64_t *ts);
double tpb_cntvct_el0_ns_per_tick(void);
#endif

#endif
