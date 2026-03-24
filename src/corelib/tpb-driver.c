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
#ifdef __linux__
#include <linux/limits.h>
#else
#include <limits.h>
#endif
#include "tpb-public.h"
#include "tpb-driver.h"
#include "tpb-impl.h"
#include "tpb-io.h"
#include "tpb-dynloader.h"
#include "tpb-argp.h"
#include "tpb-autorecord.h"

/* Module-level state variables */
static int nkern = 0, nhdl, ihdl;  // number of registered kernels，number of handles， handle id

static tpb_kernel_t *kernel_all = NULL;  // array of all kernels
static tpb_kernel_t *current_kernel = NULL;  // kernel being registered
static tpb_k_rthdl_t *current_rthdl = NULL;  // runtime handle being tested
static tpb_kernel_t kernel_common;  // common parameters for all kernels
static tpb_timer_t timer;

/* Handle management state */
static tpb_k_rthdl_t *handle_list = NULL;  // array of runtime handles
static int timer_set = 0;                  // flag to track if timer is set
static int s_pli_handle_index = 0;         // 0-based handle index for auto-record

static int
_tpb_get_kernel_by_name(const char *name, tpb_kernel_t **kernel_out)
{
    if (name == NULL || kernel_out == NULL) {
        return TPBE_KERN_ARG_FAIL;
    }

    if (strcmp(kernel_common.info.name, name) == 0) {
        *kernel_out = &kernel_common;
        return 0;
    }

    for (int i = 0; i < nkern; i++) {
        if (strcmp(kernel_all[i].info.name, name) == 0) {
            *kernel_out = &kernel_all[i];
            return 0;
        }
    }

    return TPBE_LIST_NOT_FOUND;
}

static int
_tpb_get_kernel_by_index(int idx, tpb_kernel_t **kernel_out)
{
    if (kernel_out == NULL) {
        return TPBE_KERN_ARG_FAIL;
    }

    if (idx < 0 || idx >= nkern) {
        return TPBE_LIST_NOT_FOUND;
    }

    *kernel_out = &kernel_all[idx];
    return 0;
}

/**
 * @brief Deep copy kernel info from source to destination.
 *
 * Allocates and returns a new tpb_kernel_t with fully isolated copy:
 * - All arrays (parms, plims, outs) are newly allocated
 * - No pointers in dst point to driver-internal data
 *
 * @param src Source kernel (driver-internal)
 * @return Allocated deep copy on success, NULL on failure
 */
static tpb_kernel_t *
_tpb_deep_copy_kernel(const tpb_kernel_t *src)
{
    if (src == NULL) {
        return NULL;
    }

    tpb_kernel_t *dst = (tpb_kernel_t *)malloc(sizeof(tpb_kernel_t));
    if (dst == NULL) {
        return NULL;
    }

    /* Copy scalar fields in info */
    snprintf(dst->info.name, TPBM_NAME_STR_MAX_LEN, "%s", src->info.name);
    snprintf(dst->info.note, TPBM_NOTE_STR_MAX_LEN, "%s", src->info.note);
    memcpy(dst->info.kernel_id, src->info.kernel_id, 20);
    dst->info.kctrl = src->info.kctrl;
    dst->info.nparms = src->info.nparms;
    dst->info.nouts = src->info.nouts;

    /* Copy parms array */
    if (src->info.nparms > 0 && src->info.parms != NULL) {
        dst->info.parms = (tpb_rt_parm_t *)malloc(
            sizeof(tpb_rt_parm_t) * src->info.nparms);
        if (dst->info.parms == NULL) {
            free(dst);
            return NULL;
        }

        for (int i = 0; i < src->info.nparms; i++) {
            /* Copy scalar fields */
            snprintf(dst->info.parms[i].name, TPBM_NAME_STR_MAX_LEN, "%s",
                     src->info.parms[i].name);
            snprintf(dst->info.parms[i].note, TPBM_NOTE_STR_MAX_LEN, "%s",
                     src->info.parms[i].note);
            dst->info.parms[i].value = src->info.parms[i].value;
            dst->info.parms[i].ctrlbits = src->info.parms[i].ctrlbits;
            dst->info.parms[i].nlims = src->info.parms[i].nlims;

            /* Deep copy plims if present */
            if (src->info.parms[i].plims != NULL && src->info.parms[i].nlims > 0) {
                dst->info.parms[i].plims = (tpb_parm_value_t *)malloc(
                    sizeof(tpb_parm_value_t) * src->info.parms[i].nlims);
                if (dst->info.parms[i].plims == NULL) {
                    /* Clean up already allocated plims */
                    for (int j = 0; j < i; j++) {
                        if (dst->info.parms[j].plims != NULL) {
                            free(dst->info.parms[j].plims);
                        }
                    }
                    free(dst->info.parms);
                    free(dst);
                    return NULL;
                }
                for (int j = 0; j < src->info.parms[i].nlims; j++) {
                    dst->info.parms[i].plims[j] = src->info.parms[i].plims[j];
                }
            } else {
                dst->info.parms[i].plims = NULL;
            }
        }
    } else {
        dst->info.parms = NULL;
    }

    /* Copy outs array */
    if (src->info.nouts > 0 && src->info.outs != NULL) {
        dst->info.outs = (tpb_k_output_t *)malloc(
            sizeof(tpb_k_output_t) * src->info.nouts);
        if (dst->info.outs == NULL) {
            /* Clean up parms and plims */
            if (dst->info.parms != NULL) {
                for (int i = 0; i < dst->info.nparms; i++) {
                    if (dst->info.parms[i].plims != NULL) {
                        free(dst->info.parms[i].plims);
                    }
                }
                free(dst->info.parms);
            }
            free(dst);
            return NULL;
        }

        for (int i = 0; i < src->info.nouts; i++) {
            snprintf(dst->info.outs[i].name, TPBM_NAME_STR_MAX_LEN, "%s",
                     src->info.outs[i].name);
            snprintf(dst->info.outs[i].note, TPBM_NOTE_STR_MAX_LEN, "%s",
                     src->info.outs[i].note);
            dst->info.outs[i].dtype = src->info.outs[i].dtype;
            dst->info.outs[i].unit = src->info.outs[i].unit;
            dst->info.outs[i].n = src->info.outs[i].n;
            dst->info.outs[i].p = NULL;  /* Runtime data, not static info */
        }
    } else {
        dst->info.outs = NULL;
    }

    /* Copy function pointers */
    dst->func.k_output_decorator = src->func.k_output_decorator;

    return dst;
}

/* ========== Public API functions ========== */

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
tpb_k_get_timer(tpb_timer_t *timer_out)
{
    return tpb_driver_get_timer(timer_out);
}

int
tpb_driver_set_kernel_id(const char *kernel_name,
                         const unsigned char kernel_id[20])
{
    tpb_kernel_t *kernel = NULL;
    int err;

    if (kernel_name == NULL || kernel_id == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    err = _tpb_get_kernel_by_name(kernel_name, &kernel);
    if (err != 0) {
        return err;
    }

    /* Persist resolved KernelID for later handle/env propagation. */
    memcpy(kernel->info.kernel_id, kernel_id, 20);
    return 0;
}

int
tpb_get_nkern(void)
{
    return (int)nkern;
}

int
tpb_get_nhdl(void)
{
    return nhdl;
}

/* Register common parameters that apply to all kernels by default */
int
tpb_register_common()
{
    memset(&kernel_common, 0, sizeof(tpb_kernel_t));
    kernel_common.info.nparms = 3;
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
    kernel_common.info.parms[0].value.i64 = 10;
    kernel_common.info.parms[0].nlims = 2;
    kernel_common.info.parms[0].plims = (tpb_parm_value_t *)malloc(sizeof(tpb_parm_value_t) * 2);
    if (kernel_common.info.parms[0].plims == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    kernel_common.info.parms[0].plims[0].i64 = 1;
    kernel_common.info.parms[0].plims[1].i64 = 100000;

    /* twarm: warmup time in milliseconds */
    snprintf(kernel_common.info.parms[1].name, TPBM_NAME_STR_MAX_LEN, "twarm");
    snprintf(kernel_common.info.parms[1].note, TPBM_NOTE_STR_MAX_LEN, "Warm-up time in milliseconds");
    kernel_common.info.parms[1].ctrlbits = TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE;
    kernel_common.info.parms[1].value.i64 = 100;
    kernel_common.info.parms[1].nlims = 2;
    kernel_common.info.parms[1].plims = (tpb_parm_value_t *)malloc(sizeof(tpb_parm_value_t) * 2);
    if (kernel_common.info.parms[1].plims == NULL) {
        free (kernel_common.info.parms[0].plims);
        return TPBE_MALLOC_FAIL;
    }
    kernel_common.info.parms[1].plims[0].i64 = 0;
    kernel_common.info.parms[1].plims[1].i64 = 10000;

    /* total_memsize: memory size in KiB */
    snprintf(kernel_common.info.parms[2].name, TPBM_NAME_STR_MAX_LEN, "total_memsize");
    snprintf(kernel_common.info.parms[2].note, TPBM_NOTE_STR_MAX_LEN, "Memory size in KiB");
    kernel_common.info.parms[2].ctrlbits = TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_RANGE;
    kernel_common.info.parms[2].value.u64 = 32;
    kernel_common.info.parms[2].nlims = 2;
    kernel_common.info.parms[2].plims = (tpb_parm_value_t *)malloc(sizeof(tpb_parm_value_t) * 2);
    if (kernel_common.info.parms[2].plims == NULL) {
        free (kernel_common.info.parms[0].plims);
        free (kernel_common.info.parms[1].plims);
        return TPBE_MALLOC_FAIL;
    }
    kernel_common.info.parms[2].plims[0].f64 = 0.0009765625;
    kernel_common.info.parms[2].plims[1].f64 = DBL_MAX;

    return 0;
}

/* Initialize kernel registry and common parameters. */
int
tpb_register_kernel()
{
    int err;

    if (current_rthdl || kernel_all || handle_list) {
        err = TPBE_ILLEGAL_CALL;
        tpb_printf(TPBM_PRTN_M_TSTAG | tpb_get_err_exit_flag(err), "Illegal call to tpb_register_kernel().\n");
        return err;
    }

    nhdl = 0;
    ihdl = -1;

    /* Register common parameters */
    err = tpb_register_common();
    if (err != 0) {
        return err;
    }

    /* Dynamically scan for kernel shared libraries */
    err = tpb_dl_scan();
    if (err != 0) {
        return err;
    }

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

/**
 * @brief Free memory allocated by tpb_query_kernel().
 *
 * Frees all nested structures (parms, plims, outs) within the kernel instance.
 * Note: This does NOT free the kernel struct itself - that is the caller's
 * responsibility. Use this for both heap-allocated (from tpb_query_kernel)
 * and inline kernel structs (like hdl->kernel).
 *
 * To free a kernel allocated by tpb_query_kernel:
 *   tpb_free_kernel(kernel);
 *   free(kernel);  // if kernel was allocated on heap
 *
 * To clean up an inline kernel struct (like hdl->kernel):
 *   tpb_free_kernel(&hdl->kernel);
 *   // no free() needed - struct is inline
 *
 * @param kernel Kernel instance to clean up. Can be NULL (no-op).
 */
void
tpb_free_kernel(tpb_kernel_t *kernel)
{
    if (kernel == NULL) {
        return;
    }

    /* Free each parm's plims */
    if (kernel->info.parms != NULL) {
        for (int i = 0; i < kernel->info.nparms; i++) {
            if (kernel->info.parms[i].plims != NULL) {
                free(kernel->info.parms[i].plims);
                kernel->info.parms[i].plims = NULL;
            }
        }
        /* Free parms array */
        free(kernel->info.parms);
        kernel->info.parms = NULL;
    }

    /* Free outs array */
    if (kernel->info.outs != NULL) {
        free(kernel->info.outs);
        kernel->info.outs = NULL;
    }

    /* Zero counts */
    kernel->info.nparms = 0;
    kernel->info.nouts = 0;
}

/**
 * @brief Query kernel information by ID or name.
 *
 * Returns the total number of registered kernels. If kernel_out is non-NULL,
 * attempts to look up a kernel and allocate a fully isolated copy.
 *
 * @param id Kernel index (>=0). If id >= 0, kernel_name is ignored.
 *           If id < 0, kernel_name is used to look up the kernel.
 * @param kernel_name Kernel name (used only when id < 0). Can be NULL if id >= 0.
 * @param kernel_out Pointer to a NULL tpb_kernel_t* pointer. On success,
 *                   *kernel_out will point to an allocated kernel copy.
 *                   On failure or if kernel_out is NULL, *kernel_out is unchanged.
 *                   Caller must free with tpb_free_kernel().
 * @return Total number of registered kernels. Check *kernel_out to determine
 *         if the specific kernel lookup succeeded (non-NULL) or failed (NULL).
 */
int
tpb_query_kernel(int id, const char *kernel_name, tpb_kernel_t **kernel_out)
{
    const tpb_kernel_t *src = NULL;
    int err;

    /* Always return the total kernel count */
    int count = nkern;

    /* If kernel_out is NULL, just return count */
    if (kernel_out == NULL) {
        return count;
    }

    /* If *kernel_out is not NULL, it's an invalid call - return count */
    if (*kernel_out != NULL) {
        return count;
    }

    /* Lookup source kernel */
    if (id >= 0) {
        /* Use index - ignore kernel_name */
        err = _tpb_get_kernel_by_index(id, (tpb_kernel_t **)&src);
    } else {
        /* Use name */
        if (kernel_name == NULL) {
            /* Invalid lookup, return count with *kernel_out still NULL */
            return count;
        }
        err = _tpb_get_kernel_by_name(kernel_name, (tpb_kernel_t **)&src);
    }

    if (err != 0) {
        /* Kernel not found, *kernel_out stays NULL */
        return count;
    }

    /* Perform deep copy */
    *kernel_out = _tpb_deep_copy_kernel(src);
    if (*kernel_out == NULL) {
        /* Allocation failed, *kernel_out stays NULL */
        return count;
    }

    return count;
}

int
tpb_k_register(const char name[TPBM_NAME_STR_MAX_LEN], const char note[TPBM_NOTE_STR_MAX_LEN], TPB_K_CTRL kctrl)
{
    if (name == NULL) {
        return TPBE_KERN_ARG_FAIL;
    }
    
    /* Check if name is unique */
    for (int i = 0; i < nkern; i++) {
        if (strcmp(kernel_all[i].info.name, name) == 0) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, 
                      "At tpb_k_register: Kernel name \'%s\' already registered\n", name);
            return TPBE_LIST_DUP;
        }
    }
    
    /* Reallocate kernel array */
    kernel_all = (tpb_kernel_t *)realloc(kernel_all, sizeof(tpb_kernel_t) * (nkern + 1));
    if (kernel_all == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "At tpb_k_register: realloc filed for kernel %s, "
                   "current regisered kernel number is %llu\n", name, nkern);
        return TPBE_MALLOC_FAIL;
    }

    /* Initialize new kernel */
    current_kernel = &kernel_all[nkern];
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
               const char *default_val, TPB_DTYPE dtype, ...)
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
    if (name == NULL || default_val == NULL || note == NULL) {
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
            parm->value.i64 = atoll(default_val);
            break;
        case TPB_UINT8_T:
        case TPB_UINT16_T:
        case TPB_UINT32_T:
        case TPB_UINT64_T:
            parm->value.u64 = strtoull(default_val, NULL, 10);
            break;
        case TPB_FLOAT_T:
            parm->value.f32 = (float)atof(default_val);
            break;
        case TPB_DOUBLE_T:
            parm->value.f64 = atof(default_val);
            break;
        case TPB_CHAR_T:
            parm->value.c = default_val[0];
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
tpb_dtype_elem_size(TPB_DTYPE dtype, size_t *out)
{
    if (!out) return TPBE_NULLPTR_ARG;

    switch (dtype & TPB_PARM_TYPE_MASK) {
        case TPB_INT_T:
            *out = sizeof(int);
            break;
        case TPB_INT8_T:
        case TPB_UINT8_T:
        case TPB_CHAR_T:
            *out = 1;
            break;
        case TPB_INT16_T:
        case TPB_UINT16_T:
            *out = 2;
            break;
        case TPB_INT32_T:
        case TPB_UINT32_T:
            *out = 4;
            break;
        case TPB_FLOAT_T:
            *out = sizeof(float);
            break;
        case TPB_INT64_T:
        case TPB_UINT64_T:
        case TPB_DOUBLE_T:
            *out = sizeof(double);
            break;
        case TPB_LONG_DOUBLE_T:
            *out = sizeof(long double);
            break;
        default:
            return TPBE_LIST_NOT_FOUND;
    }
    return 0;
}

int
tpb_k_alloc_output(const char *name, uint64_t n, void *ptr)
{
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
            int err = tpb_dtype_elem_size(current_rthdl->respack.outputs[i].dtype,
                                          &elem_size);
            if (err != 0) {
                tpb_printf(TPBM_PRTN_M_DIRECT,
                           "In tpb_k_alloc_output: DTYPE 0x%08llx is not supported.\n",
                           (unsigned long long)current_rthdl->respack.outputs[i].dtype);
                return TPBE_LIST_NOT_FOUND;
            }

            current_rthdl->respack.outputs[i].p = malloc(n * elem_size);
            if (current_rthdl->respack.outputs[i].p == NULL) {
                return TPBE_MALLOC_FAIL;
            }
            current_rthdl->respack.outputs[i].n = n;

            *((void **)ptr) = current_rthdl->respack.outputs[i].p;
            return 0;
        }
    }

    return TPBE_LIST_NOT_FOUND;
}

int
tpb_driver_clean_handle(tpb_k_rthdl_t *hdl)
{
    /* !!! DO NOT FREE ANYTHING IN hdl->kernel !!! */

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
        /* Free plims for each argument */
        for (int i = 0; i < hdl->argpack.n; i++) {
            if (hdl->argpack.args[i].plims != NULL) {
                free(hdl->argpack.args[i].plims);
                hdl->argpack.args[i].plims = NULL;
            }
            hdl->argpack.args[i].nlims = 0;
        }
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
    err = _tpb_get_kernel_by_name(kernel_name, &kernel);
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

        /* Step 1: Copy kernel-specific parameters (value already holds the default) */
        for (int i = 0; i < k_nparms; i++) {
            memcpy(&hdl->argpack.args[i], &kernel->info.parms[i], sizeof(tpb_rt_parm_t));
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
        int err = _tpb_get_kernel_by_name(kernel_name, &kernel);
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

int
tpb_run_pli(tpb_k_rthdl_t *hdl)
{
    char *exec_path;
    char *full_cmd;
    pid_t pid;
    int status;
    const char *log_path;

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

    /* Build full command: [ENV=val ...] [mpirun <mpiargs>] <exec_path> <timer_name> <params...> */
    {
        char value_buf[256];
        char kernel_id_hex[41];
        size_t cmd_size = 8192;
        full_cmd = (char *)malloc(cmd_size);
        if (full_cmd == NULL) {
            return TPBE_MALLOC_FAIL;
        }
        full_cmd[0] = '\0';
        size_t pos = 0;

        pos += snprintf(full_cmd + pos, cmd_size - pos, "TPBENCH_TIMER=%s ", timer.name);
        tpb_rawdb_id_to_hex(hdl->kernel.info.kernel_id, kernel_id_hex);
        pos += snprintf(full_cmd + pos, cmd_size - pos,
                        "TPB_KERNEL_ID=%s ", kernel_id_hex);

        /* Inject auto-record env vars if a batch is active */
        const char *ar_bid = tpb_record_get_tbatch_id_hex();
        const char *ar_ws = tpb_record_get_workspace();
        if (ar_bid != NULL) {
            pos += snprintf(full_cmd + pos, cmd_size - pos,
                            "TPB_TBATCH_ID=%s ", ar_bid);
            pos += snprintf(full_cmd + pos, cmd_size - pos,
                            "TPB_HANDLE_INDEX=%d ", s_pli_handle_index);
            if (ar_ws != NULL) {
                pos += snprintf(full_cmd + pos, cmd_size - pos,
                                "TPB_WORKSPACE=%s ", ar_ws);
            }
        }

        for (int i = 0; i < hdl->envpack.n; i++) {
            pos += snprintf(full_cmd + pos, cmd_size - pos, "%s=%s ",
                            hdl->envpack.envs[i].name, hdl->envpack.envs[i].value);
        }

        if (hdl->mpipack.mpiargs != NULL && hdl->mpipack.mpiargs[0] != '\0') {
            pos += snprintf(full_cmd + pos, cmd_size - pos, "mpirun %s ", hdl->mpipack.mpiargs);
        }

        pos += snprintf(full_cmd + pos, cmd_size - pos, "%s %s", exec_path, timer.name);

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
            pos += snprintf(full_cmd + pos, cmd_size - pos, " %s", value_buf);
        }
    }

    /* Print full command for debugging/analysis */
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Exec: %s\n", full_cmd);

    /*
     * Close parent's log stream and publish path so the PLI child opens the same file
     * in append mode; reopen in the parent after fork.
     */
    tpb_log_cleanup();
    log_path = tpb_log_get_filepath();
    if (log_path != NULL) {
        setenv(TPB_LOG_FILE_ENV, log_path, 1);
    }

    pid = fork();
    if (pid < 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "fork() failed\n");
        (void)tpb_log_init();
        free(full_cmd);
        return TPBE_FILE_IO_FAIL;
    }

    if (pid == 0) {
        /* Execute via shell to handle env vars and mpiargs correctly */
        execl("/bin/sh", "sh", "-c", full_cmd, (char *)NULL);
        fprintf(stderr, "execl failed for /bin/sh\n");
        _exit(127);
    }

    if (tpb_log_init() != TPBE_SUCCESS) {
        fprintf(stderr, "Warning: could not reopen run log after fork\n");
    }

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
        /* Progress: i/(nhdl-1) instead of (i+1)/nhdl */
        tpb_printf(TPBM_PRTN_M_DIRECT, "Test %d/%d  \n", i, nhdl - 1);

        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Kernel %s started (PLI).\n",
                   handle->kernel.info.name);
        s_pli_handle_index = i - 1;
        err = tpb_run_pli(handle);

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
tpb_driver_reset_handles(void)
{
    /* Clean all handles except pseudo handle (index 0), reset handle list */
    if (handle_list == NULL || nhdl <= 1) {
        return 0;  /* Nothing to reset */
    }

    /* Clean handles from index 1 to nhdl-1 */
    for (int i = 1; i < nhdl; i++) {
        tpb_driver_clean_handle(&handle_list[i]);
    }

    /* Shrink handle_list to just pseudo handle */
    tpb_k_rthdl_t *new_list = (tpb_k_rthdl_t *)realloc(handle_list, sizeof(tpb_k_rthdl_t));
    if (new_list != NULL) {
        handle_list = new_list;
    }

    /* Reset handle count and current index */
    nhdl = 1;
    ihdl = 0;

    /* Clear pseudo handle's per-run settings (mpiargs, envs) */
    if (handle_list[0].mpipack.mpiargs != NULL) {
        free(handle_list[0].mpipack.mpiargs);
        handle_list[0].mpipack.mpiargs = NULL;
    }
    if (handle_list[0].envpack.envs != NULL) {
        free(handle_list[0].envpack.envs);
        handle_list[0].envpack.envs = NULL;
    }
    handle_list[0].envpack.n = 0;

    /* Reset current_rthdl to pseudo handle */
    current_rthdl = &handle_list[0];

    return 0;
}

int
tpb_k_finalize_pli(void)
{
    if (current_kernel == NULL) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                   "No kernel registered. Call tpb_k_register first.\n");
        return TPBE_KERN_ARG_FAIL;
    }

    /* Finalize registration: increment kernel count */
    nkern++;
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
    if (kernel_all == NULL || nkern == 0) {
        return TPBE_KERN_ARG_FAIL;
    }

    /* Use the most recently registered kernel */
    tpb_kernel_t *kernel = &kernel_all[nkern - 1];
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
            }
            /* else: memcpy already set value to the default from kernel template */
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
