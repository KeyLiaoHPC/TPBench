/**
 * @file tpb-printf.h
 * @brief Internal helpers for tpblog formatted output.
 */

#ifndef TPB_PRINTF_MODULE_H
#define TPB_PRINTF_MODULE_H

#include <stdint.h>

/**
 * @brief Map legacy TPBE_* severity bits to tpblog tag strings.
 * @param log_type One of TPBLOG_TYPE_* values.
 * @return Tag string such as "INFO", "WARN", or "ERRO".
 */
const char *_tpblog_tag_from_type(uint32_t log_type);

/**
 * @brief Compute proportional column widths for tpblog_printf_c.
 *
 * Algorithm: total width minus gaps, proportional floor allocation, then subtract
 * one character per column for hyphenation. If any final width is below 1, widths
 * are zeroed to signal degenerate line-by-line output.
 *
 * @param total_width Terminal width in characters.
 * @param col_ratios Column ratio array, or NULL for equal ratios.
 * @param ncol Number of columns.
 * @param gap Gap between columns.
 * @param widths_out Output array of length ncol.
 * @return 1 when degenerate mode is required, 0 for table layout.
 */
int _tpblog_compute_column_widths(int total_width, const float *col_ratios,
                                  int ncol, int gap, int *widths_out);

/**
 * @brief Resolve terminal width for column layout.
 *
 * Uses TPBLOG_TEST_WIDTH when set, otherwise ioctl on stdout, otherwise
 * TPBLOG_DEFAULT_WIDTH.
 */
int _tpblog_terminal_width(void);

#endif /* TPB_PRINTF_MODULE_H */
