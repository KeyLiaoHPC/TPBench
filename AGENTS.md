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

### 1.1.2 Running tests

Step 1: Run the ctest suite. If any tests fail, check the issues and fix.

```bash
# Execute in the root folder of the workspace.
$ cd build
$ ctest
```

Step 2: Run a single-core kernel. If the kernel does not finish normally and print "triad_bw_walltime" that larger than 1000 MB/s, check the issues and fix.

```bash
# Execute in the root folder of the workspace.
$ ./build/bin/tpbcli run --kernel stream --kargs total_memsize=524288,ntest=100
```

### 1.1.3 Project Structure

The following file tree shows the source code organization. Items in `.gitignore` (build artifacts, binaries, data files) are excluded.

```
TPBench/
├── CMakeLists.txt              # Root CMake configuration
├── cmake/
│   ├── TPBenchConfig.cmake.in      # CMake package config template
│   ├── TPBenchKernel.cmake         # Kernel registration module (out-of-tree PLI)
│   ├── TPBenchKernelRegistry.cmake # CPU + ROCm kernel lists (build + tpb_cmake_help)
│   ├── TPBenchGpuKernelsRocm.cmake # ROCm targets from TPB_ROCM_KERNEL_DEFS
│   └── TPBenchCmakeHelp.cmake      # Generates tpb_cmake_help.txt
│
├── docs/
│   ├── API_Reference.md        # Public API documentation
│   ├── STYLE_GUIDE.md          # Code style guidelines
│   ├── USAGE*.md               # User manual (EN/CN)
│   ├── design_*.md             # Design documents
│   └── howto_*.md              # How-to guides
│
├── setup/
│   ├── Make.*                  # Platform-specific Makefiles
│   └── yaml/
│       ├── default.yml         # Default benchmark configuration
│       └── *_template.yml      # Benchmark templates
│
├── src/
│   ├── tpbcli.c                # CLI entry point
│   ├── tpbcli-database.c       # `database` subcommand — dispatch (list|ls|dump)
│   ├── tpbcli-database-ls.c    # `database list` / `ls`
│   ├── tpbcli-database-dump.c  # `database dump`
│   ├── tpbcli-database.h       # `tpbcli_database`, `tpbcli_database_ls`, `tpbcli_database_dump`
│   ├── tpbcli-*.c/h            # Other CLI subcommands (run/list/benchmark/help)
│   ├── tpb-bench-*.c/h         # Benchmark execution engine
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
│   │   ├── tpb-dynloader.c/h   # Dynamic library loader
│   │   ├── tpb-impl.c/h        # Internal implementation
│   │   ├── tpb-io.c/h          # I/O utilities
│   │   ├── tpb-stat.c/h        # Statistics collection
│   │   ├── tpb-argp.c/h        # Argument parsing
│   │   ├── tpb-unitcast.c/h    # Unit conversion
│   │   ├── strftime.c/h        # Time formatting
│   │   └── raw_db/             # Raw database backend
│   │       ├── tpb-rawdb-*.c/h # Database operations
│   │       └── tpb-sha1.c/h    # SHA-1 checksum
│   │
│   ├── kernels/
│   │   ├── CMakeLists.txt      # Kernel build configuration
│   │   ├── kernels.h           # Legacy placeholders (PLI uses dlopen/dlsym)
│   │   ├── simple/             # Simple CPU kernels
│   │   │   ├── tpbk_stream.c   # STREAM (single .c → libtpbk_stream.so + tpbk_stream.tpbx)
│   │   │   ├── tpbk_triad*.c   # TRIAD (sources present; enable via registry when ported)
│   │   │   ├── tpbk_rtriad*.c  # Reverse TRIAD
│   │   │   ├── tpbk_striad*.c  # Strided TRIAD
│   │   │   ├── tpbk_sum*.c     # SUM reduction
│   │   │   └── tpbk_staxpy*.c  # Strided AXPY
│   │   └── rocm/               # ROCm GPU kernels
│   │       └── tpbk_roofline*.hip/cpp # Roofline model kernel
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
    ├── corelib/
    │   ├── test_rawdb.c        # Raw database tests
    │   ├── test_strftime.c     # Time formatting tests
    │   ├── mock_*.c/h          # Mock implementations for testing
    │   └── tpb_run_pli.c       # PLI interface tests
    └── tpbcli/
        └── test-cli-run-dimargs.c  # CLI dimension argument tests
```

#### 1.1.3.1 Key Components

| Component | Path | Purpose |
|-----------|------|---------|
| CLI Frontend | `src/tpbcli*.c` | Command-line interface for running, recording kernels and benchmarks |
| Core Library | `src/corelib/` | Kernel loading, execution, and result collection |
| Benchmark Kernels | `src/kernels/simple/` | CPU benchmark implementations (STREAM, TRIAD, etc.) |
| GPU Kernels | `src/kernels/rocm/` | ROCm GPU benchmark implementations |
| Raw Database | `src/corelib/raw_db/` | Persistent storage for benchmark results |
| Timer Backend | `src/timers/` | High-resolution timing implementations |
| PMU Support | `src/libpfc/`, `src/pmu/` | Hardware performance counter access |

#### 1.1.3.2 Build Output Structure

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

### 1.1.4 CPU PLI kernel layout 

A kernel named `<kern>` requires at least one dedicated source code file `src/kernels/simple/tpbk_<kern>.c`:

- **`libtpbk_<kern>.so`**: `add_library` compiles that file **without** `TPB_K_BUILD_MAIN`. Corelib discovers it under `lib/` and calls `dlsym("tpbk_pli_register_<kern>")` (see [`src/corelib/tpb-dynloader.c`](src/corelib/tpb-dynloader.c)).
- **`tpbk_<kern>.tpbx`**: `add_executable` compiles the **same** file with compile definition `TPB_K_BUILD_MAIN`, which enables `main()` for fork/exec from the driver. The executable links `tpbench` only (not the kernel `.so`).

Both corelib and the kernel call `tpbk_pli_register_<kern>()` (params and static outputs) to register parameters and target metrics information; dynamic outputs may still be added at run time in the runner.

### 1.1.5 Development Guidelines

1. **New `TPB_*` build option**: Add an `option()` or `set(... CACHE STRING "help" )` in root [`CMakeLists.txt`](CMakeLists.txt) (or in [`src/kernels/CMakeLists.txt`](src/kernels/CMakeLists.txt) for kernel-selection caches). **Also** append one `"VAR|same short description"` entry to `_tpb_cmake_help_doc_lines` in [`cmake/TPBenchCmakeHelp.cmake`](cmake/TPBenchCmakeHelp.cmake). That list is the fallback when CMake replaces the cache HELPSTRING (e.g. after `-DTPB_*=...` on the command line); without it, `tpb_cmake_help` would show a generic placeholder. Reconfigure and run `cmake --build build --target tpb_cmake_help` to verify.
2. **New CPU kernel**: Add one row to `TPB_CPU_KERNEL_DEFS` in [`cmake/TPBenchKernelRegistry.cmake`](cmake/TPBenchKernelRegistry.cmake) (`NAME|DEFAULT_TAGS|EXTRA_LINK_LIBS|CONDITION`). Add `src/kernels/simple/tpbk_<kern>.c` implementing `tpbk_pli_register_<kern>`, the runner (e.g. `_tpbk_run_<kern>`), and `main` under `#ifdef TPB_K_BUILD_MAIN`. Build rules are generated in [`src/kernels/CMakeLists.txt`](src/kernels/CMakeLists.txt) (no per-kernel `add_library` in root CMake). [`src/kernels/kernels.h`](src/kernels/kernels.h) is legacy; PLI does not require new `register_*` declarations there.
3. **New ROCm kernel**: Add one row to `TPB_ROCM_KERNEL_DEFS` in the same registry (`NAME|TAGS|PREREQ_TEXT|rocm|<hip path>|<pli main path>` relative to source root). Do not hand-edit per-kernel `add_library` in root CMake; build rules are generated by [`cmake/TPBenchGpuKernelsRocm.cmake`](cmake/TPBenchGpuKernelsRocm.cmake).
4. **CLI Commands**: Add new subcommands in `src/tpbcli-<cmd>.c/h` and register in [`tpbcli.c`](src/tpbcli.c)
5. **Corelib Feature Extensions**: Extend [`src/corelib/`](src/corelib/) for new TPBench features
6. **Tests**: Add unit tests in `tests/` following existing patterns. Ask users for the index.
7. **Limited Kernel Modification**: When kernel codes have to be changed according to front-end or corelib modifications, unless explicit requests, only modify and test [`tpbk_stream.c`](src/kernels/simple/tpbk_stream.c), do not touch other kernels.
