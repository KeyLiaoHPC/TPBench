/**
 * @file tpb-bench-score.h
 * @brief Score calculation for benchmark results.
 */

#ifndef TPB_BENCH_SCORE_H
#define TPB_BENCH_SCORE_H

#include "tpb-bench-yaml.h"

/**
 * @brief Calculate all scores in the benchmark.
 * 
 * Uses recursive evaluation to resolve @references between scores
 * and batch results.
 * 
 * @param bench Pointer to benchmark structure with batch results filled.
 * @return 0 on success, error code otherwise.
 */
int tpb_bench_score_calculate_all(tpb_benchmark_t *bench);

/**
 * @brief Resolve a reference string to a numeric value.
 * 
 * Supports:
 *   - @batch[id=='xxx'].v[N] - Get Nth vresult from batch
 *   - @score[id==xxx] - Get score value (recursive)
 * 
 * @param bench Pointer to benchmark structure.
 * @param ref Reference string to resolve.
 * @return Resolved value, or NAN on error.
 */
double tpb_bench_score_resolve_ref(tpb_benchmark_t *bench, const char *ref);

/**
 * @brief Apply a modifier function to an array of arguments.
 * 
 * Modifiers: "raw", "sum", "div", "mul", "mean"
 * 
 * @param modifier Modifier name.
 * @param args Array of argument values.
 * @param nargs Number of arguments.
 * @return Calculated result, or NAN on error.
 */
double tpb_bench_score_apply_modifier(const char *modifier, double *args, int nargs);

/**
 * @brief Display benchmark scores to console/log.
 * 
 * @param bench Pointer to benchmark structure with calculated scores.
 */
void tpb_bench_score_display(tpb_benchmark_t *bench);

#endif /* TPB_BENCH_SCORE_H */
