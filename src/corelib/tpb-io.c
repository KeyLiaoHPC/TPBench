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
#include "tpb-tag-norm.h"
#include "tpb_corelib_state.h"
#include "rafdb/tpb-raf-types.h"

/* Local Function Prototypes */
static int _sf_format_parm_value(const tpb_rt_arg_t *parm, char *buf, size_t bufsize);
static int _sf_format_parm_value_only(const tpb_rt_arg_t *parm, char *buf,
                                      size_t bufsize);
static int _sf_format_sigfig(double value, char *buf, size_t bufsize, int sigbit);
static int _sf_format_cli_num(double value, char *buf, size_t bufsize,
                              int sigbit_trim, int sigbit, int intbit,
                              int trim_disabled);
static void _sf_format_dim_n(int n, TPB_UNIT_T unit, char *buf, size_t bufsize);
static inline TPB_UNIT_T _sf_get_uname(TPB_UNIT_T unit);
static void _sf_init_cliout(void);
static void _sf_transpose(uint64_t *out, uint64_t **in, int m, int n);
static void _sf_cliout_print_row(const float *ratios, int ncol, int gap,
                                 const uint32_t *align, const uint32_t *wrap,
                                 int term_width_min,
                                 const char *const *cells);

/* Module-level state: fixed result percentiles p25..p99 (not Q0.xx). */
static double default_qtiles[] = {0.25, 0.50, 0.75, 0.90, 0.95, 0.99};
static tpb_cliout_format_t cliout_fmt = {
    .max_col = 85,
    .qtiles = default_qtiles,
    .nq = 6,
    .unit_cast = 0,
    .sigbit_trim = 5,
    .initialized = 0
};

/** @brief Floor terminal width for args table. */
#define TPB_CLIOUT_TERM_WIDTH_MIN_ARGS    60
/**
 * @brief Floor terminal width for results table.
 *
 * Fourteen columns need ~160 chars before wrap; 60 would force near-empty cells.
 */
#define TPB_CLIOUT_TERM_WIDTH_MIN_RESULTS 160
#define TPB_CLIOUT_ARGS_NCOL      6
#define TPB_CLIOUT_RESULT_NCOL    14
#define TPB_CLIOUT_COL_GAP        1

#define MAX_UNAME_GROUPS 32

/* Local Function Implementations */

static void
_sf_init_cliout(void)
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
_sf_get_uname(TPB_UNIT_T unit)
{
    return unit & TPB_UNAME_MASK;
}

static int
_sf_format_sigfig(double value, char *buf, size_t bufsize, int sigbit)
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
_sf_print_dhline(int width)
{
    for (int i = 0; i < width; i++) {
        putchar('=');
    }
    putchar('\n');
}

static int
_sf_format_parm_value(const tpb_rt_arg_t *parm, char *buf, size_t bufsize)
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

/** @brief Format argument value only (no name=) for the args table Value column. */
static int
_sf_format_parm_value_only(const tpb_rt_arg_t *parm, char *buf, size_t bufsize)
{
    TPB_DTYPE type_only = parm->ctrlbits & TPB_PARM_TYPE_MASK;

    switch (type_only) {
    case TPB_INT_T:
    case TPB_INT32_T:
        return snprintf(buf, bufsize, "%d", (int)parm->value.i64);
    case TPB_INT8_T:
    case TPB_INT16_T:
    case TPB_INT64_T:
        return snprintf(buf, bufsize, "%ld", (long)parm->value.i64);
    case TPB_UINT8_T:
    case TPB_UINT16_T:
    case TPB_UINT32_T:
    case TPB_UINT64_T:
        return snprintf(buf, bufsize, "%lu", (unsigned long)parm->value.u64);
    case TPB_FLOAT_T:
        return snprintf(buf, bufsize, "%.6g", (double)parm->value.f32);
    case TPB_DOUBLE_T:
        return snprintf(buf, bufsize, "%.6g", parm->value.f64);
    default:
        return snprintf(buf, bufsize, "?");
    }
}

/**
 * @brief Format a CLI numeric cell using sigbit_trim / raw / sigbit rules.
 */
static int
_sf_format_cli_num(double value, char *buf, size_t bufsize,
                   int sigbit_trim, int sigbit, int intbit, int trim_disabled)
{
    if (sigbit_trim > 0 && !trim_disabled) {
        return tpb_format_value(value, buf, bufsize, sigbit_trim, intbit);
    }
    if (sigbit_trim == 0 || trim_disabled) {
        if (value == floor(value)) {
            return snprintf(buf, bufsize, "%.0f", value);
        }
        {
            int len = snprintf(buf, bufsize, "%f", value);
            while (len > 0 && buf[len - 1] == '0') {
                buf[--len] = '\0';
            }
            if (len > 0 && buf[len - 1] == '.') {
                buf[--len] = '\0';
            }
            return len;
        }
    }
    return tpb_format_value(value, buf, bufsize, sigbit, intbit);
}

/**
 * @brief Build dim cell: point => "1"; else element count (runtime has no ndim).
 */
static void
_sf_format_dim_n(int n, TPB_UNIT_T unit, char *buf, size_t bufsize)
{
    TPB_UNIT_T shape = unit & TPB_UATTR_SHAPE_MASK;

    if (shape == TPB_UATTR_SHAPE_POINT || n <= 1) {
        snprintf(buf, bufsize, "1");
        return;
    }
    snprintf(buf, bufsize, "%d", n);
}

/** @brief Print one cliout table row via tpblog_printf_ctab. */
static void
_sf_cliout_print_row(const float *ratios, int ncol, int gap,
                     const uint32_t *align, const uint32_t *wrap,
                     int term_width_min,
                     const char *const *cells)
{
    tpblog_ctab_t opt;

    memset(&opt, 0, sizeof(opt));
    opt.col_ratios = ratios;
    opt.ncol = ncol;
    opt.gap = gap;
    opt.align = align;
    opt.wrap = wrap;
    opt.term_col_width_min = term_width_min;
    tpblog_printf_ctab(&opt, cells);
}

static void
_sf_transpose(uint64_t *out, uint64_t **in, int m, int n)
{
    for (int j = 0; j < n; j++) {
        for (int i = 0; i < m; i++) {
            out[j * m + i] = in[i][j];
        }
    }
}

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
        TPB_FAIL(TPB_MOD_IO, TPBE_FILE_IO_FAIL, NULL);
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

int
tpb_cliout_args(tpb_k_rthdl_t *handle)
{
    static const float ratios[TPB_CLIOUT_ARGS_NCOL] = {
        1.0f, 3.0f, 2.0f, 1.5f, 1.0f, 3.0f
    };
    static const uint32_t align[TPB_CLIOUT_ARGS_NCOL] = {
        TPBLOG_ALIGN_RIGHT, TPBLOG_ALIGN_LEFT, TPBLOG_ALIGN_LEFT,
        TPBLOG_ALIGN_LEFT, TPBLOG_ALIGN_RIGHT, TPBLOG_ALIGN_LEFT
    };
    static const uint32_t wrap[TPB_CLIOUT_ARGS_NCOL] = {
        TPBLOG_WRAP_NO_HYPHEN, TPBLOG_WRAP_NO_HYPHEN, TPBLOG_WRAP_NO_HYPHEN,
        TPBLOG_WRAP_NO_HYPHEN, TPBLOG_WRAP_NO_HYPHEN, TPBLOG_WRAP_NO_HYPHEN
    };
    static const char *hdr[TPB_CLIOUT_ARGS_NCOL] = {
        "ID", "Name", "Tags", "Unit", "dim", "value"
    };
    const char *cells[TPB_CLIOUT_ARGS_NCOL];
    char id_buf[32];
    char tags_disp[TPBM_NAME_STR_MAX_LEN * 2];
    char dim_buf[32];
    char val_buf[128];
    int i;

    if (handle == NULL) {
        TPB_FAIL(TPB_MOD_IO, TPBE_NULLPTR_ARG, NULL);
    }

    _sf_init_cliout();

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "Input\n");
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "Kernel Name: %s\n", handle->kernel.info.name);

    /* Independent args table: ID Name Tags Unit dim value. */
    _sf_cliout_print_row(ratios, TPB_CLIOUT_ARGS_NCOL, TPB_CLIOUT_COL_GAP,
                         align, wrap, TPB_CLIOUT_TERM_WIDTH_MIN_ARGS, hdr);

    if (handle->argpack.n <= 0 || handle->argpack.args == NULL) {
        return TPBE_SUCCESS;
    }

    for (i = 0; i < handle->argpack.n; i++) {
        tpb_rt_arg_t *arg = &handle->argpack.args[i];

        snprintf(id_buf, sizeof(id_buf), "%d", i);
        if (_sf_format_tags_display(tags_disp, sizeof(tags_disp),
                                    arg->tag) != 0) {
            tags_disp[0] = '\0';
        }
        /* Runtime args are scalars; dim is always 1. */
        snprintf(dim_buf, sizeof(dim_buf), "1");
        _sf_format_parm_value_only(arg, val_buf, sizeof(val_buf));

        cells[0] = id_buf;
        cells[1] = arg->name;
        cells[2] = tags_disp;
        cells[3] = "-";
        cells[4] = dim_buf;
        cells[5] = val_buf;
        _sf_cliout_print_row(ratios, TPB_CLIOUT_ARGS_NCOL, TPB_CLIOUT_COL_GAP,
                             align, wrap, TPB_CLIOUT_TERM_WIDTH_MIN_ARGS, cells);
    }

    return TPBE_SUCCESS;
}

int
tpb_cliout_results(tpb_k_rthdl_t *handle)
{
    /* Columns: ID Name Tags Unit dim mean min max p25 p50 p75 p90 p95 p99 */
    static const float ratios[TPB_CLIOUT_RESULT_NCOL] = {
        1.0f, 3.0f, 2.0f, 1.2f, 1.2f,
        1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f
    };
    static const uint32_t align[TPB_CLIOUT_RESULT_NCOL] = {
        TPBLOG_ALIGN_RIGHT, TPBLOG_ALIGN_LEFT, TPBLOG_ALIGN_LEFT,
        TPBLOG_ALIGN_LEFT, TPBLOG_ALIGN_RIGHT,
        TPBLOG_ALIGN_RIGHT, TPBLOG_ALIGN_RIGHT, TPBLOG_ALIGN_RIGHT,
        TPBLOG_ALIGN_RIGHT, TPBLOG_ALIGN_RIGHT, TPBLOG_ALIGN_RIGHT,
        TPBLOG_ALIGN_RIGHT, TPBLOG_ALIGN_RIGHT, TPBLOG_ALIGN_RIGHT
    };
    static const uint32_t wrap[TPB_CLIOUT_RESULT_NCOL] = {
        TPBLOG_WRAP_NO_HYPHEN, TPBLOG_WRAP_NO_HYPHEN, TPBLOG_WRAP_NO_HYPHEN,
        TPBLOG_WRAP_NO_HYPHEN, TPBLOG_WRAP_NO_HYPHEN,
        TPBLOG_WRAP_NO_HYPHEN, TPBLOG_WRAP_NO_HYPHEN, TPBLOG_WRAP_NO_HYPHEN,
        TPBLOG_WRAP_NO_HYPHEN, TPBLOG_WRAP_NO_HYPHEN, TPBLOG_WRAP_NO_HYPHEN,
        TPBLOG_WRAP_NO_HYPHEN, TPBLOG_WRAP_NO_HYPHEN, TPBLOG_WRAP_NO_HYPHEN
    };
    static const char *hdr[TPB_CLIOUT_RESULT_NCOL] = {
        "ID", "Name", "Tags", "Unit", "dim",
        "mean", "min", "max", "p25", "p50", "p75", "p90", "p95", "p99"
    };
    const char *cells[TPB_CLIOUT_RESULT_NCOL];
    char id_buf[32];
    char tags_disp[TPBM_NAME_STR_MAX_LEN * 2];
    char dim_buf[64];
    char mean_buf[64], min_buf[64], max_buf[64];
    char pbuf[6][64];
    double *qout = NULL;
    double *qtiles;
    size_t nq;
    int sigbit;
    int intbit;
    int unit_cast;
    int sigbit_trim;
    int visible_id = 0;
    TPB_UNIT_T group_unames[MAX_UNAME_GROUPS];
    TPB_UNIT_T group_targets[MAX_UNAME_GROUPS];
    TPB_UNIT_T group_orig_units[MAX_UNAME_GROUPS];
    double group_mins[MAX_UNAME_GROUPS];
    int ngroups = 0;
    int i;

    if (handle == NULL) {
        TPB_FAIL(TPB_MOD_IO, TPBE_NULLPTR_ARG, NULL);
    }

    _sf_init_cliout();

    qtiles = cliout_fmt.qtiles;
    nq = cliout_fmt.nq;
    sigbit = cliout_fmt.sigbit;
    intbit = cliout_fmt.intbit;
    unit_cast = cliout_fmt.unit_cast;
    sigbit_trim = cliout_fmt.sigbit_trim;

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    "Output\n");

    qout = (double *)malloc(nq * sizeof(double));
    if (qout == NULL) {
        TPB_FAIL(TPB_MOD_IO, TPBE_MALLOC_FAIL, NULL);
    }

    /* Pass 1: Build UNAME groups for cast-enabled outputs. */
    for (i = 0; i < handle->respack.n; i++) {
        tpb_k_output_t *out = &handle->respack.outputs[i];
        int cast_enabled;
        TPB_UNIT_T uname;
        int gidx;
        int g;

        if (out->p == NULL || out->n <= 0) {
            continue;
        }
        cast_enabled = (out->unit & TPB_UATTR_CAST_MASK) == TPB_UATTR_CAST_Y;
        if (!cast_enabled || !unit_cast) {
            continue;
        }
        uname = _sf_get_uname(out->unit);
        gidx = -1;
        for (g = 0; g < ngroups; g++) {
            if (group_unames[g] == uname) {
                gidx = g;
                break;
            }
        }
        if (gidx < 0 && ngroups < MAX_UNAME_GROUPS) {
            gidx = ngroups++;
            group_unames[gidx] = uname;
            group_mins[gidx] = 1e308;
            group_orig_units[gidx] = out->unit & ~TPB_UATTR_MASK;
        }
        if (gidx >= 0) {
            double out_min = 0.0;
            if (tpb_stat_min(out->p, (size_t)out->n, out->dtype,
                             &out_min) == TPBE_SUCCESS) {
                double abs_min = fabs(out_min);
                if (abs_min > 0 && abs_min < group_mins[gidx]) {
                    group_mins[gidx] = abs_min;
                }
            }
        }
    }

    for (i = 0; i < ngroups; i++) {
        double min_arr[1] = { group_mins[i] };
        double cast_arr[1];
        TPB_UNIT_T target_unit = group_orig_units[i];
        int ret = tpb_cast_unit(min_arr, 1, TPB_DOUBLE_T,
                                group_orig_units[i], &target_unit, cast_arr,
                                sigbit, intbit);
        if (ret == TPBE_SUCCESS) {
            group_targets[i] = target_unit;
        } else {
            group_targets[i] = group_orig_units[i];
        }
    }

    _sf_cliout_print_row(ratios, TPB_CLIOUT_RESULT_NCOL, TPB_CLIOUT_COL_GAP,
                         align, wrap, TPB_CLIOUT_TERM_WIDTH_MIN_RESULTS, hdr);

    for (i = 0; i < handle->respack.n; i++) {
        tpb_k_output_t *out = &handle->respack.outputs[i];
        TPB_UNIT_T shape;
        int cast_enabled;
        int trim_disabled;
        TPB_UNIT_T base_unit;
        TPB_UNIT_T display_unit;
        int g;
        size_t q;
        int err;
        double mean_val = 0.0;
        double min_val = 0.0;
        double max_val = 0.0;
        int is_point;

        if (out->p == NULL) {
            continue;
        }

        shape = out->unit & TPB_UATTR_SHAPE_MASK;
        cast_enabled = (out->unit & TPB_UATTR_CAST_MASK) == TPB_UATTR_CAST_Y;
        trim_disabled = (out->unit & TPB_UATTR_TRIM_MASK) == TPB_UATTR_TRIM_N;
        base_unit = out->unit & ~TPB_UATTR_MASK;
        display_unit = base_unit;
        is_point = (shape == TPB_UATTR_SHAPE_POINT);

        if (cast_enabled && unit_cast) {
            TPB_UNIT_T uname = _sf_get_uname(out->unit);
            for (g = 0; g < ngroups; g++) {
                if (group_unames[g] == uname) {
                    display_unit = group_targets[g];
                    break;
                }
            }
        }

        snprintf(id_buf, sizeof(id_buf), "%d", visible_id++);
        if (_sf_format_tags_display(tags_disp, sizeof(tags_disp),
                                    out->tag) != 0) {
            tags_disp[0] = '\0';
        }
        _sf_format_dim_n(out->n, out->unit, dim_buf, sizeof(dim_buf));

        /* Default fillers: point rows leave non-mean stats as "-". */
        snprintf(mean_buf, sizeof(mean_buf), "-");
        snprintf(min_buf, sizeof(min_buf), "-");
        snprintf(max_buf, sizeof(max_buf), "-");
        for (q = 0; q < 6; q++) {
            snprintf(pbuf[q], sizeof(pbuf[q]), "-");
        }

        if (is_point || out->n <= 1) {
            err = tpb_stat_mean(out->p, (size_t)(out->n > 0 ? out->n : 1),
                                out->dtype, &mean_val);
            if (err != TPBE_SUCCESS) {
                snprintf(mean_buf, sizeof(mean_buf), "N/A");
            } else {
                if (cast_enabled && unit_cast && display_unit != base_unit) {
                    double current_scale = tpb_unit_get_scale(base_unit);
                    double target_scale = tpb_unit_get_scale(display_unit);
                    mean_val = (mean_val * current_scale) / target_scale;
                }
                _sf_format_cli_num(mean_val, mean_buf, sizeof(mean_buf),
                                   sigbit_trim, sigbit, intbit, trim_disabled);
            }
        } else {
            if (shape != TPB_UATTR_SHAPE_1D) {
                tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN,
                                TPBLOG_FLAG_TSTAG,
                                "Multi-dimension array flattened for CLI stats\n");
            }
            err = tpb_stat_mean(out->p, (size_t)out->n, out->dtype, &mean_val);
            if (err != TPBE_SUCCESS) {
                snprintf(mean_buf, sizeof(mean_buf), "N/A");
                snprintf(min_buf, sizeof(min_buf), "N/A");
                snprintf(max_buf, sizeof(max_buf), "N/A");
                for (q = 0; q < 6; q++) {
                    snprintf(pbuf[q], sizeof(pbuf[q]), "N/A");
                }
            } else {
                (void)tpb_stat_min(out->p, (size_t)out->n, out->dtype, &min_val);
                (void)tpb_stat_max(out->p, (size_t)out->n, out->dtype, &max_val);
                err = tpb_stat_qtile_1d(out->p, (size_t)out->n, out->dtype,
                                        qtiles, nq, qout);
                if (cast_enabled && unit_cast && display_unit != base_unit) {
                    double current_scale = tpb_unit_get_scale(base_unit);
                    double target_scale = tpb_unit_get_scale(display_unit);
                    mean_val = (mean_val * current_scale) / target_scale;
                    min_val = (min_val * current_scale) / target_scale;
                    max_val = (max_val * current_scale) / target_scale;
                    if (err == TPBE_SUCCESS) {
                        for (q = 0; q < nq; q++) {
                            qout[q] = (qout[q] * current_scale) / target_scale;
                        }
                    }
                }
                _sf_format_cli_num(mean_val, mean_buf, sizeof(mean_buf),
                                   sigbit_trim, sigbit, intbit, trim_disabled);
                _sf_format_cli_num(min_val, min_buf, sizeof(min_buf),
                                   sigbit_trim, sigbit, intbit, trim_disabled);
                _sf_format_cli_num(max_val, max_buf, sizeof(max_buf),
                                   sigbit_trim, sigbit, intbit, trim_disabled);
                if (err != TPBE_SUCCESS) {
                    for (q = 0; q < 6; q++) {
                        snprintf(pbuf[q], sizeof(pbuf[q]), "N/A");
                    }
                } else {
                    for (q = 0; q < nq && q < 6; q++) {
                        _sf_format_cli_num(qout[q], pbuf[q], sizeof(pbuf[q]),
                                           sigbit_trim, sigbit, intbit,
                                           trim_disabled);
                    }
                }
            }
        }

        cells[0] = id_buf;
        cells[1] = out->name;
        cells[2] = tags_disp;
        cells[3] = tpb_unit_to_string(display_unit);
        cells[4] = dim_buf;
        cells[5] = mean_buf;
        cells[6] = min_buf;
        cells[7] = max_buf;
        cells[8] = pbuf[0];
        cells[9] = pbuf[1];
        cells[10] = pbuf[2];
        cells[11] = pbuf[3];
        cells[12] = pbuf[4];
        cells[13] = pbuf[5];
        _sf_cliout_print_row(ratios, TPB_CLIOUT_RESULT_NCOL, TPB_CLIOUT_COL_GAP,
                             align, wrap, TPB_CLIOUT_TERM_WIDTH_MIN_RESULTS, cells);
    }

    free(qout);
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                    DHLINE "\n");
    return TPBE_SUCCESS;
}
