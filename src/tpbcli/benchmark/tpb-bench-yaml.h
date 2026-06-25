/**
 * @file tpb-bench-yaml.h
 * @brief YAML parser for benchmark definition files.
 */

#ifndef TPB_BENCH_YAML_H
#define TPB_BENCH_YAML_H

#include <stdint.h>

/* Maximum limits for benchmark structures */
#define TPB_BENCH_MAX_BATCHES   64
#define TPB_BENCH_MAX_SCORES    64
#define TPB_BENCH_MAX_VSPECS    16
#define TPB_BENCH_MAX_ARGS      32

/**
 * @brief Value specification for benchmark output metrics.
 */
typedef struct tpb_bench_vspec {
    char name[256];           /**< Output metric name */
    char reducer[32];         /**< Reducer: "mean", "median", "max", "min", "Q=xx" */
} tpb_bench_vspec_t;

/**
 * @brief Batch entry in benchmark definition.
 */
typedef struct tpb_bench_batch {
    char id[64];              /**< Unique identifier */
    char note[256];           /**< Description */
    char kernel[64];          /**< Kernel name */
    char kargs[2048];         /**< Formatted as key=val:key=val */
    char kenvs[1024];         /**< Environment variables string */
    char kmpiargs[1024];      /**< MPI arguments string */
    int nvspecs;              /**< Number of value specifications */
    tpb_bench_vspec_t vspecs[TPB_BENCH_MAX_VSPECS];
    double vresults[TPB_BENCH_MAX_VSPECS];  /**< Parsed results from log */
} tpb_bench_batch_t;

/**
 * @brief Score definition in benchmark.
 */
typedef struct tpb_bench_score {
    char id[64];              /**< Score identifier */
    char name[256];           /**< Score display name */
    int display;              /**< 1 to display, 0 to hide */
    char modifier[32];        /**< Modifier: "raw", "sum", "div", "mul", "mean" */
    int nargs;                /**< Number of arguments */
    char args[TPB_BENCH_MAX_ARGS][256];  /**< Reference strings */
    double value;             /**< Calculated value */
    int calculated;           /**< Flag: already calculated */
} tpb_bench_score_t;

/**
 * @brief Complete benchmark definition.
 */
typedef struct tpb_benchmark {
    char name[256];           /**< Benchmark name */
    char note[1024];          /**< Benchmark description */
    int nbatches;             /**< Number of batch entries */
    tpb_bench_batch_t batches[TPB_BENCH_MAX_BATCHES];
    int nscores;              /**< Number of score definitions */
    tpb_bench_score_t scores[TPB_BENCH_MAX_SCORES];
} tpb_benchmark_t;

/**
 * @brief Load and parse a YAML benchmark definition file.
 * 
 * @param path Path to the YAML file.
 * @param bench Pointer to benchmark structure to fill.
 * @return 0 on success, error code otherwise.
 */
int tpb_bench_yaml_load(const char *path, tpb_benchmark_t *bench);

/**
 * @brief Free resources associated with a benchmark structure.
 * 
 * @param bench Pointer to benchmark structure.
 */
void tpb_bench_yaml_free(tpb_benchmark_t *bench);

#endif /* TPB_BENCH_YAML_H */
