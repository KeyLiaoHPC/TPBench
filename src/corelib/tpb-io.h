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

#define DHLINE "==="
#define HLINE  "---"

#define TPBM_HELP_DOC_TOTAL \
    "tpbcli is the command-line interface of the active launcher of TPBench.\n" \
    "Usage: tpbcli <action> <option>\n" \
    "Action: run/r, benchmark/b, database/d, list/l, help/h\n" \
    "Options and explanation for each action:\n" \
    "    r, run: Run one or more benchmark kernels.\n" \
    "        -P          Use Process-Level Integration mode (default). Kernels run as\n" \
    "                    separate processes via fork/exec.\n" \
    "        -F          Use Function-Level Integration mode. Kernels run as linked\n" \
    "                    functions within the same process.\n" \
    "        --kernel    <kernel_name>\n" \
    "                    Mandatory. Kernel list separated by comma. Each kernel can have\n" \
    "                    multiple kargs separated by colon. (e.g.\n" \
    "                    d_init:total_memsize=1024,triad:fp=fp64:total_memsize=1024)\n" \
    "                    Use 'help <kernel>' for available options of a kernel.\n" \
    "        --kargs     <key>=<value>\n" \
    "                    Optional. Default kernel run-time arguments.\n" \
    "                    Use 'help kargs' for supported keys and values.\n" \
    "        --kargs_dim \n" \
    "                    Optional. Create one or more dimensional arguments, and TPBench will \n" \
    "                    execute the kernel with arguments along the designated dimension. If \n" \
    "                    " \
    "        --workdir   <PATH> (Default: $CWD/workspace)\n" \
    "                    Optional. Path to the directory of the workspace. \n" \
    "        --outdir    <PATH> (Default: $CWD/workspace/<tpbrun-YYYYMMDDThhmmss>)\n" \
    "                    Optional.Path to the data directory of the current test. \n" \
    "        --timer     <timer_name> (Optional, default: clock_gettime)\n" \
    "                    Optional. Timer name. Supported: clock_gettime, tsc_asym.\n" \
    "        -l          List available kernels.\n" \
    "        -h, --help  Print usages of the `tpbcli run` subcommand.\n" \
    "    b, benchmark: Run predefined benchmark suites.\n" \
    "        --suite     <PATH> (Default: $CWD/workspace)\n" \
    "        --workdir   <PATH> (Optional. Default: $CWD/workspace)\n" \
    "                    Path to the directory of the workspace. \n" \
    "        --outdir    <PATH> (Optional. Default: $CWD/workspace/<tpbrun-YYYYMMDDThhmmss>)\n" \
    "                    Path to the data directory of the current test. \n" \
    "        -h, --help  Print usages of the `tpbcli benchmark` subcommand.\n" \
    "    d, database: Read and manage TPBench database.\n" \
    "    l, list: List objects.\n" \
    "        -P          Use Process-Level Integration mode (default).\n" \
    "        -F          Use Function-Level Integration mode.\n" \
    "    help: Print help message for an object and exit.\n" \

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
 * @brief Set output formatting arguments for CLI display.
 *
 * This function sets the unit_cast and sigbit_trim options for output formatting.
 *
 * @param unit_cast Enable/disable unit casting (0 or 1).
 * @param sigbit_trim Significant bits for trimming (default 5, 0=no trim).
 */
void tpb_set_outargs(int unit_cast, int sigbit_trim);

/**
 * @brief Initialize the logging system.
 *
 * Creates ${TPB_DIR}/log directory and opens a timestamped log file.
 * Log filename format: tpbrunlog_YYYYMMDDThhmmss_<hostname>.md
 *
 * @return 0 on success, error code on failure (non-fatal).
 */
int tpb_log_init(void);

/**
 * @brief Cleanup and close the log file.
 */
void tpb_log_cleanup(void);

/**
 * @brief Write output directly to the log file.
 *
 * This is used by PLI kernels to write captured stdout/stderr to the log.
 *
 * @param output String to write to log file.
 */
void tpb_log_write_output(const char *output);

/**
 * @brief Get the current log file path.
 *
 * @return Path to the log file, or NULL if not initialized.
 */
const char *tpb_log_get_filepath(void);

#endif /* TPB_IO_H */
