/**
 * @file kernels.h
 * @brief Kernel informations.
 */

#ifndef _KERNELS_H
#define _KERNELS_H

#include "../tpb-types.h"

/**
 * @brief Register triad kernel
 * @param kernel Pointer to kernel structure to fill
 * @return Error code (0 on success)
 */
extern int register_triad(tpb_kernel_t *kernel);

/**
 * @brief Run triad kernel
 * @param handle Runtime handle with parameters, timer, and result package
 * @return Error code (0 on success)
 */
extern int run_triad(tpb_rt_handle_t *handle);

#endif // #ifndef _KERNELS_H