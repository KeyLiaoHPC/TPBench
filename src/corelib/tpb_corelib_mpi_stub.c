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
