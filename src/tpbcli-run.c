/*
 * tpbcli-run.c
 * Run subcommand implementation.
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <linux/limits.h>
#include <string.h>
#include "tpbcli-run.h"
#include "tpb-argp.h"
#include "tpb-driver.h"
#include "tpb-impl.h"
#include "tpb-io.h"
#include "tpb-types.h"

/* Local Function Prototypes */

/* Set timer in driver from CLI argument */
static int set_driver_timer(const char *arg);

/* Parse run-specific arguments */
static int parse_run(int argc, char **argv);

/* Local Function Implementations */

static int
set_driver_timer(const char *arg)
{
    tpb_timer_t timer;

    memset(&timer, 0, sizeof(tpb_timer_t));

    if (strcmp(arg, "clock_gettime") == 0) {
        snprintf(timer.name, TPBM_NAME_STR_MAX_LEN, "clock_gettime");
        timer.unit = TPB_UNIT_NS;
        timer.dtype = TPB_INT64_T;
        timer.init = init_timer_clock_gettime;
        timer.tick = tick_clock_gettime;
        timer.tock = tock_clock_gettime;
        timer.get_stamp = get_time_clock_gettime;
    } else if (strcmp(arg, "tsc_asym") == 0) {
        snprintf(timer.name, TPBM_NAME_STR_MAX_LEN, "tsc_asym");
        timer.unit = TPB_UNIT_CY;
        timer.dtype = TPB_INT64_T;
        timer.init = init_timer_tsc_asym;
        timer.tick = tick_tsc_asym;
        timer.tock = tock_tsc_asym;
        timer.get_stamp = get_time_tsc_asym;
    } else {
        return -1;
    }

    return tpb_driver_set_timer(timer);
}

static int
parse_run(int argc, char **argv)
{
    int err = 0;
    int nkern_parsed = 0;

    /* Set default timer */
    err = set_driver_timer("clock_gettime");
    if (err != 0) {
        return err;
    }

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--kernel") == 0) {
            if (i + 1 >= argc) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Option %s requires arguments.\n", argv[i]);
                return TPBE_CLI_FAIL;
            }
            i++;  /* Move to kernel name */

            /* Add handle for this kernel */
            err = tpb_driver_add_handle(argv[i]);
            if (err != 0) {
                return err;
            }
            nkern_parsed++;

        } else if (strcmp(argv[i], "--kargs") == 0) {
            if (i + 1 >= argc) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Option %s requires arguments.\n", argv[i]);
                return TPBE_CLI_FAIL;
            }
            i++;  /* Move to the argument string */

            /* Parse and set the kargs for current handle */
            err = tpb_argp_set_kargs_tokstr((int)strlen(argv[i]), argv[i], NULL);
            if (err != 0) {
                return err;
            }

        } else if (strcmp(argv[i], "--timer") == 0) {
            if (i + 1 >= argc) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Option %s requires arguments.\n", argv[i]);
                return TPBE_CLI_FAIL;
            }
            i++;
            err = set_driver_timer(argv[i]);
            if (err != 0) {
                tpb_printf(TPBM_PRTN_M_DIRECT, "Invalid timer: %s\n", argv[i]);
                return TPBE_CLI_FAIL;
            }

        } else {
            tpb_printf(TPBM_PRTN_M_DIRECT, "Unknown option %s.\n", argv[i]);
            return TPBE_CLI_FAIL;
        }
    }

    if (nkern_parsed == 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "No kernels specified. Use --kernel option.\n");
        return TPBE_CLI_FAIL;
    }

    return 0;
}

/* Public Function Implementations */

int
tpbcli_run(int argc, char **argv)
{
    int err = 0;

    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Initializing TPBench kernels.\n");
    err = tpb_register_kernel();
    __tpbm_exit_on_error(err, "At tpbcli-run.c: tpb_register_kernel");

    if (argc <= 1) {
        /* No args */
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "No arguments provided, exit after printing help message.\n");
        tpb_print_help_total();
        return TPBE_EXIT_ON_HELP;
    }

    err = parse_run(argc, argv);
    if (err == TPBE_EXIT_ON_HELP) {
        return err;
    }
    __tpbm_exit_on_error(err, "At tpbcli-run.c: parse_run");

    /* Print kernels to run */
    int nhdl = tpb_driver_get_nhdl();
    if (nhdl > 1) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Number of kernels to run: %d\n", nhdl - 1);
    }

    /* Run all handles using driver */
    err = tpb_driver_run_all();
    __tpbm_exit_on_error(err, "At tpbcli-run.c: tpb_driver_run_all");

    return err;
}
