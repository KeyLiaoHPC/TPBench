#include <stddef.h>
#ifndef TPB_MPI_STUB_H
#define TPB_MPI_STUB_H

#ifdef __cplusplus
extern "C" {
#endif

/* MPI Version Information */
#define MPI_VERSION    3
#define MPI_SUBVERSION 1

/* Basic MPI Types */
typedef int MPI_Comm;
typedef int MPI_Datatype;
typedef int MPI_Op;
typedef int MPI_Group;
typedef int MPI_Win;
typedef int MPI_File;
typedef int MPI_Info;
typedef int MPI_Errhandler;
typedef long MPI_Aint;
typedef long MPI_Offset;
typedef int MPI_Request;
typedef int MPI_Message;

/* MPI Status Structure */
typedef struct MPI_Status {
    int MPI_SOURCE;
    int MPI_TAG;
    int MPI_ERROR;
    int _cancelled;
    size_t _ucount;
} MPI_Status;

/* MPI Constants - Communicators */
#define MPI_COMM_WORLD  ((MPI_Comm)0x44000000)
#define MPI_COMM_SELF   ((MPI_Comm)0x44000001)
#define MPI_COMM_NULL   ((MPI_Comm)0x04000000)

/* MPI Constants - Groups */
#define MPI_GROUP_EMPTY ((MPI_Group)0x48000000)
#define MPI_GROUP_NULL  ((MPI_Group)0x08000000)

/* MPI Constants - Datatypes */
#define MPI_CHAR           ((MPI_Datatype)0x4c000101)
#define MPI_SHORT          ((MPI_Datatype)0x4c000201)
#define MPI_INT            ((MPI_Datatype)0x4c000405)
#define MPI_LONG           ((MPI_Datatype)0x4c000807)
#define MPI_LONG_LONG_INT  ((MPI_Datatype)0x4c000809)
#define MPI_LONG_LONG      ((MPI_Datatype)0x4c000809)
#define MPI_SIGNED_CHAR    ((MPI_Datatype)0x4c000118)
#define MPI_UNSIGNED_CHAR  ((MPI_Datatype)0x4c000102)
#define MPI_UNSIGNED_SHORT ((MPI_Datatype)0x4c000202)
#define MPI_UNSIGNED       ((MPI_Datatype)0x4c000406)
#define MPI_UNSIGNED_INT   ((MPI_Datatype)0x4c000406)
#define MPI_UNSIGNED_LONG  ((MPI_Datatype)0x4c000808)
#define MPI_UNSIGNED_LONG_LONG ((MPI_Datatype)0x4c000819)
#define MPI_FLOAT          ((MPI_Datatype)0x4c00040a)
#define MPI_DOUBLE         ((MPI_Datatype)0x4c00080b)
#define MPI_LONG_DOUBLE    ((MPI_Datatype)0x4c00100c)
#define MPI_BYTE           ((MPI_Datatype)0x4c00010d)
#define MPI_WCHAR          ((MPI_Datatype)0x4c00040e)
#define MPI_PACKED         ((MPI_Datatype)0x4c00010f)

/* C99 Integer Types */
#define MPI_INT8_T         ((MPI_Datatype)0x4c000137)
#define MPI_INT16_T        ((MPI_Datatype)0x4c000238)
#define MPI_INT32_T        ((MPI_Datatype)0x4c000439)
#define MPI_INT64_T        ((MPI_Datatype)0x4c00083a)
#define MPI_UINT8_T        ((MPI_Datatype)0x4c00013b)
#define MPI_UINT16_T       ((MPI_Datatype)0x4c00023c)
#define MPI_UINT32_T       ((MPI_Datatype)0x4c00043d)
#define MPI_UINT64_T       ((MPI_Datatype)0x4c00083e)

/* C99 Complex Types */
#define MPI_C_COMPLEX             ((MPI_Datatype)0x4c000823)
#define MPI_C_FLOAT_COMPLEX       ((MPI_Datatype)0x4c000823)
#define MPI_C_DOUBLE_COMPLEX      ((MPI_Datatype)0x4c001024)
#define MPI_C_LONG_DOUBLE_COMPLEX ((MPI_Datatype)0x4c002025)

/* MPI Constants - Operations */
#define MPI_MAX     ((MPI_Op)0x58000001)
#define MPI_MIN     ((MPI_Op)0x58000002)
#define MPI_SUM     ((MPI_Op)0x58000003)
#define MPI_PROD    ((MPI_Op)0x58000004)
#define MPI_LAND    ((MPI_Op)0x58000005)
#define MPI_BAND    ((MPI_Op)0x58000006)
#define MPI_LOR     ((MPI_Op)0x58000007)
#define MPI_BOR     ((MPI_Op)0x58000008)
#define MPI_LXOR    ((MPI_Op)0x58000009)
#define MPI_BXOR    ((MPI_Op)0x5800000a)
#define MPI_MINLOC  ((MPI_Op)0x5800000b)
#define MPI_MAXLOC  ((MPI_Op)0x5800000c)
#define MPI_REPLACE ((MPI_Op)0x5800000d)
#define MPI_NO_OP   ((MPI_Op)0x5800000e)

/* MPI Constants - Return Codes */
#define MPI_SUCCESS          0
#define MPI_ERR_BUFFER       1
#define MPI_ERR_COUNT        2
#define MPI_ERR_TYPE         3
#define MPI_ERR_TAG          4
#define MPI_ERR_COMM         5
#define MPI_ERR_RANK         6
#define MPI_ERR_REQUEST      7
#define MPI_ERR_ROOT         8
#define MPI_ERR_GROUP        9
#define MPI_ERR_OP           10
#define MPI_ERR_TOPOLOGY     11
#define MPI_ERR_DIMS         12
#define MPI_ERR_ARG          13
#define MPI_ERR_UNKNOWN      14
#define MPI_ERR_TRUNCATE     15
#define MPI_ERR_OTHER        16
#define MPI_ERR_INTERN       17
#define MPI_ERR_IN_STATUS    18
#define MPI_ERR_PENDING      19
#define MPI_ERR_ACCESS       20
#define MPI_ERR_AMODE        21
#define MPI_ERR_ASSERT       22
#define MPI_ERR_BAD_FILE     23
#define MPI_ERR_BASE         24
#define MPI_ERR_CONVERSION   25
#define MPI_ERR_DISP         26
#define MPI_ERR_DUP_DATAREP  27
#define MPI_ERR_FILE_EXISTS  28
#define MPI_ERR_FILE_IN_USE  29
#define MPI_ERR_FILE         30
#define MPI_ERR_INFO_KEY     31
#define MPI_ERR_INFO_NOKEY   32
#define MPI_ERR_INFO_VALUE   33
#define MPI_ERR_INFO         34
#define MPI_ERR_IO           35
#define MPI_ERR_KEYVAL       36
#define MPI_ERR_LOCKTYPE     37
#define MPI_ERR_NAME         38
#define MPI_ERR_NO_MEM       39
#define MPI_ERR_NOT_SAME     40
#define MPI_ERR_NO_SPACE     41
#define MPI_ERR_NO_SUCH_FILE 42
#define MPI_ERR_PORT         43
#define MPI_ERR_QUOTA        44
#define MPI_ERR_READ_ONLY    45
#define MPI_ERR_RMA_CONFLICT 46
#define MPI_ERR_RMA_SYNC     47
#define MPI_ERR_SERVICE      48
#define MPI_ERR_SIZE         49
#define MPI_ERR_SPAWN        50
#define MPI_ERR_UNSUPPORTED_DATAREP 51
#define MPI_ERR_UNSUPPORTED_OPERATION 52
#define MPI_ERR_WIN          53
#define MPI_ERR_LASTCODE     54

/* MPI Constants - Special Values */
#define MPI_UNDEFINED      (-32766)
#define MPI_ANY_SOURCE     (-2)
#define MPI_ANY_TAG        (-1)
#define MPI_PROC_NULL      (-1)
#define MPI_ROOT           (-3)

#define MPI_REQUEST_NULL   ((MPI_Request)0x2c000000)
#define MPI_MESSAGE_NULL   ((MPI_Message)0x2c000000)
#define MPI_MESSAGE_NO_PROC ((MPI_Message)0x6c000000)
#define MPI_OP_NULL        ((MPI_Op)0x18000000)
#define MPI_ERRHANDLER_NULL ((MPI_Errhandler)0x14000000)
#define MPI_INFO_NULL      ((MPI_Info)0x1c000000)
#define MPI_WIN_NULL       ((MPI_Win)0x20000000)
#define MPI_FILE_NULL      ((MPI_File)0)

/* MPI Constants - Status */
#define MPI_STATUS_IGNORE   ((MPI_Status *)1)
#define MPI_STATUSES_IGNORE ((MPI_Status *)1)

/* MPI Constants - Info */
#define MPI_INFO_ENV       ((MPI_Info)0x5c000001)

/* MPI Constants - Thread Support Levels */
#define MPI_THREAD_SINGLE     0
#define MPI_THREAD_FUNNELED   1
#define MPI_THREAD_SERIALIZED 2
#define MPI_THREAD_MULTIPLE   3

/* MPI Constants - Miscellaneous */
#define MPI_MAX_PROCESSOR_NAME    256
#define MPI_MAX_ERROR_STRING      512
#define MPI_MAX_OBJECT_NAME       128
#define MPI_MAX_INFO_KEY          255
#define MPI_MAX_INFO_VAL          1024
#define MPI_MAX_PORT_NAME         1024

/* MPI-3 Neighborhood Collectives */
#define MPI_UNWEIGHTED            ((int *)2)
#define MPI_WEIGHTS_EMPTY         ((int *)3)

/* MPI Constants - In Place */
#define MPI_IN_PLACE  ((void *)1)

/* MPI Constants - Bottom */
#define MPI_BOTTOM    ((void *)0)

/* MPI-3 Constants for RMA */
#define MPI_WIN_BASE      0
#define MPI_WIN_SIZE      1
#define MPI_WIN_DISP_UNIT 2
#define MPI_WIN_CREATE_FLAVOR 3
#define MPI_WIN_MODEL     4

/* Window Flavors */
#define MPI_WIN_FLAVOR_CREATE    1
#define MPI_WIN_FLAVOR_ALLOCATE  2
#define MPI_WIN_FLAVOR_DYNAMIC   3
#define MPI_WIN_FLAVOR_SHARED    4

/* Window Memory Models */
#define MPI_WIN_SEPARATE   1
#define MPI_WIN_UNIFIED    2

/* Lock Types for RMA */
#define MPI_LOCK_EXCLUSIVE 234
#define MPI_LOCK_SHARED    235

/* RMA Assertion Values */
#define MPI_MODE_NOCHECK    1024
#define MPI_MODE_NOSTORE    2048
#define MPI_MODE_NOPUT      4096
#define MPI_MODE_NOPRECEDE  8192
#define MPI_MODE_NOSUCCEED 16384

/* ================ MPI-3 Function Declarations ================ */

/* Initialization and Finalization */
int MPI_Init(int *argc, char ***argv);
int MPI_Init_thread(int *argc, char ***argv, int required, int *provided);
int MPI_Finalize(void);
int MPI_Finalized(int *flag);
int MPI_Initialized(int *flag);
int MPI_Abort(MPI_Comm comm, int errorcode);

/* Communicator Management */
int MPI_Comm_size(MPI_Comm comm, int *size);
int MPI_Comm_rank(MPI_Comm comm, int *rank);
int MPI_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm);
int MPI_Comm_create(MPI_Comm comm, MPI_Group group, MPI_Comm *newcomm);
int MPI_Comm_split(MPI_Comm comm, int color, int key, MPI_Comm *newcomm);
int MPI_Comm_free(MPI_Comm *comm);
int MPI_Comm_compare(MPI_Comm comm1, MPI_Comm comm2, int *result);
int MPI_Comm_group(MPI_Comm comm, MPI_Group *group);
int MPI_Comm_test_inter(MPI_Comm comm, int *flag);
int MPI_Comm_remote_size(MPI_Comm comm, int *size);
int MPI_Comm_remote_group(MPI_Comm comm, MPI_Group *group);

/* Group Management */
int MPI_Group_size(MPI_Group group, int *size);
int MPI_Group_rank(MPI_Group group, int *rank);
int MPI_Group_translate_ranks(MPI_Group group1, int n, const int ranks1[],
                              MPI_Group group2, int ranks2[]);
int MPI_Group_compare(MPI_Group group1, MPI_Group group2, int *result);
int MPI_Group_union(MPI_Group group1, MPI_Group group2, MPI_Group *newgroup);
int MPI_Group_intersection(MPI_Group group1, MPI_Group group2, MPI_Group *newgroup);
int MPI_Group_difference(MPI_Group group1, MPI_Group group2, MPI_Group *newgroup);
int MPI_Group_incl(MPI_Group group, int n, const int ranks[], MPI_Group *newgroup);
int MPI_Group_excl(MPI_Group group, int n, const int ranks[], MPI_Group *newgroup);
int MPI_Group_range_incl(MPI_Group group, int n, int ranges[][3], MPI_Group *newgroup);
int MPI_Group_range_excl(MPI_Group group, int n, int ranges[][3], MPI_Group *newgroup);
int MPI_Group_free(MPI_Group *group);

/* Point-to-Point Communication - Blocking */
int MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest,
             int tag, MPI_Comm comm);
int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source,
             int tag, MPI_Comm comm, MPI_Status *status);
int MPI_Sendrecv(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                 int dest, int sendtag, void *recvbuf, int recvcount,
                 MPI_Datatype recvtype, int source, int recvtag,
                 MPI_Comm comm, MPI_Status *status);
int MPI_Sendrecv_replace(void *buf, int count, MPI_Datatype datatype,
                         int dest, int sendtag, int source, int recvtag,
                         MPI_Comm comm, MPI_Status *status);

/* Point-to-Point Communication - Send Modes */
int MPI_Bsend(const void *buf, int count, MPI_Datatype datatype, int dest,
              int tag, MPI_Comm comm);
int MPI_Ssend(const void *buf, int count, MPI_Datatype datatype, int dest,
              int tag, MPI_Comm comm);
int MPI_Rsend(const void *buf, int count, MPI_Datatype datatype, int dest,
              int tag, MPI_Comm comm);

/* Point-to-Point Communication - Non-blocking */
int MPI_Isend(const void *buf, int count, MPI_Datatype datatype, int dest,
              int tag, MPI_Comm comm, MPI_Request *request);
int MPI_Irecv(void *buf, int count, MPI_Datatype datatype, int source,
              int tag, MPI_Comm comm, MPI_Request *request);
int MPI_Ibsend(const void *buf, int count, MPI_Datatype datatype, int dest,
               int tag, MPI_Comm comm, MPI_Request *request);
int MPI_Issend(const void *buf, int count, MPI_Datatype datatype, int dest,
               int tag, MPI_Comm comm, MPI_Request *request);
int MPI_Irsend(const void *buf, int count, MPI_Datatype datatype, int dest,
               int tag, MPI_Comm comm, MPI_Request *request);

/* Request Management */
int MPI_Wait(MPI_Request *request, MPI_Status *status);
int MPI_Test(MPI_Request *request, int *flag, MPI_Status *status);
int MPI_Waitany(int count, MPI_Request array_of_requests[], int *index,
                MPI_Status *status);
int MPI_Testany(int count, MPI_Request array_of_requests[], int *index,
                int *flag, MPI_Status *status);
int MPI_Waitall(int count, MPI_Request array_of_requests[],
                MPI_Status array_of_statuses[]);
int MPI_Testall(int count, MPI_Request array_of_requests[], int *flag,
                MPI_Status array_of_statuses[]);
int MPI_Waitsome(int incount, MPI_Request array_of_requests[],
                 int *outcount, int array_of_indices[],
                 MPI_Status array_of_statuses[]);
int MPI_Testsome(int incount, MPI_Request array_of_requests[],
                 int *outcount, int array_of_indices[],
                 MPI_Status array_of_statuses[]);
int MPI_Request_free(MPI_Request *request);
int MPI_Cancel(MPI_Request *request);
int MPI_Request_get_status(MPI_Request request, int *flag, MPI_Status *status);

/* Probe and Message Management */
int MPI_Probe(int source, int tag, MPI_Comm comm, MPI_Status *status);
int MPI_Iprobe(int source, int tag, MPI_Comm comm, int *flag, MPI_Status *status);

/* MPI-3 Matched Probe */
int MPI_Mprobe(int source, int tag, MPI_Comm comm, MPI_Message *message,
               MPI_Status *status);
int MPI_Improbe(int source, int tag, MPI_Comm comm, int *flag,
                MPI_Message *message, MPI_Status *status);
int MPI_Mrecv(void *buf, int count, MPI_Datatype datatype,
              MPI_Message *message, MPI_Status *status);
int MPI_Imrecv(void *buf, int count, MPI_Datatype datatype,
               MPI_Message *message, MPI_Request *request);

/* Status Management */
int MPI_Get_count(const MPI_Status *status, MPI_Datatype datatype, int *count);
int MPI_Test_cancelled(const MPI_Status *status, int *flag);
int MPI_Status_set_cancelled(MPI_Status *status, int flag);
int MPI_Status_set_elements(MPI_Status *status, MPI_Datatype datatype, int count);
int MPI_Get_elements(const MPI_Status *status, MPI_Datatype datatype, int *count);

/* Collective Communication - Barrier */
int MPI_Barrier(MPI_Comm comm);
int MPI_Ibarrier(MPI_Comm comm, MPI_Request *request);

/* Collective Communication - Broadcast */
int MPI_Bcast(void *buffer, int count, MPI_Datatype datatype, int root,
              MPI_Comm comm);
int MPI_Ibcast(void *buffer, int count, MPI_Datatype datatype, int root,
               MPI_Comm comm, MPI_Request *request);

/* Collective Communication - Gather */
int MPI_Gather(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
               void *recvbuf, int recvcount, MPI_Datatype recvtype,
               int root, MPI_Comm comm);
int MPI_Igather(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, int recvcount, MPI_Datatype recvtype,
                int root, MPI_Comm comm, MPI_Request *request);
int MPI_Gatherv(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, const int recvcounts[], const int displs[],
                MPI_Datatype recvtype, int root, MPI_Comm comm);
int MPI_Igatherv(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                 void *recvbuf, const int recvcounts[], const int displs[],
                 MPI_Datatype recvtype, int root, MPI_Comm comm,
                 MPI_Request *request);

/* Collective Communication - Scatter */
int MPI_Scatter(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, int recvcount, MPI_Datatype recvtype,
                int root, MPI_Comm comm);
int MPI_Iscatter(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                 void *recvbuf, int recvcount, MPI_Datatype recvtype,
                 int root, MPI_Comm comm, MPI_Request *request);
int MPI_Scatterv(const void *sendbuf, const int sendcounts[], const int displs[],
                 MPI_Datatype sendtype, void *recvbuf, int recvcount,
                 MPI_Datatype recvtype, int root, MPI_Comm comm);
int MPI_Iscatterv(const void *sendbuf, const int sendcounts[], const int displs[],
                  MPI_Datatype sendtype, void *recvbuf, int recvcount,
                  MPI_Datatype recvtype, int root, MPI_Comm comm,
                  MPI_Request *request);

/* Collective Communication - All-to-All */
int MPI_Allgather(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                  void *recvbuf, int recvcount, MPI_Datatype recvtype,
                  MPI_Comm comm);
int MPI_Iallgather(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                   void *recvbuf, int recvcount, MPI_Datatype recvtype,
                   MPI_Comm comm, MPI_Request *request);
int MPI_Allgatherv(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                   void *recvbuf, const int recvcounts[], const int displs[],
                   MPI_Datatype recvtype, MPI_Comm comm);
int MPI_Iallgatherv(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                    void *recvbuf, const int recvcounts[], const int displs[],
                    MPI_Datatype recvtype, MPI_Comm comm, MPI_Request *request);
int MPI_Alltoall(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                 void *recvbuf, int recvcount, MPI_Datatype recvtype,
                 MPI_Comm comm);
int MPI_Ialltoall(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                  void *recvbuf, int recvcount, MPI_Datatype recvtype,
                  MPI_Comm comm, MPI_Request *request);
int MPI_Alltoallv(const void *sendbuf, const int sendcounts[],
                  const int sdispls[], MPI_Datatype sendtype,
                  void *recvbuf, const int recvcounts[], const int rdispls[],
                  MPI_Datatype recvtype, MPI_Comm comm);
int MPI_Ialltoallv(const void *sendbuf, const int sendcounts[],
                   const int sdispls[], MPI_Datatype sendtype,
                   void *recvbuf, const int recvcounts[], const int rdispls[],
                   MPI_Datatype recvtype, MPI_Comm comm, MPI_Request *request);
int MPI_Alltoallw(const void *sendbuf, const int sendcounts[],
                  const int sdispls[], const MPI_Datatype sendtypes[],
                  void *recvbuf, const int recvcounts[], const int rdispls[],
                  const MPI_Datatype recvtypes[], MPI_Comm comm);
int MPI_Ialltoallw(const void *sendbuf, const int sendcounts[],
                   const int sdispls[], const MPI_Datatype sendtypes[],
                   void *recvbuf, const int recvcounts[], const int rdispls[],
                   const MPI_Datatype recvtypes[], MPI_Comm comm,
                   MPI_Request *request);

/* Collective Communication - Reduce */
int MPI_Reduce(const void *sendbuf, void *recvbuf, int count,
               MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm);
int MPI_Ireduce(const void *sendbuf, void *recvbuf, int count,
                MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm,
                MPI_Request *request);
int MPI_Allreduce(const void *sendbuf, void *recvbuf, int count,
                  MPI_Datatype datatype, MPI_Op op, MPI_Comm comm);
int MPI_Iallreduce(const void *sendbuf, void *recvbuf, int count,
                   MPI_Datatype datatype, MPI_Op op, MPI_Comm comm,
                   MPI_Request *request);
int MPI_Reduce_scatter(const void *sendbuf, void *recvbuf, const int recvcounts[],
                       MPI_Datatype datatype, MPI_Op op, MPI_Comm comm);
int MPI_Ireduce_scatter(const void *sendbuf, void *recvbuf, const int recvcounts[],
                        MPI_Datatype datatype, MPI_Op op, MPI_Comm comm,
                        MPI_Request *request);
int MPI_Reduce_scatter_block(const void *sendbuf, void *recvbuf, int recvcount,
                              MPI_Datatype datatype, MPI_Op op, MPI_Comm comm);
int MPI_Ireduce_scatter_block(const void *sendbuf, void *recvbuf, int recvcount,
                               MPI_Datatype datatype, MPI_Op op, MPI_Comm comm,
                               MPI_Request *request);

/* Collective Communication - Scan */
int MPI_Scan(const void *sendbuf, void *recvbuf, int count,
             MPI_Datatype datatype, MPI_Op op, MPI_Comm comm);
int MPI_Iscan(const void *sendbuf, void *recvbuf, int count,
              MPI_Datatype datatype, MPI_Op op, MPI_Comm comm,
              MPI_Request *request);
int MPI_Exscan(const void *sendbuf, void *recvbuf, int count,
               MPI_Datatype datatype, MPI_Op op, MPI_Comm comm);
int MPI_Iexscan(const void *sendbuf, void *recvbuf, int count,
                MPI_Datatype datatype, MPI_Op op, MPI_Comm comm,
                MPI_Request *request);

/* Datatype Management */
int MPI_Type_contiguous(int count, MPI_Datatype oldtype, MPI_Datatype *newtype);
int MPI_Type_vector(int count, int blocklength, int stride,
                    MPI_Datatype oldtype, MPI_Datatype *newtype);
int MPI_Type_hvector(int count, int blocklength, MPI_Aint stride,
                     MPI_Datatype oldtype, MPI_Datatype *newtype);
int MPI_Type_indexed(int count, const int array_of_blocklengths[],
                     const int array_of_displacements[],
                     MPI_Datatype oldtype, MPI_Datatype *newtype);
int MPI_Type_hindexed(int count, const int array_of_blocklengths[],
                      const MPI_Aint array_of_displacements[],
                      MPI_Datatype oldtype, MPI_Datatype *newtype);
int MPI_Type_struct(int count, const int array_of_blocklengths[],
                    const MPI_Aint array_of_displacements[],
                    const MPI_Datatype array_of_types[],
                    MPI_Datatype *newtype);
int MPI_Type_create_struct(int count, const int array_of_blocklengths[],
                           const MPI_Aint array_of_displacements[],
                           const MPI_Datatype array_of_types[],
                           MPI_Datatype *newtype);
int MPI_Type_create_resized(MPI_Datatype oldtype, MPI_Aint lb, MPI_Aint extent,
                            MPI_Datatype *newtype);
int MPI_Type_size(MPI_Datatype datatype, int *size);
int MPI_Type_commit(MPI_Datatype *datatype);
int MPI_Type_free(MPI_Datatype *datatype);
int MPI_Type_get_extent(MPI_Datatype datatype, MPI_Aint *lb, MPI_Aint *extent);
int MPI_Type_get_true_extent(MPI_Datatype datatype, MPI_Aint *true_lb,
                             MPI_Aint *true_extent);

/* Pack and Unpack */
int MPI_Pack(const void *inbuf, int incount, MPI_Datatype datatype,
             void *outbuf, int outsize, int *position, MPI_Comm comm);
int MPI_Unpack(const void *inbuf, int insize, int *position, void *outbuf,
               int outcount, MPI_Datatype datatype, MPI_Comm comm);
int MPI_Pack_size(int incount, MPI_Datatype datatype, MPI_Comm comm, int *size);

/* One-sided Communication (RMA) */
int MPI_Win_create(void *base, MPI_Aint size, int disp_unit, MPI_Info info,
                   MPI_Comm comm, MPI_Win *win);
int MPI_Win_allocate(MPI_Aint size, int disp_unit, MPI_Info info,
                     MPI_Comm comm, void *baseptr, MPI_Win *win);
int MPI_Win_allocate_shared(MPI_Aint size, int disp_unit, MPI_Info info,
                            MPI_Comm comm, void *baseptr, MPI_Win *win);
int MPI_Win_create_dynamic(MPI_Info info, MPI_Comm comm, MPI_Win *win);
int MPI_Win_attach(MPI_Win win, void *base, MPI_Aint size);
int MPI_Win_detach(MPI_Win win, const void *base);
int MPI_Win_free(MPI_Win *win);
int MPI_Win_get_group(MPI_Win win, MPI_Group *group);
int MPI_Win_shared_query(MPI_Win win, int rank, MPI_Aint *size, int *disp_unit,
                         void *baseptr);

/* RMA Data Access */
int MPI_Put(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype,
            int target_rank, MPI_Aint target_disp, int target_count,
            MPI_Datatype target_datatype, MPI_Win win);
int MPI_Get(void *origin_addr, int origin_count, MPI_Datatype origin_datatype,
            int target_rank, MPI_Aint target_disp, int target_count,
            MPI_Datatype target_datatype, MPI_Win win);
int MPI_Accumulate(const void *origin_addr, int origin_count,
                   MPI_Datatype origin_datatype, int target_rank,
                   MPI_Aint target_disp, int target_count,
                   MPI_Datatype target_datatype, MPI_Op op, MPI_Win win);
int MPI_Get_accumulate(const void *origin_addr, int origin_count,
                       MPI_Datatype origin_datatype, void *result_addr,
                       int result_count, MPI_Datatype result_datatype,
                       int target_rank, MPI_Aint target_disp, int target_count,
                       MPI_Datatype target_datatype, MPI_Op op, MPI_Win win);
int MPI_Fetch_and_op(const void *origin_addr, void *result_addr,
                     MPI_Datatype datatype, int target_rank,
                     MPI_Aint target_disp, MPI_Op op, MPI_Win win);
int MPI_Compare_and_swap(const void *origin_addr, const void *compare_addr,
                         void *result_addr, MPI_Datatype datatype,
                         int target_rank, MPI_Aint target_disp, MPI_Win win);

/* RMA Request-based Operations */
int MPI_Rput(const void *origin_addr, int origin_count,
             MPI_Datatype origin_datatype, int target_rank,
             MPI_Aint target_disp, int target_count,
             MPI_Datatype target_datatype, MPI_Win win, MPI_Request *request);
int MPI_Rget(void *origin_addr, int origin_count, MPI_Datatype origin_datatype,
             int target_rank, MPI_Aint target_disp, int target_count,
             MPI_Datatype target_datatype, MPI_Win win, MPI_Request *request);
int MPI_Raccumulate(const void *origin_addr, int origin_count,
                    MPI_Datatype origin_datatype, int target_rank,
                    MPI_Aint target_disp, int target_count,
                    MPI_Datatype target_datatype, MPI_Op op, MPI_Win win,
                    MPI_Request *request);
int MPI_Rget_accumulate(const void *origin_addr, int origin_count,
                        MPI_Datatype origin_datatype, void *result_addr,
                        int result_count, MPI_Datatype result_datatype,
                        int target_rank, MPI_Aint target_disp,
                        int target_count, MPI_Datatype target_datatype,
                        MPI_Op op, MPI_Win win, MPI_Request *request);

/* RMA Synchronization */
int MPI_Win_fence(int assert, MPI_Win win);
int MPI_Win_start(MPI_Group group, int assert, MPI_Win win);
int MPI_Win_complete(MPI_Win win);
int MPI_Win_post(MPI_Group group, int assert, MPI_Win win);
int MPI_Win_wait(MPI_Win win);
int MPI_Win_test(MPI_Win win, int *flag);
int MPI_Win_lock(int lock_type, int rank, int assert, MPI_Win win);
int MPI_Win_unlock(int rank, MPI_Win win);
int MPI_Win_lock_all(int assert, MPI_Win win);
int MPI_Win_unlock_all(MPI_Win win);
int MPI_Win_flush(int rank, MPI_Win win);
int MPI_Win_flush_all(MPI_Win win);
int MPI_Win_flush_local(int rank, MPI_Win win);
int MPI_Win_flush_local_all(MPI_Win win);
int MPI_Win_sync(MPI_Win win);

/* Topology */
int MPI_Cart_create(MPI_Comm comm_old, int ndims, const int dims[],
                    const int periods[], int reorder, MPI_Comm *comm_cart);
int MPI_Cart_coords(MPI_Comm comm, int rank, int maxdims, int coords[]);
int MPI_Cart_get(MPI_Comm comm, int maxdims, int dims[], int periods[],
                 int coords[]);
int MPI_Cart_rank(MPI_Comm comm, const int coords[], int *rank);
int MPI_Cart_shift(MPI_Comm comm, int direction, int disp, int *rank_source,
                   int *rank_dest);
int MPI_Cart_sub(MPI_Comm comm, const int remain_dims[], MPI_Comm *newcomm);
int MPI_Dims_create(int nnodes, int ndims, int dims[]);
int MPI_Cartdim_get(MPI_Comm comm, int *ndims);

/* Graph Topology */
int MPI_Graph_create(MPI_Comm comm_old, int nnodes, const int index[],
                     const int edges[], int reorder, MPI_Comm *comm_graph);
int MPI_Graph_get(MPI_Comm comm, int maxindex, int maxedges, int index[],
                  int edges[]);
int MPI_Graph_map(MPI_Comm comm, int nnodes, const int index[], const int edges[],
                  int *newrank);
int MPI_Graph_neighbors_count(MPI_Comm comm, int rank, int *nneighbors);
int MPI_Graph_neighbors(MPI_Comm comm, int rank, int maxneighbors,
                        int neighbors[]);
int MPI_Graphdims_get(MPI_Comm comm, int *nnodes, int *nedges);

/* Distributed Graph Topology (MPI-3) */
int MPI_Dist_graph_create(MPI_Comm comm_old, int n, const int sources[],
                          const int degrees[], const int destinations[],
                          const int weights[], MPI_Info info, int reorder,
                          MPI_Comm *comm_dist_graph);
int MPI_Dist_graph_create_adjacent(MPI_Comm comm_old, int indegree,
                                   const int sources[], const int sourceweights[],
                                   int outdegree, const int destinations[],
                                   const int destweights[], MPI_Info info,
                                   int reorder, MPI_Comm *comm_dist_graph);
int MPI_Dist_graph_neighbors_count(MPI_Comm comm, int *indegree, int *outdegree,
                                   int *weighted);
int MPI_Dist_graph_neighbors(MPI_Comm comm, int maxindegree, int sources[],
                             int sourceweights[], int maxoutdegree,
                             int destinations[], int destweights[]);

/* Neighborhood Collectives (MPI-3) */
int MPI_Neighbor_allgather(const void *sendbuf, int sendcount,
                           MPI_Datatype sendtype, void *recvbuf,
                           int recvcount, MPI_Datatype recvtype, MPI_Comm comm);
int MPI_Ineighbor_allgather(const void *sendbuf, int sendcount,
                            MPI_Datatype sendtype, void *recvbuf,
                            int recvcount, MPI_Datatype recvtype,
                            MPI_Comm comm, MPI_Request *request);
int MPI_Neighbor_allgatherv(const void *sendbuf, int sendcount,
                            MPI_Datatype sendtype, void *recvbuf,
                            const int recvcounts[], const int displs[],
                            MPI_Datatype recvtype, MPI_Comm comm);
int MPI_Ineighbor_allgatherv(const void *sendbuf, int sendcount,
                             MPI_Datatype sendtype, void *recvbuf,
                             const int recvcounts[], const int displs[],
                             MPI_Datatype recvtype, MPI_Comm comm,
                             MPI_Request *request);
int MPI_Neighbor_alltoall(const void *sendbuf, int sendcount,
                          MPI_Datatype sendtype, void *recvbuf,
                          int recvcount, MPI_Datatype recvtype, MPI_Comm comm);
int MPI_Ineighbor_alltoall(const void *sendbuf, int sendcount,
                           MPI_Datatype sendtype, void *recvbuf,
                           int recvcount, MPI_Datatype recvtype,
                           MPI_Comm comm, MPI_Request *request);
int MPI_Neighbor_alltoallv(const void *sendbuf, const int sendcounts[],
                           const int sdispls[], MPI_Datatype sendtype,
                           void *recvbuf, const int recvcounts[],
                           const int rdispls[], MPI_Datatype recvtype,
                           MPI_Comm comm);
int MPI_Ineighbor_alltoallv(const void *sendbuf, const int sendcounts[],
                            const int sdispls[], MPI_Datatype sendtype,
                            void *recvbuf, const int recvcounts[],
                            const int rdispls[], MPI_Datatype recvtype,
                            MPI_Comm comm, MPI_Request *request);
int MPI_Neighbor_alltoallw(const void *sendbuf, const int sendcounts[],
                           const MPI_Aint sdispls[],
                           const MPI_Datatype sendtypes[], void *recvbuf,
                           const int recvcounts[], const MPI_Aint rdispls[],
                           const MPI_Datatype recvtypes[], MPI_Comm comm);
int MPI_Ineighbor_alltoallw(const void *sendbuf, const int sendcounts[],
                            const MPI_Aint sdispls[],
                            const MPI_Datatype sendtypes[], void *recvbuf,
                            const int recvcounts[], const MPI_Aint rdispls[],
                            const MPI_Datatype recvtypes[], MPI_Comm comm,
                            MPI_Request *request);

/* Info Objects */
int MPI_Info_create(MPI_Info *info);
int MPI_Info_set(MPI_Info info, const char *key, const char *value);
int MPI_Info_get(MPI_Info info, const char *key, int valuelen, char *value,
                 int *flag);
int MPI_Info_delete(MPI_Info info, const char *key);
int MPI_Info_get_nkeys(MPI_Info info, int *nkeys);
int MPI_Info_get_nthkey(MPI_Info info, int n, char *key);
int MPI_Info_get_valuelen(MPI_Info info, const char *key, int *valuelen,
                          int *flag);
int MPI_Info_dup(MPI_Info info, MPI_Info *newinfo);
int MPI_Info_free(MPI_Info *info);

/* Error Handling */
int MPI_Errhandler_free(MPI_Errhandler *errhandler);
int MPI_Error_string(int errorcode, char *string, int *resultlen);
int MPI_Error_class(int errorcode, int *errorclass);

/* Timing */
double MPI_Wtime(void);
double MPI_Wtick(void);

/* Environment */
int MPI_Get_processor_name(char *name, int *resultlen);
int MPI_Get_version(int *version, int *subversion);
int MPI_Get_library_version(char *version, int *resultlen);

/* Attribute Caching */
int MPI_Keyval_free(int *keyval);
int MPI_Attr_put(MPI_Comm comm, int keyval, void *attribute_val);
int MPI_Attr_get(MPI_Comm comm, int keyval, void *attribute_val, int *flag);
int MPI_Attr_delete(MPI_Comm comm, int keyval);

/* Buffer Management */
int MPI_Buffer_attach(void *buffer, int size);
int MPI_Buffer_detach(void *buffer, int *size);

/* Address Manipulation */
int MPI_Get_address(const void *location, MPI_Aint *address);
int MPI_Address(void *location, MPI_Aint *address);

/* Query Thread Support */
int MPI_Query_thread(int *provided);
int MPI_Is_thread_main(int *flag);

/* Dynamic Process Management */
int MPI_Comm_spawn(const char *command, char *argv[], int maxprocs,
                   MPI_Info info, int root, MPI_Comm comm, MPI_Comm *intercomm,
                   int array_of_errcodes[]);
int MPI_Comm_spawn_multiple(int count, char *array_of_commands[],
                            char **array_of_argv[], const int array_of_maxprocs[],
                            const MPI_Info array_of_info[], int root,
                            MPI_Comm comm, MPI_Comm *intercomm,
                            int array_of_errcodes[]);
int MPI_Comm_get_parent(MPI_Comm *parent);
int MPI_Comm_connect(const char *port_name, MPI_Info info, int root,
                     MPI_Comm comm, MPI_Comm *newcomm);
int MPI_Comm_accept(const char *port_name, MPI_Info info, int root,
                    MPI_Comm comm, MPI_Comm *newcomm);
int MPI_Comm_disconnect(MPI_Comm *comm);
int MPI_Open_port(MPI_Info info, char *port_name);
int MPI_Close_port(const char *port_name);
int MPI_Publish_name(const char *service_name, MPI_Info info,
                     const char *port_name);
int MPI_Unpublish_name(const char *service_name, MPI_Info info,
                       const char *port_name);
int MPI_Lookup_name(const char *service_name, MPI_Info info, char *port_name);

/* MPI I/O */
int MPI_File_open(MPI_Comm comm, const char *filename, int amode,
                  MPI_Info info, MPI_File *fh);
int MPI_File_close(MPI_File *fh);
int MPI_File_delete(const char *filename, MPI_Info info);
int MPI_File_set_size(MPI_File fh, MPI_Offset size);
int MPI_File_preallocate(MPI_File fh, MPI_Offset size);
int MPI_File_get_size(MPI_File fh, MPI_Offset *size);
int MPI_File_get_group(MPI_File fh, MPI_Group *group);
int MPI_File_get_amode(MPI_File fh, int *amode);
int MPI_File_set_info(MPI_File fh, MPI_Info info);
int MPI_File_get_info(MPI_File fh, MPI_Info *info_used);
int MPI_File_set_view(MPI_File fh, MPI_Offset disp, MPI_Datatype etype,
                      MPI_Datatype filetype, const char *datarep, MPI_Info info);
int MPI_File_get_view(MPI_File fh, MPI_Offset *disp, MPI_Datatype *etype,
                      MPI_Datatype *filetype, char *datarep);
int MPI_File_read_at(MPI_File fh, MPI_Offset offset, void *buf, int count,
                     MPI_Datatype datatype, MPI_Status *status);
int MPI_File_read_at_all(MPI_File fh, MPI_Offset offset, void *buf, int count,
                         MPI_Datatype datatype, MPI_Status *status);
int MPI_File_write_at(MPI_File fh, MPI_Offset offset, const void *buf,
                      int count, MPI_Datatype datatype, MPI_Status *status);
int MPI_File_write_at_all(MPI_File fh, MPI_Offset offset, const void *buf,
                          int count, MPI_Datatype datatype, MPI_Status *status);
int MPI_File_iread_at(MPI_File fh, MPI_Offset offset, void *buf, int count,
                      MPI_Datatype datatype, MPI_Request *request);
int MPI_File_iwrite_at(MPI_File fh, MPI_Offset offset, const void *buf,
                       int count, MPI_Datatype datatype, MPI_Request *request);
int MPI_File_read(MPI_File fh, void *buf, int count, MPI_Datatype datatype,
                  MPI_Status *status);
int MPI_File_read_all(MPI_File fh, void *buf, int count, MPI_Datatype datatype,
                      MPI_Status *status);
int MPI_File_write(MPI_File fh, const void *buf, int count,
                   MPI_Datatype datatype, MPI_Status *status);
int MPI_File_write_all(MPI_File fh, const void *buf, int count,
                       MPI_Datatype datatype, MPI_Status *status);
int MPI_File_iread(MPI_File fh, void *buf, int count, MPI_Datatype datatype,
                   MPI_Request *request);
int MPI_File_iwrite(MPI_File fh, const void *buf, int count,
                    MPI_Datatype datatype, MPI_Request *request);
int MPI_File_seek(MPI_File fh, MPI_Offset offset, int whence);
int MPI_File_get_position(MPI_File fh, MPI_Offset *offset);
int MPI_File_get_byte_offset(MPI_File fh, MPI_Offset offset,
                             MPI_Offset *disp);
int MPI_File_sync(MPI_File fh);

/* Function Pointer Types for Callbacks */
typedef void MPI_Handler_function(MPI_Comm *, int *, ...);
typedef int MPI_Copy_function(MPI_Comm oldcomm, int keyval, void *extra_state,
                              void *attribute_val_in, void *attribute_val_out,
                              int *flag);
typedef int MPI_Delete_function(MPI_Comm comm, int keyval, void *attribute_val,
                                void *extra_state);
typedef void MPI_User_function(void *invec, void *inoutvec, int *len,
                               MPI_Datatype *datatype);

/* Create Custom Operations */
int MPI_Op_create(MPI_User_function *function, int commute, MPI_Op *op);
int MPI_Op_free(MPI_Op *op);

#ifdef __cplusplus
}
#endif

#endif /* TPB_MPI_STUB_H */

