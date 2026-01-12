/*
 * tpb-mpi_stub.c
 * MPI-3 stub implementation for building TPBench without MPI.
 */
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "tpb-mpi_stub.h"

/* Initialization and Finalization */

int MPI_Init(int *argc, char ***argv)
{
    (void)argc; (void)argv;
    return MPI_SUCCESS;
}

int MPI_Init_thread(int *argc, char ***argv, int required, int *provided)
{
    (void)argc; (void)argv; (void)required;
    if (provided) *provided = MPI_THREAD_SINGLE;
    return MPI_SUCCESS;
}

int MPI_Finalize(void)
{
    return MPI_SUCCESS;
}

int MPI_Finalized(int *flag)
{
    if (flag) *flag = 0;
    return MPI_SUCCESS;
}

int MPI_Initialized(int *flag)
{
    if (flag) *flag = 1;
    return MPI_SUCCESS;
}

int MPI_Abort(MPI_Comm comm, int errorcode)
{
    (void)comm; (void)errorcode;
    return MPI_SUCCESS;
}

/* Communicator Management */

int MPI_Comm_size(MPI_Comm comm, int *size)
{
    (void)comm;
    if (size) *size = 1;
    return MPI_SUCCESS;
}

int MPI_Comm_rank(MPI_Comm comm, int *rank)
{
    (void)comm;
    if (rank) *rank = 0;
    return MPI_SUCCESS;
}

int MPI_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm)
{
    (void)comm;
    if (newcomm) *newcomm = MPI_COMM_WORLD;
    return MPI_SUCCESS;
}

int MPI_Comm_create(MPI_Comm comm, MPI_Group group, MPI_Comm *newcomm)
{
    (void)comm; (void)group;
    if (newcomm) *newcomm = MPI_COMM_WORLD;
    return MPI_SUCCESS;
}

int MPI_Comm_split(MPI_Comm comm, int color, int key, MPI_Comm *newcomm)
{
    (void)comm; (void)color; (void)key;
    if (newcomm) *newcomm = MPI_COMM_WORLD;
    return MPI_SUCCESS;
}

int MPI_Comm_free(MPI_Comm *comm)
{
    (void)comm;
    return MPI_SUCCESS;
}

int MPI_Comm_compare(MPI_Comm comm1, MPI_Comm comm2, int *result)
{
    (void)comm1; (void)comm2;
    if (result) *result = 0;
    return MPI_SUCCESS;
}

int MPI_Comm_group(MPI_Comm comm, MPI_Group *group)
{
    (void)comm;
    if (group) *group = 0;
    return MPI_SUCCESS;
}

int MPI_Comm_test_inter(MPI_Comm comm, int *flag)
{
    (void)comm;
    if (flag) *flag = 0;
    return MPI_SUCCESS;
}

int MPI_Comm_remote_size(MPI_Comm comm, int *size)
{
    (void)comm;
    if (size) *size = 0;
    return MPI_SUCCESS;
}

int MPI_Comm_remote_group(MPI_Comm comm, MPI_Group *group)
{
    (void)comm;
    if (group) *group = MPI_GROUP_NULL;
    return MPI_SUCCESS;
}

/* Group Management */

int MPI_Group_size(MPI_Group group, int *size)
{
    (void)group;
    if (size) *size = 1;
    return MPI_SUCCESS;
}

int MPI_Group_rank(MPI_Group group, int *rank)
{
    (void)group;
    if (rank) *rank = 0;
    return MPI_SUCCESS;
}

int MPI_Group_translate_ranks(MPI_Group group1, int n, const int ranks1[],
                              MPI_Group group2, int ranks2[])
{
    (void)group1; (void)n; (void)ranks1; (void)group2;
    if (ranks2 && n > 0) ranks2[0] = 0;
    return MPI_SUCCESS;
}

int MPI_Group_compare(MPI_Group group1, MPI_Group group2, int *result)
{
    (void)group1; (void)group2;
    if (result) *result = 0;
    return MPI_SUCCESS;
}

int MPI_Group_union(MPI_Group group1, MPI_Group group2, MPI_Group *newgroup)
{
    (void)group1; (void)group2;
    if (newgroup) *newgroup = 0;
    return MPI_SUCCESS;
}

int MPI_Group_intersection(MPI_Group group1, MPI_Group group2, MPI_Group *newgroup)
{
    (void)group1; (void)group2;
    if (newgroup) *newgroup = 0;
    return MPI_SUCCESS;
}

int MPI_Group_difference(MPI_Group group1, MPI_Group group2, MPI_Group *newgroup)
{
    (void)group1; (void)group2;
    if (newgroup) *newgroup = 0;
    return MPI_SUCCESS;
}

int MPI_Group_incl(MPI_Group group, int n, const int ranks[], MPI_Group *newgroup)
{
    (void)group; (void)n; (void)ranks;
    if (newgroup) *newgroup = 0;
    return MPI_SUCCESS;
}

int MPI_Group_excl(MPI_Group group, int n, const int ranks[], MPI_Group *newgroup)
{
    (void)group; (void)n; (void)ranks;
    if (newgroup) *newgroup = 0;
    return MPI_SUCCESS;
}

int MPI_Group_range_incl(MPI_Group group, int n, int ranges[][3], MPI_Group *newgroup)
{
    (void)group; (void)n; (void)ranges;
    if (newgroup) *newgroup = 0;
    return MPI_SUCCESS;
}

int MPI_Group_range_excl(MPI_Group group, int n, int ranges[][3], MPI_Group *newgroup)
{
    (void)group; (void)n; (void)ranges;
    if (newgroup) *newgroup = 0;
    return MPI_SUCCESS;
}

int MPI_Group_free(MPI_Group *group)
{
    (void)group;
    return MPI_SUCCESS;
}

/* Point-to-Point Communication - Blocking */

int MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest,
             int tag, MPI_Comm comm)
{
    (void)buf; (void)count; (void)datatype; (void)dest; (void)tag; (void)comm;
    return MPI_SUCCESS;
}

int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source,
             int tag, MPI_Comm comm, MPI_Status *status)
{
    (void)buf; (void)count; (void)datatype; (void)source; (void)tag; (void)comm;
    if (status) {
        status->MPI_SOURCE = 0;
        status->MPI_TAG = 0;
        status->MPI_ERROR = MPI_SUCCESS;
    }
    return MPI_SUCCESS;
}

int MPI_Sendrecv(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                 int dest, int sendtag, void *recvbuf, int recvcount,
                 MPI_Datatype recvtype, int source, int recvtag,
                 MPI_Comm comm, MPI_Status *status)
{
    (void)sendbuf; (void)sendcount; (void)sendtype; (void)dest; (void)sendtag;
    (void)recvbuf; (void)recvcount; (void)recvtype; (void)source; (void)recvtag; (void)comm;
    if (status) {
        status->MPI_SOURCE = 0;
        status->MPI_TAG = 0;
        status->MPI_ERROR = MPI_SUCCESS;
    }
    return MPI_SUCCESS;
}

int MPI_Sendrecv_replace(void *buf, int count, MPI_Datatype datatype,
                         int dest, int sendtag, int source, int recvtag,
                         MPI_Comm comm, MPI_Status *status)
{
    (void)buf; (void)count; (void)datatype; (void)dest; (void)sendtag;
    (void)source; (void)recvtag; (void)comm;
    if (status) {
        status->MPI_SOURCE = 0;
        status->MPI_TAG = 0;
        status->MPI_ERROR = MPI_SUCCESS;
    }
    return MPI_SUCCESS;
}

/* Point-to-Point Communication - Send Modes */

int MPI_Bsend(const void *buf, int count, MPI_Datatype datatype, int dest,
              int tag, MPI_Comm comm)
{
    (void)buf; (void)count; (void)datatype; (void)dest; (void)tag; (void)comm;
    return MPI_SUCCESS;
}

int MPI_Ssend(const void *buf, int count, MPI_Datatype datatype, int dest,
              int tag, MPI_Comm comm)
{
    (void)buf; (void)count; (void)datatype; (void)dest; (void)tag; (void)comm;
    return MPI_SUCCESS;
}

int MPI_Rsend(const void *buf, int count, MPI_Datatype datatype, int dest,
              int tag, MPI_Comm comm)
{
    (void)buf; (void)count; (void)datatype; (void)dest; (void)tag; (void)comm;
    return MPI_SUCCESS;
}

/* Point-to-Point Communication - Non-blocking */

int MPI_Isend(const void *buf, int count, MPI_Datatype datatype, int dest,
              int tag, MPI_Comm comm, MPI_Request *request)
{
    (void)buf; (void)count; (void)datatype; (void)dest; (void)tag; (void)comm;
    if (request) *request = MPI_REQUEST_NULL;
    return MPI_SUCCESS;
}

int MPI_Irecv(void *buf, int count, MPI_Datatype datatype, int source,
              int tag, MPI_Comm comm, MPI_Request *request)
{
    (void)buf; (void)count; (void)datatype; (void)source; (void)tag; (void)comm;
    if (request) *request = MPI_REQUEST_NULL;
    return MPI_SUCCESS;
}

int MPI_Ibsend(const void *buf, int count, MPI_Datatype datatype, int dest,
               int tag, MPI_Comm comm, MPI_Request *request)
{
    (void)buf; (void)count; (void)datatype; (void)dest; (void)tag; (void)comm;
    if (request) *request = MPI_REQUEST_NULL;
    return MPI_SUCCESS;
}

int MPI_Issend(const void *buf, int count, MPI_Datatype datatype, int dest,
               int tag, MPI_Comm comm, MPI_Request *request)
{
    (void)buf; (void)count; (void)datatype; (void)dest; (void)tag; (void)comm;
    if (request) *request = MPI_REQUEST_NULL;
    return MPI_SUCCESS;
}

int MPI_Irsend(const void *buf, int count, MPI_Datatype datatype, int dest,
               int tag, MPI_Comm comm, MPI_Request *request)
{
    (void)buf; (void)count; (void)datatype; (void)dest; (void)tag; (void)comm;
    if (request) *request = MPI_REQUEST_NULL;
    return MPI_SUCCESS;
}

/* Request Management */

int MPI_Wait(MPI_Request *request, MPI_Status *status)
{
    (void)request;
    if (status) {
        status->MPI_SOURCE = 0;
        status->MPI_TAG = 0;
        status->MPI_ERROR = MPI_SUCCESS;
    }
    return MPI_SUCCESS;
}

int MPI_Test(MPI_Request *request, int *flag, MPI_Status *status)
{
    (void)request;
    if (flag) *flag = 1;
    if (status) {
        status->MPI_SOURCE = 0;
        status->MPI_TAG = 0;
        status->MPI_ERROR = MPI_SUCCESS;
    }
    return MPI_SUCCESS;
}

int MPI_Waitany(int count, MPI_Request array_of_requests[], int *index,
                MPI_Status *status)
{
    (void)count; (void)array_of_requests;
    if (index) *index = 0;
    if (status) {
        status->MPI_SOURCE = 0;
        status->MPI_TAG = 0;
        status->MPI_ERROR = MPI_SUCCESS;
    }
    return MPI_SUCCESS;
}

int MPI_Testany(int count, MPI_Request array_of_requests[], int *index,
                int *flag, MPI_Status *status)
{
    (void)count; (void)array_of_requests;
    if (index) *index = 0;
    if (flag) *flag = 1;
    if (status) {
        status->MPI_SOURCE = 0;
        status->MPI_TAG = 0;
        status->MPI_ERROR = MPI_SUCCESS;
    }
    return MPI_SUCCESS;
}

int MPI_Waitall(int count, MPI_Request array_of_requests[],
                MPI_Status array_of_statuses[])
{
    int i;
    (void)count; (void)array_of_requests;
    if (array_of_statuses) {
        for (i = 0; i < count; i++) {
            array_of_statuses[i].MPI_SOURCE = 0;
            array_of_statuses[i].MPI_TAG = 0;
            array_of_statuses[i].MPI_ERROR = MPI_SUCCESS;
        }
    }
    return MPI_SUCCESS;
}

int MPI_Testall(int count, MPI_Request array_of_requests[], int *flag,
                MPI_Status array_of_statuses[])
{
    int i;
    (void)count; (void)array_of_requests;
    if (flag) *flag = 1;
    if (array_of_statuses) {
        for (i = 0; i < count; i++) {
            array_of_statuses[i].MPI_SOURCE = 0;
            array_of_statuses[i].MPI_TAG = 0;
            array_of_statuses[i].MPI_ERROR = MPI_SUCCESS;
        }
    }
    return MPI_SUCCESS;
}

int MPI_Waitsome(int incount, MPI_Request array_of_requests[],
                 int *outcount, int array_of_indices[],
                 MPI_Status array_of_statuses[])
{
    (void)incount; (void)array_of_requests; (void)array_of_indices; (void)array_of_statuses;
    if (outcount) *outcount = 0;
    return MPI_SUCCESS;
}

int MPI_Testsome(int incount, MPI_Request array_of_requests[],
                 int *outcount, int array_of_indices[],
                 MPI_Status array_of_statuses[])
{
    (void)incount; (void)array_of_requests; (void)array_of_indices; (void)array_of_statuses;
    if (outcount) *outcount = 0;
    return MPI_SUCCESS;
}

int MPI_Request_free(MPI_Request *request)
{
    (void)request;
    return MPI_SUCCESS;
}

int MPI_Cancel(MPI_Request *request)
{
    (void)request;
    return MPI_SUCCESS;
}

int MPI_Request_get_status(MPI_Request request, int *flag, MPI_Status *status)
{
    (void)request;
    if (flag) *flag = 1;
    if (status) {
        status->MPI_SOURCE = 0;
        status->MPI_TAG = 0;
        status->MPI_ERROR = MPI_SUCCESS;
    }
    return MPI_SUCCESS;
}

/* Probe and Message Management */

int MPI_Probe(int source, int tag, MPI_Comm comm, MPI_Status *status)
{
    (void)source; (void)tag; (void)comm;
    if (status) {
        status->MPI_SOURCE = 0;
        status->MPI_TAG = 0;
        status->MPI_ERROR = MPI_SUCCESS;
    }
    return MPI_SUCCESS;
}

int MPI_Iprobe(int source, int tag, MPI_Comm comm, int *flag, MPI_Status *status)
{
    (void)source; (void)tag; (void)comm;
    if (flag) *flag = 0;
    if (status) {
        status->MPI_SOURCE = 0;
        status->MPI_TAG = 0;
        status->MPI_ERROR = MPI_SUCCESS;
    }
    return MPI_SUCCESS;
}

/* MPI-3 Matched Probe */

int MPI_Mprobe(int source, int tag, MPI_Comm comm, MPI_Message *message,
               MPI_Status *status)
{
    (void)source; (void)tag; (void)comm;
    if (message) *message = MPI_MESSAGE_NULL;
    if (status) {
        status->MPI_SOURCE = 0;
        status->MPI_TAG = 0;
        status->MPI_ERROR = MPI_SUCCESS;
    }
    return MPI_SUCCESS;
}

int MPI_Improbe(int source, int tag, MPI_Comm comm, int *flag,
                MPI_Message *message, MPI_Status *status)
{
    (void)source; (void)tag; (void)comm;
    if (flag) *flag = 0;
    if (message) *message = MPI_MESSAGE_NULL;
    if (status) {
        status->MPI_SOURCE = 0;
        status->MPI_TAG = 0;
        status->MPI_ERROR = MPI_SUCCESS;
    }
    return MPI_SUCCESS;
}

int MPI_Mrecv(void *buf, int count, MPI_Datatype datatype,
              MPI_Message *message, MPI_Status *status)
{
    (void)buf; (void)count; (void)datatype; (void)message;
    if (status) {
        status->MPI_SOURCE = 0;
        status->MPI_TAG = 0;
        status->MPI_ERROR = MPI_SUCCESS;
    }
    return MPI_SUCCESS;
}

int MPI_Imrecv(void *buf, int count, MPI_Datatype datatype,
               MPI_Message *message, MPI_Request *request)
{
    (void)buf; (void)count; (void)datatype; (void)message;
    if (request) *request = MPI_REQUEST_NULL;
    return MPI_SUCCESS;
}

/* Status Management */

int MPI_Get_count(const MPI_Status *status, MPI_Datatype datatype, int *count)
{
    (void)status; (void)datatype;
    if (count) *count = 0;
    return MPI_SUCCESS;
}

int MPI_Test_cancelled(const MPI_Status *status, int *flag)
{
    (void)status;
    if (flag) *flag = 0;
    return MPI_SUCCESS;
}

int MPI_Status_set_cancelled(MPI_Status *status, int flag)
{
    (void)status; (void)flag;
    return MPI_SUCCESS;
}

int MPI_Status_set_elements(MPI_Status *status, MPI_Datatype datatype, int count)
{
    (void)status; (void)datatype; (void)count;
    return MPI_SUCCESS;
}

int MPI_Get_elements(const MPI_Status *status, MPI_Datatype datatype, int *count)
{
    (void)status; (void)datatype;
    if (count) *count = 0;
    return MPI_SUCCESS;
}

/* Collective Communication - Barrier */

int MPI_Barrier(MPI_Comm comm)
{
    (void)comm;
    return MPI_SUCCESS;
}

int MPI_Ibarrier(MPI_Comm comm, MPI_Request *request)
{
    (void)comm;
    if (request) *request = MPI_REQUEST_NULL;
    return MPI_SUCCESS;
}

/* Collective Communication - Broadcast */

int MPI_Bcast(void *buffer, int count, MPI_Datatype datatype, int root,
              MPI_Comm comm)
{
    (void)buffer; (void)count; (void)datatype; (void)root; (void)comm;
    return MPI_SUCCESS;
}

int MPI_Ibcast(void *buffer, int count, MPI_Datatype datatype, int root,
               MPI_Comm comm, MPI_Request *request)
{
    (void)buffer; (void)count; (void)datatype; (void)root; (void)comm;
    if (request) *request = MPI_REQUEST_NULL;
    return MPI_SUCCESS;
}

/* Generate all remaining stubs with a macro */

#define STUB_COLLECTIVE(name, ...) \
    int name(__VA_ARGS__) { return MPI_SUCCESS; }

/* Gather operations */
STUB_COLLECTIVE(MPI_Gather, const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, int recvcount, MPI_Datatype recvtype,
                int root, MPI_Comm comm)
STUB_COLLECTIVE(MPI_Igather, const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, int recvcount, MPI_Datatype recvtype,
                int root, MPI_Comm comm, MPI_Request *request)
STUB_COLLECTIVE(MPI_Gatherv, const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, const int recvcounts[], const int displs[],
                MPI_Datatype recvtype, int root, MPI_Comm comm)
STUB_COLLECTIVE(MPI_Igatherv, const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, const int recvcounts[], const int displs[],
                MPI_Datatype recvtype, int root, MPI_Comm comm, MPI_Request *request)

/* Scatter operations */
STUB_COLLECTIVE(MPI_Scatter, const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, int recvcount, MPI_Datatype recvtype,
                int root, MPI_Comm comm)
STUB_COLLECTIVE(MPI_Iscatter, const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, int recvcount, MPI_Datatype recvtype,
                int root, MPI_Comm comm, MPI_Request *request)
STUB_COLLECTIVE(MPI_Scatterv, const void *sendbuf, const int sendcounts[], const int displs[],
                MPI_Datatype sendtype, void *recvbuf, int recvcount,
                MPI_Datatype recvtype, int root, MPI_Comm comm)
STUB_COLLECTIVE(MPI_Iscatterv, const void *sendbuf, const int sendcounts[], const int displs[],
                MPI_Datatype sendtype, void *recvbuf, int recvcount,
                MPI_Datatype recvtype, int root, MPI_Comm comm, MPI_Request *request)

/* Allgather operations */
STUB_COLLECTIVE(MPI_Allgather, const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm)
STUB_COLLECTIVE(MPI_Iallgather, const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, int recvcount, MPI_Datatype recvtype,
                MPI_Comm comm, MPI_Request *request)
STUB_COLLECTIVE(MPI_Allgatherv, const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, const int recvcounts[], const int displs[],
                MPI_Datatype recvtype, MPI_Comm comm)
STUB_COLLECTIVE(MPI_Iallgatherv, const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, const int recvcounts[], const int displs[],
                MPI_Datatype recvtype, MPI_Comm comm, MPI_Request *request)

/* Alltoall operations */
STUB_COLLECTIVE(MPI_Alltoall, const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm)
STUB_COLLECTIVE(MPI_Ialltoall, const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, int recvcount, MPI_Datatype recvtype,
                MPI_Comm comm, MPI_Request *request)
STUB_COLLECTIVE(MPI_Alltoallv, const void *sendbuf, const int sendcounts[],
                const int sdispls[], MPI_Datatype sendtype,
                void *recvbuf, const int recvcounts[], const int rdispls[],
                MPI_Datatype recvtype, MPI_Comm comm)
STUB_COLLECTIVE(MPI_Ialltoallv, const void *sendbuf, const int sendcounts[],
                const int sdispls[], MPI_Datatype sendtype,
                void *recvbuf, const int recvcounts[], const int rdispls[],
                MPI_Datatype recvtype, MPI_Comm comm, MPI_Request *request)
STUB_COLLECTIVE(MPI_Alltoallw, const void *sendbuf, const int sendcounts[],
                const int sdispls[], const MPI_Datatype sendtypes[],
                void *recvbuf, const int recvcounts[], const int rdispls[],
                const MPI_Datatype recvtypes[], MPI_Comm comm)
STUB_COLLECTIVE(MPI_Ialltoallw, const void *sendbuf, const int sendcounts[],
                const int sdispls[], const MPI_Datatype sendtypes[],
                void *recvbuf, const int recvcounts[], const int rdispls[],
                const MPI_Datatype recvtypes[], MPI_Comm comm, MPI_Request *request)

/* Reduce operations */
STUB_COLLECTIVE(MPI_Reduce, const void *sendbuf, void *recvbuf, int count,
                MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm)
STUB_COLLECTIVE(MPI_Ireduce, const void *sendbuf, void *recvbuf, int count,
                MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm,
                MPI_Request *request)
STUB_COLLECTIVE(MPI_Allreduce, const void *sendbuf, void *recvbuf, int count,
                MPI_Datatype datatype, MPI_Op op, MPI_Comm comm)
STUB_COLLECTIVE(MPI_Iallreduce, const void *sendbuf, void *recvbuf, int count,
                MPI_Datatype datatype, MPI_Op op, MPI_Comm comm, MPI_Request *request)
STUB_COLLECTIVE(MPI_Reduce_scatter, const void *sendbuf, void *recvbuf, const int recvcounts[],
                MPI_Datatype datatype, MPI_Op op, MPI_Comm comm)
STUB_COLLECTIVE(MPI_Ireduce_scatter, const void *sendbuf, void *recvbuf, const int recvcounts[],
                MPI_Datatype datatype, MPI_Op op, MPI_Comm comm, MPI_Request *request)
STUB_COLLECTIVE(MPI_Reduce_scatter_block, const void *sendbuf, void *recvbuf, int recvcount,
                MPI_Datatype datatype, MPI_Op op, MPI_Comm comm)
STUB_COLLECTIVE(MPI_Ireduce_scatter_block, const void *sendbuf, void *recvbuf, int recvcount,
                MPI_Datatype datatype, MPI_Op op, MPI_Comm comm, MPI_Request *request)

/* Scan operations */
STUB_COLLECTIVE(MPI_Scan, const void *sendbuf, void *recvbuf, int count,
                MPI_Datatype datatype, MPI_Op op, MPI_Comm comm)
STUB_COLLECTIVE(MPI_Iscan, const void *sendbuf, void *recvbuf, int count,
                MPI_Datatype datatype, MPI_Op op, MPI_Comm comm, MPI_Request *request)
STUB_COLLECTIVE(MPI_Exscan, const void *sendbuf, void *recvbuf, int count,
                MPI_Datatype datatype, MPI_Op op, MPI_Comm comm)
STUB_COLLECTIVE(MPI_Iexscan, const void *sendbuf, void *recvbuf, int count,
                MPI_Datatype datatype, MPI_Op op, MPI_Comm comm, MPI_Request *request)

/* Datatype operations */
STUB_COLLECTIVE(MPI_Type_contiguous, int count, MPI_Datatype oldtype, MPI_Datatype *newtype)
STUB_COLLECTIVE(MPI_Type_vector, int count, int blocklength, int stride,
                MPI_Datatype oldtype, MPI_Datatype *newtype)
STUB_COLLECTIVE(MPI_Type_hvector, int count, int blocklength, MPI_Aint stride,
                MPI_Datatype oldtype, MPI_Datatype *newtype)
STUB_COLLECTIVE(MPI_Type_indexed, int count, const int array_of_blocklengths[],
                const int array_of_displacements[],
                MPI_Datatype oldtype, MPI_Datatype *newtype)
STUB_COLLECTIVE(MPI_Type_hindexed, int count, const int array_of_blocklengths[],
                const MPI_Aint array_of_displacements[],
                MPI_Datatype oldtype, MPI_Datatype *newtype)
STUB_COLLECTIVE(MPI_Type_struct, int count, const int array_of_blocklengths[],
                const MPI_Aint array_of_displacements[],
                const MPI_Datatype array_of_types[], MPI_Datatype *newtype)
STUB_COLLECTIVE(MPI_Type_create_struct, int count, const int array_of_blocklengths[],
                const MPI_Aint array_of_displacements[],
                const MPI_Datatype array_of_types[], MPI_Datatype *newtype)
STUB_COLLECTIVE(MPI_Type_create_resized, MPI_Datatype oldtype, MPI_Aint lb, MPI_Aint extent,
                MPI_Datatype *newtype)
STUB_COLLECTIVE(MPI_Type_size, MPI_Datatype datatype, int *size)
STUB_COLLECTIVE(MPI_Type_commit, MPI_Datatype *datatype)
STUB_COLLECTIVE(MPI_Type_free, MPI_Datatype *datatype)
STUB_COLLECTIVE(MPI_Type_get_extent, MPI_Datatype datatype, MPI_Aint *lb, MPI_Aint *extent)
STUB_COLLECTIVE(MPI_Type_get_true_extent, MPI_Datatype datatype, MPI_Aint *true_lb,
                MPI_Aint *true_extent)

/* Pack/Unpack */
STUB_COLLECTIVE(MPI_Pack, const void *inbuf, int incount, MPI_Datatype datatype,
                void *outbuf, int outsize, int *position, MPI_Comm comm)
STUB_COLLECTIVE(MPI_Unpack, const void *inbuf, int insize, int *position, void *outbuf,
                int outcount, MPI_Datatype datatype, MPI_Comm comm)
STUB_COLLECTIVE(MPI_Pack_size, int incount, MPI_Datatype datatype, MPI_Comm comm, int *size)

/* RMA Window management */
STUB_COLLECTIVE(MPI_Win_create, void *base, MPI_Aint size, int disp_unit, MPI_Info info,
                MPI_Comm comm, MPI_Win *win)
STUB_COLLECTIVE(MPI_Win_allocate, MPI_Aint size, int disp_unit, MPI_Info info,
                MPI_Comm comm, void *baseptr, MPI_Win *win)
STUB_COLLECTIVE(MPI_Win_allocate_shared, MPI_Aint size, int disp_unit, MPI_Info info,
                MPI_Comm comm, void *baseptr, MPI_Win *win)
STUB_COLLECTIVE(MPI_Win_create_dynamic, MPI_Info info, MPI_Comm comm, MPI_Win *win)
STUB_COLLECTIVE(MPI_Win_attach, MPI_Win win, void *base, MPI_Aint size)
STUB_COLLECTIVE(MPI_Win_detach, MPI_Win win, const void *base)
STUB_COLLECTIVE(MPI_Win_free, MPI_Win *win)
STUB_COLLECTIVE(MPI_Win_get_group, MPI_Win win, MPI_Group *group)
STUB_COLLECTIVE(MPI_Win_shared_query, MPI_Win win, int rank, MPI_Aint *size, int *disp_unit,
                void *baseptr)

/* RMA operations */
STUB_COLLECTIVE(MPI_Put, const void *origin_addr, int origin_count, MPI_Datatype origin_datatype,
                int target_rank, MPI_Aint target_disp, int target_count,
                MPI_Datatype target_datatype, MPI_Win win)
STUB_COLLECTIVE(MPI_Get, void *origin_addr, int origin_count, MPI_Datatype origin_datatype,
                int target_rank, MPI_Aint target_disp, int target_count,
                MPI_Datatype target_datatype, MPI_Win win)
STUB_COLLECTIVE(MPI_Accumulate, const void *origin_addr, int origin_count,
                MPI_Datatype origin_datatype, int target_rank,
                MPI_Aint target_disp, int target_count,
                MPI_Datatype target_datatype, MPI_Op op, MPI_Win win)
STUB_COLLECTIVE(MPI_Get_accumulate, const void *origin_addr, int origin_count,
                MPI_Datatype origin_datatype, void *result_addr,
                int result_count, MPI_Datatype result_datatype,
                int target_rank, MPI_Aint target_disp, int target_count,
                MPI_Datatype target_datatype, MPI_Op op, MPI_Win win)
STUB_COLLECTIVE(MPI_Fetch_and_op, const void *origin_addr, void *result_addr,
                MPI_Datatype datatype, int target_rank,
                MPI_Aint target_disp, MPI_Op op, MPI_Win win)
STUB_COLLECTIVE(MPI_Compare_and_swap, const void *origin_addr, const void *compare_addr,
                void *result_addr, MPI_Datatype datatype,
                int target_rank, MPI_Aint target_disp, MPI_Win win)
STUB_COLLECTIVE(MPI_Rput, const void *origin_addr, int origin_count,
                MPI_Datatype origin_datatype, int target_rank,
                MPI_Aint target_disp, int target_count,
                MPI_Datatype target_datatype, MPI_Win win, MPI_Request *request)
STUB_COLLECTIVE(MPI_Rget, void *origin_addr, int origin_count, MPI_Datatype origin_datatype,
                int target_rank, MPI_Aint target_disp, int target_count,
                MPI_Datatype target_datatype, MPI_Win win, MPI_Request *request)
STUB_COLLECTIVE(MPI_Raccumulate, const void *origin_addr, int origin_count,
                MPI_Datatype origin_datatype, int target_rank,
                MPI_Aint target_disp, int target_count,
                MPI_Datatype target_datatype, MPI_Op op, MPI_Win win, MPI_Request *request)
STUB_COLLECTIVE(MPI_Rget_accumulate, const void *origin_addr, int origin_count,
                MPI_Datatype origin_datatype, void *result_addr,
                int result_count, MPI_Datatype result_datatype,
                int target_rank, MPI_Aint target_disp,
                int target_count, MPI_Datatype target_datatype,
                MPI_Op op, MPI_Win win, MPI_Request *request)

/* RMA synchronization */
STUB_COLLECTIVE(MPI_Win_fence, int assert, MPI_Win win)
STUB_COLLECTIVE(MPI_Win_start, MPI_Group group, int assert, MPI_Win win)
STUB_COLLECTIVE(MPI_Win_complete, MPI_Win win)
STUB_COLLECTIVE(MPI_Win_post, MPI_Group group, int assert, MPI_Win win)
STUB_COLLECTIVE(MPI_Win_wait, MPI_Win win)
STUB_COLLECTIVE(MPI_Win_test, MPI_Win win, int *flag)
STUB_COLLECTIVE(MPI_Win_lock, int lock_type, int rank, int assert, MPI_Win win)
STUB_COLLECTIVE(MPI_Win_unlock, int rank, MPI_Win win)
STUB_COLLECTIVE(MPI_Win_lock_all, int assert, MPI_Win win)
STUB_COLLECTIVE(MPI_Win_unlock_all, MPI_Win win)
STUB_COLLECTIVE(MPI_Win_flush, int rank, MPI_Win win)
STUB_COLLECTIVE(MPI_Win_flush_all, MPI_Win win)
STUB_COLLECTIVE(MPI_Win_flush_local, int rank, MPI_Win win)
STUB_COLLECTIVE(MPI_Win_flush_local_all, MPI_Win win)
STUB_COLLECTIVE(MPI_Win_sync, MPI_Win win)

/* Topology */
STUB_COLLECTIVE(MPI_Cart_create, MPI_Comm comm_old, int ndims, const int dims[],
                const int periods[], int reorder, MPI_Comm *comm_cart)
STUB_COLLECTIVE(MPI_Cart_coords, MPI_Comm comm, int rank, int maxdims, int coords[])
STUB_COLLECTIVE(MPI_Cart_get, MPI_Comm comm, int maxdims, int dims[], int periods[],
                int coords[])
STUB_COLLECTIVE(MPI_Cart_rank, MPI_Comm comm, const int coords[], int *rank)
STUB_COLLECTIVE(MPI_Cart_shift, MPI_Comm comm, int direction, int disp, int *rank_source,
                int *rank_dest)
STUB_COLLECTIVE(MPI_Cart_sub, MPI_Comm comm, const int remain_dims[], MPI_Comm *newcomm)
STUB_COLLECTIVE(MPI_Dims_create, int nnodes, int ndims, int dims[])
STUB_COLLECTIVE(MPI_Cartdim_get, MPI_Comm comm, int *ndims)

/* Graph topology */
STUB_COLLECTIVE(MPI_Graph_create, MPI_Comm comm_old, int nnodes, const int index[],
                const int edges[], int reorder, MPI_Comm *comm_graph)
STUB_COLLECTIVE(MPI_Graph_get, MPI_Comm comm, int maxindex, int maxedges, int index[],
                int edges[])
STUB_COLLECTIVE(MPI_Graph_map, MPI_Comm comm, int nnodes, const int index[], const int edges[],
                int *newrank)
STUB_COLLECTIVE(MPI_Graph_neighbors_count, MPI_Comm comm, int rank, int *nneighbors)
STUB_COLLECTIVE(MPI_Graph_neighbors, MPI_Comm comm, int rank, int maxneighbors,
                int neighbors[])
STUB_COLLECTIVE(MPI_Graphdims_get, MPI_Comm comm, int *nnodes, int *nedges)

/* Distributed graph topology */
STUB_COLLECTIVE(MPI_Dist_graph_create, MPI_Comm comm_old, int n, const int sources[],
                const int degrees[], const int destinations[],
                const int weights[], MPI_Info info, int reorder,
                MPI_Comm *comm_dist_graph)
STUB_COLLECTIVE(MPI_Dist_graph_create_adjacent, MPI_Comm comm_old, int indegree,
                const int sources[], const int sourceweights[],
                int outdegree, const int destinations[],
                const int destweights[], MPI_Info info,
                int reorder, MPI_Comm *comm_dist_graph)
STUB_COLLECTIVE(MPI_Dist_graph_neighbors_count, MPI_Comm comm, int *indegree, int *outdegree,
                int *weighted)
STUB_COLLECTIVE(MPI_Dist_graph_neighbors, MPI_Comm comm, int maxindegree, int sources[],
                int sourceweights[], int maxoutdegree,
                int destinations[], int destweights[])

/* Neighborhood collectives */
STUB_COLLECTIVE(MPI_Neighbor_allgather, const void *sendbuf, int sendcount,
                MPI_Datatype sendtype, void *recvbuf,
                int recvcount, MPI_Datatype recvtype, MPI_Comm comm)
STUB_COLLECTIVE(MPI_Ineighbor_allgather, const void *sendbuf, int sendcount,
                MPI_Datatype sendtype, void *recvbuf,
                int recvcount, MPI_Datatype recvtype,
                MPI_Comm comm, MPI_Request *request)
STUB_COLLECTIVE(MPI_Neighbor_allgatherv, const void *sendbuf, int sendcount,
                MPI_Datatype sendtype, void *recvbuf,
                const int recvcounts[], const int displs[],
                MPI_Datatype recvtype, MPI_Comm comm)
STUB_COLLECTIVE(MPI_Ineighbor_allgatherv, const void *sendbuf, int sendcount,
                MPI_Datatype sendtype, void *recvbuf,
                const int recvcounts[], const int displs[],
                MPI_Datatype recvtype, MPI_Comm comm, MPI_Request *request)
STUB_COLLECTIVE(MPI_Neighbor_alltoall, const void *sendbuf, int sendcount,
                MPI_Datatype sendtype, void *recvbuf,
                int recvcount, MPI_Datatype recvtype, MPI_Comm comm)
STUB_COLLECTIVE(MPI_Ineighbor_alltoall, const void *sendbuf, int sendcount,
                MPI_Datatype sendtype, void *recvbuf,
                int recvcount, MPI_Datatype recvtype,
                MPI_Comm comm, MPI_Request *request)
STUB_COLLECTIVE(MPI_Neighbor_alltoallv, const void *sendbuf, const int sendcounts[],
                const int sdispls[], MPI_Datatype sendtype,
                void *recvbuf, const int recvcounts[],
                const int rdispls[], MPI_Datatype recvtype, MPI_Comm comm)
STUB_COLLECTIVE(MPI_Ineighbor_alltoallv, const void *sendbuf, const int sendcounts[],
                const int sdispls[], MPI_Datatype sendtype,
                void *recvbuf, const int recvcounts[],
                const int rdispls[], MPI_Datatype recvtype,
                MPI_Comm comm, MPI_Request *request)
STUB_COLLECTIVE(MPI_Neighbor_alltoallw, const void *sendbuf, const int sendcounts[],
                const MPI_Aint sdispls[],
                const MPI_Datatype sendtypes[], void *recvbuf,
                const int recvcounts[], const MPI_Aint rdispls[],
                const MPI_Datatype recvtypes[], MPI_Comm comm)
STUB_COLLECTIVE(MPI_Ineighbor_alltoallw, const void *sendbuf, const int sendcounts[],
                const MPI_Aint sdispls[],
                const MPI_Datatype sendtypes[], void *recvbuf,
                const int recvcounts[], const MPI_Aint rdispls[],
                const MPI_Datatype recvtypes[], MPI_Comm comm, MPI_Request *request)

/* Info objects */
STUB_COLLECTIVE(MPI_Info_create, MPI_Info *info)
STUB_COLLECTIVE(MPI_Info_set, MPI_Info info, const char *key, const char *value)
STUB_COLLECTIVE(MPI_Info_get, MPI_Info info, const char *key, int valuelen, char *value,
                int *flag)
STUB_COLLECTIVE(MPI_Info_delete, MPI_Info info, const char *key)
STUB_COLLECTIVE(MPI_Info_get_nkeys, MPI_Info info, int *nkeys)
STUB_COLLECTIVE(MPI_Info_get_nthkey, MPI_Info info, int n, char *key)
STUB_COLLECTIVE(MPI_Info_get_valuelen, MPI_Info info, const char *key, int *valuelen,
                int *flag)
STUB_COLLECTIVE(MPI_Info_dup, MPI_Info info, MPI_Info *newinfo)
STUB_COLLECTIVE(MPI_Info_free, MPI_Info *info)

/* Error handling */
STUB_COLLECTIVE(MPI_Errhandler_create, MPI_Handler_function *function,
                MPI_Errhandler *errhandler)
STUB_COLLECTIVE(MPI_Errhandler_set, MPI_Comm comm, MPI_Errhandler errhandler)
STUB_COLLECTIVE(MPI_Errhandler_get, MPI_Comm comm, MPI_Errhandler *errhandler)
STUB_COLLECTIVE(MPI_Errhandler_free, MPI_Errhandler *errhandler)
STUB_COLLECTIVE(MPI_Error_string, int errorcode, char *string, int *resultlen)
STUB_COLLECTIVE(MPI_Error_class, int errorcode, int *errorclass)

/* Environment */
int MPI_Get_processor_name(char *name, int *resultlen)
{
    if (name) strcpy(name, "localhost");
    if (resultlen) *resultlen = 9;
    return MPI_SUCCESS;
}

int MPI_Get_version(int *version, int *subversion)
{
    if (version) *version = MPI_VERSION;
    if (subversion) *subversion = MPI_SUBVERSION;
    return MPI_SUCCESS;
}

int MPI_Get_library_version(char *version, int *resultlen)
{
    if (version) strcpy(version, "TPBench MPI Stub 3.1");
    if (resultlen) *resultlen = 20;
    return MPI_SUCCESS;
}

/* Attribute caching */
STUB_COLLECTIVE(MPI_Keyval_create, MPI_Copy_function *copy_fn, MPI_Delete_function *delete_fn,
                int *keyval, void *extra_state)
STUB_COLLECTIVE(MPI_Keyval_free, int *keyval)
STUB_COLLECTIVE(MPI_Attr_put, MPI_Comm comm, int keyval, void *attribute_val)
STUB_COLLECTIVE(MPI_Attr_get, MPI_Comm comm, int keyval, void *attribute_val, int *flag)
STUB_COLLECTIVE(MPI_Attr_delete, MPI_Comm comm, int keyval)

/* Buffer management */
STUB_COLLECTIVE(MPI_Buffer_attach, void *buffer, int size)
STUB_COLLECTIVE(MPI_Buffer_detach, void *buffer, int *size)

/* Address manipulation */
STUB_COLLECTIVE(MPI_Get_address, const void *location, MPI_Aint *address)
STUB_COLLECTIVE(MPI_Address, void *location, MPI_Aint *address)

/* Thread support */
STUB_COLLECTIVE(MPI_Query_thread, int *provided)
STUB_COLLECTIVE(MPI_Is_thread_main, int *flag)

/* Dynamic process management */
STUB_COLLECTIVE(MPI_Comm_spawn, const char *command, char *argv[], int maxprocs,
                MPI_Info info, int root, MPI_Comm comm, MPI_Comm *intercomm,
                int array_of_errcodes[])
STUB_COLLECTIVE(MPI_Comm_spawn_multiple, int count, char *array_of_commands[],
                char **array_of_argv[], const int array_of_maxprocs[],
                const MPI_Info array_of_info[], int root,
                MPI_Comm comm, MPI_Comm *intercomm, int array_of_errcodes[])
STUB_COLLECTIVE(MPI_Comm_get_parent, MPI_Comm *parent)
STUB_COLLECTIVE(MPI_Comm_connect, const char *port_name, MPI_Info info, int root,
                MPI_Comm comm, MPI_Comm *newcomm)
STUB_COLLECTIVE(MPI_Comm_accept, const char *port_name, MPI_Info info, int root,
                MPI_Comm comm, MPI_Comm *newcomm)
STUB_COLLECTIVE(MPI_Comm_disconnect, MPI_Comm *comm)
STUB_COLLECTIVE(MPI_Open_port, MPI_Info info, char *port_name)
STUB_COLLECTIVE(MPI_Close_port, const char *port_name)
STUB_COLLECTIVE(MPI_Publish_name, const char *service_name, MPI_Info info,
                const char *port_name)
STUB_COLLECTIVE(MPI_Unpublish_name, const char *service_name, MPI_Info info,
                const char *port_name)
STUB_COLLECTIVE(MPI_Lookup_name, const char *service_name, MPI_Info info, char *port_name)

/* MPI I/O */
STUB_COLLECTIVE(MPI_File_open, MPI_Comm comm, const char *filename, int amode,
                MPI_Info info, MPI_File *fh)
STUB_COLLECTIVE(MPI_File_close, MPI_File *fh)
STUB_COLLECTIVE(MPI_File_delete, const char *filename, MPI_Info info)
STUB_COLLECTIVE(MPI_File_set_size, MPI_File fh, MPI_Offset size)
STUB_COLLECTIVE(MPI_File_preallocate, MPI_File fh, MPI_Offset size)
STUB_COLLECTIVE(MPI_File_get_size, MPI_File fh, MPI_Offset *size)
STUB_COLLECTIVE(MPI_File_get_group, MPI_File fh, MPI_Group *group)
STUB_COLLECTIVE(MPI_File_get_amode, MPI_File fh, int *amode)
STUB_COLLECTIVE(MPI_File_set_info, MPI_File fh, MPI_Info info)
STUB_COLLECTIVE(MPI_File_get_info, MPI_File fh, MPI_Info *info_used)
STUB_COLLECTIVE(MPI_File_set_view, MPI_File fh, MPI_Offset disp, MPI_Datatype etype,
                MPI_Datatype filetype, const char *datarep, MPI_Info info)
STUB_COLLECTIVE(MPI_File_get_view, MPI_File fh, MPI_Offset *disp, MPI_Datatype *etype,
                MPI_Datatype *filetype, char *datarep)
STUB_COLLECTIVE(MPI_File_read_at, MPI_File fh, MPI_Offset offset, void *buf, int count,
                MPI_Datatype datatype, MPI_Status *status)
STUB_COLLECTIVE(MPI_File_read_at_all, MPI_File fh, MPI_Offset offset, void *buf, int count,
                MPI_Datatype datatype, MPI_Status *status)
STUB_COLLECTIVE(MPI_File_write_at, MPI_File fh, MPI_Offset offset, const void *buf,
                int count, MPI_Datatype datatype, MPI_Status *status)
STUB_COLLECTIVE(MPI_File_write_at_all, MPI_File fh, MPI_Offset offset, const void *buf,
                int count, MPI_Datatype datatype, MPI_Status *status)
STUB_COLLECTIVE(MPI_File_iread_at, MPI_File fh, MPI_Offset offset, void *buf, int count,
                MPI_Datatype datatype, MPI_Request *request)
STUB_COLLECTIVE(MPI_File_iwrite_at, MPI_File fh, MPI_Offset offset, const void *buf,
                int count, MPI_Datatype datatype, MPI_Request *request)
STUB_COLLECTIVE(MPI_File_read, MPI_File fh, void *buf, int count, MPI_Datatype datatype,
                MPI_Status *status)
STUB_COLLECTIVE(MPI_File_read_all, MPI_File fh, void *buf, int count, MPI_Datatype datatype,
                MPI_Status *status)
STUB_COLLECTIVE(MPI_File_write, MPI_File fh, const void *buf, int count,
                MPI_Datatype datatype, MPI_Status *status)
STUB_COLLECTIVE(MPI_File_write_all, MPI_File fh, const void *buf, int count,
                MPI_Datatype datatype, MPI_Status *status)
STUB_COLLECTIVE(MPI_File_iread, MPI_File fh, void *buf, int count, MPI_Datatype datatype,
                MPI_Request *request)
STUB_COLLECTIVE(MPI_File_iwrite, MPI_File fh, const void *buf, int count,
                MPI_Datatype datatype, MPI_Request *request)
STUB_COLLECTIVE(MPI_File_seek, MPI_File fh, MPI_Offset offset, int whence)
STUB_COLLECTIVE(MPI_File_get_position, MPI_File fh, MPI_Offset *offset)
STUB_COLLECTIVE(MPI_File_get_byte_offset, MPI_File fh, MPI_Offset offset,
                MPI_Offset *disp)
STUB_COLLECTIVE(MPI_File_sync, MPI_File fh)

/* Custom operations */
STUB_COLLECTIVE(MPI_Op_create, MPI_User_function *function, int commute, MPI_Op *op)
STUB_COLLECTIVE(MPI_Op_free, MPI_Op *op)

/* Timing functions */

double MPI_Wtime(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec * 1.0e-6;
}

double MPI_Wtick(void)
{
    return 1.0e-6;  /* 1 microsecond resolution */
}
