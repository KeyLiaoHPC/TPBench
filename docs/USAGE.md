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
| `-DTPB_MPI_PATH=</path/to/mpi>` | empty | MPI root for selected MPI kernel targets only (`libtpbench` does not link MPI; empty = auto-detect) |

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
- **`tpbcli kernel`**: Manage kernel compile history in the workspace — **`list`** / **`ls`** (scan and register PLI kernels), **`init`** (scaffold out-of-tree kernel project), **`build`** (compile and install `libtpbk_<name>.so`), **`get`** (read-only query), **`set`** (write metadata), and internal **`backup-inactive`** (used by the build system).
- **`tpbcli help`**: Display help documentation.

Output results on screen are also written to the run log under `<workspace>/rafdb/log/` via `tpblog_*` (dual-write to stdout and the log file). See [design_tpblog_CN.md](./design/design_tpblog_CN.md).

## 2.2 tpbcli run

### 2.2.1 Basic Format

Command-line format for `tpbcli run` shown below. By combining `kargs[_dim]`/`kenvs[_dim]` with optional `--wrapper` chains, run multiple kernel evaluations. Create different parameter combinations for different kernels. Use one command to run multi-dimensional variable parameter tests for multiple kernels. In the command format below, all angle bracket `<>` options must be replaced with actual option names. Note that when using `--kargs-dim` and `--kenvs-dim`, the options need to be quoted.

```bash
tpbcli run <tpbench_options> \
[--wrapper <app> [--wrapper-args '<args>']]... \
[--kernel <kernel_name> [-og] \
[--kargs/--kargs-dim <opts> | --kenvs/--kenvs-dim <opts>] \
[--wrapper <app> [--wrapper-args '<args>']]...]...
```

**Rule:** Every `--kargs`, `--kargs-dim`, `--kenvs`, and `--kenvs-dim` must appear **after** a `--kernel` that it applies to (the most recent `--kernel` on the command line). There is no separate “default” handle for settings before the first `--kernel`.

**Wrapper rule:** `--wrapper` / `--wrapper-args` before the first `--kernel` form a **global** chain prepended to every kernel unless that kernel group uses `-og` / `--override-global`. Wrappers after a `--kernel` (and before the next `--kernel`) belong to that kernel only. Wrappers chain in order; later wrappers do not replace earlier ones.

**Kernel discovery:** `tpbcli run` loads only the kernel(s) named on `--kernel` / `-k`. It does **not** scan all of `lib/libtpbk_*.so`. If a kernel name is missing or its `.so` cannot be loaded, TPBench prints `Kernel <name> not found. Use \`tpbcli kernel list\` to show kernel lists.` Use **`tpbcli kernel list`** to discover installed kernels. When a named kernel is found, run logs `Kernel <name> found, KernelID: <id>` (or `New kernel found, add to kernel records.` on first registration).

`<tpbench_options>` supported options include:
- `-P`: Select PLI-integrated kernels (default, kept for backward compatibility).
- `--timer`: Select timing method named `<timer_name>`, default is `clock_gettime`.
- `-d` / `--dry-run`: Parse arguments, expand dimensions, print `Exec:` lines for each handle, but do not fork the kernel or write auto-record batches.
- `--help` / `-h` (at `run` level): Print run subcommand help. Under `--kernel`, `-h` / `--help` prints detailed kernel help (parameters, metrics, version/variation when recorded) in the same layout as **`tpbcli kernel get -v`**.
- `--outargs`: Log and data output format settings
    - `unit_cast=[0/1]`: Whether to perform automatic unit conversion, default is 0, no conversion.
    - `sigbit_trim=<x>`: Limit the number of significant digits in output data. Integer parts exceeding the number of digits will be represented in scientific notation.

### 2.2.2 Run Basic Evaluation

**1) Select Evaluation Kernel and Kernel Parameters** (`--kernel, --kargs`)

`--kernel` defines a kernel to test, named `<kernel_name>`. All options starting with `--k*` after it apply to that kernel until the next `--kernel` or end of line. TPBench runs one handle per `--kernel` occurrence (dimension expansion can add more handles for the same kernel name). A single `--kargs` can set multiple parameters; separate with commas. If a value contains commas, use quotes.

Syntax: `--kernel <kernel_name> --kargs <key1>=<value1>,<key2>=<value2>,<key3>="<v3>,<with>,<complex>,<section>",...`

For one `--kernel` block, `--kargs` and `--kenvs` can appear multiple times, but options with suffix `_dim` follow the limits in section 2.2.3. When the same parameter name is set more than once in the same block, the last occurrence wins.

Parameter options (`--kargs` and `--kenvs`) accept a comma-separated list of `<key>=<value>` entries. TPBench checks names and types against that kernel’s registered parameters. If multiple settings apply to the same parameter for one handle, the priority is: values from `--kargs-dim` / `--kenvs-dim` expansion (where applicable) override plain `--kargs` / `--kenvs`, and later options override earlier ones in the same block.

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

### 2.2.7 PLI Wrapper Chain

Use `--wrapper` and `--wrapper-args` to prepend an ordered chain of executables before the kernel entry command. MPI runs use `--wrapper mpirun --wrapper-args '...'` instead of a dedicated MPI CLI option.

**1) Global and per-kernel wrappers**

- Wrappers **before** the first `--kernel` apply to every kernel (global chain).
- Wrappers **after** a `--kernel` apply only to that kernel (local chain), appended after the global chain.
- `-og` / `--override-global` on a kernel group skips the global chain for that kernel; local wrappers are kept.

Syntax:

```bash
--wrapper <app> [--wrapper-args '<args_str>']
```

Example: global `numactl`, second kernel also uses `mpirun`:

```bash
$ tpbcli run --wrapper numactl --kernel stream --kernel stream_mpi \
    --wrapper mpirun --wrapper-args '-np 20'
```

Example: override global wrappers for the second kernel:

```bash
$ tpbcli run --wrapper numactl --kernel stream --kernel stream_mpi -og \
    --wrapper mpirun --wrapper-args '-np 20'
```

Example: MPI kernel with explicit `mpirun` path:

```bash
$ tpbcli run --kernel stream_mpi --kargs ntest=100,stream_array_size=43690 \
    --wrapper mpirun --wrapper-args '-np 2'
```

You can specify `--wrapper-args` multiple times after the same `--wrapper`; each fragment is concatenated with a space. `--wrapper-args` without a preceding `--wrapper` is rejected.

**2) Command Line Visibility**

When TPBench executes a PLI kernel, the full command line is printed to the terminal for debugging and analysis. The command line format is:

```
TPBENCH_TIMER=<timer> [ENV=VAL ...] [wrapper_app wrapper_args ...] <kernel_entry> <timer> <params...>
```

`<kernel_entry>` is the resolved command that starts the kernel (for shared-library PLI kernels this is typically `tpbcli-pli-launcher` plus the kernel `.so` path). Wrapper failures are reported by the wrapper executable; TPBench does not validate wrapper arguments.

## 2.3 tpbcli database

Synonyms: **`tpbcli database`**, **`tpbcli db`**.

You must supply a subcommand: **`list`** (alias **`ls`**) or **`dump`**.

- **`tpbcli db list`** — List rafdb index records (default: tbatch domain, latest 20). Options select domain and how many rows to show.
- **`tpbcli db dump`** — Requires exactly one of: **`--id`**, **`--tbatch-id`**, **`--kernel-id`**, **`--task-id`**, **`--score-id`**, **`--file`** *path*, **`--entry`** *name* (see **`dump --help`** for semantics). Selectors are mutually exclusive.

### `db list` options

```
tpbcli db list [-dT|-dt|-dk | --domain <tbatch|task|kernel>] [-n <N> | -N <N>]
```

Defaults: **`-dT`** (tbatch) and **`-n 20`** (latest 20 records).

| Option | Meaning |
|--------|---------|
| `-dT` | Tbatch domain (default) |
| `-dt` | Task domain (entry points only; MPI capsules, not per-rank rows) |
| `-dk` | Kernel domain |
| `--domain <name>` | Same as `-dT`/`-dt`/`-dk` (`tbatch`, `task`, or `kernel`) |
| `-n <N>` | Latest *N* records (newest first) |
| `-N <N>` | Oldest *N* records (oldest first) |

Domain selectors are mutually exclusive. **`-n`** and **`-N`** are mutually exclusive.

Record IDs are shown as a 6-digit hex prefix plus `*` (for example `b12a23*`), matching `tpbcli kernel ls`. Columns use proportional terminal-width layout via `tpblog_printf_c` (same as kernel list).

**Tbatch columns:** Start Time (UTC), Type, Duration(s), NTask, NKernel, NScore, TBatch ID

**Task columns:** Start Time (UTC), Kernel, Exit, Duration(s), Handle, Task ID, TBatch ID

**Kernel columns:** Kernel Name, Active, NParm, NMetric, Build Time (UTC), Kernel ID

Examples:

```bash
$ tpbcli db list
$ tpbcli db list -dt -n 10
$ tpbcli db list --domain kernel -N 5
$ tpbcli database dump --tbatch-id <40_hex_chars>
```

### 2.3.1 Quick Results Access

For most users, the quickest way to review recent benchmark results is:

```bash
# List recent runs with basic summary
tpbcli db list

# Get detailed output from the latest log file (includes printed metrics)
LOG_FILE=$(ls -t ~/.tpbench/rafdb/log/tpbrunlog_*.log | head -1)
tail -50 "$LOG_FILE"
```

The `list` command shows the TBatchID, number of tasks, and duration. The terminal output during the run (also saved in the log file) typically contains the actual performance metrics (e.g., bandwidth, latency) in human-readable form.

### 2.3.2 Working with MPI Results

For MPI kernels like `stream_mpi`:

- Each run creates a **task capsule** that groups all rank records.
- `tpbcli db list` shows the capsule as the entry point (counts as 1 task).
- To get the capsule ID from a TBatchID:

```bash
tpbcli db dump --tbatch-id <TBatchID> | grep -A1 "Record Data"
```

- The capsule's `.tpbr` file contains an array of all rank TaskRecordIDs. Individual rank records have `derive_to` pointing to the capsule.
- For aggregate metrics (recommended), use the capsule record:

```bash
tpbcli db dump --task-id <CapsuleID>
```

This outputs the capsule metadata and the list of member task IDs. The actual performance numbers are in the individual rank task records. Use the member IDs to dump specific ranks:

```bash
tpbcli db dump --task-id <Rank0TaskID>
```

### 2.3.3 Raw Record Exploration

The `--entry` option shows summary information without needing a specific ID:

```bash
# List all task batch entries
tpbcli db dump --entry task_batch

# List all kernel definitions
tpbcli db dump --entry kernel

# List all task entry points (capsules and standalone tasks)
tpbcli db dump --entry task
```

**Note:** `.tpbr` files contain binary record data. For automated analysis, consider parsing these with a script using the `rafdb` API or converting to JSON/CSV via custom tools.

## 2.4 tpbcli kernel

The **`kernel`** subcommand manages Kernel Domain records in rafdb: registered parameters, metrics, compile metadata, and **`active`** status for each KernelID variant.

### 2.4.1 List registered kernels

```bash
tpbcli kernel list
# alias: tpbcli kernel ls
```

Scans `lib/libtpbk_*.so`, registers any new KernelID, and prints a table with columns **Kernel**, **KernelID**, **Tags**, and **Description**. Compiled kernels appear first (in loader order); known source kernels from **`$TPB_HOME/src/kernels/kernel_list.cmake.in`** (and any scanned `tpbk_*.c` entry files) that are not yet installed show **`N/A`** in the KernelID column. Tags come from the registry file, not from rafdb records.

Long **Description** and **Tags** cells wrap with the same fixed-width hyphenation rules used by **`kernel get -v`**.

### 2.4.1a Kernel source registry

CPU kernel names, tags, and source locations are declared in **`src/kernels/kernel_list.cmake.in`** (installed under **`$TPB_HOME/src/kernels/`**):

```text
# Format: NAME|TAGS|PATH
stream|default,bandwidth|simple
stream_mpi|bandwidth,mpi|streaming_memory_access_mpi
```

**`PATH`** is relative to `src/kernels/` and must contain **`tpbk_<NAME>.c`**. Special link libraries and build conditions (for example MPI) remain in **`cmake/TPBenchKernelRegistry.cmake`** as **`TPB_CPU_KERNEL_LINK_DEFS`**.

When adding a kernel in a nested directory, add a row to **`kernel_list.cmake.in`**, place **`tpbk_<name>.c`** under that path, and (for MPI kernels) add a link-def override if needed. Reconfigure CMake or run **`tpbcli kernel build --kernel <name>`** to build from the staged registry tree.

### 2.4.2 Query kernel records (read-only)

```bash
tpbcli kernel get --kernel <name>
tpbcli kernel get --id <40-char-hex-kernel-id>
tpbcli kernel get -v --kernel <name>    # detailed active/latest record + versions
tpbcli kernel get -v --id <hex>         # detailed record by KernelID + versions
```

**`get` never scans `.so` files and never calls registration.** It reads only `rafdb/kernel/kernel.tpbe` and the matching `.tpbr` files. Without **`-v`**, it prints only parameter and metric names for the active record (or the newest if none are active). With **`-v`**, it prints the full kernel help layout and a **`Kernel Versions:`** table listing all known IDs and variation tags for that kernel name.

### 2.4.3 Set compile metadata

```bash
tpbcli kernel set --kernel <name> \
  --key <section>.<subkey> '<value>' \
  [--key <section>.<subkey> '<value>' ...]
```

Registers the named kernel (hashes the current `libtpbk_<name>.so`) if needed, then patches metadata headers. Supported sections: **`variation`**, **`compilation`**, **`dependency`**. Payload is `key=value` text (one pair per line).

Examples:

```bash
tpbcli kernel set --kernel stream \
  --key compilation.compiler.id 'GNU' \
  --key dependency.tpbench 'libtpbench.so'

# Values starting with '-' must follow --key (not parsed as CLI flags):
tpbcli kernel set --kernel stream --key compilation.kernel_cflags -O3
```

When the KernelID already exists, **`set` skips the update** and prints a warning unless **`TPB_K_OVERRIDE=1`** (or `true`/`yes`) is set in the environment. The same guard applies when re-registering an unchanged `.so` during **`kernel list`**. **`tpbcli run`** does not update kernel metadata; it only reports whether the named kernel was found.

### 2.4.4 Compile history from CMake

When **`TPB_RECORD_KERNEL_COMPILE_HISTORY=ON`** (default), each built kernel target runs a post-build step that calls **`tpbcli kernel set`** with **`variation`**, **`compilation`**, and **`dependency`** keys. Registration also writes **`utc_bits`** (build datetime) into the kernel `.tpbe` entry and `.tpbr` record; **`tpbcli database dump`** shows it as **`Build datetime (UTC)`**. Set **`TPB_WORKSPACE`** when building so records land in the intended workspace:

```bash
export TPB_WORKSPACE=$HOME/my-tpbench-ws
cmake -B build-o2 -DTPB_KERNELS=stream -DTPB_KERNEL_CFLAGS=-O2
TPB_WORKSPACE=$HOME/my-tpbench-ws cmake --build build-o2 --target tpbk_stream

cmake -B build-o3 -DTPB_KERNELS=stream -DTPB_KERNEL_CFLAGS=-O3
TPB_WORKSPACE=$HOME/my-tpbench-ws cmake --build build-o3 --target tpbk_stream

tpbcli kernel get -v --kernel stream
```

Rebuilding with the same flags produces the same KernelID; metadata is not overwritten unless **`TPB_K_OVERRIDE=1`**.

Before replacing `lib/libtpbk_<name>.so`, the build moves the previous file to **`lib/inactive/libkernel_<name>_<kernel_id>.so_bak`** and marks the old KernelID **`active=0`**. The dynloader scans only `lib/libtpbk_*.so` (non-recursive); backup files are not loaded.

### 2.4.5 Initialize out-of-tree kernel project

Templates are installed under **`${TPB_HOME}/etc/cmake/kernel/`** (functional CMake package files remain under **`${TPB_HOME}/lib/cmake/TPBench/`**).

```bash
export TPB_HOME=/path/to/tpbench    # or rely on $HOME/.tpbench default
tpbcli kernel init --dir ./mykern --kernel mykern
```

Creates `CMakeLists.txt` and `tpbk_mykern.c` with a minimal registerable kernel (parameter **`n`**, metric **`FOM,COUNT::Value`**).

### 2.4.6 Build kernel(s)

```bash
# Build one or more kernels from the registry (default --dir = TPB_HOME):
tpbcli kernel build --kernel stream
tpbcli kernel build --kernel 'stream,triad'
tpbcli kernel build --kernel-tag bandwidth

# Out-of-tree project (explicit source directory):
tpbcli kernel build --dir ./mykern --kernel mykern \
  [--tpb-home /path/to/tpbench] \
  [--ldflags "-Wl,--as-needed"] \
  [-DTPB_KERNEL_CFLAGS=-O3] [--cc gcc] [--cflags "-O3 -march=native"]

# MPI kernel from installed registry (use MPI wrapper compiler):
tpbcli kernel build --kernel stream_mpi --cc mpicc
```

Kernel source files should include only the installed flat header **`#include "tpbench.h"`** (under **`$TPB_HOME/include`**). Do not include **`tpb-public.h`** or corelib private headers in kernel code.

**Selector (required, mutually exclusive):**

- **`--kernel <names>`** — comma-separated kernel names; optional outer single or double quotes (`'stream,triad'`).
- **`--kernel-tag <tags>`** — comma-separated tags; expands to all registry kernels whose tag set intersects the request.

**Other options:**

- **`--dir <path>`** — defaults to resolved **`TPB_HOME`**. When defaulted, each kernel’s source directory is **`$TPB_HOME/src/kernels/<PATH>`** from **`kernel_list.cmake.in`**. When explicit, the same directory is used for every selected kernel (typical out-of-tree layout).
- **`--ldflags <flags>`** — passed to CMake as **`-DTPB_KERNEL_LDFLAGS=...`** and recorded as **`compilation.kernel_ldflags`**.
- **`--tpb-home`** — accepted only on **`kernel build`**. Priority: **`--tpb-home`**, then **`$TPB_HOME`**, then **`$HOME/.tpbench`**.

Multiple kernels build sequentially; each prints **PASS** or **FAIL**, followed by a summary line. The command exits nonzero if any selected kernel failed.

The command configures and builds with **`find_package(TPBench)`**, backs up any prior active `.so`, installs **`libtpbk_<name>.so`** into **`<tpb-home>/lib`**, reactivates the matching KernelID, and writes compile metadata via **`kernel set`**. Per-kernel build trees are **`build_<name>`** under the source directory (for example **`simple/build_stream`**).

Runtime kernel discovery uses **`$TPB_HOME`** (then **`$HOME/.tpbench`**) to locate **`bin/`**, **`lib/`**, and **`include/`** — set these in your environment before **`tpbcli run`**. When running from a build tree without installing, point **`TPB_HOME`** at the CMake build directory (for example `export TPB_HOME=$PWD/build`).

The **`tpbcli`** executable and its subcommands are implemented under **`src/tpbcli/`**; they consume only the public **`tpbench.h`** API from **`libtpbench.so`**. Third-party tools should link the library the same way rather than including corelib or rafdb private headers.

Example workflow:

```bash
export TPB_HOME=$HOME/.tpbench
export TPB_WORKSPACE=$HOME/my-tpbench-ws

tpbcli kernel init --dir ~/dev/mykern --kernel mykern
# edit ~/dev/mykern/tpbk_mykern.c if needed
tpbcli kernel build --dir ~/dev/mykern --kernel mykern --cflags "-O2"
tpbcli run --kernel mykern --kargs n=100
tpbcli kernel get -v --kernel mykern
```
