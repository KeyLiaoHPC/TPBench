/**
 * @file clock_gettime.c
 * @brief: Implementation of clock_gettime timer.
 */

#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <time.h>
#include <stdint.h>
#include <unistd.h>

int
init_timer_clock_gettime(void)
{
    // No initialization needed for clock_gettime
    return 0;
}

void
tick_clock_gettime(int64_t *ts)
{
    struct timespec tv;

    clock_gettime(CLOCK_MONOTONIC, &tv);
    if (ts) *ts = (int64_t)(tv.tv_sec * 1000000000LL + tv.tv_nsec);
}

void
tock_clock_gettime(int64_t *ts)
{
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    if (ts) *ts = (int64_t)(tv.tv_sec * 1000000000LL + tv.tv_nsec) - *ts;
}

void
get_time_clock_gettime(int64_t *ts)
{
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    if (ts) *ts = (int64_t)(tv.tv_sec * 1000000000LL + tv.tv_nsec);
}
 