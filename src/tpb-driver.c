/*
 * @file tpb-driver.c
 * @brief Main entry of benchmarking kernels.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "tpb-driver.h"
#include "tpb-io.h"
#include "tpb-types.h"
#include "kernels/kernels.h"

#define MAX_KERNELS 128

int nkern = 0;
int nkrout = 0;
tpb_kernel_t *kernel_all = NULL;

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

    // Register triad kernel
    err = tpb_add_kernel(register_triad);
    if(err != 0) {
        return err;
    }

    return 0;
}

int
tpb_run_kernel(int id, tpb_timer_t *timer, int ntest, int64_t *time_arr, uint64_t nkib)
{
    int err;

    if(id < 0 || id >= nkern) {
        return TPBE_KERN_NOT_FOUND;
    }

    tpb_printf(TPBM_PRTN_M_DIRECT, "Running Kernel %s\n", kernel_all[id].info.kname);
    tpb_printf(TPBM_PRTN_M_DIRECT, "Description: %s\n", kernel_all[id].info.note);
    tpb_printf(TPBM_PRTN_M_DIRECT, "Number of tests: %d\n", ntest);
    tpb_printf(TPBM_PRTN_M_DIRECT, "# of Elements per Array: %ld\n", nkib * 1024 / sizeof(double));
    
    // Call d_triad directly (can be extended with a dispatch mechanism later)
    err = d_triad(timer, ntest, time_arr, nkib);
    
    tpb_printf(TPBM_PRTN_M_DIRECT, HLINE);
    return err;
}