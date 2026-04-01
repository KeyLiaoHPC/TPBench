/*
 * tpb_corelib_mpi_stub.c
 * Stub MPI corelib init when TPBench is built without MPI.
 */

#include "tpb-public.h"

int
tpb_mpik_corelib_init(void *mpi_comm, const char *tpb_workspace_path)
{
    (void)mpi_comm;
    (void)tpb_workspace_path;
    return TPBE_ILLEGAL_CALL;
}

int
tpb_mpik_write_task(tpb_k_rthdl_t *hdl, int exit_code,
                    unsigned char *task_id_out,
                    unsigned char *tcap_id_out)
{
    (void)hdl;
    (void)exit_code;
    (void)task_id_out;
    (void)tcap_id_out;
    return TPBE_ILLEGAL_CALL;
}
