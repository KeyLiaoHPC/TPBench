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

/**
 * @brief Register stream kernel
 * @param kernel Pointer to kernel structure to fill
 * @return Error code (0 on success)
 */
extern int register_stream(void);

/**
 * @brief Register staxpy kernel (Stanza AXPY)
 * @return Error code (0 on success)
 */
extern int register_staxpy(void);

/**
 * @brief Register striad kernel (Stanza Triad)
 * @return Error code (0 on success)
 */
extern int register_striad(void);

#endif // #ifndef _KERNELS_H