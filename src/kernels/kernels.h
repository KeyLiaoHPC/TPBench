/**
 * @file kernels.h
 * @brief Kernel informations.
 */

#ifndef _KERNELS_H
#define _KERNELS_H

#include "../corelib/tpb-types.h"

/*
 * Legacy declarations — these register_* functions are not defined or called.
 * Kernels are discovered at runtime via dlopen/dlsym (tpbk_pli_register_<name>).
 * Kept as placeholders until kernels.h is fully retired.
 */
// extern int register_stream(void);
// extern int register_triad(void);
// extern int register_scale(void);
// extern int register_axpy(void);
// extern int register_rtriad(void);
// extern int register_sum(void);
// extern int register_staxpy(void);
// extern int register_striad(void);
// extern int register_roofline_rocm(void);

#endif // #ifndef _KERNELS_H