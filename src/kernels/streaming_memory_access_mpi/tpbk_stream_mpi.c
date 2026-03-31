/*
 * tpbk_stream_mpi.c
 * STREAM benchmark (MPI + OpenMP) as a TPBench PLI kernel.
 */

#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <float.h>
#include <math.h>
#include <mpi.h>
#ifdef _OPENMP
#include <omp.h>
#endif

#include "tpb-public.h"
#include "tpbench.h"

#ifndef STREAM_TYPE
#define STREAM_TYPE double
#endif

#define HLINE "-------------------------------------------------------------\n"
#define SCALAR 0.42
#define ARRAY_ALIGNMENT 64

#define MALLOC_ALIGN_GOTO(_A, _nel, _label) \
    do { \
        int _ma_rc = posix_memalign((void **)&(_A), ARRAY_ALIGNMENT, \
            sizeof(STREAM_TYPE) * (size_t)(_nel)); \
        if (_ma_rc != 0 || (_A) == NULL) { \
            err = TPBE_MALLOC_FAIL; \
            goto _label; \
        } \
    } while (0)

/* Local Function Prototypes */
int tpbk_pli_register_stream_mpi(void);
static int run_stream_mpi(void);

static int d_stream_mpi(tpb_timer_t *timer, int rank, int nprocs, int ntest,
    uint64_t agg_elems, int64_t twarm_ms, int64_t *copy_time,
    int64_t *scale_time, int64_t *add_time, int64_t *triad_time,
    uint64_t *real_total_memsize, uint64_t *array_size_out,
    double *copy_bw, double *scale_bw, double *add_bw, double *triad_bw,
    double **summary16);

/* -------- STREAM MPI reference-style globals (for ported helpers) -------- */
#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif
#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

static STREAM_TYPE *a;
static STREAM_TYPE *b;
static STREAM_TYPE *c;
static uint64_t array_elements;

#define M 20

static int checktick(void);
static void computeSTREAMerrors(STREAM_TYPE *aAvgErr, STREAM_TYPE *bAvgErr,
    STREAM_TYPE *cAvgErr, int ntimes);
static void checkSTREAMresults(STREAM_TYPE *AvgErrByRank, int numranks,
    int ntimes);

#ifndef abs
#define abs(x) ((x) >= 0 ? (x) : -(x))
#endif

static const char *const s_summary_names[16] = {
    "Summary::Copy Best Rate MB/s",
    "Summary::Copy Avg time",
    "Summary::Copy Min time",
    "Summary::Copy Max time",
    "Summary::Scale Best Rate MB/s",
    "Summary::Scale Avg time",
    "Summary::Scale Min time",
    "Summary::Scale Max time",
    "Summary::Add Best Rate MB/s",
    "Summary::Add Avg time",
    "Summary::Add Min time",
    "Summary::Add Max time",
    "Summary::Triad Best Rate MB/s",
    "Summary::Triad Avg time",
    "Summary::Triad Min time",
    "Summary::Triad Max time",
};

int
tpbk_pli_register_stream_mpi(void)
{
    int err;
    int i;

    err = tpb_k_register("stream_mpi",
        "STREAM benchmark with MPI and OpenMP.", TPB_KTYPE_PLI);
    if (err != 0) {
        return err;
    }

    err = tpb_k_add_parm("ntest", "Number of test iterations", "10",
        TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
        (int64_t)2, (int64_t)100000);
    if (err != 0) {
        return err;
    }
    err = tpb_k_add_parm("stream_array_size",
        "Total aggregate elements per array across all MPI ranks", "0",
        TPB_PARM_CLI | TPB_UINT64_T | TPB_PARM_RANGE,
        (int64_t)1, (int64_t)9223372036854775807LL);
    if (err != 0) {
        return err;
    }
    err = tpb_k_add_parm("twarm",
        "Warm-up time in milliseconds, 0<=twarm<=10000, default 500", "500",
        TPB_PARM_CLI | TPB_INT64_T | TPB_PARM_RANGE,
        (int64_t)0, (int64_t)10000);
    if (err != 0) {
        return err;
    }

    err = tpb_k_add_output("Allocated memory size",
        "Local memory footprint of three stream arrays (bytes).",
        TPB_UINT64_T,
        TPB_UNIT_B | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) {
        return err;
    }
    err = tpb_k_add_output("STREAM array size",
        "Local number of elements per array on this rank.",
        TPB_UINT64_T,
        TPB_UNAME_UNDEF | TPB_UBASE_BASE | TPB_UATTR_CAST_N
            | TPB_UATTR_TRIM_N | TPB_UATTR_SHAPE_POINT);
    if (err != 0) {
        return err;
    }
    err = tpb_k_add_output("Time::Copy", "Measured runtime of copy operation.",
        TPB_DTYPE_TIMER_T,
        TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y
            | TPB_UATTR_SHAPE_1D);
    if (err != 0) {
        return err;
    }
    err = tpb_k_add_output("Time::Scale", "Measured runtime of scale operation.",
        TPB_DTYPE_TIMER_T,
        TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y
            | TPB_UATTR_SHAPE_1D);
    if (err != 0) {
        return err;
    }
    err = tpb_k_add_output("Time::Add", "Measured runtime of add operation.",
        TPB_DTYPE_TIMER_T,
        TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y
            | TPB_UATTR_SHAPE_1D);
    if (err != 0) {
        return err;
    }
    err = tpb_k_add_output("Time::Triad", "Measured runtime of triad operation.",
        TPB_DTYPE_TIMER_T,
        TPB_UNIT_TIMER | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y
            | TPB_UATTR_SHAPE_1D);
    if (err != 0) {
        return err;
    }
    err = tpb_k_add_output("Bandwidth::Copy",
        "Local copy bandwidth in decimal based MB/s.",
        TPB_DOUBLE_T,
        TPB_UNIT_MBPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y
            | TPB_UATTR_SHAPE_1D);
    if (err != 0) {
        return err;
    }
    err = tpb_k_add_output("Bandwidth::Scale",
        "Local scale bandwidth in decimal based MB/s.",
        TPB_DOUBLE_T,
        TPB_UNIT_MBPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y
            | TPB_UATTR_SHAPE_1D);
    if (err != 0) {
        return err;
    }
    err = tpb_k_add_output("Bandwidth::Add",
        "Local add bandwidth in decimal based MB/s.",
        TPB_DOUBLE_T,
        TPB_UNIT_MBPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y
            | TPB_UATTR_SHAPE_1D);
    if (err != 0) {
        return err;
    }
    err = tpb_k_add_output("Bandwidth::Triad",
        "Local triad bandwidth in decimal based MB/s.",
        TPB_DOUBLE_T,
        TPB_UNIT_MBPS | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y
            | TPB_UATTR_SHAPE_1D);
    if (err != 0) {
        return err;
    }

    for (i = 0; i < 16; i++) {
        TPB_UNIT_T u = (i % 4 == 0) ? TPB_UNIT_MBPS : TPB_UNIT_SEC;
        err = tpb_k_add_output(s_summary_names[i],
            "Rank 0 global STREAM summary; other ranks store zero.",
            TPB_DOUBLE_T,
            u | TPB_UATTR_CAST_Y | TPB_UATTR_TRIM_Y | TPB_UATTR_SHAPE_POINT);
        if (err != 0) {
            return err;
        }
    }

    return tpb_k_finalize_pli();
}

static int
run_stream_mpi(void)
{
    int tpberr;
    int64_t ntest64;
    int ntest;
    int rank;
    int nprocs;
    TPB_UNIT_T tpb_uname;
    tpb_timer_t timer;
    uint64_t agg_elems;
    int64_t twarm_ms;
    void *copy_time = NULL;
    void *scale_time = NULL;
    void *add_time = NULL;
    void *triad_time = NULL;
    uint64_t *real_total_memsize = NULL;
    uint64_t *array_size_out = NULL;
    double *copy_bw = NULL;
    double *scale_bw = NULL;
    double *add_bw = NULL;
    double *triad_bw = NULL;
    double *summary_ptrs[16];
    int i;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    tpberr = tpb_k_get_timer(&timer);
    if (tpberr) {
        return tpberr;
    }

    tpberr = tpb_k_get_arg("ntest", TPB_INT64_T, (void *)&ntest64);
    if (tpberr) {
        return tpberr;
    }
    tpberr = tpb_k_get_arg("stream_array_size", TPB_UINT64_T,
        (void *)&agg_elems);
    if (tpberr) {
        return tpberr;
    }
    tpberr = tpb_k_get_arg("twarm", TPB_INT64_T, (void *)&twarm_ms);
    if (tpberr) {
        return tpberr;
    }

    if (ntest64 < 2) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "stream_mpi: ntest must be >= 2, got %" PRId64 "\n", ntest64);
        return TPBE_KERN_ARG_FAIL;
    }
    if (ntest64 > (int64_t)INT_MAX) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "stream_mpi: ntest %" PRId64 " exceeds INT_MAX\n", ntest64);
        return TPBE_KERN_ARG_FAIL;
    }
    ntest = (int)ntest64;

    tpberr = tpb_k_alloc_output("Time::Copy", (uint64_t)ntest, &copy_time);
    if (tpberr) {
        return tpberr;
    }
    tpberr = tpb_k_alloc_output("Time::Scale", (uint64_t)ntest, &scale_time);
    if (tpberr) {
        return tpberr;
    }
    tpberr = tpb_k_alloc_output("Time::Add", (uint64_t)ntest, &add_time);
    if (tpberr) {
        return tpberr;
    }
    tpberr = tpb_k_alloc_output("Time::Triad", (uint64_t)ntest, &triad_time);
    if (tpberr) {
        return tpberr;
    }
    tpberr = tpb_k_alloc_output("Allocated memory size", 1,
        (void **)&real_total_memsize);
    if (tpberr) {
        return tpberr;
    }
    tpberr = tpb_k_alloc_output("STREAM array size", 1,
        (void **)&array_size_out);
    if (tpberr) {
        return tpberr;
    }

    tpb_uname = timer.unit & TPB_UNAME_MASK;
    if (tpb_uname == TPB_UNAME_WALLTIME) {
        tpberr = tpb_k_alloc_output("Bandwidth::Copy", (uint64_t)ntest,
            (void **)&copy_bw);
        if (tpberr) {
            return tpberr;
        }
        tpberr = tpb_k_alloc_output("Bandwidth::Scale", (uint64_t)ntest,
            (void **)&scale_bw);
        if (tpberr) {
            return tpberr;
        }
        tpberr = tpb_k_alloc_output("Bandwidth::Add", (uint64_t)ntest,
            (void **)&add_bw);
        if (tpberr) {
            return tpberr;
        }
        tpberr = tpb_k_alloc_output("Bandwidth::Triad", (uint64_t)ntest,
            (void **)&triad_bw);
        if (tpberr) {
            return tpberr;
        }
    } else {
        tpb_printf(TPBM_PRTN_M_DIRECT, "Unsupported timer: %s\n The STREAM benchmark only support wallclock timer. ", timer.name);
        return TPBE_KERN_ARG_FAIL;
    }

    for (i = 0; i < 16; i++) {
        void *p = NULL;
        tpberr = tpb_k_alloc_output(s_summary_names[i], 1, &p);
        if (tpberr) {
            return tpberr;
        }
        summary_ptrs[i] = (double *)p;
        summary_ptrs[i][0] = 0.0;
    }

    tpberr = d_stream_mpi(&timer, rank, nprocs, ntest, agg_elems, twarm_ms,
        copy_time, scale_time, add_time, triad_time,
        real_total_memsize, array_size_out,
        copy_bw, scale_bw, add_bw, triad_bw, summary_ptrs);

    return tpberr;
}

static int
d_stream_mpi(tpb_timer_t *timer, int rank, int nprocs, int ntest,
    uint64_t agg_elems, int64_t twarm_ms, int64_t *copy_time,
    int64_t *scale_time, int64_t *add_time, int64_t *triad_time,
    uint64_t *real_total_memsize, uint64_t *array_size_out,
    double *copy_bw, double *scale_bw, double *add_bw, double *triad_bw,
    double **summary16)
{
    int err = 0;
    int BytesPerWord;
    ssize_t j;
    int i, k;
    STREAM_TYPE scalar;
    uint64_t base_sz;
    uint64_t local_n;
    size_t array_bytes;
    int quantum;
    double t_est_us;
    int64_t tcal_lo;
    STREAM_TYPE AvgError[3];
    STREAM_TYPE *AvgErrByRank = NULL;
    double *times_double = NULL;
    double *TimesByRank = NULL;
    STREAM_TYPE s = (STREAM_TYPE)SCALAR;
    double bytes_agg[4];

    static char *label[4] = {"Copy:      ", "Scale:     ", "Add:       ",
        "Triad:     "};

    if (agg_elems == 0 || (uint64_t)nprocs > agg_elems) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "stream_mpi: illegal stream_array_size %" PRIu64 " nprocs %d\n",
            agg_elems, nprocs);
        return TPBE_KERN_ARG_FAIL;
    }

    base_sz = agg_elems / (uint64_t)nprocs;
    local_n = base_sz;
    if (rank == nprocs - 1) {
        local_n += agg_elems % nprocs;
    }
    if (local_n <= 0) {
        return TPBE_KERN_ARG_FAIL;
    }

    array_elements = local_n;
    *array_size_out = local_n;
    *real_total_memsize = 3 * (uint64_t)sizeof(STREAM_TYPE) * local_n;

    a = NULL;
    b = NULL;
    c = NULL;
    a = malloc(local_n * sizeof (STREAM_TYPE));
    b = malloc(local_n * sizeof (STREAM_TYPE));
    c = malloc(local_n * sizeof (STREAM_TYPE));
    times_double = malloc(ntest * sizeof(double));
    if (a == NULL || b == NULL || c == NULL || times_double == NULL) {
        if (a) free(a);
        if (b) free(b);
        if (c) free(c);
        if (times_double) free(times_double);
        tpb_printf(TPBM_PRTN_M_DIRECT, "FATAL: Rank %d failed at malloc stream arrays.\n", rank);
        MPI_Abort(MPI_COMM_WORLD, TPBE_MALLOC_FAIL);
    }

    array_bytes = (size_t)local_n * sizeof(STREAM_TYPE);
    (void)array_bytes;

    bytes_agg[0] = 2.0 * (double)sizeof(STREAM_TYPE) * (double)agg_elems;
    bytes_agg[1] = bytes_agg[0];
    bytes_agg[2] = 3.0 * (double)sizeof(STREAM_TYPE) * (double)agg_elems;
    bytes_agg[3] = bytes_agg[2];
    uint64_t cbytes = 2 * (uint64_t)array_elements * sizeof(STREAM_TYPE);
    uint64_t sbytes = 2 * (uint64_t)array_elements * sizeof(STREAM_TYPE);
    uint64_t abytes = 3 * (uint64_t)array_elements * sizeof(STREAM_TYPE);
    uint64_t tbytes = 3 * (uint64_t)array_elements * sizeof(STREAM_TYPE);

    // Rank 0 needs to allocate arrays to hold error data and timing data from
	// all ranks for analysis and output.
	// Allocate and instantiate the arrays here -- after the primary arrays 
	// have been instantiated -- so there is no possibility of having these 
	// auxiliary arrays mess up the NUMA placement of the primary arrays.

    if (rank == 0) {
        // There are 3 average error values for each rank (using STREAM_TYPE).
        AvgErrByRank = (double *) malloc(3 * sizeof(STREAM_TYPE) * nprocs);
        if (AvgErrByRank == NULL) {
            tpb_printf(TPBM_PRTN_M_DIRECT, "Ooops -- allocation of arrays to collect errors on MPI rank 0 failed\n");
            MPI_Abort(MPI_COMM_WORLD, TPBE_MALLOC_FAIL);
        }
        memset(AvgErrByRank,0,3*sizeof(STREAM_TYPE)*nprocs);

        // There are 4*NTIMES timing values for each rank (always doubles)
        TimesByRank = (double *) malloc(4 * ntest * sizeof(double) * nprocs);
        if (TimesByRank == NULL) {
            tpb_printf(TPBM_PRTN_M_DIRECT, "Ooops -- allocation of arrays to collect timing data on MPI rank 0 failed\n");
            MPI_Abort(MPI_COMM_WORLD, TPBE_MALLOC_FAIL);
        }
        memset(TimesByRank,0,4*ntest*sizeof(double)*nprocs);
    }


    if (rank == 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT, HLINE);
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "STREAM version $Revision: 1.8 $ (TPBench stream_mpi)\n");
        tpb_printf(TPBM_PRTN_M_DIRECT, HLINE);
        BytesPerWord = (int)sizeof(STREAM_TYPE);
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "This system uses %d bytes per array element.\n", BytesPerWord);
        tpb_printf(TPBM_PRTN_M_DIRECT, HLINE);
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "Total Aggregate Array size = %" PRIu64 " (elements)\n",
            agg_elems);
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "Total Aggregate Memory per array = %.1f MiB (= %.1f GiB).\n",
            BytesPerWord * ((double)agg_elems / 1024.0 / 1024.0),
            BytesPerWord * ((double)agg_elems / 1024.0 / 1024.0 / 1024.0));
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "Total Aggregate memory required = %.1f MiB (= %.1f GiB).\n",
            (3.0 * BytesPerWord) * ((double)agg_elems / 1024.0 / 1024.0),
            (3.0 * BytesPerWord) * ((double)agg_elems / 1024.0 / 1024.0
                / 1024.0));
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "Data is distributed across %d MPI ranks\n", nprocs);
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "   Array size per MPI rank = %zu (elements)\n", array_elements);
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "   Memory per array per MPI rank = %.1f MiB (= %.1f GiB).\n",
            BytesPerWord * ((double)array_elements / 1024.0 / 1024.0),
            BytesPerWord * ((double)array_elements / 1024.0 / 1024.0
                / 1024.0));
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "   Total memory per MPI rank = %.1f MiB (= %.1f GiB).\n",
            (3.0 * BytesPerWord) * ((double)array_elements / 1024.0 / 1024.0),
            (3.0 * BytesPerWord) * ((double)array_elements / 1024.0 / 1024.0
                / 1024.0));
        tpb_printf(TPBM_PRTN_M_DIRECT, HLINE);
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "Each kernel will be executed %d times.\n", ntest);
        tpb_printf(TPBM_PRTN_M_DIRECT,
            " The *best* time for each kernel (excluding the first iteration)\n");
        tpb_printf(TPBM_PRTN_M_DIRECT,
            " will be used to compute the reported bandwidth.\n");
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "The SCALAR value used for this run is %f\n", SCALAR);
#ifdef _OPENMP
        tpb_printf(TPBM_PRTN_M_DIRECT, HLINE);
#pragma omp parallel
        {
#pragma omp master
            {
                k = omp_get_num_threads();
                tpb_printf(TPBM_PRTN_M_DIRECT,
                    "Number of Threads requested for each MPI rank = %i\n", k);
            }
        }
#endif
#ifdef _OPENMP
        k = 0;
#pragma omp parallel
#pragma omp atomic
        k++;
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "Number of Threads counted for rank 0 = %i\n", k);
#endif
    }

#pragma omp parallel for
    for (j = 0; j < (ssize_t)array_elements; j++) {
        a[j] = 1.0;
        b[j] = 2.0;
        c[j] = 0.0;
    }


    {
        struct timespec wts;
        uint64_t wns0, wns1;
        clock_gettime(CLOCK_MONOTONIC, &wts);
        wns0 = (uint64_t)wts.tv_sec * 1000000000ULL
            + (uint64_t)wts.tv_nsec;
        wns1 = wns0 + (uint64_t)twarm_ms * 1000000ULL;
        while (wns0 < wns1) {
            #pragma omp parallel for
            for (j = 0; j < (ssize_t)array_elements; j++) {
                a[j] = b[j] + s * c[j];
            }
            clock_gettime(CLOCK_MONOTONIC, &wts);
            wns0 = (uint64_t)wts.tv_sec * 1000000000ULL
                + (uint64_t)wts.tv_nsec;
        }
    }

#pragma omp parallel for
    for (j = 0; j < (ssize_t)array_elements; j++) {
        a[j] = 1.0;
        b[j] = 2.0;
        c[j] = 0.0;
    }

    if (rank == 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT, HLINE);
        quantum = checktick();
        if (quantum >= 1) {
            tpb_printf(TPBM_PRTN_M_DIRECT,
                "Your timer granularity/precision appears to be "
                "%d microseconds.\n", quantum);
        } else {
            tpb_printf(TPBM_PRTN_M_DIRECT,
                "Your timer granularity appears to be "
                "less than one microsecond.\n");
            quantum = 1;
        }
    }




    timer->tick(&tcal_lo);
#pragma omp parallel for
    for (j = 0; j < (ssize_t)array_elements; j++) {
        a[j] = 2.0E0 * a[j];
    }
    timer->tock(&tcal_lo);
    t_est_us = (double)tcal_lo / 1000.0;

    if (rank == 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "Each test below will take on the order of %d microseconds.\n",
            (int)t_est_us);
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "   (= %d timer ticks)\n", (int)(t_est_us / (double)quantum));
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "Increase the size of the arrays if this shows that\n");
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "you are not getting at least 20 timer ticks per test.\n");
        tpb_printf(TPBM_PRTN_M_DIRECT, HLINE);
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "WARNING -- The above is only a rough guideline.\n");
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "For best results, please be sure you know the\n");
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "precision of your system timer.\n");
        tpb_printf(TPBM_PRTN_M_DIRECT, HLINE);
    }

    scalar = (STREAM_TYPE)SCALAR;
    timer->init();

    for (k = 0; k < ntest; k++) {
        timer->tick(&copy_time[k]);
        MPI_Barrier(MPI_COMM_WORLD);
#pragma omp parallel for
        for (j = 0; j < (ssize_t)array_elements; j++) {
            c[j] = a[j];
        }
        MPI_Barrier(MPI_COMM_WORLD);
        timer->tock(&copy_time[k]);

        timer->tick(&scale_time[k]);
        MPI_Barrier(MPI_COMM_WORLD);
#pragma omp parallel for
        for (j = 0; j < (ssize_t)array_elements; j++) {
            b[j] = scalar * c[j];
        }
        MPI_Barrier(MPI_COMM_WORLD);
        timer->tock(&scale_time[k]);

        timer->tick(&add_time[k]);
        MPI_Barrier(MPI_COMM_WORLD);
#pragma omp parallel for
        for (j = 0; j < (ssize_t)array_elements; j++) {
            c[j] = a[j] + b[j];
        }
        MPI_Barrier(MPI_COMM_WORLD);
        timer->tock(&add_time[k]);

        timer->tick(&triad_time[k]);
        MPI_Barrier(MPI_COMM_WORLD);
#pragma omp parallel for
        for (j = 0; j < (ssize_t)array_elements; j++) {
            a[j] = b[j] + scalar * c[j];
        }
        MPI_Barrier(MPI_COMM_WORLD);
        timer->tock(&triad_time[k]);
    }

    /*
     * Gather timings in kernel order (Copy, Scale, Add, Triad). Each block is
     * nprocs * ntest doubles: rank-major, iteration-minor, same as one
     * MPI_Gather per kernel with recvcount = ntest per rank.
     */
    for (i = 0; i < ntest; i++) {
        times_double[i] = (double)copy_time[i] * 1e-9;
    }
    MPI_Gather(times_double, ntest, MPI_DOUBLE, TimesByRank, ntest, MPI_DOUBLE, 0,
        MPI_COMM_WORLD);

    for (i = 0; i < ntest; i++) {
        times_double[i] = (double)scale_time[i] * 1e-9;
    }
    MPI_Gather(times_double, ntest, MPI_DOUBLE, TimesByRank + ntest * nprocs, ntest, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    for (i = 0; i < ntest; i++) {
        times_double[i] = (double)add_time[i] * 1e-9;
    }
    MPI_Gather(times_double, ntest, MPI_DOUBLE, TimesByRank + 2 * ntest * nprocs, ntest, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    for (i = 0; i < ntest; i++) {
        times_double[i] = (double)triad_time[i] * 1e-9;
    }
    MPI_Gather(times_double, ntest, MPI_DOUBLE, TimesByRank + 3 * ntest * nprocs, ntest, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    for (k = 0; k < ntest; k++) {
        copy_bw[k] = copy_time[k] == 0? 0: (double)cbytes / ((double)(copy_time[k]) * 1e-3);
        scale_bw[k] = scale_time[k] == 0? 0: (double)sbytes / ((double)(scale_time[k]) * 1e-3);
        add_bw[k] = add_time[k] == 0? 0: (double)abytes / ((double)(add_time[k]) * 1e-3);
        triad_bw[k] = triad_time[k] == 0? 0: (double)tbytes / ((double)(triad_time[k]) * 1e-3);
    }

    if (rank == 0) {
        double avgtime[4] = {0., 0., 0., 0.};
        double maxtime[4] = {0., 0., 0., 0.};
        double mintime[4] = {FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX};
        size_t blksz;

        /*
         * TimesByRank: block j (0..3) is kernel j; within a block, rank i times
         * start at offset i * ntest (MPI_Gather rank order).
         * Per stream_mpi.c: for each (iteration, kernel), min over ranks; then
         * for k >= 1 only, accumulate avg / min / max and best MB/s from mintime.
         */
        blksz = (size_t)ntest * (size_t)nprocs;
        for (k = 1; k < ntest; k++) {
            for (j = 0; j < 4; j++) {
                double t_min_rank;
                size_t jk_base;
                size_t idx;

                jk_base = (size_t)j * blksz;
                t_min_rank = 1.0e36;
                for (i = 0; i < nprocs; i++) {
                    idx = jk_base + (size_t)i * (size_t)ntest + (size_t)k;
                    t_min_rank = MIN(t_min_rank, TimesByRank[idx]);
                }
                avgtime[j] += t_min_rank;
                mintime[j] = MIN(mintime[j], t_min_rank);
                maxtime[j] = MAX(maxtime[j], t_min_rank);
            }
        }

        for (j = 0; j < 4; j++) {
            avgtime[j] /= (double)(ntest - 1);
        }

        tpb_printf(TPBM_PRTN_M_DIRECT,
            "Function    Best Rate MB/s  Avg time     Min time     Max time\n");
        for (j = 0; j < 4; j++) {
            double best_mb_s = 0.0;

            if (mintime[j] > 0.0 && mintime[j] < (double)FLT_MAX) {
                best_mb_s = 1.0E-06 * bytes_agg[j] / mintime[j];
            }
            tpb_printf(TPBM_PRTN_M_DIRECT,
                "%s%11.1f  %11.6f  %11.6f  %11.6f\n",
                label[j], best_mb_s, avgtime[j], mintime[j], maxtime[j]);

            summary16[j * 4 + 0][0] = best_mb_s;
            summary16[j * 4 + 1][0] = avgtime[j];
            summary16[j * 4 + 2][0] = mintime[j];
            summary16[j * 4 + 3][0] = maxtime[j];
        }
        tpb_printf(TPBM_PRTN_M_DIRECT, HLINE);
    }

    computeSTREAMerrors(&AvgError[0], &AvgError[1], &AvgError[2], ntest);
    MPI_Gather(AvgError, 3, MPI_DOUBLE, AvgErrByRank, 3, MPI_DOUBLE, 0,
        MPI_COMM_WORLD);

    if (rank == 0) {
        checkSTREAMresults(AvgErrByRank, nprocs, ntest);
        tpb_printf(TPBM_PRTN_M_DIRECT, HLINE);
    }

cleanup_alloc:
    if (TimesByRank) {
        free(TimesByRank);
    }
    if (AvgErrByRank) {
        free(AvgErrByRank);
    }
    if (times_double) {
        free(times_double);
        times_double = NULL;
    }
    free(c);
    free(b);
    free(a);
    a = b = c = NULL;

    return err;

}

int
checktick(void)
{
    int i, minDelta, Delta;
    struct timespec w0, w1;
    double timesfound[M];
    int64_t a0, a1;

    for (i = 0; i < M; i++) {
        clock_gettime(CLOCK_MONOTONIC, &w0);
        a0 = (int64_t)w0.tv_sec * 1000000000LL + w0.tv_nsec;
        do {
            clock_gettime(CLOCK_MONOTONIC, &w1);
            a1 = (int64_t)w1.tv_sec * 1000000000LL + w1.tv_nsec;
        } while ((a1 - a0) < 1000);
        timesfound[i] = (double)a1 * 1e-9;
    }

    minDelta = 1000000;
    for (i = 1; i < M; i++) {
        Delta = (int)(1.0E6 * (timesfound[i] - timesfound[i - 1]));
        minDelta = MIN(minDelta, MAX(Delta, 0));
    }

    return minDelta;
}

void
computeSTREAMerrors(STREAM_TYPE *aAvgErr, STREAM_TYPE *bAvgErr,
    STREAM_TYPE *cAvgErr, int ntimes)
{
    STREAM_TYPE aj, bj, cj, scalar;
    STREAM_TYPE aSumErr, bSumErr, cSumErr;
    ssize_t jj;
    int kk;

    aj = 1.0;
    bj = 2.0;
    cj = 0.0;
    aj = 2.0E0 * aj;
    scalar = (STREAM_TYPE)SCALAR;
    for (kk = 0; kk < ntimes; kk++) {
        cj = aj;
        bj = scalar * cj;
        cj = aj + bj;
        aj = bj + scalar * cj;
    }

    aSumErr = 0.0;
    bSumErr = 0.0;
    cSumErr = 0.0;
    for (jj = 0; jj < (ssize_t)array_elements; jj++) {
        aSumErr += abs(a[jj] - aj);
        bSumErr += abs(b[jj] - bj);
        cSumErr += abs(c[jj] - cj);
    }
    *aAvgErr = aSumErr / (STREAM_TYPE)array_elements;
    *bAvgErr = bSumErr / (STREAM_TYPE)array_elements;
    *cAvgErr = cSumErr / (STREAM_TYPE)array_elements;
}

void
checkSTREAMresults(STREAM_TYPE *AvgErrByRank, int numranks, int ntimes)
{
    STREAM_TYPE aj, bj, cj, scalar;
    STREAM_TYPE aSumErr, bSumErr, cSumErr;
    STREAM_TYPE aAvgErr, bAvgErr, cAvgErr;
    double epsilon;
    ssize_t jj;
    int ierr, err;
    int kk;

    aj = 1.0;
    bj = 2.0;
    cj = 0.0;
    aj = 2.0E0 * aj;
    scalar = (STREAM_TYPE)SCALAR;
    for (kk = 0; kk < ntimes; kk++) {
        cj = aj;
        bj = scalar * cj;
        cj = aj + bj;
        aj = bj + scalar * cj;
    }

    aSumErr = 0.0;
    bSumErr = 0.0;
    cSumErr = 0.0;
    for (kk = 0; kk < numranks; kk++) {
        aSumErr += AvgErrByRank[3 * kk + 0];
        bSumErr += AvgErrByRank[3 * kk + 1];
        cSumErr += AvgErrByRank[3 * kk + 2];
    }
    aAvgErr = aSumErr / (STREAM_TYPE)numranks;
    bAvgErr = bSumErr / (STREAM_TYPE)numranks;
    cAvgErr = cSumErr / (STREAM_TYPE)numranks;

    if (sizeof(STREAM_TYPE) == 4) {
        epsilon = 1.e-6;
    } else if (sizeof(STREAM_TYPE) == 8) {
        epsilon = 1.e-13;
    } else {
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "WEIRD: sizeof(STREAM_TYPE) = %zu\n", sizeof(STREAM_TYPE));
        epsilon = 1.e-6;
    }

    err = 0;
    if (abs(aAvgErr / aj) > epsilon) {
        err++;
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "Failed Validation on array a[], AvgRelAbsErr > epsilon (%e)\n",
            epsilon);
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "     Expected Value: %e, AvgAbsErr: %e, AvgRelAbsErr: %e\n",
            aj, aAvgErr, abs(aAvgErr) / aj);
        ierr = 0;
        for (jj = 0; jj < (ssize_t)array_elements; jj++) {
            if (abs(a[jj] / aj - 1.0) > epsilon) {
                ierr++;
#ifdef VERBOSE
                if (ierr < 10) {
                    tpb_printf(TPBM_PRTN_M_DIRECT,
                        "         array a: index: %ld, expected: %e, "
                        "observed: %e, relative error: %e\n",
                        (long)jj, aj, a[jj], abs((aj - a[jj]) / aAvgErr));
                }
#endif
            }
        }
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "     For array a[], %d errors were found.\n", ierr);
    }
    if (abs(bAvgErr / bj) > epsilon) {
        err++;
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "Failed Validation on array b[], AvgRelAbsErr > epsilon (%e)\n",
            epsilon);
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "     Expected Value: %e, AvgAbsErr: %e, AvgRelAbsErr: %e\n",
            bj, bAvgErr, abs(bAvgErr) / bj);
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "     AvgRelAbsErr > Epsilon (%e)\n", epsilon);
        ierr = 0;
        for (jj = 0; jj < (ssize_t)array_elements; jj++) {
            if (abs(b[jj] / bj - 1.0) > epsilon) {
                ierr++;
#ifdef VERBOSE
                if (ierr < 10) {
                    tpb_printf(TPBM_PRTN_M_DIRECT,
                        "         array b: index: %ld, expected: %e, "
                        "observed: %e, relative error: %e\n",
                        (long)jj, bj, b[jj], abs((bj - b[jj]) / bAvgErr));
                }
#endif
            }
        }
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "     For array b[], %d errors were found.\n", ierr);
    }
    if (abs(cAvgErr / cj) > epsilon) {
        err++;
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "Failed Validation on array c[], AvgRelAbsErr > epsilon (%e)\n",
            epsilon);
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "     Expected Value: %e, AvgAbsErr: %e, AvgRelAbsErr: %e\n",
            cj, cAvgErr, abs(cAvgErr) / cj);
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "     AvgRelAbsErr > Epsilon (%e)\n", epsilon);
        ierr = 0;
        for (jj = 0; jj < (ssize_t)array_elements; jj++) {
            if (abs(c[jj] / cj - 1.0) > epsilon) {
                ierr++;
#ifdef VERBOSE
                if (ierr < 10) {
                    tpb_printf(TPBM_PRTN_M_DIRECT,
                        "         array c: index: %ld, expected: %e, "
                        "observed: %e, relative error: %e\n",
                        (long)jj, cj, c[jj], abs((cj - c[jj]) / cAvgErr));
                }
#endif
            }
        }
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "     For array c[], %d errors were found.\n", ierr);
    }
    if (err == 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "Solution Validates: avg error less than %e on all three arrays\n",
            epsilon);
    }
#ifdef VERBOSE
    tpb_printf(TPBM_PRTN_M_DIRECT,
        "Results Validation Verbose Results: \n");
    tpb_printf(TPBM_PRTN_M_DIRECT,
        "    Expected a(1), b(1), c(1): %f %f %f \n", aj, bj, cj);
    tpb_printf(TPBM_PRTN_M_DIRECT,
        "    Observed a(1), b(1), c(1): %f %f %f \n", a[1], b[1], c[1]);
    tpb_printf(TPBM_PRTN_M_DIRECT,
        "    Rel Errors on a, b, c:     %e %e %e \n",
        abs(aAvgErr / aj), abs(bAvgErr / bj), abs(cAvgErr / cj));
#endif
}

#ifdef TPB_K_BUILD_MAIN
int
main(int argc, char **argv)
{
    int err;
    int rank;
    int nprocs;
    const char *timer_name = NULL;
    unsigned char my_task_id[20];
    unsigned char capsule_id[20];
    unsigned char kernel_id_bin[20];
    uint32_t handle_index = 0;
    int werr;
    int cap_err;
    const char *ev;

    memset(my_task_id, 0, sizeof(my_task_id));
    memset(capsule_id, 0, sizeof(capsule_id));
    memset(kernel_id_bin, 0, sizeof(kernel_id_bin));

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    /*
     * Required for tpb_raf_resolve_workspace (write_task + capsule paths).
     * Uses TPB_WORKSPACE from the environment when non-NULL.
     */
    err = tpb_k_corelib_init(NULL);
    if (err != 0 && err != TPBE_ILLEGAL_CALL) {
        if (rank == 0) {
            fprintf(stderr, "Error: tpb_k_corelib_init failed: %d\n", err);
        }
        MPI_Finalize();
        return err;
    }

    if (argc >= 2) {
        timer_name = argv[1];
    } else {
        timer_name = getenv("TPBENCH_TIMER");
    }
    if (timer_name == NULL) {
        if (rank == 0) {
            fprintf(stderr,
                "Error: Timer not specified (argv[1] or TPBENCH_TIMER)\n");
        }
        MPI_Finalize();
        return TPBE_CLI_FAIL;
    }

    err = tpb_k_pli_set_timer(timer_name);
    if (err != 0) {
        if (rank == 0) {
            fprintf(stderr, "Error: Failed to set timer '%s'\n", timer_name);
        }
        MPI_Finalize();
        return err;
    }

    err = tpbk_pli_register_stream_mpi();
    if (err != 0) {
        if (rank == 0) {
            fprintf(stderr, "Error: Failed to register kernel\n");
        }
        MPI_Finalize();
        return err;
    }

    {
        tpb_k_rthdl_t handle;
        err = tpb_k_pli_build_handle(&handle, argc - 2, argv + 2);
        if (err != 0) {
            if (rank == 0) {
                fprintf(stderr, "Error: Failed to build handle\n");
            }
            MPI_Finalize();
            return err;
        }

        if (rank == 0) {
            tpb_cliout_args(&handle);
            tpb_printf(TPBM_PRTN_M_DIRECT, "Kernel logs\n");
        }

        ev = getenv("TPB_HANDLE_INDEX");
        if (ev != NULL) {
            handle_index = (uint32_t)atoi(ev);
        }
        ev = getenv("TPB_KERNEL_ID");
        if (ev != NULL && strlen(ev) == 40) {
            (void)tpb_raf_hex_to_id(ev, kernel_id_bin);
        }

        err = run_stream_mpi();

        if (err != 0) {
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_FAIL,
                "Rank %d: Kernel stream_mpi failed: %d\n", rank, err);
            werr = tpb_k_write_task(&handle, err, NULL);
            (void)werr;
            tpb_driver_clean_handle(&handle);
            MPI_Finalize();
            return err;
        }

        if (rank == 0) {
            int baton = 0;
            MPI_Status status;
            tpb_cliout_results(&handle);
            tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
                "Kernel stream_mpi finished successfully.\n");
            fflush(stdout);
            if (nprocs > 1) {
                MPI_Send(&baton, 1, MPI_INT, rank + 1, rank + 1, MPI_COMM_WORLD);
            }
        } else {
            int baton = 0;
            MPI_Status status;
            MPI_Recv(&baton, 1, MPI_INT, rank - 1, rank, MPI_COMM_WORLD, &status);
            if (rank != nprocs - 1) {
                MPI_Send(&baton, 1, MPI_INT, (rank + 1) % nprocs, (rank + 1) % nprocs, MPI_COMM_WORLD);
            }
        }

        tpb_printf(TPBM_PRTN_M_DIRECT | TPBE_NOTE, "Rank %d starts write task.\n", rank);
        fflush(stdout);
        werr = tpb_k_write_task(&handle, 0, my_task_id);
        tpb_printf(TPBM_PRTN_M_DIRECT | TPBE_NOTE, "Rank %d write task ended.\n", rank);
        fflush(stdout);

        MPI_Barrier(MPI_COMM_WORLD);

        memset(capsule_id, 0, sizeof(capsule_id));
        cap_err = TPBE_SUCCESS;
        if (rank == 0) {
            if (werr == 0) {
                cap_err = tpb_k_create_capsule_task(my_task_id, capsule_id);
                if (cap_err != 0) {
                    tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                        "stream_mpi: tpb_k_create_capsule_task failed %d\n",
                        cap_err);
                }
                tpb_printf(TPBM_PRTN_M_DIRECT | TPBE_NOTE, "Rank 0 created task capsule ended.\n");
            } else {
                cap_err = werr;
            }
        }
        (void)MPI_Bcast(capsule_id, 20, MPI_BYTE, 0, MPI_COMM_WORLD);
        (void)MPI_Bcast(&cap_err, 1, MPI_INT, 0, MPI_COMM_WORLD);

        if (nprocs >= 2 && cap_err == 0 && rank != 0) {
            cap_err = tpb_k_append_capsule_task(capsule_id, my_task_id);
            if (cap_err != 0) {
                tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_WARN,
                    "stream_mpi: tpb_k_append_capsule_task failed %d\n",
                    cap_err);
            }
        }

        MPI_Barrier(MPI_COMM_WORLD);
        if (rank == 0 && werr == 0 && cap_err == 0) {
            (void)tpb_k_unlink_capsule_sync_shm(kernel_id_bin, handle_index);
        }

        tpb_driver_clean_handle(&handle);
    }

    MPI_Finalize();
    return 0;
}
#endif /* TPB_K_BUILD_MAIN */
