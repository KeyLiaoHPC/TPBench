/**
 * @file tpb-argp.h
 * @brief Header for TPBench argument parser infrastructure.
 * 
 * This module provides internal utilities for parsing and validating
 * kernel-specific arguments. Most functions are kept internal and
 * are not exposed in this header.
 */

#ifndef TPB_ARGP_H
#define TPB_ARGP_H

#include "tpb-types.h"

/* 
 * Note: Most parser utilities are kept as static/internal functions
 * in tpb-argp.c to minimize the exposed API surface. Only essential
 * functions that need to be shared across CLI subcommands are declared here.
 */

/**
 * @brief Append tokens from comma-separated argument string.
 * @param argstr Argument string with comma-separated values.
 * @param tokens Pointer to token array (allocated dynamically).
 * @param count Pointer to token count.
 * @param cap Pointer to token capacity.
 * @return 0 on success, error code otherwise.
 */
int append_tokens(const char *argstr, char ***tokens, int *count, int *cap);

/**
 * @brief Free token array.
 * @param tokens Token array to free.
 * @param count Number of tokens.
 */
void free_tokens(char **tokens, int count);

/**
 * @brief Validate kernel arguments against kernel parameter definitions.
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
 * @brief Parse comma-separated key=value string and set kernel arguments.
 * @param nchar Number of characters in tokstr (unused, for compatibility).
 * @param tokstr Comma-separated key=value string.
 * @param narg Pointer to argument counter. If not NULL, initialized to 1 and
 *             incremented for each token processed.
 * @return 0 on success, error code on failure. Increments *narg for each token.
 */
int tpb_argp_set_kargs_tokstr(int nchar, char *tokstr, int *narg);

/**
 * @brief Set the timer for the driver by name.
 * @param timer_name Timer name ("clock_gettime", "tsc_asym").
 * @return 0 on success, error code on failure.
 */
int tpb_argp_set_timer(const char *timer_name);

#endif /* TPB_ARGP_H */
