/**
 * @file tpbcli-benchmark.h
 * @brief Header for TPBench CLI benchmark subcommand.
 */

#ifndef TPBCLI_BENCHMARK_H
#define TPBCLI_BENCHMARK_H

/**
 * @brief Execute the benchmark subcommand.
 * @param argc Argument count from main().
 * @param argv Argument vector from main().
 * @return 0 on success, error code otherwise.
 */
int tpbcli_benchmark(int argc, char **argv);

#endif /* TPBCLI_BENCHMARK_H */
