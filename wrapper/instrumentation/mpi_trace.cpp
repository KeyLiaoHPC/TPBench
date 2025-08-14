// mpi_trace.cpp
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <memory>
#include <unistd.h>
#include <chrono>

#include <mpi.h>

// Include TPBench headers for additional functionality
extern "C" {
#include "tpbench.h"
}

// Optional RDMA-based sync hooks
#include "sync/sync.h"

namespace fs = std::filesystem;

namespace {
    #define GET_NS \
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()
    // Define a prefix for log files
    static std::string LOG_PATH_PREFIX = "./tpmpi_trace_logs"; // Change this to your desired log path prefix

    /* Global variables for logging */
    // log files' directory should use this pattern: 
    // {LOG_PATH_PREFIX}/logs/{timestamp}/{node_name}/mpi_profile_rank_{rank}.log
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
}

// tools: 
const char* get_type_name(MPI_Datatype type) {
    if (type == MPI_CHAR) return "MPI_CHAR";
    if (type == MPI_INT) return "MPI_INT";
    if (type == MPI_FLOAT) return "MPI_FLOAT";
    if (type == MPI_DOUBLE) return "MPI_DOUBLE";
    // Add more types as needed
    return "UNKNOWN_TYPE";
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
    // 1. Call the actual MPI_Init
    int ret = PMPI_Init(argc, argv);

    // 2. Get the process rank
    PMPI_Comm_rank(MPI_COMM_WORLD, &profiler_rank);

     // Initialize optional RDMA sync (no-op if disabled)
    rdmasync::init_if_needed(profiler_rank);

    const char* log_path_env = std::getenv("MPI_LOG_PATH_PREFIX");
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

    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    hostname[sizeof(hostname) - 1] = '\0'; // Ensure null-termination
    std::string node_name = hostname;

    log_file_dir = LOG_PATH_PREFIX + "/" + node_name;
    fs::path log_path(log_file_dir);
    // Create the directory for log files
    try {
        if (!fs::exists(log_path)) {
            fs::create_directories(log_path);
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error creating log directory: " << e.what() << std::endl;
        return ret; // Return an error code
    }

    std::string filename = log_file_dir + "/mpi_profile_rank_" + std::to_string(profiler_rank) + ".log";
    log_file = std::make_unique<std::ofstream>(filename);

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
    // Optional RDMA sync finalization
    rdmasync::finalize();
    int ret = PMPI_Finalize();
    __getcy_end_t;
    program_end_cy = ((hi2 << 32) | lo2);
    program_end_time = GET_NS;
    if (log_file && log_file->is_open()) {
        *log_file << "[ " << program_end_time << " ] MPI_Finalize()\n";
        *log_file << "=========================================================\n";
        *log_file << " MPI Profiler Finalized for Rank " << profiler_rank << "\n";
        *log_file << " Total Execution Time: " << (program_end_time - program_start_time) << " ns\n";
        *log_file << " Total Execution Cycles: " << (program_end_cy - program_start_cy) << "\n";
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
    rdmasync::hook(__func__);

    uint64_t start_time, end_time, elapsed;
    uint64_t start_cy, end_cy;
    

    start_time = GET_NS;
    __getcy_st_t;
    int ret = PMPI_Send(buf, count, datatype, dest, tag, comm);
    __getcy_end_t;
    end_time = GET_NS;

    start_cy = ((hi1 << 32) | lo1);
    end_cy = ((hi2 << 32) | lo2);
    
    if (log_file && log_file->is_open()) {
        elapsed = end_time - start_time;
        *log_file << "[ " << start_time << " ] MPI_Send(count=" << count 
                  << ", type=" << get_type_name(datatype) 
                  << ", dest=" << dest 
                  << ", tag=" << tag << ")\n";
        *log_file << "    >> Path: " << profiler_rank << " -> " << dest << "\n";
        *log_file << "    >> Execution Time: " << elapsed << " ns\n";
        *log_file << "    >> Execution Cycles: " << end_cy - start_cy << "\n";
    }
    return ret;
}

int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status) {
    // Optional sync hook
    rdmasync::hook(__func__);

    uint64_t start_time, end_time, elapsed;
    uint64_t start_cy, end_cy;
    
    start_time = GET_NS;
    __getcy_st_t;
    int ret = PMPI_Recv(buf, count, datatype, source, tag, comm, status);
    __getcy_end_t;
    end_time = GET_NS;

    start_cy = ((hi1 << 32) | lo1);
    end_cy = ((hi2 << 32) | lo2);

    if (log_file && log_file->is_open()) {
        elapsed = end_time - start_time;
        int actual_source = (status != MPI_STATUS_IGNORE) ? status->MPI_SOURCE : source;
        *log_file << "[ " << start_time << " ] MPI_Recv(count=" << count 
                  << ", type=" << get_type_name(datatype) 
                  << ", source=" << source 
                  << ", tag=" << tag << ")\n";
        *log_file << "    >> Path: " << actual_source << " -> " << profiler_rank << "\n";
        *log_file << "    >> Execution Time: " << elapsed << " ns\n";
        *log_file << "    >> Execution Cycles: " << end_cy - start_cy << "\n";
    }
    
    return ret;
}

/*
 * =============================================================================
 * Collective Communication Functions
 * =============================================================================
 */

int MPI_Bcast(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm) {
    // Optional sync hook
    rdmasync::hook(__func__);

    uint64_t start_time, end_time, elapsed;
    uint64_t start_cy, end_cy;
    
    start_time = GET_NS;
    __getcy_st_t;
    int ret = PMPI_Bcast(buffer, count, datatype, root, comm);
    __getcy_end_t;
    end_time = GET_NS;

    if (log_file && log_file->is_open()) {
        elapsed = end_time - start_time;
        *log_file << "[ " << start_time << " ] MPI_Bcast(count=" << count
                  << ", type=" << get_type_name(datatype)
                  << ", root=" << root << ")\n";
        *log_file << "    >> Path: " << root << " -> all\n";
        *log_file << "    >> Execution Time: " << elapsed << " ns\n";
        *log_file << "    >> Execution Cycles: " << end_cy - start_cy << "\n";
    }
    
    return ret;
}

int MPI_Reduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm) {
    // Optional sync hook
    rdmasync::hook(__func__);
    uint64_t start_time, end_time, elapsed;
    uint64_t start_cy, end_cy;
    
    start_time = GET_NS;
    __getcy_st_t;
    int ret = PMPI_Reduce(sendbuf, recvbuf, count, datatype, op, root, comm);
    __getcy_end_t;
    end_time = GET_NS;

    if (log_file && log_file->is_open()) {
        elapsed = end_time - start_time;
        *log_file << "[ " << start_time << " ] MPI_Reduce(count=" << count
                  << ", type=" << get_type_name(datatype)
                  << ", root=" << root << ")\n";
        *log_file << "    >> Path: all -> " << root << "\n";
        *log_file << "    >> Execution Time: " << elapsed << " ns\n";
        *log_file << "    >> Execution Cycles: " << end_cy - start_cy << "\n";
    }
    
    return ret;
}

int MPI_Allreduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm) {
    // Optional sync hook
    rdmasync::hook(__func__);

    uint64_t start_time, end_time, elapsed;
    uint64_t start_cy, end_cy;

    start_time = GET_NS;
    __getcy_st_t;
    int ret = PMPI_Allreduce(sendbuf, recvbuf, count, datatype, op, comm);
    __getcy_end_t;
    end_time = GET_NS;

    if (log_file && log_file->is_open()) {
        end_time = GET_NS;
        elapsed = end_time - start_time;
        *log_file << "[ " << start_time << " ] MPI_Allreduce(count=" << count
                  << ", type=" << get_type_name(datatype) << ")\n";
        *log_file << "    >> Path: all -> all\n";
        *log_file << "    >> Execution Time: " << elapsed << " ns\n";
        *log_file << "    >> Execution Cycles: " << end_cy - start_cy << "\n";
    }
    
    return ret;
}


int MPI_Alltoall(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm) {
    // Optional sync hook
    rdmasync::hook(__func__);

    uint64_t start_time, end_time, elapsed;
    uint64_t start_cy, end_cy;

    start_time = GET_NS;
    __getcy_st_t;
    int ret = PMPI_Alltoall(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm);
    __getcy_end_t;
    end_time = GET_NS;

    if (log_file && log_file->is_open()) {
        end_time = GET_NS;
        elapsed = end_time - start_time;
        *log_file << "[ " << start_time << " ] MPI_Alltoall(sendcount=" << sendcount
                  << ", sendtype=" << get_type_name(sendtype) << ", recvcount=" << recvcount
                  << ", recvtype=" << get_type_name(recvtype) << ")\n";
        *log_file << "    >> Path: all <-> all\n";
        *log_file << "    >> Execution Time: " << elapsed << " ns\n";
        *log_file << "    >> Execution Cycles: " << end_cy - start_cy << "\n";
    }
    
    return ret;
}


int MPI_Barrier(MPI_Comm comm) {
    // Optional sync hook
    rdmasync::hook(__func__);

    uint64_t start_time, end_time, elapsed;
    uint64_t start_cy, end_cy;
    

    start_time = GET_NS;
    __getcy_st_t;
    int ret = PMPI_Barrier(comm);
    __getcy_end_t;
    end_time = GET_NS;

    if (log_file && log_file->is_open()) {
        end_time = GET_NS;
        elapsed = end_time - start_time;
        *log_file << "[ " << start_time << " ] MPI_Barrier()\n";
        *log_file << "    >> Execution Time: " << elapsed << " ns\n";
        *log_file << "    >> Execution Cycles: " << end_cy - start_cy << "\n";

    }
    
    return ret;
}

} // extern "C"