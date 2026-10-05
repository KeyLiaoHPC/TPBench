/*
 * test_fortran_abi.c
 * Test pack A13: typed timer/arg ABI used by Fortran kernels.
 */

#include <stdio.h>
#include <string.h>
#include "include/tpb-public.h"
#include "timers/timers.h"

static int
test_elapsed_ns(void)
{
    tpb_timer_t timer;
    double sec;

    memset(&timer, 0, sizeof(timer));
    timer.unit = TPB_UNIT_NS;
    if (tpb_timer_elapsed_seconds(&timer, 2000000000LL, &sec) !=
        TPBE_SUCCESS) {
        return 1;
    }
    if (sec < 1.999 || sec > 2.001) {
        return 1;
    }
    return 0;
}

static int
test_elapsed_scaled_units(void)
{
    tpb_timer_t timer;
    double sec;

    memset(&timer, 0, sizeof(timer));
    timer.unit = TPB_UNIT_US;
    if (tpb_timer_elapsed_seconds(&timer, 1500000LL, &sec) !=
        TPBE_SUCCESS) {
        return 1;
    }
    if (sec < 1.499 || sec > 1.501) {
        return 1;
    }
    timer.unit = TPB_UNIT_MS;
    if (tpb_timer_elapsed_seconds(&timer, 2500LL, &sec) != TPBE_SUCCESS) {
        return 1;
    }
    if (sec < 2.499 || sec > 2.501) {
        return 1;
    }
    timer.unit = TPB_UNIT_SS;
    if (tpb_timer_elapsed_seconds(&timer, 7, &sec) != TPBE_SUCCESS) {
        return 1;
    }
    if (sec < 6.999 || sec > 7.001) {
        return 1;
    }
    return 0;
}

static int
test_elapsed_null_and_bad_unit(void)
{
    tpb_timer_t timer;
    double sec;
    int err;

    memset(&timer, 0, sizeof(timer));
    timer.unit = TPB_UNIT_NS;
    err = tpb_timer_elapsed_seconds(NULL, 1, &sec);
    if (TPBE_CAUSE(err) != TPBE_NULLPTR_ARG) {
        return 1;
    }
    err = tpb_timer_elapsed_seconds(&timer, 1, NULL);
    if (TPBE_CAUSE(err) != TPBE_NULLPTR_ARG) {
        return 1;
    }
    timer.unit = TPB_UNIT_SEC;
    err = tpb_timer_elapsed_seconds(&timer, 1, &sec);
    if (TPBE_CAUSE(err) != TPBE_KERN_ARG_FAIL) {
        return 1;
    }
    return 0;
}

static int
test_typed_get_null(void)
{
    int err;
    int64_t i64;
    char c;
    double d;

    err = tpb_k_get_arg_i64("class", NULL);
    if (TPBE_CAUSE(err) != TPBE_NULLPTR_ARG) {
        return 1;
    }
    err = tpb_k_get_arg_char("class", NULL);
    if (TPBE_CAUSE(err) != TPBE_NULLPTR_ARG) {
        return 1;
    }
    err = tpb_k_get_arg_double("class", NULL);
    if (TPBE_CAUSE(err) != TPBE_NULLPTR_ARG) {
        return 1;
    }
    (void)i64;
    (void)c;
    (void)d;
    return 0;
}

typedef struct {
    const char *id;
    const char *name;
    int (*fn)(void);
} test_case_t;

static int
test_set_timer_arch(void)
{
    int err;

#if defined(__x86_64__) || defined(_M_X64)
    err = tpb_argp_set_timer("rdtscp");
    if (err != TPBE_SUCCESS) {
        return 1;
    }
    err = tpb_argp_set_timer("cntvct_el0");
    if (TPBE_CAUSE(err) != TPBE_CLI_FAIL) {
        return 1;
    }
#elif defined(__aarch64__)
    err = tpb_argp_set_timer("cntvct_el0");
    if (err != TPBE_SUCCESS) {
        return 1;
    }
    err = tpb_argp_set_timer("rdtscp");
    if (TPBE_CAUSE(err) != TPBE_CLI_FAIL) {
        return 1;
    }
#else
    (void)err;
#endif
    return 0;
}

static int
test_mpi_wtime_no_mpi(void)
{
    int err;

    err = tpb_argp_set_timer("mpi_wtime");
    if (err != TPBE_SUCCESS) {
        return 1;
    }
    if (init_timer_mpi_wtime() == 0) {
        return 1;
    }
    return 0;
}

static int
test_tick_ns_positive(void)
{
    tpb_timer_t timer;
    int64_t elapsed;
    double sec;
    volatile int x;
    int i;

    memset(&timer, 0, sizeof(timer));
    timer.unit = TPB_UNIT_NS;
    elapsed = 0;
#if defined(__x86_64__) || defined(_M_X64)
    if (init_timer_rdtscp() != 0) {
        return 1;
    }
    tick_rdtscp(&elapsed);
    x = 0;
    for (i = 0; i < 20000000; i++) {
        x += i;
    }
    (void)x;
    tock_rdtscp(&elapsed);
#elif defined(__aarch64__)
    if (init_timer_cntvct_el0() != 0) {
        return 1;
    }
    tick_cntvct_el0(&elapsed);
    x = 0;
    for (i = 0; i < 20000000; i++) {
        x += i;
    }
    (void)x;
    tock_cntvct_el0(&elapsed);
#else
    return 0;
#endif
    if (tpb_timer_elapsed_seconds(&timer, elapsed, &sec) != TPBE_SUCCESS) {
        return 1;
    }
    if (sec <= 0.0 || sec >= 10.0) {
        return 1;
    }
    return 0;
}

static test_case_t cases[] = {
    { "A13.1", "elapsed_ns", test_elapsed_ns },
    { "A13.2", "elapsed_scaled", test_elapsed_scaled_units },
    { "A13.3", "elapsed_reject", test_elapsed_null_and_bad_unit },
    { "A13.4", "get_arg_null", test_typed_get_null },
    { "A13.5", "set_timer_arch", test_set_timer_arch },
    { "A13.6", "mpi_wtime_no_mpi", test_mpi_wtime_no_mpi },
    { "A13.7", "tick_ns_positive", test_tick_ns_positive },
};

int
main(int argc, char **argv)
{
    const char *filter = (argc > 1) ? argv[1] : NULL;
    int failures = 0;
    int i;
    int n;

    n = (int)(sizeof(cases) / sizeof(cases[0]));
    for (i = 0; i < n; i++) {
        if (filter != NULL && strcmp(cases[i].id, filter) != 0) {
            continue;
        }
        if (cases[i].fn() != 0) {
            fprintf(stderr, "FAIL A13 %s\n", cases[i].name);
            failures++;
        }
    }
    return (failures > 0) ? 1 : 0;
}
