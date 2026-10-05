/*
 * tpb_npb_timers.c
 * NPB timer_start/stop/read using the TPBench wall-clock timer.
 */

#include <stdlib.h>
#include <string.h>
#include "tpb_npb_timers.h"

#define TPB_NPB_NTIMER     64
#define TPB_NPB_SAMP_INIT  1024
#define TPB_NPB_SAMP_DEF   1048576

static tpb_timer_t s_timer;
static int s_bound;
static int64_t s_start[TPB_NPB_NTIMER];
static double s_elapsed[TPB_NPB_NTIMER];

#if NPB_TIMER_FLAG
static double *s_samp[TPB_NPB_NTIMER];
static size_t s_nsamp[TPB_NPB_NTIMER];
static size_t s_cap[TPB_NPB_NTIMER];
static uint64_t s_ncall[TPB_NPB_NTIMER];
static uint64_t s_ndrop[TPB_NPB_NTIMER];
static size_t s_sample_max = TPB_NPB_SAMP_DEF;
#endif

static int _sf_slot_ok(int n);
#if NPB_TIMER_FLAG
static void _sf_append_sample(int n, double sec);
static void _sf_reset_slot_samples(int n);
#endif

static int
_sf_slot_ok(int n)
{
    return n >= 0 && n < TPB_NPB_NTIMER;
}

#if NPB_TIMER_FLAG
static void
_sf_reset_slot_samples(int n)
{
    s_nsamp[n] = 0;
    s_ncall[n] = 0;
    s_ndrop[n] = 0;
}

static void
_sf_append_sample(int n, double sec)
{
    double *p;
    size_t ncap;

    s_ncall[n]++;
    if (s_nsamp[n] >= s_sample_max) {
        s_ndrop[n]++;
        return;
    }
    if (s_nsamp[n] >= s_cap[n]) {
        ncap = (s_cap[n] == 0) ? TPB_NPB_SAMP_INIT : s_cap[n] * 2;
        if (ncap > s_sample_max) {
            ncap = s_sample_max;
        }
        if (ncap <= s_cap[n]) {
            s_ndrop[n]++;
            return;
        }
        p = realloc(s_samp[n], ncap * sizeof(double));
        if (p == NULL) {
            s_ndrop[n]++;
            return;
        }
        s_samp[n] = p;
        s_cap[n] = ncap;
    }
    s_samp[n][s_nsamp[n]++] = sec;
}
#endif

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
        if (s_timer.init() != 0) {
            s_bound = 0;
            return TPBE_KERN_ARG_FAIL;
        }
    }
    for (i = 0; i < TPB_NPB_NTIMER; i++) {
        s_start[i] = 0;
        s_elapsed[i] = 0.0;
#if NPB_TIMER_FLAG
        _sf_reset_slot_samples(i);
#endif
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
#if NPB_TIMER_FLAG
        _sf_reset_slot_samples(n);
#endif
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
#if NPB_TIMER_FLAG
    _sf_append_sample(n, sec);
#endif
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
    return NPB_TIMER_FLAG;
}

void
tpb_npb_timers_set_sample_cap(size_t n)
{
#if NPB_TIMER_FLAG
    if (n == 0) {
        n = 1;
    }
    s_sample_max = n;
#else
    (void)n;
#endif
}

void
tpb_npb_timers_free(void)
{
#if NPB_TIMER_FLAG
    int i;

    for (i = 0; i < TPB_NPB_NTIMER; i++) {
        free(s_samp[i]);
        s_samp[i] = NULL;
        s_cap[i] = 0;
        _sf_reset_slot_samples(i);
    }
#endif
}

size_t
tpb_npb_timer_nsample(int n)
{
#if NPB_TIMER_FLAG
    if (!_sf_slot_ok(n)) {
        return 0;
    }
    return s_nsamp[n];
#else
    (void)n;
    return 0;
#endif
}

const double *
tpb_npb_timer_samples(int n)
{
#if NPB_TIMER_FLAG
    if (!_sf_slot_ok(n)) {
        return NULL;
    }
    return s_samp[n];
#else
    (void)n;
    return NULL;
#endif
}

uint64_t
tpb_npb_timer_ncall(int n)
{
#if NPB_TIMER_FLAG
    if (!_sf_slot_ok(n)) {
        return 0;
    }
    return s_ncall[n];
#else
    (void)n;
    return 0;
#endif
}

uint64_t
tpb_npb_timer_ndropped(int n)
{
#if NPB_TIMER_FLAG
    if (!_sf_slot_ok(n)) {
        return 0;
    }
    return s_ndrop[n];
#else
    (void)n;
    return 0;
#endif
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
