/**
 * @file tpbcli-kernel-build.h
 * @brief `tpbcli kernel build` — compile out-of-tree kernel and register metadata.
 */

#ifndef TPBCLI_KERNEL_BUILD_H
#define TPBCLI_KERNEL_BUILD_H

/**
 * @brief Build an out-of-tree kernel and install libtpbk_<name>.so into TPB_HOME.
 * @param argc Argument count from main().
 * @param argv Argument vector; argv[2] is "build".
 * @return 0 on success, error code otherwise.
 */
int tpbcli_kernel_build(int argc, char **argv);

#endif /* TPBCLI_KERNEL_BUILD_H */
