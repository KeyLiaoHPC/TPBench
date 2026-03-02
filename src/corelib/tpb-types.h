/**
 * @file tpb-types.h
 * @brief Internal type definitions for TPBench corelib.
 *
 * Public types, macros, and structs come from tpbench.h (the single source
 * of truth).  This header adds internal-only definitions used within
 * corelib but not exposed to kernel consumers.
 */

#ifndef TPB_TYPES_H
#define TPB_TYPES_H
#define TPB_VERSION 0.8

#include <limits.h>
#ifdef __linux__
#include <linux/limits.h>
#else
#include <limits.h>
#endif

#include "../include/tpbench.h"

/** @brief Maximum number of kernels */
#define TPBM_CLI_K_MAX 128

/** Integration mode for driver */
#define TPB_INTEG_MODE_FLI  0
#define TPB_INTEG_MODE_PLI  1

typedef struct _tpb_error {
    tpb_errno_t err_code;
    unsigned err_type;
    char err_msg[256];
} tpb_error_type;

/** @brief TPBench run-time parameters */
typedef struct tpb_args {
    tpb_timer_t timer;
    int nkern;
} tpb_args_t;

/** @brief Metric-unit pair for performance reporting */
typedef struct tpb_k_metric {
    char *metric;
    char *unit;
} tpb_k_metric_t;

/** @brief TPBench result data file struct */
typedef struct tpb_res {
    char header[1024];
    char fname[1024];
    char fdir[PATH_MAX];
    char fpath[PATH_MAX];
    int64_t **data;
} tpb_res_t;

/** @brief CLI output format configuration structure */
typedef struct tpb_cliout_format {
    int max_col;
    size_t nq;
    double *qtiles;
    int sigbit, intbit;
    int unit_cast;      /* Enable/disable unit casting (0 or 1) */
    int sigbit_trim;    /* Significant bits for trimming (default 5, 0=no trim) */
    int initialized;
} tpb_cliout_format_t;

#endif /* TPB_TYPES_H */
