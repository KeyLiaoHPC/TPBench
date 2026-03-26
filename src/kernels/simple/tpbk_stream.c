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
#include "tpb-public.h"
#include "tpbench.h"

#define HLINE "-------------------------------------------------------------\n"
#ifndef STREAM_TYPE
#define STREAM_TYPE double
#endif

/*
 * MALLOC macro: may fail expansion if not wrapped in braces and used in single-statement contexts.
 * If used in if() or other macros, surrounding block is REQUIRED to avoid parse errors or unexpected behavior.
 */
#define MALLOC(_A, _array_size)   (_A) = ((STREAM_TYPE *)aligned_alloc(64, sizeof(STREAM_TYPE) * (_array_size))); \
                            if ((_A) == NULL) { return TPBE_MALLOC_FAIL; }

static double epsilon = 1.e-8;

// Forward declarations
int _tpbk_run_stream(void);
int tpbk_pli_register_stream(void);
static int d_stream(tpb_timer_t *timer, int ntest, uint64_t array_size,
                   int64_t twarm_ms, int64_t *copy_time, int64_t *scale_time,
                   int64_t *add_time, int64_t *triad_time, uint64_t *real_total_memsize,
                   uint32_t *array_size_out, double *copy_bw, double *scale_bw,
                   double *add_bw, double *triad_bw);
static int check_d_stream(int array_size, int ntest, double *a, double *b, double *c, double s, double epsilon, double *errval);

int
tpbk_pli_register_stream(void)
{
    int err;

    err = tpb_k_register("stream", "The STREAM benchmark with OpenMP enabled.", TPB_KTYPE_PLI);
    if (err != 0) return err;

    /* Kernel input parameters */
    err = tpb_k_add_parm("ntest", "Number of test iterations", "10",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)1, (int64_t)100000);
    if (err != 0) return err;
    err = tpb_k_add_parm("stream_array_size", "Number of elements per array", "0",
                         TPB_PARM_CLI | TPB_UINT32_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)4294967295);
    if (err != 0) return err;
    err = tpb_k_add_parm("twarm", "Warm-up time in milliseconds, 0<=twarm<=10000, default 1", "1",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)10000);
    if (err != 0) return err;

    /* Kernel outputs */
    err = tpb_k_add_output("Allocated memory size", "Actual memory footprint of three stream arrays.",
                           TPB_UINT64_T, TPB_UNIT_B | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;
    err = tpb_k_add_output("STREAM array size", "Actual number of elements per array.",
                           TPB_UINT32_T, TPB_UNAME_UNDEF | TPB_UBASE_BASE | TPB_UATTR_CAST_N | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;
    err = tpb_k_add_output("Time::Copy", "Measured runtime of copy operation.", 
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;
    err = tpb_k_add_output("Time::Scale", "Measured runtime of scale operation.", 
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;
    err = tpb_k_add_output("Time::Add", "Measured runtime of add operation.", 
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;
    err = tpb_k_add_output("Time::Triad", "Measured runtime of triad operation.", 
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;
    err = tpb_k_add_output("Bandwidth::Copy", "Measured copy bandwidth in decimal based MB/s.", 
                     TPB_DOUBLE_T, TPB_UNIT_MBPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;
    err = tpb_k_add_output("Bandwidth::Scale", "Measured scale bandwidth in decimal based MB/s.", 
                     TPB_DOUBLE_T, TPB_UNIT_MBPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;
    err = tpb_k_add_output("Bandwidth::Add", "Measured add bandwidth in decimal based MB/s.", 
                     TPB_DOUBLE_T, TPB_UNIT_MBPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;
    err = tpb_k_add_output("Bandwidth::Triad", "Measured triad bandwidth in decimal based MB/s.", 
                     TPB_DOUBLE_T, TPB_UNIT_MBPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;

    return tpb_k_finalize_pli();
}

int
run_stream(void)
{
    int tpberr;
    /* Input */
    int ntest;
    TPB_UNIT_T tpb_uname;
    tpb_timer_t timer;
    uint64_t array_size;
    int64_t twarm_ms;
    /* Output */
    void *copy_time = NULL;
    void *scale_time = NULL;
    void *add_time = NULL;
    void *triad_time = NULL;
    uint64_t *real_total_memsize = NULL;
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
    tpberr = tpb_k_get_arg("stream_array_size", TPB_UINT64_T, (void *)&array_size);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("twarm", TPB_INT64_T, (void *)&twarm_ms);
    if (tpberr) return tpberr;

    /* Malloc callbacks for kernel\'s outputs - 4 separate timing arrays */
    tpberr = tpb_k_alloc_output("Time::Copy", ntest, &copy_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("Time::Scale", ntest, &scale_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("Time::Add", ntest, &add_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("Time::Triad", ntest, &triad_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("Allocated memory size", 1, &real_total_memsize);
    if (tpberr) return tpberr;
    uint32_t *array_size_out = NULL;
    tpberr = tpb_k_alloc_output("STREAM array size", 1, &array_size_out);
    if (tpberr) return tpberr;

    /* Measured data throughput rate is a derived metrics, adding at run-time */
    tpb_uname = timer.unit & TPB_UNAME_MASK;
    if (tpb_uname == TPB_UNAME_WALLTIME) {
        tpberr = tpb_k_alloc_output("Bandwidth::Copy", ntest, &copy_bw);
        if (tpberr) return tpberr;
        tpberr = tpb_k_alloc_output("Bandwidth::Scale", ntest, &scale_bw);
        if (tpberr) return tpberr;
        tpberr = tpb_k_alloc_output("Bandwidth::Add", ntest, &add_bw);
        if (tpberr) return tpberr;
        tpberr = tpb_k_alloc_output("Bandwidth::Triad", ntest, &triad_bw);
        if (tpberr) return tpberr;
    } else {
        tpb_printf(TPBM_PRTN_M_DIRECT, "Unsupported timer: %s\n kernel stream: The STREAM benchmark only support wallclock timer. ", timer.name);
        return TPBE_KERN_ARG_FAIL;
    }

    /* Call the actual kernel implementation */
    tpberr = d_stream(&timer, ntest, array_size, twarm_ms,
                      copy_time, scale_time, add_time, triad_time,
                      real_total_memsize, array_size_out, copy_bw, scale_bw,
                      add_bw, triad_bw);

    return tpberr;
}

static int
d_stream(tpb_timer_t *timer, int ntest, uint64_t array_size,
          int64_t twarm_ms, int64_t *copy_time, int64_t *scale_time,
          int64_t *add_time, int64_t *triad_time, uint64_t *real_total_memsize,
          uint32_t *array_size_out, double *copy_bw, double *scale_bw,
          double *add_bw, double *triad_bw) {
    int err;
    int BytesPerWord;
    STREAM_TYPE *a, *b, *c;
    double s = 0.42;
    uint64_t t0, t1;

    printf(HLINE);
    tpb_printf(TPBM_PRTN_M_DIRECT, "STREAM version $Revision: 5.10 $\n");
    printf(HLINE);
    BytesPerWord = sizeof(STREAM_TYPE);
    printf(TPBM_PRTN_M_DIRECT, "This system uses %d bytes per array element.\n", BytesPerWord);

    err = 0;
    /* Use array_size if specified (non-zero), otherwise use total_memsize */
    if (array_size <= 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "Illegal arguments stream_array_size must > 0, currently %u", array_size);
        return TPBE_KERN_ARG_FAIL;
    }

    *real_total_memsize = 3 * BytesPerWord * array_size;
    tpb_printf(TPBM_PRTN_M_DIRECT,"Array size = %llu (elements), Offset = %d (elements)\n" , 
                (unsigned long long) array_size, 0);
    tpb_printf(TPBM_PRTN_M_DIRECT,"Memory per array = %.1f MiB (= %.1f GiB).\n", 
	BytesPerWord * ( (double) array_size / 1024.0/1024.0),
	BytesPerWord * ( (double) array_size / 1024.0/1024.0/1024.0));
    tpb_printf(TPBM_PRTN_M_DIRECT,"Total memory required = %.1f MiB (= %.1f GiB).\n",
	(3.0 * BytesPerWord) * ( (double) array_size / 1024.0/1024.),
	(3.0 * BytesPerWord) * ( (double) array_size / 1024.0/1024./1024.));
    tpb_printf(TPBM_PRTN_M_DIRECT,"Each kernel will be executed %d times.\n", ntest);
    tpb_printf(TPBM_PRTN_M_DIRECT," The *best* time for each kernel (excluding the first iteration)\n"); 
    tpb_printf(TPBM_PRTN_M_DIRECT," will be used to compute the reported bandwidth.\n");

    *array_size_out = array_size;

    MALLOC(a, array_size);
    MALLOC(b, array_size);
    MALLOC(c, array_size);

    for(int i = 0; i < array_size; i ++) {
        a[i] = 1.0;
        b[i] = 2.0;
        c[i] = 3.0;
    }

    // kernel warm
    struct timespec wts;
    uint64_t wns0, wns1;

    clock_gettime(CLOCK_MONOTONIC, &wts);
    wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    wns1 = wns0 + (uint64_t)twarm_ms * 1000000ULL;
    while(wns0 < wns1) {
        #pragma omp parallel for shared(a, b, c, s, array_size)
        for(int j = 0; j < array_size; j ++){
            a[j] = b[j] + s * c[j];
        }
        clock_gettime(CLOCK_MONOTONIC, &wts);
        wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    }

    /* Reset arrays to initial values after warm-up for verification */
    for(int i = 0; i < array_size; i ++) {
        a[i] = 1.0;
        b[i] = 2.0;
        c[i] = 3.0;
    }

    timer->init();

    // kernel start - 4 separate timing sections
    for(int i = 0; i < ntest; i ++){
        // Copy: c[j] = a[j]
        timer->tick(copy_time + i);
        #pragma omp parallel for shared(a, b, c, s, array_size)
        for(int j = 0; j < array_size; j ++){
            c[j] = a[j];
        }
        timer->tock(copy_time + i);
        
        // Scale: b[j] = s * c[j]
        timer->tick(scale_time + i);
        #pragma omp parallel for shared(a, b, c, s, array_size)
        for(int j = 0; j < array_size; j ++){
            b[j] = s * c[j];
        }
        timer->tock(scale_time + i);
        
        // Add: c[j] = a[j] + b[j]
        timer->tick(add_time + i);
        #pragma omp parallel for shared(a, b, c, s, array_size)
        for(int j = 0; j < array_size; j ++){
            c[j] = a[j] + b[j];
        }
        timer->tock(add_time + i);
        
        // Triad: a[j] = b[j] + s * c[j]
        timer->tick(triad_time + i);
        #pragma omp parallel for shared(a, b, c, s, array_size)
        for(int j = 0; j < array_size; j ++){
            a[j] = b[j] + s * c[j];
        }
        timer->tock(triad_time + i);
    }
    
    // Calculate bandwidth for each operation
    // Copy: 2 arrays (read a, write c) = 2 * array_size * sizeof(double)
    for (int i = 0; i < ntest; i ++) {
        uint64_t copy_bytes = 2 * array_size * sizeof(double);
        copy_bw[i] = ((double)copy_bytes * 1e-6)  / ((double)(copy_time[i]) * 1e-9);
    }
    
    // Scale: 2 arrays (read c, write b) = 2 * array_size * sizeof(double)
    for (int i = 0; i < ntest; i ++) {
        uint64_t scale_bytes = 2 * array_size * sizeof(double);
        scale_bw[i] = ((double)scale_bytes * 1e-6)  / ((double)(scale_time[i]) * 1e-9);
    }
    
    // Add: 3 arrays (read a, b, write c) = 3 * array_size * sizeof(double)
    for (int i = 0; i < ntest; i ++) {
        uint64_t add_bytes = 3 * array_size * sizeof(double);
        add_bw[i] = ((double)add_bytes * 1e-6)  / ((double)(add_time[i]) * 1e-9);
    }
    
    // Triad: 3 arrays (read b, c, write a) = 3 * array_size * sizeof(double)
    for (int i = 0; i < ntest; i ++) {
        uint64_t triad_bytes = 3 * array_size * sizeof(double);
        triad_bw[i] = ((double)triad_bytes * 1e-6)  / ((double)(triad_time[i]) * 1e-9);
    }

    /*--- SUMMARY ---*/
    double avgtime[4] = {0}, maxtime[4] = {0}, mintime[4] = {FLT_MAX,FLT_MAX,FLT_MAX,FLT_MAX};
    char *label[4] = {"Copy:      ", "Scale:     ","Add:       ", "Triad:     "};
    double bytes[4] = { 
        2 * sizeof(STREAM_TYPE) * array_size,
        2 * sizeof(STREAM_TYPE) * array_size,
        3 * sizeof(STREAM_TYPE) * array_size,
        3 * sizeof(STREAM_TYPE) * array_size
    };

    for (int k=1; k < ntest; k ++) {
        avgtime[0] += (double)copy_time[k];
	    mintime[0] = mintime[0] < copy_time[k] ? mintime[0] : copy_time[k];
	    maxtime[0] = maxtime[0] > copy_time[k] ? maxtime[0] : copy_time[k];
        avgtime[1] += (double)scale_time[k];
	    mintime[1] = mintime[1] < scale_time[k] ? mintime[1] : scale_time[k];
	    maxtime[1] = maxtime[1] > scale_time[k] ? maxtime[1] : scale_time[k];
        avgtime[2] += (double)add_time[k];
        mintime[2] = mintime[2] < add_time[k] ? mintime[2] : add_time[k];
	    maxtime[2] = maxtime[2] > add_time[k] ? maxtime[2] : add_time[k];
        avgtime[3] += (double)triad_time[k];
        mintime[3] = mintime[3] < triad_time[k] ? mintime[3] : triad_time[k];
	    maxtime[3] = maxtime[3] > triad_time[k] ? maxtime[3] : triad_time[k];
	}
    
    tpb_printf(TPBM_PRTN_M_DIRECT, "Function    Best Rate MB/s  Avg time     Min time     Max time\n");
    for (int j = 0; j < 4; j ++) {
		avgtime[j] = avgtime[j] / (double)(ntest-1) / 1e9;
        mintime[j] /= 1e9;
        maxtime[j] /= 1e9;

		printf("%s%12.1f  %11.6f  %11.6f  %11.6f\n", label[j], 1.0E-06 * bytes[j]/mintime[j], avgtime[j], mintime[j], maxtime[j]);
    }
    printf(HLINE);
    
    /* Verify results. */
    double errval;
    err = check_d_stream(array_size, ntest, a, b, c, s, epsilon, &errval);
    tpb_printf(TPBM_PRTN_M_DIRECT, "stream error: %.17f\n", errval);
    // kernel end
    
    free((void *)a);
    free((void *)b);
    free((void *)c);
    return err;
}

static int 
check_d_stream(int array_size, int ntest, double *a, double *b, double *c, double s, double epsilon, double *errval)
{
    int err;
    double a0 = 1.0;
    double b0 = 2.0;
    double c0 = 3.0;
    double asum, bsum, csum;

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
    
    a0 = a0 * (double)(array_size);
    b0 = b0 * (double)(array_size);
    c0 = c0 * (double)(array_size);
    asum = 0.0;
    bsum = 0.0;
    csum = 0.0;
    for (int i = 0; i < array_size; i ++) {
        asum += a[i];
        bsum += b[i];
        csum += c[i];
    }
    printf ("Results Comparison: \n");
    printf ("        Expected  : %f %f %f \n",a0, b0, c0);
    printf ("        Observed  : %f %f %f \n",asum, bsum, csum);
#define abs(a) ((a) >= 0? (a) : (-a))
	if (abs(a0-asum)/asum > epsilon) {
		printf ("Failed Validation on array a[]\n");
		printf ("        Expected  : %f \n",a0);
		printf ("        Observed  : %f \n",asum);
        err = TPBE_KERN_VERIFY_FAIL;
	}
	else if (abs(b0-bsum)/bsum > epsilon) {
		printf ("Failed Validation on array b[]\n");
		printf ("        Expected  : %f \n",b0);
		printf ("        Observed  : %f \n",bsum);
        err = TPBE_KERN_VERIFY_FAIL;
	}
	else if (abs(c0-csum)/csum > epsilon) {
		printf ("Failed Validation on array c[]\n");
		printf ("        Expected  : %f \n",c0);
		printf ("        Observed  : %f \n",csum);
        err = TPBE_KERN_VERIFY_FAIL;
	}
	else {
		printf ("Solution Validates\n");
        err = 0;
	}

    return err;
}

/* =========================================================================
 * PLI executable entry point (only compiled into .tpbx, not into .so)
 * ========================================================================= */
#ifdef TPB_K_BUILD_MAIN
int
main(int argc, char **argv)
{
    int err;

    err = tpb_k_corelib_init(NULL);
    if (err != 0) {
        fprintf(stderr, "Error: tpb_k_corelib_init failed: %d\n", err);
        return err;
    }

    const char *timer_name = NULL;
    if (argc >= 2) {
        timer_name = argv[1];
    } else {
        timer_name = getenv("TPBENCH_TIMER");
    }
    if (timer_name == NULL) {
        fprintf(stderr, "Error: Timer not specified (argv[1] or TPBENCH_TIMER)\n");
        return TPBE_CLI_FAIL;
    }

    err = tpb_k_pli_set_timer(timer_name);
    if (err != 0) {
        fprintf(stderr, "Error: Failed to set timer '%s'\n", timer_name);
        return err;
    }

    err = tpbk_pli_register_stream();
    if (err != 0) {
        fprintf(stderr, "Error: Failed to register kernel\n");
        return err;
    }

    tpb_k_rthdl_t handle;
    err = tpb_k_pli_build_handle(&handle, argc - 2, argv + 2);
    if (err != 0) {
        fprintf(stderr, "Error: Failed to build handle\n");
        return err;
    }

    tpb_cliout_args(&handle);

    tpb_printf(TPBM_PRTN_M_DIRECT, "Kernel logs\n");
    err = run_stream();
    if (err != 0) {
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL, "Kernel stream failed: %d\n", err);
        return err;
    }

    tpb_cliout_results(&handle);
    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE, "Kernel stream finished successfully.\n");

    tpb_k_write_task(&handle, 0);

    tpb_driver_clean_handle(&handle);

    return 0;
}
#endif /* TPB_K_BUILD_MAIN */
