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

/**
 * @brief Register scale kernel (STREAM Scale)
 * @return Error code (0 on success)
 */
extern int register_scale(void);

/**
 * @brief Register axpy kernel (BLAS AXPY)
 * @return Error code (0 on success)
 */
extern int register_axpy(void);

/**
 * @brief Register rtriad kernel (Repeat Triad)
 * @return Error code (0 on success)
 */
extern int register_rtriad(void);

/**
 * @brief Register sum kernel (Reduction)
 * @return Error code (0 on success)
 */
extern int register_sum(void);

#ifdef TPB_USE_ROCM
/**
 * @brief Register roofline_rocm kernel (ROCm GPU Roofline Model)
 * @return Error code (0 on success)
 */
extern int register_roofline_rocm(void);
#endif

#endif // #ifndef _KERNELS_H