/*
 * tpb-driver.c
 * Main entry of benchmarking kernels.
 */

#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <float.h>
#include <unistd.h>
#include <sys/wait.h>
#include <linux/limits.h>
#include "tpb-driver.h"
#include "tpb-impl.h"
#include "tpb-io.h"
#include "tpb-types.h"
#include "tpb-dynloader.h"
#include "tpb-argp.h"

/* Module-level state variables */
static uint64_t tpb_driver_nkern = 0;  // number of registered kernels
static tpb_kernel_t *kernel_all = NULL;  // array of all kernels
static tpb_kernel_t *current_kernel = NULL;  // kernel being registered
static tpb_k_rthdl_t *current_rthdl = NULL;  // runtime handle being tested
static tpb_kernel_t kernel_common;  // common parameters for all kernels
static tpb_timer_t timer;

/* Handle management state */
static tpb_k_rthdl_t *handle_list = NULL;  // array of runtime handles
static int nhdl = 0;                       // number of handles
static int ihdl = -1;                      // current handle index
static int timer_set = 0;                  // flag to track if timer is set

/* Integration mode: PLI (default) or FLI */
static int integration_mode = TPB_INTEG_MODE_PLI;

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
    timer_set = 1;

    return 0;
}

int
tpb_driver_get_timer(tpb_timer_t *timer_out)
{
    if (timer_out == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    snprintf(timer_out->name, TPBM_NAME_STR_MAX_LEN, "%s", timer.name);
    timer_out->unit = timer.unit;
    timer_out->dtype = timer.dtype;
    timer_out->init = timer.init;
    timer_out->tick = timer.tick;
    timer_out->tock = timer.tock;
    timer_out->get_stamp = timer.get_stamp;

    return 0;
}

int
tpb_driver_get_nkern(void)
{
    return (int)tpb_driver_nkern;
}

int
tpb_driver_get_nhdl(void)
{
    return nhdl;
}

/* Register common parameters that apply to all kernels by default */
int
tpb_register_common()
{
    memset(&kernel_common, 0, sizeof(tpb_kernel_t));
    kernel_common.info.nparms = 4;
    kernel_common.info.parms = (tpb_rt_parm_t *)malloc(sizeof(tpb_rt_parm_t) * kernel_common.info.nparms);
    if (kernel_common.info.parms == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    sprintf(kernel_common.info.name, "_tpb_common");

    memset(kernel_common.info.parms, 0,
           sizeof(tpb_rt_parm_t) * kernel_common.info.nparms);

    /* ntest: number of test iterations */
    snprintf(kernel_common.info.parms[0].name, TPBM_NAME_STR_MAX_LEN, "ntest");
    snprintf(kernel_common.info.parms[0].note, TPBM_NOTE_STR_MAX_LEN, "Number of test iterations");
    kernel_common.info.parms[0].ctrlbits = TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE;
    kernel_common.info.parms[0].default_value.i64 = 10;
    kernel_common.info.parms[0].value.i64 = 10;
    kernel_common.info.parms[0].nlims = 2;
    kernel_common.info.parms[0].plims = (tpb_parm_value_t *)malloc(sizeof(tpb_parm_value_t) * 2);
    kernel_common.info.parms[0].plims[0].i64 = 1;
    kernel_common.info.parms[0].plims[1].i64 = 100000;

    // /* nskip: number of initial iterations to skip */
    // snprintf(kernel_common.info.parms[1].name, TPBM_NAME_STR_MAX_LEN, "nskip");
    // snprintf(kernel_common.info.parms[1].note, TPBM_NOTE_STR_MAX_LEN, "Number of initial iterations to skip");
    // kernel_common.info.parms[1].ctrlbits = TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE;
    // kernel_common.info.parms[1].default_value.i64 = 2;
    // kernel_common.info.parms[1].value.i64 = 2;
    // kernel_common.info.parms[1].nlims = 2;
    // kernel_common.info.parms[1].plims = (tpb_parm_value_t *)malloc(sizeof(tpb_parm_value_t) * 2);
    // kernel_common.info.parms[1].plims[0].i64 = 0;
    // kernel_common.info.parms[1].plims[1].i64 = 1000;

    /* twarm: warmup time in milliseconds */
    snprintf(kernel_common.info.parms[2].name, TPBM_NAME_STR_MAX_LEN, "twarm");
    snprintf(kernel_common.info.parms[2].note, TPBM_NOTE_STR_MAX_LEN, "Warm-up time in milliseconds");
    kernel_common.info.parms[2].ctrlbits = TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE;
    kernel_common.info.parms[2].default_value.i64 = 100;
    kernel_common.info.parms[2].value.i64 = 100;
    kernel_common.info.parms[2].nlims = 2;
    kernel_common.info.parms[2].plims = (tpb_parm_value_t *)malloc(sizeof(tpb_parm_value_t) * 2);
    kernel_common.info.parms[2].plims[0].i64 = 0;
    kernel_common.info.parms[2].plims[1].i64 = 10000;

    /* total_memsize: memory size in KiB */
    snprintf(kernel_common.info.parms[3].name, TPBM_NAME_STR_MAX_LEN, "total_memsize");
    snprintf(kernel_common.info.parms[3].note, TPBM_NOTE_STR_MAX_LEN, "Memory size in KiB");
    kernel_common.info.parms[3].ctrlbits = TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_RANGE;
    kernel_common.info.parms[3].default_value.u64 = 32;
    kernel_common.info.parms[3].value.u64 = 32;
    kernel_common.info.parms[3].nlims = 2;
    kernel_common.info.parms[3].plims = (tpb_parm_value_t *)malloc(sizeof(tpb_parm_value_t) * 2);
    kernel_common.info.parms[3].plims[0].f64 = 0.0009765625;
    kernel_common.info.parms[3].plims[1].f64 = DBL_MAX;

    return 0;
}

/* Initialize kernel registry and common parameters. */
int
tpb_register_kernel()
{
    int err;

    /* Initialize current handle to null */
    current_rthdl = NULL;
    tpb_driver_nkern = 0;

    /* Free any existing kernel array */
    if (kernel_all != NULL) {
        free(kernel_all);
        kernel_all = NULL;
    }

    /* Free any existing handle list */
    if (handle_list != NULL) {
        free(handle_list);
        handle_list = NULL;
    }
    nhdl = 0;
    ihdl = -1;

    /* Register common parameters */
    err = tpb_register_common();
    if (err != 0) {
        return err;
    }

    /* Register kernels based on integration mode */
    if (integration_mode == TPB_INTEG_MODE_PLI) {
        /* PLI mode: dynamically scan for kernels */
        err = tpb_dl_scan();
        if (err != 0) {
            return err;
        }
    }
    /* FLI mode: kernels are registered by tpbcli after this call */

    /* Create pseudo handle (ihdl=0) for _tpb_common */
    handle_list = (tpb_k_rthdl_t *)malloc(sizeof(tpb_k_rthdl_t));
    if (handle_list == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    memset(&handle_list[0], 0, sizeof(tpb_k_rthdl_t));

    /* Copy kernel_common info to pseudo handle */
    memcpy(&handle_list[0].kernel, &kernel_common, sizeof(tpb_kernel_t));

    /* Set pseudo handle's argpack parameter count to 0 */
    handle_list[0].argpack.n = 0;
    handle_list[0].argpack.args = NULL;

    handle_list[0].respack.n = 0;
    handle_list[0].respack.outputs = NULL;

    nhdl = 1;
    ihdl = 0;
    current_rthdl = &handle_list[0];
    current_kernel = &kernel_common;

    return 0;
}

int
tpb_get_kernel(const char *name, tpb_kernel_t **kernel_out)
{
    if (name == NULL || kernel_out == NULL) {
        return TPBE_KERN_ARG_FAIL;
    }

    if (strcmp(kernel_common.info.name, name) == 0) {
        *kernel_out = &kernel_common;
        return 0;
    }

    for (int i = 0; i < tpb_driver_nkern; i++) {
        if (strcmp(kernel_all[i].info.name, name) == 0) {
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


    /* Initialize handle\'s respack from kernel\'s registered outputs */
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

                    /* Resolve TPB_UNIT_TIMER: use the timer\'s unit, preserve attributes */
                    TPB_UNIT_T base_unit = src_outs[j].unit & ~TPB_UATTR_MASK;
                    if (base_unit == TPB_UNIT_TIMER) {
                        TPB_UNIT_T attrs = src_outs[j].unit & TPB_UATTR_MASK;
                        hdl->respack.outputs[j].unit = timer.unit | attrs;
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
    tpb_cliout_args(hdl);

    /* Call kernel runner */
    tpb_printf(TPBM_PRTN_M_DIRECT, "## Kernel logs\n");
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
    tpb_cliout_results(hdl);
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
    if (kernel_out == NULL) {
        return TPBE_KERN_ARG_FAIL;
    }

    if (idx < 0 || idx >= tpb_driver_nkern) {
        return TPBE_LIST_NOT_FOUND;
    }

    *kernel_out = &kernel_all[idx];
    return 0;
}

int
tpb_k_register(const char name[TPBM_NAME_STR_MAX_LEN], const char note[TPBM_NOTE_STR_MAX_LEN], TPB_K_CTRL kctrl)
{
    if (name == NULL) {
        return TPBE_KERN_ARG_FAIL;
    }
    
    /* Check if name is unique */
    for (int i = 0; i < tpb_driver_nkern; i++) {
        if (strcmp(kernel_all[i].info.name, name) == 0) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, 
                      "At tpb_k_register: Kernel name \'%s\' already registered\n", name);
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
    current_kernel->info.kctrl = kctrl;
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

    /* Handle TPB_DTYPE_TIMER_T: use the timer\'s dtype */
    uint32_t type_code = (uint32_t)(dtype & TPB_PARM_TYPE_MASK);
    if (type_code == (TPB_DTYPE_TIMER_T & TPB_PARM_TYPE_MASK)) {
        /* Replace TIMER_T with the actual timer dtype, preserving source and check flags */
        parm->ctrlbits = (dtype & ~TPB_PARM_TYPE_MASK) | (timer.dtype & TPB_PARM_TYPE_MASK);
        type_code = (uint32_t)(timer.dtype & TPB_PARM_TYPE_MASK);
    } else {
        parm->ctrlbits = dtype;
    }
    va_list args;
    va_start(args, dtype);

    /* Parse default value based on type */
    switch (type_code) {
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
            /* Empty */
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
        /* Runtime context: add output to handle\'s respack */
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
        /* Registration context: add output to kernel\'s info.outs */
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

    if (current_rthdl == NULL || name == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    for (int i = 0; i < current_rthdl->argpack.n; i++) {
        if (strcmp(current_rthdl->argpack.args[i].name, name) == 0) {
            switch (dtype & TPB_PARM_TYPE_MASK) {
                case TPB_INT_T:
                    *((int *)argptr) = (int)current_rthdl->argpack.args[i].value.i64;
                    break;
                case TPB_INT64_T:
                    *((int64_t *)argptr) = current_rthdl->argpack.args[i].value.i64;
                    break;
                case TPB_UINT8_T:
                    *((uint8_t *)argptr) = (uint8_t)current_rthdl->argpack.args[i].value.u64;
                    break;
                case TPB_UINT16_T:
                    *((uint16_t *)argptr) = (uint16_t)current_rthdl->argpack.args[i].value.u64;
                    break;
                case TPB_UINT32_T:
                    *((uint32_t *)argptr) = (uint32_t)current_rthdl->argpack.args[i].value.u64;
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
                    /* Unknown type, return not found */
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

    if (current_rthdl == NULL || name == NULL || ptr == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    for (int i = 0; i < current_rthdl->respack.n; i++) {
        if ((current_rthdl->respack.outputs[i].unit & TPB_UNAME_MASK) == TPB_UNAME_TIMERTIME) {
            current_rthdl->respack.outputs[i].unit = timer.unit;
        }
    }

    for (int i = 0; i < current_rthdl->respack.n; i++) {
        if (strcmp(current_rthdl->respack.outputs[i].name, name) == 0) {
            /* Determine element size based on dtype */
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
                    elem_size = 4;
                    break;
                case TPB_FLOAT_T:
                    elem_size = sizeof(float);
                    break;
                case TPB_INT64_T:
                case TPB_UINT64_T:
                case TPB_DOUBLE_T:
                    elem_size = sizeof(double);
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

            /* Allocate memory for output data */
            current_rthdl->respack.outputs[i].p = malloc(n * elem_size);
            if (current_rthdl->respack.outputs[i].p == NULL) {
                return TPBE_MALLOC_FAIL;
            }
            current_rthdl->respack.outputs[i].n = n;

            /* Return pointer to caller */
            *((void **)ptr) = current_rthdl->respack.outputs[i].p;
            return 0;
        }
    }

    return TPBE_LIST_NOT_FOUND;
}

int
tpb_driver_clean_handle(tpb_k_rthdl_t *hdl)
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

    /* Free argpack.args (allocated per handle during tpb_driver_add_handle) */
    if (hdl->argpack.args != NULL) {
        free(hdl->argpack.args);
        hdl->argpack.args = NULL;
    }
    hdl->argpack.n = 0;

    /* Free envpack.envs */
    if (hdl->envpack.envs != NULL) {
        free(hdl->envpack.envs);
        hdl->envpack.envs = NULL;
    }
    hdl->envpack.n = 0;

    /* Free mpipack.mpiargs */
    if (hdl->mpipack.mpiargs != NULL) {
        free(hdl->mpipack.mpiargs);
        hdl->mpipack.mpiargs = NULL;
    }

    return 0;
}

int
tpb_driver_add_handle(const char *kernel_name)
{
    tpb_kernel_t *kernel = NULL;
    int err;

    if (kernel_name == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    /* Lookup kernel by name */
    err = tpb_get_kernel(kernel_name, &kernel);
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "Kernel \'%s\' not found.\n", kernel_name);
        return TPBE_KERNEL_NE_FAIL;
    }

    /* For PLI kernels, check that both .so and .tpbx exist */
    if ((kernel->info.kctrl & TPB_KTYPE_MASK) == TPB_KTYPE_PLI) {
        if (!tpb_dl_is_complete(kernel_name)) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                       "Incomplete kernel: '%s' (missing .tpbx executable)\n", kernel_name);
            return TPBE_KERNEL_INCOMPLETE;
        }
    }

    /* Reallocate handle_list */
    tpb_k_rthdl_t *new_list = (tpb_k_rthdl_t *)realloc(handle_list,
                                                        sizeof(tpb_k_rthdl_t) * (nhdl + 1));
    if (new_list == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    handle_list = new_list;

    /* Initialize new handle */
    tpb_k_rthdl_t *hdl = &handle_list[nhdl];
    memset(hdl, 0, sizeof(tpb_k_rthdl_t));

    /* Copy kernel info */
    memcpy(&hdl->kernel, kernel, sizeof(tpb_kernel_t));

    /* Build argpack: inherit kernel's default values, then apply common parameters from pseudo handle */
    int k_nparms = kernel->info.nparms;

    if (k_nparms > 0) {
        hdl->argpack.args = (tpb_rt_parm_t *)malloc(sizeof(tpb_rt_parm_t) * k_nparms);
        if (hdl->argpack.args == NULL) {
            return TPBE_MALLOC_FAIL;
        }
        hdl->argpack.n = k_nparms;

        /* Step 1: Copy kernel-specific parameters with their default values */
        for (int i = 0; i < k_nparms; i++) {
            memcpy(&hdl->argpack.args[i], &kernel->info.parms[i], sizeof(tpb_rt_parm_t));
            /* Set value to default_value */
            hdl->argpack.args[i].value = kernel->info.parms[i].default_value;
        }

        /* Step 2: Check each argument's name and set common argument from pseudo handle */
        if (handle_list != NULL && nhdl > 0) {
            for (int i = 0; i < hdl->argpack.n; i++) {
                for (int j = 0; j < handle_list[0].argpack.n; j++) {
                    if (strcmp(hdl->argpack.args[i].name, handle_list[0].argpack.args[j].name) == 0) {
                        /* Apply value from pseudo handle (_tpb_common) */
                        hdl->argpack.args[i].value = handle_list[0].argpack.args[j].value;
                        break;
                    }
                }
            }
        }
    } else {
        hdl->argpack.n = 0;
        hdl->argpack.args = NULL;
    }

    hdl->respack.n = 0;
    hdl->respack.outputs = NULL;

    /* Initialize envpack and inherit from pseudo handle */
    hdl->envpack.n = 0;
    hdl->envpack.envs = NULL;

    if (handle_list != NULL && nhdl > 0 && handle_list[0].envpack.n > 0) {
        int env_n = handle_list[0].envpack.n;
        hdl->envpack.envs = (tpb_env_entry_t *)malloc(sizeof(tpb_env_entry_t) * env_n);
        if (hdl->envpack.envs == NULL) {
            return TPBE_MALLOC_FAIL;
        }
        hdl->envpack.n = env_n;
        for (int i = 0; i < env_n; i++) {
            memcpy(&hdl->envpack.envs[i], &handle_list[0].envpack.envs[i],
                   sizeof(tpb_env_entry_t));
        }
    }

    /* Initialize mpipack and inherit from pseudo handle */
    hdl->mpipack.mpiargs = NULL;

    if (handle_list != NULL && nhdl > 0 && handle_list[0].mpipack.mpiargs != NULL) {
        hdl->mpipack.mpiargs = strdup(handle_list[0].mpipack.mpiargs);
        if (hdl->mpipack.mpiargs == NULL) {
            return TPBE_MALLOC_FAIL;
        }
    }

    /* Set ihdl = nhdl, then increment nhdl */
    ihdl = nhdl;
    nhdl++;

    /* Set current_rthdl to newly created handle */
    current_rthdl = hdl;

    return 0;
}

int
tpb_driver_get_kparm_ptr(const char *kernel_name, const char *parm_name,
                         void **v, TPB_DTYPE *dtype)
{
    if (parm_name == NULL) {
        return TPBE_KARG_NE_FAIL;
    }

    if (kernel_name != NULL) {
        /* Search in registered kernel */
        tpb_kernel_t *kernel = NULL;
        int err = tpb_get_kernel(kernel_name, &kernel);
        if (err != 0) {
            return TPBE_KERNEL_NE_FAIL;
        }

        for (int i = 0; i < kernel->info.nparms; i++) {
            if (strcmp(kernel->info.parms[i].name, parm_name) == 0) {
                if (v != NULL) {
                    *v = &kernel->info.parms[i].value;
                }
                if (dtype != NULL) {
                    *dtype = kernel->info.parms[i].ctrlbits;
                }
                return 0;
            }
        }
        return TPBE_KARG_NE_FAIL;
    } else {
        /* Search in current_rthdl */
        if (current_rthdl == NULL) {
            return TPBE_NULLPTR_ARG;
        }

        for (int i = 0; i < current_rthdl->argpack.n; i++) {
            if (strcmp(current_rthdl->argpack.args[i].name, parm_name) == 0) {
                if (v != NULL) {
                    *v = &current_rthdl->argpack.args[i].value;
                }
                if (dtype != NULL) {
                    *dtype = current_rthdl->argpack.args[i].ctrlbits;
                }
                return 0;
            }
        }
        return TPBE_KARG_NE_FAIL;
    }
}

int
tpb_driver_set_hdl_karg(const char *parm_name, void *v)
{
    if (parm_name == NULL || v == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    /* Check if handle_list or current handle is NULL */
    if (handle_list == NULL || current_rthdl == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "In tpb_driver_set_hdl_karg: Empty kernel running list.\n");
        return TPBE_ILLEGAL_CALL;
    }

    /* Check if parameter exists in current handle */
    tpb_rt_parm_t *parm = NULL;
    for (int i = 0; i < current_rthdl->argpack.n; i++) {
        if (strcmp(current_rthdl->argpack.args[i].name, parm_name) == 0) {
            parm = &current_rthdl->argpack.args[i];
            break;
        }
    }

    if (parm != NULL) {
        /* Parameter found in current handle, set the value */
    } else if (strcmp(current_rthdl->kernel.info.name, "_tpb_common") == 0) {
        /* Parameter not found but kernel is _tpb_common, search in kernel info and add */
        for (int i = 0; i < kernel_common.info.nparms; i++) {
            if (strcmp(kernel_common.info.parms[i].name, parm_name) == 0) {
                /* Reallocate argpack to add new parameter */
                int new_n = current_rthdl->argpack.n + 1;
                tpb_rt_parm_t *new_args = (tpb_rt_parm_t *)realloc(
                    current_rthdl->argpack.args,
                    sizeof(tpb_rt_parm_t) * new_n);
                if (new_args == NULL) {
                    return TPBE_MALLOC_FAIL;
                }
                current_rthdl->argpack.args = new_args;
                
                /* Copy parameter from kernel_common */
                memcpy(&current_rthdl->argpack.args[current_rthdl->argpack.n],
                       &kernel_common.info.parms[i], sizeof(tpb_rt_parm_t));
                
                parm = &current_rthdl->argpack.args[current_rthdl->argpack.n];
                current_rthdl->argpack.n = new_n;
                break;
            }
        }
        
        if (parm == NULL) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                       "Parameter '%s' is not existed as a common parameter.\n", parm_name);
            return TPBE_KARG_NE_FAIL;
        }
    } else {
        /* Parameter not found and kernel is not _tpb_common */
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "Parameter '%s' is not existed in the kernel.\n", parm_name);
        return TPBE_KARG_NE_FAIL;
    }

    /* Parse value based on type */
    uint32_t type_code = parm->ctrlbits & TPB_PARM_TYPE_MASK;
    tpb_parm_value_t parsed_value;
    char *value_str = (char *)v;

    if (type_code == TPB_FLOAT_T || type_code == TPB_DOUBLE_T || type_code == TPB_LONG_DOUBLE_T) {
        parsed_value.f64 = strtod(value_str, NULL);
    } else if (type_code == TPB_INT_T || type_code == TPB_INT8_T || type_code == TPB_INT16_T ||
               type_code == TPB_INT32_T || type_code == TPB_INT64_T) {
        parsed_value.i64 = strtoll(value_str, NULL, 10);
    } else if (type_code == TPB_UINT8_T || type_code == TPB_UINT16_T ||
               type_code == TPB_UINT32_T || type_code == TPB_UINT64_T) {
        parsed_value.u64 = strtoull(value_str, NULL, 10);
    } else if (type_code == TPB_CHAR_T) {
        parsed_value.c = value_str[0];
    } else {
        return TPBE_DTYPE_NOT_SUPPORTED;
    }

    /* Validate using TPB_PARM_CHECK_MASK */
    uint32_t check_mode = parm->ctrlbits & TPB_PARM_CHECK_MASK;
    if (check_mode == TPB_PARM_RANGE && parm->plims != NULL && parm->nlims == 2) {
        if (type_code == TPB_DOUBLE_T || type_code == TPB_LONG_DOUBLE_T) {
            if (parsed_value.f64 < parm->plims[0].f64 || parsed_value.f64 > parm->plims[1].f64) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Parameter '%s' value %lf out of range [%lf, %lf]\n",
                           parm->name, parsed_value.f64, parm->plims[0].f64, parm->plims[1].f64);
                return TPBE_KERN_ARG_FAIL;
            }
        } else if (type_code == TPB_FLOAT_T) {
            if (parsed_value.f32 < parm->plims[0].f32 || parsed_value.f32 > parm->plims[1].f32) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Parameter '%s' value %f out of range [%f, %f]\n",
                           parm->name, parsed_value.f32, parm->plims[0].f32, parm->plims[1].f32);
                return TPBE_KERN_ARG_FAIL;
            }
        } else if (type_code == TPB_INT_T || type_code == TPB_INT8_T || type_code == TPB_INT16_T ||
                   type_code == TPB_INT32_T || type_code == TPB_INT64_T) {
            if (parsed_value.i64 < parm->plims[0].i64 || parsed_value.i64 > parm->plims[1].i64) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Parameter '%s' value %" PRId64 " out of range [%" PRId64 ", %" PRId64 "]\n",
                           parm->name, parsed_value.i64, parm->plims[0].i64, parm->plims[1].i64);
                return TPBE_KERN_ARG_FAIL;
            }
        } else if (type_code == TPB_UINT8_T || type_code == TPB_UINT16_T ||
                   type_code == TPB_UINT32_T || type_code == TPB_UINT64_T) {
            if (parsed_value.u64 < parm->plims[0].u64 || parsed_value.u64 > parm->plims[1].u64) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "Parameter '%s' value %" PRIu64 " out of range [%" PRIu64 ", %" PRIu64 "]\n",
                           parm->name, parsed_value.u64, parm->plims[0].u64, parm->plims[1].u64);
                return TPBE_KERN_ARG_FAIL;
            }
        }
    }

    parm->value = parsed_value;
    return 0;
}

int
tpb_driver_set_hdl_env(const char *env_name, const char *env_value)
{
    if (env_name == NULL || env_value == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    if (handle_list == NULL || current_rthdl == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "In tpb_driver_set_hdl_env: Empty kernel running list.\n");
        return TPBE_ILLEGAL_CALL;
    }

    /* Check if env var already exists in current handle, if so update it */
    for (int i = 0; i < current_rthdl->envpack.n; i++) {
        if (strcmp(current_rthdl->envpack.envs[i].name, env_name) == 0) {
            snprintf(current_rthdl->envpack.envs[i].value,
                     TPBM_CLI_STR_MAX_LEN, "%s", env_value);
            return 0;
        }
    }

    /* Add new env var */
    int new_n = current_rthdl->envpack.n + 1;
    tpb_env_entry_t *new_envs = (tpb_env_entry_t *)realloc(
        current_rthdl->envpack.envs,
        sizeof(tpb_env_entry_t) * new_n);
    if (new_envs == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    current_rthdl->envpack.envs = new_envs;

    /* Initialize new entry */
    tpb_env_entry_t *entry = &current_rthdl->envpack.envs[current_rthdl->envpack.n];
    snprintf(entry->name, TPBM_NAME_STR_MAX_LEN, "%s", env_name);
    snprintf(entry->value, TPBM_CLI_STR_MAX_LEN, "%s", env_value);

    current_rthdl->envpack.n = new_n;
    return 0;
}

int
tpb_driver_set_hdl_mpiargs(const char *mpiargs_str)
{
    if (mpiargs_str == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    if (handle_list == NULL || current_rthdl == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "In tpb_driver_set_hdl_mpiargs: Empty kernel running list.\n");
        return TPBE_ILLEGAL_CALL;
    }

    /* Free existing mpiargs if any */
    if (current_rthdl->mpipack.mpiargs != NULL) {
        free(current_rthdl->mpipack.mpiargs);
    }

    /* Set new mpiargs string */
    current_rthdl->mpipack.mpiargs = strdup(mpiargs_str);
    if (current_rthdl->mpipack.mpiargs == NULL) {
        return TPBE_MALLOC_FAIL;
    }

    return 0;
}

int
tpb_driver_append_hdl_mpiargs(const char *mpiargs_str)
{
    if (mpiargs_str == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    if (handle_list == NULL || current_rthdl == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "In tpb_driver_append_hdl_mpiargs: Empty kernel running list.\n");
        return TPBE_ILLEGAL_CALL;
    }

    if (current_rthdl->mpipack.mpiargs == NULL) {
        /* No existing mpiargs, just set */
        current_rthdl->mpipack.mpiargs = strdup(mpiargs_str);
        if (current_rthdl->mpipack.mpiargs == NULL) {
            return TPBE_MALLOC_FAIL;
        }
    } else {
        /* Concatenate with space separator */
        size_t old_len = strlen(current_rthdl->mpipack.mpiargs);
        size_t new_len = strlen(mpiargs_str);
        char *new_str = (char *)malloc(old_len + 1 + new_len + 1);
        if (new_str == NULL) {
            return TPBE_MALLOC_FAIL;
        }
        sprintf(new_str, "%s %s", current_rthdl->mpipack.mpiargs, mpiargs_str);
        free(current_rthdl->mpipack.mpiargs);
        current_rthdl->mpipack.mpiargs = new_str;
    }

    return 0;
}

const char *
tpb_driver_get_hdl_mpiargs(void)
{
    if (handle_list == NULL || current_rthdl == NULL) {
        return NULL;
    }
    return current_rthdl->mpipack.mpiargs;
}

int
tpb_driver_copy_hdl_from(int src_idx)
{
    if (handle_list == NULL || current_rthdl == NULL) {
        return TPBE_ILLEGAL_CALL;
    }

    if (src_idx < 0 || src_idx >= nhdl) {
        return TPBE_KERN_ARG_FAIL;
    }

    tpb_k_rthdl_t *src = &handle_list[src_idx];

    /* Copy argpack values (structures already allocated in add_handle) */
    for (int i = 0; i < current_rthdl->argpack.n && i < src->argpack.n; i++) {
        /* Find matching parameter by name and copy value */
        for (int j = 0; j < src->argpack.n; j++) {
            if (strcmp(current_rthdl->argpack.args[i].name,
                       src->argpack.args[j].name) == 0) {
                current_rthdl->argpack.args[i].value = src->argpack.args[j].value;
                break;
            }
        }
    }

    /* Copy envpack */
    if (src->envpack.n > 0 && src->envpack.envs != NULL) {
        /* Free existing envpack if any */
        if (current_rthdl->envpack.envs != NULL) {
            free(current_rthdl->envpack.envs);
        }

        current_rthdl->envpack.envs = (tpb_env_entry_t *)malloc(
            sizeof(tpb_env_entry_t) * src->envpack.n);
        if (current_rthdl->envpack.envs == NULL) {
            return TPBE_MALLOC_FAIL;
        }
        current_rthdl->envpack.n = src->envpack.n;

        for (int i = 0; i < src->envpack.n; i++) {
            memcpy(&current_rthdl->envpack.envs[i], &src->envpack.envs[i],
                   sizeof(tpb_env_entry_t));
        }
    }

    /* Copy mpipack */
    if (src->mpipack.mpiargs != NULL) {
        /* Free existing mpipack if any */
        if (current_rthdl->mpipack.mpiargs != NULL) {
            free(current_rthdl->mpipack.mpiargs);
        }

        current_rthdl->mpipack.mpiargs = strdup(src->mpipack.mpiargs);
        if (current_rthdl->mpipack.mpiargs == NULL) {
            return TPBE_MALLOC_FAIL;
        }
    }

    return 0;
}

int
tpb_driver_get_current_hdl_idx(void)
{
    return ihdl;
}

/**
 * @brief Build full command string for PLI execution.
 * Format: [ENV=val ...] [mpirun <mpiargs>] <exec_path> <timer_name> <params...>
 * Environment variables are prepended explicitly for visibility and to avoid
 * polluting the parent process environment.
 */
static char *
build_pli_command(tpb_k_rthdl_t *hdl, const char *exec_path, const char *timer_name)
{
    char value_buf[256];
    size_t cmd_size = 8192;  /* Initial buffer size */
    char *cmd = (char *)malloc(cmd_size);
    if (cmd == NULL) return NULL;
    cmd[0] = '\0';

    size_t pos = 0;

    /* 1. Add environment variables as prefix */
    /* TPBENCH_TIMER */
    pos += snprintf(cmd + pos, cmd_size - pos, "TPBENCH_TIMER=%s ", timer_name);

    /* envpack variables */
    for (int i = 0; i < hdl->envpack.n; i++) {
        pos += snprintf(cmd + pos, cmd_size - pos, "%s=%s ",
                        hdl->envpack.envs[i].name, hdl->envpack.envs[i].value);
    }

    /* 2. Add mpirun and mpiargs if MPI is used */
    if (hdl->mpipack.mpiargs != NULL && hdl->mpipack.mpiargs[0] != '\0') {
        pos += snprintf(cmd + pos, cmd_size - pos, "mpirun %s ", hdl->mpipack.mpiargs);
    }

    /* 3. Add executable path and timer name */
    pos += snprintf(cmd + pos, cmd_size - pos, "%s %s", exec_path, timer_name);

    /* 4. Add kernel parameters */
    for (int i = 0; i < hdl->argpack.n; i++) {
        tpb_rt_parm_t *parm = &hdl->argpack.args[i];
        uint32_t type_code = parm->ctrlbits & TPB_PARM_TYPE_MASK;

        if (type_code == TPB_FLOAT_T || type_code == TPB_DOUBLE_T || type_code == TPB_LONG_DOUBLE_T) {
            snprintf(value_buf, sizeof(value_buf), "%.15g", parm->value.f64);
        } else if (type_code == TPB_INT_T || type_code == TPB_INT8_T || type_code == TPB_INT16_T ||
                   type_code == TPB_INT32_T || type_code == TPB_INT64_T) {
            snprintf(value_buf, sizeof(value_buf), "%" PRId64, parm->value.i64);
        } else if (type_code == TPB_UINT8_T || type_code == TPB_UINT16_T ||
                   type_code == TPB_UINT32_T || type_code == TPB_UINT64_T) {
            snprintf(value_buf, sizeof(value_buf), "%" PRIu64, parm->value.u64);
        } else if (type_code == TPB_CHAR_T) {
            snprintf(value_buf, sizeof(value_buf), "%c", parm->value.c);
        } else {
            snprintf(value_buf, sizeof(value_buf), "0");
        }
        pos += snprintf(cmd + pos, cmd_size - pos, " %s", value_buf);
    }

    return cmd;
}

/* Run a PLI kernel via fork/exec */
static int
tpb_driver_run_pli(tpb_k_rthdl_t *hdl)
{
    char *exec_path;
    char *full_cmd;
    pid_t pid;
    int status;
    int pipefd[2];
    char buffer[4096];
    ssize_t nbytes;

    if (hdl == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    /* Get executable path */
    exec_path = (char *)tpb_dl_get_exec_path(hdl->kernel.info.name);
    if (exec_path == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "Incomplete kernel: '%s'\n", hdl->kernel.info.name);
        return TPBE_KERNEL_INCOMPLETE;
    }

    /* Build full command string with env vars, mpiargs, and kernel params */
    full_cmd = build_pli_command(hdl, exec_path, timer.name);
    if (full_cmd == NULL) {
        return TPBE_MALLOC_FAIL;
    }

    /* Print full command for debugging/analysis */
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Exec: %s\n", full_cmd);

    /* Create pipe for capturing child output */
    if (pipe(pipefd) == -1) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "pipe() failed\n");
        free(full_cmd);
        return TPBE_FILE_IO_FAIL;
    }

    /* Fork and exec */
    pid = fork();
    if (pid < 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "fork() failed\n");
        close(pipefd[0]);
        close(pipefd[1]);
        free(full_cmd);
        return TPBE_FILE_IO_FAIL;
    }

    if (pid == 0) {
        /* Child process: redirect stdout and stderr to pipe, then exec via shell */
        close(pipefd[0]);  /* Close read end */
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        /* Execute via shell to handle env vars and mpiargs correctly */
        execl("/bin/sh", "sh", "-c", full_cmd, (char *)NULL);
        fprintf(stderr, "execl failed for /bin/sh\n");
        _exit(127);
    }

    /* Parent process: close write end and read from pipe */
    close(pipefd[1]);

    /* Read and forward child output to both console and log */
    while ((nbytes = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[nbytes] = '\0';
        /* Write to console */
        fputs(buffer, stdout);
        fflush(stdout);
        /* Write to log file */
        tpb_log_write_output(buffer);
    }

    close(pipefd[0]);

    /* Wait for child to complete */
    waitpid(pid, &status, 0);

    /* Clean up */
    free(full_cmd);

    /* Check exit status */
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code != 0) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                       "Kernel %s exited with code %d\n",
                       hdl->kernel.info.name, exit_code);
            return exit_code;
        }
    } else if (WIFSIGNALED(status)) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "Kernel %s killed by signal %d\n",
                   hdl->kernel.info.name, WTERMSIG(status));
        return TPBE_KERN_ARG_FAIL;
    }

    return 0;
}

int
tpb_driver_run_all(void)
{
    int err = 0;

    if (handle_list == NULL || nhdl <= 1) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN, "No kernels to run.\n");
        return 0;
    }

    tpb_printf(TPBM_PRTN_M_DIRECT, DHLINE "\n");
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Start driver runner.\n");

    /* Loop from ihdl=1 to nhdl-1 (skip pseudo handle at index 0) */
    for (int i = 1; i < nhdl; i++) {
        tpb_k_rthdl_t *handle = &handle_list[i];
        TPB_K_CTRL ktype = handle->kernel.info.kctrl & TPB_KTYPE_MASK;

        /* Progress: i/(nhdl-1) instead of (i+1)/nhdl */
        tpb_printf(TPBM_PRTN_M_DIRECT, "# Test %d/%d  \n", i, nhdl - 1);

        if (ktype == TPB_KTYPE_PLI) {
            /* PLI mode: fork and exec */
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Kernel %s started (PLI).\n",
                       handle->kernel.info.name);
            err = tpb_driver_run_pli(handle);
        } else {
            /* FLI mode: call runner directly */
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Kernel %s started.\n",
                       handle->kernel.info.name);
            err = tpb_run_kernel(handle);
            tpb_driver_clean_handle(handle);
        }

        if (err != 0) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "Kernel %s failed.\n",
                       handle->kernel.info.name);
            return err;
        }
    }

    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "TPBench exit.\n");
    return 0;
}

/* PLI API implementations */

void
tpb_driver_enable_kernel_reg(void)
{
    /* Temporarily clear current_rthdl to allow kernel registration */
    current_rthdl = NULL;
}

void
tpb_driver_disable_kernel_reg(void)
{
    /* Restore current_rthdl to pseudo handle (index 0) if available */
    if (handle_list != NULL && nhdl > 0) {
        current_rthdl = &handle_list[0];
    }
}

int
tpb_driver_set_integ_mode(int mode)
{
    if (mode != TPB_INTEG_MODE_FLI && mode != TPB_INTEG_MODE_PLI) {
        return TPBE_KERN_ARG_FAIL;
    }
    integration_mode = mode;
    return 0;
}

int
tpb_driver_get_integ_mode(void)
{
    return integration_mode;
}

int
tpb_k_finalize_pli(void)
{
    if (current_kernel == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "No kernel registered. Call tpb_k_register first.\n");
        return TPBE_KERN_ARG_FAIL;
    }

    /* PLI kernels don't have a runner function */
    current_kernel->func.k_run = NULL;

    /* Finalize registration: increment kernel count */
    tpb_driver_nkern++;
    current_kernel = NULL;

    return 0;
}

int
tpb_k_pli_set_timer(const char *timer_name)
{
    if (timer_name == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    /* Use the same timer setup logic as tpb_argp_set_timer */
    return tpb_argp_set_timer(timer_name);
}

int
tpb_k_pli_build_handle(tpb_k_rthdl_t *handle, int argc, char **argv)
{
    if (handle == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    /* Initialize handle first to ensure safe cleanup on error */
    memset(handle, 0, sizeof(tpb_k_rthdl_t));

    /* The current_kernel should be set by the kernel's registration */
    if (kernel_all == NULL || tpb_driver_nkern == 0) {
        return TPBE_KERN_ARG_FAIL;
    }

    /* Use the most recently registered kernel */
    tpb_kernel_t *kernel = &kernel_all[tpb_driver_nkern - 1];
    memcpy(&handle->kernel, kernel, sizeof(tpb_kernel_t));

    /* Build argpack from positional arguments */
    int nparms = kernel->info.nparms;
    if (nparms > 0) {
        handle->argpack.args = (tpb_rt_parm_t *)malloc(sizeof(tpb_rt_parm_t) * nparms);
        if (handle->argpack.args == NULL) {
            return TPBE_MALLOC_FAIL;
        }
        handle->argpack.n = nparms;

        /* Copy parameter definitions and apply values from argv */
        for (int i = 0; i < nparms; i++) {
            memcpy(&handle->argpack.args[i], &kernel->info.parms[i], sizeof(tpb_rt_parm_t));

            /* Apply value from argv if provided, otherwise use default */
            if (i < argc && argv[i] != NULL) {
                uint32_t type_code = handle->argpack.args[i].ctrlbits & TPB_PARM_TYPE_MASK;
                tpb_parm_value_t *value = &handle->argpack.args[i].value;

                if (type_code == TPB_FLOAT_T || type_code == TPB_DOUBLE_T || type_code == TPB_LONG_DOUBLE_T) {
                    value->f64 = strtod(argv[i], NULL);
                } else if (type_code == TPB_INT_T || type_code == TPB_INT8_T || type_code == TPB_INT16_T ||
                           type_code == TPB_INT32_T || type_code == TPB_INT64_T) {
                    value->i64 = strtoll(argv[i], NULL, 10);
                } else if (type_code == TPB_UINT8_T || type_code == TPB_UINT16_T ||
                           type_code == TPB_UINT32_T || type_code == TPB_UINT64_T) {
                    value->u64 = strtoull(argv[i], NULL, 10);
                } else if (type_code == TPB_CHAR_T) {
                    value->c = argv[i][0];
                }
            } else {
                /* Use default value */
                handle->argpack.args[i].value = kernel->info.parms[i].default_value;
            }
        }
    } else {
        handle->argpack.n = 0;
        handle->argpack.args = NULL;
    }

    /* Build respack from kernel's registered outputs */
    int nouts = kernel->info.nouts;
    if (nouts > 0 && kernel->info.outs != NULL) {
        handle->respack.outputs = (tpb_k_output_t *)malloc(sizeof(tpb_k_output_t) * nouts);
        if (handle->respack.outputs == NULL) {
            return TPBE_MALLOC_FAIL;
        }
        handle->respack.n = nouts;

        for (int i = 0; i < nouts; i++) {
            memcpy(&handle->respack.outputs[i], &kernel->info.outs[i], sizeof(tpb_k_output_t));
            handle->respack.outputs[i].p = NULL;
            handle->respack.outputs[i].n = 0;

            /* Resolve TPB_UNIT_TIMER: use the timer's unit, preserve attributes */
            TPB_UNIT_T base_unit = kernel->info.outs[i].unit & ~TPB_UATTR_MASK;
            if (base_unit == TPB_UNIT_TIMER) {
                TPB_UNIT_T attrs = kernel->info.outs[i].unit & TPB_UATTR_MASK;
                handle->respack.outputs[i].unit = timer.unit | attrs;
            }
        }
    } else {
        handle->respack.n = 0;
        handle->respack.outputs = NULL;
    }

    /* Set as current handle for kernel API calls */
    current_rthdl = handle;

    return 0;
}
