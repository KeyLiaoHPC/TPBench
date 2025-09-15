
Wrapper is a framework to run any benchmarks with benefits of TPBench high precision timing and multicore synchronization. It focuses on evaluating the system's communication performance. The wrapper can run some MPI-based application and hijack their MPI calls and record the time consumption and communication information of each MPI call. It also supports precise time synchronization of processes in a non-MPI environment.

# Background

Currently, the experimental results we can observe indicate that indeed some nodes may have serious problems in inner node performance or inter node communication (To Be Discovered). The TPBench needs to conduct tests from the bottom to the top.

We have test many benchmarks on a sharedIO system, and the results show that the performance of some nodes is very anomalous. The performance of benchmarks below exists great difference under different communication environments(such as communication nodes and network adapter interference):


- RDMA benchmarks (perftest)
  - ib_send_bw, ib_read_bw and ib_write_bw
- MPI benchmarks (OSU & Ember)
  - OSU MPI Benchmarks 
    - MPI send & MPI recieve
    - MPI alltoall
  - Ember MPI Benchmarks
    - Halo3D, Halo3D-26, Sweep3D
- Mini applications
  - SFFT, CloveLeaf

# Process

## Phase I (July - August) ✓ Completed

Implementation on x86_64 platform. And the short paper will follow below implementation.

From node level:
- Inner-node computing performance evaluation
  - It is required that TPBench can serve as a wrapper for the operation of other programs. The high-precision timing feature of TPBench needs to be utilized.
  - Inner-node performance benchmarks
     - TPbench kernels (e.g., Roofline etc.)
     - Other benchmarks (e.g., STREAM etc.)
  - TPBench should be able to run conveniently among multiple node lists.
- Inter-node communication ability evaluation
  - This part should be bottom - up, and the following Benchmarks will be used.
    - RDMA: perftest
    - MPI: OSU/Ember
    - Application: some mini-applications
    - Wrapper (supports passing in the MPI running environment)

Therefore, we should focus on the following implementation:
- Wrapper:
  - Wrapper is a framework to run TPBench kernels and other benchmarks
- Hijacking MPI calls:
  - Record time consumption and communication information of each MPI call
  - Hijack different MPI calls to achieve synchronize processor's MPI calls (in different MPI global environment)
- Precise multi-core and multi-node synchronization:
  - How to achieve precise synchronization among processes
    - in a non-MPI environment
    - in different MPI global environments

Cases:
- SharedIO performance evaluation

### Benchmark Path

The realization path of tpwrapper follows three key components:
- interference pattern:
  - Resources Contention
- interference behavior:
  - symmetrical (sync):
    - two same mpirun processes
  - asymmetrical (trigger):
    - custom + preset(Communication/IO)
- metrics:
  - Performance Metrics


## Phase II (September - October)

- Implementation on arm platform
  - Change the MPI hijacking and synchronization implementation
- If we have time, we can also implement the cross analysis of the TPBench kernel and mini applications
  - For example, we use the RDMA and OSU benchmark's performance result to analyze the performance anomaly of mini applications such as FFT


# Structure

## Architecture Overview

The TPBench Wrapper consists of four main components:

```
TPBench/wrapper/
├── tpwrapper.py                 # Main Python framework for orchestrating multi-node execution
├── instrumentation/
│   └── mpi_trace.cpp           # MPI call interception and timing (libmpi_trace.so)
├── sync/
│   ├── sync.cpp                # RDMA-based synchronization implementation  
│   └── sync.h                  # Synchronization API (libtpbench_sync.so)
├── trigger/
│   ├── participant.cpp         # Participant process for asymmetrical testing
│   ├── bench_mpi.cpp          # MPI benchmarks (allreduce, send/recv, etc.)
│   ├── bench_rdma.cpp         # RDMA benchmarks (read, write, send)
│   ├── bench_io.cpp           # I/O benchmarks (read, write)
│   └── bench.h                # Common benchmark interface
├── bin/
│   └── participant            # Compiled participant executable
├── lib/
│   ├── libmpi_trace.so        # MPI tracing library
│   └── libtpbench_sync.so     # Synchronization library
└── test/                      # Test cases and examples
```

## Core Components

### 1. Python Framework (tpwrapper.py)

The main orchestration framework that:
- **Host Management**: Expands host expressions (e.g., `node[1-8]`) and distributes them across MPI processes
- **MPI Implementation Detection**: Auto-detects OpenMPI, Intel MPI, or MPICH for proper environment variable propagation
- **Process Orchestration**: Manages multiple `mpirun` processes with proper synchronization
- **Error Handling**: Implements retry mechanisms and TPSYNC error detection with process termination
- **Logging Management**: Organizes output logs by timestamp and host groups

Key functions:
- `run_parallel_symmetrical()`: Executes identical programs on different node groups with synchronization
- `run_parallel_asymmetrical()`: Executes coordinator program with participant interference programs
- `monitor_processes_with_error_detection()`: Monitors process execution and handles TPSYNC errors
- `build_mpirun_commands()`: Constructs MPI-specific command lines with environment variables

### 2. MPI Hijacking (instrumentation/mpi_trace.cpp)

The MPI interception library provides:
- **Function Interception**: Uses `LD_PRELOAD` to intercept MPI calls (Send, Recv, Bcast, Allreduce, etc.)
- **High-Precision Timing**: Utilizes TPBench's cycle-accurate timing (`tptimer.h`)
- **Synchronization Hooks**: Calls `rdmasync::tp_sync()` at each MPI operation for cross-process synchronization
- **Performance Logging**: Records timing, data size, and communication patterns to structured log files

Intercepted MPI functions:
```cpp
enum class MPI_FUNC {
    MPI_SEND, MPI_RECV, MPI_BCAST, MPI_SCATTER, 
    MPI_GATHER, MPI_ALLREDUCE, MPI_ALLTOALL, MPI_WAIT
};
```

Each intercepted call records:
- Wall-clock time and CPU cycles
- Data size and count
- Source/destination ranks
- MPI datatype information

### 3. Synchronization System (sync/)

#### sync.h API
```cpp
namespace rdmasync {
    void init(uint32_t mpi_rank=0);           // Initialize RDMA connection
    void finalize();                          // Cleanup resources
    void tp_sync(const char* name);           // Symmetric synchronization point
    TRIGGER_MSG tp_trigger(TRIGGER_MSG msg);  // Asymmetric trigger communication
    bool is_enabled();                        // Check if sync is active
}
```

#### sync.cpp Implementation
The synchronization system uses RDMA for ultra-low latency communication:

**Coordinator Role:**
- Binds to specified IP address and waits for participant connections
- Maintains connection state for all expected participants
- Sends synchronization signals and trigger messages

**Participant Role:**  
- Connects to coordinator IP address
- Waits for synchronization signals before proceeding
- Executes triggered benchmarks when commanded

**RDMA Communication Protocol:**
- Connection establishment with reliability checking
- Message passing for synchronization points
- Trigger command distribution (`ONCE`, `REPEAT`, `STOP`, `FINISH`)

### 4. Trigger System (trigger/)

#### Participant Framework (participant.cpp)
The participant process implements a state machine:

```cpp
class ParticipantBench {
    std::atomic<bool> running_{true};
    std::atomic<bool> repeat_mode_{false};
    std::string benchmark_type_;     // "mpi", "rdma", "io"
    std::string function_name_;      // specific function to execute
    size_t data_size_;              // benchmark data size
};
```

**State Transitions:**
- `ONCE`: Execute benchmark once and wait
- `REPEAT`: Execute benchmark continuously until `STOP`
- `STOP`: Halt current benchmark execution
- `FINISH`: Terminate participant process

#### Benchmark Implementations

**MPI Benchmarks (bench_mpi.cpp):**
- `allreduce`: MPI_Allreduce with configurable data sizes
- `sendrecv`: Point-to-point communication patterns
- `bcast`: Broadcast operations
- `alltoall`: All-to-all communication

**RDMA Benchmarks (bench_rdma.cpp):**
- `read`: RDMA read operations using perftest library
- `write`: RDMA write operations 
- `send`: RDMA send/receive operations
- Supports client-server modes with automatic IP resolution

**I/O Benchmarks (bench_io.cpp):**
- `read`: Sequential file reads with configurable sizes
- `write`: Sequential file writes
- `random`: Random I/O patterns
- Uses standard POSIX I/O for cross-platform compatibility

## Operational Modes 

### Symmetrical Mode (Synchronous)
**Use Case:** Evaluating performance impact of resource contention between identical workloads

**Configuration:**
```bash
python tpwrapper.py --tphosts node1,node2,node3,node4 --nmpi 2 --sync --trace \
    mpirun -n 80 -ppn 40 -hosts TPHOSTS ./application
```

**Execution Flow:**
1. **Host Distribution**: Wrapper divides hosts into groups (node1,node2) and (node3,node4)
2. **Synchronized Launch**: Both MPI processes start simultaneously with RDMA sync enabled
3. **Cross-Process Synchronization**: Each MPI call waits for corresponding call in peer process
4. **Performance Collection**: MPI timing data collected from both processes

**Environment Variables Set:**
```bash
# Process 1 (Coordinator)
RDMASYNC_ENABLE=1
RDMASYNC_ROLE=coordinator  
RDMASYNC_BIND_IP=<node1_ip>
RDMASYNC_SERVER_IP=<node1_ip>
RDMASYNC_EXPECTED=1
LD_PRELOAD=./libmpi_trace.so

# Process 2 (Participant)  
RDMASYNC_ENABLE=1
RDMASYNC_ROLE=participant
RDMASYNC_SERVER_IP=<node1_ip>
LD_PRELOAD=./libmpi_trace.so
```

### Asymmetrical Mode (Triggered Interference)
**Use Case:** Evaluating performance impact of specific interference patterns during application execution

**Configuration:**
```bash
python tpwrapper.py --tphosts node1,node2,node3,node4 --nmpi 2 --trigger \
    --trigger_program mpi_allreduce --trigger_program_size 1024 \
    mpirun -n 80 -ppn 40 -hosts TPHOSTS ./application
```

**Execution Flow:**
1. **Coordinator Setup**: Main application runs on first node group with sync hooks
2. **Participant Initialization**: Trigger participants start on remaining nodes, waiting for commands
3. **Trigger Activation**: When coordinator hits MPI call, `tp_trigger()` sends command to participants
4. **Interference Execution**: Participants execute specified benchmark (e.g., MPI_Allreduce with 1024 bytes)
5. **Synchronized Completion**: Coordinator waits for participants to complete before proceeding

**Trigger Commands:**
- `ONCE`: Execute benchmark once when triggered
- `REPEAT`: Execute benchmark continuously until `STOP` command
- `STOP`: Halt ongoing benchmark execution
- `FINISH`: Terminate participant processes

### Performance Analysis Capabilities

**Metrics Collected:**
1. **Timing Data**: 
   - Wall-clock time per MPI operation
   - CPU cycle counts for cycle-accurate measurement
   - Per-rank timing distributions

2. **Communication Patterns**:
   - Message sizes and counts
   - Source/destination mapping
   - Communication volume analysis

3. **Synchronization Overhead**:
   - RDMA synchronization latency
   - Cross-process waiting times
   - Network interference impact

**Log File Organization:**
```
tpwrapper_logs/YYYYMMDD_HHMMSS_microseconds/
├── coordinator_node1,node2.out       # Coordinator stdout
├── coordinator_node1,node2.err       # Coordinator stderr  
├── participant_node3,node4.out       # Participant stdout
└── participant_node3,node4.err       # Participant stderr

tpmpi_trace_logs/YYYYMMDD_HHMMSS_microseconds/
└── node1/
    ├── mpi_trace_rank_0.log          # Per-rank MPI call traces
    ├── mpi_trace_rank_1.log
    └── ...
```

**Error Handling and Reliability:**
- **TPSYNC Error Detection**: Monitors stderr for synchronization failures
- **Automatic Retry**: Configurable retry attempts for transient failures  
- **Process Cleanup**: Graceful termination of all processes on error
- **Timeout Management**: Prevents hanging processes during synchronization

## Advanced Features

### Multi-MPI Implementation Support
The wrapper automatically detects and configures environment variables for:
- **OpenMPI**: Uses `-x VAR=VALUE` format
- **Intel MPI/OneAPI**: Uses `-genv VAR VALUE` format  
- **MPICH**: Uses `-genv VAR VALUE` format

### Dynamic Host Management
```python
# Supports flexible host expressions
hosts = expand_hosts("node[1-8],gpu[01-04]")  
# Result: ["node1", "node2", ..., "node8", "gpu01", "gpu02", "gpu03", "gpu04"]

# Intelligent host chunking for load balancing
groups = chunk_hosts(hosts, nmpi=3, node_per_mpi=2, trigger_enabled=True)
# Result: [[coordinator_hosts], [participant_group1], [participant_group2]]
```

### Cross-Platform Compatibility
- **x86-64**: Full support with Intel MPI, OpenMPI, MPICH
- **ARM64**: Native support with architecture-specific optimizations
- **Cycle Counters**: Platform-specific high-resolution timing via TPBench

## Use Cases and Applications

### 1. Network Interference Analysis
**Scenario**: Evaluate how background MPI traffic affects application performance
```bash
# Run application with MPI_Allreduce interference
python tpwrapper.py --trigger --trigger_program mpi_allreduce \
    --trigger_program_size 1048576 ...
```

### 2. I/O Contention Studies  
**Scenario**: Measure impact of concurrent file system access
```bash
# Run application with I/O interference
python tpwrapper.py --trigger --trigger_program io_write \
    --trigger_program_size 1073741824 ...
```

### 3. Resource Contention Characterization
**Scenario**: Quantify performance degradation under resource sharing
```bash
# Run identical applications with sync
python tpwrapper.py --sync --nmpi 4 ...
```

### 4. Benchmark Validation
**Scenario**: Verify benchmark results across different interference conditions
- Baseline measurement (no interference)
- Controlled interference injection  
- Performance variance analysis

This comprehensive framework enables systematic study of HPC system performance under realistic multi-application workload conditions, providing insights into resource contention, network interference, and synchronization overhead impacts.

# Testing and Validation(To be fixed, just a placeholder)

## Test Infrastructure

The wrapper includes comprehensive testing capabilities in the `test/` directory:

### Test Categories

1. **Unit Tests**
   - Synchronization mechanism validation
   - MPI interception accuracy
   - Host expansion and chunking algorithms
   - Error detection and recovery

2. **Integration Tests**  
   - Multi-node synchronization
   - MPI trace collection
   - Trigger system functionality
   - Cross-platform compatibility

3. **Performance Tests**
   - Synchronization overhead measurement
   - Interference pattern validation
   - Scalability analysis

### Example Test Cases

#### Synchronization Proof Test
```bash
# test/sync/run_sync_proof_test.sh
# Validates RDMA-based synchronization across multiple nodes
python tpwrapper.py --tphosts node1,node2,node3,node4 --sync --nmpi 2 \
    mpirun -n 128 -ppn 64 -hosts TPHOSTS ./mpi_collective_test
```

#### Trigger Functionality Test  
```bash
# test_triggers.sh
# Tests asymmetrical trigger with MPI interference
timeout 40 python tpwrapper.py --tphosts node662,node663,node664,node665 \
    --sync --trace --nmpi 2 --trigger --trigger_program mpi_allreduce \
    --trigger_program_size 10 --merge_trace_files -v \
    mpirun -n 2 -ppn 1 -hosts TPHOSTS ./simple_mpi
```

#### Performance Comparison Test
```bash
# Baseline measurement
time mpirun -n 128 -ppn 64 -hosts node857,node858 ./mpi_collective

# With interference
time python tpwrapper.py --tphosts node857,node858,node859,node860 \
    --sync --trace --nmpi 2 --trigger --trigger_program mpi_sendrecv \
    --trigger_program_size 1024 --merge_trace_files \
    mpirun -n 128 -ppn 64 -hosts TPHOSTS ./mpi_collective
```

# TPSYNC Realization

The TPSYNC (TPBench Synchronization) system is implemented as a C++ library (`libtpbench_sync.so`) that provides RDMA-based synchronization primitives for multi-node coordination. The implementation follows a coordinator-participant model with ultra-low latency communication.

## Initialization

The `rdmasync::init(uint32_t mpi_rank)` function sets up the RDMA infrastructure and establishes connections between coordinator and participants.

### Environment Configuration
The initialization process is controlled by several environment variables:
- `RDMASYNC_ENABLE`: Must be "1" to enable synchronization
- `RDMASYNC_ROLE`: Specifies "coordinator", "participant", or "AUTO"
- `RDMASYNC_SERVER_IP`: IP address of the coordinator
- `RDMASYNC_BIND_IP`: IP address for coordinator to bind to
- `RDMASYNC_EXPECTED`: Number of participants expected by coordinator
- `RDMASYNC_PORT`: TCP port for RDMA connection (default: 2673)
- `RDMASYNC_DEBUG`: Enable debug output if set to "1"


### Coordinator Initialization (`setup_coordinator()`)

The coordinator process performs the following steps:

1. **RDMA Event Channel Setup**:
   ```cpp
   ec_ = rdma_create_event_channel();
   rdma_create_id(ec_, &listen_id_, nullptr, RDMA_PS_TCP);
   ```

2. **Address Binding and Listening**:
   ```cpp
   sockaddr_in sin{};
   sin.sin_family = AF_INET;
   sin.sin_port = htons((uint16_t)std::atoi(port_.c_str()));
   if (!bind_ip_.empty() && bind_ip_ != "0.0.0.0") {
       inet_pton(AF_INET, bind_ip_.c_str(), &sin.sin_addr);
   } else {
       sin.sin_addr.s_addr = INADDR_ANY;
   }
   rdma_bind_addr(listen_id_, (sockaddr*)&sin);
   rdma_listen(listen_id_, expected_);
   ```

3. **Connection Acceptance with Timeout**:
   ```cpp
   // Get coordinator timeout from environment (default 5 seconds)
   const char* timeout_env = std::getenv("RDMASYNC_COORDINATOR_TIMEOUT");
   int coordinator_timeout_ms = timeout_env ? std::atoi(timeout_env) : 5000;
   
   std::vector<rdma_cm_id*> pending_ids;
   for (int i = 0; i < expected_; ++i) {
       rdma_cm_event* event = nullptr;
       if (rdma_get_cm_event_timeout(ec_, &event, coordinator_timeout_ms) == 0) {
           if (event->event == RDMA_CM_EVENT_CONNECT_REQUEST) {
               pending_ids.push_back(event->id);
               rdma_ack_cm_event(event);
           }
       }
   }
   ```

4. **Queue Pair Creation and Memory Registration**:
   ```cpp
   // Initialize shared PD and CQ for all connections
   if (!pending_ids.empty()) {
       pd_ = ibv_alloc_pd(pending_ids[0]->verbs);
       cq_ = ibv_create_cq(pending_ids[0]->verbs, 1024, nullptr, nullptr, 0);
   }
   
   // Setup each connection
   for (rdma_cm_id* id : pending_ids) {
       ibv_qp_init_attr qp_attr{};
       qp_attr.send_cq = cq_;
       qp_attr.recv_cq = cq_;
       qp_attr.qp_type = IBV_QPT_RC;
       qp_attr.cap.max_send_wr = 64;
       qp_attr.cap.max_recv_wr = 64;
       qp_attr.cap.max_send_sge = 1;
       qp_attr.cap.max_recv_sge = 1;
       rdma_create_qp(id, pd_, &qp_attr);
   }
   ```

5. **Connection Establishment and Flag Exchange**: Waits for ESTABLISHED events and receives participant flag information

### Participant Initialization (`setup_participant()`)
Each participant connects to the coordinator:

1. **Address Resolution and Retry Logic**:
   ```cpp
   addrinfo hints{}, *res = nullptr;
   hints.ai_family = AF_INET; 
   hints.ai_socktype = SOCK_STREAM;
   getaddrinfo(server_ip_.c_str(), port_.c_str(), &hints, &res);
   
   const int kMaxAttempts = 40;
   int attempt = 0;
   bool connected = false;
   
   while (attempt++ < kMaxAttempts && !connected) {
       // Connection attempt with exponential backoff
       usleep(attempt * 1000); // Increasing delay
   }
   ```

2. **Memory Allocation with mmap**:
   ```cpp
   size_t ctrl_size = std::max(sizeof(MsgPart2C), sizeof(MsgEpoch));
   
   ctrl_mem_ = mmap(nullptr, ctrl_size, PROT_READ|PROT_WRITE, 
                    MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
   if (ctrl_mem_ == MAP_FAILED) {
       ERRORPRINT("Failed to allocate ctrl memory", errno);
       return false;
   }
   
   flag_mem_ = mmap(nullptr, 8, PROT_READ|PROT_WRITE, 
                    MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
   if (flag_mem_ == MAP_FAILED) {
       ERRORPRINT("Failed to allocate flag memory", errno);
       return false;
   }
   *(volatile uint64_t*)flag_mem_ = 0; // Initialize flag
   ```

3. **RDMA Connection Establishment**:
   ```cpp
   rdma_create_id(ec_, &id_p_, nullptr, RDMA_PS_TCP);
   rdma_resolve_addr(id_p_, nullptr, res->ai_addr, 1000);
   
   // Wait for address resolution
   rdma_cm_event* event = nullptr;
   rdma_get_cm_event_timeout(ec_, &event, 1000);
   rdma_ack_cm_event(event);
   
   // Resolve route and create QP
   rdma_resolve_route(id_p_, 1000);
   // ... QP creation similar to coordinator
   ```

4. **Memory Registration and Flag Information Exchange**:
   ```cpp
   mr_ctrl_send_ = ibv_reg_mr(pd_, ctrl_mem_, ctrl_size, IBV_ACCESS_LOCAL_WRITE);
   mr_ctrl_recv_ = ibv_reg_mr(pd_, ctrl_mem_, ctrl_size, IBV_ACCESS_LOCAL_WRITE);
   mr_flag_ = ibv_reg_mr(pd_, flag_mem_, 8, 
                         IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
   
   // Send flag info to coordinator
   MsgPart2C* flag_msg = reinterpret_cast<MsgPart2C*>(ctrl_mem_);
   flag_msg->flag_addr = (uint64_t)flag_mem_;
   flag_msg->flag_rkey = mr_flag_->rkey;
   
   ibv_send_wr wr{};
   wr.opcode = IBV_WR_SEND;
   wr.send_flags = IBV_SEND_SIGNALED;
   ibv_post_send(id_p_->qp, &wr, &bad);
   ```

## tp_sync Implementation

The `tp_sync(const char* name)` function provides symmetric synchronization points where all processes wait for each other before proceeding.

### Synchronization Protocol

**Participant Side Implementation**:
```cpp
void tp_sync(const char* /*name*/) {
    if (!enabled_) return;
    
    // Passive participants (rank > 0) just return
    if (is_passive_participant_) return;
    
    uint64_t e = epoch_.fetch_add(1, std::memory_order_relaxed) + 1;
    
    // Send epoch notification to coordinator
    MsgEpoch* msg = reinterpret_cast<MsgEpoch*>(ctrl_mem_);
    msg->epoch = e;
    
    ibv_sge sge{ 
        .addr = (uintptr_t)msg, 
        .length = (uint32_t)sizeof(*msg), 
        .lkey = mr_ctrl_send_->lkey 
    };
    ibv_send_wr wr{}, *bad = nullptr;
    wr.opcode = IBV_WR_SEND;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.sg_list = &sge; 
    wr.num_sge = 1;
    
    DBGPRINT("Participant sending for epoch flag", e, debug_);
    int ret = ibv_post_send(id_p_->qp, &wr, &bad);
    if (ret != 0) {
        ERRORPRINT("Failed to post send", ret);
        return;
    }
    
    // Wait for send completion
    poll_cq_one(cq_);
    
    // Busy-wait for coordinator's GO signal (RDMA write to flag)
    DBGPRINT("Participant waiting for epoch flag", e, debug_);
    volatile uint64_t* flag = (volatile uint64_t*) flag_mem_;
    while (__builtin_expect(*flag != e, 1)) {
        cpu_relax();  // Platform-specific CPU relaxation
        usleep(spin_us_); // Optional microsecond sleep
    }
    DBGPRINT("Participant tp_sync completed", e, debug_);
}
```

**Coordinator Side Implementation**:
```cpp
void tp_sync(const char* /*name*/) {
    uint64_t e = epoch_.fetch_add(1, std::memory_order_relaxed) + 1;
    
    // Ensure enough RECV work requests are posted
    int need_to_post = expected_ - posted_recvs_;
    ensure_posted_recvs(need_to_post);
    
    // Wait for all participants to send epoch notifications
    int got = 0;
    long long poll_attempts = 0;
    const long long max_poll_attempts = 6000000000; // Timeout protection
    
    while (got < expected_ && poll_attempts < max_poll_attempts) {
        ibv_wc wc{};
        int n = ibv_poll_cq(cq_, 1, &wc);
        poll_attempts++;
        
        if (n < 0) {
            ERRORPRINT("CQ poll error", n);
            break;
        }
        if (n == 0) continue;
        
        if (wc.status != IBV_WC_SUCCESS) {
            WARNINGPRINT("Work completion failed", wc.status, debug_);
            continue;
        }
        if (wc.opcode == IBV_WC_RECV) {
            DBGPRINT("Received epoch notification", got, debug_);
            got++;
        }
    }
    
    if (poll_attempts >= max_poll_attempts) {
        ERRORPRINT("Timeout waiting for RECVs after attempts", poll_attempts);
        return;
    }
    
    posted_recvs_ -= expected_;
    
    // Send GO signal to all participants via RDMA write
    for (auto& c : conns_) {
        uint64_t val = e;
        
        ibv_sge sge{ 
            .addr = (uintptr_t)&val, 
            .length = (uint32_t)sizeof(val), 
            .lkey = 0  // Inline data, no local key needed
        };
        ibv_send_wr wr{}, *bad = nullptr;
        wr.opcode = IBV_WR_RDMA_WRITE;
        wr.send_flags = IBV_SEND_SIGNALED | IBV_SEND_INLINE;
        wr.wr.rdma.remote_addr = c.part_info.flag_addr;
        wr.wr.rdma.rkey = c.part_info.flag_rkey;
        wr.sg_list = &sge;
        wr.num_sge = 1;
        
        DBGPRINT("RDMA WRITE: flag_addr", c.part_info.flag_addr, debug_);
        int ret = ibv_post_send(c.qp, &wr, &bad);
        if (ret != 0) {
            ERRORPRINT("Failed to post RDMA write", ret);
        }
    }
    
    // Wait for all RDMA writes to complete
    int done = 0;
    while (done < expected_) {
        ibv_wc wc{};
        int n = ibv_poll_cq(cq_, 1, &wc);
        if (n > 0 && wc.status == IBV_WC_SUCCESS) {
            done++;
        }
    }
    DBGPRINT("All RDMA WRITEs completed", done, debug_);
}
```

### Key Features:
- **Epoch-based Coordination**: Uses atomic epoch counters to ensure proper ordering
- **RDMA Write for Signaling**: Ultra-low latency notification via direct memory writes with inline data
- **Busy-wait Polling**: Minimizes latency through active polling with `cpu_relax()` and optional microsleep
- **Memory Barriers**: Ensures proper memory ordering across NUMA boundaries
- **Timeout Protection**: Prevents infinite waits with configurable timeout limits
- **Passive Participant Support**: Handles non-rank-0 participants that don't need RDMA operations
- **Work Request Management**: Pre-posts RECV operations and manages completion queue efficiently

## tp_trigger Implementation

The `tp_trigger(TRIGGER_MSG send_msg)` function implements asymmetric communication for interference injection.

### Trigger Message Types
```cpp
enum class TRIGGER_MSG {
    ONCE = 1,      // Execute benchmark once
    REPEAT = 2,    // Execute continuously  
    STOP = 3,      // Stop current execution
    FINISH = 4,    // Terminate participant
    UNKOWN = 0     // Error/unknown state
};
```

### Coordinator Trigger Sending
```cpp
TRIGGER_MSG tp_trigger(TRIGGER_MSG send_msg) {
    if (!enabled_) {
        WARNINGPRINT("RDMASync not enabled, skipping tp_trigger", "", debug_);
        return TRIGGER_MSG::UNKOWN;
    }
    
    if (is_passive_participant_) return TRIGGER_MSG::UNKOWN;
    
    if (role_ == "coordinator") {
        DBGPRINT("Coordinator sending trigger message", (int)send_msg, debug_);
        
        // Send trigger command to all participants
        for (auto& c : conns_) {
            MsgC2P* msg = reinterpret_cast<MsgC2P*>(ctrl_mem_);
            msg->flag_addr = c.part_info.flag_addr;     // For verification
            msg->flag_rkey = c.part_info.flag_rkey;     // For verification
            msg->send_msg = send_msg;                   // Actual command
            
            ibv_sge sge{ 
                .addr = (uintptr_t)msg, 
                .length = (uint32_t)sizeof(*msg), 
                .lkey = mr_ctrl_send_->lkey 
            };
            ibv_send_wr wr{}, *bad = nullptr;
            wr.opcode = IBV_WR_SEND;
            wr.send_flags = IBV_SEND_SIGNALED;
            wr.sg_list = &sge;
            wr.num_sge = 1;
            
            int ret = ibv_post_send(c.qp, &wr, &bad);
            if (ret != 0) {
                ERRORPRINT("Failed to send trigger", ret);
                return TRIGGER_MSG::UNKOWN;
            }
        }
        
        // Wait for send completions
        int sent = 0;
        while (sent < expected_) {
            if (poll_cq_one_timeout(cq_, 1000)) {
                sent++;
            } else {
                ERRORPRINT("Timeout waiting for trigger send completion", sent);
                return TRIGGER_MSG::UNKOWN;
            }
        }
        
        // Wait for acknowledgments from all participants
        bool success = wait_for_responses();
        DBGPRINT("Trigger response received", success, debug_);
        return success ? send_msg : TRIGGER_MSG::UNKOWN;
    }
    
    return TRIGGER_MSG::UNKOWN;
}
```

### Participant Trigger Handling
```cpp
TRIGGER_MSG tp_trigger(TRIGGER_MSG send_msg) {
    if (role_ == "participant") {
        DBGPRINT("Participant waiting for trigger message", "", debug_);
        
        // Ensure RECV is posted to receive trigger message
        if (posted_recvs_ <= 0) {
            ensure_posted_recvs(1);
        }
        
        // Wait for trigger message from coordinator
        ibv_wc wc{};
        int poll_attempts = 0;
        const int max_attempts = 10000000;
        
        while (poll_attempts < max_attempts) {
            int n = ibv_poll_cq(cq_, 1, &wc);
            poll_attempts++;
            
            if (n > 0 && wc.status == IBV_WC_SUCCESS && wc.opcode == IBV_WC_RECV) {
                break;
            }
            if (n < 0) {
                ERRORPRINT("CQ poll error in trigger", n);
                return TRIGGER_MSG::UNKOWN;
            }
            usleep(1); // Small delay to prevent CPU spinning
        }
        
        if (poll_attempts >= max_attempts) {
            ERRORPRINT("Timeout waiting for trigger message", poll_attempts);
            return TRIGGER_MSG::UNKOWN;
        }
        
        posted_recvs_--;
        
        // Extract trigger command from received message
        MsgC2P* msg = reinterpret_cast<MsgC2P*>(ctrl_mem_);
        TRIGGER_MSG received_msg = msg->send_msg;
        
        DBGPRINT("Participant received trigger", (int)received_msg, debug_);
        
        // Send acknowledgment back to coordinator
        ibv_sge sge{ 
            .addr = (uintptr_t)&received_msg, 
            .length = (uint32_t)sizeof(received_msg), 
            .lkey = mr_ctrl_send_->lkey 
        };
        ibv_send_wr wr{}, *bad = nullptr;
        wr.opcode = IBV_WR_SEND;
        wr.send_flags = IBV_SEND_SIGNALED;
        wr.sg_list = &sge;
        wr.num_sge = 1;
        
        int ret = ibv_post_send(id_p_->qp, &wr, &bad);
        if (ret != 0) {
            ERRORPRINT("Failed to send trigger ack", ret);
            return TRIGGER_MSG::UNKOWN;
        }
        
        // Wait for send completion
        poll_cq_one(cq_);
        
        DBGPRINT("Participant sent trigger acknowledgment", (int)received_msg, debug_);
        return received_msg;
    }
    
    return TRIGGER_MSG::UNKOWN;
}
```

### Helper Function: wait_for_responses()
```cpp
bool wait_for_responses() {
    if (!enabled_ || role_ != "coordinator") return false;
    
    // Ensure enough RECVs are posted for responses
    int need_to_post = expected_ - posted_recvs_;
    if (need_to_post > 0) {
        ensure_posted_recvs(need_to_post);
    }
    
    int responses_received = 0;
    const int timeout_ms = 5000;
    auto start_time = std::chrono::steady_clock::now();
    
    while (responses_received < expected_) {
        ibv_wc wc{};
        int n = ibv_poll_cq(cq_, 1, &wc);
        
        if (n > 0 && wc.status == IBV_WC_SUCCESS && wc.opcode == IBV_WC_RECV) {
            responses_received++;
            DBGPRINT("Received response", responses_received, debug_);
        }
        
        // Check timeout
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - start_time).count();
        if (elapsed >= timeout_ms) {
            ERRORPRINT("Timeout waiting for responses", responses_received);
            return false;
        }
        
        usleep(100); // Small delay
    }
    
    posted_recvs_ -= expected_;
    return true;
}
```

### Protocol Features:
- **Command-Response Pattern**: Coordinator sends commands, participants acknowledge with timeout protection
- **Structured Messaging**: Uses packed structs (`MsgC2P`, `MsgEpoch`) for efficient wire protocol
- **Reliability**: Waits for acknowledgments to ensure message delivery with configurable timeouts
- **State Validation**: Includes flag address/rkey for connection validation and debugging
- **Error Handling**: Comprehensive error checking with detailed logging and graceful degradation
- **Work Request Management**: Proper RECV posting and completion queue management
- **Performance Monitoring**: Debug output for latency and throughput analysis

## Finalization

The `rdmasync::finalize()` function performs cleanup of all RDMA resources and connections.

### Cleanup Sequence

**Participant Cleanup**:
```cpp
void finalize() {
    if (!enabled_) return;
    
    // Passive participants don't have RDMA resources
    if (is_passive_participant_) {
        enabled_ = false;
        return;
    }
    
    if (role_ == "participant") {
        // Destroy RDMA connection and queue pair
        if (id_p_) {
            if (id_p_->qp) {
                rdma_destroy_qp(id_p_);
            }
            rdma_destroy_id(id_p_);
            id_p_ = nullptr;
        }
        
        // Clean up event channel
        if (ec_) {
            rdma_destroy_event_channel(ec_);
            ec_ = nullptr;
        }
        
        // Deregister and unmap memory regions
        if (mr_ctrl_send_) { ibv_dereg_mr(mr_ctrl_send_); mr_ctrl_send_ = nullptr; }
        if (mr_ctrl_recv_) { ibv_dereg_mr(mr_ctrl_recv_); mr_ctrl_recv_ = nullptr; }
        if (mr_flag_) { ibv_dereg_mr(mr_flag_); mr_flag_ = nullptr; }
        
        // Clean up protection domain and completion queue
        if (cq_) { ibv_destroy_cq(cq_); cq_ = nullptr; }
        if (pd_) { ibv_dealloc_pd(pd_); pd_ = nullptr; }
        
        // Unmap memory regions
        if (flag_mem_) { 
            munmap(flag_mem_, 8); 
            flag_mem_ = nullptr; 
        }
        if (ctrl_mem_) { 
            size_t ctrl_size = std::max(sizeof(MsgPart2C), sizeof(MsgEpoch));
            munmap(ctrl_mem_, ctrl_size); 
            ctrl_mem_ = nullptr; 
        }
    }
    enabled_ = false;
}
```

**Coordinator Cleanup**:
```cpp
void finalize() {
    if (role_ == "coordinator") {
        // Clean up all participant connections
        for (auto& c : conns_) {
            // Destroy queue pairs
            if (c.qp) {
                ibv_destroy_qp(c.qp);
                c.qp = nullptr;
            }
            
            // Destroy connection IDs
            if (c.id) {
                rdma_destroy_id(c.id);
                c.id = nullptr;
            }
            
            // Deregister memory regions
            if (c.mr_ctrl_recv) {
                ibv_dereg_mr(c.mr_ctrl_recv);
                c.mr_ctrl_recv = nullptr;
            }
            if (c.mr_ctrl_send) {
                ibv_dereg_mr(c.mr_ctrl_send);
                c.mr_ctrl_send = nullptr;
            }
        }
        conns_.clear();
        
        // Clean up shared resources
        if (cq_) { 
            ibv_destroy_cq(cq_); 
            cq_ = nullptr; 
        }
        if (pd_) { 
            ibv_dealloc_pd(pd_); 
            pd_ = nullptr; 
        }
        
        // Clean up listen socket and event channel
        if (listen_id_) {
            rdma_destroy_id(listen_id_);
            listen_id_ = nullptr;
        }
        if (ec_) {
            rdma_destroy_event_channel(ec_);
            ec_ = nullptr;
        }
        
        // Reset state variables
        expected_ = 0;
        posted_recvs_ = 0;
    }
    enabled_ = false;
}
```