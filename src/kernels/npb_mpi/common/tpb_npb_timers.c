/*
 * tpb_npb_timers.c
 * NPB timer_start/stop/read using the TPBench wall-clock timer.
 */

#include <string.h>
#include "tpb_npb_timers.h"

#define TPB_NPB_NTIMER 64

static tpb_timer_t s_timer;
static int s_bound;
static int64_t s_start[TPB_NPB_NTIMER];
static double s_elapsed[TPB_NPB_NTIMER];

static int
_sf_slot_ok(int n)
{
    return n >= 0 && n < TPB_NPB_NTIMER;
}

int
tpb_npb_timers_bind(void)
{
    int err;
    int i;

    memset(&s_timer, 0, sizeof(s_timer));
    err = tpb_k_get_timer(&s_timer);
    if (err != TPBE_SUCCESS) {
        s_bound = 0;
        return err;
    }
    if ((s_timer.unit & TPB_UNAME_MASK) != TPB_UNAME_WALLTIME) {
        s_bound = 0;
        return TPBE_KERN_ARG_FAIL;
    }
    if (s_timer.init != NULL) {
        (void)s_timer.init();
    }
    for (i = 0; i < TPB_NPB_NTIMER; i++) {
        s_start[i] = 0;
        s_elapsed[i] = 0.0;
    }
    s_bound = 1;
    return TPBE_SUCCESS;
}

void
tpb_npb_timer_clear(int n)
{
    if (_sf_slot_ok(n)) {
        s_elapsed[n] = 0.0;
        s_start[n] = 0;
    }
}

void
tpb_npb_timer_start(int n)
{
    if (!s_bound || !_sf_slot_ok(n)) {
        return;
    }
    (void)tpb_timer_tick(&s_timer, &s_start[n]);
}

void
tpb_npb_timer_stop(int n)
{
    int64_t elapsed;
    double sec;

    if (!s_bound || !_sf_slot_ok(n)) {
        return;
    }
    elapsed = s_start[n];
    if (tpb_timer_tock(&s_timer, &elapsed) != TPBE_SUCCESS) {
        return;
    }
    if (tpb_timer_elapsed_seconds(&s_timer, elapsed, &sec) !=
        TPBE_SUCCESS) {
        return;
    }
    s_elapsed[n] += sec;
}

double
tpb_npb_timer_read(int n)
{
    if (!_sf_slot_ok(n)) {
        return 0.0;
    }
    return s_elapsed[n];
}

int
tpb_npb_check_timer_flag(void)
{
    /* First draft: extra NPB SECTION timers stay off. */
    return 0;
}

void
timer_clear(int n)
{
    tpb_npb_timer_clear(n);
}

void
timer_start(int n)
{
    tpb_npb_timer_start(n);
}

void
timer_stop(int n)
{
    tpb_npb_timer_stop(n);
}

double
timer_read(int n)
{
    return tpb_npb_timer_read(n);
}

int
check_timer_flag(void)
{
    return tpb_npb_check_timer_flag();
}
