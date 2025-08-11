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
- `--trace`: Enable tracing (default is disabled)

For launching mpirun processes to correct nodes, you can use `TPHOSTS` to replace the host input with the host configurationin your program options. We list two examples below.

The original command can be:
```bash
# mpirun cmd
mpirun -n 80 -ppn 40 --hosts node1,node2 ./test_program
# non-mpi cmd
./test_program --hosts node1,node2
```

You can change it to:

```bash
# mpirun cmd
python tpwrapper.py --hosts node1,node2,node3,node4 --nmpi 2 mpirun -n 80 -ppn 40 --hosts TPHOSTS ./test_program
# non-mpi cmd
python tpwrapper.py --hosts node1,node2,node3,node4 --nmpi 2 ./test_program --hosts node1,node2
```

If you have more complex requirements, you can also use the `--configfile` option to specify a configuration file that contains all the necessary parameters. The format of the configuration file is as follows:

```ini
[tpwrapper]
sync = true
trace = true
nmpi = 2
hosts = node[1-8]
mpi_alloc = true

[program]
# MPI options for the program, except for the node allocation
# must has `-ppn` option and `-hosts  TPHOSTS` options.
mpi_optitons = -ppn 40 --bind-to core -hosts TPHOSTS 
mpi_program = ./test_program test_program_options

[hosts]
mpi_1: node1,node2
mpi_2: node3,node4,node5,node6
# This configuration means that the wrapper will run a 
```

The wrapper will execute two mpirun processes:
```bash
# mpi_1
# set sync server environment variables
# RDMASYNC_BIND_IP will bind to the node1's IP, and it should be auto-detected by the wrapper.
RDMASYNC_ENABLE=1 \
RDMASYNC_ROLE=AUTO \
RDMASYNC_BIND_IP=10.0.0.1 \ 
RDMASYNC_SERVER_IP=10.0.0.1 \
RDMASYNC_EXPECTED=240 \
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