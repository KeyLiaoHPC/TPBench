/**
 * @file tpbcli-kernel-table.h
 * @brief Fixed-width table printing with wrapping and hyphenation.
 */

#ifndef TPBCLI_KERNEL_TABLE_H
#define TPBCLI_KERNEL_TABLE_H

#include <stdio.h>

#define TPBCLI_KERNEL_TABLE_MAX_COLS  8

/**
 * @brief Print a table header row.
 * @param out Output stream.
 * @param headers Column titles.
 * @param widths Column widths.
 * @param ncols Number of columns.
 * @param gap Spaces between columns.
 */
void tpbcli_kernel_table_print_header(FILE *out,
                                      const char *headers[],
                                      const int *widths,
                                      int ncols,
                                      int gap);

/**
 * @brief Print one table row; cells may wrap with hyphenation.
 * @param out Output stream.
 * @param cells Cell text pointers.
 * @param widths Column widths.
 * @param ncols Number of columns.
 * @param gap Spaces between columns.
 */
void tpbcli_kernel_table_print_row(FILE *out,
                                   const char *cells[],
                                   const int *widths,
                                   int ncols,
                                   int gap);

#endif /* TPBCLI_KERNEL_TABLE_H */
