/*
 * tpb_corelib_mpi.c
 * MPI-coordinated corelib initialization for MPI-linked kernel processes.
 */

#include <mpi.h>
#include <stdlib.h>
#include <unistd.h>

#include "tpb-public.h"
#include "tpb_corelib_state.h"

/* Local Function Prototypes */
static int mpi_fail_if(int mpi_rc);

/*
 * Map MPI error return to TPBench errno.
 */
static int
mpi_fail_if(int mpi_rc)
{
    if (mpi_rc != MPI_SUCCESS) {
        return TPBE_MPI_FAIL;
    }
    return TPBE_SUCCESS;
}

int
tpb_mpik_corelib_init(void *mpi_comm, const char *tpb_workspace_path)
{
    MPI_Comm comm;
    int rank;
    int nprocs;
    int bcast_err = 0;
    int local_err;
    int max_err;
    int local_pid;
    int *allpids;
    int gather_alloc_fail;
    int max_alloc_fail;
    int r;
    int merr;

    if (mpi_comm == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    comm = (MPI_Comm)mpi_comm;

    merr = mpi_fail_if(MPI_Comm_rank(comm, &rank));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }
    merr = mpi_fail_if(MPI_Comm_size(comm, &nprocs));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }

    if (rank == 0) {
        bcast_err = _tpb_init_corelib(tpb_workspace_path,
            TPB_CORELIB_CTX_CALLER_KERNEL_MPI_MAIN_RANK);
    }

    merr = mpi_fail_if(MPI_Bcast(&bcast_err, 1, MPI_INT, 0, comm));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }

    if (bcast_err != TPBE_SUCCESS) {
        if (rank != 0) {
            tpb_printf(TPBM_PRTN_M_DIRECT,
                "At tpb_mpik_corelib_init: Rank %d receives abort message\n",
                rank);
            return TPBE_MPI_FAIL;
        }
        return bcast_err;
    }

    if (rank == 0) {
        local_err = TPBE_SUCCESS;
    } else {
        local_err = _tpb_init_corelib(tpb_workspace_path,
            TPB_CORELIB_CTX_CALLER_KERNEL_MPI_SUB_RANK);
    }

    merr = mpi_fail_if(MPI_Allreduce(&local_err, &max_err, 1, MPI_INT,
        MPI_MAX, comm));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }
    if (max_err != TPBE_SUCCESS) {
        return max_err;
    }

    allpids = NULL;
    if (rank == 0) {
        allpids = calloc((size_t)nprocs, sizeof *allpids);
    }
    gather_alloc_fail = (rank == 0 && allpids == NULL) ? 1 : 0;
    merr = mpi_fail_if(MPI_Allreduce(&gather_alloc_fail, &max_alloc_fail, 1,
        MPI_INT, MPI_MAX, comm));
    if (merr != TPBE_SUCCESS) {
        free(allpids);
        return merr;
    }
    if (max_alloc_fail != 0) {
        return TPBE_MALLOC_FAIL;
    }

    local_pid = (int)getpid();
    merr = mpi_fail_if(MPI_Gather(&local_pid, 1, MPI_INT, allpids, 1, MPI_INT,
        0, comm));
    if (merr != TPBE_SUCCESS) {
        free(allpids);
        return merr;
    }

    if (rank == 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "TPBench is now initializing by rank: ");
        for (r = 0; r < nprocs; r++) {
            tpb_printf(TPBM_PRTN_M_DIRECT, "%d (pid=%d)%s",
                r, allpids[r], (r + 1 < nprocs) ? ", " : "");
        }
        tpb_printf(TPBM_PRTN_M_DIRECT, "\n");
        free(allpids);
    }

    merr = mpi_fail_if(MPI_Barrier(comm));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }

    if (rank == 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "TPBench has been initialized by the MPI kernel.\n");
    }

    return TPBE_SUCCESS;
}
