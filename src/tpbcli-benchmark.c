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
#include "tpbcli-benchmark.h"
#include "tpb-bench-yaml.h"
#include "tpb-bench-score.h"
#include "corelib/tpb-io.h"
#include "corelib/tpb-driver.h"
#include "corelib/tpb-dynloader.h"
#include "corelib/tpb-argp.h"

/* Error handling macro */
#define __tpbm_exit_on_error(e, x) \
    do { \
        if ((e) != 0) { \
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, \
                       "%s, errcode=%d\n", x, (e)); \
            return (e); \
        } \
    } while(0)

/* Local Function Prototypes */
static int resolve_suite_path(const char *suite_arg, char *suite_path, size_t path_size);
static int run_batch(tpb_bench_batch_t *batch);
static int parse_log_for_metrics(const char *log_path, tpb_bench_batch_t *batch);
static double parse_result_value(const char *str);

/* Local Function Implementations */

/**
 * @brief Resolve suite argument to full path.
 * 
 * Checks if argument is an existing file path, otherwise looks in ${TPB_DIR}/etc/
 */
static int
resolve_suite_path(const char *suite_arg, char *suite_path, size_t path_size)
{
    if (suite_arg == NULL || suite_path == NULL) {
        return TPBE_NULLPTR_ARG;
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
    
    /* Look in ${TPB_DIR}/etc/ */
    const char *tpb_dir = tpb_dl_get_tpb_dir();
    if (tpb_dir != NULL && strlen(tpb_dir) > 0) {
        snprintf(suite_path, path_size, "%s/etc/%s", tpb_dir, suite_arg);
        if (access(suite_path, F_OK) == 0) {
            return TPBE_SUCCESS;
        }
        
        /* Try with .yml extension */
        snprintf(suite_path, path_size, "%s/etc/%s.yml", tpb_dir, suite_arg);
        if (access(suite_path, F_OK) == 0) {
            return TPBE_SUCCESS;
        }
    }
    
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, 
               "Suite file not found: %s\n", suite_arg);
    return TPBE_FILE_IO_FAIL;
}

/**
 * @brief Parse a result value string to double.
 */
static double
parse_result_value(const char *str)
{
    if (str == NULL) return NAN;
    
    /* Skip leading whitespace */
    while (*str && isspace((unsigned char)*str)) str++;
    
    if (*str == '\0' || strcmp(str, "N/A") == 0) {
        return NAN;
    }
    
    char *endptr;
    double val = strtod(str, &endptr);
    
    /* Check for parse errors */
    if (endptr == str) {
        return NAN;
    }
    
    return val;
}

/**
 * @brief Parse log file for metrics specified in batch.
 */
static int
parse_log_for_metrics(const char *log_path, tpb_bench_batch_t *batch)
{
    FILE *fp;
    char line[4096];
    char current_metric[256] = {0};
    int found_metric = 0;
    
    if (log_path == NULL || batch == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    
    /* Initialize all vresults to NAN */
    for (int i = 0; i < batch->nvspecs; i++) {
        batch->vresults[i] = NAN;
    }
    
    fp = fopen(log_path, "r");
    if (fp == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, 
                   "Cannot open log file: %s\n", log_path);
        return TPBE_FILE_IO_FAIL;
    }
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        /* Check for metrics section: "### Metrics: <name>" */
        if (strncmp(line, "### Metrics:", 12) == 0) {
            char *name_start = line + 12;
            while (*name_start && isspace((unsigned char)*name_start)) name_start++;
            
            /* Copy metric name, removing trailing newline */
            strncpy(current_metric, name_start, sizeof(current_metric) - 1);
            current_metric[sizeof(current_metric) - 1] = '\0';
            char *nl = strchr(current_metric, '\n');
            if (nl) *nl = '\0';
            
            found_metric = 1;
            continue;
        }
        
        /* Check for result values when in a metric section */
        if (found_metric) {
            double value = NAN;
            int got_value = 0;
            
            /* Check for "Result mean: <value>" */
            if (strncmp(line, "Result mean:", 12) == 0) {
                value = parse_result_value(line + 12);
                got_value = 1;
            }
            /* Check for "Result value: <value>" */
            else if (strncmp(line, "Result value:", 13) == 0) {
                value = parse_result_value(line + 13);
                got_value = 1;
            }
            
            if (got_value) {
                /* Find matching vspec and store value */
                for (int i = 0; i < batch->nvspecs; i++) {
                    if (strcmp(batch->vspecs[i].name, current_metric) == 0) {
                        batch->vresults[i] = value;
                        break;
                    }
                }
                found_metric = 0;  /* Reset for next metric */
            }
            
            /* Reset if we hit another section */
            if (line[0] == '#') {
                found_metric = 0;
            }
        }
    }
    
    fclose(fp);
    
    /* Check if all required metrics were found */
    int all_found = 1;
    for (int i = 0; i < batch->nvspecs; i++) {
        if (isnan(batch->vresults[i])) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN, 
                       "Metric '%s' not found in log for batch '%s'\n",
                       batch->vspecs[i].name, batch->id);
            all_found = 0;
        }
    }
    
    return all_found ? TPBE_SUCCESS : TPBE_WARN;
}

/**
 * @brief Run a single batch entry.
 */
static int
run_batch(tpb_bench_batch_t *batch)
{
    int err;
    
    if (batch == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    
    tpb_printf(TPBM_PRTN_M_DIRECT, "\n");
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, 
               "Running batch: %s (kernel: %s)\n", batch->id, batch->kernel);
    
    /* Print the command that would be run */
    tpb_printf(TPBM_PRTN_M_DIRECT, "Command: tpbcli run --kernel %s", batch->kernel);
    if (batch->kargs[0] != '\0') {
        tpb_printf(TPBM_PRTN_M_DIRECT, ":%s", batch->kargs);
    }
    if (batch->kmpiargs[0] != '\0') {
        tpb_printf(TPBM_PRTN_M_DIRECT, " --kmpiargs \"%s\"", batch->kmpiargs);
    }
    if (batch->kenvs[0] != '\0') {
        tpb_printf(TPBM_PRTN_M_DIRECT, " --kenvs \"%s\"", batch->kenvs);
    }
    tpb_printf(TPBM_PRTN_M_DIRECT, "\n");
    
    /* Add handle for this kernel */
    err = tpb_driver_add_handle(batch->kernel);
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, 
                   "Failed to add handle for kernel: %s\n", batch->kernel);
        return err;
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
                    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN, 
                               "Failed to set karg %s=%s\n", key, val);
                }
            }
            token = strtok_r(NULL, ":", &saveptr);
        }
    }
    
    /* Set MPI arguments */
    if (batch->kmpiargs[0] != '\0') {
        err = tpb_driver_set_hdl_mpiargs(batch->kmpiargs);
        if (err != 0) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN, 
                       "Failed to set mpiargs: %s\n", batch->kmpiargs);
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
                    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN, 
                               "Failed to set env %s=%s\n", key, val);
                }
            }
            token = strtok_r(NULL, ", ", &saveptr);
        }
    }
    
    /* Run the kernel */
    err = tpb_driver_run_all();
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, 
                   "Batch %s execution failed\n", batch->id);
        return err;
    }
    
    /* Parse log file for metrics */
    const char *log_path = tpb_log_get_filepath();
    if (log_path != NULL) {
        err = parse_log_for_metrics(log_path, batch);
        if (err != 0 && err != TPBE_WARN) {
            return err;
        }
    } else {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN, 
                   "Log file not available for parsing\n");
    }
    
    /* Print parsed values */
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, 
               "Parsed values for batch %s:\n", batch->id);
    for (int i = 0; i < batch->nvspecs; i++) {
        if (isnan(batch->vresults[i])) {
            tpb_printf(TPBM_PRTN_M_DIRECT, "  %s: N/A\n", batch->vspecs[i].name);
        } else {
            tpb_printf(TPBM_PRTN_M_DIRECT, "  %s: %.6g\n", 
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
                tpb_printf(TPBM_PRTN_M_DIRECT, 
                           "Option --suite requires an argument.\n");
                return TPBE_CLI_FAIL;
            }
            i++;
            suite_arg = argv[i];
            
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            tpb_printf(TPBM_PRTN_M_DIRECT,
                       "Usage: tpbcli benchmark --suite <path_or_name>\n"
                       "\nOptions:\n"
                       "  --suite <path>  Path to benchmark YAML file, or suite name\n"
                       "                  (looks in ${TPB_DIR}/etc/ if name given)\n"
                       "  -h, --help      Show this help message\n");
            return TPBE_EXIT_ON_HELP;
            
        } else {
            tpb_printf(TPBM_PRTN_M_DIRECT, 
                       "Unknown option: %s\n", argv[i]);
            return TPBE_CLI_FAIL;
        }
    }
    
    if (suite_arg == NULL) {
        tpb_printf(TPBM_PRTN_M_DIRECT, 
                   "No suite specified. Use --suite <path_or_name>.\n");
        return TPBE_CLI_FAIL;
    }
    
    /* Resolve suite path */
    err = resolve_suite_path(suite_arg, suite_path, sizeof(suite_path));
    if (err != 0) {
        return err;
    }
    
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, 
               "Loading benchmark suite: %s\n", suite_path);
    
    /* Load and parse YAML */
    err = tpb_bench_yaml_load(suite_path, &bench);
    if (err != 0) {
        return err;
    }
    
    /* Print benchmark info */
    tpb_printf(TPBM_PRTN_M_DIRECT, "Benchmark: %s\n", bench.name);
    if (bench.note[0] != '\0') {
        tpb_printf(TPBM_PRTN_M_DIRECT, "Description: %s\n", bench.note);
    }
    tpb_printf(TPBM_PRTN_M_DIRECT, "Batches: %d, Scores: %d\n", 
               bench.nbatches, bench.nscores);
    
    /* Set default timer (same as tpbcli-run) */
    err = tpb_argp_set_timer("clock_gettime");
    __tpbm_exit_on_error(err, "At tpbcli-benchmark.c: tpb_argp_set_timer");
    
    /* Initialize kernel registry */
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Initializing TPBench kernels.\n");
    err = tpb_register_kernel();
    __tpbm_exit_on_error(err, "At tpbcli-benchmark.c: tpb_register_kernel");
    
    /* Run each batch - kernel registry persists across batches */
    for (int i = 0; i < bench.nbatches; i++) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "\n=== Batch %d/%d: %s ===\n", 
                   i + 1, bench.nbatches, bench.batches[i].id);
        
        /* Reset handles before each batch (except first) to clear previous state */
        if (i > 0) {
            tpb_driver_reset_handles();
        }
        
        err = run_batch(&bench.batches[i]);
        if (err != 0 && err != TPBE_WARN) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, 
                       "Batch %s failed with error %d\n", 
                       bench.batches[i].id, err);
            /* Continue with other batches even if one fails */
        }
    }
    
    /* Calculate scores */
    tpb_printf(TPBM_PRTN_M_DIRECT, "\n");
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Calculating scores...\n");
    err = tpb_bench_score_calculate_all(&bench);
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN, 
                   "Score calculation had errors\n");
    }
    
    /* Display scores */
    tpb_bench_score_display(&bench);
    
    /* Cleanup */
    tpb_bench_yaml_free(&bench);
    
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Benchmark complete.\n");
    
    return TPBE_SUCCESS;
}
