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
 * @param argc Argument count from main()
 * @param argv Argument vector from main()
 * @param tpb_args CLI run-time args data structure
 * @param tpb_kargs_common CLI run-time args data structure
 * @param timer Pointer to timer data structure
 * @return Error code (0 on success)
 */
int tpb_parse_args( int argc, 
                    char **argv, 
                    tpb_args_t *tpb_args, 
                    tpb_kargs_common_t *tpb_kargs, 
                    tpb_timer_t *timer);

/**
 * @brief Check syntax and count segments in kernel string.
 * @param n Pointer to store segment count
 * @param strarg Kernel string argument
 * @return Error code (0 on success)
 */
int tpb_check_count(int *n, char *strarg);

/**
 * @brief Parse kernel list string into kernel IDs.
 * @param tpb_args Pointer to argument data structure
 * @return Error code (0 on success)
 */
int tpb_parse_klist(tpb_args_t *tpb_args);

/**
 * @brief Tokenize argument string into key-value pairs.
 * @param argstr Argument string to parse
 * @param karg_token Pointer to token structure to fill
 * @return Error code (0 on success, negative on failure)
 */
int tpb_argstr_token(const char *argstr, tpb_kargs_token_t *karg_token);

/**
 * @brief Free memory allocated for kargs token structure.
 * @param karg_token Pointer to token structure to free
 */
void tpb_argstr_token_free(tpb_kargs_token_t *karg_token);

/**
 * @brief Parse default kernel arguments string.
 * @param tpb_args Pointer to argument structure containing kargstr
 * @param tpb_kargs_common Pointer to common kernel arguments
 * @return Error code (0 on success)
 */
int tpb_parse_kargs_common(tpb_args_t *tpb_args, tpb_kargs_common_t *tpb_kargs);

/**
 * @brief Validate kernel-specific arguments against supported parameters.
 * @param kargs_user Pointer to user kernel arguments token structure
 * @param kernel_id ID of the kernel to validate against
 * @param kargs_common Pointer to common kernel arguments to update
 * @return Error code (0 on success, error on unsupported kernel-specific arg)
 */
int tpb_validate_kernel_args(tpb_kargs_token_t *kargs_user, int kernel_id, 
                              tpb_kargs_common_t *kargs_common);

/**
 * @brief Print help document and exit.
 * @param progname Program name (argv[0])
 */
void tpb_print_help(const char *progname);

/**
 * @brief Print help document and exit.
 * @param argc Argument count from main()
 * @param argv Argument vector from main()
 */
void tpb_print_help_callback_overall(void);

#endif
