/**
 * @file tpb-cli.h
 * @brief Header for tpbench command line parser.
 */

#ifndef TPB_CLI_H
#define TPB_CLI_H

#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "tpb-types.h"
#include "tpb-impl.h"

/**
 * @brief Parse command line arguments.
 * @param argc Argument count from main().
 * @param argv Argument vector from main().
 * @param tpb_args CLI run-time args data structure.
 * @param kernel_handles_out Pointer to runtime handle array (allocated by parser).
 * @return 0 on success, error code otherwise.
 */
int tpb_parse_args(int argc, char **argv, tpb_args_t *tpb_args,
                   tpb_k_rthdl_t **kernel_handles_out);

/**
 * @brief Validate kernel-specific arguments against supported parameters.
 * @param common_tokens Common tokens parsed from CLI.
 * @param ncommon Number of common tokens.
 * @param kernel_tokens Kernel-specific tokens parsed from CLI.
 * @param nkernel Number of kernel-specific tokens.
 * @param kernel Pointer to kernel definition.
 * @param rt_parms_out Output: runtime parameters array.
 * @param nparms_out Output: number of parameters.
 * @return 0 on success, error code on validation failure.
 */
int tpb_check_kargs(char **common_tokens, int ncommon,
                    char **kernel_tokens, int nkernel,
                    tpb_kernel_t *kernel,
                    tpb_rt_parm_t **rt_parms_out, int *nparms_out);

/**
 * @brief Print help document and exit.
 * @param progname Program name (argv[0]).
 */
void tpb_print_help(const char *progname);

/**
 * @brief Print overall help document and exit.
 */
void tpb_print_help_callback_overall(void);

#endif /* TPB_CLI_H */
