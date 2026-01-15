/*
 * tpb-cli-parse.c
 * TPBench command line parser.
 */

#include <linux/limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include "tpb-cli.h"
#include "timers/timers.h"
#include "tpb-impl.h"
#include "tpb-driver.h"
#include "tpb-io.h"
#include "tpb-types.h"

/* Local Function Prototypes */

/* Trim whitespace from both ends of a string */
static char *trim_whitespace(char *str);

/* Free token array */
static void free_tokens(char **tokens, int count);

/* Append tokens from comma-separated argument string */
static int append_tokens(const char *argstr, char ***tokens, int *count, int *cap);

/* Find parameter index by name */
static int find_parm_index(tpb_rt_parm_t *rt_parms, int nparms, const char *name);

/* Apply kernel argument tokens to runtime parameters */
static int apply_karg_tokens(char **tokens, int ntokens, tpb_rt_parm_t *rt_parms,
                             int nparms, const char *kernel_name, int warn_unknown);

/* Build runtime parameters from kernel and common definitions */
static int build_rt_parms(tpb_kernel_t *kernel, tpb_kernel_t *kernel_common,
                          tpb_rt_parm_t **rt_parms_out, int *nparms_out);

/* Check if value is within range */
static int check_arg_range(tpb_rt_parm_t *parm, tpb_parm_value_t *value);

/* Check if value is in list */
static int check_arg_list(tpb_rt_parm_t *parm, tpb_parm_value_t *value);

/* Set benchmark mode */
static int set_mode(tpb_args_t *args, const char *arg);

/* Set timer from CLI argument */
static int set_timer(const char *arg, tpb_args_t *args);

/* Local Function Implementations */

static char *
trim_whitespace(char *str)
{
    char *end;

    if (str == NULL) {
        return NULL;
    }

    while (*str && isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == '\0') {
        return str;
    }

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    if (isspace((unsigned char)*end)) {
        *end = '\0';
    }

    return str;
}

static void
free_tokens(char **tokens, int count)
{
    if (tokens == NULL) {
        return;
    }

    for (int i = 0; i < count; i++) {
        free(tokens[i]);
    }
    free(tokens);
}

static int
append_tokens(const char *argstr, char ***tokens, int *count, int *cap)
{
    char buf[TPBM_CLI_STR_MAX_LEN];
    char *saveptr;
    char *token;

    if (argstr == NULL || tokens == NULL || count == NULL || cap == NULL) {
        return TPBE_CLI_FAIL;
    }

    if (argstr[0] == '\0') {
        return TPBE_CLI_FAIL;
    }

    snprintf(buf, sizeof(buf), "%s", argstr);
    token = strtok_r(buf, ",", &saveptr);
    while (token != NULL) {
        char *trim = trim_whitespace(token);
        if (trim == NULL || *trim == '\0') {
            return TPBE_CLI_FAIL;
        }
        if (*count >= *cap) {
            int new_cap = (*cap == 0) ? 8 : (*cap * 2);
            char **new_tokens = (char **)realloc(*tokens,
                                                 sizeof(char *) * new_cap);
            if (new_tokens == NULL) {
                return TPBE_MALLOC_FAIL;
            }
            *tokens = new_tokens;
            *cap = new_cap;
        }
        (*tokens)[*count] = strdup(trim);
        if ((*tokens)[*count] == NULL) {
            return TPBE_MALLOC_FAIL;
        }
        (*count)++;
        token = strtok_r(NULL, ",", &saveptr);
    }

    return 0;
}

static int
find_parm_index(tpb_rt_parm_t *rt_parms, int nparms, const char *name)
{
    if (rt_parms == NULL || name == NULL) {
        return -1;
    }

    for (int i = 0; i < nparms; i++) {
        if (strcmp(rt_parms[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
}

static int
apply_karg_tokens(char **tokens, int ntokens, tpb_rt_parm_t *rt_parms,
                  int nparms, const char *kernel_name, int warn_unknown)
{
    int err;

    if (ntokens <= 0) {
        return 0;
    }

    if (tokens == NULL || rt_parms == NULL || nparms <= 0) {
        return warn_unknown ? TPBE_KERN_ARG_FAIL : 0;
    }

    for (int i = 0; i < ntokens; i++) {
        char token_buf[TPBM_CLI_STR_MAX_LEN];
        char *eq;
        char *key;
        char *value;
        int parm_idx;
        tpb_parm_value_t parsed_value;

        snprintf(token_buf, sizeof(token_buf), "%s", tokens[i]);
        eq = strchr(token_buf, '=');
        if (eq == NULL) {
            tpb_printf(TPBM_PRTN_M_DIRECT,
                       "Invalid kernel arg \"%s\". Expected key=value.\n",
                       tokens[i]);
            return TPBE_KERN_ARG_FAIL;
        }

        *eq = '\0';
        key = trim_whitespace(token_buf);
        value = trim_whitespace(eq + 1);

        if (key == NULL || value == NULL || *key == '\0' || *value == '\0') {
            tpb_printf(TPBM_PRTN_M_DIRECT,
                       "Invalid kernel arg. Empty key or value detected.\n");
            return TPBE_KERN_ARG_FAIL;
        }

        parm_idx = find_parm_index(rt_parms, nparms, key);
        if (parm_idx < 0) {
            if (warn_unknown) {
                tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                           "Warning: Unsupported argument '%s' for kernel '%s'. Ignoring.\n",
                           key, (kernel_name != NULL) ? kernel_name : "unknown");
            }
            continue;
        }

        uint32_t type_code = rt_parms[parm_idx].dtype & TPB_PARM_TYPE_MASK;
        /* Check float/double types first (type codes overlap with int range numerically) */
        if (type_code == TPB_FLOAT_T || type_code == TPB_DOUBLE_T || type_code == TPB_LONG_DOUBLE_T) {
            parsed_value.f64 = strtod(value, NULL);
        } else if (type_code == TPB_INT_T || type_code == TPB_INT8_T || type_code == TPB_INT16_T ||
                   type_code == TPB_INT32_T || type_code == TPB_INT64_T) {
            parsed_value.i64 = strtoll(value, NULL, 10);
        } else if (type_code == TPB_UINT8_T || type_code == TPB_UINT16_T ||
                   type_code == TPB_UINT32_T || type_code == TPB_UINT64_T) {
            parsed_value.u64 = strtoull(value, NULL, 10);
        } else if (type_code == TPB_CHAR_T) {
            parsed_value.c = value[0];
            if (parsed_value.c == '\0') {
                return TPBE_MALLOC_FAIL;
            }
        } else {
            return TPBE_KERN_ARG_FAIL;
        }

        uint32_t check_mode = rt_parms[parm_idx].dtype & TPB_PARM_CHECK_MASK;
        if (check_mode == TPB_PARM_RANGE) {
            err = check_arg_range(&rt_parms[parm_idx], &parsed_value);
            if (err != 0) {
                return err;
            }
        } else if (check_mode == TPB_PARM_LIST) {
            err = check_arg_list(&rt_parms[parm_idx], &parsed_value);
            if (err != 0) {
                return err;
            }
        }

        rt_parms[parm_idx].value = parsed_value;
    }

    return 0;
}

static int
build_rt_parms(tpb_kernel_t *kernel, tpb_kernel_t *kernel_common,
               tpb_rt_parm_t **rt_parms_out, int *nparms_out)
{
    tpb_rt_parm_t *rt_parms;
    int max_parms;
    int nparms = 0;

    if (kernel == NULL || rt_parms_out == NULL || nparms_out == NULL) {
        return TPBE_KERN_ARG_FAIL;
    }

    max_parms = kernel->info.nparms + (kernel_common ? kernel_common->info.nparms : 0);
    if (max_parms == 0) {
        *rt_parms_out = NULL;
        *nparms_out = 0;
        return 0;
    }

    rt_parms = (tpb_rt_parm_t *)malloc(sizeof(tpb_rt_parm_t) * max_parms);
    if (rt_parms == NULL) {
        return TPBE_MALLOC_FAIL;
    }

    for (int i = 0; i < kernel->info.nparms; i++) {
        memcpy(&rt_parms[nparms], &kernel->info.parms[i], sizeof(tpb_rt_parm_t));
        nparms++;
    }

    if (kernel_common != NULL) {
        for (int i = 0; i < kernel_common->info.nparms; i++) {
            if (find_parm_index(rt_parms, nparms, kernel_common->info.parms[i].name) >= 0) {
                continue;
            }
            memcpy(&rt_parms[nparms], &kernel_common->info.parms[i], sizeof(tpb_rt_parm_t));
            nparms++;
        }
    }

    for (int i = 0; i < nparms; i++) {
        rt_parms[i].value = rt_parms[i].default_value;
    }

    *rt_parms_out = rt_parms;
    *nparms_out = nparms;
    return 0;
}

/* Check if value is within range [plims[0], plims[1]] */
static int
check_arg_range(tpb_rt_parm_t *parm, tpb_parm_value_t *value)
{
    if (parm == NULL || value == NULL || parm->plims == NULL || parm->nlims != 2) {
        return TPBE_KERN_ARG_FAIL;
    }

    uint32_t type_code = parm->dtype & TPB_PARM_TYPE_MASK;

    /* Check float/double types first (type codes overlap with uint range numerically) */
    if (type_code == TPB_DOUBLE_T || type_code == TPB_LONG_DOUBLE_T) {
        if (value->f64 < parm->plims[0].f64 || value->f64 > parm->plims[1].f64) {
            tpb_printf(TPBM_PRTN_M_DIRECT,
                       "Parameter '%s' value %lf out of range [%lf, %lf]\n",
                       parm->name, value->f64, parm->plims[0].f64, parm->plims[1].f64);
            return TPBE_KERN_ARG_FAIL;
        }
    } else if (type_code == TPB_FLOAT_T) {
        if (value->f32 < parm->plims[0].f32 || value->f32 > parm->plims[1].f32) {
            tpb_printf(TPBM_PRTN_M_DIRECT,
                       "Parameter '%s' value %f out of range [%f, %f]\n",
                       parm->name, value->f32, parm->plims[0].f32, parm->plims[1].f32);
            return TPBE_KERN_ARG_FAIL;
        }
    } else if (type_code == TPB_INT_T || type_code == TPB_INT8_T || type_code == TPB_INT16_T ||
               type_code == TPB_INT32_T || type_code == TPB_INT64_T) {
        if (value->i64 < parm->plims[0].i64 || value->i64 > parm->plims[1].i64) {
            tpb_printf(TPBM_PRTN_M_DIRECT,
                       "Parameter '%s' value %" PRId64 " out of range [%" PRId64 ", %" PRId64 "]\n",
                       parm->name, value->i64, parm->plims[0].i64, parm->plims[1].i64);
            return TPBE_KERN_ARG_FAIL;
        }
    } else if (type_code == TPB_UINT8_T || type_code == TPB_UINT16_T ||
               type_code == TPB_UINT32_T || type_code == TPB_UINT64_T) {
        if (value->u64 < parm->plims[0].u64 || value->u64 > parm->plims[1].u64) {
            tpb_printf(TPBM_PRTN_M_DIRECT,
                       "Parameter '%s' value %" PRIu64 " out of range [%" PRIu64 ", %" PRIu64 "]\n",
                       parm->name, value->u64, parm->plims[0].u64, parm->plims[1].u64);
            return TPBE_KERN_ARG_FAIL;
        }
    } else if (type_code == TPB_CHAR_T) {
        if (value->c < parm->plims[0].c || value->c > parm->plims[1].c) {
            tpb_printf(TPBM_PRTN_M_DIRECT,
                       "Parameter '%s' value '%c' out of range ['%c', '%c']\n",
                       parm->name, value->c, parm->plims[0].c, parm->plims[1].c);
            return TPBE_KERN_ARG_FAIL;
        }
    }

    return 0;
}

/* Check if value is in the list plims[0..nlims-1] */
static int
check_arg_list(tpb_rt_parm_t *parm, tpb_parm_value_t *value)
{
    if (parm == NULL || value == NULL || parm->plims == NULL || parm->nlims == 0) {
        return TPBE_KERN_ARG_FAIL;
    }

    uint32_t type_code = parm->dtype & TPB_PARM_TYPE_MASK;

    /* Check float/double types first (type codes overlap with uint range numerically) */
    if (type_code == TPB_DOUBLE_T || type_code == TPB_LONG_DOUBLE_T) {
        for (int i = 0; i < parm->nlims; i++) {
            if (fabs(value->f64 - parm->plims[i].f64) < 1e-15) {
                return 0;
            }
        }
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Parameter '%s' value %lf is not in list\n", parm->name, value->f64);
    } else if (type_code == TPB_FLOAT_T) {
        for (int i = 0; i < parm->nlims; i++) {
            if (fabs(value->f32 - parm->plims[i].f32) < 1e-7) {
                return 0;
            }
        }
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Parameter '%s' value %f is not in list\n", parm->name, value->f32);
    } else if (type_code == TPB_INT_T || type_code == TPB_INT8_T || type_code == TPB_INT16_T ||
               type_code == TPB_INT32_T || type_code == TPB_INT64_T) {
        for (int i = 0; i < parm->nlims; i++) {
            if (value->i64 == parm->plims[i].i64) {
                return 0;
            }
        }
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Parameter '%s' value %" PRId64 " is not in list\n", parm->name, value->i64);
    } else if (type_code == TPB_UINT8_T || type_code == TPB_UINT16_T ||
               type_code == TPB_UINT32_T || type_code == TPB_UINT64_T) {
        for (int i = 0; i < parm->nlims; i++) {
            if (value->u64 == parm->plims[i].u64) {
                return 0;
            }
        }
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Parameter '%s' value %" PRIu64 " is not in list\n", parm->name, value->u64);
    } else if (type_code == TPB_CHAR_T) {
        for (int i = 0; i < parm->nlims; i++) {
            if (value->c == parm->plims[i].c) {
                return 0;
            }
        }
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "Parameter '%s' value %c is not in list\n", parm->name, value->c);
    }

    return TPBE_KERN_ARG_FAIL;
}

/* Public Function Implementations */

int
tpb_check_kargs(char **common_tokens, int ncommon,
                char **kernel_tokens, int nkernel,
                tpb_kernel_t *kernel,
                tpb_rt_parm_t **rt_parms_out, int *nparms_out)
{
    tpb_kernel_t *kernel_common = NULL;
    tpb_rt_parm_t *rt_parms;
    int err;

    if (kernel == NULL) {
        return TPBE_KERN_ARG_FAIL;
    }

    err = tpb_get_kernel("tpb_common", &kernel_common);
    if (err != 0) {
        kernel_common = NULL;
    }

    err = build_rt_parms(kernel, kernel_common, &rt_parms, nparms_out);
    if (err != 0) {
        return err;
    }

    err = apply_karg_tokens(common_tokens, ncommon,
                            rt_parms, *nparms_out,
                            kernel->info.name, 0);
    if (err != 0) {
        free(rt_parms);
        return err;
    }

    err = apply_karg_tokens(kernel_tokens, nkernel,
                            rt_parms, *nparms_out,
                            kernel->info.name, 1);
    if (err != 0) {
        free(rt_parms);
        return err;
    }

    *rt_parms_out = rt_parms;
    return 0;
}

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

int
tpb_parse_args(int argc, char **argv, tpb_args_t *tpb_args,
               tpb_k_rthdl_t **kernel_handles_out)
{
    int err = 0;
    int ret = 0;
    tpb_k_rthdl_t *kernel_handles = NULL;

    if (kernel_handles_out == NULL) {
        return TPBE_CLI_FAIL;
    }
    *kernel_handles_out = NULL;

    if (argc <= 1) {
        /* No args */
        tpb_printf(TPBM_PRTN_M_DIRECT,
                   "No arguments provided, exit after printing help message.\n");
        tpb_print_help_total();
        return TPBE_EXIT_ON_HELP;
    }

    if (strcmp(argv[1], "help") == 0) {
        if (argc <= 2) {
            tpb_print_help_total();
        }
        return TPBE_EXIT_ON_HELP;
    } else if (strcmp(argv[1], "run") == 0) {
        /* Set default */
        tpb_args->mode = 0;
        tpb_args->nkern = 0;
        tpb_args->list_only_flag = 0;
        strcpy(tpb_args->data_dir, "./data");
        set_timer("clock_gettime", tpb_args);

        /* New parsing logic for -k/-K pairs */
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
                    /* Common args (no -k yet) */
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
            tpb_printf(TPBM_PRTN_M_DIRECT, "No kernels specified. Use -k option.\n");
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

    } else if (strcmp(argv[1], "benchmark") == 0) {
        /* TODO: Implement benchmark action */
    } else if (strcmp(argv[1], "list") == 0) {
        /* TODO: Implement list action */
    } else {
        tpb_printf(TPBM_PRTN_M_DIRECT, "Unsupported action: %s. Please use one of actions:\n"
                   "run, benchmark, list, help.\n", argv[1]);
        tpb_print_help_total();
        return TPBE_CLI_FAIL;
    }

    return 0;
}
