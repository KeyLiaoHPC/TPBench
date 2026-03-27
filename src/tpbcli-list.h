/**
 * @file tpbcli-list.h
 * @brief Header for TPBench CLI kernel list output.
 */

#ifndef TPBCLI_LIST_H
#define TPBCLI_LIST_H

/**
 * @brief Print the integrated kernel table (assumes tpb_register_kernel() already ran).
 * @param argc Argument count from main().
 * @param argv Argument vector from main().
 * @return 0 on success, error code otherwise.
 */
int tpbcli_kernel_list(int argc, char **argv);

#endif /* TPBCLI_LIST_H */
