/**
 * @file cntvct_el0.c
 * @brief: Implementation of CNTVCT_EL0 timer (ticks converted to nanoseconds).
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>

/* Local Function Prototypes */
static int64_t _sf_now_ns(void);
static uint64_t _sf_read_cntfrq(void);
static uint64_t _sf_read_cntvct(void);

static double s_ns_per_tick;

static uint64_t
_sf_read_cntvct(void)
{
    uint64_t v;

    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(v) :: "memory");
    return v;
}

static uint64_t
_sf_read_cntfrq(void)
{
    uint64_t v;

    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(v) :: "memory");
    return v;
}

static int64_t
_sf_now_ns(void)
{
    return (int64_t)((double)_sf_read_cntvct() * s_ns_per_tick);
}

int
init_timer_cntvct_el0(void)
{
    uint64_t frq;

    frq = _sf_read_cntfrq();
    if (frq == 0) {
        s_ns_per_tick = 0.0;
        return -1;
    }
    s_ns_per_tick = 1e9 / (double)frq;
    if (s_ns_per_tick <= 0.0) {
        s_ns_per_tick = 0.0;
        return -1;
    }
    return 0;
}

void
tick_cntvct_el0(int64_t *ts)
{
    if (ts) {
        *ts = _sf_now_ns();
    }
}

void
tock_cntvct_el0(int64_t *ts)
{
    if (ts) {
        *ts = _sf_now_ns() - *ts;
    }
}

void
get_time_cntvct_el0(int64_t *ts)
{
    if (ts) {
        *ts = _sf_now_ns();
    }
}

double
tpb_cntvct_el0_ns_per_tick(void)
{
    return s_ns_per_tick;
}
