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
#include "tpb-public.h"
#include "tpb-driver.h"
#include "tpb-types.h"
#include "tpb-unitcast.h"
#include "tpb_corelib_state.h"
#include "raw_db/tpb-rawdb-types.h"

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
    .unit_cast = 0,
    .sigbit_trim = 5,
    .initialized = 0
};

/* Logging state */
static FILE *log_file = NULL;
static char log_filepath[PATH_MAX] = {0};

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
    
    /* Read outargs from environment variables (for PLI kernels) */
    const char *env_unit_cast = getenv("TPBENCH_UNIT_CAST");
    const char *env_sigbit_trim = getenv("TPBENCH_SIGBIT_TRIM");
    if (env_unit_cast != NULL) {
        cliout_fmt.unit_cast = atoi(env_unit_cast);
    }
    if (env_sigbit_trim != NULL) {
        cliout_fmt.sigbit_trim = atoi(env_sigbit_trim);
    }
    
    cliout_fmt.initialized = 1;
}

void
tpb_set_outargs(int unit_cast, int sigbit_trim)
{
    cliout_fmt.unit_cast = unit_cast;
    cliout_fmt.sigbit_trim = sigbit_trim;
    
    /* Set environment variables for PLI kernels */
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", unit_cast);
    setenv("TPBENCH_UNIT_CAST", buf, 1);
    snprintf(buf, sizeof(buf), "%d", sigbit_trim);
    setenv("TPBENCH_SIGBIT_TRIM", buf, 1);
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

/* Logging Function Implementations */

int
tpb_log_init(void)
{
    char hostname[256] = {0};
    char logdir[PATH_MAX];
    char timestamp[32];
    time_t now;
    struct tm *tm_now;
    const char *ws;
    const char *env_path;

    if (log_file != NULL) {
        return TPBE_SUCCESS;
    }

    env_path = getenv(TPB_LOG_FILE_ENV);
    if (env_path != NULL && env_path[0] != '\0') {
        if (strlen(env_path) >= sizeof(log_filepath)) {
            printf("Warning: %s path too long\n", TPB_LOG_FILE_ENV);
            return TPBE_FILE_IO_FAIL;
        }
        snprintf(log_filepath, sizeof(log_filepath), "%s", env_path);
        log_file = fopen(log_filepath, "a");
        if (log_file == NULL) {
            printf("Warning: Could not open log file %s\n", log_filepath);
            return TPBE_FILE_IO_FAIL;
        }
        fflush(log_file);
        return TPBE_SUCCESS;
    }

    ws = _tpb_workspace_path_get();
    if (ws == NULL || ws[0] == '\0') {
        fprintf(stderr, "Warning: TPBench workspace not set for logging\n");
        return TPBE_FILE_IO_FAIL;
    }

    if (snprintf(logdir, sizeof(logdir), "%s/%s", ws, TPB_RAWDB_LOG_REL)
        >= (int)sizeof(logdir)) {
        fprintf(stderr, "Warning: Log directory path too long\n");
        return TPBE_FILE_IO_FAIL;
    }

    /* Get hostname */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        snprintf(hostname, sizeof(hostname), "unknown");
    }

    /* Generate timestamp */
    now = time(NULL);
    tm_now = localtime(&now);
    snprintf(timestamp, sizeof(timestamp), "%04d%02d%02dT%02d%02d%02d",
             tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday,
             tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);

    /* Construct log filepath */
    snprintf(log_filepath, sizeof(log_filepath),
             "%s/tpbrunlog_%s_%s.log", logdir, timestamp, hostname);

    /* Open log file */
    log_file = fopen(log_filepath, "w");
    if (log_file == NULL) {
        fprintf(stderr, "Warning: Could not open log file %s\n", log_filepath);
        return TPBE_FILE_IO_FAIL;
    }

    fprintf(log_file, "TPBench Run Log\n");
    fprintf(log_file, "Session Started: %04d-%02d-%02d %02d:%02d:%02d\n",
            tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday,
            tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
    fprintf(log_file, "Hostname: %s\n", hostname);
    fprintf(log_file, "TPB Version: %g\n", TPB_VERSION);
    fprintf(log_file, "TPBench workspace: %s\n", ws);
    fprintf(log_file,
            "Note: Raw database and this log file live under this workspace.\n");
    fprintf(log_file, "\n");
    fflush(log_file);

    return TPBE_SUCCESS;
}

void
tpb_log_cleanup(void)
{
    if (log_file != NULL) {
        fclose(log_file);
        log_file = NULL;
    }
}

static void
tpb_log_write(const char *msg)
{
    if (log_file != NULL && msg != NULL) {
        fputs(msg, log_file);
        fflush(log_file);
    }
}

const char *
tpb_log_get_filepath(void)
{
    if (log_filepath[0] != '\0') {
        return log_filepath;
    }
    return NULL;
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
    char msg_buf[4096];
    char header_buf[128] = {0};
    int header_len = 0;

    if (tag_mode == TPBE_WARN) {
        tag = "WARN";
    } else if (tag_mode == TPBE_FAIL) {
        tag = "FAIL";
    } else if (tag_mode == TPBE_UNKN) {
        tag = "UNKN";
    }

    va_list args, args_copy;
    va_start(args, fmt);

    /* Format the message for both console and log */
    if (print_mode == TPBM_PRTN_M_DIRECT) {
        /* Direct mode - format the message to buffer */
        va_copy(args_copy, args);
        vsnprintf(msg_buf, sizeof(msg_buf), fmt, args_copy);
        va_end(args_copy);
        
        vprintf(fmt, args);
        va_end(args);
        
        tpb_log_write(msg_buf);
        return;
    }
    
    /* Build header string for timestamped/tagged output */
    if (print_mode & TPBM_PRTN_M_TS) {
        time_t t = time(0);
        struct tm* lt = localtime(&t);
        header_len += snprintf(header_buf + header_len, 
                               sizeof(header_buf) - header_len,
                               "%04d-%02d-%02d %02d:%02d:%02d ",
                               lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
                               lt->tm_hour, lt->tm_min, lt->tm_sec);
    }
    if (print_mode & TPBM_PRTN_M_TAG) {
        header_len += snprintf(header_buf + header_len,
                               sizeof(header_buf) - header_len,
                               "[%s] ", tag);
    }
    
    /* Format the actual message */
    va_copy(args_copy, args);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args_copy);
    va_end(args_copy);
    
    /* Print to console */
    if (header_len > 0) {
        printf("%s", header_buf);
    }
    vprintf(fmt, args);
    va_end(args);
    
    /* Write to log file */
    if (header_len > 0) {
        tpb_log_write(header_buf);
    }
    tpb_log_write(msg_buf);
}

void
tpb_print_help_total(void)
{
    printf(TPBM_HELP_DOC_TOTAL);
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
    tpb_printf(TPBM_PRTN_M_DIRECT, "Input info\n");
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

            /* Check if we need to wrap - disabled, do not auto wrap */
            /*
            if (cur_col + total_len > max_col && cur_col > 0) {
                tpb_printf(TPBM_PRTN_M_DIRECT, "\n");
                cur_col = 0;
            }
            */

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
    int unit_cast = cliout_fmt.unit_cast;
    int sigbit_trim = cliout_fmt.sigbit_trim;

    /* Test results section */
    tpb_printf(TPBM_PRTN_M_DIRECT, "Output\n");

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

        /* Check if cast is enabled for this output and unit_cast is enabled */
        int cast_enabled = (out->unit & TPB_UATTR_CAST_MASK) == TPB_UATTR_CAST_Y;
        if (!cast_enabled || !unit_cast) {
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

        /* Extract shape, cast control, and trim control from unit */
        TPB_UNIT_T shape = out->unit & TPB_UATTR_SHAPE_MASK;
        int cast_enabled = (out->unit & TPB_UATTR_CAST_MASK) == TPB_UATTR_CAST_Y;
        int trim_disabled = (out->unit & TPB_UATTR_TRIM_MASK) == TPB_UATTR_TRIM_N;  /* TRIM_N means bit is set */
        TPB_UNIT_T base_unit = out->unit & ~TPB_UATTR_MASK;  /* Strip attributes */

        /* Print metrics name */
        tpb_printf(TPBM_PRTN_M_DIRECT, "Metrics: %s\n", out->name);

        /* Determine display unit */
        TPB_UNIT_T display_unit = base_unit;
        if (cast_enabled && unit_cast) {
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
            if (cast_enabled && unit_cast && display_unit != base_unit) {
                double current_scale = tpb_unit_get_scale(base_unit);
                double target_scale = tpb_unit_get_scale(display_unit);
                value = (value * current_scale) / target_scale;
            }

            char buf[64];
            /* Use sigbit_trim if enabled and not disabled for this output */
            if (sigbit_trim > 0 && !trim_disabled) {
                tpb_format_value(value, buf, sizeof(buf), sigbit_trim, intbit);
            } else if (sigbit_trim == 0 || trim_disabled) {
                /* sigbit_trim=0 or TRIM_N: no formatting, print as-is WITHOUT scientific notation */
                /* Check if value is integer-like (no significant fractional part) */
                if (value == floor(value)) {
                    snprintf(buf, sizeof(buf), "%.0f", value);
                } else {
                    snprintf(buf, sizeof(buf), "%f", value);
                    /* Remove trailing zeros after decimal point */
                    int len = strlen(buf);
                    while (len > 0 && buf[len-1] == '0') {
                        buf[--len] = '\0';
                    }
                    if (len > 0 && buf[len-1] == '.') {
                        buf[--len] = '\0';
                    }
                }
            } else {
                tpb_format_value(value, buf, sizeof(buf), sigbit, intbit);
            }
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
                if (cast_enabled && unit_cast && display_unit != base_unit) {
                    double current_scale = tpb_unit_get_scale(base_unit);
                    double target_scale = tpb_unit_get_scale(display_unit);
                    mean_val = (mean_val * current_scale) / target_scale;
                }
                char buf[64];
                /* Use sigbit_trim if enabled and not disabled for this output */
                if (sigbit_trim > 0 && !trim_disabled) {
                    tpb_format_value(mean_val, buf, sizeof(buf), sigbit_trim, intbit);
                } else if (sigbit_trim == 0 || trim_disabled) {
                    /* sigbit_trim=0 or TRIM_N: no formatting, print as-is WITHOUT scientific notation */
                    /* Check if value is integer-like (no significant fractional part) */
                    if (mean_val == floor(mean_val)) {
                        snprintf(buf, sizeof(buf), "%.0f", mean_val);
                    } else {
                        snprintf(buf, sizeof(buf), "%f", mean_val);
                        /* Remove trailing zeros after decimal point */
                        int len = strlen(buf);
                        while (len > 0 && buf[len-1] == '0') {
                            buf[--len] = '\0';
                        }
                        if (len > 0 && buf[len-1] == '.') {
                            buf[--len] = '\0';
                        }
                    }
                } else {
                    tpb_format_value(mean_val, buf, sizeof(buf), sigbit, intbit);
                }
                tpb_printf(TPBM_PRTN_M_DIRECT, "Result mean: %s\n", buf);
                tpb_printf(TPBM_PRTN_M_DIRECT, "Result quantiles: N/A\n");
                continue;
            }

            /* Scale values if cast enabled and units differ */
            if (cast_enabled && unit_cast && display_unit != base_unit) {
                double current_scale = tpb_unit_get_scale(base_unit);
                double target_scale = tpb_unit_get_scale(display_unit);
                mean_val = (mean_val * current_scale) / target_scale;
                for (size_t q = 0; q < nq; q++) {
                    qout[q] = (qout[q] * current_scale) / target_scale;
                }
            }

            /* Print mean */
            char buf[64];
            /* Use sigbit_trim if enabled and not disabled for this output */
            if (sigbit_trim > 0 && !trim_disabled) {
                tpb_format_value(mean_val, buf, sizeof(buf), sigbit_trim, intbit);
            } else if (sigbit_trim == 0 || trim_disabled) {
                /* sigbit_trim=0 or TRIM_N: no formatting, print as-is WITHOUT scientific notation */
                /* Check if value is integer-like (no significant fractional part) */
                if (mean_val == floor(mean_val)) {
                    snprintf(buf, sizeof(buf), "%.0f", mean_val);
                } else {
                    snprintf(buf, sizeof(buf), "%f", mean_val);
                    /* Remove trailing zeros after decimal point */
                    int len = strlen(buf);
                    while (len > 0 && buf[len-1] == '0') {
                        buf[--len] = '\0';
                    }
                    if (len > 0 && buf[len-1] == '.') {
                        buf[--len] = '\0';
                    }
                }
            } else {
                tpb_format_value(mean_val, buf, sizeof(buf), sigbit, intbit);
            }
            tpb_printf(TPBM_PRTN_M_DIRECT, "Result mean: %s\n", buf);

            /* Print quantiles */
            tpb_printf(TPBM_PRTN_M_DIRECT, "Result quantiles: ");
            for (size_t q = 0; q < nq; q++) {
                if (q > 0) {
                    tpb_printf(TPBM_PRTN_M_DIRECT, ", ");
                }
                /* Use sigbit_trim if enabled and not disabled for this output */
                if (sigbit_trim > 0 && !trim_disabled) {
                    tpb_format_value(qout[q], buf, sizeof(buf), sigbit_trim, intbit);
                } else if (sigbit_trim == 0 || trim_disabled) {
                    /* sigbit_trim=0 or TRIM_N: no formatting, print as-is WITHOUT scientific notation */
                    /* Check if value is integer-like (no significant fractional part) */
                    if (qout[q] == floor(qout[q])) {
                        snprintf(buf, sizeof(buf), "%.0f", qout[q]);
                    } else {
                        snprintf(buf, sizeof(buf), "%f", qout[q]);
                        /* Remove trailing zeros after decimal point */
                        int len = strlen(buf);
                        while (len > 0 && buf[len-1] == '0') {
                            buf[--len] = '\0';
                        }
                        if (len > 0 && buf[len-1] == '.') {
                            buf[--len] = '\0';
                        }
                    }
                } else {
                    tpb_format_value(qout[q], buf, sizeof(buf), sigbit, intbit);
                }
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
            if (cast_enabled && unit_cast && display_unit != base_unit) {
                double current_scale = tpb_unit_get_scale(base_unit);
                double target_scale = tpb_unit_get_scale(display_unit);
                mean_val = (mean_val * current_scale) / target_scale;
            }

            char buf[64];
            /* Use sigbit_trim if enabled and not disabled for this output */
            if (sigbit_trim > 0 && !trim_disabled) {
                tpb_format_value(mean_val, buf, sizeof(buf), sigbit_trim, intbit);
            } else if (sigbit_trim == 0 || trim_disabled) {
                /* sigbit_trim=0 or TRIM_N: no formatting, print as-is WITHOUT scientific notation */
                /* Check if value is integer-like (no significant fractional part) */
                if (mean_val == floor(mean_val)) {
                    snprintf(buf, sizeof(buf), "%.0f", mean_val);
                } else {
                    snprintf(buf, sizeof(buf), "%f", mean_val);
                    /* Remove trailing zeros after decimal point */
                    int len = strlen(buf);
                    while (len > 0 && buf[len-1] == '0') {
                        buf[--len] = '\0';
                    }
                    if (len > 0 && buf[len-1] == '.') {
                        buf[--len] = '\0';
                    }
                }
            } else {
                tpb_format_value(mean_val, buf, sizeof(buf), sigbit, intbit);
            }
            tpb_printf(TPBM_PRTN_M_DIRECT, "Result mean: %s\n", buf);
        }
    }

    free(qout);

    /* Print footer line */
    tpb_printf(TPBM_PRTN_M_DIRECT, DHLINE "\n");

    return TPBE_SUCCESS;
}
