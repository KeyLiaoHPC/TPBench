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
#include "kernels/kernels.h"

/* Local Function Prototypes */

/* Pre-parse integration mode (-P/-F) before kernel registration */
static void parse_integ_mode(int argc, char **argv);

/* Parse run-specific arguments */
static int parse_run(int argc, char **argv);

/* Parse outargs string and set output formatting options */
static int parse_outargs_string(const char *outargs_str);

/* Local Function Implementations */

static void
parse_integ_mode(int argc, char **argv)
{
    /* Default is PLI mode */
    int mode = TPB_INTEG_MODE_PLI;

    /* Scan for -P or -F switches */
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-P") == 0) {
            mode = TPB_INTEG_MODE_PLI;
        } else if (strcmp(argv[i], "-F") == 0) {
            mode = TPB_INTEG_MODE_FLI;
        }
    }

    tpb_driver_set_integ_mode(mode);
}

static int
parse_outargs_string(const char *outargs_str)
{
    char *str_copy = strdup(outargs_str);
    if (str_copy == NULL) {
        return TPBE_MALLOC_FAIL;
    }

    /* Variables to accumulate settings - use defaults if not specified */
    int unit_cast = 0;      /* Default: no unit casting */
    int sigbit_trim = 5;    /* Default: 5 significant bits */

    char *token = strtok(str_copy, ",");
    while (token != NULL) {
        /* Skip leading whitespace */
        while (*token == ' ' || *token == '\t') {
            token++;
        }

        /* Parse key=value pair */
        char *eq = strchr(token, '=');
        if (eq != NULL) {
            *eq = '\0';
            char *key = token;
            char *value = eq + 1;

            /* Remove trailing whitespace from key */
            char *key_end = key + strlen(key) - 1;
            while (key_end > key && (*key_end == ' ' || *key_end == '\t')) {
                *key_end = '\0';
                key_end--;
            }

            /* Skip leading whitespace in value */
            while (*value == ' ' || *value == '\t') {
                value++;
            }

            if (strcmp(key, "unit_cast") == 0) {
                unit_cast = atoi(value);
            } else if (strcmp(key, "sigbit_trim") == 0) {
                sigbit_trim = atoi(value);
            } else {
                tpb_printf(TPBM_PRTN_M_DIRECT, "Unknown outargs key: %s\n", key);
                free(str_copy);
                return TPBE_CLI_FAIL;
            }
        }

        token = strtok(NULL, ",");
    }

    /* Apply settings after parsing all key=value pairs */
    tpb_set_outargs(unit_cast, sigbit_trim);

    free(str_copy);
    return 0;
}

static int
parse_run(int argc, char **argv)
{
    int err = 0;
    int nkern_parsed = 0;

    /* Set default timer */
    err = tpb_argp_set_timer("clock_gettime");
    if (err != 0) {
        return err;
    }

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-P") == 0) {
            /* Already handled in parse_integ_mode */
            continue;

        } else if (strcmp(argv[i], "-F") == 0) {
            /* Already handled in parse_integ_mode */
            continue;

        } else if (strcmp(argv[i], "--kernel") == 0) {
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
            err = tpb_argp_set_timer(argv[i]);
            if (err != 0) {
                tpb_printf(TPBM_PRTN_M_DIRECT, "Invalid timer: %s\n", argv[i]);
                return TPBE_CLI_FAIL;
            }

        } else if (strcmp(argv[i], "--outargs") == 0) {
            if (i + 1 >= argc) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Option %s requires arguments.\n", argv[i]);
                return TPBE_CLI_FAIL;
            }
            i++;
            err = parse_outargs_string(argv[i]);
            if (err != 0) {
                return err;
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

    if (argc <= 1) {
        /* No args */
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "No arguments provided, exit after printing help message.\n");
        tpb_print_help_total();
        return TPBE_EXIT_ON_HELP;
    }

    /* Parse integration mode BEFORE registering kernels */
    parse_integ_mode(argc, argv);

    /* Print integration mode */
    int mode = tpb_driver_get_integ_mode();
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "TPBench kernel integration mode: %s\n",
               mode == TPB_INTEG_MODE_PLI ? "PLI" : "FLI");

    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Initializing TPBench kernels.\n");
    err = tpb_register_kernel();
    __tpbm_exit_on_error(err, "At tpbcli-run.c: tpb_register_kernel");

    /* For FLI mode, register the statically linked kernels */
    if (mode == TPB_INTEG_MODE_FLI) {
        tpb_driver_enable_kernel_reg();
        err = register_triad();
        __tpbm_exit_on_error(err, "At tpbcli-run.c: register_triad");
        err = register_stream();
        __tpbm_exit_on_error(err, "At tpbcli-run.c: register_stream");
        tpb_driver_disable_kernel_reg();
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
