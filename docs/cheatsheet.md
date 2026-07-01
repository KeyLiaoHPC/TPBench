# Configure, Build, and Test

Configure, build, and test TPBench.

## Mandatory


| Option | Purpose |
| ------ | ------- |
| `cmake -B <build-dir>` | Configure and generate TPBench targets. Requires CMake ≥ 3.16. |
| `cmake --build <build-dir>` | Build default targets (`tpbcli`, `libtpbench.so`, selected `libtpbk_*.so`, and tests when enabled). |


### `ctest` (when tests are enabled)


| Option | Purpose |
| ------ | ------- |
| `cd <build-dir> && ctest` | Run the CTest suite. Available when `BUILD_TESTING=ON` (default). |
| `cmake --build <build-dir> --target check` | Build test dependencies and run `ctest --output-on-failure` (`tests/CMakeLists.txt`). |


Each CTest case first builds its target via `tests/RunBuiltTest.cmake`, then runs the test binary.

## Optional

### Build commands


| Option | Purpose |
| ------ | ------- |
| `cmake --build <dir> --config Release` | Select build type on multi-config generators (passed to test builds as `CTEST_CONFIGURATION_TYPE`). |
| `cmake --build <dir> --target <name>` | Build a single target or component. Common: `tpbcli`, `tpbench`, `tpbk_<kern>`, `tpb_cmake_help`, `tpb_build_kernel`, `tpb_install_kernel`, `check`. |
| `cmake --install <dir> [--prefix PATH]` | Install core library and CLI; kernel `.so` via `--component tpbench_kernels`. `--prefix` overrides configured `CMAKE_INSTALL_PREFIX`. |
| `cmake --build <dir> --target tpb_cmake_help` | Print all `TPB_*` cache options and the kernel catalog (no compile). |


### Kernel selection (`-D`)


| Option | Default | Purpose |
| ------ | ------- | ------- |
| `-DTPB_KERNELS=<list>` | `default` | Comma-separated kernel names, or `all` / `default`. Selection logic in `cmake/TPBenchKernelSelect.cmake`. |
| `-DTPB_KERNEL_TAGS=<list>` | *(empty)* | Comma-separated tags; any matching tag selects the kernel (union with `TPB_KERNELS`). |
| `-DTPB_KERNEL_<NAME>_TAGS=<list>` | *(per registry)* | Override default tags for one kernel (`NAME` uppercased, e.g. `TPB_KERNEL_STREAM_TAGS`). |


**`TPB_KERNELS` candidates:** `default`, `all`, or any name from `cmake/TPBenchKernelRegistry.cmake` (e.g. `stream`, `triad`, `scale`, `axpy`, `rtriad`, `sum`, `staxpy`, `striad`, `stream_mpi`, `roofline_rocm`).

**Common tag candidates:** `default`, `bandwidth`, `stanza`, `mpi`, `gpu`, `roofline`, `rocm`.

### Compiler, flags, and parallelism (`-D`)


| Option | Default | Purpose |
| ------ | ------- | ------- |
| `-DCMAKE_C_COMPILER=<path>` | *(auto)* | Host C compiler for corelib, CLI, and CPU kernels (standard CMake). |
| `-DCMAKE_CXX_COMPILER=<path>` | *(auto)* | C++ compiler for ROCm kernel entry sources (standard CMake). |
| `-DCMAKE_BUILD_TYPE=Release\|Debug\|…` | *(generator-dependent)* | Global build type; written to kernel compile metadata when `TPB_RECORD_KERNEL_COMPILE_HISTORY=ON`. |
| `-DCMAKE_C_FLAGS="..."` | *(empty)* | Global C compile options; post-build writes `compilation.c_flags`. |
| `-DTPB_KERNEL_CFLAGS="..."` | *(empty → `-O2`)* | When non-empty, **replaces** CPU kernel C compile options (`src/kernels/CMakeLists.txt`). |
| `-DTPB_KERNEL_CXXFLAGS="..."` | *(empty → `-O2`)* | When non-empty, **replaces** ROCm/HIP kernel compile options (`cmake/TPBenchGpuKernelsRocm.cmake`). |
| `-DTPB_KERNEL_FFLAGS="..."` | *(empty → `-O2`)* | Reserved for future Fortran kernels. |
| `-DTPB_ENABLE_OPENMP=ON` | `OFF` | Add OpenMP compile/link to selected benchmark kernels. |
| `-DTPB_USE_AVX512=ON` | `OFF` | Define `TPB_USE_AVX512` project-wide. |
| `-DTPB_USE_AVX2=ON` | `OFF` | Define `TPB_USE_AVX2`. |
| `-DTPB_USE_KP_SVE=ON` | `OFF` | Define `TPB_USE_KP_SVE`. |
| `-DTPB_SHOW_DEBUG=ON` | `OFF` | Define `TPB_K_DEBUG=1` on benchmark kernel targets. |
| `-DTPB_RECORD_KERNEL_COMPILE_HISTORY=ON\|OFF` | `ON` | Post-build: run `tpbcli kernel set` to record `variation` / `compilation` / `dependency` metadata. |


### Paths and install layout (`-D`)


| Option | Default | Purpose |
| ------ | ------- | ------- |
| `-DTPB_WORKSPACE=<path>` | `$ENV{TPB_WORKSPACE}` | Workspace root; used as `CMAKE_INSTALL_PREFIX` when the prefix is still the CMake default. |
| `-DTPB_HOME=<path>` | `<build-dir>` | Baked into `libtpbench.so` for dynloader kernel/launcher discovery (`src/corelib/tpb-dynloader.c`). |
| `-DTPB_MPI_PATH=<path>` | *(empty)* | MPI install root for selected MPI kernel targets only (libtpbench does not link MPI); empty = auto-detect. |
| `-DTPB_ROCM_PATH=<path>` | *(empty)* | ROCm root when a `rocm`-tagged GPU kernel is selected; empty = auto-detect (`ROCM_PATH` env fallback). |
| `-DBUILD_TESTING=OFF` | `ON` | Skip test targets and `enable_testing()`. |


## Environment variables

Variables effective **during CMake configure/build** (unless noted, not runtime `tpbcli`-specific):


| Variable | Purpose |
| -------- | ------- |
| `TPB_WORKSPACE` | If set at configure time, seeds `-DTPB_WORKSPACE` cache and default install prefix. If set during `cmake --build`, forwarded to post-build `tpbcli kernel set` / `backup-inactive`. |
| `ROCM_PATH` | Fallback ROCm root when `TPB_ROCM_PATH` is empty and `/opt/rocm` is absent (root `CMakeLists.txt`). |
| `DESTDIR` | Staging prefix for `cmake --install` (root `install(CODE …)`). |


---

# tpbcli

TPBench command-line front-end for running and managing benchmark kernels and run records. Command form:

```
tpbcli [--workspace PATH] <run/r>|<kernel/k>|<databases/db>|<benchmark/b>|<help/h> [options]
```

Workspace resolution order: `--workspace` → `$TPB_WORKSPACE` → `$HOME/.tpbench` (`src/corelib/tpb_corelib_state.c`).

## tpbcli run / r

### Mandatory


| Option | Purpose |
| ------ | ------- |
| `--kernel <name>` / `-k <name>` | Kernel to run; marked `TPBCLI_ARGF_MANDATORY`. Repeatable for multiple handles. |


### Optional


| Option | Purpose |
| ------ | ------- |
| `--kargs <key=val,...>` | Runtime kernel parameters for the current handle (comma-separated `key=value`). |
| `--kargs-dim <spec>` | Dimension sweep on kernel args; multiple flags form a Cartesian product (max 16). |
| `--kenvs <KEY=VAL,...>` | Per-handle environment variables passed to the kernel process. |
| `--kenvs-dim <spec>` | Dimension sweep on environment variables. |
| `--kmpiargs '<args>'` | Append MPI launcher arguments (quoted string; MPI kernels). |
| `--kmpiargs-dim '[...]'` | Dimension sweep on MPI arg lists (`['arg1','arg2',…]`). |
| `--timer <name>` | Timer backend; preset default `clock_gettime`. Candidates: `clock_gettime`; `tsc_asym` (x86_64 only). |
| `--outargs <key=val,...>` | Output format: `unit_cast=<int>`, `sigbit_trim=<int>` (defaults 0 and 5). |
| `--dry-run` / `-d` | Validate arguments and print commands without executing kernels. |
| `--help` / `-h` | Subcommand or per-kernel help (`--kernel <name> --help` lists parameters/outputs). |


**Example**

```bash
./build/bin/tpbcli run --kernel stream --kargs stream_array_size=524288,ntest=100
./build/bin/tpbcli r -k stream_mpi --kargs ntest=100 --kmpiargs '-np 4'
```

### Environment variables


| Variable | Purpose |
| -------- | ------- |
| `TPB_WORKSPACE` | RAFDB workspace root when `--workspace` is omitted. |
| `TPB_TBATCH_ID` | Tbatch ID for PLI child recording (`src/corelib/tpb-autorecord.c`). |
| `TPB_HANDLE_INDEX` | Handle index for PLI child capsule linking. |
| `TPB_KERNEL_ID` | Kernel ID override for PLI child recording. |
| `TPB_LOG_FILE` | Absolute log path for PLI children (set by driver before fork/exec; cleared at each `tpbcli` start). |
| `TPBENCH_TIMER` | Timer name injected into PLI child environment by the driver. |
| `TPBENCH_UNIT_CAST` | Unit-cast flag passed to children (from `--outargs` or environment). |
| `TPBENCH_SIGBIT_TRIM` | Significant-bit trim for output (from `--outargs` or environment). |
| `OMP_NUM_THREADS` | *(external)* OpenMP thread count for OpenMP kernels; can also be set via `--kenvs`. |


## tpbcli kernel / k

Subcommands: `list`/`ls`, `get`, `set`, `backup-inactive`. With no subcommand options, scans `lib/libtpbk_*.so`, registers kernels, and prints name / KernelID prefix / description.


### `kernel get`

#### Mandatory


| Option | Purpose |
| ------ | ------- |
| `--kernel <name>` | Logical kernel name to query in RAFDB. |


#### Optional


| Option | Purpose |
| ------ | ------- |
| `-v` / `--verbose` | List **all** variants (newest first). Default: latest active record, else newest inactive. |


Read-only: does not scan `.so` or trigger registration (`src/tpbcli-kernel-get.c`).

### `kernel set`

#### Mandatory


| Option | Purpose |
| ------ | ------- |
| `--kernel <name>` | Kernel name; matches the kernel record marked active. |
| `--key <section.subkey> '<value>'` | Metadata key to write (repeatable, max 32 pairs). Dotted form, e.g. `compilation.kernel_cflags`, `variation.build_type`. |


Registers the kernel if needed, then updates RAFDB `variation` / `compilation` / `dependency` headers.

### `kernel backup-inactive`

#### Mandatory


| Option | Purpose |
| ------ | ------- |
| `--kernel <name>` | Move `lib/libtpbk_<name>.so` to `lib/inactive/libkernel_<name>_<kernel_id>.so_bak` and mark that KernelID inactive. |


### Environment variables


| Variable | Purpose |
| -------- | ------- |
| `TPB_WORKSPACE` | Target workspace for RAFDB reads/writes. |
| `TPB_K_OVERRIDE=1\|true\|yes` | Allow overwriting an existing KernelID record (`kernel set`, `kernel list`, dynamic load). |


## tpbcli database / db

RAFDB front-end. Subcommands: `list`/`ls`, `dump` (`src/tpbcli-database.c`).

### `database list` / `database ls`

#### Mandatory


| Option | Purpose |
| ------ | ------- |
| `list` / `ls` | Subcommand only; no further required flags. |


Shows up to the latest 20 tbatch records from `rafdb/task_batch/` (`src/tpbcli-database-ls.c`).

#### Optional


| Option | Purpose |
| ------ | ------- |
| `--help` / `-h` | Subcommand help. |


### `database dump`

#### Mandatory

Exactly **one** selector (mutually exclusive):


| Option | Purpose |
| ------ | ------- |
| `--id <hex>` | Global SHA-1 search (4–40 hex digits) across domains. |
| `--tbatch-id <hex>` | Dump tbatch record by ID. |
| `--kernel-id <hex>` | Dump kernel record by ID. |
| `--task-id <hex>` | Dump task record by ID. |
| `--score-id <hex>` | Accepted, but score domain is not implemented yet (prints note, exits 0). |
| `--file <path>` | Dump a `.tpbr`/`.tpbe` file (basename under workspace or full path). |
| `--entry <name>` | Dump domain index CSV. Candidates: `task_batch`, `kernel`, `task`. |


#### Optional


| Option | Purpose |
| ------ | ------- |
| `--help` / `-h` | Dump option help. |


### Environment variables


| Variable | Purpose |
| -------- | ------- |
| `TPB_WORKSPACE` | Workspace root for record lookup (same resolution as other subcommands). |


**Example**

```bash
./build/bin/tpbcli db list
./build/bin/tpbcli database dump --entry kernel
./build/bin/tpbcli database dump --tbatch-id abcdef0123456789abcd
```
