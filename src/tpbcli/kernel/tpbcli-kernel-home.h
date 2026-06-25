/**
 * @file tpbcli-kernel-home.h
 * @brief Resolve TPB_HOME for `tpbcli kernel build`.
 */

#ifndef TPBCLI_KERNEL_HOME_H
#define TPBCLI_KERNEL_HOME_H

#include <stddef.h>

/**
 * @brief Resolve install root for kernel build operations.
 *
 * Priority: override (from --tpb-home), then $TPB_HOME, then $HOME/.tpbench.
 *
 * @param override Optional path from --tpb-home; NULL or empty to skip.
 * @param out Resolved path buffer.
 * @param outlen Size of out buffer.
 * @return 0 on success, error code otherwise.
 */
int tpbcli_kernel_resolve_home(const char *override, char *out, size_t outlen);

/**
 * @brief Validate kernel name for init/build (C identifier).
 * @param kernel_name Candidate kernel registry name.
 * @return 1 if valid, 0 otherwise.
 */
int tpbcli_kernel_name_valid(const char *kernel_name);

#endif /* TPBCLI_KERNEL_HOME_H */
