/**
 * @file tpbcli-kernel.h
 * @brief TPBench CLI `kernel` subcommand (kernel management).
 */

#ifndef TPBCLI_KERNEL_H
#define TPBCLI_KERNEL_H

/**
 * @brief Dispatch `tpbcli kernel` / `tpbcli k`: refresh kernel records, then subcommand.
 * @param argc Argument count from main().
 * @param argv Argument vector from main(); argv[1] is "kernel" or "k".
 * @return 0 on success, error code otherwise.
 */
int tpbcli_kernel(int argc, char **argv);

#endif /* TPBCLI_KERNEL_H */
