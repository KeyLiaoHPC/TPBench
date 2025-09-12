// mpi_trace.cpp
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <cstring>
#include <memory>
#include <unistd.h>
#include <chrono>
#include <array>
#include <functional>

#include <mpi.h>

// Include TPBench headers for additional functionality
extern "C" {
#include "tpbench.h"
}

// Optional RDMA-based sync hooks
#include "sync/sync.h"

namespace fs = std::filesystem;

namespace {

#ifdef DEBUG

#define DBGPRINT(msg, dbg) if (dbg == true && profiler_rank == 0) { \
    std::cout << "[MPI_TRACE][" << __FILE__ << ":" << __LINE__ << "][" << node_name << "] "<< msg << std::endl; \
}
#define WARNINGPRINT(msg, dbg) if (dbg == true && profiler_rank == 0) { \
    std::cout << "[MPI_TRACE WARNING][" << __FILE__ << ":" << __LINE__ << "][" << node_name << "] " << msg << std::endl; \
}
#else
#define DBGPRINT(msg, dbg)
#define WARNINGPRINT(msg, dbg)
#endif

#define ERRORPRINT(msg, dbg) if (profiler_rank == 0) { \
    std::cerr << "[MPI_TRACE ERROR][" << __FILE__ << ":" << __LINE__ << "][" << node_name << "] " << msg << std::endl; \
}

// Define a prefix for log files
static std::string LOG_PATH_PREFIX = "./tpmpi_trace_logs"; // Change this to your desired log path prefix
static bool debug = false;
static std::string node_name;

/* Global variables for logging */
// log files' directory should use this pattern: 
// {LOG_PATH_PREFIX}/logs/{timestamp}/{node_name}/mpi_profile_rank_{rank}.log
static bool trace_store_flag = true;
static bool sync_flag = true;
static bool trigger_flag = true;
static std::string log_file_dir;    
static std::unique_ptr<std::ofstream> log_file;
static int profiler_rank = -1;

uint64_t total_time = 0;
uint64_t program_start_time = 0;
uint64_t program_end_time = 0;
uint64_t program_start_cy = 0;
uint64_t program_end_cy = 0;

__getcy_init;
__getcy_grp_init;


#define GET_NS \
    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()

enum class MPI_FUNC {
    MPI_SEND = 0,
    MPI_RECV,
    MPI_BCAST,
    MPI_SCATTER,
    MPI_GATHER,
    MPI_ALLREDUCE,
    MPI_ALLTOALL,
    MPI_WAIT
};

// record time, cy, size, count
std::array <std::array<uint64_t, 4>, static_cast<size_t>(MPI_FUNC::MPI_WAIT) + 1> MPI_FUNC_TRACE_TIMER;

// tools: 
std::string get_type_name(MPI_Datatype type) {
    if (type == MPI_CHAR) return "MPI_CHAR";
    if (type == MPI_INT) return "MPI_INT";
    if (type == MPI_FLOAT) return "MPI_FLOAT";
    if (type == MPI_DOUBLE) return "MPI_DOUBLE";
    // Add more types as needed
    return "UNKNOWN_TYPE";
}

std::string get_func_name(MPI_FUNC func) {
    switch (func) {
        case MPI_FUNC::MPI_SEND: return "MPI_Send";
        case MPI_FUNC::MPI_RECV: return "MPI_Recv";
        case MPI_FUNC::MPI_BCAST: return "MPI_Bcast";
        case MPI_FUNC::MPI_SCATTER: return "MPI_Scatter";
        case MPI_FUNC::MPI_GATHER: return "MPI_Gather";
        case MPI_FUNC::MPI_ALLREDUCE: return "MPI_Allreduce";
        case MPI_FUNC::MPI_ALLTOALL: return "MPI_Alltoall";
        case MPI_FUNC::MPI_WAIT: return "MPI_Wait";
        default: return "UNKNOWN_FUNC";
    }
}

size_t get_type_size(MPI_Datatype type) {
    if (type == MPI_CHAR) return sizeof(char);
    if (type == MPI_INT) return sizeof(int);
    if (type == MPI_FLOAT) return sizeof(float);
    if (type == MPI_DOUBLE) return sizeof(double);
    // Add more types as needed
    return 1; // Default size
}

template <typename Func>
inline int measure_time(Func func, MPI_FUNC func_type, size_t size, const std::string& log_info = "") {
    uint64_t start_time, end_time, elapsed_time;
    uint64_t start_cy, end_cy, elapsed_cy;
    
    start_time = GET_NS;
    __getcy_st_t;
    int result = func();
    __getcy_end_t;
    end_time = GET_NS;

    start_cy = ((hi1 << 32) | lo1);
    end_cy = ((hi2 << 32) | lo2);

    elapsed_time = end_time - start_time;
    elapsed_cy = end_cy - start_cy;

    MPI_FUNC_TRACE_TIMER[static_cast<size_t>(func_type)][0] += elapsed_time;
    MPI_FUNC_TRACE_TIMER[static_cast<size_t>(func_type)][1] += elapsed_cy;
    MPI_FUNC_TRACE_TIMER[static_cast<size_t>(func_type)][2] += size;
    MPI_FUNC_TRACE_TIMER[static_cast<size_t>(func_type)][3] ++;

    if (log_file != nullptr && log_file->is_open() && !log_info.empty()) {
        *log_file << "[ " << start_time << " ] " << log_info << "\n";
        *log_file << "    >> Execution Time: " << elapsed_time << " ns\n";
        *log_file << "    >> Data Size: " << size << " bytes\n";
        *log_file << "    >> Execution Cycles: " << elapsed_cy << "\n";
    }
    return result;
}

}


// C linkage for MPI functions
extern "C" {
/*
 * =============================================================================
 * Initialization and Finalization Functions
 * =============================================================================
 */

int MPI_Init(int *argc, char ***argv) {

    program_start_time = GET_NS;
    __getcy_st_t;
    program_start_cy = ((hi1 << 32) | lo1);

    // 0. Get the environment variables
    const char *trace_file_env = std::getenv("TPMPI_TRACE_STORE");
    const char *log_path_env = std::getenv("MPI_LOG_PATH_PREFIX");
    const char *sync_env = std::getenv("RDMASYNC_ENABLE");
    const char *trigger_env = std::getenv("RDMASYNC_TRIGGER");
    const char *debug_env = std::getenv("TPMPI_DEBUG");

    if (trace_file_env != nullptr && std::strcmp(trace_file_env, "0") == 0) {
        trace_store_flag = false;
    }
    if (sync_env == nullptr || std::strcmp(sync_env, "1") != 0) {
        sync_flag = false;
    }
    if (trigger_env == nullptr || std::strcmp(trigger_env, "1") != 0) {
        trigger_flag = false;
    }
    if (debug_env != nullptr && std::strcmp(debug_env, "1") == 0) {
        debug = true;
    }
    
    // Get hostname using standard POSIX function
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    hostname[sizeof(hostname) - 1] = '\0'; // Ensure null-termination
    node_name = hostname;

    // DBGPRINT("MPI_Init called with trace_store_flag: " + std::to_string(trace_store_flag) +
    //    ", sync_flag: " + std::to_string(sync_flag) +
    //    ", trigger_flag: " + std::to_string(trigger_flag));

    // 1. Call the actual MPI_Init
    int ret = PMPI_Init(argc, argv);

    // 2. Get the process rank
    PMPI_Comm_rank(MPI_COMM_WORLD, &profiler_rank);

    // Initialize optional RDMA sync (no-op if disabled)
    if (sync_flag == true) {
        // Check if we're inside a participant process - if so, skip RDMA initialization
        // to prevent double initialization conflicts
        DBGPRINT("Initializing RDMA sync...", debug);
        rdmasync::init(profiler_rank);
        // Check if RDMA is actually enabled after initialization
        if (!rdmasync::is_enabled()) {
            ERRORPRINT("RDMA initialization failed, disabling trigger and sync", debug);
            sync_flag = false;
            trigger_flag = false;
        } else {
            DBGPRINT("RDMA sync enabled", debug);
        }
    } else {
        DBGPRINT("RDMA sync disabled by environment", debug);
    }
    
    if (log_path_env != nullptr) {
        // Use environment variable if set
        LOG_PATH_PREFIX = log_path_env;
    } else {
        // 3. Create a separate log file for each process
        char time_buf[32];
        if (profiler_rank == 0) {
            time_t now = time(nullptr);
            strftime(time_buf, sizeof(time_buf), "%Y%m%d_%H%M%S", localtime(&now));
        }
        PMPI_Bcast(&time_buf, sizeof(time_buf), MPI_CHAR, 0, MPI_COMM_WORLD);
        std::string time_stamp{time_buf};
        // Use default log path prefix
        LOG_PATH_PREFIX = "./tpmpi_trace_logs/" + time_stamp;
    }



    log_file_dir = LOG_PATH_PREFIX + "/" + node_name;
    fs::path log_path(log_file_dir);
    // Create the directory for log files
    try {
        if (!fs::exists(log_path)) {
            fs::create_directories(log_path);
        }
    } catch (const fs::filesystem_error& e) {
        ERRORPRINT("Error creating log directory: " + std::string(e.what()), debug);
        return ret; // Return an error code
    }

    std::string filename = log_file_dir + "/mpi_profile_rank_" + std::to_string(profiler_rank) + ".log";

    if (trace_store_flag == true) {
        log_file = std::make_unique<std::ofstream>(filename);
    } else {
        log_file = nullptr;
    }

    if (log_file && log_file->is_open()) {
        *log_file << "=========================================================\n";
        *log_file << " MPI Profiler Initialized for Rank " << profiler_rank << "\n";
        *log_file << " Log File: " << filename << "\n";
        *log_file << " Start Time: " << program_start_time << " ns\n";
        *log_file << " Start Cycles: " << program_start_cy << "\n";
        *log_file << "=========================================================\n";
    }
    return ret;
}


int MPI_Finalize() {
    if (trigger_flag == true) {
        DBGPRINT("Finalizing RDMA trigger...", debug);
        rdmasync::tp_trigger(rdmasync::TRIGGER_MSG::FINISH);
    }
    if (sync_flag == true) {
        rdmasync::finalize();
    }
    int ret = PMPI_Finalize();
    __getcy_end_t;
    program_end_cy = ((hi2 << 32) | lo2);
    program_end_time = GET_NS;

    std::string log_data;
    for (size_t i = 0; i < MPI_FUNC_TRACE_TIMER.size(); ++i) {
        const auto& value = MPI_FUNC_TRACE_TIMER[i];
        if (value[3] > 0) {
            MPI_FUNC func_type = static_cast<MPI_FUNC>(i);
            log_data += "[ " + std::to_string(program_end_time) + " ] " + get_func_name(func_type) + "\n";
            log_data += "    >> Total Time: " + std::to_string(value[0]) + " ns\n";
            log_data += "    >> Total Cycles: " + std::to_string(value[1]) + "\n";
            log_data += "    >> Data Size: " + std::to_string(value[2]) + " bytes\n";
            log_data += "    >> Call Count: " + std::to_string(value[3]) + "\n";
        }
    }
    
    log_data += " Total Execution Time: " + std::to_string(program_end_time - program_start_time) + " ns\n";
    log_data += " Total Execution Cycles: " + std::to_string(program_end_cy - program_start_cy) + "\n";

    if (profiler_rank == 0) {
        std::cout << "=========================================================\n";
        std::cout << log_data;
        std::cout << "=========================================================\n";
    }
    
    if (log_file && log_file->is_open()) {
        *log_file << "[ " << program_end_time << " ] MPI_Finalize()\n";
        *log_file << "=========================================================\n";
        *log_file << " MPI Profiler Finalized for Rank " << profiler_rank << "\n";
        *log_file << log_data;
        *log_file << "=========================================================\n";
        log_file->close();
    }
    return ret;
}

/*
 * =============================================================================
 * Point-to-point Communication Functions
 * =============================================================================
 */
int MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm) {
    DBGPRINT("MPI_Send called: count=" + std::to_string(count) + ", type=" + get_type_name(datatype) +
        ", dest=" + std::to_string(dest) + ", tag=" + std::to_string(tag), debug);
    if (trigger_flag == true) {
        DBGPRINT("Trigger before MPI_Send", debug);
        rdmasync::tp_trigger(rdmasync::TRIGGER_MSG::ONCE);
        PMPI_Barrier(comm);
    }
    if (sync_flag == true && trigger_flag == false) {
        DBGPRINT("Sync before MPI_Send", debug);
        rdmasync::tp_sync(__func__);
        PMPI_Barrier(comm);
    }
    int ret = measure_time([&]() {
        return PMPI_Send(buf, count, datatype, dest, tag, comm);
    }, MPI_FUNC::MPI_SEND, count * get_type_size(datatype),
    "MPI_Send(count=" + std::to_string(count) + ", type=" + get_type_name(datatype) +
    ", dest=" + std::to_string(dest) + ", tag=" + std::to_string(tag) + ")" +
    "\n    >> Path: " + std::to_string(profiler_rank) + " -> " + std::to_string(dest));

    return ret;
}

int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status) {
    DBGPRINT("MPI_Recv called: count=" + std::to_string(count) + ", type=" + get_type_name(datatype) +
        ", source=" + std::to_string(source) + ", tag=" + std::to_string(tag), debug);
    if (trigger_flag == true) {
        DBGPRINT("Trigger before MPI_Recv", debug);
        rdmasync::tp_trigger(rdmasync::TRIGGER_MSG::ONCE);
        PMPI_Barrier(comm);
    }
    if (sync_flag == true && trigger_flag == false) {
        DBGPRINT("Sync before MPI_Recv", debug);
        rdmasync::tp_sync(__func__);
        PMPI_Barrier(comm);
    }
    
    int ret = measure_time([&]() {
        return PMPI_Recv(buf, count, datatype, source, tag, comm, status);
    }, MPI_FUNC::MPI_RECV, count * get_type_size(datatype), 
    "MPI_Recv(count=" + std::to_string(count) + ", type=" + get_type_name(datatype) + 
    ", source=" + std::to_string(source) + ", tag=" + std::to_string(tag) + ")" +
    "\n    >> Path: " + std::to_string(source) + " -> " + std::to_string(profiler_rank));
    
    return ret;
}

/*
 * =============================================================================
 * Collective Communication Functions
 * =============================================================================
 */
int MPI_Bcast(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm) {
    DBGPRINT("MPI_Bcast called: count=" + std::to_string(count) + ", type=" + get_type_name(datatype) +
        ", root=" + std::to_string(root), debug);
    if (trigger_flag == true) {
        rdmasync::tp_trigger(rdmasync::TRIGGER_MSG::ONCE);
        PMPI_Barrier(comm);
    }
    if (sync_flag == true && trigger_flag == false) {
        rdmasync::tp_sync(__func__);
        PMPI_Barrier(comm);
    }

    int ret = measure_time([&]() {
        return PMPI_Bcast(buffer, count, datatype, root, comm);
    }, MPI_FUNC::MPI_BCAST, count * get_type_size(datatype),
    "MPI_Bcast(count=" + std::to_string(count) + ", type=" + get_type_name(datatype) +
    ", root=" + std::to_string(root) + ")" +
    "\n    >> Path: " + std::to_string(root) + " -> all");

    return ret;
}

int MPI_Reduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm) {
    DBGPRINT("MPI_Reduce called: count=" + std::to_string(count) + ", type=" + get_type_name(datatype) +
        ", root=" + std::to_string(root), debug);
    if (trigger_flag == true) {
        rdmasync::tp_trigger(rdmasync::TRIGGER_MSG::ONCE);
        PMPI_Barrier(comm);
    }

    if (sync_flag == true && trigger_flag == false) {
        rdmasync::tp_sync(__func__);
        PMPI_Barrier(comm);
    }
    
    int ret = measure_time([&]() {
        return PMPI_Reduce(sendbuf, recvbuf, count, datatype, op, root, comm);
    }, MPI_FUNC::MPI_GATHER, count * get_type_size(datatype),
    "MPI_Reduce(count=" + std::to_string(count) + ", type=" + get_type_name(datatype) +
    ", root=" + std::to_string(root) + ")" +
    "\n    >> Path: all -> " + std::to_string(root));

    return ret;
}

int MPI_Allreduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm) {
    DBGPRINT("MPI_Allreduce called: count=" + std::to_string(count) + ", type=" + get_type_name(datatype), debug);
    if (trigger_flag == true) {
        rdmasync::tp_trigger(rdmasync::TRIGGER_MSG::ONCE);
        PMPI_Barrier(comm);
    }
    if (sync_flag == true && trigger_flag == false) {
        rdmasync::tp_sync(__func__);
        PMPI_Barrier(comm);
    }

    int ret = measure_time([&]() {
        return PMPI_Allreduce(sendbuf, recvbuf, count, datatype, op, comm);
    }, MPI_FUNC::MPI_ALLREDUCE, count * get_type_size(datatype),
    "MPI_Allreduce(count=" + std::to_string(count) + ", type=" + get_type_name(datatype) + ")" +
    "\n    >> Path: all -> all");

    return ret;
}

int MPI_Scatter(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm) {
    DBGPRINT("MPI_Scatter called: sendcount=" + std::to_string(sendcount) + ", sendtype=" + get_type_name(sendtype) +
        ", recvcount=" + std::to_string(recvcount) + ", recvtype=" + get_type_name(recvtype) +
        ", root=" + std::to_string(root), debug);
    if (trigger_flag == true) {
        rdmasync::tp_trigger(rdmasync::TRIGGER_MSG::ONCE);
    }

    if (sync_flag == true && trigger_flag == false) {
        rdmasync::tp_sync(__func__);
        PMPI_Barrier(comm);
    }

    int ret = measure_time([&]() {
        return PMPI_Scatter(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm);
    }, MPI_FUNC::MPI_SCATTER, sendcount * get_type_size(sendtype),
    "MPI_Scatter(sendcount=" + std::to_string(sendcount) + ", sendtype=" + get_type_name(sendtype) +
    ", recvcount=" + std::to_string(recvcount) + ", recvtype=" + get_type_name(recvtype) +
    ", root=" + std::to_string(root) + ")" +
    "\n    >> Path: " + std::to_string(root) + " -> all");

    return ret;
}

int MPI_Gather(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm) {
    DBGPRINT("MPI_Gather called: sendcount=" + std::to_string(sendcount) + ", sendtype=" + get_type_name(sendtype) +
        ", recvcount=" + std::to_string(recvcount) + ", recvtype=" + get_type_name(recvtype) +
        ", root=" + std::to_string(root), debug);
    if (trigger_flag == true) {
        rdmasync::tp_trigger(rdmasync::TRIGGER_MSG::ONCE);
        PMPI_Barrier(comm);
    }
    if (sync_flag == true && trigger_flag == false) {
        rdmasync::tp_sync(__func__);
        PMPI_Barrier(comm);
    }

    int ret = measure_time([&]() {
        return PMPI_Gather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm);
    }, MPI_FUNC::MPI_GATHER, sendcount * get_type_size(sendtype),
    "MPI_Gather(sendcount=" + std::to_string(sendcount) + ", sendtype=" + get_type_name(sendtype) +
    ", recvcount=" + std::to_string(recvcount) + ", recvtype=" + get_type_name(recvtype) +
    ", root=" + std::to_string(root) + ")" +
    "\n    >> Path: " + std::to_string(root) + " <- all");
    return ret;
}

int MPI_Alltoall(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm) {
    DBGPRINT("MPI_Alltoall called: sendcount=" + std::to_string(sendcount) + ", sendtype=" + get_type_name(sendtype) +
        ", recvcount=" + std::to_string(recvcount) + ", recvtype=" + get_type_name(recvtype), debug);
    if (trigger_flag == true) {
        rdmasync::tp_trigger(rdmasync::TRIGGER_MSG::ONCE);
        PMPI_Barrier(comm);
    }
    if (sync_flag == true && trigger_flag == false) {
        rdmasync::tp_sync(__func__);
        PMPI_Barrier(comm);
    }
    int ret = measure_time([&]() {
        return PMPI_Alltoall(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm);
    }, MPI_FUNC::MPI_ALLTOALL, sendcount * get_type_size(sendtype),
    "MPI_Alltoall(sendcount=" + std::to_string(sendcount) + ", sendtype=" + get_type_name(sendtype) +
    ", recvcount=" + std::to_string(recvcount) + ", recvtype=" + get_type_name(recvtype) + ")" +
    "\n    >> Path: all <-> all");
    return ret;
}

int MPI_Barrier(MPI_Comm comm) {
    DBGPRINT("MPI_Barrier called", debug);
    if (sync_flag == true && trigger_flag == false){
        DBGPRINT("Sync before MPI_Barrier", debug);
        rdmasync::tp_sync(__func__);
    }
    int ret = measure_time([&]() {
        return PMPI_Barrier(comm);
    }, MPI_FUNC::MPI_WAIT, 0, "MPI_Barrier()");
    
    return ret;
}


} // extern "C"