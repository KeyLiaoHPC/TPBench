/*
 * tpb-argp.c
 * TPBench argument parser infrastructure.
 * Provides internal utilities for parsing and validating kernel arguments.
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
#include "tpb-argp.h"
#include "timers/timers.h"
#include "tpb-impl.h"
#include "tpb-driver.h"
#include "tpb-io.h"
#include "tpb-types.h"

/* Local Function Prototypes */

/* Trim whitespace from both ends of a string */
static char *trim_whitespace(char *str);

/* Free token array */
void free_tokens(char **tokens, int count);

/* Append tokens from comma-separated argument string */
int append_tokens(const char *argstr, char ***tokens, int *count, int *cap);

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

void
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

int
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

        uint32_t type_code = rt_parms[parm_idx].ctrlbits & TPB_PARM_TYPE_MASK;
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

        uint32_t check_mode = rt_parms[parm_idx].ctrlbits & TPB_PARM_CHECK_MASK;
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

    uint32_t type_code = parm->ctrlbits & TPB_PARM_TYPE_MASK;

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

    uint32_t type_code = parm->ctrlbits & TPB_PARM_TYPE_MASK;

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

    err = tpb_get_kernel("_tpb_common", &kernel_common);
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

int
tpb_argp_set_kargs_tokstr(int nchar, char *tokstr, int *narg)
{
    char *saveptr;
    char *token;
    char buf[TPBM_CLI_STR_MAX_LEN];
    int err;

    (void)nchar;  /* Unused parameter */

    if (tokstr == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    if (narg != NULL) {
        *narg = 1;  /* Initialize to 1 as per specification */
    }

    snprintf(buf, sizeof(buf), "%s", tokstr);
    token = strtok_r(buf, ",", &saveptr);

    while (token != NULL) {
        char *trimmed = trim_whitespace(token);
        if (trimmed == NULL || *trimmed == '\0') {
            return TPBE_CLI_FAIL;
        }

        /* Parse key=value */
        char *eq = strchr(trimmed, '=');
        if (eq == NULL) {
            tpb_printf(TPBM_PRTN_M_DIRECT,
                       "Invalid kernel arg \"%s\". Expected key=value.\n", trimmed);
            return TPBE_KERN_ARG_FAIL;
        }

        *eq = '\0';
        char *key = trim_whitespace(trimmed);
        char *value = trim_whitespace(eq + 1);

        if (key == NULL || value == NULL || *key == '\0' || *value == '\0') {
            tpb_printf(TPBM_PRTN_M_DIRECT,
                       "Invalid kernel arg. Empty key or value detected.\n");
            return TPBE_KERN_ARG_FAIL;
        }

        /* Use tpb_driver_set_karg to set the argument for current handle */
        err = tpb_driver_set_karg(NULL, key, value);
        if (err != 0) {
            return err;
        }

        if (narg != NULL) {
            (*narg)++;
        }

        token = strtok_r(NULL, ",", &saveptr);
    }

    return 0;
}

int
tpb_argp_set_timer(const char *timer_name)
{
    tpb_timer_t timer;

    if (timer_name == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    memset(&timer, 0, sizeof(tpb_timer_t));

    if (strcmp(timer_name, "clock_gettime") == 0) {
        snprintf(timer.name, TPBM_NAME_STR_MAX_LEN, "clock_gettime");
        timer.unit = TPB_UNIT_NS;
        timer.dtype = TPB_INT64_T;
        timer.init = init_timer_clock_gettime;
        timer.tick = tick_clock_gettime;
        timer.tock = tock_clock_gettime;
        timer.get_stamp = get_time_clock_gettime;
    } else if (strcmp(timer_name, "tsc_asym") == 0) {
        snprintf(timer.name, TPBM_NAME_STR_MAX_LEN, "tsc_asym");
        timer.unit = TPB_UNIT_CY;
        timer.dtype = TPB_INT64_T;
        timer.init = init_timer_tsc_asym;
        timer.tick = tick_tsc_asym;
        timer.tock = tock_tsc_asym;
        timer.get_stamp = get_time_tsc_asym;
    } else {
        return TPBE_CLI_FAIL;
    }

    return tpb_driver_set_timer(timer);
}
