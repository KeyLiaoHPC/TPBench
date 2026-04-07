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

TPBench supports enabling SIMD instruction sets (`TPB_USE_AVX512`, `TPB_USE_AVX2`, `TPB_USE_KP_SVE`) and optional OpenMP on kernel targets (`TPB_ENABLE_OPENMP`). Common options:

| CMake Option | Default | Description |
| -------- | -------- | -------- |
| `-DTPB_USE_AVX512=ON` | OFF | Enable AVX-512 instruction set (x86_64) |
| `-DTPB_USE_AVX2=ON` | OFF | Enable AVX2 instruction set (x86_64) |
| `-DTPB_USE_KP_SVE=ON` | OFF | Enable ARM SVE instruction set (aarch64) |
| `-DTPB_ENABLE_OPENMP=ON` | OFF | Add OpenMP to built kernel targets (does not select kernels) |
| `-DTPB_MPI_PATH=</path/to/mpi>` | empty | MPI root when an MPI-conditioned kernel is selected (empty = auto-detect) |

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

Top-level subcommands (short aliases in parentheses): **`run` (`r`)**, **`benchmark` (`b`)**, **`database` (`db`)**, **`kernel` (`k`)**, **`help` (`h`)**. Use **`--help`** or **`-h`** at the top level for full CLI help.

- **`tpbcli run`**: Run one or more TPBench kernels. Set runtime parameters, scan variable parameter dimensions. Pass runtime command-line arguments, environment variables, or MPI runtime parameters to each kernel through the TPBench framework.
- **`tpbcli benchmark`**: Run predefined benchmark suites. Each suite contains benchmark kernels with predefined parameters, scoring rules, and formulas. Outputs benchmark process and result scores.
- **`tpbcli database`** / **`tpbcli db`**: Inspect rafdb results in the workspace — **`list`** / **`ls`** (recent tbatch table) or **`dump`** with exactly one selector among **`--id`**, **`--tbatch-id`**, **`--kernel-id`**, **`--task-id`**, **`--score-id`**, **`--file`**, **`--entry`**. Run **`tpbcli database --help`** for a subcommand summary; **`tpbcli database dump --help`** for dump-only options. A bare **`tpbcli database`** (no `list`/`dump`) is an error.
- **`tpbcli kernel`**: Refresh kernel metadata in the workspace, then **`list`** / **`ls`** registered PLI kernels (not the same as the old top-level `list` command).
- **`tpbcli help`**: Display help documentation.

Output results on screen are also written to the log directory.

## 2.2 tpbcli run

### 2.2.1 Basic Format

Command-line format for `tpbcli run` shown below. By combining `kargs[_dim]`/`kenvs[_dim]`/`kmpiargs[_dim]` options, run multiple kernel evaluations. Create different parameter combinations for different kernels. Use one command to run multi-dimensional variable parameter tests for multiple kernels. In the command format below, all angle bracket `<>` options must be replaced with actual option names. Note that when using `--kargs-dim`, `--kenvs-dim`, and `--kmpiargs-dim`, the options need to be quoted.

```bash
tpbcli run <tpbench_options> \
[--kernel <kernel_name> \
[--kargs/--kargs-dim <opts> | --kenvs/--kenvs-dim <opts> | --kmpiargs/--kmpiargs-dim <opts>]]
```

**Rule:** Every `--kargs`, `--kargs-dim`, `--kenvs`, `--kenvs-dim`, `--kmpiargs`, and `--kmpiargs-dim` must appear **after** a `--kernel` that it applies to (the most recent `--kernel` on the command line). There is no separate “default” handle for settings before the first `--kernel`.

`<tpbench_options>` supported options include:
- `-P`: Select PLI-integrated kernels (default, kept for backward compatibility).
- `--timer`: Select timing method named `<timer_name>`, default is `clock_gettime`.
- `-d` / `--dry-run`: Parse arguments, expand dimensions, print `Exec:` lines for each handle, but do not fork the kernel or write auto-record batches.
- `--help` / `-h` (at `run` level): Print run subcommand help. Under `--kernel`, `-h` / `--help` prints kernel-specific help (see below).
- `--outargs`: Log and data output format settings
    - `unit_cast=[0/1]`: Whether to perform automatic unit conversion, default is 0, no conversion.
    - `sigbit_trim=<x>`: Limit the number of significant digits in output data. Integer parts exceeding the number of digits will be represented in scientific notation.

### 2.2.2 Run Basic Evaluation

**1) Select Evaluation Kernel and Kernel Parameters** (`--kernel, --kargs`)

`--kernel` defines a kernel to test, named `<kernel_name>`. All options starting with `--k*` after it apply to that kernel until the next `--kernel` or end of line. TPBench runs one handle per `--kernel` occurrence (dimension expansion can add more handles for the same kernel name). A single `--kargs` can set multiple parameters; separate with commas. If a value contains commas, use quotes.

Syntax: `--kernel <kernel_name> --kargs <key1>=<value1>,<key2>=<value2>,<key3>="<v3>,<with>,<complex>,<section>",...`

For one `--kernel` block, `--kargs`, `--kenvs`, and `--kmpiargs` can appear multiple times, but options with suffix `_dim` follow the limits in section 2.2.3. When the same parameter name is set more than once in the same block, the last occurrence wins.

Parameter options (`--kargs`, `--kenvs`, and `--kmpiargs`) accept a comma-separated list of `<key>=<value>` entries. TPBench checks names and types against that kernel’s registered parameters. If multiple settings apply to the same parameter for one handle, the priority is: values from `--kargs-dim` / `--kenvs-dim` / `--kmpiargs-dim` expansion (where applicable) override plain `--kargs` / `--kenvs` / `--kmpiargs`, and later options override earlier ones in the same block.

Note: Registered defaults may not match what the kernel finally uses after its own logic (e.g. alignment). For example, `total_memsize=128` KiB with double precision triad (`a_i=b_i+s*c_i`) may yield **131064** bytes per array, not 131072. After each run, the kernel prints the parameters it actually used.

Example 1: Run triad kernel, total memory capacity 128KiB
```bash
$ tpbcli run -k triad --kargs total_memsize=128
```

Example 2: Two triad runs: first 100 loops at 128KiB, second 100 loops at 256KiB
```bash
$ tpbcli run -k triad --kargs ntest=100,total_memsize=128 -k triad --kargs ntest=100,total_memsize=256
```

Example 3: Run triad then pchase: triad at 128KiB and 100 loops; pchase at 1000 loops (example parameter names depend on the pchase kernel).
```bash
$ tpbcli run -k triad --kargs total_memsize=128,ntest=100 -k pchase --kargs ntest=1000
```

Use `--kernel -l` to list currently available evaluation kernels. Use `--kernel <foo> --kargs -l` to list command-line input parameters supported by `<foo>` kernel.

### 2.2.3 Build and Run Heterogeneous Evaluation

TPBench enables HIP only when a `rocm`-tagged GPU kernel is selected (for example `-DTPB_KERNEL_TAGS=rocm` or `-DTPB_KERNELS=roofline_rocm`). Set `TPB_ROCM_PATH` if the toolchain is not found automatically.

**1) Build with ROCm Support**

```bash
# Configure when a ROCm kernel is selected (requires ROCm installation)
$ cmake -B build_rocm -DTPB_KERNEL_TAGS=rocm -DTPB_ROCM_PATH=/opt/rocm

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

Output includes FLOP/s measurements at arithmetic intensities: 0.1, 0.25, 0.5, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 15.0, 20.0, 25.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, and 90.0.

### 2.2.4 Run Variable Parameter Evaluation

When configuring variable parameter evaluation, TPBench runs tests sequentially according to predefined value sequences. Each value executes the kernel once. Get results as a parameter changes along a coordinate axis (e.g., a performance curve as a parameter changes). When specifying evaluation kernel `--kernel <foo>`, any parameter configurable via `--kargs` in the `foo` kernel can be configured via `--kargs-dim` as a variable parameter. If a variable parameter name duplicates a parameter name in `--kargs`, the `--kargs` value is ignored.

Currently, `tpbcli run` supports explicit list and recursive sequence for each `--kargs-dim` (and similarly `--kenvs-dim`). To sweep multiple parameters, use **multiple** `--kargs-dim` options after the same `--kernel`; TPBench builds the **Cartesian product** of all those dimensions (e.g. two lists of length 2 and 3 yield six runs). The `{…}` nesting syntax in a single `--kargs-dim` string is **not** supported.

**1) Explicit List**

For one parameter of the specified evaluation kernel, generate a set with explicitly specified elements. Run evaluation using each element in the set sequentially. Syntax shown below. For parameter `<parm_name>`, generate a set containing elements `a`, `b`, `c`, ...

Syntax: `--kargs-dim <parm_name>=[a, b, c, ...]`

Example: Run `triad` kernel. Set total memory capacity 256KiB. Each test round runs 100 loops. Configure data type `dtype` as variable parameter. Parameter is explicit list sequence. Contains `double`, `float`, `iso-fp16`. TPBench uses above 3 variable formats sequentially. Runs 3 rounds of triad tests.

```
$ tpbcli --kernel triad --kargs ntest=100,total_memsize=256 --kargs-dim dtype=[double,float,iso-fp16]
```

**2) Recursive Sequence**

For one parameter of the specified evaluation kernel, generate a recursive parameter sequence. Initial value `st`. Use calculation `<op>`. Calculate a new parameter value based on the previous test parameter value `@`. In the parameter configuration above, symbol `@` represents the recursive variable. `op` represents the operator. Currently supports add, sub, mul, div, and pow. `st` represents the value of the `parm_name` parameter for the first test. `min`, `max`, and `nlim` represent minimum value, maximum value, and recursive step limit of the recursive result. When the recursive result exceeds the `[min, max]` interval, or recursive steps exceed `nlim`, recursion terminates. `nlim` set to 0 means no recursive step limit.

Syntax: `--kargs-dim <parm_name>=<op>(@,x)(st,min,max,nlim)`

Example: Run `triad` kernel. Each round executes 100 loops. Set `total_memsize` to `16`, `32`, `64`, `128` sequentially. Runs 4 rounds of tests.

```
$ tpbcli --kernel triad --kargs ntest=100 --kargs-dim total_memsize=mul(@,2)(16,16,128,0)
```

**3) Multiple dimensions (Cartesian product)**

Use one `--kargs-dim` per parameter axis. Example: combine a three-value `dtype` list with a four-value `total_memsize` recursive sweep (12 runs total):

```
$ tpbcli run --kernel triad --kargs ntest=100 \
    --kargs-dim dtype=[double,float,iso-fp16] \
    --kargs-dim total_memsize=mul(@,2)(16,16,128,0)
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

Example: Run stream_mpi kernel with 100 test iterations, aggregate array size of 43690 elements per array across all ranks, using 2 MPI processes.

```bash
$ tpbcli run --kernel stream_mpi --kargs ntest=100,stream_array_size=43690 --kmpiargs '-np 2'
```

You can specify `--kmpiargs` multiple times after the same `--kernel`; each fragment is concatenated with a space.

**2) Variable MPI Arguments**

`--kmpiargs-dim` supports a single quoted list of MPI argument strings. Each list entry becomes one handle (after copying the rest of the handle from the template). To combine process counts and binding policies, put both fragments in **one** list entry (space-separated within the quoted string).

Syntax: `--kmpiargs-dim "['opt1', 'opt2', ...]"`

Example: Run `stream_mpi`, scanning MPI process counts 1, 2, and 4.

```bash
$ tpbcli run --kernel stream_mpi --kargs ntest=100,stream_array_size=43690 \
    --kmpiargs '--bind-to core' \
    --kmpiargs-dim "['-np 1', '-np 2', '-np 4']"
```

Example: Several full `mpirun` argument bundles in one list (each row is one handle):

```bash
$ tpbcli run --kernel stream_mpi --kargs ntest=100,stream_array_size=43690 \
    --kmpiargs-dim "['-np 2 --bind-to core', '-np 2 --bind-to socket', \
                     '-np 4 --bind-to core', '-np 4 --bind-to socket']"
```

**3) Command Line Visibility**

When TPBench executes a PLI kernel, the full command line is printed to the terminal for debugging and analysis. The command line format is:

```
TPBENCH_TIMER=<timer> [ENV=VAL ...] [mpirun <mpiargs>] <exec_path> <timer> <params...>
```

Note: MPI arguments are passed directly to `mpirun` and are not validated by TPBench. Errors will be reported if `mpirun` subprocesses fail.

## 2.3 tpbcli database

Synonyms: **`tpbcli database`**, **`tpbcli db`**.

You must supply a subcommand: **`list`** (alias **`ls`**) or **`dump`**.

- **`tpbcli db list`** — Print recent tbatch rows from the workspace index (same data as **`database list`**).
- **`tpbcli db dump`** — Requires exactly one of: **`--id`**, **`--tbatch-id`**, **`--kernel-id`**, **`--task-id`**, **`--score-id`**, **`--file`** *path*, **`--entry`** *name* (see **`dump --help`** for semantics). Selectors are mutually exclusive.

Examples:

```bash
$ tpbcli db list
$ tpbcli database dump --tbatch-id <40_hex_chars>
```
