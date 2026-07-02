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
#include "tpblog/tpb-log.h"

#define DHLINE "==="
#define HLINE  "---"

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

#endif /* TPB_IO_H */
