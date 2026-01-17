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
CMakeCache.txt  CMakeFiles  cmake_install.cmake  libtpbench.so  libtriad.so  Makefile  tpbcli
```

# 2 Use tpbcli

## 2.1 Introduction

tpbcli basic format:
```bash
tpbcli <subcommand> <options>
```

Supports two subcommands: `run` and `benchmark`.
- `tpbcli run`: Run one or more TPBench kernels. Set runtime parameters, scan variable parameter dimensions. Pass runtime command-line arguments, environment variables, or MPI runtime parameters to each kernel through the TPBench framework.
- `tpbcli benchmark` (under development): Run predefined benchmark suites. Each suite contains benchmark kernels with predefined parameters, scoring rules, and formulas. Outputs benchmark process and result scores.

## 2.2 tpbcli run

### 2.2.1 Basic Format

Command-line format for `tpbcli run` shown below. By combining `kargs[_dim]`/`kenvs[_dim]`/`kmpiargs[_dim]` options, run multiple kernel evaluations. Create different parameter combinations for different kernels. Use one command to run multi-dimensional variable parameter tests for multiple kernels.

```bash
tpbcli run <tpbench_options> <default_args> \
[--kernel <kernel_name> \
[--kargs/--kargs-dim <opts> | --kenvs/--kenvs-dim <opts> | --kmpiargs <opts> | --kmpiargs-dims <opts>]]
```

In the command format above, all angle bracket `<>` options must be replaced with actual option names.

### 2.2.2 Run Basic Evaluation

**1) Select Evaluation Kernel and Kernel Parameters** (`--kernel, --kargs`)

`--kernel` defines a kernel to test, named `<kernel_name>`. All options starting with `--k*` after this represent kernel options. Applied to this kernel until the next `--kernel` appears or the command line ends. If `--kargs`, `--kenvs`, and `--kmpiargs` appear before `--kernel`, the set parameters serve as defaults. Passed to all kernels to run. TPBench executes multiple rounds of tests. Completes all kernel tests defined by `--kernel`, or exits on error. A single `--kargs` can set multiple parameters. Separate parameters with commas. If a value contains commas, use quotes to prevent incorrect parsing.

Syntax: `--kernel <kernel_name> --kargs <key1>=<value1>,<key2>=<value2>,<key3>="<v3>,<with>,<complex>,<section>",...`

For one `--kernel` definition, `--kargs`, `--kenvs`, and `--kmpiargs` can appear multiple times. Options with suffix `_dim` can only appear once. When same parameter name appears multiple times after one `--kernel` definition, TPBench uses the last occurrence. Similarly, if `--kargs` definition after `--kernel <foo>` duplicates default parameter settings, `<foo>` uses the last occurrence in its scope. For example, the following three commands have the same effect.

```
$ tpbcli --kargs memsize=128,ntest=100 --kernel triad
$ tpbcli --kargs ntest=10 --kernel triad --kargs memsize=128,ntest=100
$ tpbcli --kernel triad \
         --kargs memsize=128 \
         --kargs ntest=100
```

Note: default parameters may not be adopted as-is. Two main reasons: 1) parameter processing is defined by the evaluation kernel; 2) an evaluation kernel may not support all default parameter names. For example, total memory capacity (`memsize=128`): using double precision, the triad calculation (`a_i=b_i+s*c_i`) allocates 5461 double variables per array (128/3*1024/sizeof(double), rounded down). Actual total memory capacity is **131064 Bytes**, not **131072 Bytes**. Therefore, after each test round, actual parameters used by the evaluation kernel are output to the terminal. Use these as the actual input parameters for the evaluation kernel.

Example 1: Run triad kernel, total memory capacity 128KiB
```bash
$ tpbcli run --kargs memsize=128 -k triad
```

Example 2: Test 2 rounds of the triad kernel. Round 1 runs 100 loops, total memory capacity 128KiB. Round 2 runs 100 loops, total memory capacity 256KiB
```bash
$ tpbcli run --kargs ntest=100 -k triad --kargs memsize=128 -k triad --kargs memsize=256
```

Example 3: Run triad and pchase kernels sequentially. Each kernel uses total memory capacity 128KiB. triad loops 100 times, pchase loops 1000 times.
```bash
$ tpbcli run --kargs memsize=128,ntest=100 -k triad -k pchase --kargs=1000
```

Use `--kernel -l` to list currently available evaluation kernels. Use `--kernel <foo> --kargs -l` to list command-line input parameters supported by `<foo>` kernel.

### 2.2.3 Run Variable Parameter Evaluation

When configuring variable parameter evaluation, TPBench runs tests sequentially according to predefined value sequences. Each value executes the kernel once. Get results as a parameter changes along a coordinate axis (e.g., a performance curve as a parameter changes). When specifying evaluation kernel `--kernel <foo>`, any parameter configurable via `--kargs` in the `foo` kernel can be configured via `--kargs-dim` as a variable parameter. If a variable parameter name duplicates a parameter name in `--kargs`, the `--kargs` value is ignored.

Currently, `tpbcli run` supports configuring single parameter linear sequence, explicit list, and recursive sequence. To traverse multiple dimensions, nest multiple parameter sequences.

**1) Linear Sequence**

For one parameter of the specified evaluation kernel, generate a continuous sequence with a determined step size. Syntax shown below. For parameter `<parm_name>`, generate a list starting from `st`, step size `step`. Parameter values satisfy the closed interval `[st,en]`.

Syntax: `--kargs-dim <parm_name>=(st,en,step)`

Example: Run `triad` kernel. Use `double` data type. Each test round runs 100 loops. Configure total memory capacity `memsize` as variable parameter. Parameter is linear sequence. Interval 128KiB. 128KiB <= memsize <= 512KiB. TPBench runs 4 rounds of `triad` tests. Set `memsize` parameter to 128, 256, 384, 512 respectively.

```
$ tpbcli --kernel triad --kargs ntest=100,dtype=double --kargs-dim memsize=(128,512,128)
```

**2) Explicit List**

For one parameter of the specified evaluation kernel, generate a set with explicitly specified elements. Run evaluation using each element in the set sequentially. Syntax shown below. For parameter `<parm_name>`, generate a set containing elements `a`, `b`, `c`, ...

Syntax: `--kargs-dim <parm_name>=[a, b, c, ...]`

Example: Run `triad` kernel. Set total memory capacity 256KiB. Each test round runs 100 loops. Configure data type `dtype` as variable parameter. Parameter is explicit list sequence. Contains `double`, `float`, `iso-fp16`. TPBench uses above 3 variable formats sequentially. Runs 3 rounds of triad tests.

```
$ tpbcli --kernel triad --kargs ntest=100,memsize=256 --kargs-dim dtype=[double,float,iso-fp16]
```

**3) Recursive Sequence**

For one parameter of the specified evaluation kernel, generate a recursive parameter sequence. Initial value `st`. Use calculation `<op>`. Calculate a new parameter value based on the previous test parameter value `@`. In the parameter configuration above, symbol `@` represents the recursive variable. `op` represents the operator. Currently supports add, sub, mul, div, and pow. `st` represents the value of the `parm_name` parameter for the first test. `min`, `max`, and `nlim` represent minimum value, maximum value, and recursive step limit of the recursive result. When the recursive result exceeds the `[min, max]` interval, or recursive steps exceed `nlim`, recursion terminates. `nlim` set to 0 means no recursive step limit.

Syntax: `--kargs-dim <parm_name>=<op>(@,x)(st,min,max,nlim)`

Example: Run `triad` kernel. Each round executes 100 loops. Set `memsize` to `16`, `32`, `64`, `128` sequentially. Runs 4 rounds of tests.

```
$ tpbcli --kernel triad --kargs ntest=100 --kargs-dim memsize=mul(@,2)(16,16,128,0)
```

**4) Nested Sequence**

Nest multiple variable parameters. After defining each variable parameter, define another variable parameter in braces. In command-line parsing, innermost parameter list calculated first. Note, currently not allowed to define same parameter name at different nesting levels.

Syntax: `--kargs-dim <dim>{<nested_dim1>{<nested_dim2>{...}}}`

Example: Run `triad` kernel. Each round executes 100 loops. Use `double`, `float`, and `iso-fp16` data formats sequentially. For each format, set `memsize` to `16`, `32`, `64`, `128` sequentially. Runs 12 rounds of tests.

```
$ tpbcli --kernel triad --kargs ntest=100 --kargs-dim dtype=[double,float,iso-fp16]{memsize=mul(@,2)(16,16,128,0)}
```

### 2.2.4 Set Timing Method

TPBench defaults to using `clock_gettime` to get the `CLOCK_MONOTONIC_RAW` clock (Linux kernel version not lower than 2.6.28). You can also use `--timer <timer_name>` to set other timing methods. Use `tpbcli run --timer -l` to display currently supported timing methods. `<timer_name>` can choose the following timing methods:

| `<timer_name>` | Unit (abbr.) | Description |
| -------- | -------- | -------- |
| clock_gettime | nanosecond(ns) | POSIX clock_gettime with CLOCK_MONOTONIC_RAW |
| tsc_asym | cycle(cy) | A RDTSCP-based timing method provided by Gabriele Paoloni in Intel White Paper 324264-001 |
| cpu_clk_p | cycle(cy) | Count the CPU_CLK_UNHALTED.THREAD_P event. |
| cntvct | tick(tk) | The CNTVCT counter. Only available on the aarch64 platform. |
| pncctr | cycle(cy) | The CNTVCT counter. Only available on the aarch64 platform. |

One timing method defines clock source, function to read clock source, data format, storage format, storage method, and clock calculation method. Therefore, the same evaluation kernel and parameters using different timing methods may produce different output results and output units.

### 2.2.5 Set Environment Variables Passed to Evaluation Kernel

**1) Set Single or Multiple Environment Variables**

Example: Run triad. Loop 100 times. Memory size 128KiB. Use 16 OpenMP threads. Group every 4 threads. Bind to CPU cores 0-3, 4-7, 8-11, 12-15.

```
$ tpbcli --kernel triad --kargs ntest=100,memsize=128 --kenvs OMP_NUM_THREADS=16,OMP_PLACES="{0:4}:4:4"
```

### 2.2.6 Set MPI Runtime Parameters
