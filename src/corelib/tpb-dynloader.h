/**
 * @file tpb-dynloader.h
 * @brief Dynamic kernel loader for PLI (Process-Level Integration) mode.
 *
 * Provides functions to scan ${TPB_DIR}/lib and ${TPB_DIR}/bin for PLI kernels,
 * dynamically load kernel shared libraries, and manage kernel executable paths.
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
 * @brief Scan ${TPB_DIR}/lib and ${TPB_DIR}/bin for PLI kernels.
 *
 * For each libtpbk_<name>.so found, attempts to:
 *   1. dlopen() the library
 *   2. dlsym() for tpbk_pli_register_<name> function
 *   3. Call the registration function to register name/note/parameters
 *   4. Store the exec path mapping: kernel_name -> ${TPB_DIR}/bin/tpbk_<name>.tpbx
 *
 * Kernels missing their .tpbx executable are marked as incomplete.
 *
 * @return 0 on success, error code otherwise.
 */
int tpb_dl_scan(void);

/**
 * @brief Get the PLI kernel executable path by name.
 * @param kernel_name Name of the kernel.
 * @return Path to the .tpbx executable, or NULL if not found or incomplete.
 */
const char *tpb_dl_get_exec_path(const char *kernel_name);

/**
 * @brief Check if a kernel has both .so and .tpbx files.
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
