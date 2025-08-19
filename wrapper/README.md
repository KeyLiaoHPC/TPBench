# TPBench Wrapper

TPBench Wrapper (tpwrapper) is a framework designed to run any micro-benchmarks and mini-applications with benefits of TPBench's high-precision timing capabilities. It aims to evaluate the performance of 
It supports both MPI and non-MPI environments, allowing for precise multi-core and multi-node synchronization. 


## Compilation


```bash
make clean
make all
```

After compilation, there will be `libmpi_trace.so` and `libtpbench_sync.so` in the current directory.

## Usage

We can use tpwrapper to run any micro-benchmarks or mini-applications. The basic usage is as follows:
```bash
python tpwrapper.py [options] <your_program> [your_program options]
```

The [options] can include:
- `-h` or `--help`: Show help message
- `-v` or `--verbose`: Enable verbose output
- `--nmpi`: Set number of mpirun processes
- `--configfile`: Use a specific configuration file
- `--hosts`: Specify host file for multi-node execution
- `--sync`: Enable synchronization (default is enabled)
- `--trace`: Enable tracing (default is enabled)

For launching mpirun processes to correct nodes, you can use `TPHOSTS` to replace the host input with the host configurationin your program options. We list two examples below.

### symmetrical mode

The original command can be:
```bash
# mpirun cmd
mpirun -n 80 -ppn 40 --hosts node1,node2 ./test_program

```

You can change it to:

```bash
# mpirun cmd
python tpwrapper.py --hosts node1,node2,node3,node4 --nmpi 2 mpirun -n 80 -ppn 40 --hosts TPHOSTS ./test_program
```
The wrapper will execute two mpirun processes:
```bash
# mpi_1
# set sync server environment variables
# RDMASYNC_BIND_IP will bind to the node1's IP, and it should be auto-detected by the wrapper.
RDMASYNC_ENABLE=1 \
RDMASYNC_ROLE=coordinator \
RDMASYNC_BIND_IP=10.0.0.1 \ 
RDMASYNC_SERVER_IP=10.0.0.1 \
RDMASYNC_EXPECTED=1 \
LD_PRELOAD=./libmpi_trace.so \
mpirun -n 80 -ppn 40 --hosts node1,node2 ./test_program test_program_options
# mpi_2
# set sync participant environment variables
RDMASYNC_ENABLE=1 \
RDMASYNC_ROLE=participant \
RDMASYNC_SERVER_IP=10.0.0.1 \
LD_PRELOAD=./libmpi_trace.so \
LD_PRELOAD=libmpi_trace.so mpirun -n 160 -ppn 40 --hosts node3,node4,node5,node6 ./test_program test_program_options
```

And the wrapper will synchronize the processes mpi_1 and mpi_2, and record the time consumption and communication information of each MPI call. 

### Asymmetrical Mode

The asymmetrical mode allows the user to define the interference of the real world program. The TPWrapper's asymmetrical mode enables you to start a preset program in other nodes when the real world program start communication. This can be useful for detecting the performance impact of the real world program when it interacts with other network activities such as communication and io.

The preset programs in TPWrapper conclude three kinds:
- RDMA benchmark
- MPI benchmark
- IO benchmark

The [options] can include:
- `--trigger_enable`: enable trigger, the default value is fault.
- `--trigger_program`: Specify the preset program to run in other nodes when the real world program start communication. The preset program can be one of the following:
  - `rdma_${func}`: Run RDMA benchmark
    - The func includes 
  - `mpi_${func}`: Run MPI benchmark
    - The func includes allreduce, send/recv, 
  - `io_${func}`: Run IO benchmark
- `--trigger_program_size`: Specify the size of the preset program to run in other nodes when the real world program start communication.

The original command can be:
```bash
# mpirun cmd
mpirun -n 80 -ppn 40 --hosts node1,node2 ./test_program
```
You can change it to:

```bash
# example 1, run mpi trigger
python tpwrapper.py --hosts node1,node2,node3,node4 --trigger_enable --trigger_program mpi_allreduce --trigger_program_size 1024 --nmpi 2 mpirun -n 80 -ppn 40 --hosts TPHOSTS ./test_program

# example 2, run io trigger
python tpwrapper.py --hosts node1,node2,node3 --trigger_enable --trigger_program io_read --trigger_program_size 1024 --nmpi 2 mpirun -n 80 -ppn 40 --hosts TPHOSTS ./test_program

# example 3, run rdma trigger
python tpwrapper.py --hosts node1,node2,node3,node4 --trigger_enable --trigger_program rdma_write --trigger_program_size 1024 --nmpi 2 mpirun -n 80 -ppn 40 --hosts TPHOSTS ./test_program
```

For example1, the wrapper will execute two mpirun processes:
```bash
# mpi_1
# set sync server environment variables
mpirun -genv RDMASYNC_ENABLE 1 -genv RDMASYNC_ROLE coordinator -genv RDMASYNC_SERVER_IP 10.0.0.1 -genv RDMASYNC_EXPECTED 1 -genv LD_PRELOAD ./libmpi_trace.so -n 80 -ppn 40 --hosts node1,node2 ./test_program test_program_options
# mpi_2
# set sync participant environment variables
# when we launch 
mpirun -genv RDMASYNC_ENABLE 1 -genv RDMASYNC_ROLE participant -genv RDMASYNC_SERVER_IP 10.0.0.1 -genv RDMASYNC_EXPECTED 1 -n 80 -ppn 40 --hosts node3,node4 ${TPBENCH_ROOT_DIR}/wrapper/bin/mpi_bench --mpi_func allreduce --size 1024
```

For example2, the wrapper will execute one process and one normal process:
```bash
# mpi_1
# set sync server environment variables
mpirun -genv RDMASYNC_ENABLE 1 -genv RDMASYNC_ROLE coordinator -genv RDMASYNC_SERVER_IP 10.0.0.1 -genv RDMASYNC_EXPECTED 1 -genv LD_PRELOAD ./libmpi_trace.so -n 80 -ppn 40 --hosts node1,node2 ./test_program test_program_options
# mpi_2
# set sync participant environment variables
# when we launch 
ssh node3 ${TPBENCH_ROOT_DIR}/wrapper/bin/io_bench --mpi_func read --size 1024 -genv RDMASYNC_ENABLE 1 -genv RDMASYNC_ROLE participant -genv RDMASYNC_SERVER_IP 10.0.0.1 -genv RDMASYNC_EXPECTED 1
```

For example3, the wrapper will execute one process and one normal process:
```bash
# mpi_1
# set sync server environment variables
mpirun -genv RDMASYNC_ENABLE 1 -genv RDMASYNC_ROLE coordinator -genv RDMASYNC_SERVER_IP 10.0.0.1 -genv RDMASYNC_EXPECTED 1 -genv LD_PRELOAD ./libmpi_trace.so -n 80 -ppn 40 --hosts node1,node2 ./test_program test_program_options
# mpi_2
# set sync participant environment variables
# when we launch 
ssh node3 ${TPBENCH_ROOT_DIR}/wrapper/bin/rdma_bench --mpi_func read --size 1024 -genv RDMASYNC_ENABLE 1 -genv RDMASYNC_ROLE participant -genv RDMASYNC_SERVER_IP 10.0.0.1 -genv RDMASYNC_EXPECTED 1 --hosts node3,node4 --function write --size 1024
```

The mpi_2 will listen to the mpi_1's trigger message. When mpi_2 receive mpi_1's trigger message, it will act according to the message (details in `wrapper/TechReport.md`).  