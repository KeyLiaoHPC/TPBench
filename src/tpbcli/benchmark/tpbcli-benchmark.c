/*
 * tpbcli-benchmark.c
 * Benchmark subcommand implementation.
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifdef __linux__
#include <linux/limits.h>
#else
#include <limits.h>
#endif
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include "tpb-public.h"
#include "tpbcli-benchmark.h"
#include "tpb-bench-yaml.h"
#include "tpb-bench-score.h"

/* Local Function Prototypes */
static int resolve_suite_path(const char *suite_arg, char *suite_path, size_t path_size);
static int run_batch(tpb_bench_batch_t *batch, int *tbatch_fatal);
static int load_metrics_from_rafdb(const char *workspace,
                                   const unsigned char tbatch_id[20],
                                   tpb_bench_batch_t *batch);
static int apply_reducer(void *arr, size_t narr, TPB_DTYPE dtype,
                         const char *reducer, double *out);
static int header_narr(const tpb_meta_header_t *h, size_t esz, size_t *narr_out);
static int is_zero_id(const unsigned char id[20]);

/* Local Function Implementations */

/**
 * @brief Resolve suite argument to full path.
 * 
 * Checks if argument is an existing file path, otherwise looks in ${TPB_HOME}/etc/
 */
static int
resolve_suite_path(const char *suite_arg, char *suite_path, size_t path_size)
{
    if (suite_arg == NULL || suite_path == NULL) {
        TPB_FAIL(TPB_MOD_CLI_BENCHMARK, TPBE_NULLPTR_ARG, NULL);
    }
    
    /* First, check if it's a direct path that exists */
    if (access(suite_arg, F_OK) == 0) {
        strncpy(suite_path, suite_arg, path_size - 1);
        suite_path[path_size - 1] = '\0';
        return TPBE_SUCCESS;
    }
    
    /* Try adding .yml extension */
    char with_ext[PATH_MAX];
    snprintf(with_ext, sizeof(with_ext), "%s.yml", suite_arg);
    if (access(with_ext, F_OK) == 0) {
        strncpy(suite_path, with_ext, path_size - 1);
        suite_path[path_size - 1] = '\0';
        return TPBE_SUCCESS;
    }
    
    /* Look in ${TPB_HOME}/etc/ */
    const char *tpb_home = tpb_dl_get_tpb_home();
    if (tpb_home != NULL && strlen(tpb_home) > 0) {
        snprintf(suite_path, path_size, "%s/etc/%s", tpb_home, suite_arg);
        if (access(suite_path, F_OK) == 0) {
            return TPBE_SUCCESS;
        }
        
        /* Try with .yml extension */
        snprintf(suite_path, path_size, "%s/etc/%s.yml", tpb_home, suite_arg);
        if (access(suite_path, F_OK) == 0) {
            return TPBE_SUCCESS;
        }
    }
    
    tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG, 
               "Suite file not found: %s\n", suite_arg);
    TPB_FAIL(TPB_MOD_CLI_BENCHMARK, TPBE_FILE_IO_FAIL, NULL);
}

/**
 * @brief True when a 20-byte ID is all zeros (entry-point / no derive_to).
 */
static int
is_zero_id(const unsigned char id[20])
{
    int i;

    for (i = 0; i < 20; i++) {
        if (id[i] != 0) {
            return 0;
        }
    }
    return 1;
}

/**
 * @brief Element count for a header payload from dimsizes or data_size/esz.
 */
static int
header_narr(const tpb_meta_header_t *h, size_t esz, size_t *narr_out)
{
    uint64_t total = 1;
    uint32_t j;
    uint32_t nd;

    if (h == NULL || narr_out == NULL || esz == 0) {
        return -1;
    }
    nd = h->ndim;
    if (nd > TPBM_DATA_NDIM_MAX) {
        nd = TPBM_DATA_NDIM_MAX;
    }
    if (nd == 0) {
        *narr_out = (size_t)(h->data_size / esz);
        return 0;
    }
    for (j = 0; j < nd; j++) {
        if (h->dimsizes[j] == 0) {
            total = 0;
            break;
        }
        if (total > UINT64_MAX / h->dimsizes[j]) {
            return -1;
        }
        total *= h->dimsizes[j];
    }
    if (total == 0) {
        *narr_out = (size_t)(h->data_size / esz);
        return 0;
    }
    *narr_out = (size_t)total;
    return 0;
}

/**
 * @brief Apply YAML reducer to a typed array (mean/min/max/median/pXX/Q=).
 */
static int
apply_reducer(void *arr, size_t narr, TPB_DTYPE dtype,
              const char *reducer, double *out)
{
    double q;
    double qarr[1];
    double qout[1];
    const char *p;
    char *endptr;

    if (arr == NULL || out == NULL || narr == 0) {
        return -1;
    }
    if (reducer == NULL || reducer[0] == '\0' ||
        strcmp(reducer, "mean") == 0) {
        return tpb_stat_mean(arr, narr, dtype, out) == TPBE_SUCCESS ? 0 : -1;
    }
    if (strcmp(reducer, "min") == 0) {
        return tpb_stat_min(arr, narr, dtype, out) == TPBE_SUCCESS ? 0 : -1;
    }
    if (strcmp(reducer, "max") == 0) {
        return tpb_stat_max(arr, narr, dtype, out) == TPBE_SUCCESS ? 0 : -1;
    }
    if (strcmp(reducer, "median") == 0 || strcmp(reducer, "p50") == 0) {
        qarr[0] = 0.50;
        if (tpb_stat_qtile_1d(arr, narr, dtype, qarr, 1, qout) != TPBE_SUCCESS) {
            return -1;
        }
        *out = qout[0];
        return 0;
    }
    /* p25 / p90 / ... or Q=25 / Q=0.25 */
    p = reducer;
    if ((p[0] == 'p' || p[0] == 'P') && isdigit((unsigned char)p[1])) {
        q = strtod(p + 1, &endptr);
        if (endptr == p + 1) {
            return -1;
        }
        if (q > 1.0) {
            q /= 100.0;
        }
        qarr[0] = q;
        if (tpb_stat_qtile_1d(arr, narr, dtype, qarr, 1, qout) != TPBE_SUCCESS) {
            return -1;
        }
        *out = qout[0];
        return 0;
    }
    if ((p[0] == 'Q' || p[0] == 'q') && p[1] == '=') {
        q = strtod(p + 2, &endptr);
        if (endptr == p + 2) {
            return -1;
        }
        if (q > 1.0) {
            q /= 100.0;
        }
        qarr[0] = q;
        if (tpb_stat_qtile_1d(arr, narr, dtype, qarr, 1, qout) != TPBE_SUCCESS) {
            return -1;
        }
        *out = qout[0];
        return 0;
    }
    /* Unknown reducer: fall back to mean. */
    return tpb_stat_mean(arr, narr, dtype, out) == TPBE_SUCCESS ? 0 : -1;
}

/**
 * @brief Load batch vresults from rafdb task payloads linked by tbatch TaskID.
 *
 * Saves TBatchID must be captured before end_batch. Matches YAML v: names to
 * output header local names. Multiple entry-point tasks (derive_to all-zero)
 * are aggregated by mean of each reducer's scalar.
 */
static int
load_metrics_from_rafdb(const char *workspace,
                        const unsigned char tbatch_id[20],
                        tpb_bench_batch_t *batch)
{
    tbatch_attr_t tattr;
    void *tdata = NULL;
    uint64_t tdatasize = 0;
    const void *task_ids_blob = NULL;
    uint64_t task_ids_bytes = 0;
    uint32_t ntasks;
    uint32_t ti;
    int err;
    int i;
    int task_hdr_idx = -1;
    double *sums = NULL;
    int *counts = NULL;

    if (workspace == NULL || tbatch_id == NULL || batch == NULL) {
        TPB_FAIL(TPB_MOD_CLI_BENCHMARK, TPBE_NULLPTR_ARG, NULL);
    }

    for (i = 0; i < batch->nvspecs; i++) {
        batch->vresults[i] = NAN;
    }

    memset(&tattr, 0, sizeof(tattr));
    err = tpb_raf_record_read_tbatch(workspace, tbatch_id, &tattr, &tdata,
                                     &tdatasize);
    if (err != TPBE_SUCCESS) {
        TPB_PROPAGATE(TPB_MOD_CLI_BENCHMARK, err, "read_tbatch");
    }

    for (ti = 0; ti < tattr.nheader; ti++) {
        if (strcmp(tattr.headers[ti].name, "TaskID") == 0) {
            task_hdr_idx = (int)ti;
            break;
        }
    }
    if (task_hdr_idx < 0) {
        tpb_raf_free_headers(tattr.headers, tattr.nheader);
        free(tdata);
        TPB_FAIL(TPB_MOD_CLI_BENCHMARK, TPBE_LIST_NOT_FOUND,
                 "tbatch missing TaskID header");
    }

    err = tpb_raf_header_data_ptr(tattr.headers, tattr.nheader, tdata,
                                  tdatasize, (uint32_t)task_hdr_idx,
                                  &task_ids_blob, &task_ids_bytes);
    if (err != TPBE_SUCCESS) {
        tpb_raf_free_headers(tattr.headers, tattr.nheader);
        free(tdata);
        TPB_PROPAGATE(TPB_MOD_CLI_BENCHMARK, err, "TaskID payload");
    }

    ntasks = (uint32_t)(task_ids_bytes / 20u);
    sums = (double *)calloc((size_t)batch->nvspecs, sizeof(double));
    counts = (int *)calloc((size_t)batch->nvspecs, sizeof(int));
    if (sums == NULL || counts == NULL) {
        free(sums);
        free(counts);
        tpb_raf_free_headers(tattr.headers, tattr.nheader);
        free(tdata);
        TPB_FAIL(TPB_MOD_CLI_BENCHMARK, TPBE_MALLOC_FAIL, NULL);
    }

    for (ti = 0; ti < ntasks; ti++) {
        const unsigned char *tid =
            (const unsigned char *)task_ids_blob + (size_t)ti * 20u;
        task_attr_t attr;
        void *data = NULL;
        uint64_t datasize = 0;
        uint32_t hi;

        memset(&attr, 0, sizeof(attr));
        err = tpb_raf_record_read_task(workspace, tid, &attr, &data, &datasize);
        if (err != TPBE_SUCCESS) {
            continue;
        }
        /* Prefer entry-point tasks (not capsule members). */
        if (!is_zero_id(attr.derive_to)) {
            tpb_raf_free_headers(attr.headers, attr.nheader);
            free(data);
            continue;
        }

        for (i = 0; i < batch->nvspecs; i++) {
            for (hi = attr.ninput; hi < attr.ninput + attr.noutput; hi++) {
                const void *payload = NULL;
                uint64_t nbytes = 0;
                size_t esz = 0;
                size_t narr = 0;
                double val = NAN;
                TPB_DTYPE dtype;

                if (hi >= attr.nheader) {
                    break;
                }
                if (strcmp(attr.headers[hi].name, batch->vspecs[i].name) != 0) {
                    continue;
                }
                err = tpb_raf_header_data_ptr(attr.headers, attr.nheader, data,
                                              datasize, hi, &payload, &nbytes);
                if (err != TPBE_SUCCESS || payload == NULL || nbytes == 0) {
                    break;
                }
                dtype = (TPB_DTYPE)attr.headers[hi].type_bits;
                if (tpb_dtype_elem_size(dtype, &esz) != 0 || esz == 0) {
                    break;
                }
                if (header_narr(&attr.headers[hi], esz, &narr) != 0 ||
                    narr == 0) {
                    narr = (size_t)(nbytes / esz);
                }
                if (apply_reducer((void *)payload, narr, dtype,
                                  batch->vspecs[i].reducer, &val) == 0 &&
                    !isnan(val)) {
                    sums[i] += val;
                    counts[i] += 1;
                }
                break;
            }
        }

        tpb_raf_free_headers(attr.headers, attr.nheader);
        free(data);
    }

    tpb_raf_free_headers(tattr.headers, tattr.nheader);
    free(tdata);

    {
        int all_found = 1;

        for (i = 0; i < batch->nvspecs; i++) {
            if (counts[i] > 0) {
                batch->vresults[i] = sums[i] / (double)counts[i];
            } else {
                tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN,
                                TPBLOG_FLAG_TSTAG,
                                "Metric '%s' not found in rafdb for batch '%s'\n",
                                batch->vspecs[i].name, batch->id);
                all_found = 0;
            }
        }
        free(sums);
        free(counts);
        if (all_found) {
            return TPBE_SUCCESS;
        }
        TPB_FAIL(TPB_MOD_CLI_BENCHMARK, TPBE_METRIC_MISSING, NULL);
    }
}

/**
 * @brief Run a single batch entry.
 */
static int
run_batch(tpb_bench_batch_t *batch, int *tbatch_fatal)
{
    int err;
    int rec_err;
    int nhdl;
    unsigned char tbatch_id[20];
    char tbatch_hex[41];
    const char *workspace = NULL;
    int kernel_err = 0;

    if (batch == NULL) {
        TPB_FAIL(TPB_MOD_CLI_BENCHMARK, TPBE_NULLPTR_ARG, NULL);
    }
    if (tbatch_fatal != NULL) {
        *tbatch_fatal = 0;
    }
    
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, "\n");
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG, 
               "Running batch: %s (kernel: %s)\n", batch->id, batch->kernel);
    
    /* Print the command that would be run */
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, "Command: tpbcli run --kernel %s", batch->kernel);
    if (batch->kargs[0] != '\0') {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, ":%s", batch->kargs);
    }
    if (batch->wrapper[0] != '\0') {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, " --wrapper %s", batch->wrapper);
        if (batch->wrapper_args[0] != '\0') {
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, " --wrapper-args '%s'",
                       batch->wrapper_args);
        }
    }
    if (batch->kenvs[0] != '\0') {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, " --kenvs \"%s\"", batch->kenvs);
    }
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, "\n");
    
    /* Add handle for this kernel */
    err = tpb_driver_add_handle(batch->kernel);
    if (err != 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG, 
                   "Failed to add handle for kernel: %s\n", batch->kernel);
        TPB_PROPAGATE(TPB_MOD_CLI_BENCHMARK, err, "tpb_driver_add_handle");
    }
    
    /* Set kernel arguments (format: key=val:key=val) */
    if (batch->kargs[0] != '\0') {
        char kargs_copy[2048];
        strncpy(kargs_copy, batch->kargs, sizeof(kargs_copy) - 1);
        kargs_copy[sizeof(kargs_copy) - 1] = '\0';
        
        char *saveptr;
        char *token = strtok_r(kargs_copy, ":", &saveptr);
        while (token != NULL) {
            char *eq = strchr(token, '=');
            if (eq != NULL) {
                *eq = '\0';
                char *key = token;
                char *val = eq + 1;
                
                err = tpb_driver_set_hdl_karg(key, val);
                if (err != 0) {
                    tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN, TPBLOG_FLAG_TSTAG, 
                               "Failed to set karg %s=%s\n", key, val);
                }
            }
            token = strtok_r(NULL, ":", &saveptr);
        }
    }
    
    /* Set wrapper chain */
    if (batch->wrapper[0] != '\0') {
        tpb_wrapper_link_t link;

        memset(&link, 0, sizeof(link));
        snprintf(link.app, TPBM_NAME_STR_MAX_LEN, "%s", batch->wrapper);
        if (batch->wrapper_args[0] != '\0') {
            link.args = batch->wrapper_args;
        }
        err = tpb_driver_set_hdl_wrappers(&link, 1);
        if (err != 0) {
            tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN, TPBLOG_FLAG_TSTAG,
                       "Failed to set wrapper: %s\n", batch->wrapper);
        }
    }
    
    /* Set environment variables (format: KEY=VAL,KEY=VAL or KEY=VAL KEY=VAL) */
    if (batch->kenvs[0] != '\0') {
        char kenvs_copy[1024];
        strncpy(kenvs_copy, batch->kenvs, sizeof(kenvs_copy) - 1);
        kenvs_copy[sizeof(kenvs_copy) - 1] = '\0';
        
        char *saveptr;
        char *token = strtok_r(kenvs_copy, ", ", &saveptr);
        while (token != NULL) {
            char *eq = strchr(token, '=');
            if (eq != NULL) {
                *eq = '\0';
                char *key = token;
                char *val = eq + 1;
                
                err = tpb_driver_set_hdl_env(key, val);
                if (err != 0) {
                    tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN, TPBLOG_FLAG_TSTAG, 
                               "Failed to set env %s=%s\n", key, val);
                }
            }
            token = strtok_r(NULL, ", ", &saveptr);
        }
    }
    
    rec_err = tpb_record_begin_batch(TPB_BATCH_TYPE_BENCHMARK);
    if (rec_err != 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                   "tpbcli_benchmark: begin_batch failed (%d)\n", rec_err);
        if (tbatch_fatal != NULL) {
            *tbatch_fatal = 1;
        }
        TPB_PROPAGATE(TPB_MOD_CLI_BENCHMARK, rec_err, "tpb_record_begin_batch");
    }

    /* Capture TBatchID before end_batch clears the active-batch flag. */
    {
        const char *hex = tpb_record_get_tbatch_id_hex();
        if (hex == NULL || tpb_raf_hex_to_id(hex, tbatch_id) != 0) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                       "tpbcli_benchmark: cannot capture TBatchID\n");
            TPB_FAIL(TPB_MOD_CLI_BENCHMARK, TPBE_FILE_IO_FAIL, NULL);
        }
        snprintf(tbatch_hex, sizeof(tbatch_hex), "%s", hex);
    }
    workspace = tpb_record_get_workspace();
    if (workspace == NULL) {
        workspace = getenv("TPB_WORKSPACE");
    }

    /* Run the kernel */
    err = tpb_driver_run_all();
    if (err != 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                   "Batch %s execution failed\n", batch->id);
        kernel_err = err;
    }

    nhdl = tpb_get_nhdl();
    rec_err = tpb_record_end_batch(nhdl);
    if (rec_err != 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                   "tpbcli_benchmark: end_batch failed (%d)\n", rec_err);
        if (tbatch_fatal != NULL) {
            *tbatch_fatal = 1;
        }
        TPB_PROPAGATE(TPB_MOD_CLI_BENCHMARK, rec_err, "tpb_record_end_batch");
    }

    TPB_PROPAGATE(TPB_MOD_CLI_BENCHMARK, kernel_err, NULL);

    /* Score inputs come from rafdb task payloads, not the human-readable log. */
    if (workspace != NULL) {
        err = load_metrics_from_rafdb(workspace, tbatch_id, batch);
        if (err != 0 && TPBE_CAUSE(err) != TPBE_METRIC_MISSING) {
            TPB_PROPAGATE(TPB_MOD_CLI_BENCHMARK, err, "load_metrics_from_rafdb");
        }
    } else {
        tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN, TPBLOG_FLAG_TSTAG,
                   "Workspace not available for rafdb metric load\n");
    }
    
    /* Print parsed values */
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG, 
               "Parsed values for batch %s:\n", batch->id);
    for (int i = 0; i < batch->nvspecs; i++) {
        if (isnan(batch->vresults[i])) {
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, "  %s: N/A\n", batch->vspecs[i].name);
        } else {
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, "  %s: %.6g\n", 
                       batch->vspecs[i].name, batch->vresults[i]);
        }
    }
    
    return TPBE_SUCCESS;
}

/* Public Function Implementations */

int
tpbcli_benchmark(int argc, char **argv)
{
    int err = 0;
    char suite_path[PATH_MAX] = {0};
    const char *suite_arg = NULL;
    tpb_benchmark_t bench;
    
    /* Parse command line arguments */
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--suite") == 0) {
            if (i + 1 >= argc) {
                tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, 
                           "Option --suite requires an argument.\n");
                TPB_FAIL(TPB_MOD_CLI_BENCHMARK, TPBE_CLI_FAIL, NULL);
            }
            i++;
            suite_arg = argv[i];
            
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                       "Usage: tpbcli benchmark --suite <path_or_name>\n"
                       "\nOptions:\n"
                       "  --suite <path>  Path to benchmark YAML file, or suite name\n"
                       "                  (looks in ${TPB_HOME}/etc/ if name given)\n"
                       "  -h, --help      Show this help message\n");
            return TPBE_EXIT_ON_HELP;
            
        } else {
            tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, 
                       "Unknown option: %s\n", argv[i]);
            TPB_FAIL(TPB_MOD_CLI_BENCHMARK, TPBE_CLI_FAIL, NULL);
        }
    }
    
    if (suite_arg == NULL) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, 
                   "No suite specified. Use --suite <path_or_name>.\n");
        TPB_FAIL(TPB_MOD_CLI_BENCHMARK, TPBE_CLI_FAIL, NULL);
    }
    
    /* Resolve suite path */
    err = resolve_suite_path(suite_arg, suite_path, sizeof(suite_path));
    TPB_PROPAGATE(TPB_MOD_CLI_BENCHMARK, err, NULL);
    
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG, 
               "Loading benchmark suite: %s\n", suite_path);
    
    /* Load and parse YAML */
    err = tpb_bench_yaml_load(suite_path, &bench);
    TPB_PROPAGATE(TPB_MOD_CLI_BENCHMARK, err, NULL);
    
    /* Print benchmark info */
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, "Benchmark: %s\n", bench.name);
    if (bench.note[0] != '\0') {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, "Description: %s\n", bench.note);
    }
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, "Batches: %d, Scores: %d\n", 
               bench.nbatches, bench.nscores);
    
    /* Set default timer (same as tpbcli-run) */
    err = tpb_argp_set_timer("clock_gettime");
    TPB_PROPAGATE(TPB_MOD_CLI_BENCHMARK, err, "At tpbcli-benchmark.c: tpb_argp_set_timer");
    
    /* Initialize kernel registry */
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG, "Initializing TPBench kernels.\n");
    err = tpb_register_kernel();
    TPB_PROPAGATE(TPB_MOD_CLI_BENCHMARK, err, "At tpbcli-benchmark.c: tpb_register_kernel");
    
    /* Run each batch - kernel registry persists across batches */
    for (int i = 0; i < bench.nbatches; i++) {
        int tbatch_fatal = 0;

        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, "\n=== Batch %d/%d: %s ===\n", 
                   i + 1, bench.nbatches, bench.batches[i].id);
        
        /* Reset handles before each batch (except first) to clear previous state */
        if (i > 0) {
            tpb_driver_reset_handles();
        }
        
        err = run_batch(&bench.batches[i], &tbatch_fatal);
        if (tbatch_fatal) {
            tpb_bench_yaml_free(&bench);
            TPB_PROPAGATE(TPB_MOD_CLI_BENCHMARK, err, "run_batch");
        }
        if (err != 0 && TPBE_CAUSE(err) != TPBE_METRIC_MISSING) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG, 
                       "Batch %s failed with error %d\n", 
                       bench.batches[i].id, err);
            /* Continue with other batches even if one fails */
        }
    }
    
    /* Calculate scores */
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, "\n");
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG, "Calculating scores...\n");
    err = tpb_bench_score_calculate_all(&bench);
    if (err != 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN, TPBLOG_FLAG_TSTAG, 
                   "Score calculation had errors\n");
    }
    
    /* Display scores */
    tpb_bench_score_display(&bench);
    
    /* Cleanup */
    tpb_bench_yaml_free(&bench);
    
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG, "Benchmark complete.\n");
    
    return TPBE_SUCCESS;
}
