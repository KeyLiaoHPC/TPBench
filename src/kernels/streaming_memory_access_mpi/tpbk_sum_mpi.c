/**
 * tpbk_sum_mpi.c
 * MPI-enabled Sum (Reduction): s += a[i] with MPI_Allreduce
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

static double epsilon = 1.e-6;

/* Forward declarations */
int _tpbk_register_sum_mpi(void);
int _tpbk_run_sum_mpi(void);
int tpbk_pli_register_sum_mpi(void);
static int d_sum_mpi(tpb_timer_t *timer, int ntest, double kib, uint32_t array_size,
                     int64_t twarm_ms,
                     int64_t *step_time, uint64_t *real_total_memsize,
                     uint32_t *array_size_out, double *result_sum, double *bw);
static int check_d_sum(int total_narr, int ntest, double *result_sum, double init_val,
                       double epsilon, double *errval);

/* PLI registration */
int
tpbk_pli_register_sum_mpi(void)
{
    int err;

    err = tpb_k_register("sum_mpi", "Sum (Reduction) with MPI+OpenMP: s += a[i]", TPB_KTYPE_PLI);
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
_tpbk_register_sum_mpi(void)
{
    int err;
    
    err = tpb_k_register("sum_mpi", "Sum (Reduction) with MPI+OpenMP: s += a[i]", TPB_KTYPE_FLI);
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
    err = tpb_k_add_output("real_total_memsize", "Actual memory footprint of sum array.",
                           TPB_UINT64_T, TPB_UNIT_B | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;
    err = tpb_k_add_output("array_size", "Actual number of elements per array per rank.",
                           TPB_UINT32_T, TPB_UNAME_UNDEF | TPB_UBASE_BASE | TPB_UATTR_CAST_N | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) return err;
    err = tpb_k_add_output("result_sum", "Global sum result per iteration (for verification).",
                           TPB_DOUBLE_T, TPB_UNAME_UNDEF | TPB_UBASE_BASE | TPB_UATTR_CAST_N | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_1D);
    if (err != 0) return err;

    err = tpb_k_add_runner(_tpbk_run_sum_mpi);
    if (err != 0) return err;
    
    return 0;
}

int
_tpbk_run_sum_mpi(void)
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
    double *result_sum = NULL;
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
    tpberr = tpb_k_alloc_output("result_sum", ntest, &result_sum);
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
        tpb_printf(TPBM_PRTN_M_DIRECT, "In kernel sum_mpi: unknown timer unit name %llx\n", 
                   (unsigned long long)tpb_uname);
        return TPBE_KERN_ARG_FAIL;
    }

    tpberr = d_sum_mpi(&timer, ntest, total_memsize, array_size, twarm_ms,
                       step_time, real_total_memsize, array_size_out, result_sum, bw);

    return tpberr;
}

static int
d_sum_mpi(tpb_timer_t *timer, int ntest, double kib, uint32_t array_size,
          int64_t twarm_ms,
          int64_t *step_time, uint64_t *real_total_memsize,
          uint32_t *array_size_out, double *result_sum, double *bw)
{
    int narr, err, k;
    double *a;
    double init_val = 0.11;
    double local_s, global_s;
    int rank, nprocs;
    uint64_t total_array_size, base_size, array_bytes;

    int64_t *local_step_time = NULL;
    int64_t *all_step_time = NULL;

    err = 0;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    if (array_size > 0) {
        total_array_size = array_size;
    } else {
        /* sum uses 1 array */
        total_array_size = (uint64_t)(kib * 1024 / sizeof(double));
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

    *real_total_memsize = (narr * nprocs + total_array_size % nprocs) * sizeof(double);
    *array_size_out = narr;

    if (rank == 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT, "-------------------------------------------------------------\n");
        tpb_printf(TPBM_PRTN_M_DIRECT, "MPI Sum (Reduction) Benchmark\n");
        tpb_printf(TPBM_PRTN_M_DIRECT, "-------------------------------------------------------------\n");
        tpb_printf(TPBM_PRTN_M_DIRECT, "Data is distributed across %d MPI ranks\n", nprocs);
        tpb_printf(TPBM_PRTN_M_DIRECT, "Array size per rank = %u elements\n", (unsigned int)narr);
        tpb_printf(TPBM_PRTN_M_DIRECT, "Each kernel will be executed %d times.\n", ntest);
        tpb_printf(TPBM_PRTN_M_DIRECT, "-------------------------------------------------------------\n");
    }

    local_step_time = (int64_t *)malloc(sizeof(int64_t) * ntest);
    if (!local_step_time) return TPBE_MALLOC_FAIL;

    if (rank == 0) {
        all_step_time = (int64_t *)malloc(sizeof(int64_t) * ntest * nprocs);
        if (!all_step_time) return TPBE_MALLOC_FAIL;
    }

    for (int i = 0; i < narr; i++) {
        a[i] = init_val;
    }

    MPI_Barrier(MPI_COMM_WORLD);

    /* Warm-up */
    struct timespec wts;
    uint64_t wns0, wns1;
    clock_gettime(CLOCK_MONOTONIC, &wts);
    wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    wns1 = wns0 + (uint64_t)twarm_ms * 1000000ULL;
    while (wns0 < wns1) {
        local_s = 0.0;
        #pragma omp parallel for shared(a, narr) reduction(+:local_s)
        for (int j = 0; j < narr; j++) {
            local_s += a[j];
        }
        MPI_Allreduce(&local_s, &global_s, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        clock_gettime(CLOCK_MONOTONIC, &wts);
        wns0 = wts.tv_sec * 1e9 + wts.tv_nsec;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    timer->init();

    for (int i = 0; i < ntest; i++) {
        MPI_Barrier(MPI_COMM_WORLD);

        timer->tick(local_step_time + i);
        local_s = 0.0;
        #pragma omp parallel for shared(a, narr) reduction(+:local_s)
        for (int j = 0; j < narr; j++) {
            local_s += a[j];
        }
        MPI_Allreduce(&local_s, &global_s, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        timer->tock(local_step_time + i);
        
        result_sum[i] = global_s;
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

        /* Sum: read a = 1 array per rank */
        for (int i = 0; i < ntest; i++) {
            uint64_t bytes = (uint64_t)narr * sizeof(double) * nprocs;
            bw[i] = ((double)bytes * 1e-6) / ((double)(step_time[i]) * 1e-9);
        }

        /* Calculate total array size for verification */
        int total_narr = narr * nprocs;
        if (total_array_size % nprocs != 0) {
            total_narr = (int)total_array_size;
        }
        
        double errval;
        err = check_d_sum(total_narr, ntest, result_sum, init_val, epsilon, &errval);
        tpb_printf(TPBM_PRTN_M_DIRECT, "sum_mpi relative error: %.6e\n", errval);
    }

    free((void *)a);
    free(local_step_time);
    if (rank == 0) free(all_step_time);

    return err;
}

static int 
check_d_sum(int total_narr, int ntest, double *result_sum, double init_val,
            double epsilon, double *errval)
{
    /* Expected sum: total_narr * init_val */
    double expected = (double)total_narr * init_val;

    *errval = 0;
    for (int i = 0; i < ntest; i++) {
        double diff = (result_sum[i] - expected);
        double rel_err = (diff > 0 ? diff : -diff) / expected;
        if (rel_err > *errval) *errval = rel_err;
    }

    if (*errval > epsilon) return TPBE_KERN_VERIFY_FAIL;

    return 0;
}
