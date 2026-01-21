/**
 * @file tpb-driver.h
 * @brief Header for tpb-driver library.
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
 * @brief Get number of registered kernels.
 * @return Number of registered kernels.
 */
int tpb_driver_get_nkern(void);

/**
 * @brief Get number of handles.
 * @return Number of handles.
 */
int tpb_driver_get_nhdl(void);

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
 * @brief Add a handle for a kernel by name.
 * Creates handle, sets current_rthdl internally, increments nhdl.
 * @param kernel_name Kernel name to create handle for.
 * @return 0 on success, error code otherwise.
 */
int tpb_driver_add_handle(const char *kernel_name);

/**
 * @brief Run all handles starting from index 1.
 * @return 0 on success, error code on first failure.
 */
int tpb_driver_run_all(void);

/**
 * @brief Register common parameters
 * @return Error code (0 on success)
 */
int tpb_register_common();

/**
 * @brief Initialize kernel registry and register common parameters.
 * @return Error code (0 on success)
 */
int tpb_register_kernel();

/**
 * @brief Get the number of registered kernels
 * @return Kernel count
 */
int tpb_get_kernel_count(void);

/**
 * @brief Get a registered kernel by name
 * @param name Kernel name
 * @param kernel_out Pointer to receive kernel address
 * @return 0 on success, error code otherwise
 */
int tpb_get_kernel(const char *name, tpb_kernel_t **kernel_out);

/**
 * @brief Get a registered kernel by index
 * @param idx Kernel index
 * @param kernel_out Pointer to receive kernel address
 * @return 0 on success, error code otherwise
 */
int tpb_get_kernel_by_index(int idx, tpb_kernel_t **kernel_out);

/**
 * @brief Run a registered kernel
 * @param id Kernel ID
 * @param handle Runtime handle with kernel info, timer, parms, and result package
 * @return int Error code (0 on success)
 */
int tpb_run_kernel(tpb_k_rthdl_t *handle);

/* Kernel Registration API */

/**
 * @brief Register a new kernel with given a name and description.
 * @param name Kernel name (must be unique)
 * @param note Kernel description
 * @param kctrl Kernel attribution control bits
 * @return 0 on success, error code otherwise
 */
int tpb_k_register(const char *name, const char *note, TPB_K_CTRL kctrl);

/**
 * @brief Add a runtime parameter to the current kernel
 * 
 * This function defines a parameter for the kernel that can be set at runtime
 * through CLI arguments or other configuration methods.
 * 
 * The dtype parameter uses a 32-bit encoding: 0xSSCCTTTT
 *   - SS (bits 24-31): Parameter Source flags
 *   - CC (bits 16-23): Check/Validation mode flags
 *   - TTTT (bits 0-15): Type code (MPI-compatible)
 * 
 * @param name Parameter name (used for CLI argument matching)
 * @param default_value String representation of default value
 * @param description Human-readable parameter description
 * @param dtype Combined data type: source | check | type
 *              Source: TPB_PARM_CLI, TPB_PARM_MACRO, TPB_PARM_CONFIG, TPB_PARM_ENV
 *              Type: TPB_INT8_T, TPB_INT16_T, TPB_INT32_T, TPB_INT64_T,
 *                    TPB_UINT8_T, TPB_UINT16_T, TPB_UINT32_T, TPB_UINT64_T,
 *                    TPB_FLOAT_T, TPB_DOUBLE_T, TPB_LONG_DOUBLE_T,
 *                    TPB_STRING_T, TPB_CHAR_T
 *              Validation: TPB_PARM_NOCHECK, TPB_PARM_RANGE, TPB_PARM_LIST, TPB_PARM_CUSTOM
 * @param ... Variable arguments based on validation mode:
 *            - TPB_PARM_RANGE: Two args (lo, hi) for range [lo, hi]
 *                For signed int types: int64_t lo, int64_t hi
 *                For unsigned int types: uint64_t lo, uint64_t hi
 *                For float types: double lo, double hi
 *            - TPB_PARM_LIST: Two args (n, plist) for list validation
 *                n: int (number of valid values)
 *                plist: pointer to array of valid values (type must match parameter type)
 *            - TPB_PARM_NOCHECK: No additional arguments
 * 
 * @return 0 on success, error code otherwise
 * 
 * @example
 *   // Range check for integer
 *   tpb_k_add_parm("ntest", "Number of tests", "10",
 *                  TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
 *                  (int64_t)1, (int64_t)10000);
 * 
 *   // List check for string
 *   const char *dtypes[] = {"float", "double", "int"};
 *   tpb_k_add_parm("dtype", "Data type", "double",
 *                  TPB_PARM_CLI | TPB_STRING_T | TPB_PARM_LIST,
 *                  3, dtypes);
 * 
 *   // No check for double
 *   tpb_k_add_parm("epsilon", "Convergence threshold", "1e-6",
 *                  TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_NOCHECK);
 */
int tpb_k_add_parm(const char *name, const char *note,
                   const char *default_value, TPB_DTYPE dtype, ...);

/**
 * @brief Set the runner function for the current kernel
 * @param runner Function pointer to kernel runner
 * @return 0 on success, error code otherwise
 */
int tpb_k_add_runner(int (*runner)(void));

/**
 * @brief Register a new output data definition for the current kernel.
 *
 * This function must be called during kernel registration (after tpb_k_register,
 * before tpb_k_add_runner) to define output metrics. The output definitions are
 * stored and later copied to the runtime handle's respack when tpb_run_kernel
 * is called.
 *
 * @param name   Output name (used to look up when allocating/reporting).
 * @param note   Human-readable description.
 * @param dtype  Data type of the output (TPB_INT64_T, TPB_DOUBLE_T, etc.).
 * @param unit   Unit type (TPB_UNIT_NS, TPB_UNIT_BYTE, etc.).
 * @return 0 on success, error code otherwise.
 */
int tpb_k_add_output(const char *name, const char *note, TPB_DTYPE dtype, TPB_UNIT_T unit);

/**
 * @brief Get argument value from runtime handle
 * @param handle Runtime handle
 * @param name Parameter name
 * @return Pointer to parameter value, NULL if not found
 */
int tpb_k_get_arg(const char *name, TPB_DTYPE dtype, void *argptr);

/**
 * @brief Get timer function
 */
int tpb_k_get_timer(tpb_timer_t *);

/**
 * @brief Allocating memory for output data in the TPB framework, return the pointer.
 * @param name The name of a output variable.
 * @param n The number of elements with 'dtype' defined in tpb_k_add_output
 * @param ptr Pointer to the header of allocated memory, NULL if failed.
 * @return 0 if successful, otherwise error code. 
 */
int tpb_k_alloc_output(const char *name, uint64_t n, void *ptr);

/**
 * @brief Clean up and free all memory in the kernel runtime handle.
 *
 * This function releases all dynamic allocations made for output variables and
 * kernel arguments managed by the given kernel runtime handle, ensuring no memory
 * leaks after a kernel execution. It frees respack outputs and argpack arguments,
 * and sets all pointers to NULL.
 *
 * @param handle Pointer to the kernel runtime handle .
 * @return 0 on successful cleanup, or an error code if any issues occurred.
 */
int tpb_driver_clean_handle(tpb_k_rthdl_t *handle);

/* PLI (Process-Level Integration) API */

/**
 * @brief Enable kernel registration mode.
 *
 * This function resets internal state to allow additional kernels to be
 * registered after tpb_register_kernel() has been called. Used for FLI
 * mode where kernels are registered separately from the driver init.
 */
void tpb_driver_enable_kernel_reg(void);

/**
 * @brief Disable kernel registration mode.
 *
 * Restores internal state after kernel registration is complete.
 */
void tpb_driver_disable_kernel_reg(void);

/**
 * @brief Set the integration mode for the driver.
 * @param mode TPB_INTEG_MODE_FLI or TPB_INTEG_MODE_PLI
 * @return 0 on success, error code otherwise.
 */
int tpb_driver_set_integ_mode(int mode);

/**
 * @brief Get the current integration mode.
 * @return Current integration mode (TPB_INTEG_MODE_FLI or TPB_INTEG_MODE_PLI).
 */
int tpb_driver_get_integ_mode(void);

/**
 * @brief Finalize PLI kernel registration.
 *
 * For PLI kernels, this replaces tpb_k_add_runner(). It increments the kernel
 * count without setting a runner function (PLI kernels use exec instead).
 *
 * @return 0 on success, error code otherwise.
 */
int tpb_k_finalize_pli(void);

/**
 * @brief Set timer for PLI kernel executable.
 *
 * This function is intended to be called from .tpbx executables. It sets up
 * the timer based on the provided timer name. This function is only valid
 * to call from a PLI kernel process context.
 *
 * @param timer_name Timer name (e.g., "clock_gettime", "tsc_asym")
 * @return 0 on success, error code otherwise.
 */
int tpb_k_pli_set_timer(const char *timer_name);

/**
 * @brief Build handle from positional arguments for PLI executable.
 *
 * This function parses positional arguments (in parameter registration order)
 * and builds a runtime handle. Called from .tpbx executables.
 *
 * @param handle Pointer to runtime handle to populate.
 * @param argc Number of positional argument values.
 * @param argv Array of argument value strings.
 * @return 0 on success, error code otherwise.
 */
int tpb_k_pli_build_handle(tpb_k_rthdl_t *handle, int argc, char **argv);

#endif /* TPB_DRIVER_H */