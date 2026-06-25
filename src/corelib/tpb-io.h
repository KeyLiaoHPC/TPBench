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

/**
 * @brief Environment variable: absolute path of the active run log for PLI children.
 *
 * Set by the driver before fork/exec; read by tpb_log_init() to open the log in append
 * mode without writing a second session header. Cleared at the start of tpb_corelib_init().
 */
#define TPB_LOG_FILE_ENV "TPB_LOG_FILE"

/**
 * @brief Create a directory recursively.
 * @param dirpath Path to the directory.
 * @return 0 on success, -1 on error.
 */
int tpb_mkdir(char *dirpath);

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
 * @brief Open or reopen the run log for this process.
 *
 * If the log FILE* is already open, returns TPBE_SUCCESS. Else if environment variable
 * TPB_LOG_FILE (TPB_LOG_FILE_ENV) is set and non-empty, opens that path in append mode
 * without writing the session banner (shared log between tpbcli parent and PLI child).
 * Otherwise requires workspace from tpb_corelib_init, creates
 * <workspace>/rafdb/log/tpbrunlog_YYYYMMDDThhmmss_<host>.log with mode "w", and writes
 * the session header.
 *
 * @return TPBE_SUCCESS on success, error code on failure (non-fatal to some callers).
 */
int tpb_log_init(void);

/**
 * @brief Cleanup and close the log file.
 */
void tpb_log_cleanup(void);

/**
 * @brief Get the current log file path.
 *
 * @return Path to the log file, or NULL if not initialized.
 */
const char *tpb_log_get_filepath(void);

#endif /* TPB_IO_H */
