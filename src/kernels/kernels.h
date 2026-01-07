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
extern int register_triad(void);

#endif // #ifndef _KERNELS_H