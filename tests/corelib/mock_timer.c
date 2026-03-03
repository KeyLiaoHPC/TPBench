#include <stdio.h>
#include <time.h>
#include "mock_timer.h"

static int
mock_timer_init(void)
{
    return 0;
}

static void
mock_timer_tick(int64_t *ts)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    *ts = t.tv_sec * 1000000000LL + t.tv_nsec;
}

static void
mock_timer_tock(int64_t *ts)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    *ts = t.tv_sec * 1000000000LL + t.tv_nsec;
}

static void
mock_timer_stamp(int64_t *ts)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    *ts = t.tv_sec * 1000000000LL + t.tv_nsec;
}

tpb_timer_t
mock_get_timer(void)
{
    tpb_timer_t t;
    snprintf(t.name, TPBM_NAME_STR_MAX_LEN, "mock_timer");
    t.unit = TPB_UNIT_NS;
    t.dtype = TPB_INT64_T;
    t.init = mock_timer_init;
    t.tick = mock_timer_tick;
    t.tock = mock_timer_tock;
    t.get_stamp = mock_timer_stamp;
    return t;
}
