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
 * Basic information of tpbench benchmarking sets, init by tpb_register_kernel().
 */
// # of kernels, kernel routines, groups and group routines
extern int nkern, nkrout;
extern tpb_kernel_t *kernel_all;

// Common parameters
extern tpb_rt_parm_t *tpb_parms_common;
extern int nparms_common;

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
 * @brief Run a registered kernel
 * @param id Kernel ID
 * @param timer Timer structure
 * @param ntest Number of tests
 * @param time_arr Output array for timing results
 * @param nkib Memory size in KiB
 * @param rt_parms Pre-configured runtime parameters for this instance
 * @param nparms Number of parameters
 * @return int Error code (0 on success)
 */
int tpb_run_kernel(int id, tpb_timer_t *timer, int ntest, int64_t *time_arr, uint64_t nkib,
                   tpb_rt_parm_t *rt_parms, int nparms);

/**
 * @brief get error message according to error code
 * @param err  error code
 * @param buf  error message buffer, 31 characters.
 * @return char* reutrn buf.
 */
char *tpb_geterr(const int err, char *buf);

// === New Kernel Registration API ===
/**
 * @brief Register a new kernel with given name
 * @param name Kernel name (must be unique)
 * @return 0 on success, error code otherwise
 */
int tpb_k_register(const char *name);

/**
 * @brief Set the description/note for the current kernel
 * @param note Description string
 * @return 0 on success, error code otherwise
 */
int tpb_k_set_note(const char *note);

/**
 * @brief Add a runtime parameter to the current kernel
 * @param name Parameter name
 * @param default_value Default value string
 * @param description Parameter description
 * @param dtype Parameter data type (source | check | type)
 * @param ... Variable arguments for validation (range: min, max; list: count, array)
 * @return 0 on success, error code otherwise
 */
int tpb_k_add_parm(const char *name, const char *default_value, 
                   const char *description, TPB_DTYPE_U64 dtype, ...);

/**
 * @brief Set the runner function for the current kernel
 * @param runner Function pointer to kernel runner
 * @return 0 on success, error code otherwise
 */
int tpb_k_add_runner(int (*runner)(tpb_rt_handle_t *handle));

/**
 * @brief Set the number of dimensions for the current kernel
 * @param ndim Number of dimensions
 * @return 0 on success, error code otherwise
 */
int tpb_k_set_dim(int ndim);

/**
 * @brief Set bytes per iteration for the current kernel
 * @param nbyte Bytes through core per iteration
 * @return 0 on success, error code otherwise
 */
int tpb_k_set_nbyte(uint64_t nbyte);

/**
 * @brief Add axis for output data
 */
int tpb_k_add_axis(void *ptr, int cnt, TPB_DTYPE_U64, TPB_DTYPE_U64, char note[TPBM_CLI_STR_MAX_LEN]);

/**
 * @brief Get parameter value from runtime handle
 * @param handle Runtime handle
 * @param name Parameter name
 * @return Pointer to parameter value, NULL if not found
 */
tpb_parm_value_t *tpb_rt_get_parm(tpb_rt_handle_t *handle, const char *name);