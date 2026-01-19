/*
 * tpb-io.c
 * I/O and CLI output functions for TPBench.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <inttypes.h>
#include "tpb-io.h"
#include "tpb-stat.h"
#include "tpb-driver.h"
#include "tpb-types.h"
#include "tpb-unitcast.h"

/* Local Function Prototypes */

/* Initialize CLI output format controller, gets terminal width via ioctl */
static void init_cliout(void);

/* Extract UNAME+UKIND from a unit for grouping purposes */
static inline TPB_UNIT_T get_uname(TPB_UNIT_T unit);

/* Format a value with exactly sigbit significant figures */
static int format_sigfig(double value, char *buf, size_t bufsize, int sigbit);

/* Print a dynamic-width double horizontal line */
static void print_dhline(int width);

/* Format and print a parameter value based on its dtype */
static int format_parm_value(const tpb_rt_parm_t *parm, char *buf, size_t bufsize);

/* Transpose a 2D array */
static void transpose(uint64_t *out, uint64_t **in, int m, int n);

/* Module-level state */
static double default_qtiles[] = {0.05, 0.25, 0.50, 0.75, 0.95};
static tpb_cliout_format_t cliout_fmt = {
    .max_col = 85,
    .qtiles = default_qtiles,
    .nq = 5,
    .initialized = 0
};

#define MAX_UNAME_GROUPS 32

/* Local Function Implementations */

static void
init_cliout(void)
{
    if (cliout_fmt.initialized) {
        return;
    }

    /* Try to get actual terminal width */
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        cliout_fmt.max_col = ws.ws_col;
    }
    /* Keep default (85) if ioctl fails */

    cliout_fmt.sigbit = 5;
    cliout_fmt.intbit = 1;
    cliout_fmt.initialized = 1;
}

static inline TPB_UNIT_T
get_uname(TPB_UNIT_T unit)
{
    return unit & TPB_UNAME_MASK;
}

static int
format_sigfig(double value, char *buf, size_t bufsize, int sigbit)
{
    if (bufsize == 0) {
        return 0;
    }
    if (value == 0.0) {
        return snprintf(buf, bufsize, "0.%0*d", sigbit - 1, 0);
    }

    double absval = fabs(value);
    int exponent = (int)floor(log10(absval));
    int decimals = sigbit - 1 - exponent;

    if (decimals < 0) {
        decimals = 0;
    }

    return snprintf(buf, bufsize, "%.*f", decimals, value);
}

static void
print_dhline(int width)
{
    for (int i = 0; i < width; i++) {
        putchar('=');
    }
    putchar('\n');
}

static int
format_parm_value(const tpb_rt_parm_t *parm, char *buf, size_t bufsize)
{
    TPB_DTYPE type_only = parm->ctrlbits & TPB_PARM_TYPE_MASK;

    switch (type_only) {
    case TPB_INT_T:
    case TPB_INT32_T:
        return snprintf(buf, bufsize, "%s=%d", parm->name, (int)parm->value.i64);
    case TPB_INT8_T:
    case TPB_INT16_T:
    case TPB_INT64_T:
        return snprintf(buf, bufsize, "%s=%ld", parm->name, (long)parm->value.i64);
    case TPB_UINT8_T:
    case TPB_UINT16_T:
    case TPB_UINT32_T:
    case TPB_UINT64_T:
        return snprintf(buf, bufsize, "%s=%lu", parm->name, (unsigned long)parm->value.u64);
    case TPB_FLOAT_T:
        return snprintf(buf, bufsize, "%s=%.6g", parm->name, (double)parm->value.f32);
    case TPB_DOUBLE_T:
        return snprintf(buf, bufsize, "%s=%.6g", parm->name, parm->value.f64);
    default:
        return snprintf(buf, bufsize, "%s=?", parm->name);
    }
}

static void
transpose(uint64_t *out, uint64_t **in, int m, int n)
{
    for (int j = 0; j < n; j++) {
        for (int i = 0; i < m; i++) {
            out[j * m + i] = in[i][j];
        }
    }
}

/* Public Function Implementations */

int
tpb_mkdir(char *path)
{
    int err;
    const size_t len = strlen(path);
    char _path[PATH_MAX];
    char *p;

    errno = 0;

    /* Copy string so its mutable */
    if (len > sizeof(_path) - 1) {
        errno = ENAMETOOLONG;
        return -1;
    }
    strcpy(_path, path);

    /* Iterate the string */
    for (p = _path + 1; *p; p++) {
        if (*p == '/') {
            /* Temporarily truncate */
            *p = '\0';

            if (mkdir(_path, S_IRWXU) != 0) {
                if (errno != EEXIST) {
                    return -1;
                }
            }

            *p = '/';
        }
    }

    if (mkdir(_path, S_IRWXU) != 0) {
        if (errno != EEXIST) {
            return -1;
        }
    }
    return 0;
}


int
tpb_writecsv(char *path, int64_t **data, int nrow, int ncol, char *header)
{
#ifdef TPM_NO_RAW_DATA
    return 0;
#else
    int err, i, j;
    FILE *fp;

    fp = fopen(path, "w");
    if (fp == NULL) {
        return TPBE_FILE_IO_FAIL;
    }
    if (header != NULL && strlen(header) > 0) {
        fprintf(fp, "%s\n", header);
    }

    /* data[col][row], for kernel benchmark */
    for (i = 0; i < nrow; i++) {
        for (j = 0; j < ncol - 1; j++) {
            fprintf(fp, "%"PRId64",", data[j][i]);
        }
        fprintf(fp, "%"PRId64"\n", data[ncol - 1][i]);
    }
    fflush(fp);
    fclose(fp);

    return 0;
#endif
}

/* TPBench printf wrapper */
void
tpb_printf(uint64_t mode_bit, char *fmt, ...)
{
    uint64_t print_mode = mode_bit & 0x0F;
    uint64_t tag_mode = mode_bit & 0xF0;
    const char *tag = "NOTE";

    if (tag_mode == TPBE_WARN) {
        tag = "WARN";
    } else if (tag_mode == TPBE_FAIL) {
        tag = "FAIL";
    } else if (tag_mode == TPBE_UNKN) {
        tag = "UNKN";
    }

    va_list args;
    va_start(args, fmt);

    if (print_mode == TPBM_PRTN_M_DIRECT) {
        vprintf(fmt, args);
        va_end(args);
        return;
    }
    if (print_mode & TPBM_PRTN_M_TS) {
        time_t t = time(0);
        struct tm* lt = localtime(&t);
        printf("%04d-%02d-%02d %02d:%02d:%02d ",
               lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
               lt->tm_hour, lt->tm_min, lt->tm_sec);
    }
    if (print_mode & TPBM_PRTN_M_TAG) {
        printf("[%s] ", tag);
    }
    vprintf(fmt, args);
    va_end(args);
}

void
tpb_print_help_total(void)
{
    printf(TPBM_HELP_DOC_TOTAL);
}

void
tpb_list()
{
    int nkern = tpb_get_kernel_count();
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Listing supported kernels.\n");
    tpb_printf(TPBM_PRTN_M_DIRECT, "Kernel          Description\n");
    for (int i = 0; i < nkern; i++) {
        tpb_kernel_t *kernel = NULL;
        if (tpb_get_kernel_by_index(i, &kernel) != 0) {
            continue;
        }
        tpb_printf(TPBM_PRTN_M_DIRECT, "%-15s %s\n",
                   kernel->info.name, kernel->info.note);
    }
}

int
tpb_cliout_args(tpb_k_rthdl_t *handle)
{
    if (handle == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    /* Initialize format controller on first call */
    init_cliout();

    int max_col = cliout_fmt.max_col;

    /* Kernel Name - do not wrap even if over max_col */
    tpb_printf(TPBM_PRTN_M_DIRECT, "## Input info  \n");
    tpb_printf(TPBM_PRTN_M_DIRECT, "Kernel Name: %s\n", handle->kernel.info.name);

    /* Run-time parameter settings - wrap at max_col */
    if (handle->argpack.n > 0 && handle->argpack.args != NULL) {
        const char *prefix = "Run-time parameter settings: ";
        int prefix_len = (int)strlen(prefix);
        int cur_col = 0;

        tpb_printf(TPBM_PRTN_M_DIRECT, "%s", prefix);
        cur_col = prefix_len;

        for (int i = 0; i < handle->argpack.n; i++) {
            char parm_buf[256];
            int parm_len = format_parm_value(&handle->argpack.args[i],
                                             parm_buf, sizeof(parm_buf));

            /* Add separator */
            const char *sep = (i < handle->argpack.n - 1) ? ", " : "";
            int sep_len = (int)strlen(sep);
            int total_len = parm_len + sep_len;

            /* Check if we need to wrap */
            if (cur_col + total_len > max_col && cur_col > prefix_len) {
                tpb_printf(TPBM_PRTN_M_DIRECT, "\n");
                /* Print continuation indent */
                char indent[256];
                memset(indent, ' ', prefix_len);
                indent[prefix_len] = '\0';
                tpb_printf(TPBM_PRTN_M_DIRECT, "%s", indent);
                cur_col = prefix_len;
            }

            tpb_printf(TPBM_PRTN_M_DIRECT, "%s%s", parm_buf, sep);
            cur_col += total_len;
        }
        tpb_printf(TPBM_PRTN_M_DIRECT, "\n");
    }

    return TPBE_SUCCESS;
}

int
tpb_cliout_results(tpb_k_rthdl_t *handle)
{
    if (handle == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    /* Initialize format controller on first call */
    init_cliout();

    double *qtiles = cliout_fmt.qtiles;
    size_t nq = cliout_fmt.nq;
    int sigbit = cliout_fmt.sigbit;
    int intbit = cliout_fmt.intbit;

    /* Test results section */
    tpb_printf(TPBM_PRTN_M_DIRECT, "## Output  \n");

    /* Allocate quantile output array */
    double *qout = (double *)malloc(nq * sizeof(double));
    if (qout == NULL) {
        return TPBE_MALLOC_FAIL;
    }

    /* Pass 1: Build UNAME groups for cast-enabled outputs */
    TPB_UNIT_T group_unames[MAX_UNAME_GROUPS];
    TPB_UNIT_T group_targets[MAX_UNAME_GROUPS];
    TPB_UNIT_T group_orig_units[MAX_UNAME_GROUPS];
    double group_mins[MAX_UNAME_GROUPS];
    int ngroups = 0;

    for (int i = 0; i < handle->respack.n; i++) {
        tpb_k_output_t *out = &handle->respack.outputs[i];
        if (out->p == NULL || out->n <= 0) {
            continue;
        }

        /* Check if cast is enabled for this output */
        int cast_enabled = (out->unit & TPB_UATTR_CAST_MASK) == TPB_UATTR_CAST_Y;
        if (!cast_enabled) {
            continue;
        }

        TPB_UNIT_T uname = get_uname(out->unit);

        /* Find or create group for this UNAME */
        int gidx = -1;
        for (int g = 0; g < ngroups; g++) {
            if (group_unames[g] == uname) {
                gidx = g;
                break;
            }
        }
        if (gidx < 0 && ngroups < MAX_UNAME_GROUPS) {
            gidx = ngroups++;
            group_unames[gidx] = uname;
            group_mins[gidx] = 1e308;
            group_orig_units[gidx] = out->unit & ~TPB_UATTR_MASK;  /* Strip attributes */
        }

        if (gidx >= 0) {
            /* Find minimum value in this output's raw data */
            double out_min = 0.0;
            if (tpb_stat_min(out->p, (size_t)out->n, out->dtype, &out_min) == TPBE_SUCCESS) {
                double abs_min = fabs(out_min);
                if (abs_min > 0 && abs_min < group_mins[gidx]) {
                    group_mins[gidx] = abs_min;
                }
            }
        }
    }

    /* Determine target unit for each group using the group minimum */
    for (int g = 0; g < ngroups; g++) {
        double min_arr[1] = { group_mins[g] };
        double cast_arr[1];
        TPB_UNIT_T target_unit = group_orig_units[g];

        int ret = tpb_cast_unit(min_arr, 1, TPB_DOUBLE_T,
                                group_orig_units[g], &target_unit, cast_arr,
                                sigbit, intbit);
        if (ret == TPBE_SUCCESS) {
            group_targets[g] = target_unit;
        } else {
            group_targets[g] = group_orig_units[g];
        }
    }

    /* Pass 2: Print each output based on shape */
    for (int i = 0; i < handle->respack.n; i++) {
        tpb_k_output_t *out = &handle->respack.outputs[i];

        /* Skip if output data is NULL */
        if (out->p == NULL) {
            continue;
        }

        /* Extract shape and cast control from unit */
        TPB_UNIT_T shape = out->unit & TPB_UATTR_SHAPE_MASK;
        int cast_enabled = (out->unit & TPB_UATTR_CAST_MASK) == TPB_UATTR_CAST_Y;
        TPB_UNIT_T base_unit = out->unit & ~TPB_UATTR_MASK;  /* Strip attributes */

        /* Print metrics name */
        tpb_printf(TPBM_PRTN_M_DIRECT, "### Metrics: %s\n", out->name);

        /* Determine display unit */
        TPB_UNIT_T display_unit = base_unit;
        if (cast_enabled) {
            TPB_UNIT_T uname = get_uname(out->unit);
            for (int g = 0; g < ngroups; g++) {
                if (group_unames[g] == uname) {
                    display_unit = group_targets[g];
                    break;
                }
            }
        }

        /* Print unit */
        tpb_printf(TPBM_PRTN_M_DIRECT, "Units: %s\n", tpb_unit_to_string(display_unit));

        /* Handle output based on shape */
        if (shape == TPB_UATTR_SHAPE_POINT) {
            /* Single-point data */
            double value = 0.0;
            int err = tpb_stat_mean(out->p, 1, out->dtype, &value);
            if (err != TPBE_SUCCESS) {
                tpb_printf(TPBM_PRTN_M_DIRECT, "Result value: N/A\n");
                continue;
            }

            /* Scale value if cast enabled and units differ */
            if (cast_enabled && display_unit != base_unit) {
                double current_scale = tpb_unit_get_scale(base_unit);
                double target_scale = tpb_unit_get_scale(display_unit);
                value = (value * current_scale) / target_scale;
            }

            char buf[64];
            tpb_format_value(value, buf, sizeof(buf), sigbit, intbit);
            tpb_printf(TPBM_PRTN_M_DIRECT, "Result value: %s\n", buf);

        } else if (shape == TPB_UATTR_SHAPE_1D) {
            /* 1D array - calculate mean and quantiles */
            double mean_val = 0.0;
            int err = tpb_stat_mean(out->p, (size_t)out->n, out->dtype, &mean_val);
            if (err != TPBE_SUCCESS) {
                tpb_printf(TPBM_PRTN_M_DIRECT, "Result mean: N/A\n");
                tpb_printf(TPBM_PRTN_M_DIRECT, "Result quantiles: N/A\n");
                continue;
            }

            /* Calculate quantiles */
            err = tpb_stat_qtile_1d(out->p, (size_t)out->n, out->dtype, qtiles, nq, qout);
            if (err != TPBE_SUCCESS) {
                /* Scale and format mean */
                if (cast_enabled && display_unit != base_unit) {
                    double current_scale = tpb_unit_get_scale(base_unit);
                    double target_scale = tpb_unit_get_scale(display_unit);
                    mean_val = (mean_val * current_scale) / target_scale;
                }
                char buf[64];
                tpb_format_value(mean_val, buf, sizeof(buf), sigbit, intbit);
                tpb_printf(TPBM_PRTN_M_DIRECT, "Result mean: %s\n", buf);
                tpb_printf(TPBM_PRTN_M_DIRECT, "Result quantiles: N/A\n");
                continue;
            }

            /* Scale values if cast enabled and units differ */
            if (cast_enabled && display_unit != base_unit) {
                double current_scale = tpb_unit_get_scale(base_unit);
                double target_scale = tpb_unit_get_scale(display_unit);
                mean_val = (mean_val * current_scale) / target_scale;
                for (size_t q = 0; q < nq; q++) {
                    qout[q] = (qout[q] * current_scale) / target_scale;
                }
            }

            /* Print mean */
            char buf[64];
            tpb_format_value(mean_val, buf, sizeof(buf), sigbit, intbit);
            tpb_printf(TPBM_PRTN_M_DIRECT, "Result mean: %s\n", buf);

            /* Print quantiles */
            tpb_printf(TPBM_PRTN_M_DIRECT, "Result quantiles: ");
            for (size_t q = 0; q < nq; q++) {
                if (q > 0) {
                    tpb_printf(TPBM_PRTN_M_DIRECT, ", ");
                }
                tpb_format_value(qout[q], buf, sizeof(buf), sigbit, intbit);
                tpb_printf(TPBM_PRTN_M_DIRECT, "Q%.2f=%s", qtiles[q], buf);
            }
            tpb_printf(TPBM_PRTN_M_DIRECT, "\n");

        } else {
            /* 2D+ arrays - warn and treat as 1D */
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                       "Multi-dimension array not supported by CLI output\n");

            /* Calculate mean of all elements as fallback */
            double mean_val = 0.0;
            int err = tpb_stat_mean(out->p, (size_t)out->n, out->dtype, &mean_val);
            if (err != TPBE_SUCCESS) {
                tpb_printf(TPBM_PRTN_M_DIRECT, "Result mean: N/A\n");
                continue;
            }

            /* Scale value if cast enabled and units differ */
            if (cast_enabled && display_unit != base_unit) {
                double current_scale = tpb_unit_get_scale(base_unit);
                double target_scale = tpb_unit_get_scale(display_unit);
                mean_val = (mean_val * current_scale) / target_scale;
            }

            char buf[64];
            tpb_format_value(mean_val, buf, sizeof(buf), sigbit, intbit);
            tpb_printf(TPBM_PRTN_M_DIRECT, "Result mean: %s\n", buf);
        }
    }

    free(qout);

    /* Print footer line */
    tpb_printf(TPBM_PRTN_M_DIRECT, DHLINE "\n");

    return TPBE_SUCCESS;
}
