/*
 * @file tpb-driver.c
 * @brief Main entry of benchmarking kernels.
 */

#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <float.h>
#include "tpb-driver.h"
#include "tpb-impl.h"
#include "tpb-io.h"
#include "tpb-types.h"
#include "kernels/kernels.h"

/* Internal status */
static uint64_t tpb_driver_nkern = 0;
static tpb_kernel_t *kernel_all = NULL;
/* Current kernel being registered */
static tpb_kernel_t *current_kernel = NULL;
/* Current runtime handle being tested */
static tpb_k_rthdl_t *current_rthdl = NULL;
/* Common parameters applied to all kernels by default */
static tpb_kernel_t kernel_common;
static tpb_timer_t timer;

int
tpb_driver_set_timer(tpb_timer_t timer_in)
{
    snprintf(timer.name, TPBM_NAME_STR_MAX_LEN, "%s", timer_in.name);
    timer.unit = timer_in.unit;
    timer.dtype = timer_in.dtype;
    timer.init = timer_in.init;
    timer.tick = timer_in.tick;
    timer.tock = timer_in.tock;
    timer.get_stamp = timer_in.get_stamp;

    return 0;
}

/**
 * @brief Register common parameters that apply to all kernels by default
 * @return Error code (0 on success)
 */
int
tpb_register_common()
{
    memset(&kernel_common, 0, sizeof(tpb_kernel_t));
    kernel_common.info.nparms = 4;
    kernel_common.info.parms = (tpb_rt_parm_t *)malloc(sizeof(tpb_rt_parm_t) * kernel_common.info.nparms);
    if(kernel_common.info.parms == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    sprintf(kernel_common.info.name, "tpb_common");

    memset(kernel_common.info.parms, 0,
           sizeof(tpb_rt_parm_t) * kernel_common.info.nparms);
    
    // ntest: number of test iterations
    snprintf(kernel_common.info.parms[0].name, TPBM_NAME_STR_MAX_LEN, "ntest");
    snprintf(kernel_common.info.parms[0].note, TPBM_NOTE_STR_MAX_LEN, "Number of test iterations");
    kernel_common.info.parms[0].dtype = TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE;
    kernel_common.info.parms[0].default_value.i64 = 10;
    kernel_common.info.parms[0].value.i64 = 10;
    kernel_common.info.parms[0].nlims = 2;
    kernel_common.info.parms[0].plims = (tpb_parm_value_t *)malloc(sizeof(tpb_parm_value_t) * 2);
    kernel_common.info.parms[0].plims[0].i64 = 1;
    kernel_common.info.parms[0].plims[1].i64 = 100000;
    
    // nskip: number of initial iterations to skip
    snprintf(kernel_common.info.parms[1].name, TPBM_NAME_STR_MAX_LEN, "nskip");
    snprintf(kernel_common.info.parms[1].note, TPBM_NOTE_STR_MAX_LEN, "Number of initial iterations to skip");
    kernel_common.info.parms[1].dtype = TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE;
    kernel_common.info.parms[1].default_value.i64 = 2;
    kernel_common.info.parms[1].value.i64 = 2;
    kernel_common.info.parms[1].nlims = 2;
    kernel_common.info.parms[1].plims = (tpb_parm_value_t *)malloc(sizeof(tpb_parm_value_t) * 2);
    kernel_common.info.parms[1].plims[0].i64 = 0;
    kernel_common.info.parms[1].plims[1].i64 = 1000;
    
    // twarm: warmup time in milliseconds
    snprintf(kernel_common.info.parms[2].name, TPBM_NAME_STR_MAX_LEN, "twarm");
    snprintf(kernel_common.info.parms[2].note, TPBM_NOTE_STR_MAX_LEN, "Warmup time in milliseconds");
    kernel_common.info.parms[2].dtype = TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE;
    kernel_common.info.parms[2].default_value.i64 = 100;
    kernel_common.info.parms[2].value.i64 = 100;
    kernel_common.info.parms[2].nlims = 2;
    kernel_common.info.parms[2].plims = (tpb_parm_value_t *)malloc(sizeof(tpb_parm_value_t) * 2);
    kernel_common.info.parms[2].plims[0].i64 = 0;
    kernel_common.info.parms[2].plims[1].i64 = 10000;
    
    // memsize: memory size in KiB
    snprintf(kernel_common.info.parms[3].name, TPBM_NAME_STR_MAX_LEN, "memsize");
    snprintf(kernel_common.info.parms[3].note, TPBM_NOTE_STR_MAX_LEN, "Memory size in KiB");
    kernel_common.info.parms[3].dtype = TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_RANGE;
    kernel_common.info.parms[3].default_value.u64 = 32;
    kernel_common.info.parms[3].value.u64 = 32;
    kernel_common.info.parms[3].nlims = 2;
    kernel_common.info.parms[3].plims = (tpb_parm_value_t *)malloc(sizeof(tpb_parm_value_t) * 2);
    kernel_common.info.parms[3].plims[0].f64 = 0.0009765625;
    kernel_common.info.parms[3].plims[1].f64 = DBL_MAX;
    
    return 0;
}

/**
 * @brief Initialize kernel registry by calling registration functions
 * @return Error code (0 on success)
 */
int
tpb_register_kernel()
{
    int err;

    // Initialize current handle to null
    current_rthdl = NULL;
    tpb_driver_nkern = 0; 
    // Free any existing kernel array
    if(kernel_all != NULL) {
        free(kernel_all);
        kernel_all = NULL;
    }
    
    // Register common parameters
    err = tpb_register_common();
    if(err != 0) {
        return err;
    }
    
    // Register triad kernel using new API
    err = register_triad();
    if(err != 0) {
        return err;
    }

    return 0;
}

int
tpb_get_kernel(const char *name, tpb_kernel_t **kernel_out)
{
    if(name == NULL || kernel_out == NULL) {
        return TPBE_KERN_ARG_FAIL;
    }

    if (strcmp(kernel_common.info.name, name) == 0) {
        *kernel_out = &kernel_common;
        return 0;
    }

    for(int i = 0; i < tpb_driver_nkern; i++) {
        if(strcmp(kernel_all[i].info.name, name) == 0) {
            *kernel_out = &kernel_all[i];
            return 0;
        }
    }

    return TPBE_LIST_NOT_FOUND;
}

int
tpb_run_kernel(tpb_k_rthdl_t *hdl)
{
    int err;

    if (hdl == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    current_rthdl = hdl;


    /* Initialize handle's respack from kernel's registered outputs */
    for (int i = 0; i < tpb_driver_nkern; i++) {
        if (strcmp(kernel_all[i].info.name, hdl->kernel.info.name) == 0) {
            int nouts = kernel_all[i].info.nouts;
            tpb_k_output_t *src_outs = kernel_all[i].info.outs;
            if (nouts > 0 && src_outs != NULL) {
                hdl->respack.n = nouts;
                hdl->respack.outputs = (tpb_k_output_t *)malloc(
                    sizeof(tpb_k_output_t) * nouts);
                if (hdl->respack.outputs == NULL) {
                    return TPBE_MALLOC_FAIL;
                }
                for (int j = 0; j < nouts; j++) {
                    memcpy(&hdl->respack.outputs[j], &src_outs[j],
                           sizeof(tpb_k_output_t));
                    hdl->respack.outputs[j].p = NULL;
                    hdl->respack.outputs[j].n = 0;

                    /* Resolve TPB_UNIT_TIMER: use the timer's unit */
                    if (src_outs[j].unit == TPB_UNIT_TIMER) {
                        hdl->respack.outputs[j].unit = timer.unit;
                    }
                }
            }
            break;
        }
    }

    if (hdl->kernel.func.k_run == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "Kernel %s has empty runner.\n", hdl->kernel.info.name);
        return TPBE_KERN_ARG_FAIL;
    }

    /* Print running message and kernel arguments */
    tpb_printf(TPBM_PRTN_M_TSTAG, "Running Kernel %s\n", hdl->kernel.info.name);
    tpb_report_args_cli(hdl);

    /* Call kernel runner */
    err = hdl->kernel.func.k_run();
    if (err) {
        if (tpb_get_err_exit_flag(err) == TPBE_WARN) {
            /* WARN case - print warning but continue */
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN, "Kernel %s: %s\n",
                       hdl->kernel.info.name, tpb_get_err_msg(err));
        } else {
            /* FAIL case - return immediately */
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "Kernel %s failed: %s\n",
                       hdl->kernel.info.name, tpb_get_err_msg(err));
            current_rthdl = NULL;
            return err;
        }
    }

    /* Print results and success message */
    tpb_report_result_cli(hdl);
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Kernel %s finished successfully.\n",
               hdl->kernel.info.name);
    current_rthdl = NULL;
    return err;
}

int
tpb_get_kernel_count(void)
{
    return tpb_driver_nkern; 
}

int
tpb_get_kernel_by_index(int idx, tpb_kernel_t **kernel_out)
{
    if(kernel_out == NULL) {
        return TPBE_KERN_ARG_FAIL;
    }

    if(idx < 0 || idx >= tpb_driver_nkern) {
        return TPBE_LIST_NOT_FOUND;
    }

    *kernel_out = &kernel_all[idx];
    return 0;
}

int
tpb_k_register(const char name[TPBM_NAME_STR_MAX_LEN], const char note[TPBM_NOTE_STR_MAX_LEN])
{
    if (name == NULL) {
        return TPBE_KERN_ARG_FAIL;
    }
    
    /* Check if name is unique */
    for (int i = 0; i < tpb_driver_nkern; i++) {
        if (strcmp(kernel_all[i].info.name, name) == 0) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, 
                      "At tpb_k_register: Kernel name '%s' already registered\n", name);
            return TPBE_LIST_DUP;
        }
    }
    
    /* Reallocate kernel array */
    kernel_all = (tpb_kernel_t *)realloc(kernel_all, sizeof(tpb_kernel_t) * (tpb_driver_nkern + 1));
    if (kernel_all == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "At tpb_k_register: realloc filed for kernel %s, "
                   "current regisered kernel number is %llu\n", name, tpb_driver_nkern);
        return TPBE_MALLOC_FAIL;
    }

    /* Initialize new kernel */
    current_kernel = &kernel_all[tpb_driver_nkern];
    memset(current_kernel, 0, sizeof(tpb_kernel_t));
    
    sprintf(current_kernel->info.name, "%s", name);
    sprintf(current_kernel->info.note, "%s", note);
    current_kernel->info.nparms = 0;
    current_kernel->info.parms = NULL;
    current_kernel->info.nouts = 0;
    current_kernel->info.outs = NULL;
    
    return 0;
}


int
tpb_k_add_parm(const char *name, const char *note,
               const char *default_value, TPB_DTYPE dtype, ...)
{
    int tpberr = 0;
    int nparms;
    tpb_rt_parm_t *parm;
    
    if (current_kernel == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, 
                  "No kernel registered. Call tpb_k_register first.\n");
        return TPBE_KERN_ARG_FAIL;
    }
    if (current_rthdl != NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                  "tpb_k_add_parm cannot be called during kernel execution.\n");
        return TPBE_ILLEGAL_CALL;
    }
    if (name == NULL || default_value == NULL || note == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    nparms = current_kernel->info.nparms;

    /* Reallocate parameter array */
    current_kernel->info.parms = (tpb_rt_parm_t *)realloc(current_kernel->info.parms, 
                                                          sizeof(tpb_rt_parm_t) * (nparms + 1));
    if (current_kernel->info.parms == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, 
                  "At tpb_k_add_parm: Failed to realloc for %s.\n", name);
        return TPBE_MALLOC_FAIL;
    }

    /* Get pointer AFTER realloc (memory may have moved) */
    parm = &current_kernel->info.parms[nparms];
    memset(parm, 0, sizeof(tpb_rt_parm_t));
    
    snprintf(parm->name, TPBM_NAME_STR_MAX_LEN, "%s", name);
    snprintf(parm->note, TPBM_NOTE_STR_MAX_LEN, "%s", note);
    
    // Handle TPB_DTYPE_TIMER_T: use the timer's dtype
    uint32_t type_code = (uint32_t)(dtype & TPB_PARM_TYPE_MASK);
    if (type_code == (TPB_DTYPE_TIMER_T & TPB_PARM_TYPE_MASK)) {
        // Replace TIMER_T with the actual timer dtype, preserving source and check flags
        parm->dtype = (dtype & ~TPB_PARM_TYPE_MASK) | (timer.dtype & TPB_PARM_TYPE_MASK);
        type_code = (uint32_t)(timer.dtype & TPB_PARM_TYPE_MASK);
    } else {
        parm->dtype = dtype;
    }
    va_list args;
    va_start(args, dtype);
    
    // Parse default value based on type
    switch(type_code) {
        case TPB_INT_T:
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
            parm->default_value.f32 = (float)atof(default_value);
            parm->value.f32 = parm->default_value.f32;
            break;
        case TPB_DOUBLE_T:
            parm->default_value.f64 = atof(default_value);
            parm->value.f64 = parm->default_value.f64;
            break;
        case TPB_CHAR_T:
            parm->default_value.c = default_value[0];
            parm->value.c = default_value[0];
            break;
        default:
            va_end(args);
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "At tpb_k_add_parm: unsupported type %llx\n", type_code);
            return TPBE_KERN_ARG_FAIL;
    }
    
    /* Handle validation parameters */
    uint32_t check_mode = (uint32_t)(dtype & TPB_PARM_CHECK_MASK);
    if (check_mode == TPB_PARM_RANGE) {
        /* Range validation: [lo, hi] */
        parm->nlims = 2;
        parm->plims = (tpb_parm_value_t *)malloc(sizeof(tpb_parm_value_t) * 2);
        
        /* Check float/double types first (type codes overlap with int/uint numerically) */
        if (type_code == TPB_DOUBLE_T || type_code == TPB_LONG_DOUBLE_T) {
            /* Double range bounds */
            parm->plims[0].f64 = va_arg(args, double);
            parm->plims[1].f64 = va_arg(args, double);
            if (parm->plims[0].f64 > parm->plims[1].f64) {
                tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "At tpb_k_add_parm: Illegal range\n");
                return TPBE_KERN_ARG_FAIL;
            }
        } else if (type_code == TPB_FLOAT_T) {
            /* Float range bounds */
            parm->plims[0].f32 = (float)va_arg(args, double);
            parm->plims[1].f32 = (float)va_arg(args, double);
            if (parm->plims[0].f32 > parm->plims[1].f32) {
                tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "At tpb_k_add_parm: Illegal range\n");
                return TPBE_KERN_ARG_FAIL;
            }
        } else if (type_code == TPB_INT_T || type_code == TPB_INT8_T || type_code == TPB_INT16_T ||
                   type_code == TPB_INT32_T || type_code == TPB_INT64_T) {
            /* Signed integer types use int64_t for range bounds */
            parm->plims[0].i64 = va_arg(args, int64_t);
            parm->plims[1].i64 = va_arg(args, int64_t);
            if (parm->plims[0].i64 > parm->plims[1].i64) {
                tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "At tpb_k_add_parm: Illegal range\n");
                return TPBE_KERN_ARG_FAIL;
            }
        } else if (type_code == TPB_UINT8_T || type_code == TPB_UINT16_T ||
                   type_code == TPB_UINT32_T || type_code == TPB_UINT64_T) {
            /* Unsigned integer types use uint64_t for range bounds */
            parm->plims[0].u64 = va_arg(args, uint64_t);
            parm->plims[1].u64 = va_arg(args, uint64_t);
            if (parm->plims[0].u64 > parm->plims[1].u64) {
                tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "At tpb_k_add_parm: Illegal range\n");
                return TPBE_KERN_ARG_FAIL;
            }
        } else if (type_code == TPB_CHAR_T) {
            /* Char between 2 alphabets */
            parm->plims[0].c = (char)va_arg(args, int);
            parm->plims[1].c = (char)va_arg(args, int);
            if (parm->plims[0].c > parm->plims[1].c) {
                tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "At tpb_k_add_parm: Illegal range\n");
                return TPBE_KERN_ARG_FAIL;
            }
        } else {
            // Empty
        }
    } else if (check_mode == TPB_PARM_LIST) {
        /* List validation: (n, plist) */
        parm->nlims = va_arg(args, int);
        
        /* Allocate and copy list values based on type */
        parm->plims = (tpb_parm_value_t *)malloc(
            sizeof(tpb_parm_value_t) * parm->nlims);
        
        /* Check float/double types first (type codes overlap with int/uint numerically) */
        if (type_code == TPB_FLOAT_T || type_code == TPB_DOUBLE_T || type_code == TPB_LONG_DOUBLE_T) {
            double *src = va_arg(args, double *);
            for (int i = 0; i < parm->nlims; i++) {
                parm->plims[i].f64 = src[i];
            }
        } else if (type_code == TPB_INT_T || type_code == TPB_INT8_T || type_code == TPB_INT16_T ||
                   type_code == TPB_INT32_T || type_code == TPB_INT64_T) {
            int64_t *src = va_arg(args, int64_t *);
            for (int i = 0; i < parm->nlims; i++) {
                parm->plims[i].i64 = src[i];
            }
        } else if (type_code == TPB_UINT8_T || type_code == TPB_UINT16_T ||
                   type_code == TPB_UINT32_T || type_code == TPB_UINT64_T) {
            uint64_t *src = va_arg(args, uint64_t *);
            for (int i = 0; i < parm->nlims; i++) {
                parm->plims[i].u64 = src[i];
            }
        } else if (type_code == TPB_CHAR_T) {
            char *src = va_arg(args, char *);
            for (int i = 0; i < parm->nlims; i++) {
                parm->plims[i].c = src[i];
            }
        }
    } else {
        /* No validation */
        parm->nlims = 0;
        parm->plims = NULL;
    }
    
    va_end(args);
    
    current_kernel->info.nparms++;
    return 0;
}

int
tpb_k_add_runner(int (*runner)(void))
{
    if (current_kernel == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, 
                  "No kernel registered. Call tpb_k_register first.\n");
        return TPBE_KERN_ARG_FAIL;
    }
    
    if (runner == NULL) {
        return TPBE_KERN_ARG_FAIL;
    }
    
    current_kernel->func.k_run = runner;

    /* Finalize registration: increment kernel count */
    tpb_driver_nkern++;
    current_kernel = NULL;

    return 0;
}


int
tpb_k_add_output(const char *name, const char *note, TPB_DTYPE dtype, TPB_UNIT_T unit)
{
    if (name == NULL || note == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    tpb_k_output_t *out = NULL;

    if (current_rthdl != NULL) {
        /* Runtime context: add output to handle's respack */
        tpb_respack_t *respack = &current_rthdl->respack;
        int new_n = respack->n + 1;
        respack->outputs = (tpb_k_output_t *)realloc(respack->outputs,
                           sizeof(tpb_k_output_t) * new_n);
        if (respack->outputs == NULL) {
            return TPBE_MALLOC_FAIL;
        }
        out = &respack->outputs[respack->n];
        respack->n = new_n;
    } else {
        /* Registration context: add output to kernel's info.outs */
        if (current_kernel == NULL) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                       "No kernel registered. Call tpb_k_register first.\n");
            return TPBE_KERN_ARG_FAIL;
        }
        int new_n = current_kernel->info.nouts + 1;
        current_kernel->info.outs = (tpb_k_output_t *)realloc(current_kernel->info.outs,
                                    sizeof(tpb_k_output_t) * new_n);
        if (current_kernel->info.outs == NULL) {
            return TPBE_MALLOC_FAIL;
        }
        out = &current_kernel->info.outs[current_kernel->info.nouts];
        current_kernel->info.nouts = new_n;
    }

    /* Add output definition - store original dtype/unit, resolve at runtime */
    memset(out, 0, sizeof(tpb_k_output_t));
    snprintf(out->name, TPBM_NAME_STR_MAX_LEN, "%s", name);
    snprintf(out->note, TPBM_NOTE_STR_MAX_LEN, "%s", note);
    if ((uint32_t)(dtype & TPB_PARM_TYPE_MASK) == (TPB_DTYPE_TIMER_T & TPB_PARM_TYPE_MASK)) {
        out->dtype = TPB_INT64_T;
    } else {
        out->dtype = dtype;
    }
    out->unit = unit;
    out->n = 0;
    out->p = NULL;

    return 0;
}

int
tpb_k_get_timer(tpb_timer_t *timer_out)
{
    if (timer_out == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    timer_out->init = timer.init;
    timer_out->tick = timer.tick;
    timer_out->tock = timer.tock;
    timer_out->get_stamp = timer.get_stamp;
    snprintf(timer_out->name, TPBM_NAME_STR_MAX_LEN, "%s", timer.name);
    timer_out->unit = timer.unit;
    timer_out->dtype = timer.dtype;

    return 0;
}

int
tpb_k_get_arg(const char *name, TPB_DTYPE dtype, void *argptr)
{
    int tpberr;

    if(current_rthdl == NULL || name == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    
    for(int i = 0; i < current_rthdl->argpack.n; i++) {
        if(strcmp(current_rthdl->argpack.args[i].name, name) == 0) {
            switch (dtype & TPB_PARM_TYPE_MASK) {
                case TPB_INT_T:
                    *((int *)argptr) = (int)current_rthdl->argpack.args[i].value.i64;
                    break;
                case TPB_INT64_T:
                    *((int64_t *)argptr) = current_rthdl->argpack.args[i].value.i64;
                    break;
                case TPB_UINT64_T:
                    *((uint64_t *)argptr) = (uint64_t)current_rthdl->argpack.args[i].value.u64;
                    break;
                case TPB_FLOAT_T:
                    *((float *)argptr) = current_rthdl->argpack.args[i].value.f32;
                    break;
                case TPB_DOUBLE_T:
                    *((double *)argptr) = current_rthdl->argpack.args[i].value.f64;
                    break;
                case TPB_CHAR_T:
                    *((char *)argptr) = current_rthdl->argpack.args[i].value.c;
                    break;
                default:
                    // Unknown type, return not found
                    tpb_printf(TPBM_PRTN_M_DIRECT, "DTYPE 0x%08llx is not supported.", dtype);
                    return TPBE_LIST_NOT_FOUND;
            }
            return 0;
        }
    }
    
    return TPBE_LIST_NOT_FOUND;
}

int
tpb_k_alloc_output(const char *name, uint64_t n, void *ptr)
{
    int tpberr = 0;
    size_t elem_size = 0;

    if(current_rthdl == NULL || name == NULL || ptr == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    for(int i = 0; i < current_rthdl->respack.n; i++) {
        if(strcmp(current_rthdl->respack.outputs[i].name, name) == 0) {
            // Determine element size based on dtype
            TPB_DTYPE dtype = current_rthdl->respack.outputs[i].dtype;
            switch (dtype & TPB_PARM_TYPE_MASK) {
                case TPB_INT_T:
                    elem_size = sizeof(int);
                    break;
                case TPB_INT8_T:
                case TPB_UINT8_T:
                case TPB_CHAR_T:
                    elem_size = 1;
                    break;
                case TPB_INT16_T:
                case TPB_UINT16_T:
                    elem_size = 2;
                    break;
                case TPB_INT32_T:
                case TPB_UINT32_T:
                case TPB_FLOAT_T:
                    elem_size = 4;
                    break;
                case TPB_INT64_T:
                case TPB_UINT64_T:
                case TPB_DOUBLE_T:
                    elem_size = 8;
                    break;
                case TPB_LONG_DOUBLE_T:
                    elem_size = sizeof(long double);
                    break;
                case TPB_STRING_T:
                    elem_size = sizeof(char *);
                    break;
                default:
                    tpb_printf(TPBM_PRTN_M_DIRECT, 
                               "In tpb_k_alloc_output: DTYPE 0x%08llx is not supported.\n",
                               dtype);
                    return TPBE_LIST_NOT_FOUND;
            }

            // Allocate memory for output data
            current_rthdl->respack.outputs[i].p = malloc(n * elem_size);
            if(current_rthdl->respack.outputs[i].p == NULL) {
                return TPBE_MALLOC_FAIL;
            }
            current_rthdl->respack.outputs[i].n = n;

            // Return pointer to caller
            *((void **)ptr) = current_rthdl->respack.outputs[i].p;
            return 0;
        }
    }

    return TPBE_LIST_NOT_FOUND;
}

int
tpb_clean_output(tpb_k_rthdl_t *hdl)
{
    if (hdl == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    /* Free individual output data pointers */
    for (int i = 0; i < hdl->respack.n; i++) {
        if (hdl->respack.outputs[i].p != NULL) {
            free(hdl->respack.outputs[i].p);
            hdl->respack.outputs[i].p = NULL;
        }
    }

    /* Free the outputs array itself */
    if (hdl->respack.outputs != NULL) {
        free(hdl->respack.outputs);
        hdl->respack.outputs = NULL;
    }
    hdl->respack.n = 0;

    return 0;
}