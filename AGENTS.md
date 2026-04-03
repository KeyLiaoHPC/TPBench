# TPBench Development Instructions

These instructions apply to all AI-assisted contributions to TPBench. Breaching these instructions can result in automatic banning.



## 1 AI Agent Guidelines

### 1.1 Basic version

The basic version supports single-core kernels and pthread-based multithread kernels.

#### 1.1.1 Build

``` bash
# Execute in the root folder of the workspace.
$ cmake -B build
$ cmake --build build --config Release
```

Optional: list `TPB_*` CMake options and registered kernels (no compile):

```bash
cmake --build build --target tpb_cmake_help
```

#### 1.1.2 Running tests

Step 1: Run the ctest suite. If any tests fail, check the issues and fix.

```bash
# Execute in the root folder of the workspace.
$ cd build
$ ctest
```

Step 2: Run a single-core kernel. If the kernel does not finish normally and print "triad_bw_walltime" that larger than 1000 MB/s, check the issues and fix.

```bash
# Execute in the root folder of the workspace.
$ ./build/bin/tpbcli run --kernel stream --kargs stream_array_size=524288,ntest=100
```

#### 1.1.3 Project Structure

The following file tree shows the source code organization. Items in `.gitignore` (build artifacts, binaries, data files) are excluded.

```
TPBench/
├── CMakeLists.txt              # Root CMake configuration
├── cmake/
│   ├── TPBenchConfig.cmake.in      # CMake package config template
│   ├── TPBenchKernel.cmake         # Kernel registration module (out-of-tree PLI)
│   ├── TPBenchKernelRegistry.cmake # CPU + ROCm kernel catalogs (build + tpb_cmake_help)
│   ├── TPBenchKernelSelect.cmake   # Tag/name-based kernel selection logic
│   ├── TPBenchGpuKernelsRocm.cmake # ROCm targets from TPB_ROCM_KERNEL_DEFS
│   ├── TPBenchInstallRpath.cmake   # RPATH helpers for install targets
│   └── TPBenchCmakeHelp.cmake      # Generates tpb_cmake_help.txt
│
├── docs/
│   ├── API_Reference.md        # Public API documentation
│   ├── STYLE_GUIDE.md          # Code style guidelines
│   ├── USAGE.md / USAGE_CN.md  # User manual (EN/CN)
│   ├── arts/                   # Diagrams and illustrations
│   ├── design/                 # Design documents (EN/CN)
│   └── howtos/                 # How-to guides (EN/CN)
│
├── setup/
│   ├── Make.*                  # Platform-specific Makefiles
│   └── yaml/
│       ├── default.yml         # Default benchmark configuration
│       └── *_template.yml      # Benchmark templates
│
├── src/
│   ├── tpbcli.c                # CLI entry point
│   ├── tpbcli-run.c/h          # `run` subcommand
│   ├── tpbcli-run-dim.c/h      # `run-dim` subcommand (dimensional sweep)
│   ├── tpbcli-benchmark.c/h    # `benchmark` subcommand
│   ├── tpbcli-kernel.c/h       # `kernel` subcommand (dispatch)
│   ├── tpbcli-kernel-list.c/h  # `kernel list` / `ls`
│   ├── tpbcli-database*.c/h    # `database` subcommand (list, dump)
│   ├── tpbcli-help.c/h         # `help` subcommand
│   ├── tpb-bench-yaml.c/h      # YAML benchmark configuration parser
│   ├── tpb-bench-score.c/h     # Benchmark scoring
│   ├── tpb-timer.c/h           # Timer abstraction
│   │
│   ├── include/
│   │   ├── tpb-public.h        # Public API header
│   │   ├── tpb-unitdefs.h      # Unit definitions
│   │   └── tpbench.h*          # Version header (generated)
│   │
│   ├── corelib/
│   │   ├── CMakeLists.txt      # Core library build config
│   │   ├── tpb-driver.c/h      # Kernel driver (load/execute kernels)
│   │   ├── tpb-dynloader.c/h   # Dynamic library loader (dlopen/dlsym)
│   │   ├── tpb-impl.c/h        # Internal implementation
│   │   ├── tpb-io.c/h          # I/O utilities
│   │   ├── tpb-stat.c/h        # Statistics collection
│   │   ├── tpb-argp.c/h        # Argument parsing
│   │   ├── tpb-unitcast.c/h    # Unit conversion
│   │   ├── tpb-autorecord.c/h  # Auto-recording (tbatch, task capsule)
│   │   ├── tpb-types.h         # Internal type definitions
│   │   ├── tpb_corelib_state.c/h # Global state management
│   │   ├── tpb_corelib_mpi.c   # MPI coordination (compiled when MPI found)
│   │   ├── tpb_corelib_mpi_stub.c # MPI stub (compiled when MPI absent)
│   │   ├── tpb-mpi_stub.c/h    # MPI type stubs for non-MPI builds
│   │   ├── strftime.c/h        # Time formatting
│   │   └── rafdb/              # Run-and-forget database backend
│   │       ├── tpb-raf-*.c/h   # entry, id, magic, merge, record, workspace
│   │       ├── tpb-raf-types.h # RAFDB type definitions
│   │       └── tpb-sha1.c/h    # SHA-1 checksum
│   │
│   ├── kernels/
│   │   ├── CMakeLists.txt      # Kernel build; GLOB discovers sources in subdirectories
│   │   ├── kernels.h           # Legacy placeholders (PLI uses dlopen/dlsym)
│   │   ├── simple/             # Single-process CPU kernels (tpbk_*.c)
│   │   ├── streaming_memory_access_mpi/ # MPI CPU kernels (see 1.1.4)
│   │   ├── stream/             # Reference STREAM sources (not built by default)
│   │   └── rocm/               # ROCm GPU kernels (tpbk_roofline*.hip/cpp)
│   │
│   ├── libpfc/                 # Performance counter library (3rd party)
│   │   ├── include/libpfc*.h   # PMU counter headers
│   │   ├── src/                # Library implementation
│   │   └── kmod/               # Kernel module for TSC access
│   │
│   ├── pmu/                    # PMU enable utilities
│   │   ├── armv8/enable_pmu.c  # ARMv8 PMU enabler
│   │   └── x86-64/pfckmod.c    # x86 performance counter module
│   │
│   ├── timers/                 # Timer implementations
│   │   ├── clock_gettime.c     # POSIX timer
│   │   ├── tsc_asym.c          # TSC-based timer
│   │   └── timers.h            # Timer interface
│   │
│   └── utils/                  # Utility programs
│       ├── pchase*.c           # Cache line ping-pong latency test
│       ├── get_time_error.c    # Timer error measurement
│       └── watch_cy_armv8.c    # Cycle counter monitor
│
└── tests/
    ├── CMakeLists.txt          # Test suite configuration
    ├── RunBuiltTest.cmake      # Test runner script
    ├── corelib/                # Unit tests (raf, strftime, capsule, 1d_array_write, pli, mocks)
    ├── integration/            # merge_fork, merge_hybrid, merge_pthread, tri_tests_record
    └── tpbcli/                 # CLI tests (e.g. dimargs); has CMakeLists.txt
```

##### 1.1.3.1 Key Components

| Component | Path | Purpose |
|-----------|------|---------|
| CLI Frontend | `src/tpbcli*.c` | Command-line interface; YAML/scoring for `benchmark` |
| Core Library | `src/corelib/` | Kernel loading, execution, result collection, auto-record |
| Benchmark Kernels | `src/kernels/simple/`, `src/kernels/streaming_memory_access_mpi/` | CPU kernels (single-process and MPI) |
| MPI Support | `src/corelib/tpb_corelib_mpi.c` (+ stub) | MPI-coordinated init, task write, capsule |
| GPU Kernels | `src/kernels/rocm/` | ROCm GPU benchmark implementations |
| rafdb (run-and-forget DB) | `src/corelib/rafdb/` | Persistent storage for benchmark results |
| Timer Backend | `src/timers/` | High-resolution timing implementations |
| PMU Support | `src/libpfc/`, `src/pmu/` | Hardware performance counter access |

##### 1.1.3.2 Build Output Structure

After building with `cmake --build build`, the output structure is:

```
build/
├── bin/
│   ├── tpbcli              # Main CLI executable
│   ├── tpbk_*.tpbx         # Kernel PLI executables (e.g., tpbk_stream.tpbx)
│   └── tests/              # Test executables
├── lib/
│   ├── libtpbench.so       # Core TPBench library
│   └── libtpbk_*.so        # Kernel shared libraries
└── etc/
    └── yaml/               # Installed configuration files
```

#### 1.1.4 CPU PLI kernel layout 

**Source discovery.** [`src/kernels/CMakeLists.txt`](src/kernels/CMakeLists.txt) finds `tpbk_<kern>.c` with `file(GLOB ... "/*/tpbk_${_kname}.c")` under **any** immediate subdirectory of `src/kernels/` (e.g. `simple/`, `streaming_memory_access_mpi/`). Place new CPU kernels accordingly; the registry name `<kern>` must match the basename `tpbk_<kern>.c`.

**Single-file kernels.** 

One file `tpbk_<kern>.c` instrumented by TPBench API and linked to `libtpbench.so`:

- **`libtpbk_<kern>.so`**: `add_library` compiles that file **without** `TPB_K_BUILD_MAIN`. Corelib discovers it under `lib/` and calls `dlsym("tpbk_pli_register_<kern>")` (see [`src/corelib/tpb-dynloader.c`](src/corelib/tpb-dynloader.c)).
- **`tpbk_<kern>.tpbx`**: `add_executable` compiles the **same** file with compile definition `TPB_K_BUILD_MAIN`, which enables `main()` for fork/exec from the driver. The executable links `tpbench` only (not the kernel `.so`).

**Registry vs source.** In [`cmake/TPBenchKernelRegistry.cmake`](cmake/TPBenchKernelRegistry.cmake), `stream_mpi` is enabled when MPI is found; `scale_mpi`, `axpy_mpi`, `rtriad_mpi`, and `sum_mpi` have sources under `streaming_memory_access_mpi/` but their registry rows are commented out until re-enabled.

Both corelib and the kernel call `tpbk_pli_register_<kern>()` (params and static outputs) to register parameters and target metrics information; dynamic outputs may still be added at run time in the runner.


### 1.2 Development Guidelines

1. **New `TPB_*` build option**: Add an `option()` or `set(... CACHE STRING "help" )` in root [`CMakeLists.txt`](CMakeLists.txt) (or in [`src/kernels/CMakeLists.txt`](src/kernels/CMakeLists.txt) for kernel-selection caches). **Also** append one `"VAR|same short description"` entry to `_tpb_cmake_help_doc_lines` in [`cmake/TPBenchCmakeHelp.cmake`](cmake/TPBenchCmakeHelp.cmake). That list is the fallback when CMake replaces the cache HELPSTRING (e.g. after `-DTPB_*=...` on the command line); without it, `tpb_cmake_help` would show a generic placeholder. Reconfigure and run `cmake --build build --target tpb_cmake_help` to verify. Existing kernel compile overrides: `TPB_KERNEL_CFLAGS`, `TPB_KERNEL_CXXFLAGS`, `TPB_KERNEL_FFLAGS` in [`src/kernels/CMakeLists.txt`](src/kernels/CMakeLists.txt) (empty default uses `-O2` for the relevant language).
2. **New CPU kernel**: Add one row to `TPB_CPU_KERNEL_DEFS` in [`cmake/TPBenchKernelRegistry.cmake`](cmake/TPBenchKernelRegistry.cmake) (`NAME|DEFAULT_TAGS|EXTRA_LINK_LIBS|CONDITION`). Add `tpbk_<kern>.c` under **any** `src/kernels/<subdir>/` that the GLOB can see (convention: `simple/` for single-process). Implement `tpbk_pli_register_<kern>`, the runner, and (for the single-file pattern) `main` under `#ifdef TPB_K_BUILD_MAIN`. Build rules are generated in [`src/kernels/CMakeLists.txt`](src/kernels/CMakeLists.txt) (no per-kernel `add_library` in root CMake). [`src/kernels/kernels.h`](src/kernels/kernels.h) is legacy; PLI does not require new `register_*` declarations there.
3. **New MPI CPU kernel**: Use `CONDITION` `MPI_C_FOUND` and `EXTRA_LINK_LIBS` `MPI::MPI_C` in the registry row (see existing `stream_mpi` row). Root [`CMakeLists.txt`](CMakeLists.txt) enables `find_package(MPI)` when a selected kernel needs it. Prefer `streaming_memory_access_mpi/` for MPI sources; follow the single-file or dual-file layout described in §1.1.4.
4. **New ROCm kernel**: Add one row to `TPB_ROCM_KERNEL_DEFS` in the same registry (`NAME|TAGS|PREREQ_TEXT|rocm|<hip path>|<pli main path>` relative to source root). Do not hand-edit per-kernel `add_library` in root CMake; build rules are generated by [`cmake/TPBenchGpuKernelsRocm.cmake`](cmake/TPBenchGpuKernelsRocm.cmake).
5. **CLI Commands**: Add new subcommands in `src/tpbcli-<cmd>.c/h` (and benchmark/YAML helpers in `src/tpb-bench-*.c/h` if needed) and register in [`tpbcli.c`](src/tpbcli.c).
6. **Corelib Feature Extensions**: Extend [`src/corelib/`](src/corelib/) for new TPBench features.
7. **Tests**: Add unit tests in `tests/` following existing patterns. Ask users for the index.
8. **Limited Kernel Modification**: When kernel codes have to be changed according to front-end or corelib modifications, unless explicit requests, only modify and test [`tpbk_stream.c`](src/kernels/simple/tpbk_stream.c), do not touch other kernels.
9. **All code contributions MUST follow the style rules defined in [`docs/STYLE_GUIDE.md`](docs/STYLE_GUIDE.md).** Before writing or modifying any C code, review, obey, and per-item check the whole style guide.