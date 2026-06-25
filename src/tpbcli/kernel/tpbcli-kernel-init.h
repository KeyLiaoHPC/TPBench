/**
 * @file tpbcli-kernel-init.h
 * @brief `tpbcli kernel init` — initialize out-of-tree kernel project.
 */

#ifndef TPBCLI_KERNEL_INIT_H
#define TPBCLI_KERNEL_INIT_H

/**
 * @brief Initialize a kernel development directory from installed templates.
 * @param argc Argument count from main().
 * @param argv Argument vector; argv[2] is "init".
 * @return 0 on success, error code otherwise.
 */
int tpbcli_kernel_init(int argc, char **argv);

#endif /* TPBCLI_KERNEL_INIT_H */
