/*
 * @file tpb-driver.c
 * @brief Main entry of benchmarking kernels.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "tpb-driver.h"
#include "tpb-io.h"
#include "tpb-types.h"
#include "kernels/kernels.h"

#define MAX_KERNELS 128

int nkern = 0;
int nkrout = 0;
tpb_kernel_t *kernel_all = NULL;

// Current kernel being registered
static tpb_kernel_t *current_kernel = NULL;

/**
 * @brief Add a kernel to the registry
 * @param reg_func Function pointer to register the kernel
 * @return Error code (0 on success)
 */
int
tpb_add_kernel(int (*reg_func)(tpb_kernel_t *))
{
    tpb_kernel_t kernel;
    int err;

    if(nkern >= MAX_KERNELS) {
        return TPBE_KERN_ARG_FAIL;
    }

    // Initialize kernel structure
    memset(&kernel, 0, sizeof(tpb_kernel_t));
    
    // Call registration function
    err = reg_func(&kernel);
    if(err != 0) {
        return err;
    }

    // Reallocate kernel array
    kernel_all = (tpb_kernel_t *)realloc(kernel_all, sizeof(tpb_kernel_t) * (nkern + 1));
    if(kernel_all == NULL) {
        return TPBE_MALLOC_FAIL;
    }

    // Copy kernel to array
    memcpy(&kernel_all[nkern], &kernel, sizeof(tpb_kernel_t));
    
    nkern++;
    nkrout++;  // For compatibility
    
    return 0;
}

/**
 * @brief Initialize kernel registry by calling registration functions
 * @return Error code (0 on success)
 */
int
tpb_get_kernel_info()
{
    int err;
    
    nkern = 0;
    nkrout = 0;
    
    // Free any existing kernel array
    if(kernel_all != NULL) {
        free(kernel_all);
        kernel_all = NULL;
    }
    
    // Register triad kernel using new API
    err = register_triad(NULL);
    if(err != 0) {
        return err;
    }

    return 0;
}

int
tpb_run_kernel(int id, tpb_timer_t *timer, int ntest, int64_t *time_arr, uint64_t nkib,
               tpb_k_arg_token_t *kargs_kernel, int kernel_idx)
{
    int err;

    if(id < 0 || id >= nkern) {
        return TPBE_KERN_NOT_FOUND;
    }

    tpb_printf(TPBM_PRTN_M_DIRECT, "Running Kernel %s\n", kernel_all[id].info.kname);
    tpb_printf(TPBM_PRTN_M_DIRECT, "Description: %s\n", kernel_all[id].info.note);
    tpb_printf(TPBM_PRTN_M_DIRECT, "Number of tests: %d\n", ntest);
    tpb_printf(TPBM_PRTN_M_DIRECT, "# of Elements per Array: %ld\n", nkib * 1024 / sizeof(double));
    
    // Prepare runtime handle
    tpb_rt_handle_t handle;
    tpb_respack_t respack;
    
    // Copy parameter definitions to runtime parameters
    handle.nparms = kernel_all[id].info.nparms;
    handle.rt_parms = (tpb_rt_parm_t *)malloc(sizeof(tpb_rt_parm_t) * handle.nparms);
    for(int i = 0; i < handle.nparms; i++) {
        memcpy(&handle.rt_parms[i], &kernel_all[id].info.parms[i], sizeof(tpb_rt_parm_t));
        // Set to default values initially
        handle.rt_parms[i].value = handle.rt_parms[i].default_value;
    }
    
    // Apply kernel-specific arguments if provided
    if(kargs_kernel != NULL && kernel_idx >= 0 && kernel_idx < kargs_kernel->nkern) {
        int token_start = 0;
        for(int i = 0; i < kernel_idx; i++) {
            token_start += kargs_kernel->ntoken[i];
        }
        
        for(int i = 0; i < kargs_kernel->ntoken[kernel_idx]; i++) {
            char token_buf[TPBM_CLI_STR_MAX_LEN];
            snprintf(token_buf, sizeof(token_buf), "%s", kargs_kernel->token[token_start + i]);
            char *eq = strchr(token_buf, '=');
            if(eq) {
                *eq = '\0';
                char *key = token_buf;
                char *value = eq + 1;
                
                // Find and update the parameter
                for(int j = 0; j < handle.nparms; j++) {
                    if(strcmp(handle.rt_parms[j].name, key) == 0) {
                        // Parse and set the value based on type
                        uint32_t type_code = handle.rt_parms[j].dtype & TPB_PARM_TYPE_MASK;
                        if(type_code >= TPB_INT8_T && type_code <= TPB_INT64_T) {
                            handle.rt_parms[j].value.i64 = strtoll(value, NULL, 10);
                        } else if(type_code >= TPB_UINT8_T && type_code <= TPB_UINT64_T) {
                            handle.rt_parms[j].value.u64 = strtoull(value, NULL, 10);
                        } else if(type_code == TPB_FLOAT_T || type_code == TPB_DOUBLE_T || type_code == TPB_LONG_DOUBLE_T) {
                            handle.rt_parms[j].value.f64 = strtod(value, NULL);
                        } else if(type_code == TPB_STRING_T) {
                            handle.rt_parms[j].value.str = strdup(value);
                        }
                        break;
                    }
                }
            }
        }
    }
    
    // Set up result package
    respack.time_arr = time_arr;
    respack.ntest = ntest;
    respack.nbyte = kernel_all[id].info.nbyte;
    respack.nsize = nkib * 1024 / sizeof(double);
    
    handle.timer = timer;
    handle.respack = &respack;
    
    // Call kernel runner
    err = kernel_all[id].func.kfunc_run(&handle);
    
    // Clean up
    free(handle.rt_parms);
    
    tpb_printf(TPBM_PRTN_M_DIRECT, HLINE);
    return err;
}

// === New Kernel Registration API Implementation ===

int
tpb_k_register(const char *name)
{
    if(name == NULL) {
        return TPBE_KERN_ARG_FAIL;
    }
    
    // Check if name is unique
    for(int i = 0; i < nkern; i++) {
        if(kernel_all[i].info.kname && strcmp(kernel_all[i].info.kname, name) == 0) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, 
                      "Kernel name '%s' already registered\n", name);
            return TPBE_KERN_ARG_FAIL;
        }
    }
    
    if(nkern >= MAX_KERNELS) {
        return TPBE_KERN_ARG_FAIL;
    }
    
    // Reallocate kernel array
    kernel_all = (tpb_kernel_t *)realloc(kernel_all, sizeof(tpb_kernel_t) * (nkern + 1));
    if(kernel_all == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    
    // Initialize new kernel
    current_kernel = &kernel_all[nkern];
    memset(current_kernel, 0, sizeof(tpb_kernel_t));
    
    current_kernel->info.kname = strdup(name);
    current_kernel->info.nparms = 0;
    current_kernel->info.parms = NULL;
    current_kernel->info.ndim = 1;  // Default to 1D
    
    nkern++;
    nkrout++;
    
    return 0;
}

int
tpb_k_set_note(const char *note)
{
    if(current_kernel == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, 
                  "No kernel registered. Call tpb_k_register first.\n");
        return TPBE_KERN_ARG_FAIL;
    }
    
    if(note == NULL) {
        return TPBE_KERN_ARG_FAIL;
    }
    
    current_kernel->info.note = strdup(note);
    return 0;
}

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
int
tpb_k_add_parm(const char *name, const char *default_value, 
               const char *description, TPB_DTYPE dtype, ...)
{
    if(current_kernel == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, 
                  "No kernel registered. Call tpb_k_register first.\n");
        return TPBE_KERN_ARG_FAIL;
    }
    
    if(name == NULL || default_value == NULL || description == NULL) {
        return TPBE_KERN_ARG_FAIL;
    }
    
    // Reallocate parameter array
    int nparms = current_kernel->info.nparms;
    current_kernel->info.parms = (tpb_rt_parm_t *)realloc(
        current_kernel->info.parms, 
        sizeof(tpb_rt_parm_t) * (nparms + 1)
    );
    if(current_kernel->info.parms == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    
    tpb_rt_parm_t *parm = &current_kernel->info.parms[nparms];
    memset(parm, 0, sizeof(tpb_rt_parm_t));
    
    parm->name = strdup(name);
    parm->description = strdup(description);
    parm->dtype = dtype;
    
    // Extract type code from dtype
    uint32_t type_code = (uint32_t)(dtype & TPB_PARM_TYPE_MASK);
    va_list args;
    va_start(args, dtype);
    
    // Parse default value based on type
    switch(type_code) {
        case TPB_INT8_T:
        case TPB_INT16_T:
        case TPB_INT32_T:
        case TPB_INT64_T:
            parm->default_value.i64 = atoll(default_value);
            parm->value.i64 = parm->default_value.i64;
            break;
        case TPB_UINT8_T:
        case TPB_UINT16_T:
        case TPB_UINT32_T:
        case TPB_UINT64_T:
            parm->default_value.u64 = strtoull(default_value, NULL, 10);
            parm->value.u64 = parm->default_value.u64;
            break;
        case TPB_FLOAT_T:
        case TPB_DOUBLE_T:
        case TPB_LONG_DOUBLE_T:
            parm->default_value.f64 = atof(default_value);
            parm->value.f64 = parm->default_value.f64;
            break;
        case TPB_STRING_T:
        case TPB_CHAR_T:
            parm->default_value.str = strdup(default_value);
            parm->value.str = strdup(default_value);
            break;
        default:
            va_end(args);
            return TPBE_KERN_ARG_FAIL;
    }
    
    // Handle validation parameters
    uint32_t check_mode = (uint32_t)(dtype & TPB_PARM_CHECK_MASK);
    if(check_mode == TPB_PARM_RANGE) {
        // Range validation: [lo, hi]
        parm->nlims = 2;
        parm->plims = (tpb_parm_value_t *)malloc(sizeof(tpb_parm_value_t) * 2);
        
        // Signed integer types use int64_t for range bounds
        if(type_code >= TPB_INT8_T && type_code <= TPB_INT64_T) {
            parm->plims[0].i64 = va_arg(args, int64_t);  // lo
            parm->plims[1].i64 = va_arg(args, int64_t);  // hi
        }
        // Unsigned integer types use uint64_t for range bounds
        else if(type_code >= TPB_UINT8_T && type_code <= TPB_UINT64_T) {
            parm->plims[0].u64 = va_arg(args, uint64_t);  // lo
            parm->plims[1].u64 = va_arg(args, uint64_t);  // hi
        }
        // Floating point types use double for range bounds
        else if(type_code == TPB_FLOAT_T || type_code == TPB_DOUBLE_T || type_code == TPB_LONG_DOUBLE_T) {
            parm->plims[0].f64 = va_arg(args, double);  // lo
            parm->plims[1].f64 = va_arg(args, double);  // hi
        }
    } else if(check_mode == TPB_PARM_LIST) {
        // List validation: (n, plist)
        parm->nlims = va_arg(args, int);
        
        // Allocate and copy list values based on type
        parm->plims = (tpb_parm_value_t *)malloc(
            sizeof(tpb_parm_value_t) * parm->nlims);
        
        if(type_code >= TPB_INT8_T && type_code <= TPB_INT64_T) {
            int64_t *src = va_arg(args, int64_t *);
            for(int i = 0; i < parm->nlims; i++) {
                parm->plims[i].i64 = src[i];
            }
        } else if(type_code >= TPB_UINT8_T && type_code <= TPB_UINT64_T) {
            uint64_t *src = va_arg(args, uint64_t *);
            for(int i = 0; i < parm->nlims; i++) {
                parm->plims[i].u64 = src[i];
            }
        } else if(type_code == TPB_FLOAT_T || type_code == TPB_DOUBLE_T || type_code == TPB_LONG_DOUBLE_T) {
            double *src = va_arg(args, double *);
            for(int i = 0; i < parm->nlims; i++) {
                parm->plims[i].f64 = src[i];
            }
        } else if(type_code == TPB_STRING_T) {
            char **src = va_arg(args, char **);
            for(int i = 0; i < parm->nlims; i++) {
                parm->plims[i].str = strdup(src[i]);
            }
        }
    } else {
        // No validation
        parm->nlims = 0;
        parm->plims = NULL;
    }
    
    va_end(args);
    
    current_kernel->info.nparms++;
    return 0;
}

int
tpb_k_add_runner(int (*runner)(tpb_rt_handle_t *handle))
{
    if(current_kernel == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, 
                  "No kernel registered. Call tpb_k_register first.\n");
        return TPBE_KERN_ARG_FAIL;
    }
    
    if(runner == NULL) {
        return TPBE_KERN_ARG_FAIL;
    }
    
    current_kernel->func.kfunc_run = runner;
    return 0;
}

int
tpb_k_set_dim(int ndim)
{
    if(current_kernel == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, 
                  "No kernel registered. Call tpb_k_register first.\n");
        return TPBE_KERN_ARG_FAIL;
    }
    
    if(ndim < 1) {
        return TPBE_KERN_ARG_FAIL;
    }
    
    current_kernel->info.ndim = ndim;
    return 0;
}

int
tpb_k_set_nbyte(uint64_t nbyte)
{
    if(current_kernel == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, 
                  "No kernel registered. Call tpb_k_register first.\n");
        return TPBE_KERN_ARG_FAIL;
    }
    
    current_kernel->info.nbyte = nbyte;
    return 0;
}

tpb_parm_value_t *
tpb_rt_get_parm(tpb_rt_handle_t *handle, const char *name)
{
    if(handle == NULL || name == NULL) {
        return NULL;
    }
    
    for(int i = 0; i < handle->nparms; i++) {
        if(strcmp(handle->rt_parms[i].name, name) == 0) {
            return &handle->rt_parms[i].value;
        }
    }
    
    return NULL;
}