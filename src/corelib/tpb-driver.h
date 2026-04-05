/**
 * @file tpb-driver.h
 * @brief Internal header for the TPBench driver.
 *
 * Public kernel API declarations (tpb_k_register, tpb_k_add_parm, etc.)
 * live in tpbench.h.  This header exposes driver-internal functions only.
 */

#ifndef TPB_DRIVER_H
#define TPB_DRIVER_H

#define _GNU_SOURCE

#include "tpb-types.h"

/**
 * @brief Set timer function for the whole driver.
 */
int tpb_driver_set_timer(tpb_timer_t timer);

/**
 * @brief Get timer function for the driver.
 * @param timer Non-NULL pointer to receive timer via value-copy.
 * @return 0 on success, error code otherwise.
 */
int tpb_driver_get_timer(tpb_timer_t *timer);

/**
 * @brief Set resolved KernelID for a registered kernel by name.
 * @param kernel_name Kernel name.
 * @param kernel_id 20-byte KernelID value.
 * @return 0 on success, error code otherwise.
 */
int tpb_driver_set_kernel_id(const char *kernel_name,
                             const unsigned char kernel_id[20]);

/**
 * @brief Set whether workspace kernel record update succeeded for a kernel (dynloader).
 * @param kernel_name Kernel name.
 * @param ok Nonzero if record update succeeded, zero on failure.
 * @return 0 on success, error code otherwise.
 */
int tpb_driver_set_kernel_record_ok(const char *kernel_name, int ok);

/**
 * @brief Get number of registered kernels.
 * @return Number of registered kernels.
 */
int tpb_get_nkern(void);

/**
 * @brief Get number of handles.
 * @return Number of handles.
 */
int tpb_get_nhdl(void);

/**
 * @brief Get kernel parameter pointer and type.
 * @param kernel_name Kernel name (NULL for current handle)
 * @param parm_name Parameter name
 * @param v Output pointer to parameter value (can be NULL)
 * @param dtype Output for parameter dtype (can be NULL)
 * @return 0 on success, error code otherwise.
 */
int tpb_driver_get_kparm_ptr(const char *kernel_name, const char *parm_name,
                             void **v, TPB_DTYPE *dtype);

/**
 * @brief Set kernel argument value for current handle.
 * @param parm_name Parameter name
 * @param v Pointer to value to set
 * @return 0 on success, error code otherwise.
 */
int tpb_driver_set_hdl_karg(const char *parm_name, void *v);

/**
 * @brief Set environment variable for current handle.
 * @param env_name Environment variable name
 * @param env_value Environment variable value (string)
 * @return 0 on success, error code otherwise.
 */
int tpb_driver_set_hdl_env(const char *env_name, const char *env_value);

/**
 * @brief Set MPI arguments string for current handle (replaces existing).
 * @param mpiargs_str MPI arguments string to pass to launcher as-is
 * @return 0 on success, error code otherwise.
 */
int tpb_driver_set_hdl_mpiargs(const char *mpiargs_str);

/**
 * @brief Append MPI arguments to current handle (concatenates with space).
 * @param mpiargs_str MPI arguments string to append
 * @return 0 on success, error code otherwise.
 */
int tpb_driver_append_hdl_mpiargs(const char *mpiargs_str);

/**
 * @brief Get MPI arguments string from current handle.
 * @return MPI arguments string (may be NULL), do not free.
 */
const char *tpb_driver_get_hdl_mpiargs(void);

/**
 * @brief Copy argpack, envpack, and mpipack from source handle index to current handle.
 * @param src_idx Source handle index to copy from.
 * @return 0 on success, error code otherwise.
 */
int tpb_driver_copy_hdl_from(int src_idx);

/**
 * @brief Get the current handle index.
 * @return Current handle index, or -1 if none.
 */
int tpb_driver_get_current_hdl_idx(void);

/**
 * @brief Add a handle for a kernel by name.
 * Creates handle, sets current_rthdl internally, increments nhdl.
 * @param kernel_name Kernel name to create handle for.
 * @return 0 on success, error code otherwise.
 */
int tpb_driver_add_handle(const char *kernel_name);

/**
 * @brief Run all handles (indices 0 .. nhdl-1).
 * @return 0 on success, error code on first failure.
 */
int tpb_driver_run_all(void);

/**
 * @brief Register common parameters
 * @return Error code (0 on success)
 */
int tpb_register_common();

/* tpb_register_kernel() is declared in tpb-public.h / tpbench.h */

/* PLI (Process-Level Integration) internal API */

/**
 * @brief Enable kernel registration mode.
 *
 * Resets internal state to allow additional kernels to be registered
 * after tpb_register_kernel() has been called.
 */
void tpb_driver_enable_kernel_reg(void);

/**
 * @brief Disable kernel registration mode.
 *
 * Restores internal state after kernel registration is complete.
 */
void tpb_driver_disable_kernel_reg(void);

/**
 * @brief Reset handles for next batch run.
 *
 * Frees all runtime handles and clears the handle list.
 * Preserves the kernel registry.
 *
 * @return 0 on success.
 */
int tpb_driver_reset_handles(void);

/**
 * @brief Get element size in bytes for a given TPB_DTYPE.
 * @param dtype Data type (masked with TPB_PARM_TYPE_MASK internally).
 * @param out   Non-NULL pointer to receive element size.
 * @return 0 on success, TPBE_LIST_NOT_FOUND for unsupported types.
 */
int tpb_dtype_elem_size(TPB_DTYPE dtype, size_t *out);

#endif /* TPB_DRIVER_H */
