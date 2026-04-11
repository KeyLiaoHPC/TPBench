/*
 * tpb-argp.c
 * TPBench argument parser infrastructure.
 * Provides internal utilities for parsing and validating kernel arguments.
 */

#ifdef __linux__
#include <linux/limits.h>
#else
#include <limits.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include "tpb-argp.h"
#include "../timers/timers.h"
#include "tpb-impl.h"
#include "tpb-driver.h"

/* Local Function Prototypes */

/* Trim whitespace from both ends of a string */
static char *_sf_trim_whitespace(char *str);

/* Local Function Implementations */

static char *
_sf_trim_whitespace(char *str)
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

/* Public Function Implementations */

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
        char *trimmed = _sf_trim_whitespace(token);
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
        char *key = _sf_trim_whitespace(trimmed);
        char *value = _sf_trim_whitespace(eq + 1);

        if (key == NULL || value == NULL || *key == '\0' || *value == '\0') {
            tpb_printf(TPBM_PRTN_M_DIRECT,
                       "Invalid kernel arg. Empty key or value detected.\n");
            return TPBE_KERN_ARG_FAIL;
        }

        /* Use tpb_driver_set_hdl_karg to set the argument for current handle */
        err = tpb_driver_set_hdl_karg(key, value);
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
#if defined(__x86_64__) || defined(_M_X64)
    } else if (strcmp(timer_name, "tsc_asym") == 0) {
        snprintf(timer.name, TPBM_NAME_STR_MAX_LEN, "tsc_asym");
        timer.unit = TPB_UNIT_CY;
        timer.dtype = TPB_INT64_T;
        timer.init = init_timer_tsc_asym;
        timer.tick = tick_tsc_asym;
        timer.tock = tock_tsc_asym;
        timer.get_stamp = get_time_tsc_asym;
#endif
    } else {
        return TPBE_CLI_FAIL;
    }

    return tpb_driver_set_timer(timer);
}
