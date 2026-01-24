/*
 * =================================================================================
 * TPBench - A high-precision throughputs benchmarking tool for scientific computing
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
 * 
 * =================================================================================
 * triad.c
 * Description: a[i] = b[i] + s * c[i]
 * Author: Key Liao
 * Modified: May. 21st, 2024
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <float.h>
#include "tpbench.h"

#define MALLOC(_A, _NARR)  (_A) = (double *)aligned_alloc(64, sizeof(double) * _NARR);   \
                            if((_A) == NULL) {                                  \
                                return  TPBE_MALLOC_FAIL;                            \
                            }

static double epsilon = 1.e-8;

// Forward declarations
int register_triad(void);
int _tpbk_register_triad(void);
int _tpbk_run_triad(void);
int tpbk_pli_register_triad(void);
static int d_triad(tpb_timer_t *timer, int ntest, double kib, uint32_t array_size, int64_t twarm_ms,
                   int64_t *tot_time, int64_t *step_time, uint64_t *real_total_memsize,
                   uint32_t *array_size_out, double *bw);
static int check_d_triad(int narr, int ntest, double *a, double *b, double *c, double s, double epsilon, double *errval);

int
register_triad(void)
{
    /* Wrapper function for backward compatibility */
    return _tpbk_register_triad();
}

/* PLI registration: register only name, note, and parameters (no outputs, no runner) */
int
tpbk_pli_register_triad(void)
{
    int err;

    /* Register to TPBench as PLI kernel */
    err = tpb_k_register("triad", "STREAM Triad: a[i] = b[i] + s * c[i]", TPB_KTYPE_PLI);
    if (err != 0) return err;

    /* Kernel input parameters - same as FLI registration */
    err = tpb_k_add_parm("ntest", "Number of test iterations", "10",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)1, (int64_t)100000);
    if (err != 0) return err;
    err = tpb_k_add_parm("total_memsize", "Memory size in KiB", "32",
                         TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_RANGE,
                         0.0009765625, DBL_MAX);
    if (err != 0) return err;
    err = tpb_k_add_parm("array_size", "Number of elements per array (0 = use total_memsize)", "0",
                         TPB_PARM_CLI | TPB_UINT32_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)4294967295);
    if (err != 0) return err;
    err = tpb_k_add_parm("twarm", "Warm-up time in milliseconds, 0<=twarm<=10000, default 1", "1",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)600000);
    if (err != 0) return err;

    /* NO outputs registered here - kernel registers them in its own process */
    /* NO runner function - PLI uses exec */

    return tpb_k_finalize_pli();
}

int
_tpbk_register_triad(void)
{
    int err;
    
    /* Register to TPBench */
    err = tpb_k_register("triad", "STREAM Triad: a[i] = b[i] + s * c[i]", TPB_KTYPE_FLI);
    if(err != 0) return err;

    /* Kernel input parameters */
    err = tpb_k_add_parm("ntest", "Number of test iterations", "10",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)1, (int64_t)100000);
    if(err != 0) return err;
    err = tpb_k_add_parm("total_memsize", "Memory size in KiB", "32",
                         TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_RANGE,
                         0.0009765625, DBL_MAX);
    if(err != 0) return err;
    err = tpb_k_add_parm("array_size", "Number of elements per array (0 = use total_memsize)", "0",
                         TPB_PARM_CLI | TPB_UINT32_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)4294967295);
    if(err != 0) return err;
    err = tpb_k_add_parm("twarm", "Warm-up time in milliseconds, 0<=twarm<=10000, default 1", "1",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)600000);
    if (err != 0) return err;

    /* Kernel outputs */
    err = tpb_k_add_output("tot_time", "Measured runtime of the outer loop (all steps).", 
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_POINT);
    if(err != 0) return err;
    err = tpb_k_add_output("step_time", "Measured runtime of per loop step.", 
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
    if(err != 0) return err;
    err = tpb_k_add_output("real_total_memsize", "Actual memory footprint of three triad arrays.",
                           TPB_UINT64_T, TPB_UNIT_B | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT );
    if(err != 0) return err;
    err = tpb_k_add_output("array_size", "Actual number of elements per array.",
                           TPB_UINT32_T, TPB_UNAME_UNDEF | TPB_UBASE_BASE | TPB_UATTR_CAST_N | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if(err != 0) return err;
    // Set runner function.
    err = tpb_k_add_runner(_tpbk_run_triad);
    if(err != 0) return err;
    
    return 0;
}

int
_tpbk_run_triad(void)
{
    int tpberr;
    /* Input */
    int ntest;
    TPB_UNIT_T tpb_uname;
    tpb_timer_t timer;
    double total_memsize;
    uint32_t array_size;
    int64_t twarm_ms;
    /* Output */
    void *tot_time = NULL;
    void *step_time = NULL;
    uint64_t *real_total_memsize = NULL;
    double *bw = NULL;

    /* Get timer */
    tpberr = tpb_k_get_timer(&timer);
    if (tpberr) return tpberr;

    /* Get arguments by names */
    tpberr = tpb_k_get_arg("ntest", TPB_INT64_T, (void *)&ntest);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("total_memsize", TPB_DOUBLE_T, (void *)&total_memsize);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("array_size", TPB_UINT32_T, (void *)&array_size);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("twarm", TPB_INT64_T, (void *)&twarm_ms);
    if (tpberr) return tpberr;

    /* Malloc callbacks for kernel\'s outputs */
    tpberr = tpb_k_alloc_output("tot_time", 1, &tot_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("step_time", ntest, &step_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("real_total_memsize", 1, &real_total_memsize);
    if (tpberr) return tpberr;
    uint32_t *array_size_out = NULL;
    tpberr = tpb_k_alloc_output("array_size", 1, &array_size_out);
    if (tpberr) return tpberr;

    /* Measured data throughput rate is a derived metrics, adding at run-time */
    tpb_uname = timer.unit & TPB_UNAME_MASK;
    if (tpb_uname == TPB_UNAME_WALLTIME) {
        tpb_k_add_output("bw_walltime", "Measured sustainable memory bandwidth in decimal based MB/s.", 
                         TPB_DOUBLE_T, TPB_UNIT_MBPS | TPB_UATTR_CAST_N | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("bw_walltime", ntest, &bw);
        if (tpberr) return tpberr;
    } else if (tpb_uname == TPB_UNAME_PHYSTIME) {
        tpb_k_add_output("bw_phystime", "Measured sustainable memory bandwidth in binay based Byte/cy.", 
                         TPB_DOUBLE_T, TPB_UNIT_BYTEPCY | TPB_UATTR_CAST_N | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("bw_phystime", ntest, &bw);
        if (tpberr) return tpberr;
    } else {
        tpb_printf(TPBM_PRTN_M_DIRECT, "In kernel triad: unknown timer unit name %llx", tpb_uname);
        return TPBE_KERN_ARG_FAIL;
    }

    /* Call the actual kernel implementation */
    tpberr = d_triad(&timer, ntest, total_memsize, array_size, twarm_ms,
                     tot_time, step_time, real_total_memsize, array_size_out, bw);

    return tpberr;
}

static int
d_triad(tpb_timer_t *timer, int ntest, double kib, uint32_t array_size, int64_t twarm_ms,
        int64_t *tot_time, int64_t *step_time, uint64_t *real_total_memsize,
        uint32_t *array_size_out, double *bw) {
    int narr, err;
    double *a, *b, *c;
    double s = 0.42;
    uint64_t t0, t1;

    err = 0;
    /* Use array_size if specified (non-zero), otherwise use total_memsize */
    if (array_size > 0) {
        narr = array_size;
    } else {
        narr = (int)(kib * 1024 / sizeof(double) / 3);
    }
    *real_total_memsize = narr * sizeof(double) * 3;
    *array_size_out = narr;

    MALLOC(a, narr);
    MALLOC(b, narr);
    MALLOC(c, narr);

    for(int i = 0; i < narr; i ++) {
        a[i] = 1.0;//s;
        b[i] = 2.0;//s;// + i;
        c[i] = 3.0;//s;//i;
    }

    // kernel warm
    struct timespec wts;
    uint64_t wns0, wns1;

    clock_gettime(CLOCK_MONOTONIC, &wts);
    wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    wns1 = wns0 + (uint64_t)twarm_ms * 1000000ULL;
    while(wns0 < wns1) {
        #pragma omp parallel for shared(a, b, c, s, narr)
        for(int j = 0; j < narr; j ++){
            a[j] = b[j] + s * c[j];
        }
        clock_gettime(CLOCK_MONOTONIC, &wts);
        wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    }

    /* Reset arrays to initial values after warm-up for verification */
    for(int i = 0; i < narr; i ++) {
        a[i] = 1.0;
        b[i] = 2.0;
        c[i] = 3.0;
    }

    timer->init();

    // kernel start
    timer->tick(tot_time);
    for(int i = 0; i < ntest; i ++){
        timer->tick(step_time + i);
        #pragma omp parallel for shared(a, b, c, s, narr)
        for(int j = 0; j < narr; j ++){
            a[j] = b[j] + s * c[j];
        }
        #pragma omp barrier
        timer->tock(step_time+i);
    }
    timer->tock(tot_time);
    for (int i = 0; i < ntest; i ++) {
        bw[i] = ((double)(*real_total_memsize) * 1e-6)  / ((double)(step_time[i]) * 1e-9);
    }
    /* Verify results. */
    double errval;
    err = check_d_triad(narr, ntest, a, b, c, s, epsilon, &errval);
    tpb_printf(TPBM_PRTN_M_DIRECT, "Triad error: %lf\n", errval);
    // kernel end
    
    free((void *)a);
    free((void *)b);
    free((void *)c);
    return err;
}

static int 
check_d_triad(int narr, int ntest, double *a, double *b, double *c, double s, double epsilon, double *errval)
{
    double a0 = 1.0;
    double b0 = 2.0;
    double c0 = 3.0;

    for (int i = 0; i < ntest; i++) {
        a0 = b0 + s * c0;
    }

    *errval = 0;
    for (int i = 0; i < narr; i ++) {
        *errval += (a[i] - a0) > 0 ? (a[i] - a0): (a0 - a[i]);
    }

    if (*errval > epsilon) return TPBE_KERN_VERIFY_FAIL;

    return 0;
}
