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

/* Set benchmark mode */
static int set_mode(tpb_args_t *args, const char *arg);

/* Set timer from CLI argument */
static int set_timer(const char *arg, tpb_args_t *args);

/* Parse run-specific arguments */
static int parse_run(int argc, char **argv, tpb_args_t *tpb_args,
                     tpb_k_rthdl_t **kernel_handles_out);

/* Local Function Implementations */

static int
set_mode(tpb_args_t *args, const char *arg)
{
    if (strcmp(arg, "BenchScore") == 0) {
        args->mode = 1;
    } else if (strcmp(arg, "BenchCompute") == 0) {
        args->mode = 2;
    } else if (strcmp(arg, "BenchMemory") == 0) {
        args->mode = 3;
    } else if (strcmp(arg, "BenchNetwork") == 0) {
        args->mode = 4;
    } else if (strcmp(arg, "BenchIO") == 0) {
        args->mode = 5;
    } else {
        return -1;
    }
    return 0;
}

static int
set_timer(const char *arg, tpb_args_t *args)
{
    if (strcmp(arg, "clock_gettime") == 0) {
        sprintf(args->timer.name, "clock_gettime");
        args->timer.unit = TPB_UNIT_NS;
        args->timer.dtype = TPB_INT64_T;
        args->timer.init = init_timer_clock_gettime;
        args->timer.tick = tick_clock_gettime;
        args->timer.tock = tock_clock_gettime;
        args->timer.get_stamp = get_time_clock_gettime;
    } else if (strcmp(arg, "tsc_asym") == 0) {
        sprintf(args->timer.name, "tsc_asym");
        args->timer.unit = TPB_UNIT_CY;
        args->timer.dtype = TPB_INT64_T;
        args->timer.init = init_timer_tsc_asym;
        args->timer.tick = tick_tsc_asym;
        args->timer.tock = tock_tsc_asym;
        args->timer.get_stamp = get_time_tsc_asym;
    } else {
        return -1;
    }
    return 0;
}

static int
parse_run(int argc, char **argv, tpb_args_t *tpb_args,
          tpb_k_rthdl_t **kernel_handles_out)
{
    int err = 0;
    int ret = 0;
    tpb_k_rthdl_t *kernel_handles = NULL;

    if (kernel_handles_out == NULL) {
        return TPBE_CLI_FAIL;
    }
    *kernel_handles_out = NULL;

    /* Set default */
    tpb_args->mode = 0;
    tpb_args->nkern = 0;
    tpb_args->list_only_flag = 0;
    strcpy(tpb_args->data_dir, "./data");
    set_timer("clock_gettime", tpb_args);

    /* New parsing logic for --kernel/--kargs pairs */
    char *kernel_names[TPBM_CLI_K_MAX];
    char **kernel_tokens[TPBM_CLI_K_MAX];
    int kernel_token_counts[TPBM_CLI_K_MAX];
    int kernel_token_caps[TPBM_CLI_K_MAX];
    tpb_kernel_t *kernel_defs[TPBM_CLI_K_MAX];
    char **common_tokens = NULL;
    int common_token_count = 0;
    int common_token_cap = 0;
    int nkern_parsed = 0;
    int current_kernel = -1;  // -1 means no kernel yet (common args)

    for (int i = 0; i < TPBM_CLI_K_MAX; i++) {
        kernel_names[i] = NULL;
        kernel_tokens[i] = NULL;
        kernel_token_counts[i] = 0;
        kernel_token_caps[i] = 0;
        kernel_defs[i] = NULL;
    }

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--kernel") == 0) {
            if (i + 1 >= argc) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Option %s requires arguments.\n", argv[i]);
                ret = TPBE_CLI_FAIL;
                goto cleanup_tokens;
            }
            if (nkern_parsed >= TPBM_CLI_K_MAX) {
                tpb_printf(TPBM_PRTN_M_DIRECT, "Too many kernels specified.\n");
                ret = TPBE_CLI_FAIL;
                goto cleanup_tokens;
            }
            kernel_names[nkern_parsed] = strdup(argv[++i]);
            if (kernel_names[nkern_parsed] == NULL) {
                ret = TPBE_MALLOC_FAIL;
                goto cleanup_tokens;
            }
            current_kernel = nkern_parsed;
            nkern_parsed++;
        } else if (strcmp(argv[i], "--kargs") == 0) {
            if (i + 1 >= argc) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Option %s requires arguments.\n", argv[i]);
                ret = TPBE_CLI_FAIL;
                goto cleanup_tokens;
            }
            i++;  // Move to the argument string
            if (current_kernel >= 0) {
                /* Kernel-specific args */
                err = append_tokens(argv[i],
                                    &kernel_tokens[current_kernel],
                                    &kernel_token_counts[current_kernel],
                                    &kernel_token_caps[current_kernel]);
                if (err != 0) {
                    ret = err;
                    goto cleanup_tokens;
                }
            } else {
                /* Common args (no --kernel yet) */
                err = append_tokens(argv[i],
                                    &common_tokens,
                                    &common_token_count,
                                    &common_token_cap);
                if (err != 0) {
                    ret = err;
                    goto cleanup_tokens;
                }
            }
        } else if (strcmp(argv[i], "--workdir") == 0) {
            if (i + 1 >= argc) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Option %s requires an argument.\n", argv[i]);
                ret = TPBE_CLI_FAIL;
                goto cleanup_tokens;
            }
            if (strlen(argv[i + 1]) > PATH_MAX - 1) {
                tpb_printf(TPBM_PRTN_M_DIRECT, "Data path string too long.\n");
                ret = TPBE_CLI_FAIL;
                goto cleanup_tokens;
            }
            sprintf(tpb_args->data_dir, "%s", argv[++i]);
        } else if (strcmp(argv[i], "--timer") == 0) {
            if (i + 1 >= argc) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Option %s requires arguments.\n", argv[i]);
                ret = TPBE_CLI_FAIL;
                goto cleanup_tokens;
            }
            err = set_timer(argv[++i], tpb_args);
            if (err) {
                tpb_printf(TPBM_PRTN_M_DIRECT, "Invalid timer: %s\n", argv[i]);
                ret = TPBE_CLI_FAIL;
                goto cleanup_tokens;
            }
        } else {
            tpb_printf(TPBM_PRTN_M_DIRECT, "Unknown option %s.\n", argv[i]);
            ret = TPBE_CLI_FAIL;
            goto cleanup_tokens;
        }
    }

    if (nkern_parsed == 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "No kernels specified. Use --kernel option.\n");
        ret = TPBE_CLI_FAIL;
        goto cleanup_tokens;
    }

    tpb_args->nkern = nkern_parsed;

    /* Match kernel names to registered kernels */
    for (int i = 0; i < nkern_parsed; i++) {
        err = tpb_get_kernel(kernel_names[i], &kernel_defs[i]);
        if (err != 0) {
            tpb_printf(TPBM_PRTN_M_DIRECT, "Kernel %s not found.\n", kernel_names[i]);
            ret = TPBE_LIST_NOT_FOUND;
            goto cleanup_tokens;
        }
    }

    /* Allocate storage for per-instance runtime handles */
    kernel_handles = (tpb_k_rthdl_t *)malloc(sizeof(tpb_k_rthdl_t) * tpb_args->nkern);
    if (kernel_handles == NULL) {
        ret = TPBE_MALLOC_FAIL;
        goto cleanup_tokens;
    }
    memset(kernel_handles, 0, sizeof(tpb_k_rthdl_t) * tpb_args->nkern);

    /* Parse, validate and apply kernel-specific arguments for each instance */
    for (int i = 0; i < tpb_args->nkern; i++) {
        tpb_rt_parm_t *rt_parms = NULL;
        int nparms = 0;

        err = tpb_check_kargs(common_tokens, common_token_count,
                              kernel_tokens[i], kernel_token_counts[i],
                              kernel_defs[i], &rt_parms, &nparms);
        if (err != 0) {
            ret = err;
            goto cleanup_tokens;
        }

        memcpy(&kernel_handles[i].kernel, kernel_defs[i], sizeof(tpb_kernel_t));
        kernel_handles[i].argpack.args = rt_parms;
        kernel_handles[i].argpack.n = nparms;
        /* Initialize empty respack - outputs will be added at runtime by kernel */
        kernel_handles[i].respack.n = 0;
        kernel_handles[i].respack.outputs = NULL;
    }

cleanup_tokens:
    for (int i = 0; i < nkern_parsed; i++) {
        free(kernel_names[i]);
        free_tokens(kernel_tokens[i], kernel_token_counts[i]);
    }
    free_tokens(common_tokens, common_token_count);

    if (ret != 0) {
        if (kernel_handles != NULL) {
            for (int i = 0; i < nkern_parsed; i++) {
                free(kernel_handles[i].argpack.args);
            }
            free(kernel_handles);
            kernel_handles = NULL;
        }
        return ret;
    }

    if (kernel_handles_out != NULL) {
        *kernel_handles_out = kernel_handles;
    }

    return 0;
}

/* Public Function Implementations */

int
tpbcli_run(int argc, char **argv)
{
    int err = 0;
    tpb_args_t tpb_args;
    tpb_k_rthdl_t *kernel_handles = NULL;

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

    err = parse_run(argc, argv, &tpb_args, &kernel_handles);
    if (err == TPBE_EXIT_ON_HELP) {
        goto RUN_EXIT;
    } else {
        __tpbm_exit_on_error(err, "At tpbcli-run.c: parse_run");
    }

    if (tpb_args.nkern) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Kernels to run: ");
        for (int i = 0; i < tpb_args.nkern - 1; i++) {
            tpb_printf(TPBM_PRTN_M_DIRECT, "%s, ",
                       kernel_handles[i].kernel.info.name);
        }
        tpb_printf(TPBM_PRTN_M_DIRECT, "%s\n",
                   kernel_handles[tpb_args.nkern - 1].kernel.info.name);
    }

    tpb_driver_set_timer(tpb_args.timer);

    tpb_printf(TPBM_PRTN_M_DIRECT, DHLINE "\n");
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Start driver runner.\n");
    for (int i = 0; i < tpb_args.nkern; i++) {
        tpb_k_rthdl_t *handle = &kernel_handles[i];

        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Kernel %s started.\n",
                   handle->kernel.info.name);
        tpb_printf(TPBM_PRTN_M_DIRECT, "# Test %d/%d  \n",
                   i + 1, tpb_args.nkern);
        err = tpb_run_kernel(handle);
        __tpbm_exit_on_error(err, "At tpbcli-run.c: tpb_run_kernel");

        tpb_clean_output(handle);
    }

    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "TPBench exit.\n");

RUN_EXIT:
    if (kernel_handles != NULL) {
        for (int i = 0; i < tpb_args.nkern; i++) {
            free(kernel_handles[i].argpack.args);
        }
        free(kernel_handles);
    }

    return err;
}
