/**
 * @file tpb-io.h
 * @brief Header for handling input/output of tpbench.
 */

#ifndef TPB_IO_H
#define TPB_IO_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdint.h>
#include <limits.h>
#include "tpb-types.h"
#include "tpmpi.h"

#define DHLINE "==="
#define HLINE  "---"

#define TPBM_HELP_DOC_TOTAL \
    DHLINE \
    "Usage: tpbench <action> <option>\n" \
    "Actions: run, benchmark, list, help\n" \
    "Options and explanation for each action:\n" \
    HLINE \
    "  run               Run one or more benchmark kernels.\n" \
    "      -k, --kernel <kernel1>:[[kargs0_0]:...:[kargs0_n]],<kernel2>:...\n" \
    "                    Kernel list separated by comma. Each kernel can have\n" \
    "                    multiple kargs separated by colon. (e.g.\n" \
    "                    d_init:memsize=1024,triad:fp=fp64:memsize=1024)\n" \
    "                    Use 'help <kernel>' for available options of a kernel.\n" \
    "          --kargs <key>=<value>\n" \
    "                    Default kernel run-time arguments.\n" \
    "                    Use 'help kargs' for supported keys and values.\n" \
    "      -d, --data_path <PATH>\n" \
    "                    Data directory. Default: ./data\n" \
    "      -t, --timer <timer_name>\n" \
    "                    Timer name. Supported: clock_gettime (default),\n" \
    "                    tsc_asym.\n" \
    "      -h, --help    Print this help message and exit.\n" \
    HLINE \
    "  benchmark         Run predefined benchmark suites.\n" \
    HLINE \
    "  list              List kernels, parameters, implemented routines, etc.\n" \
    "                    of an object then exit.\n" \
    HLINE \
    "  help              Print help message for an object and exit.\n" \
    DHLINE

/**
 * @brief List all registered kernels.
 */
void tpb_list(void);

/**
 * @brief Create a directory recursively.
 * @param dirpath Path to the directory.
 * @return 0 on success, -1 on error.
 */
int tpb_mkdir(char *dirpath);

/**
 * @brief TPBench format stdout module.
 *
 * Print message if tpmpi_info.myrank == 0.
 * Output syntax: YYYY-mm-dd HH:MM:SS [TAG] *msg
 *
 * @param mode_bit Mode bit for message and error header type.
 * @param fmt Format string.
 * @param ... Varargs for fmt printf.
 */
void tpb_printf(uint64_t mode_bit, char *fmt, ...);

/**
 * @brief Print overall help message.
 */
void tpb_print_help_total(void);

/**
 * @brief Write 2D data with header into a csv file.
 * @param path File path.
 * @param data 2D data array [col][row].
 * @param nrow Number of rows.
 * @param ncol Number of columns.
 * @param header CSV header string.
 * @return 0 on success, error code otherwise.
 */
int tpb_writecsv(char *path, int64_t **data, int nrow, int ncol, char *header);

/**
 * @brief Report performance results with quantile statistics.
 */
int report_performance(uint64_t **ns, uint64_t **cy, uint64_t total_wall_time,
                       int nskip, int ntest, int nepoch, int N, int Nr,
                       int skip_comp, int skip_comm);

/**
 * @brief Log every step's performance data into a csv file.
 *
 * The csv file will be named as "np${rank_size}_kernelname_ntest_N{N}.csv"
 */
int log_step_info(uint64_t **ns, uint64_t **cy, char *kernel_name,
                  int ntest, int nepoch, int N, int Nr,
                  int skip_comp, int skip_comm);

/**
 * @brief Output kernel arguments to the command-line interface.
 *
 * This function prints the kernel name and runtime parameter settings.
 *
 * @param handle Pointer to the kernel runtime handle.
 * @return TPBE_SUCCESS on success, TPBE_NULLPTR_ARG if handle is NULL.
 */
int tpb_cliout_args(tpb_k_rthdl_t *handle);

/**
 * @brief Output kernel execution results to the command-line interface.
 *
 * This function prints test results based on the output shape attribute.
 * Casting is controlled per-output via TPB_UATTR_CAST_Y/N in the unit field.
 *
 * @param handle Pointer to the kernel runtime handle.
 * @return TPBE_SUCCESS on success, TPBE_NULLPTR_ARG if handle is NULL.
 */
int tpb_cliout_results(tpb_k_rthdl_t *handle);

#endif /* TPB_IO_H */
