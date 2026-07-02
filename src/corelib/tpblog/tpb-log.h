/**
 * @file tpb-log.h
 * @brief Internal run-log session management for the tpblog module.
 */

#ifndef TPB_LOG_MODULE_H
#define TPB_LOG_MODULE_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <limits.h>
#include "tpb-types.h"

/**
 * @brief Environment variable: absolute path of the active run log for PLI children.
 *
 * Set by the driver before fork/exec; read by tpblog_init() to open the log in append
 * mode without writing a second session header.
 */
#define TPBLOG_FILE_ENV "TPB_LOG_FILE"

/** @brief Legacy alias kept during migration. */
#define TPB_LOG_FILE_ENV TPBLOG_FILE_ENV

/** @brief Default terminal width when ioctl fails. */
#define TPBLOG_DEFAULT_WIDTH 85

/** @brief Maximum columns supported by tpblog_printf_c. */
#define TPBLOG_COLUMN_MAX 8

/**
 * @brief Open or reopen the run log for this process.
 * @return TPBE_SUCCESS on success, TPBE_FILE_IO_FAIL on failure.
 */
int tpblog_init(void);

/**
 * @brief Close the active run log file.
 */
void tpblog_cleanup(void);

/**
 * @brief Get the active run log path.
 * @return Path string, or NULL if no log is open.
 */
const char *tpblog_get_filepath(void);

/**
 * @brief Append a fully formatted message to the run log file.
 * @param msg Null-terminated message; no-op when log is closed or msg is NULL.
 */
void _tpblog_write(const char *msg);

/**
 * @brief Return the active run log FILE handle for dual-write helpers.
 * @return Open log FILE*, or NULL.
 */
FILE *_tpblog_file(void);

#endif /* TPB_LOG_MODULE_H */
