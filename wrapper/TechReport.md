
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

## Phase I (July - August)

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

## Phase II (September - October)

- Implementation on arm platform
  - Change the MPI hijacking and synchronization implementation
- If we have time, we can also implement the cross analysis of the TPBench kernel and mini applications
  - For example, we use the RDMA and OSU benchmark's performance result to analyze the performance anomaly of mini applications such as FFT


# Realization

## Wrapper

The wrapper's structure is as follows:
```bash
TPBench/wrapper
├── instrumentation
│   ├── mpi_trace.cpp        # MPI hijacking implementation
├── sync
│   ├── sync.cpp             # Synchronization implementation
│   ├── sync.h               # Header file for synchronization
├── Makefile                 # Makefile to build the wrapper and libraries
└── wrapper                  # The script to use the wrapper
```

The usage of the wrapper can be found in [README.md](./README.md).

## MPI Hijacking

The MPI hijacking implementation is in `instrumentation/mpi_trace.cpp`. It provides a shared library `libmpi_trace.so` that can be used with `LD_PRELOAD` to intercept MPI calls. The library records the time consumption and communication information of each MPI call.


## Synchronization


The synchronization implementation is in `sync/sync.cpp` and `sync/sync.h`. It is the core component of the wrapper. It provides precise multi-core and multi-node synchronization in both MPI and non-MPI environments. The synchronization can be enabled or disabled via the wrapper options.

The synchronization mechanism uses RDMA for low-latency communication between processes. It supports two roles: coordinator and participant. The coordinator is responsible for managing the synchronization, while participants synchronize their state with the coordinator.




# Test