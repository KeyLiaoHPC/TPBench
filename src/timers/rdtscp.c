/**
 * @file rdtscp.c
 * @brief: Implementation of RDTSCP timer (ticks converted to nanoseconds).
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <time.h>
#include <unistd.h>

/* Local Function Prototypes */
static int _sf_calibrate(void);
static int _sf_cpuid_tsc_hz(uint64_t *hz_out);
static int64_t _sf_now_ns(void);
static uint64_t _sf_read_rdtscp(void);

static double s_ns_per_tick;

static uint64_t
_sf_read_rdtscp(void)
{
    unsigned lo;
    unsigned hi;
    unsigned aux;

    __asm__ volatile("rdtscp"
                     : "=a"(lo), "=d"(hi), "=c"(aux)
                     :
                     : "memory");
    return ((uint64_t)hi << 32) | (uint64_t)lo;
}

static int
_sf_cpuid_tsc_hz(uint64_t *hz_out)
{
    unsigned eax;
    unsigned ebx;
    unsigned ecx;
    unsigned edx;

    eax = 0x15;
    ecx = 0;
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(eax), "c"(ecx));
    if (eax != 0 && ebx != 0 && ecx != 0) {
        *hz_out = ((uint64_t)ecx * (uint64_t)ebx) / (uint64_t)eax;
        return 0;
    }

    eax = 0x16;
    ecx = 0;
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(eax), "c"(ecx));
    if (eax != 0) {
        *hz_out = (uint64_t)eax * 1000000ULL;
        return 0;
    }
    return -1;
}

static int
_sf_calibrate(void)
{
    struct timespec t0;
    struct timespec t1;
    uint64_t c0;
    uint64_t c1;
    uint64_t hz;
    double secs;

    if (_sf_cpuid_tsc_hz(&hz) == 0 && hz != 0) {
        s_ns_per_tick = 1e9 / (double)hz;
        return 0;
    }

    clock_gettime(CLOCK_MONOTONIC, &t0);
    c0 = _sf_read_rdtscp();
    usleep(200000);
    c1 = _sf_read_rdtscp();
    clock_gettime(CLOCK_MONOTONIC, &t1);

    secs = (double)(t1.tv_sec - t0.tv_sec)
         + (double)(t1.tv_nsec - t0.tv_nsec) * 1e-9;
    if (secs <= 0.0 || c1 <= c0) {
        return -1;
    }
    hz = (uint64_t)((double)(c1 - c0) / secs);
    if (hz == 0) {
        return -1;
    }
    s_ns_per_tick = 1e9 / (double)hz;
    return 0;
}

static int64_t
_sf_now_ns(void)
{
    return (int64_t)((double)_sf_read_rdtscp() * s_ns_per_tick);
}

int
init_timer_rdtscp(void)
{
    if (_sf_calibrate() != 0 || s_ns_per_tick <= 0.0) {
        s_ns_per_tick = 0.0;
        return -1;
    }
    return 0;
}

void
tick_rdtscp(int64_t *ts)
{
    if (ts) {
        *ts = _sf_now_ns();
    }
}

void
tock_rdtscp(int64_t *ts)
{
    if (ts) {
        *ts = _sf_now_ns() - *ts;
    }
}

void
get_time_rdtscp(int64_t *ts)
{
    if (ts) {
        *ts = _sf_now_ns();
    }
}

double
tpb_rdtscp_ns_per_tick(void)
{
    return s_ns_per_tick;
}
