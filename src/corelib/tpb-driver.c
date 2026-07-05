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

/* Local Function Prototypes */

/*
 * Deep copy kernel metadata for query_kernel isolation.
 * Allocates a new tpb_kernel_t; parms, plims, and outs arrays are newly
 * allocated with no pointers into driver-internal data. Returns NULL on
 * failure or if src is NULL.
 */
static tpb_kernel_t *_sf_deep_copy_kernel(const tpb_kernel_t *src);

/*
 * Resolve registered kernel by index; writes pointer into driver storage.
 */
static int _sf_get_kernel_by_index(int idx, tpb_kernel_t **kernel_out);

/*
 * Resolve registered kernel by name (common block or kernel_all entry).
 */
static int _sf_get_kernel_by_name(const char *name, tpb_kernel_t **kernel_out);

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
static int g_dry_run_enabled = 0;

static int
_sf_get_kernel_by_name(const char *name, tpb_kernel_t **kernel_out)
{
    if (name == NULL || kernel_out == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERN_ARG_FAIL, NULL);
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

    TPB_FAIL(TPB_MOD_DRIVER, TPBE_LIST_NOT_FOUND, NULL);
}

static int
_sf_get_kernel_by_index(int idx, tpb_kernel_t **kernel_out)
{
    if (kernel_out == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERN_ARG_FAIL, NULL);
    }

    if (idx < 0 || idx >= nkern) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_LIST_NOT_FOUND, NULL);
    }

    *kernel_out = &kernel_all[idx];
    return 0;
}

static tpb_kernel_t *
_sf_deep_copy_kernel(const tpb_kernel_t *src)
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
    dst->info.kernel_record_ok = src->info.kernel_record_ok;
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

/* Public API */

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
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
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
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
    }

    err = _sf_get_kernel_by_name(kernel_name, &kernel);
    TPB_PROPAGATE(TPB_MOD_DRIVER, err, NULL);

    /* Persist resolved KernelID for later handle/env propagation. */
    memcpy(kernel->info.kernel_id, kernel_id, 20);
    return 0;
}

int
tpb_driver_set_kernel_record_ok(const char *kernel_name, int ok)
{
    tpb_kernel_t *kernel = NULL;
    int err;

    if (kernel_name == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
    }

    err = _sf_get_kernel_by_name(kernel_name, &kernel);
    TPB_PROPAGATE(TPB_MOD_DRIVER, err, NULL);

    kernel->info.kernel_record_ok = ok ? 1 : 0;
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
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_MALLOC_FAIL, NULL);
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
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_MALLOC_FAIL, NULL);
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
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_MALLOC_FAIL, NULL);
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
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_MALLOC_FAIL, NULL);
    }
    kernel_common.info.parms[2].plims[0].f64 = 0.0009765625;
    kernel_common.info.parms[2].plims[1].f64 = DBL_MAX;

    return 0;
}

static void
_sf_clear_kernel_registry(void)
{
    int i;

    if (kernel_all == NULL) {
        nkern = 0;
        current_kernel = NULL;
        return;
    }
    for (i = 0; i < nkern; i++) {
        tpb_free_kernel(&kernel_all[i]);
    }
    free(kernel_all);
    kernel_all = NULL;
    nkern = 0;
    current_kernel = NULL;
}

/* Initialize kernel registry and common parameters. */
int
tpb_register_kernel()
{
    int err;

    if (current_rthdl || handle_list) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_ILLEGAL_CALL,
                "Illegal call to tpb_register_kernel()");
    }
    if (kernel_all != NULL) {
        _sf_clear_kernel_registry();
    }

    nhdl = 0;
    ihdl = -1;

    err = tpb_register_common();
    TPB_PROPAGATE(TPB_MOD_DRIVER, err, NULL);

    err = tpb_dl_scan();
    TPB_PROPAGATE(TPB_MOD_DRIVER, err, NULL);

    handle_list = NULL;
    nhdl = 0;
    ihdl = -1;
    current_rthdl = NULL;

    return 0;
}

/**
 * @brief Register common parameters and scan only the named PLI kernels.
 */
int
tpb_register_kernels(int n, const char *const *names)
{
    int err;
    int i;

    if (current_rthdl || handle_list) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_ILLEGAL_CALL,
                "Illegal call to tpb_register_kernels()");
    }
    if (kernel_all != NULL) {
        _sf_clear_kernel_registry();
    }

    nhdl = 0;
    ihdl = -1;

    err = tpb_register_common();
    TPB_PROPAGATE(TPB_MOD_DRIVER, err, NULL);

    for (i = 0; i < n; i++) {
        if (names == NULL || names[i] == NULL) {
            TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
        }
        err = tpb_dl_scan_kernel(names[i]);
        TPB_PROPAGATE(TPB_MOD_DRIVER, err, NULL);
    }

    handle_list = NULL;
    nhdl = 0;
    ihdl = -1;
    current_rthdl = NULL;

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
        err = _sf_get_kernel_by_index(id, (tpb_kernel_t **)&src);
    } else {
        /* Use name */
        if (kernel_name == NULL) {
            /* Invalid lookup, return count with *kernel_out still NULL */
            return count;
        }
        err = _sf_get_kernel_by_name(kernel_name, (tpb_kernel_t **)&src);
    }

    if (err != 0) {
        /* Kernel not found, *kernel_out stays NULL */
        return count;
    }

    /* Perform deep copy */
    *kernel_out = _sf_deep_copy_kernel(src);
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
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERN_ARG_FAIL, NULL);
    }
    
    /* Check if name is unique */
    for (int i = 0; i < nkern; i++) {
        if (strcmp(kernel_all[i].info.name, name) == 0) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG, 
                      "At tpb_k_register: Kernel name \'%s\' already registered\n", name);
            TPB_FAIL(TPB_MOD_DRIVER, TPBE_LIST_DUP, NULL);
        }
    }
    
    /* Reallocate kernel array */
    kernel_all = (tpb_kernel_t *)realloc(kernel_all, sizeof(tpb_kernel_t) * (nkern + 1));
    if (kernel_all == NULL) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG, "At tpb_k_register: realloc filed for kernel %s, "
                   "current regisered kernel number is %llu\n", name, nkern);
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_MALLOC_FAIL, NULL);
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
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                   "No kernel registered. Call tpb_k_register first.\n");
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERN_ARG_FAIL, NULL);
    }
    if (current_rthdl != NULL) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                   "tpb_k_add_parm cannot be called during kernel execution.\n");
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_ILLEGAL_CALL, NULL);
    }
    if (name == NULL || default_val == NULL || note == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
    }

    nparms = current_kernel->info.nparms;

    /* Reallocate parameter array */
    current_kernel->info.parms = (tpb_rt_parm_t *)realloc(current_kernel->info.parms,
                                                          sizeof(tpb_rt_parm_t) * (nparms + 1));
    if (current_kernel->info.parms == NULL) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                   "At tpb_k_add_parm: Failed to realloc for %s.\n", name);
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_MALLOC_FAIL, NULL);
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
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG, "At tpb_k_add_parm: unsupported type %llx\n", type_code);
            TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERN_ARG_FAIL, NULL);
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
                tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG, "At tpb_k_add_parm: Illegal range\n");
                TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERN_ARG_FAIL, NULL);
            }
        } else if (type_code == TPB_FLOAT_T) {
            /* Float range bounds */
            parm->plims[0].f32 = (float)va_arg(args, double);
            parm->plims[1].f32 = (float)va_arg(args, double);
            if (parm->plims[0].f32 > parm->plims[1].f32) {
                tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG, "At tpb_k_add_parm: Illegal range\n");
                TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERN_ARG_FAIL, NULL);
            }
        } else if (type_code == TPB_INT_T || type_code == TPB_INT8_T || type_code == TPB_INT16_T ||
                   type_code == TPB_INT32_T || type_code == TPB_INT64_T) {
            /* Signed integer types use int64_t for range bounds */
            parm->plims[0].i64 = va_arg(args, int64_t);
            parm->plims[1].i64 = va_arg(args, int64_t);
            if (parm->plims[0].i64 > parm->plims[1].i64) {
                tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG, "At tpb_k_add_parm: Illegal range\n");
                TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERN_ARG_FAIL, NULL);
            }
        } else if (type_code == TPB_UINT8_T || type_code == TPB_UINT16_T ||
                   type_code == TPB_UINT32_T || type_code == TPB_UINT64_T) {
            /* Unsigned integer types use uint64_t for range bounds */
            parm->plims[0].u64 = va_arg(args, uint64_t);
            parm->plims[1].u64 = va_arg(args, uint64_t);
            if (parm->plims[0].u64 > parm->plims[1].u64) {
                tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG, "At tpb_k_add_parm: Illegal range\n");
                TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERN_ARG_FAIL, NULL);
            }
        } else if (type_code == TPB_CHAR_T) {
            /* Char between 2 alphabets */
            parm->plims[0].c = (char)va_arg(args, int);
            parm->plims[1].c = (char)va_arg(args, int);
            if (parm->plims[0].c > parm->plims[1].c) {
                tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG, "At tpb_k_add_parm: Illegal range\n");
                TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERN_ARG_FAIL, NULL);
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
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
    }

    tpb_k_output_t *out = NULL;

    if (current_rthdl != NULL) {
        /* Runtime context: add output to handle\'s respack */
        tpb_respack_t *respack = &current_rthdl->respack;
        int new_n = respack->n + 1;
        respack->outputs = (tpb_k_output_t *)realloc(respack->outputs,
                           sizeof(tpb_k_output_t) * new_n);
        if (respack->outputs == NULL) {
            TPB_FAIL(TPB_MOD_DRIVER, TPBE_MALLOC_FAIL, NULL);
        }
        out = &respack->outputs[respack->n];
        respack->n = new_n;
    } else {
        /* Registration context: add output to kernel\'s info.outs */
        if (current_kernel == NULL) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                       "No kernel registered. Call tpb_k_register first.\n");
            TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERN_ARG_FAIL, NULL);
        }
        int new_n = current_kernel->info.nouts + 1;
        current_kernel->info.outs = (tpb_k_output_t *)realloc(current_kernel->info.outs,
                                    sizeof(tpb_k_output_t) * new_n);
        if (current_kernel->info.outs == NULL) {
            TPB_FAIL(TPB_MOD_DRIVER, TPBE_MALLOC_FAIL, NULL);
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
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
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
                    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, "DTYPE 0x%08llx is not supported.", dtype);
                    TPB_FAIL(TPB_MOD_DRIVER, TPBE_LIST_NOT_FOUND, NULL);
            }
            return 0;
        }
    }

    TPB_FAIL(TPB_MOD_DRIVER, TPBE_LIST_NOT_FOUND, NULL);
}

int
tpb_dtype_elem_size(TPB_DTYPE dtype, size_t *out)
{
    if (!out) TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);

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
            TPB_FAIL(TPB_MOD_DRIVER, TPBE_LIST_NOT_FOUND, NULL);
    }
    return 0;
}

int
tpb_k_alloc_output(const char *name, uint64_t n, void *ptr)
{
    size_t elem_size = 0;

    if (current_rthdl == NULL || name == NULL || ptr == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
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
                tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                           "In tpb_k_alloc_output: DTYPE 0x%08llx is not supported.\n",
                           (unsigned long long)current_rthdl->respack.outputs[i].dtype);
                TPB_FAIL(TPB_MOD_DRIVER, TPBE_LIST_NOT_FOUND, NULL);
            }

            current_rthdl->respack.outputs[i].p = malloc(n * elem_size);
            if (current_rthdl->respack.outputs[i].p == NULL) {
                TPB_FAIL(TPB_MOD_DRIVER, TPBE_MALLOC_FAIL, NULL);
            }
            current_rthdl->respack.outputs[i].n = n;

            *((void **)ptr) = current_rthdl->respack.outputs[i].p;
            return 0;
        }
    }

    TPB_FAIL(TPB_MOD_DRIVER, TPBE_LIST_NOT_FOUND, NULL);
}

int
tpb_driver_clean_handle(tpb_k_rthdl_t *hdl)
{
    /* !!! DO NOT FREE ANYTHING IN hdl->kernel !!! */

    if (hdl == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
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

    /* Free wrapperpack.links */
    if (hdl->wrapperpack.links != NULL) {
        for (int i = 0; i < hdl->wrapperpack.nlinks; i++) {
            if (hdl->wrapperpack.links[i].args != NULL) {
                free(hdl->wrapperpack.links[i].args);
                hdl->wrapperpack.links[i].args = NULL;
            }
        }
        free(hdl->wrapperpack.links);
        hdl->wrapperpack.links = NULL;
    }
    hdl->wrapperpack.nlinks = 0;

    return 0;
}

int
tpb_driver_add_handle(const char *kernel_name)
{
    tpb_kernel_t *kernel = NULL;
    int err;

    if (kernel_name == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
    }

    /* Lookup kernel by name */
    err = _sf_get_kernel_by_name(kernel_name, &kernel);
    if (err != 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                   "Kernel \'%s\' not found.\n", kernel_name);
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERNEL_NE_FAIL, NULL);
    }

    /* For PLI kernels, require a registered runnable .so */
    if ((kernel->info.kctrl & TPB_KTYPE_MASK) == TPB_KTYPE_PLI) {
        if (!tpb_dl_is_complete(kernel_name)) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                       "Incomplete kernel: '%s' (missing or failed .so scan)\n",
                       kernel_name);
            TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERNEL_INCOMPLETE, NULL);
        }
    }

    /* Reallocate handle_list */
    tpb_k_rthdl_t *new_list = (tpb_k_rthdl_t *)realloc(handle_list,
                                                        sizeof(tpb_k_rthdl_t) * (nhdl + 1));
    if (new_list == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_MALLOC_FAIL, NULL);
    }
    handle_list = new_list;

    /* Initialize new handle */
    tpb_k_rthdl_t *hdl = &handle_list[nhdl];
    memset(hdl, 0, sizeof(tpb_k_rthdl_t));

    /* Copy kernel info */
    memcpy(&hdl->kernel, kernel, sizeof(tpb_kernel_t));

    /* Build argpack from kernel defaults only (no pseudo-handle inheritance) */
    int k_nparms = kernel->info.nparms;

    if (k_nparms > 0) {
        hdl->argpack.args = (tpb_rt_parm_t *)malloc(sizeof(tpb_rt_parm_t) * k_nparms);
        if (hdl->argpack.args == NULL) {
            TPB_FAIL(TPB_MOD_DRIVER, TPBE_MALLOC_FAIL, NULL);
        }
        hdl->argpack.n = k_nparms;

        for (int i = 0; i < k_nparms; i++) {
            memcpy(&hdl->argpack.args[i], &kernel->info.parms[i], sizeof(tpb_rt_parm_t));
        }
    } else {
        hdl->argpack.n = 0;
        hdl->argpack.args = NULL;
    }

    hdl->respack.n = 0;
    hdl->respack.outputs = NULL;

    hdl->envpack.n = 0;
    hdl->envpack.envs = NULL;

    hdl->wrapperpack.nlinks = 0;
    hdl->wrapperpack.links = NULL;

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
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_KARG_NE_FAIL, NULL);
    }

    if (kernel_name != NULL) {
        /* Search in registered kernel */
        tpb_kernel_t *kernel = NULL;
        int err = _sf_get_kernel_by_name(kernel_name, &kernel);
        if (err != 0) {
            TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERNEL_NE_FAIL, NULL);
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
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_KARG_NE_FAIL, NULL);
    } else {
        /* Search in current_rthdl */
        if (current_rthdl == NULL) {
            TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
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
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_KARG_NE_FAIL, NULL);
    }
}

int
tpb_driver_set_hdl_karg(const char *parm_name, void *v)
{
    if (parm_name == NULL || v == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
    }

    /* Check if handle_list or current handle is NULL */
    if (handle_list == NULL || current_rthdl == NULL) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                   "In tpb_driver_set_hdl_karg: Empty kernel running list.\n");
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_ILLEGAL_CALL, NULL);
    }

    /* Check if parameter exists in current handle */
    tpb_rt_parm_t *parm = NULL;
    for (int i = 0; i < current_rthdl->argpack.n; i++) {
        if (strcmp(current_rthdl->argpack.args[i].name, parm_name) == 0) {
            parm = &current_rthdl->argpack.args[i];
            break;
        }
    }

    if (parm == NULL) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                   "Parameter '%s' is not existed in the kernel.\n", parm_name);
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_KARG_NE_FAIL, NULL);
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
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_DTYPE_NOT_SUPPORTED, NULL);
    }

    /* Validate using TPB_PARM_CHECK_MASK */
    uint32_t check_mode = parm->ctrlbits & TPB_PARM_CHECK_MASK;
    if (check_mode == TPB_PARM_RANGE && parm->plims != NULL && parm->nlims == 2) {
        if (type_code == TPB_DOUBLE_T || type_code == TPB_LONG_DOUBLE_T) {
            if (parsed_value.f64 < parm->plims[0].f64 || parsed_value.f64 > parm->plims[1].f64) {
                tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                           "Parameter '%s' value %lf out of range [%lf, %lf]\n",
                           parm->name, parsed_value.f64, parm->plims[0].f64, parm->plims[1].f64);
                TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERN_ARG_FAIL, NULL);
            }
        } else if (type_code == TPB_FLOAT_T) {
            if (parsed_value.f32 < parm->plims[0].f32 || parsed_value.f32 > parm->plims[1].f32) {
                tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                           "Parameter '%s' value %f out of range [%f, %f]\n",
                           parm->name, parsed_value.f32, parm->plims[0].f32, parm->plims[1].f32);
                TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERN_ARG_FAIL, NULL);
            }
        } else if (type_code == TPB_INT_T || type_code == TPB_INT8_T || type_code == TPB_INT16_T ||
                   type_code == TPB_INT32_T || type_code == TPB_INT64_T) {
            if (parsed_value.i64 < parm->plims[0].i64 || parsed_value.i64 > parm->plims[1].i64) {
                tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                           "Parameter '%s' value %" PRId64 " out of range [%" PRId64 ", %" PRId64 "]\n",
                           parm->name, parsed_value.i64, parm->plims[0].i64, parm->plims[1].i64);
                TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERN_ARG_FAIL, NULL);
            }
        } else if (type_code == TPB_UINT8_T || type_code == TPB_UINT16_T ||
                   type_code == TPB_UINT32_T || type_code == TPB_UINT64_T) {
            if (parsed_value.u64 < parm->plims[0].u64 || parsed_value.u64 > parm->plims[1].u64) {
                tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT,
                           "Parameter '%s' value %" PRIu64 " out of range [%" PRIu64 ", %" PRIu64 "]\n",
                           parm->name, parsed_value.u64, parm->plims[0].u64, parm->plims[1].u64);
                TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERN_ARG_FAIL, NULL);
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
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
    }

    if (handle_list == NULL || current_rthdl == NULL) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                   "In tpb_driver_set_hdl_env: Empty kernel running list.\n");
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_ILLEGAL_CALL, NULL);
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
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_MALLOC_FAIL, NULL);
    }
    current_rthdl->envpack.envs = new_envs;

    /* Initialize new entry */
    tpb_env_entry_t *entry = &current_rthdl->envpack.envs[current_rthdl->envpack.n];
    snprintf(entry->name, TPBM_NAME_STR_MAX_LEN, "%s", env_name);
    snprintf(entry->value, TPBM_CLI_STR_MAX_LEN, "%s", env_value);

    current_rthdl->envpack.n = new_n;
    return 0;
}

static int
_sf_wrapperpack_free(tpb_wrapperpack_t *pack)
{
    if (pack == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
    }

    if (pack->links != NULL) {
        for (int i = 0; i < pack->nlinks; i++) {
            if (pack->links[i].args != NULL) {
                free(pack->links[i].args);
                pack->links[i].args = NULL;
            }
        }
        free(pack->links);
        pack->links = NULL;
    }
    pack->nlinks = 0;
    return 0;
}

static int
_sf_wrapperpack_copy(tpb_wrapperpack_t *dst, const tpb_wrapperpack_t *src)
{
    if (dst == NULL || src == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
    }

    (void)_sf_wrapperpack_free(dst);

    if (src->nlinks <= 0 || src->links == NULL) {
        return 0;
    }

    dst->links = (tpb_wrapper_link_t *)calloc((size_t)src->nlinks,
                                              sizeof(tpb_wrapper_link_t));
    if (dst->links == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_MALLOC_FAIL, NULL);
    }
    dst->nlinks = src->nlinks;

    for (int i = 0; i < src->nlinks; i++) {
        snprintf(dst->links[i].app, TPBM_NAME_STR_MAX_LEN, "%s",
                 src->links[i].app);
        if (src->links[i].args != NULL) {
            dst->links[i].args = strdup(src->links[i].args);
            if (dst->links[i].args == NULL) {
                (void)_sf_wrapperpack_free(dst);
                TPB_FAIL(TPB_MOD_DRIVER, TPBE_MALLOC_FAIL, NULL);
            }
        }
    }

    return 0;
}

static int
_sf_set_hdl_wrappers_on(tpb_k_rthdl_t *hdl,
                        const tpb_wrapper_link_t *links, int nlinks)
{
    tpb_wrapperpack_t new_pack;

    if (hdl == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
    }

    memset(&new_pack, 0, sizeof(new_pack));

    if (nlinks > 0 && links != NULL) {
        new_pack.links = (tpb_wrapper_link_t *)calloc((size_t)nlinks,
                                                        sizeof(tpb_wrapper_link_t));
        if (new_pack.links == NULL) {
            TPB_FAIL(TPB_MOD_DRIVER, TPBE_MALLOC_FAIL, NULL);
        }
        new_pack.nlinks = nlinks;

        for (int i = 0; i < nlinks; i++) {
            snprintf(new_pack.links[i].app, TPBM_NAME_STR_MAX_LEN, "%s",
                     links[i].app);
            if (links[i].args != NULL && links[i].args[0] != '\0') {
                new_pack.links[i].args = strdup(links[i].args);
                if (new_pack.links[i].args == NULL) {
                    (void)_sf_wrapperpack_free(&new_pack);
                    TPB_FAIL(TPB_MOD_DRIVER, TPBE_MALLOC_FAIL, NULL);
                }
            }
        }
    }

    (void)_sf_wrapperpack_free(&hdl->wrapperpack);
    hdl->wrapperpack = new_pack;
    return 0;
}

int
tpb_driver_set_hdl_wrappers(const tpb_wrapper_link_t *links, int nlinks)
{
    if (handle_list == NULL || current_rthdl == NULL) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                   "In tpb_driver_set_hdl_wrappers: Empty kernel running list.\n");
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_ILLEGAL_CALL, NULL);
    }

    return _sf_set_hdl_wrappers_on(current_rthdl, links, nlinks);
}

int
tpb_driver_set_hdl_wrappers_idx(int hdl_idx,
                                const tpb_wrapper_link_t *links, int nlinks)
{
    if (handle_list == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_ILLEGAL_CALL, NULL);
    }

    if (hdl_idx < 0 || hdl_idx >= nhdl) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERN_ARG_FAIL, NULL);
    }

    return _sf_set_hdl_wrappers_on(&handle_list[hdl_idx], links, nlinks);
}

int
tpb_driver_copy_hdl_from(int src_idx)
{
    int err;

    if (handle_list == NULL || current_rthdl == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_ILLEGAL_CALL, NULL);
    }

    if (src_idx < 0 || src_idx >= nhdl) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERN_ARG_FAIL, NULL);
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
            TPB_FAIL(TPB_MOD_DRIVER, TPBE_MALLOC_FAIL, NULL);
        }
        current_rthdl->envpack.n = src->envpack.n;

        for (int i = 0; i < src->envpack.n; i++) {
            memcpy(&current_rthdl->envpack.envs[i], &src->envpack.envs[i],
                   sizeof(tpb_env_entry_t));
        }
    }

    /* Copy wrapperpack */
    err = _sf_wrapperpack_copy(&current_rthdl->wrapperpack, &src->wrapperpack);
    TPB_PROPAGATE(TPB_MOD_DRIVER, err, NULL);

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
    const char *launch_path;
    char *full_cmd;
    char workspace[PATH_MAX];
    char kernel_id_hex[41];
    kernel_attr_t kernel_attr;
    void *kernel_data = NULL;
    uint64_t kernel_datasize = 0;
    pid_t pid;
    int status;
    int err;
    const char *ar_bid;
    const char *log_path;
    unsigned char zero_id[20] = {0};

    if (hdl == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
    }

    /* Get executable path */
    exec_path = (char *)tpb_dl_get_exec_path(hdl->kernel.info.name);
    if (exec_path == NULL) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                   "Incomplete kernel: '%s'\n", hdl->kernel.info.name);
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERNEL_INCOMPLETE, NULL);
    }

    launch_path = tpb_dl_get_pli_launch_path();
    if (launch_path == NULL && strstr(exec_path, ".so") != NULL) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                   "PLI launcher not found under %s/bin/tpbcli-pli-launcher\n",
                   tpb_dl_get_tpb_home());
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERNEL_INCOMPLETE, NULL);
    }

    ar_bid = tpb_record_get_tbatch_id_hex();
    if (ar_bid != NULL) {
        if (memcmp(hdl->kernel.info.kernel_id, zero_id, 20) == 0) {
            tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                       "Kernel %s has zero KernelID, stop before fork.\n",
                       hdl->kernel.info.name);
            TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERNEL_NE_FAIL, NULL);
        }

        err = tpb_raf_resolve_workspace(workspace, sizeof(workspace));
        if (err != TPBE_SUCCESS) {
            char ws_msg[256];

            snprintf(ws_msg, sizeof(ws_msg),
                     "Failed to resolve workspace for kernel %s",
                     hdl->kernel.info.name);
            TPB_PROPAGATE(TPB_MOD_DRIVER, err, ws_msg);
        }

        memset(&kernel_attr, 0, sizeof(kernel_attr));
        err = tpb_raf_record_read_kernel(workspace, hdl->kernel.info.kernel_id,
                                           &kernel_attr, &kernel_data,
                                           &kernel_datasize);
        if (kernel_attr.headers != NULL) {
            tpb_raf_free_headers(kernel_attr.headers, kernel_attr.nheader);
        }
        if (kernel_data != NULL) {
            free(kernel_data);
        }
        if (err != TPBE_SUCCESS) {
            char kid_msg[512];

            tpb_raf_id_to_hex(hdl->kernel.info.kernel_id, kernel_id_hex);
            snprintf(kid_msg, sizeof(kid_msg),
                     "Kernel %s has unrecorded KernelID=%s, stop before fork",
                     hdl->kernel.info.name, kernel_id_hex);
            TPB_PROPAGATE(TPB_MOD_DRIVER, err, kid_msg);
        }
    }

    /* Build full command: [ENV=val ...] [wrapper chain] <kernel_entry> <timer> <params...> */
    {
        char value_buf[256];
        size_t cmd_size = 8192;
        full_cmd = (char *)malloc(cmd_size);
        if (full_cmd == NULL) {
            TPB_FAIL(TPB_MOD_DRIVER, TPBE_MALLOC_FAIL, NULL);
        }
        full_cmd[0] = '\0';
        size_t pos = 0;

        pos += snprintf(full_cmd + pos, cmd_size - pos, "TPBENCH_TIMER=%s ", timer.name);
        tpb_raf_id_to_hex(hdl->kernel.info.kernel_id, kernel_id_hex);
        pos += snprintf(full_cmd + pos, cmd_size - pos,
                        "TPB_KERNEL_ID=%s ", kernel_id_hex);

        /* Inject auto-record env vars if a batch is active */
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

        for (int i = 0; i < hdl->wrapperpack.nlinks; i++) {
            pos += snprintf(full_cmd + pos, cmd_size - pos, "%s",
                            hdl->wrapperpack.links[i].app);
            if (hdl->wrapperpack.links[i].args != NULL &&
                hdl->wrapperpack.links[i].args[0] != '\0') {
                pos += snprintf(full_cmd + pos, cmd_size - pos, " %s",
                                hdl->wrapperpack.links[i].args);
            }
            pos += snprintf(full_cmd + pos, cmd_size - pos, " ");
        }

        if (launch_path != NULL && strstr(exec_path, ".so") != NULL) {
            pos += snprintf(full_cmd + pos, cmd_size - pos, "%s %s %s",
                            launch_path, exec_path, timer.name);
        } else {
            pos += snprintf(full_cmd + pos, cmd_size - pos, "%s %s",
                            exec_path, timer.name);
        }

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
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG, "Exec: %s\n", full_cmd);

    if (g_dry_run_enabled) {
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG,
                   "[DRY-RUN] Skipping fork for %s\n",
                   hdl->kernel.info.name);
        free(full_cmd);
        return 0;
    }

    /*
     * Close parent's log stream and publish path so the PLI child opens the same file
     * in append mode; reopen in the parent after fork.
     */
    tpblog_cleanup();
    log_path = tpblog_get_filepath();
    if (log_path != NULL) {
        setenv(TPBLOG_FILE_ENV, log_path, 1);
    }

    pid = fork();
    if (pid < 0) {
        (void)tpblog_init();
        free(full_cmd);
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_FILE_IO_FAIL, "fork() failed");
    }

    if (pid == 0) {
        /* Execute via shell to handle env vars and wrapper chain correctly */
        execl("/bin/sh", "sh", "-c", full_cmd, (char *)NULL);
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_DIRECT,
                        "execl failed for /bin/sh\n");
        _exit(127);
    }

    if (tpblog_init() != TPBE_SUCCESS) {
        tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN, TPBLOG_FLAG_DIRECT,
                        "Warning: could not reopen run log after fork\n");
    }

    waitpid(pid, &status, 0);

    /* Clean up */
    free(full_cmd);

    /* Check exit status */
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code != 0) {
            char exit_msg[256];

            snprintf(exit_msg, sizeof(exit_msg),
                     "Kernel %s exited with code %d",
                     hdl->kernel.info.name, exit_code);
            TPB_FAIL(TPB_MOD_DRIVER, exit_code, exit_msg);
        }
    } else if (WIFSIGNALED(status)) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                   "Kernel %s killed by signal %d\n",
                   hdl->kernel.info.name, WTERMSIG(status));
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERN_ARG_FAIL, NULL);
    }

    return 0;
}

int
tpb_driver_run_all(void)
{
    int err = 0;

    if (handle_list == NULL || nhdl <= 0) {
        tpblog_printf_f(TPB_LOG_LEVEL_WARN, TPBLOG_TYPE_WARN, TPBLOG_FLAG_TSTAG, "No kernels to run.\n");
        return 0;
    }

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, DHLINE "\n");
    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG, "Start driver runner.\n");

    for (int i = 0; i < nhdl; i++) {
        tpb_k_rthdl_t *handle = &handle_list[i];
        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_DIRECT, "Test %d/%d  \n", i + 1, nhdl);

        tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG, "Kernel %s started (PLI).\n",
                   handle->kernel.info.name);
        s_pli_handle_index = i;
        err = tpb_run_pli(handle);

        if (err != 0) {
            char fail_msg[256];

            snprintf(fail_msg, sizeof(fail_msg), "Kernel %s failed",
                     handle->kernel.info.name);
            TPB_PROPAGATE(TPB_MOD_DRIVER, err, fail_msg);
        }
    }

    tpblog_printf_f(TPB_LOG_LEVEL_INFO, TPBLOG_TYPE_INFO, TPBLOG_FLAG_TSTAG, "TPBench exit.\n");
    return 0;
}

void
tpb_driver_set_dry_run(int enabled)
{
    g_dry_run_enabled = (enabled != 0);
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
    /* No pseudo handle: leave current_rthdl NULL until add_handle */
    current_rthdl = NULL;
}

int
tpb_driver_reset_handles(void)
{
    if (handle_list == NULL || nhdl <= 0) {
        return 0;
    }

    for (int i = 0; i < nhdl; i++) {
        tpb_driver_clean_handle(&handle_list[i]);
    }

    free(handle_list);
    handle_list = NULL;
    nhdl = 0;
    ihdl = -1;
    current_rthdl = NULL;

    return 0;
}

int
tpb_k_finalize_pli(void)
{
    if (current_kernel == NULL) {
        tpblog_printf_f(TPB_LOG_LEVEL_ERROR, TPBLOG_TYPE_ERRO, TPBLOG_FLAG_TSTAG,
                   "No kernel registered. Call tpb_k_register first.\n");
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERN_ARG_FAIL, NULL);
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
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
    }

    /* Use the same timer setup logic as tpb_argp_set_timer */
    return tpb_argp_set_timer(timer_name);
}

int
tpb_k_pli_build_handle(tpb_k_rthdl_t *handle, int argc, char **argv)
{
    if (handle == NULL) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_NULLPTR_ARG, NULL);
    }

    /* Initialize handle first to ensure safe cleanup on error */
    memset(handle, 0, sizeof(tpb_k_rthdl_t));

    /* The current_kernel should be set by the kernel's registration */
    if (kernel_all == NULL || nkern == 0) {
        TPB_FAIL(TPB_MOD_DRIVER, TPBE_KERN_ARG_FAIL, NULL);
    }

    /* Use the most recently registered kernel */
    tpb_kernel_t *kernel = &kernel_all[nkern - 1];
    memcpy(&handle->kernel, kernel, sizeof(tpb_kernel_t));

    /* Build argpack from positional arguments */
    int nparms = kernel->info.nparms;
    if (nparms > 0) {
        handle->argpack.args = (tpb_rt_parm_t *)malloc(sizeof(tpb_rt_parm_t) * nparms);
        if (handle->argpack.args == NULL) {
            TPB_FAIL(TPB_MOD_DRIVER, TPBE_MALLOC_FAIL, NULL);
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
            TPB_FAIL(TPB_MOD_DRIVER, TPBE_MALLOC_FAIL, NULL);
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
