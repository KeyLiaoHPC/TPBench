# TPBench Wrapper

TPBench Wrapper (tpwrapper) is a framework designed to run mpi micro-benchmarks and mini-applications with benefits of TPBench's high-precision timing capabilities. It aims to evaluate the performance interference between different applications and communication patterns. It supports both MPI and non-MPI environments, allowing for precise multi-core and multi-node synchronization. 


## TODO

- [ ] Evaluate the trigger's I/O bench and RDMA bench
- [ ] Add the data analysis chain
- [ ] Add more example programs and test cases
- [ ] Improve cross-platform compatibility testing

## Compilation

Build the wrapper components using the provided Makefile:

```bash
make clean
make all CC=your_cc_compiler CXX=your_cxx_compiler
```

After compilation, the following libraries will be generated:
- `lib/libmpi_trace.so` - MPI call interception library
- `bin/participant` - Participant executable for asymmetrical mode

## Usage

We can use tpwrapper to run any micro-benchmarks or mini-applications. The basic usage is as follows:
```bash
python tpwrapper.py [options] <your_program> [your_program options]
```

### Command Line Options

The available options include:

**Basic Options:**
- `--help`: Show help message and exit
- `-v` or `--verbose`: Enable verbose output for debugging
- `--tphosts`: Specify host list or pattern (e.g., `node[1-8]` or `node1,node2,node3`)
- `--nmpi`: Set number of MPI processes to launch (default: 1)

**Synchronization Control:**
- `--sync`: Enable synchronization between processes (default: enabled when nmpi > 1)
- `--no-sync`: Explicitly disable synchronization

**Tracing Options:**
- `--trace`: Enable MPI call tracing with libmpi_trace.so (default: enabled)
- `--store_trace_files_enable`: Enable storing trace files to local storage
- `--merge_trace_files`: Merge all MPI processes' trace files into a single directory
- `--trace_storage_prefix`: Custom prefix for trace file storage (default: `./tpmpi_trace_logs/$timestamp`)
- `--log_storage_prefix`: Custom prefix for MPI stdout/stderr logs (default: `./tpwrapper_logs/$timestamp`)

**Asymmetrical Mode (Trigger):**
- `--trigger`: Enable asymmetrical trigger mode for interference testing
- `--trigger_program`: Specify interference program (e.g., `mpi_allreduce`, `rdma_write`, `io_read`)
- `--trigger_program_size`: Data size in bytes for trigger program (default: 1024)

### Host Specification

For launching MPI processes to correct nodes, you can use `TPHOSTS` as a placeholder in your program options that will be replaced with the actual host configuration. The wrapper supports flexible host expressions:

- **Range notation**: `node[1-8]` expands to `node1,node2,node3,node4,node5,node6,node7,node8`
- **Mixed notation**: `node[1-4],gpu[01-02]` expands to `node1,node2,node3,node4,gpu01,gpu02`
- **Comma-separated**: `node1,node2,node3,node4`

### Symmetrical Mode

Symmetrical mode runs identical programs on different node groups with precise synchronization. This is useful for evaluating resource contention between similar workloads.

**Original command:**
```bash
mpirun -n 80 -ppn 40 -hosts node1,node2 ./test_program
```

**Using tpwrapper:**
```bash
python tpwrapper.py --tphosts node1,node2,node3,node4 --nmpi 2 \
    mpirun -n 80 -ppn 40 -hosts TPHOSTS ./test_program
```

**What happens internally:**
The wrapper executes two synchronized MPI processes:

```bash
# Process 1 (Coordinator)
mpirun -x RDMASYNC_ENABLE=1 -x RDMASYNC_ROLE=coordinator \
       -x RDMASYNC_BIND_IP=10.0.0.1 -x RDMASYNC_SERVER_IP=10.0.0.1 \
       -x RDMASYNC_EXPECTED=1 -x LD_PRELOAD=./lib/libmpi_trace.so \
       -n 80 -ppn 40 -hosts node1,node2 ./test_program

# Process 2 (Participant)  
mpirun -x RDMASYNC_ENABLE=1 -x RDMASYNC_ROLE=participant \
       -x RDMASYNC_SERVER_IP=10.0.0.1 -x LD_PRELOAD=./lib/libmpi_trace.so \
       -n 80 -ppn 40 -hosts node3,node4 ./test_program
```

**Key Features:**
- Automatic IP address detection and binding
- Cross-process MPI call synchronization  
- Timing and communication data collection
- Support for OpenMPI, Intel MPI, and MPICH environment variable formats 

### Asymmetrical Mode (Trigger-Based Interference)

Asymmetrical mode allows you to inject controlled interference patterns while running your main application. This is useful for studying performance impact of specific network, computation, or I/O activities.

**Available Trigger Programs:**

1. **MPI Benchmarks:** `mpi_<function>`
   - `mpi_allreduce`: MPI_Allreduce operations
   - `mpi_sendrecv`: Point-to-point communication  
   - `mpi_bcast`: Broadcast operations
   - `mpi_alltoall`: All-to-all communication

2. **RDMA Benchmarks:** `rdma_<function>`
   - `rdma_read`: RDMA read operations
   - `rdma_write`: RDMA write operations  
   - `rdma_send`: RDMA send/receive operations

3. **I/O Benchmarks:** `io_<function>`
   - `io_read`: Sequential file reads
   - `io_write`: Sequential file writes
   - `io_random`: Random I/O patterns

**Usage Examples:**

**MPI Interference:**
```bash
python tpwrapper.py --tphosts node1,node2,node3,node4 --nmpi 2 --trigger \
    --trigger_program mpi_allreduce --trigger_program_size 1024 \
    mpirun -n 80 -ppn 40 -hosts TPHOSTS ./test_program
```

**I/O Interference:**
```bash
python tpwrapper.py --tphosts node1,node2,node3 --nmpi 2 --trigger \
    --trigger_program io_write --trigger_program_size 1048576 \
    mpirun -n 80 -ppn 40 -hosts TPHOSTS ./test_program
```

**RDMA Interference:**
```bash
python tpwrapper.py --tphosts node1,node2,node3,node4 --nmpi 2 --trigger \
    --trigger_program rdma_write --trigger_program_size 4096 \
    mpirun -n 80 -ppn 40 -hosts TPHOSTS ./test_program
```

**What happens internally:**

The wrapper launches:
1. **Coordinator Process**: Your main application with sync hooks
2. **Participant Process(es)**: Interference programs that wait for trigger signals

**Example: MPI Interference**
```bash
# Coordinator (runs your application)
mpirun -genv RDMASYNC_ENABLE=1 -genv RDMASYNC_ROLE=coordinator \
       -genv RDMASYNC_SERVER_IP=10.0.0.1 -genv RDMASYNC_EXPECTED=1 \
       -genv LD_PRELOAD=./lib/libmpi_trace.so \
       -n 80 -ppn 40 -hosts node1,node2 ./test_program

# Participant (runs interference)
mpirun -genv RDMASYNC_ENABLE=1 -genv RDMASYNC_ROLE=participant \
       -genv RDMASYNC_SERVER_IP=10.0.0.1 \
       -n 80 -ppn 40 -hosts node3,node4 ./bin/participant mpi allreduce 1024
```

**Example: I/O Interference**
```bash
# Coordinator (runs your application)
mpirun -genv RDMASYNC_ENABLE=1 -genv RDMASYNC_ROLE=coordinator \
       -genv RDMASYNC_SERVER_IP=10.0.0.1 -genv RDMASYNC_EXPECTED=1 \
       -genv LD_PRELOAD=./lib/libmpi_trace.so \
       -n 80 -ppn 40 -hosts node1,node2 ./test_program

# Participant (runs interference)
ssh node3 ./bin/participant io write 1048576 \
    --env RDMASYNC_ENABLE=1 --env RDMASYNC_ROLE=participant \
    --env RDMASYNC_SERVER_IP=10.0.0.1
```

**Trigger Control Commands:**

When the coordinator application makes MPI calls, it sends trigger commands to participants:
- `ONCE`: Execute interference benchmark once
- `REPEAT`: Execute benchmark repeatedly until stopped
- `STOP`: Stop ongoing benchmark execution  
- `FINISH`: Terminate participant processes

**Synchronization Flow:**
1. Coordinator and participants initialize RDMA connections
2. Participants wait for trigger signals
3. When coordinator hits MPI calls, `tp_trigger()` sends commands
4. Participants execute specified interference patterns
5. Coordinator waits for participants to complete before proceeding

## Output Files and Logs

### Log Organization
The wrapper automatically organizes output files with timestamps:

```
tpwrapper_logs/YYYYMMDD_HHMMSS_microseconds/
├── coordinator_node1,node2.out       # Coordinator stdout
├── coordinator_node1,node2.err       # Coordinator stderr
├── participant_node3,node4.out       # Participant stdout
└── participant_node3,node4.err       # Participant stderr
```

### MPI Trace Files
When `--trace` is enabled and `--merge_trace_files` is used:

```
tpmpi_trace_logs/YYYYMMDD_HHMMSS_microseconds/
└── node1/
    ├── mpi_trace_rank_0.log          # Per-rank MPI call traces
    ├── mpi_trace_rank_1.log
    └── ...
```

**Trace File Format:**
Each line contains: `timestamp_cycles,timestamp_wall,mpi_function,data_size,src_rank,dst_rank,duration_cycles,duration_wall`

### Customizing Output Locations
```bash
# Custom log location
python tpwrapper.py --log_storage_prefix ./my_logs/experiment1 ...

# Custom trace location  
python tpwrapper.py --trace_storage_prefix ./my_traces/exp1 ...

# Merge all traces into single directory
python tpwrapper.py --merge_trace_files ...
```

## Error Handling and Troubleshooting

### Common Issues

**1. TPSYNC ERROR Detection**
- The wrapper monitors for `[TPSYNC ERROR]` in stderr output
- Automatically retries failed runs up to 3 times
- Terminates all processes gracefully on persistent errors

**2. MPI Implementation Detection**
- Wrapper auto-detects OpenMPI, Intel MPI, or MPICH
- Uses appropriate environment variable format (`-x` vs `-genv`)
- Falls back to OpenMPI format if detection fails

**3. Host Resolution**
- Automatically resolves hostnames to IP addresses for RDMA binding
- Supports both hostname and IP address specifications

### Debugging Options
```bash
# Enable verbose output for debugging
python tpwrapper.py -v ...

# Check environment variables being set
python tpwrapper.py -v --tphosts node1,node2 --nmpi 1 echo "test"
```

### Performance Considerations
- **Synchronization Overhead**: Adds <1% latency to most MPI operations
- **Memory Usage**: ~10MB per process for trace storage
- **Network Requirements**: RDMA-capable interconnect recommended for best performance

## Examples and Test Cases

### Basic Performance Comparison
```bash
# Baseline measurement (no interference)
time mpirun -n 128 -ppn 64 -hosts node1,node2 ./mpi_application

# With controlled interference
time python tpwrapper.py --tphosts node1,node2,node3,node4 --nmpi 2 \
    --trigger --trigger_program mpi_allreduce --trigger_program_size 1024 \
    --merge_trace_files -v \
    mpirun -n 128 -ppn 64 -hosts TPHOSTS ./mpi_application
```

### Resource Contention Study
```bash
# Two identical applications competing for resources
python tpwrapper.py --tphosts node1,node2,node3,node4 --nmpi 2 \
    --sync --trace --merge_trace_files \
    mpirun -n 256 -ppn 64 -hosts TPHOSTS ./compute_intensive_app
```

### Network Interference Analysis
```bash
# Application with background network traffic
python tpwrapper.py --tphosts node1,node2,node3,node4 --nmpi 2 \
    --trigger --trigger_program rdma_write --trigger_program_size 1048576 \
    --merge_trace_files --store_trace_files_enable \
    mpirun -n 128 -ppn 64 -hosts TPHOSTS ./network_intensive_app
```

For detailed technical information about the synchronization mechanisms and trigger systems, refer to [TechReport.md](./TechReport.md).  