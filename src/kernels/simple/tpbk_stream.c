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
 * stream.c
 * Description: STREAM benchmark with 4 operations timed separately
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
int register_stream(void);
int _tpbk_register_stream(void);
int _tpbk_run_stream(void);
int tpbk_pli_register_stream(void);
static int d_stream(tpb_timer_t *timer, int ntest, double kib, uint32_t array_size,
                   int64_t *copy_time, int64_t *scale_time,
                   int64_t *add_time, int64_t *triad_time,
                   uint64_t *real_memsize, uint32_t *array_size_out,
                   double *copy_bw, double *scale_bw,
                   double *add_bw, double *triad_bw);
static int check_d_stream(int narr, int ntest, double *a, double *b, double *c, double s, double epsilon, double *errval);

int
register_stream(void)
{
    /* Wrapper function for backward compatibility */
    return _tpbk_register_stream();
}

/* PLI registration: register only name, note, and parameters (no outputs, no runner) */
int
tpbk_pli_register_stream(void)
{
    int err;

    /* Register to TPBench as PLI kernel */
    err = tpb_k_register("stream", "The STREAM benchmark with OpenMP enabled.", TPB_KTYPE_PLI);
    if (err != 0) return err;

    /* Kernel input parameters - same as FLI registration */
    err = tpb_k_add_parm("ntest", "Number of test iterations", "10",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)1, (int64_t)100000);
    if (err != 0) return err;
    err = tpb_k_add_parm("memsize", "Memory size in KiB", "32",
                         TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_RANGE,
                         0.0009765625, DBL_MAX);
    if (err != 0) return err;
    err = tpb_k_add_parm("array_size", "Number of elements per array (0 = use memsize)", "0",
                         TPB_PARM_CLI | TPB_UINT32_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)4294967295);
    if (err != 0) return err;

    /* NO outputs registered here - kernel registers them in its own process */
    /* NO runner function - PLI uses exec */

    return tpb_k_finalize_pli();
}

int
_tpbk_register_stream(void)
{
    int err;
    
    /* Register to TPBench */
    err = tpb_k_register("stream", "The STREAM benchmark with OpenMP enabled.", TPB_KTYPE_FLI);
    if(err != 0) return err;

    /* Kernel input parameters */
    err = tpb_k_add_parm("ntest", "Number of test iterations", "10",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)1, (int64_t)100000);
    if(err != 0) return err;
    err = tpb_k_add_parm("memsize", "Memory size in KiB", "32",
                         TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_RANGE,
                         0.0009765625, DBL_MAX);
    if(err != 0) return err;
    err = tpb_k_add_parm("array_size", "Number of elements per array (0 = use memsize)", "0",
                         TPB_PARM_CLI | TPB_UINT32_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)4294967295);
    if(err != 0) return err;

    /* Kernel outputs - 4 separate timing sections */
    err = tpb_k_add_output("copy_time", "Measured runtime of copy operation.", 
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_SHAPE_1D);
    if(err != 0) return err;
    err = tpb_k_add_output("scale_time", "Measured runtime of scale operation.", 
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_SHAPE_1D);
    if(err != 0) return err;
    err = tpb_k_add_output("add_time", "Measured runtime of add operation.", 
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_SHAPE_1D);
    if(err != 0) return err;
    err = tpb_k_add_output("triad_time", "Measured runtime of triad operation.", 
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_SHAPE_1D);
    if(err != 0) return err;
    err = tpb_k_add_output("real_memsize", "Actual memory footprint of three stream arrays.",
                           TPB_UINT64_T, TPB_UNIT_B | TPB_UATTR_CAST_Y | TPB_UATTR_SHAPE_POINT );
    if(err != 0) return err;
    err = tpb_k_add_output("array_size", "Actual number of elements per array.",
                           TPB_UINT32_T, TPB_UNAME_UNDEF | TPB_UBASE_BASE | TPB_UATTR_CAST_N | TPB_UATTR_SHAPE_POINT);
    if(err != 0) return err;
    // Set runner function.
    err = tpb_k_add_runner(_tpbk_run_stream);
    if(err != 0) return err;
    
    return 0;
}

int
_tpbk_run_stream(void)
{
    int tpberr;
    /* Input */
    int ntest;
    TPB_UNIT_T tpb_uname;
    tpb_timer_t timer;
    double memsize;
    uint32_t array_size;
    /* Output */
    void *copy_time = NULL;
    void *scale_time = NULL;
    void *add_time = NULL;
    void *triad_time = NULL;
    uint64_t *real_memsize = NULL;
    double *copy_bw = NULL;
    double *scale_bw = NULL;
    double *add_bw = NULL;
    double *triad_bw = NULL;

    /* Get timer */
    tpberr = tpb_k_get_timer(&timer);
    if (tpberr) return tpberr;

    /* Get arguments by names */
    tpberr = tpb_k_get_arg("ntest", TPB_INT64_T, (void *)&ntest);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("memsize", TPB_DOUBLE_T, (void *)&memsize);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("array_size", TPB_UINT32_T, (void *)&array_size);
    if (tpberr) return tpberr;

    /* Malloc callbacks for kernel\'s outputs - 4 separate timing arrays */
    tpberr = tpb_k_alloc_output("copy_time", ntest, &copy_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("scale_time", ntest, &scale_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("add_time", ntest, &add_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("triad_time", ntest, &triad_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("real_memsize", 1, &real_memsize);
    if (tpberr) return tpberr;
    uint32_t *array_size_out = NULL;
    tpberr = tpb_k_alloc_output("array_size", 1, &array_size_out);
    if (tpberr) return tpberr;

    /* Measured data throughput rate is a derived metrics, adding at run-time */
    tpb_uname = timer.unit & TPB_UNAME_MASK;
    if (tpb_uname == TPB_UNAME_WALLTIME) {
        tpb_k_add_output("copy_bw_walltime", "Measured copy bandwidth in decimal based MB/s.", 
                         TPB_DOUBLE_T, TPB_UNIT_MBPS | TPB_UATTR_CAST_N | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("copy_bw_walltime", ntest, &copy_bw);
        if (tpberr) return tpberr;
        tpb_k_add_output("scale_bw_walltime", "Measured scale bandwidth in decimal based MB/s.", 
                         TPB_DOUBLE_T, TPB_UNIT_MBPS | TPB_UATTR_CAST_N | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("scale_bw_walltime", ntest, &scale_bw);
        if (tpberr) return tpberr;
        tpb_k_add_output("add_bw_walltime", "Measured add bandwidth in decimal based MB/s.", 
                         TPB_DOUBLE_T, TPB_UNIT_MBPS | TPB_UATTR_CAST_N | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("add_bw_walltime", ntest, &add_bw);
        if (tpberr) return tpberr;
        tpb_k_add_output("triad_bw_walltime", "Measured triad bandwidth in decimal based MB/s.", 
                         TPB_DOUBLE_T, TPB_UNIT_MBPS | TPB_UATTR_CAST_N | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("triad_bw_walltime", ntest, &triad_bw);
        if (tpberr) return tpberr;
    } else if (tpb_uname == TPB_UNAME_PHYSTIME) {
        tpb_k_add_output("copy_bw_phystime", "Measured copy bandwidth in binay based Byte/cy.", 
                         TPB_DOUBLE_T, TPB_UNIT_BYTEPCY | TPB_UATTR_CAST_N | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("copy_bw_phystime", ntest, &copy_bw);
        if (tpberr) return tpberr;
        tpb_k_add_output("scale_bw_phystime", "Measured scale bandwidth in binay based Byte/cy.", 
                         TPB_DOUBLE_T, TPB_UNIT_BYTEPCY | TPB_UATTR_CAST_N | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("scale_bw_phystime", ntest, &scale_bw);
        if (tpberr) return tpberr;
        tpb_k_add_output("add_bw_phystime", "Measured add bandwidth in binay based Byte/cy.", 
                         TPB_DOUBLE_T, TPB_UNIT_BYTEPCY | TPB_UATTR_CAST_N | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("add_bw_phystime", ntest, &add_bw);
        if (tpberr) return tpberr;
        tpb_k_add_output("triad_bw_phystime", "Measured triad bandwidth in binay based Byte/cy.", 
                         TPB_DOUBLE_T, TPB_UNIT_BYTEPCY | TPB_UATTR_CAST_N | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("triad_bw_phystime", ntest, &triad_bw);
        if (tpberr) return tpberr;
    } else {
        tpb_printf(TPBM_PRTN_M_DIRECT, "In kernel stream: unknown timer unit name %llx", tpb_uname);
        return TPBE_KERN_ARG_FAIL;
    }

    /* Call the actual kernel implementation */
    tpberr = d_stream(&timer, ntest, memsize, array_size, copy_time, scale_time, add_time, triad_time,
                      real_memsize, array_size_out, copy_bw, scale_bw, add_bw, triad_bw);

    return tpberr;
}

static int
d_stream(tpb_timer_t *timer, int ntest, double kib, uint32_t array_size,
          int64_t *copy_time, int64_t *scale_time,
          int64_t *add_time, int64_t *triad_time,
          uint64_t *real_memsize, uint32_t *array_size_out,
          double *copy_bw, double *scale_bw,
          double *add_bw, double *triad_bw) {
    int narr, err;
    double *a, *b, *c;
    double s = 0.42;
    uint64_t t0, t1;

    err = 0;
    /* Use array_size if specified (non-zero), otherwise use memsize */
    if (array_size > 0) {
        narr = array_size;
    } else {
        narr = (int)(kib * 1024 / sizeof(double) / 3);
    }
    *real_memsize = narr * sizeof(double) * 3;
    *array_size_out = narr;

    MALLOC(a, narr);
    MALLOC(b, narr);
    MALLOC(c, narr);

    for(int i = 0; i < narr; i ++) {
        a[i] = 1.0;
        b[i] = 2.0;
        c[i] = 3.0;
    }

    // kernel warm
    struct timespec wts;
    uint64_t wns0, wns1;

    clock_gettime(CLOCK_MONOTONIC, &wts);
    wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    wns1 = wns0 + 1e9;
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

    // kernel start - 4 separate timing sections
    for(int i = 0; i < ntest; i ++){
        // Copy: c[j] = a[j]
        timer->tick(copy_time + i);
        #pragma omp parallel for shared(a, b, c, s, narr)
        for(int j = 0; j < narr; j ++){
            c[j] = a[j];
        }
        timer->tock(copy_time + i);
        
        // Scale: b[j] = s * c[j]
        timer->tick(scale_time + i);
        #pragma omp parallel for shared(a, b, c, s, narr)
        for(int j = 0; j < narr; j ++){
            b[j] = s * c[j];
        }
        timer->tock(scale_time + i);
        
        // Add: c[j] = a[j] + b[j]
        timer->tick(add_time + i);
        #pragma omp parallel for shared(a, b, c, s, narr)
        for(int j = 0; j < narr; j ++){
            c[j] = a[j] + b[j];
        }
        timer->tock(add_time + i);
        
        // Triad: a[j] = b[j] + s * c[j]
        timer->tick(triad_time + i);
        #pragma omp parallel for shared(a, b, c, s, narr)
        for(int j = 0; j < narr; j ++){
            a[j] = b[j] + s * c[j];
        }
        timer->tock(triad_time + i);
    }
    
    // Calculate bandwidth for each operation
    // Copy: 2 arrays (read a, write c) = 2 * narr * sizeof(double)
    for (int i = 0; i < ntest; i ++) {
        uint64_t copy_bytes = 2 * narr * sizeof(double);
        copy_bw[i] = ((double)copy_bytes * 1e-6)  / ((double)(copy_time[i]) * 1e-9);
    }
    
    // Scale: 2 arrays (read c, write b) = 2 * narr * sizeof(double)
    for (int i = 0; i < ntest; i ++) {
        uint64_t scale_bytes = 2 * narr * sizeof(double);
        scale_bw[i] = ((double)scale_bytes * 1e-6)  / ((double)(scale_time[i]) * 1e-9);
    }
    
    // Add: 3 arrays (read a, b, write c) = 3 * narr * sizeof(double)
    for (int i = 0; i < ntest; i ++) {
        uint64_t add_bytes = 3 * narr * sizeof(double);
        add_bw[i] = ((double)add_bytes * 1e-6)  / ((double)(add_time[i]) * 1e-9);
    }
    
    // Triad: 3 arrays (read b, c, write a) = 3 * narr * sizeof(double)
    for (int i = 0; i < ntest; i ++) {
        uint64_t triad_bytes = 3 * narr * sizeof(double);
        triad_bw[i] = ((double)triad_bytes * 1e-6)  / ((double)(triad_time[i]) * 1e-9);
    }
    
    /* Verify results. */
    double errval;
    err = check_d_stream(narr, ntest, a, b, c, s, epsilon, &errval);
    tpb_printf(TPBM_PRTN_M_DIRECT, "stream error: %lf\n", errval);
    // kernel end
    
    free((void *)a);
    free((void *)b);
    free((void *)c);
    return err;
}

static int 
check_d_stream(int narr, int ntest, double *a, double *b, double *c, double s, double epsilon, double *errval)
{
    double a0 = 1.0;
    double b0 = 2.0;
    double c0 = 3.0;

    /* Simulate 4 operations repeated ntest times */
    /* copy: c = a */
    /* scale: b = s * c */
    /* add: c = a + b */
    /* triad: a = b + s * c */
    for (int i = 0; i < ntest; i++) {
        c0 = a0;
        b0 = s * c0;
        c0 = a0 + b0;
        a0 = b0 + s * c0;
    }

    *errval = 0;
    for (int i = 0; i < narr; i ++) {
        *errval += (a[i] - a0) > 0 ? (a[i] - a0): (a0 - a[i]);
    }

    if (*errval > epsilon) return TPBE_KERN_VERIFY_FAIL;

    return 0;
}
