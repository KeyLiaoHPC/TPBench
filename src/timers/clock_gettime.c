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

static int64_t ts0;

int
init_timer_clock_gettime(void)
{
    // No initialization needed for clock_gettime
    return 0;
}

int
tick_clock_gettime(int64_t *ts)
{
    struct timespec tv;

    clock_gettime(CLOCK_MONOTONIC, &tv);
    ts0 = (int64_t)(tv.tv_sec * 1000000000LL + tv.tv_nsec);
    if (ts) *ts = ts0;

    return 0;
}

int
tock_clock_gettime(int64_t *ts)
{
    struct timespec tv;
    register int64_t ts1;

    clock_gettime(CLOCK_MONOTONIC, &tv);
    ts1 = (int64_t)(tv.tv_sec * 1000000000LL + tv.tv_nsec);
    if (ts1 > ts0) *ts = ts1 - ts0;

    return 0;
}

void
get_time_clock_gettime(int64_t *ts)
{
    struct timespec tv;

    clock_gettime(CLOCK_MONOTONIC, &tv);
    *ts = (int64_t)(tv.tv_sec * 1000000000LL + tv.tv_nsec);

    return;
}
 