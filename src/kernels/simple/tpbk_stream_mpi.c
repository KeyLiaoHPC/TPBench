/*
 * =================================================================================
 * TPBench - A high-precision throughputs benchmarking tool for scientific computing
 * 
 * Copyright (C) 2024 Key Liao (Liao Qiucheng)
 * 
 * This program is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software 
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
 * tpbk_stream_mpi.c
 * Description: MPI-enabled STREAM benchmark with TPBench timing
 * Author: Key Liao
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
#include <mpi.h>
#include "tpbench.h"

#define ARRAY_ALIGNMENT 64

static double epsilon = 1.e-8;

/* Forward declarations */
int _tpbk_register_stream_mpi(void);
int _tpbk_run_stream_mpi(void);
int tpbk_pli_register_stream_mpi(void);
static int d_stream_mpi(tpb_timer_t *timer, int ntest, double kib, uint32_t array_size,
                        int64_t twarm_ms, int64_t *copy_time, int64_t *scale_time,
                        int64_t *add_time, int64_t *triad_time, uint64_t *real_total_memsize,
                        uint32_t *array_size_out, double *copy_bw, double *scale_bw,
                        double *add_bw, double *triad_bw);
static int check_d_stream(int narr, int ntest, double *a, double *b, double *c, 
                          double s, double epsilon, double *errval);

/* PLI registration: register only name, note, and parameters (no outputs, no runner) */
int
tpbk_pli_register_stream_mpi(void)
{
    int err;

    /* Register to TPBench as PLI kernel */
    err = tpb_k_register("stream_mpi", "The STREAM benchmark with MPI+OpenMP enabled.", TPB_KTYPE_PLI);
    if (err != 0) return err;

    /* Kernel input parameters - same as stream kernel */
    err = tpb_k_add_parm("ntest", "Number of test iterations", "10",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)1, (int64_t)100000);
    if (err != 0) return err;
    err = tpb_k_add_parm("total_memsize", "Total memory size across all ranks in KiB (32 for default). "
                         "The overall designated memory used by the compute kernel. "
                         "total_array_size = round_down(total_memsize / 3 / sizeof(double)), "
                         "then distributed across ranks with the last rank getting any remainder.", 
                         "32",
                         TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_RANGE,
                         0.0009765625, DBL_MAX);
    if (err != 0) return err;
    err = tpb_k_add_parm("array_size", "Number of elements per array per rank (0 = use total_memsize)", "0",
                         TPB_PARM_CLI | TPB_UINT32_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)4294967295);
    if (err != 0) return err;
    err = tpb_k_add_parm("twarm", "Warm-up time in milliseconds, 0<=twarm<=10000, default 1", "1",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)10000);
    if (err != 0) return err;

    /* NO outputs registered here - kernel registers them in its own process */
    /* NO runner function - PLI uses exec */

    return tpb_k_finalize_pli();
}

int
_tpbk_register_stream_mpi(void)
{
    int err;
    
    /* Register to TPBench */
    err = tpb_k_register("stream_mpi", "The STREAM benchmark with MPI+OpenMP enabled.", TPB_KTYPE_FLI);
    if (err != 0) return err;

    /* Kernel input parameters */
    err = tpb_k_add_parm("ntest", "Number of test iterations", "10",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)1, (int64_t)100000);
    if (err != 0) return err;
    err = tpb_k_add_parm("total_memsize", "Total memory size across all ranks in KiB (overall designated memory)", "32",
                         TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_RANGE,
                         0.0009765625, DBL_MAX);
    if (err != 0) return err;
    err = tpb_k_add_parm("array_size", "Number of elements per array per rank (0 = use total_memsize)", "0",
                         TPB_PARM_CLI | TPB_UINT32_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)4294967295);
    if (err != 0) return err;
    err = tpb_k_add_parm("twarm", "Warm-up time in milliseconds, 0<=twarm<=10000, default 1", "1",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)10000);
    if (err != 0) return err;

    /* Kernel outputs - 4 separate timing sections */
    err = tpb_k_add_output("copy_time", "Measured runtime of copy operation (min across ranks).", 
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;
    err = tpb_k_add_output("scale_time", "Measured runtime of scale operation (min across ranks).", 
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;
    err = tpb_k_add_output("add_time", "Measured runtime of add operation (min across ranks).", 
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;
    err = tpb_k_add_output("triad_time", "Measured runtime of triad operation (min across ranks).", 
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;
    err = tpb_k_add_output("real_total_memsize", "Actual memory footprint of three stream arrays per rank.",
                           TPB_UINT64_T, TPB_UNIT_B | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;
    err = tpb_k_add_output("array_size", "Actual number of elements per array per rank.",
                           TPB_UINT32_T, TPB_UNAME_UNDEF | TPB_UBASE_BASE | TPB_UATTR_CAST_N | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;
    
    /* Set runner function. */
    err = tpb_k_add_runner(_tpbk_run_stream_mpi);
    if (err != 0) return err;
    
    return 0;
}

int
_tpbk_run_stream_mpi(void)
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
    tpberr = tpb_k_get_arg("total_memsize", TPB_DOUBLE_T, (void *)&total_memsize);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("array_size", TPB_UINT32_T, (void *)&array_size);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("twarm", TPB_INT64_T, (void *)&twarm_ms);
    if (tpberr) return tpberr;

    /* Malloc callbacks for kernel's outputs - 4 separate timing arrays */
    tpberr = tpb_k_alloc_output("copy_time", ntest, &copy_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("scale_time", ntest, &scale_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("add_time", ntest, &add_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("triad_time", ntest, &triad_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("real_total_memsize", 1, &real_total_memsize);
    if (tpberr) return tpberr;
    uint32_t *array_size_out = NULL;
    tpberr = tpb_k_alloc_output("array_size", 1, &array_size_out);
    if (tpberr) return tpberr;

    /* Measured data throughput rate is a derived metrics, adding at run-time */
    tpb_uname = timer.unit & TPB_UNAME_MASK;
    if (tpb_uname == TPB_UNAME_WALLTIME) {
        tpb_k_add_output("copy_bw_walltime", "Measured copy bandwidth in decimal based MB/s.", 
                         TPB_DOUBLE_T, TPB_UNIT_MBPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("copy_bw_walltime", ntest, &copy_bw);
        if (tpberr) return tpberr;
        tpb_k_add_output("scale_bw_walltime", "Measured scale bandwidth in decimal based MB/s.", 
                         TPB_DOUBLE_T, TPB_UNIT_MBPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("scale_bw_walltime", ntest, &scale_bw);
        if (tpberr) return tpberr;
        tpb_k_add_output("add_bw_walltime", "Measured add bandwidth in decimal based MB/s.", 
                         TPB_DOUBLE_T, TPB_UNIT_MBPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("add_bw_walltime", ntest, &add_bw);
        if (tpberr) return tpberr;
        tpb_k_add_output("triad_bw_walltime", "Measured triad bandwidth in decimal based MB/s.", 
                         TPB_DOUBLE_T, TPB_UNIT_MBPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("triad_bw_walltime", ntest, &triad_bw);
        if (tpberr) return tpberr;
    } else if (tpb_uname == TPB_UNAME_PHYSTIME) {
        tpb_k_add_output("copy_bw_phystime", "Measured copy bandwidth in binary based Byte/cy.", 
                         TPB_DOUBLE_T, TPB_UNIT_BYTEPCY | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("copy_bw_phystime", ntest, &copy_bw);
        if (tpberr) return tpberr;
        tpb_k_add_output("scale_bw_phystime", "Measured scale bandwidth in binary based Byte/cy.", 
                         TPB_DOUBLE_T, TPB_UNIT_BYTEPCY | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("scale_bw_phystime", ntest, &scale_bw);
        if (tpberr) return tpberr;
        tpb_k_add_output("add_bw_phystime", "Measured add bandwidth in binary based Byte/cy.", 
                         TPB_DOUBLE_T, TPB_UNIT_BYTEPCY | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("add_bw_phystime", ntest, &add_bw);
        if (tpberr) return tpberr;
        tpb_k_add_output("triad_bw_phystime", "Measured triad bandwidth in binary based Byte/cy.", 
                         TPB_DOUBLE_T, TPB_UNIT_BYTEPCY | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("triad_bw_phystime", ntest, &triad_bw);
        if (tpberr) return tpberr;
    } else {
        tpb_printf(TPBM_PRTN_M_DIRECT, "In kernel stream_mpi: unknown timer unit name %llx\n", 
                   (unsigned long long)tpb_uname);
        return TPBE_KERN_ARG_FAIL;
    }

    /* Call the actual kernel implementation */
    tpberr = d_stream_mpi(&timer, ntest, total_memsize, array_size, twarm_ms,
                          copy_time, scale_time, add_time, triad_time,
                          real_total_memsize, array_size_out, copy_bw, scale_bw,
                          add_bw, triad_bw);

    return tpberr;
}

static int
d_stream_mpi(tpb_timer_t *timer, int ntest, double kib, uint32_t array_size,
             int64_t twarm_ms, int64_t *copy_time, int64_t *scale_time,
             int64_t *add_time, int64_t *triad_time, uint64_t *real_total_memsize,
             uint32_t *array_size_out, double *copy_bw, double *scale_bw,
             double *add_bw, double *triad_bw)
{
    int narr, err, k;
    double *a, *b, *c;
    double s = 0.42;
    int rank, nprocs;
    uint64_t total_array_size, base_size, array_bytes;

    /* Local timing arrays */
    int64_t *local_copy_time = NULL;
    int64_t *local_scale_time = NULL;
    int64_t *local_add_time = NULL;
    int64_t *local_triad_time = NULL;

    /* All timing arrays for gather (rank 0 only) */
    int64_t *all_copy_time = NULL;
    int64_t *all_scale_time = NULL;
    int64_t *all_add_time = NULL;
    int64_t *all_triad_time = NULL;

    /* Error arrays for gathering */
    double local_errval = 0.0;
    double *all_errvals = NULL;

    err = 0;

    /* Get MPI info */
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    /* Use array_size if specified (non-zero), otherwise use total_memsize */
    if (array_size > 0) {
        /* If array_size is specified, it's the total across all ranks */
        total_array_size = array_size;
    } else {
        /* Calculate total_array_size from total_memsize across all ranks */
        total_array_size = (uint64_t)(kib * 1024 / sizeof(double) / 3);
    }
    
    /* Distribute array size across ranks */
    base_size = total_array_size / nprocs;
    narr = (int)base_size;
    
    /* Last rank gets the remainder */
    if (rank == nprocs - 1) {
        narr += (int)(total_array_size % nprocs);
    }

    /* Allocate arrays using posix_memalign */
    array_bytes = narr * sizeof(double);
    k = posix_memalign((void **)&a, ARRAY_ALIGNMENT, array_bytes);
    if (k != 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "Rank %d: Allocation of array a failed, return code is %d\n", rank, k);
        return TPBE_MALLOC_FAIL;
    }
    k = posix_memalign((void **)&b, ARRAY_ALIGNMENT, array_bytes);
    if (k != 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "Rank %d: Allocation of array b failed, return code is %d\n", rank, k);
        free(a);
        return TPBE_MALLOC_FAIL;
    }
    k = posix_memalign((void **)&c, ARRAY_ALIGNMENT, array_bytes);
    if (k != 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "Rank %d: Allocation of array c failed, return code is %d\n", rank, k);
        free(a);
        free(b);
        return TPBE_MALLOC_FAIL;
    }

    /* Calculate actual memory footprint per rank */
    *real_total_memsize = (narr * nprocs + total_array_size % nprocs ) * sizeof(double) * 3;
    *array_size_out = narr;

    /* Print MPI info on rank 0 */
    if (rank == 0) {
        uint64_t total_mem = 0;
        /* Gather array sizes from all ranks to compute total memory */
        uint32_t *all_narrs = (uint32_t *)malloc(sizeof(uint32_t) * nprocs);
        if (all_narrs == NULL) {
            tpb_printf(TPBM_PRTN_M_DIRECT, "Rank 0: Failed to allocate array for gathering sizes\n");
            free(a); free(b); free(c);
            return TPBE_MALLOC_FAIL;
        }
        
        MPI_Gather(&narr, 1, MPI_UINT32_T, all_narrs, 1, MPI_UINT32_T, 0, MPI_COMM_WORLD);
        
        for (int i = 0; i < nprocs; i++) {
            total_mem += all_narrs[i] * sizeof(double) * 3;
        }
        
        tpb_printf(TPBM_PRTN_M_DIRECT, "-------------------------------------------------------------\n");
        tpb_printf(TPBM_PRTN_M_DIRECT, "MPI STREAM Benchmark\n");
        tpb_printf(TPBM_PRTN_M_DIRECT, "-------------------------------------------------------------\n");
        tpb_printf(TPBM_PRTN_M_DIRECT, "This system uses %lu bytes per array element.\n", sizeof(double));
        tpb_printf(TPBM_PRTN_M_DIRECT, "-------------------------------------------------------------\n");
        tpb_printf(TPBM_PRTN_M_DIRECT, "Total Aggregate Array size = %llu (elements)\n", 
                   (unsigned long long)total_array_size);
        tpb_printf(TPBM_PRTN_M_DIRECT, "Total Aggregate Memory per array = %.1f MiB (= %.1f GiB).\n",
                   (double)total_array_size * sizeof(double) / (1024.0 * 1024.0),
                   (double)total_array_size * sizeof(double) / (1024.0 * 1024.0 * 1024.0));
        tpb_printf(TPBM_PRTN_M_DIRECT, "Total Aggregate memory required = %.1f MiB (= %.1f GiB).\n",
                   (double)total_mem / (1024.0 * 1024.0),
                   (double)total_mem / (1024.0 * 1024.0 * 1024.0));
        tpb_printf(TPBM_PRTN_M_DIRECT, "Data is distributed across %d MPI ranks\n", nprocs);
        tpb_printf(TPBM_PRTN_M_DIRECT, "   Array size per MPI rank = %u (elements, base) ", (unsigned int)base_size);
        if (total_array_size % nprocs != 0) {
            tpb_printf(TPBM_PRTN_M_DIRECT, "+ %llu (remainder on last rank)\n", 
                       (unsigned long long)(total_array_size % nprocs));
        } else {
            tpb_printf(TPBM_PRTN_M_DIRECT, "\n");
        }
        tpb_printf(TPBM_PRTN_M_DIRECT, "   Memory per array per MPI rank = %.1f MiB (= %.1f GiB).\n",
                   (double)base_size * sizeof(double) / (1024.0 * 1024.0),
                   (double)base_size * sizeof(double) / (1024.0 * 1024.0 * 1024.0));
        tpb_printf(TPBM_PRTN_M_DIRECT, "   Total memory per MPI rank = %.1f MiB (= %.1f GiB).\n",
                   (double)base_size * sizeof(double) * 3 / (1024.0 * 1024.0),
                   (double)base_size * sizeof(double) * 3 / (1024.0 * 1024.0 * 1024.0));
        tpb_printf(TPBM_PRTN_M_DIRECT, "-------------------------------------------------------------\n");
        tpb_printf(TPBM_PRTN_M_DIRECT, "Each kernel will be executed %d times.\n", ntest);
        tpb_printf(TPBM_PRTN_M_DIRECT, "The *best* time for each kernel (excluding the first iteration)\n");
        tpb_printf(TPBM_PRTN_M_DIRECT, " will be used to compute the reported bandwidth.\n");
        tpb_printf(TPBM_PRTN_M_DIRECT, "The SCALAR value used for this run is %f\n", s);
        tpb_printf(TPBM_PRTN_M_DIRECT, "-------------------------------------------------------------\n");
        
        free(all_narrs);
    } else {
        /* Other ranks also participate in the gather */
        MPI_Gather(&narr, 1, MPI_UINT32_T, NULL, 0, MPI_UINT32_T, 0, MPI_COMM_WORLD);
    }

    /* Allocate local timing arrays */
    local_copy_time = (int64_t *)malloc(sizeof(int64_t) * ntest);
    local_scale_time = (int64_t *)malloc(sizeof(int64_t) * ntest);
    local_add_time = (int64_t *)malloc(sizeof(int64_t) * ntest);
    local_triad_time = (int64_t *)malloc(sizeof(int64_t) * ntest);
    if (!local_copy_time || !local_scale_time || !local_add_time || !local_triad_time) {
        return TPBE_MALLOC_FAIL;
    }

    /* Rank 0 allocates gather buffers */
    if (rank == 0) {
        all_copy_time = (int64_t *)malloc(sizeof(int64_t) * ntest * nprocs);
        all_scale_time = (int64_t *)malloc(sizeof(int64_t) * ntest * nprocs);
        all_add_time = (int64_t *)malloc(sizeof(int64_t) * ntest * nprocs);
        all_triad_time = (int64_t *)malloc(sizeof(int64_t) * ntest * nprocs);
        if (!all_copy_time || !all_scale_time || !all_add_time || !all_triad_time) {
            return TPBE_MALLOC_FAIL;
        }
    }

    /* Initialize arrays */
    for (int i = 0; i < narr; i++) {
        a[i] = 1.0;
        b[i] = 2.0;
        c[i] = 3.0;
    }

    /* Synchronize before warm-up */
    MPI_Barrier(MPI_COMM_WORLD);

    /* Kernel warm-up */
    struct timespec wts;
    uint64_t wns0, wns1;

    clock_gettime(CLOCK_MONOTONIC, &wts);
    wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    wns1 = wns0 + (uint64_t)twarm_ms * 1000000ULL;
    while (wns0 < wns1) {
        #pragma omp parallel for shared(a, b, c, s, narr)
        for (int j = 0; j < narr; j++) {
            a[j] = b[j] + s * c[j];
        }
        clock_gettime(CLOCK_MONOTONIC, &wts);
        wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    }

    /* Reset arrays to initial values after warm-up for verification */
    for (int i = 0; i < narr; i++) {
        a[i] = 1.0;
        b[i] = 2.0;
        c[i] = 3.0;
    }

    /* Synchronize before timing */
    MPI_Barrier(MPI_COMM_WORLD);

    timer->init();

    /* Kernel execution - 4 separate timing sections */
    for (int i = 0; i < ntest; i++) {
        /* Synchronize all ranks before each iteration */
        MPI_Barrier(MPI_COMM_WORLD);

        /* Copy: c[j] = a[j] */
        timer->tick(local_copy_time + i);
        #pragma omp parallel for shared(a, b, c, s, narr)
        for (int j = 0; j < narr; j++) {
            c[j] = a[j];
        }
        timer->tock(local_copy_time + i);
        
        MPI_Barrier(MPI_COMM_WORLD);

        /* Scale: b[j] = s * c[j] */
        timer->tick(local_scale_time + i);
        #pragma omp parallel for shared(a, b, c, s, narr)
        for (int j = 0; j < narr; j++) {
            b[j] = s * c[j];
        }
        timer->tock(local_scale_time + i);
        
        MPI_Barrier(MPI_COMM_WORLD);

        /* Add: c[j] = a[j] + b[j] */
        timer->tick(local_add_time + i);
        #pragma omp parallel for shared(a, b, c, s, narr)
        for (int j = 0; j < narr; j++) {
            c[j] = a[j] + b[j];
        }
        timer->tock(local_add_time + i);
        
        MPI_Barrier(MPI_COMM_WORLD);

        /* Triad: a[j] = b[j] + s * c[j] */
        timer->tick(local_triad_time + i);
        #pragma omp parallel for shared(a, b, c, s, narr)
        for (int j = 0; j < narr; j++) {
            a[j] = b[j] + s * c[j];
        }
        timer->tock(local_triad_time + i);
    }

    /* Gather timing data from all ranks to rank 0 */
    MPI_Gather(local_copy_time, ntest, MPI_INT64_T, 
               all_copy_time, ntest, MPI_INT64_T, 0, MPI_COMM_WORLD);
    MPI_Gather(local_scale_time, ntest, MPI_INT64_T,
               all_scale_time, ntest, MPI_INT64_T, 0, MPI_COMM_WORLD);
    MPI_Gather(local_add_time, ntest, MPI_INT64_T,
               all_add_time, ntest, MPI_INT64_T, 0, MPI_COMM_WORLD);
    MPI_Gather(local_triad_time, ntest, MPI_INT64_T,
               all_triad_time, ntest, MPI_INT64_T, 0, MPI_COMM_WORLD);

    /* Rank 0: compute min time across all ranks for each iteration */
    if (rank == 0) {
        for (int i = 0; i < ntest; i++) {
            int64_t min_copy = all_copy_time[i];
            int64_t min_scale = all_scale_time[i];
            int64_t min_add = all_add_time[i];
            int64_t min_triad = all_triad_time[i];

            for (int r = 1; r < nprocs; r++) {
                if (all_copy_time[r * ntest + i] < min_copy)
                    min_copy = all_copy_time[r * ntest + i];
                if (all_scale_time[r * ntest + i] < min_scale)
                    min_scale = all_scale_time[r * ntest + i];
                if (all_add_time[r * ntest + i] < min_add)
                    min_add = all_add_time[r * ntest + i];
                if (all_triad_time[r * ntest + i] < min_triad)
                    min_triad = all_triad_time[r * ntest + i];
            }

            copy_time[i] = min_copy;
            scale_time[i] = min_scale;
            add_time[i] = min_add;
            triad_time[i] = min_triad;
        }

        /* Calculate bandwidth for each operation (aggregate across all ranks) */
        /* Copy: 2 arrays per rank = 2 * narr * sizeof(double) * nprocs */
        for (int i = 0; i < ntest; i++) {
            uint64_t copy_bytes = 2 * (uint64_t)narr * sizeof(double) * nprocs;
            copy_bw[i] = ((double)copy_bytes * 1e-6) / ((double)(copy_time[i]) * 1e-9);
        }
        
        /* Scale: 2 arrays per rank */
        for (int i = 0; i < ntest; i++) {
            uint64_t scale_bytes = 2 * (uint64_t)narr * sizeof(double) * nprocs;
            scale_bw[i] = ((double)scale_bytes * 1e-6) / ((double)(scale_time[i]) * 1e-9);
        }
        
        /* Add: 3 arrays per rank */
        for (int i = 0; i < ntest; i++) {
            uint64_t add_bytes = 3 * (uint64_t)narr * sizeof(double) * nprocs;
            add_bw[i] = ((double)add_bytes * 1e-6) / ((double)(add_time[i]) * 1e-9);
        }
        
        /* Triad: 3 arrays per rank */
        for (int i = 0; i < ntest; i++) {
            uint64_t triad_bytes = 3 * (uint64_t)narr * sizeof(double) * nprocs;
            triad_bw[i] = ((double)triad_bytes * 1e-6) / ((double)(triad_time[i]) * 1e-9);
        }
    }

    /* Verify results on each rank */
    err = check_d_stream(narr, ntest, a, b, c, s, epsilon, &local_errval);
    
    /* Rank 0 allocates array to gather all errors */
    if (rank == 0) {
        all_errvals = (double *)malloc(sizeof(double) * nprocs);
        if (all_errvals == NULL) {
            tpb_printf(TPBM_PRTN_M_DIRECT, "Rank 0: Failed to allocate array for gathering errors\n");
            free(a); free(b); free(c);
            free(local_copy_time); free(local_scale_time);
            free(local_add_time); free(local_triad_time);
            free(all_copy_time); free(all_scale_time);
            free(all_add_time); free(all_triad_time);
            return TPBE_MALLOC_FAIL;
        }
    }
    
    /* Gather all errors to rank 0 */
    MPI_Gather(&local_errval, 1, MPI_DOUBLE, all_errvals, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    
    /* Rank 0 computes and reports min, max, mean errors */
    if (rank == 0) {
        double min_err = all_errvals[0];
        double max_err = all_errvals[0];
        double sum_err = 0.0;
        int min_rank = 0;
        int max_rank = 0;
        
        for (int r = 0; r < nprocs; r++) {
            sum_err += all_errvals[r];
            if (all_errvals[r] < min_err) {
                min_err = all_errvals[r];
                min_rank = r;
            }
            if (all_errvals[r] > max_err) {
                max_err = all_errvals[r];
                max_rank = r;
            }
        }
        
        double mean_err = sum_err / nprocs;
        
        tpb_printf(TPBM_PRTN_M_DIRECT, "-------------------------------------------------------------\n");
        tpb_printf(TPBM_PRTN_M_DIRECT, "Solution Validation:\n");
        tpb_printf(TPBM_PRTN_M_DIRECT, "  Min error: %.6e (rank %d)\n", min_err, min_rank);
        tpb_printf(TPBM_PRTN_M_DIRECT, "  Max error: %.6e (rank %d)\n", max_err, max_rank);
        tpb_printf(TPBM_PRTN_M_DIRECT, "  Mean error: %.6e (across all ranks)\n", mean_err);
        
        if (max_err <= epsilon) {
            tpb_printf(TPBM_PRTN_M_DIRECT, "  Solution validates: avg error less than %.6e on all ranks\n", epsilon);
        } else {
            tpb_printf(TPBM_PRTN_M_DIRECT, "  WARNING: Solution validation failed on some ranks\n");
        }
        tpb_printf(TPBM_PRTN_M_DIRECT, "-------------------------------------------------------------\n");
        
        free(all_errvals);
    }

    /* Cleanup */
    free((void *)a);
    free((void *)b);
    free((void *)c);
    free(local_copy_time);
    free(local_scale_time);
    free(local_add_time);
    free(local_triad_time);
    if (rank == 0) {
        free(all_copy_time);
        free(all_scale_time);
        free(all_add_time);
        free(all_triad_time);
    }

    return err;
}

static int 
check_d_stream(int narr, int ntest, double *a, double *b, double *c, 
               double s, double epsilon, double *errval)
{
    double a0 = 1.0;
    double b0 = 2.0;
    double c0 = 3.0;

    /* Simulate 4 operations repeated ntest times */
    for (int i = 0; i < ntest; i++) {
        c0 = a0;
        b0 = s * c0;
        c0 = a0 + b0;
        a0 = b0 + s * c0;
    }

    *errval = 0;
    for (int i = 0; i < narr; i++) {
        *errval += (a[i] - a0) > 0 ? (a[i] - a0) : (a0 - a[i]);
    }

    if (*errval > epsilon) return TPBE_KERN_VERIFY_FAIL;

    return 0;
}
