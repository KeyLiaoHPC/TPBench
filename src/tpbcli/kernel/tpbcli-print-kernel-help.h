/**
 * @file tpbcli-print-kernel-help.h
 * @brief Formatted kernel help output for `tpbcli run --help` and `kernel get -v`.
 */

#ifndef TPBCLI_PRINT_KERNEL_HELP_H
#define TPBCLI_PRINT_KERNEL_HELP_H

#include <stdio.h>
#include <stdint.h>

#include "tpb-public.h"

/**
 * @brief Print detailed kernel help from a registered kernel object.
 * @param out Output stream.
 * @param kernel Registered kernel (non-NULL).
 * @param kernel_id_hex 40-char KernelID hex, or NULL to use kernel->info.kernel_id.
 * @param active 1 if active variant, 0 otherwise.
 */
void tpbcli_print_kernel_help_from_kernel(FILE *out,
                                          const tpb_kernel_t *kernel,
                                          const char *kernel_id_hex,
                                          int active);

/**
 * @brief Print detailed kernel help from a .tpbr kernel record.
 * @param out Output stream.
 * @param attr Kernel attributes from tpb_raf_record_read_kernel().
 * @param data Record data buffer (may be NULL).
 * @param datasize Size of data buffer in bytes.
 */
void tpbcli_print_kernel_help_from_attr(FILE *out,
                                        const kernel_attr_t *attr,
                                        const void *data,
                                        uint64_t datasize);

/**
 * @brief Print parameter and metric names only (non-verbose `kernel get`).
 * @param attr Kernel attributes from tpb_raf_record_read_kernel().
 */
void tpbcli_print_kernel_names_from_attr(const kernel_attr_t *attr);

#endif /* TPBCLI_PRINT_KERNEL_HELP_H */
