/**
 * @file tpb-dynloader.h
 * @brief Dynamic kernel loader for PLI (Process-Level Integration) mode.
 *
 * Scans ${TPB_DIR}/lib for PLI kernel modules (libtpbk_*.so). Each module is
 * registered via dlopen(); execution uses tpbcli-pli-launcher.
 */

#ifndef TPB_DYNLOADER_H
#define TPB_DYNLOADER_H

#include "tpb-types.h"

/**
 * @brief Get the resolved TPB_DIR path.
 * @return Pointer to the TPB_DIR string (static storage).
 */
const char *tpb_dl_get_tpb_dir(void);

/**
 * @brief Scan all PLI kernels under ${TPB_DIR}/lib.
 *
 * For each libtpbk_<name>.so found, attempts dlopen() registration.
 * Scan failures are logged as warnings; successful kernels remain
 * registered.
 *
 * @return 0 on success, error code otherwise.
 */
int tpb_dl_scan(void);

/**
 * @brief Scan one PLI kernel by name.
 *
 * Attempts dlopen() registration on ${TPB_DIR}/lib/libtpbk_<name>.so.
 *
 * @param kernel_name Kernel registry name.
 * @return 0 on success, error code otherwise.
 */
int tpb_dl_scan_kernel(const char *kernel_name);

/**
 * @brief Get the PLI launcher path (${TPB_DIR}/bin/tpbcli-pli-launcher).
 * @return Path to the launcher executable, or NULL if unavailable.
 */
const char *tpb_dl_get_pli_launch_path(void);

/**
 * @brief Get the PLI kernel module path by name.
 * @param kernel_name Name of the kernel.
 * @return Path to the .so module, or NULL if not found or incomplete.
 */
const char *tpb_dl_get_exec_path(const char *kernel_name);

/**
 * @brief Check if a kernel registered successfully with a runnable .so.
 * @param kernel_name Name of the kernel.
 * @return 1 if complete, 0 if incomplete or not found.
 */
int tpb_dl_is_complete(const char *kernel_name);

/**
 * @brief Get the integration type for a registered kernel.
 * @param kernel_name Name of the kernel.
 * @return Kernel integration type (`TPB_KTYPE_PLI`), or 0 if not found.
 */
TPB_K_CTRL tpb_dl_get_ktype(const char *kernel_name);

#endif /* TPB_DYNLOADER_H */
