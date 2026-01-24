/**
 * tpbk_axpy_mpi.c
 * MPI-enabled BLAS AXPY: a[i] = a[i] + s * b[i]
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
int _tpbk_register_axpy_mpi(void);
int _tpbk_run_axpy_mpi(void);
int tpbk_pli_register_axpy_mpi(void);
static int d_axpy_mpi(tpb_timer_t *timer, int ntest, double kib, uint32_t array_size,
                      int64_t twarm_ms, int64_t *step_time, uint64_t *real_total_memsize,
                      uint32_t *array_size_out, double *bw);
static int check_d_axpy(int narr, int ntest, double *a, double *b, double s,
                        double epsilon, double *errval);

/* PLI registration */
int
tpbk_pli_register_axpy_mpi(void)
{
    int err;

    err = tpb_k_register("axpy_mpi", "BLAS AXPY with MPI+OpenMP: a[i] = a[i] + s * b[i]", TPB_KTYPE_PLI);
    if (err != 0) return err;

    err = tpb_k_add_parm("ntest", "Number of test iterations", "10",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)1, (int64_t)100000);
    if (err != 0) return err;
    err = tpb_k_add_parm("total_memsize", "Total memory size across all ranks in KiB", "32",
                         TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_RANGE,
                         0.0009765625, DBL_MAX);
    if (err != 0) return err;
    err = tpb_k_add_parm("array_size", "Number of elements per array per rank (0 = use total_memsize)", "0",
                         TPB_PARM_CLI | TPB_UINT32_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)4294967295);
    if (err != 0) return err;
    err = tpb_k_add_parm("twarm", "Warm-up time in milliseconds", "1",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)10000);
    if (err != 0) return err;

    return tpb_k_finalize_pli();
}

int
_tpbk_register_axpy_mpi(void)
{
    int err;
    
    err = tpb_k_register("axpy_mpi", "BLAS AXPY with MPI+OpenMP: a[i] = a[i] + s * b[i]", TPB_KTYPE_FLI);
    if (err != 0) return err;

    err = tpb_k_add_parm("ntest", "Number of test iterations", "10",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)1, (int64_t)100000);
    if (err != 0) return err;
    err = tpb_k_add_parm("total_memsize", "Total memory size across all ranks in KiB", "32",
                         TPB_PARM_CLI | TPB_DOUBLE_T | TPB_PARM_RANGE,
                         0.0009765625, DBL_MAX);
    if (err != 0) return err;
    err = tpb_k_add_parm("array_size", "Number of elements per array per rank (0 = use total_memsize)", "0",
                         TPB_PARM_CLI | TPB_UINT32_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)4294967295);
    if (err != 0) return err;
    err = tpb_k_add_parm("twarm", "Warm-up time in milliseconds", "1",
                         TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
                         (int64_t)0, (int64_t)10000);
    if (err != 0) return err;

    err = tpb_k_add_output("step_time", "Measured runtime of per loop step (min across ranks).", 
                           TPB_DTYPE_TIMER_T, TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;
    err = tpb_k_add_output("real_total_memsize", "Actual memory footprint of two axpy arrays.",
                           TPB_UINT64_T, TPB_UNIT_B | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;
    err = tpb_k_add_output("array_size", "Actual number of elements per array per rank.",
                           TPB_UINT32_T, TPB_UNAME_UNDEF | TPB_UBASE_BASE | TPB_UATTR_CAST_N | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;

    err = tpb_k_add_runner(_tpbk_run_axpy_mpi);
    if (err != 0) return err;
    
    return 0;
}

int
_tpbk_run_axpy_mpi(void)
{
    int tpberr;
    int ntest;
    TPB_UNIT_T tpb_uname;
    tpb_timer_t timer;
    double total_memsize;
    uint32_t array_size;
    int64_t twarm_ms;
    void *step_time = NULL;
    uint64_t *real_total_memsize = NULL;
    double *bw = NULL;

    tpberr = tpb_k_get_timer(&timer);
    if (tpberr) return tpberr;

    tpberr = tpb_k_get_arg("ntest", TPB_INT64_T, (void *)&ntest);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("total_memsize", TPB_DOUBLE_T, (void *)&total_memsize);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("array_size", TPB_UINT32_T, (void *)&array_size);
    if (tpberr) return tpberr;
    tpberr = tpb_k_get_arg("twarm", TPB_INT64_T, (void *)&twarm_ms);
    if (tpberr) return tpberr;

    tpberr = tpb_k_alloc_output("step_time", ntest, &step_time);
    if (tpberr) return tpberr;
    tpberr = tpb_k_alloc_output("real_total_memsize", 1, &real_total_memsize);
    if (tpberr) return tpberr;
    uint32_t *array_size_out = NULL;
    tpberr = tpb_k_alloc_output("array_size", 1, &array_size_out);
    if (tpberr) return tpberr;

    tpb_uname = timer.unit & TPB_UNAME_MASK;
    if (tpb_uname == TPB_UNAME_WALLTIME) {
        tpb_k_add_output("bw_walltime", "Measured sustainable memory bandwidth in decimal based MB/s.", 
                         TPB_DOUBLE_T, TPB_UNIT_MBPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("bw_walltime", ntest, &bw);
        if (tpberr) return tpberr;
    } else if (tpb_uname == TPB_UNAME_PHYSTIME) {
        tpb_k_add_output("bw_phystime", "Measured sustainable memory bandwidth in binary based Byte/cy.", 
                         TPB_DOUBLE_T, TPB_UNIT_BYTEPCY | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_1D);
        tpberr = tpb_k_alloc_output("bw_phystime", ntest, &bw);
        if (tpberr) return tpberr;
    } else {
        tpb_printf(TPBM_PRTN_M_DIRECT, "In kernel axpy_mpi: unknown timer unit name %llx\n", 
                   (unsigned long long)tpb_uname);
        return TPBE_KERN_ARG_FAIL;
    }

    tpberr = d_axpy_mpi(&timer, ntest, total_memsize, array_size, twarm_ms,
                        step_time, real_total_memsize, array_size_out, bw);

    return tpberr;
}

static int
d_axpy_mpi(tpb_timer_t *timer, int ntest, double kib, uint32_t array_size,
           int64_t twarm_ms, int64_t *step_time, uint64_t *real_total_memsize,
           uint32_t *array_size_out, double *bw)
{
    int narr, err, k;
    double *a, *b;
    double s = 0.42;
    int rank, nprocs;
    uint64_t total_array_size, base_size, array_bytes;

    int64_t *local_step_time = NULL;
    int64_t *all_step_time = NULL;
    double local_errval = 0.0;
    double *all_errvals = NULL;

    err = 0;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    if (array_size > 0) {
        total_array_size = array_size;
    } else {
        /* axpy uses 2 arrays */
        total_array_size = (uint64_t)(kib * 1024 / sizeof(double) / 2);
    }
    
    base_size = total_array_size / nprocs;
    narr = (int)base_size;
    
    if (rank == nprocs - 1) {
        narr += (int)(total_array_size % nprocs);
    }

    array_bytes = narr * sizeof(double);
    k = posix_memalign((void **)&a, ARRAY_ALIGNMENT, array_bytes);
    if (k != 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "Rank %d: Allocation of array a failed\n", rank);
        return TPBE_MALLOC_FAIL;
    }
    k = posix_memalign((void **)&b, ARRAY_ALIGNMENT, array_bytes);
    if (k != 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "Rank %d: Allocation of array b failed\n", rank);
        free(a);
        return TPBE_MALLOC_FAIL;
    }

    *real_total_memsize = (narr * nprocs + total_array_size % nprocs) * sizeof(double) * 2;
    *array_size_out = narr;

    if (rank == 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "-------------------------------------------------------------\n");
        tpb_printf(TPBM_PRTN_M_DIRECT, "MPI AXPY Benchmark\n");
        tpb_printf(TPBM_PRTN_M_DIRECT, "-------------------------------------------------------------\n");
        tpb_printf(TPBM_PRTN_M_DIRECT, "Data is distributed across %d MPI ranks\n", nprocs);
        tpb_printf(TPBM_PRTN_M_DIRECT, "Array size per rank = %u elements\n", (unsigned int)narr);
        tpb_printf(TPBM_PRTN_M_DIRECT, "Each kernel will be executed %d times.\n", ntest);
        tpb_printf(TPBM_PRTN_M_DIRECT, "The SCALAR value used for this run is %f\n", s);
        tpb_printf(TPBM_PRTN_M_DIRECT, "-------------------------------------------------------------\n");
    }

    local_step_time = (int64_t *)malloc(sizeof(int64_t) * ntest);
    if (!local_step_time) return TPBE_MALLOC_FAIL;

    if (rank == 0) {
        all_step_time = (int64_t *)malloc(sizeof(int64_t) * ntest * nprocs);
        if (!all_step_time) return TPBE_MALLOC_FAIL;
    }

    for (int i = 0; i < narr; i++) {
        a[i] = 1.0;
        b[i] = 2.0;
    }

    MPI_Barrier(MPI_COMM_WORLD);

    /* Warm-up */
    struct timespec wts;
    uint64_t wns0, wns1;
    clock_gettime(CLOCK_MONOTONIC, &wts);
    wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    wns1 = wns0 + (uint64_t)twarm_ms * 1000000ULL;
    while (wns0 < wns1) {
        #pragma omp parallel for shared(a, b, s, narr)
        for (int j = 0; j < narr; j++) {
            a[j] = a[j] + s * b[j];
        }
        clock_gettime(CLOCK_MONOTONIC, &wts);
        wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    }

    for (int i = 0; i < narr; i++) {
        a[i] = 1.0;
        b[i] = 2.0;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    timer->init();

    for (int i = 0; i < ntest; i++) {
        MPI_Barrier(MPI_COMM_WORLD);

        timer->tick(local_step_time + i);
        #pragma omp parallel for shared(a, b, s, narr)
        for (int j = 0; j < narr; j++) {
            a[j] = a[j] + s * b[j];
        }
        timer->tock(local_step_time + i);
    }

    MPI_Gather(local_step_time, ntest, MPI_INT64_T, 
               all_step_time, ntest, MPI_INT64_T, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        for (int i = 0; i < ntest; i++) {
            int64_t min_time = all_step_time[i];
            for (int r = 1; r < nprocs; r++) {
                if (all_step_time[r * ntest + i] < min_time)
                    min_time = all_step_time[r * ntest + i];
            }
            step_time[i] = min_time;
        }

        /* AXPY: read a, read b, write a = 3 memory ops per element */
        for (int i = 0; i < ntest; i++) {
            uint64_t bytes = 3 * (uint64_t)narr * sizeof(double) * nprocs;
            bw[i] = ((double)bytes * 1e-6) / ((double)(step_time[i]) * 1e-9);
        }
    }

    err = check_d_axpy(narr, ntest, a, b, s, epsilon, &local_errval);
    
    if (rank == 0) {
        all_errvals = (double *)malloc(sizeof(double) * nprocs);
    }
    
    MPI_Gather(&local_errval, 1, MPI_DOUBLE, all_errvals, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    
    if (rank == 0) {
        double max_err = all_errvals[0];
        for (int r = 1; r < nprocs; r++) {
            if (all_errvals[r] > max_err) max_err = all_errvals[r];
        }
        tpb_printf(TPBM_PRTN_M_DIRECT, "axpy_mpi max error: %.6e\n", max_err);
        free(all_errvals);
    }

    free((void *)a);
    free((void *)b);
    free(local_step_time);
    if (rank == 0) free(all_step_time);

    return err;
}

static int 
check_d_axpy(int narr, int ntest, double *a, double *b, double s,
             double epsilon, double *errval)
{
    double a0 = 1.0;
    double b0 = 2.0;

    for (int i = 0; i < ntest; i++) {
        a0 = a0 + s * b0;
    }

    *errval = 0;
    for (int i = 0; i < narr; i++) {
        *errval += (a[i] - a0) > 0 ? (a[i] - a0) : (a0 - a[i]);
    }

    if (*errval > epsilon) return TPBE_KERN_VERIFY_FAIL;

    return 0;
}
