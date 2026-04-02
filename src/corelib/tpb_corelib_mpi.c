/*
 * tpb_corelib_mpi.c
 * MPI-coordinated corelib initialization for MPI-linked kernel processes.
 */

#include <mpi.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tpb-public.h"
#include "tpb-autorecord.h"
#include "tpb_corelib_state.h"

/* Local Function Prototypes */
static int _sf_mpi_fail_if(int mpi_rc);

static MPI_Comm s_mpik_comm = MPI_COMM_NULL;

/*
 * Map MPI error return to TPBench errno.
 */
static int
_sf_mpi_fail_if(int mpi_rc)
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
    s_mpik_comm = comm;

    merr = _sf_mpi_fail_if(MPI_Comm_rank(comm, &rank));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }
    merr = _sf_mpi_fail_if(MPI_Comm_size(comm, &nprocs));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }

    if (rank == 0) {
        bcast_err = _tpb_init_corelib(tpb_workspace_path,
            TPB_CORELIB_CTX_CALLER_KERNEL_MPI_MAIN_RANK);
    }

    merr = _sf_mpi_fail_if(MPI_Bcast(&bcast_err, 1, MPI_INT, 0, comm));
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

    merr = _sf_mpi_fail_if(MPI_Allreduce(&local_err, &max_err, 1, MPI_INT,
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
    merr = _sf_mpi_fail_if(MPI_Allreduce(&gather_alloc_fail, &max_alloc_fail, 1,
        MPI_INT, MPI_MAX, comm));
    if (merr != TPBE_SUCCESS) {
        free(allpids);
        return merr;
    }
    if (max_alloc_fail != 0) {
        return TPBE_MALLOC_FAIL;
    }

    local_pid = (int)getpid();
    merr = _sf_mpi_fail_if(MPI_Gather(&local_pid, 1, MPI_INT, allpids, 1, MPI_INT,
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

    merr = _sf_mpi_fail_if(MPI_Barrier(comm));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }

    if (rank == 0) {
        tpb_printf(TPBM_PRTN_M_DIRECT,
            "TPBench has been initialized by the MPI kernel.\n");
    }

    return TPBE_SUCCESS;
}

int
tpb_mpik_write_task(tpb_k_rthdl_t *hdl, int exit_code,
                    unsigned char *task_id_out,
                    unsigned char *tcap_id_out)
{
    MPI_Comm comm;
    int rank;
    int nprocs;
    unsigned char my_task_id[20];
    unsigned char capsule_id[20];
    int werr;
    int cap_err;
    int merr;
    int dup_err;
    int max_dup_err;
    unsigned char *all_ids;
    int rank0_alloc_ok;
    int min_alloc_ok;
    int ar;
    int step_err;
    int append_agg;
    int final_err;

    if (hdl == NULL) {
        return TPBE_NULLPTR_ARG;
    }
    if (s_mpik_comm == MPI_COMM_NULL) {
        return TPBE_ILLEGAL_CALL;
    }
    comm = s_mpik_comm;

    merr = _sf_mpi_fail_if(MPI_Comm_rank(comm, &rank));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }
    merr = _sf_mpi_fail_if(MPI_Comm_size(comm, &nprocs));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }

    memset(my_task_id, 0, sizeof(my_task_id));
    memset(capsule_id, 0, sizeof(capsule_id));

    werr = tpb_record_write_task(hdl, exit_code, my_task_id);

    merr = _sf_mpi_fail_if(MPI_Barrier(comm));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }

    cap_err = TPBE_SUCCESS;
    if (rank == 0) {
        if (werr == TPBE_SUCCESS) {
            cap_err = tpb_k_create_capsule_task(my_task_id, capsule_id);
        } else {
            cap_err = werr;
        }
    }

    merr = _sf_mpi_fail_if(MPI_Bcast(capsule_id, 20, MPI_BYTE, 0, comm));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }
    merr = _sf_mpi_fail_if(MPI_Bcast(&cap_err, 1, MPI_INT, 0, comm));
    if (merr != TPBE_SUCCESS) {
        return merr;
    }

    final_err = cap_err;

    if (cap_err == TPBE_SUCCESS) {
        dup_err = tpb_k_task_set_dup_to(my_task_id, capsule_id);
        merr = _sf_mpi_fail_if(MPI_Allreduce(&dup_err, &max_dup_err, 1, MPI_INT,
            MPI_MAX, comm));
        if (merr != TPBE_SUCCESS) {
            return merr;
        }
        if (max_dup_err != TPBE_SUCCESS) {
            final_err = max_dup_err;
        }
    }

    if (cap_err == TPBE_SUCCESS && nprocs >= 2) {
        all_ids = NULL;
        rank0_alloc_ok = 1;
        if (rank == 0) {
            all_ids = calloc((size_t)nprocs, (size_t)20);
            if (all_ids == NULL) {
                rank0_alloc_ok = 0;
            }
        }
        merr = _sf_mpi_fail_if(MPI_Allreduce(&rank0_alloc_ok, &min_alloc_ok, 1,
            MPI_INT, MPI_MIN, comm));
        if (merr != TPBE_SUCCESS) {
            free(all_ids);
            return merr;
        }
        if (min_alloc_ok == 0) {
            free(all_ids);
            if (task_id_out != NULL) {
                memcpy(task_id_out, my_task_id, (size_t)20);
            }
            if (tcap_id_out != NULL) {
                memcpy(tcap_id_out, capsule_id, (size_t)20);
            }
            return TPBE_MALLOC_FAIL;
        }

        merr = _sf_mpi_fail_if(MPI_Gather(my_task_id, 20, MPI_BYTE, all_ids, 20,
            MPI_BYTE, 0, comm));
        if (merr != TPBE_SUCCESS) {
            free(all_ids);
            return merr;
        }

        append_agg = TPBE_SUCCESS;
        if (rank == 0) {
            for (ar = 1; ar < nprocs; ar++) {
                step_err = tpb_k_append_capsule_task(capsule_id,
                    all_ids + (size_t)ar * (size_t)20);
                if (step_err != TPBE_SUCCESS) {
                    append_agg = step_err;
                    break;
                }
            }
            free(all_ids);
        }
        merr = _sf_mpi_fail_if(MPI_Bcast(&append_agg, 1, MPI_INT, 0, comm));
        if (merr != TPBE_SUCCESS) {
            return merr;
        }
        if (append_agg != TPBE_SUCCESS) {
            final_err = append_agg;
        }
    }

    if (task_id_out != NULL) {
        memcpy(task_id_out, my_task_id, (size_t)20);
    }
    if (tcap_id_out != NULL) {
        memcpy(tcap_id_out, capsule_id, (size_t)20);
    }

    if (final_err == TPBE_SUCCESS && rank == 0) {
        char cap_hex[41];

        tpb_raf_id_to_hex(capsule_id, cap_hex);
        tpb_printf(TPBM_PRTN_M_TSTAG | TPBE_NOTE,
                   "MPI Task recorded. Task capsule ID = %s, included %d "
                   "tasks\n",
                   cap_hex, nprocs);
    }

    return final_err;
}
