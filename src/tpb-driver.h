/**
 * =================================================================================
 * TPBench - A throughputs benchmarking tool for high-performance computing
 * 
 * Copyright (C) 2024 Key Liao (Liao Qiucheng)
 * 
 * This program is free software: you can redistribute it and/or modify it under the
 *  terms of the GNU General Public License as published by the Free Software 
 * Foundation, either version 3 of the License, or (at your option) any later 
 * version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY 
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with 
 * this program. If not, see https://www.gnu.org/licenses/.
 * =================================================================================
 * @file tpb-driver.h
 * @version 0.3
 * @brief Header for tpb-driver library 
 * @author Key Liao (keyliaohpc@gmail.com, keyliao@sjtu.edu.cn)
 * @date 2024-01-22
 */

#include "tpb-types.h"
#define _GNU_SOURCE

#include "../tpb-impl.h"
#include "tpkernels.h"
#include "tptimer.h"

/**
 * @brief Set timer function for the whole driver.
 */
int tpb_driver_set_timer(tpb_timer_t timer); 

/**
 * @brief Register common parameters
 * @return Error code (0 on success)
 */
int tpb_register_common();

/**
 * @brief Register all kernels
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

// === New Kernel Registration API ===
/**
 * @brief Register a new kernel with given a name and description.
 * @param name Kernel name (must be unique)
 * @return 0 on success, error code otherwise
 */
int tpb_k_register(const char *name, const char *note);

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
 *   tpb_k_add_parm("ntest", "10", "Number of tests",
 *                  TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
 *                  (int64_t)1, (int64_t)10000);
 * 
 *   // List check for string
 *   const char *dtypes[] = {"float", "double", "int"};
 *   tpb_k_add_parm("dtype", "double", "Data type",
 *                  TPB_PARM_CLI | TPB_STRING_T | TPB_PARM_LIST,
 *                  3, dtypes);
 * 
 *   // No check for double
 *   tpb_k_add_parm("epsilon", "1e-6", "Convergence threshold",
 *                  TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_NOCHECK);
 */
int tpb_k_add_parm(const char *name, const char *default_value, 
                   const char *description, TPB_DTYPE dtype, ...);

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
 * @brief Clean up and free memory allocated for output data in the kernel runtime handle.
 *
 * This function releases all dynamic allocations made for output variables managed
 * by the given kernel runtime handle, ensuring no memory leaks after a kernel execution.
 *
 * @param handle Pointer to the kernel runtime handle .
 * @return 0 on successful cleanup, or an error code if any issues occurred.
 */
int tpb_clean_output(tpb_k_rthdl_t *handle);