# 1 Build tpbcli

**1) Basic Build Commands**

For systems with mature development or production environments, use CMake >= 3.16 to build TPBench for compatibility, performance, and CI features. Use the following commands to build and test TPBench.

```
$ git clone https://github.com/KeyLiaoHPC/TPBench 
$ cd TPBench
$ mkdir build && cd ./build
$ cmake ..
$ make
$ ls
bin  CMakeCache.txt  CMakeFiles  cmake_install.cmake  etc  lib  log  Makefile
```

**2) SIMD and Parallelization Build Options**

TPBench supports enabling SIMD instruction sets and OpenMP parallelization through CMake options. Available `-DTPB_USE_*` options are as follows:

| CMake Option | Default | Description |
| -------- | -------- | -------- |
| `-DTPB_USE_AVX512=ON` | OFF | Enable AVX-512 instruction set (x86_64) |
| `-DTPB_USE_AVX2=ON` | OFF | Enable AVX2 instruction set (x86_64) |
| `-DTPB_USE_KP_SVE=ON` | OFF | Enable ARM SVE instruction set (aarch64) |
| `-DTPB_USE_OPENMP=ON` | OFF | Enable OpenMP |
| `-DTPB_USE_MPI=</path/to/mpi/install/dir>` | OFF | Enable MPI |

Example: Enable AVX-512 instruction set compilation on x86_64 platform
```bash
$ cmake -DTPB_USE_AVX512=ON ..
$ make
```

Example: Enable SVE instruction set compilation on ARM platform
```bash
$ cmake -DTPB_USE_KP_SVE=ON ..
$ make
```

Note: SIMD options require compiler and target platform support for the corresponding instruction sets. When enabling AVX-512 or AVX2, it is recommended to also add the `-march=native` compilation option for optimal performance.

# 2 Use tpbcli

## 2.1 Introduction

tpbcli basic format:
```bash
tpbcli <subcommand> <options>
```

Supports 4 subcommands: `run`, `benchmark`, `list`, and `help`.
- `tpbcli run`: Run one or more TPBench kernels. Set runtime parameters, scan variable parameter dimensions. Pass runtime command-line arguments, environment variables, or MPI runtime parameters to each kernel through the TPBench framework.
- `tpbcli benchmark`: Run predefined benchmark suites. Each suite contains benchmark kernels with predefined parameters, scoring rules, and formulas. Outputs benchmark process and result scores.
- `tpbcli list`: List currently supported evaluation kernels.
- `tpbcli help`: Display help documentation.

Output results on screen are also written to the log directory.

## 2.2 tpbcli run

### 2.2.1 Basic Format

Command-line format for `tpbcli run` shown below. By combining `kargs[_dim]`/`kenvs[_dim]`/`kmpiargs[_dim]` options, run multiple kernel evaluations. Create different parameter combinations for different kernels. Use one command to run multi-dimensional variable parameter tests for multiple kernels. In the command format below, all angle bracket `<>` options must be replaced with actual option names. Note that when using `--kargs-dim`, `--kenvs-dim`, and `--kmpiargs-dim`, the options need to be quoted.

```bash
tpbcli run <tpbench_options> <default_args> \
[--kernel <kernel_name> \
[--kargs/--kargs-dim <opts> | --kenvs/--kenvs-dim <opts> | --kmpiargs/--kmpiargs-dim <opts>]]
```

`<tpbench_options>` supported options include:
- `-P/-F`: Select PLI-integrated kernels or FLI-integrated kernels, default is `-P`.
- `--timer`: Select timing method named `<timer_name>`, default is `clock_gettime`.
- `--outargs`: Log and data output format settings
    - `unit_cast=[0/1]`: Whether to perform automatic unit conversion, default is 0, no conversion.
    - `sigbit_trim=<x>`: Limit the number of significant digits in output data. Integer parts exceeding the number of digits will be represented in scientific notation.

### 2.2.2 Run Basic Evaluation

**1) Select Evaluation Kernel and Kernel Parameters** (`--kernel, --kargs`)

`--kernel` defines a kernel to test, named `<kernel_name>`. All options starting with `--k*` after this represent kernel options. Applied to this kernel until the next `--kernel` appears or the command line ends. If `--kargs`, `--kenvs`, and `--kmpiargs` appear before `--kernel`, the set parameters serve as defaults. Passed to all kernels to run. TPBench executes multiple rounds of tests. Completes all kernel tests defined by `--kernel`, or exits on error. A single `--kargs` can set multiple parameters. Separate parameters with commas. If a value contains commas, use quotes to prevent incorrect parsing.

Syntax: `--kernel <kernel_name> --kargs <key1>=<value1>,<key2>=<value2>,<key3>="<v3>,<with>,<complex>,<section>",...`

For one `--kernel` definition, `--kargs`, `--kenvs`, and `--kmpiargs` can appear multiple times, but options with suffix `_dim` can only appear once (see section 2.2.3). When the same parameter name appears multiple times after one `--kernel` definition, TPBench uses the last occurrence.

Parameter options (`--kargs`, `--kenvs`, and `--kmpiargs`) accept a comma-separated string list, with each element in the format `<key>=<value>`. Multiple such key-value pairs can follow a parameter option, indicating that the variable named "<key>" in the kernel should be set to "<value>". TPBench parses the option settings and checks the parameter legality. If multiple settings with the same parameter name appear in the command line for one test, the priority from high to low is: variable parameters > kernel parameters > default parameters, with higher-priority parameter settings overriding lower-priority ones. For a kernel named "foo", if `--kargs` definition after `--kernel foo` duplicates default parameter settings, `<foo>` will use the last occurrence in its scope. Therefore, the following three commands have the same effect.

```
$ tpbcli --kargs total_memsize=128,ntest=100 --kernel triad
$ tpbcli --kargs ntest=10 --kernel triad --kargs total_memsize=128,ntest=100
$ tpbcli --kernel triad \
         --kargs total_memsize=128 \
         --kargs ntest=100
```

Note: default parameters may not be adopted as-is. Two main reasons: 1) parameter processing is defined by the evaluation kernel; 2) an evaluation kernel may not support all default parameter names. For example, total memory capacity (`total_memsize=128`): using double precision, the triad calculation (`a_i=b_i+s*c_i`) allocates 5461 double variables per array (128/3*1024/sizeof(double), rounded down). Actual total memory capacity is **131064 Bytes**, not **131072 Bytes**. Therefore, after each test round, actual parameters used by the evaluation kernel are output to the terminal. Use these as the actual input parameters for the evaluation kernel.

Example 1: Run triad kernel, total memory capacity 128KiB
```bash
$ tpbcli run --kargs total_memsize=128 -k triad
```

Example 2: Test 2 rounds of the triad kernel. Round 1 runs 100 loops, total memory capacity 128KiB. Round 2 runs 100 loops, total memory capacity 256KiB
```bash
$ tpbcli run --kargs ntest=100 -k triad --kargs total_memsize=128 -k triad --kargs total_memsize=256
```

Example 3: Run triad and pchase kernels sequentially. Each kernel uses total memory capacity 128KiB. triad loops 100 times, pchase loops 1000 times.
```bash
$ tpbcli run --kargs total_memsize=128,ntest=100 -k triad -k pchase --kargs=1000
```

Use `--kernel -l` to list currently available evaluation kernels. Use `--kernel <foo> --kargs -l` to list command-line input parameters supported by `<foo>` kernel.

### 2.2.3 Build and Run Heterogeneous Evaluation

TPBench supports heterogeneous computing evaluation with ROCm/HIP for AMD GPUs. To enable ROCm support, configure and build with the `TPB_USE_ROCM` option.

**1) Build with ROCm Support**

```bash
# Configure with ROCm enabled (requires ROCm installation)
$ cmake -B build_rocm -DTPB_USE_ROCM=ON -DCMAKE_PREFIX_PATH=/opt/rocm

# Build all targets including tpbcli
$ cmake --build build_rocm -j$(nproc)
```

**2) Run GPU Roofline Evaluation**

The `roofline_rocm` kernel measures GPU roofline model performance at various arithmetic intensities.

Parameters:
- `type_code`: Data type (0=FP16, 1=BF16, 2=FP32, 3=FP64, default: 0)
- `op_code`: Operation type (0=vector, 1=tensor, default: 0). Tensor only for BF16/FP32.
- `total_memsize`: Memory size in KiB (default: 131072)
- `ntest`: Number of test iterations (default: 100)
- `twarm`: Warm-up time in milliseconds (default: 100)

Example 1: Run FP16 vector roofline with default parameters
```bash
$ cd build_rocm
$ ./bin/tpbcli run -P --kernel roofline_rocm
```

Example 2: Run FP32 vector roofline with 512 MiB memory
```bash
$ ./bin/tpbcli run -P --kernel roofline_rocm --kargs type_code=2,total_memsize=524288
```

Example 3: Run FP64 roofline measurement
```bash
$ ./bin/tpbcli run -P --kernel roofline_rocm --kargs type_code=3
```

Example 4: Select specific GPU device using environment variables
```bash
$ HIP_VISIBLE_DEVICES=1 ./bin/tpbcli run -P --kernel roofline_rocm
$ ROCR_VISIBLE_DEVICES=0 ./bin/tpbcli run -P --kernel roofline_rocm --kargs type_code=1
```

Output includes FLOP/s measurements at arithmetic intensities: 0.1, 0.25, 0.5, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 15.0, 20.0, 25.0, and 30.0.

### 2.2.4 Run Variable Parameter Evaluation

When configuring variable parameter evaluation, TPBench runs tests sequentially according to predefined value sequences. Each value executes the kernel once. Get results as a parameter changes along a coordinate axis (e.g., a performance curve as a parameter changes). When specifying evaluation kernel `--kernel <foo>`, any parameter configurable via `--kargs` in the `foo` kernel can be configured via `--kargs-dim` as a variable parameter. If a variable parameter name duplicates a parameter name in `--kargs`, the `--kargs` value is ignored.

Currently, `tpbcli run` supports configuring single parameter linear sequence, explicit list, and recursive sequence. To traverse multiple dimensions, nest multiple parameter sequences.

**1) Linear Sequence**

For one parameter of the specified evaluation kernel, generate a continuous sequence with a determined step size. Syntax shown below. For parameter `<parm_name>`, generate a list starting from `st`, step size `step`. Parameter values satisfy the closed interval `[st,en]`.

Syntax: `--kargs-dim <parm_name>=(st,en,step)`

Example: Run `triad` kernel. Use `double` data type. Each test round runs 100 loops. Configure total memory capacity `total_memsize` as variable parameter. Parameter is linear sequence. Interval 128KiB. 128KiB <= total_memsize <= 512KiB. TPBench runs 4 rounds of `triad` tests. Set `total_memsize` parameter to 128, 256, 384, 512 respectively.

```
$ tpbcli --kernel triad --kargs ntest=100,dtype=double --kargs-dim total_memsize=(128,512,128)
```

**2) Explicit List**

For one parameter of the specified evaluation kernel, generate a set with explicitly specified elements. Run evaluation using each element in the set sequentially. Syntax shown below. For parameter `<parm_name>`, generate a set containing elements `a`, `b`, `c`, ...

Syntax: `--kargs-dim <parm_name>=[a, b, c, ...]`

Example: Run `triad` kernel. Set total memory capacity 256KiB. Each test round runs 100 loops. Configure data type `dtype` as variable parameter. Parameter is explicit list sequence. Contains `double`, `float`, `iso-fp16`. TPBench uses above 3 variable formats sequentially. Runs 3 rounds of triad tests.

```
$ tpbcli --kernel triad --kargs ntest=100,total_memsize=256 --kargs-dim dtype=[double,float,iso-fp16]
```

**3) Recursive Sequence**

For one parameter of the specified evaluation kernel, generate a recursive parameter sequence. Initial value `st`. Use calculation `<op>`. Calculate a new parameter value based on the previous test parameter value `@`. In the parameter configuration above, symbol `@` represents the recursive variable. `op` represents the operator. Currently supports add, sub, mul, div, and pow. `st` represents the value of the `parm_name` parameter for the first test. `min`, `max`, and `nlim` represent minimum value, maximum value, and recursive step limit of the recursive result. When the recursive result exceeds the `[min, max]` interval, or recursive steps exceed `nlim`, recursion terminates. `nlim` set to 0 means no recursive step limit.

Syntax: `--kargs-dim <parm_name>=<op>(@,x)(st,min,max,nlim)`

Example: Run `triad` kernel. Each round executes 100 loops. Set `total_memsize` to `16`, `32`, `64`, `128` sequentially. Runs 4 rounds of tests.

```
$ tpbcli --kernel triad --kargs ntest=100 --kargs-dim total_memsize=mul(@,2)(16,16,128,0)
```

**4) Nested Sequence**

Nest multiple variable parameters. After defining each variable parameter, define another variable parameter in braces. In command-line parsing, innermost parameter list calculated first. Note, currently not allowed to define same parameter name at different nesting levels.

Syntax: `--kargs-dim <dim>{<nested_dim1>{<nested_dim2>{...}}}`

Example: Run `triad` kernel. Each round executes 100 loops. Use `double`, `float`, and `iso-fp16` data formats sequentially. For each format, set `total_memsize` to `16`, `32`, `64`, `128` sequentially. Runs 12 rounds of tests.

```
$ tpbcli --kernel triad --kargs ntest=100 --kargs-dim dtype=[double,float,iso-fp16]{total_memsize=mul(@,2)(16,16,128,0)}
```

### 2.2.5 Set Timing Method

TPBench defaults to using `clock_gettime` to get the `CLOCK_MONOTONIC_RAW` clock (Linux kernel version not lower than 2.6.28). You can also use `--timer <timer_name>` to set other timing methods. Use `tpbcli run --timer -l` to display currently supported timing methods. `<timer_name>` can choose the following timing methods:

| `<timer_name>` | Unit (abbr.) | Description |
| -------- | -------- | -------- |
| clock_gettime | nanosecond(ns) | POSIX clock_gettime with CLOCK_MONOTONIC_RAW |
| tsc_asym | cycle(cy) | A RDTSCP-based timing method provided by Gabriele Paoloni in Intel White Paper 324264-001 |
| cpu_clk_p | cycle(cy) | Count the CPU_CLK_UNHALTED.THREAD_P event. |
| cntvct | tick(tk) | The CNTVCT counter. Only available on the aarch64 platform. |
| pncctr | cycle(cy) | The CNTVCT counter. Only available on the aarch64 platform. |

One timing method defines clock source, function to read clock source, data format, storage format, storage method, and clock calculation method. Therefore, the same evaluation kernel and parameters using different timing methods may produce different output results and output units.

### 2.2.6 Set Environment Variables Passed to Evaluation Kernel

**1) Set Single or Multiple Environment Variables**

Example: Run triad. Loop 100 times. Memory size 128KiB. Use 16 OpenMP threads. Group every 4 threads. Bind to CPU cores 0-3, 4-7, 8-11, 12-15.

```
$ tpbcli --kernel triad --kargs ntest=100,total_memsize=128 --kenvs OMP_NUM_THREADS=16,OMP_PLACES="{0:4}:4:4"
```

### 2.2.7 Set MPI Runtime Parameters

**1) Set MPI Arguments**

The `--kmpiargs` option accepts a string that is passed as-is to `mpirun`. The string should be enclosed in single or double quotes.

Syntax: `--kmpiargs '<mpi_args_string>'`

Example: Run stream_mpi kernel with 100 test iterations, memory size 1024KiB per rank, using 2 MPI processes, and allow running as root.

```bash
$ tpbcli run --kernel stream_mpi --kargs ntest=100,total_memsize=1024 --kmpiargs '-np 2'
```

You can specify `--kmpiargs` multiple times; they will be concatenated with a space. If `--kmpiargs` is specified after `--kernel`, the kernel-specific MPI arguments replace the common MPI arguments.

**2) Variable MPI Arguments**

`--kmpiargs-dim` supports explicit list and nested list formats for scanning different MPI configurations.

Syntax: `--kmpiargs-dim "['opt1', 'opt2', ...]{['opta', 'optb', ...]}"`

Example 1: Run stream_mpi kernel, scanning MPI process counts from 1 to 4.

```bash
$ tpbcli run --kernel stream_mpi --kargs ntest=100,total_memsize=1024 \
    --kmpiargs '--bind-to core' \
    --kmpiargs-dim "['-np 1', '-np 2', '-np 4']"
```

Example 2: Use nested lists to scan process counts and binding policies.

```bash
$ tpbcli run --kernel stream_mpi --kargs ntest=100,total_memsize=1024 \
    --kmpiargs '--bind-to core' \
    --kmpiargs-dim "['-np 2', '-np 4']{'--bind-to core', '--bind-to socket'}"
```

The above command generates 4 combinations:
- `-np 2 --bind-to core`
- `-np 2 --bind-to socket`
- `-np 4 --bind-to core`
- `-np 4 --bind-to socket`

**3) Command Line Visibility**

When TPBench executes a PLI kernel, the full command line is printed to the terminal for debugging and analysis. The command line format is:

```
TPBENCH_TIMER=<timer> [ENV=VAL ...] [mpirun <mpiargs>] <exec_path> <timer> <params...>
```

Note: MPI arguments are passed directly to `mpirun` and are not validated by TPBench. Errors will be reported if `mpirun` subprocesses fail.
