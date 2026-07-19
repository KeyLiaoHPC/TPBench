# TPBench Agent Guide

Applies to all AI-assisted contributions. Breaching these rules can result in automatic banning.

Basic scope: single-core and pthread multithread CPU kernels (plus optional MPI / ROCm when enabled).

## Must Follow

1. **Style**: Read and follow [`docs/STYLE_GUIDE.md`](docs/STYLE_GUIDE.md) before any C change.
2. **Kernel edits**: Unless explicitly requested, only modify/test [`src/kernels/simple/tpbk_stream.c`](src/kernels/simple/tpbk_stream.c) when adapting kernels to front-end or corelib changes.
3. **CLI boundary**: `src/tpbcli/` may include only [`src/include/tpb-public.h`](src/include/tpb-public.h) / `tpbench.h` — not corelib or rafdb private headers.
4. **Docs**: User-facing workflow changes (DB queries, result retrieval, MPI recording, etc.) require updates to [`docs/USAGE.md`](docs/USAGE.md) and relevant `docs/design/` docs with runnable examples.
5. **Tests**: Add tests under `tests/` following existing patterns; ask the user for the test index when needed.

## Build And Verify

Run from workspace root unless noted.

```bash
cmake -B build
cmake --build build --config Release
cmake --build build --target tpb_cmake_help   # list TPB_* options + kernels (no compile)
```

```bash
cd build && ctest
./build/bin/tpbcli run --kernel stream --kargs stream_array_size=524288,ntest=100
# expect normal finish; Output table mean for Triad bandwidth > 1000 MB/s
```

Optional DB check:

```bash
./build/bin/tpbcli db list
tail -50 "$(ls -t ~/.tpbench/rafdb/log/tpbrunlog_*.log | head -1)"
```

Build outputs: `build/bin/tpbcli`, `build/bin/tpbcli-pli-launcher`, `build/lib/libtpbench.so`, `build/lib/libtpbk_*.so`, `build/etc/yaml/`.

## Code Map

| Area | Path | Role |
|------|------|------|
| CLI | `src/tpbcli/` | `run`, `db`; argp in `argp/`, YAML/scoring in `benchmark/`, launcher in `pli/` |
| Public API | `src/include/tpb-public.h` | Only header front-end should use |
| Corelib | `src/corelib/` | Driver, dynloader, stats, auto-record |
| Results DB | `src/corelib/rafdb/` | Run-and-forget persistent storage |
| Kernels | `src/kernels/simple/` | Single-process CPU (`tpbk_*.c`) |
| MPI kernels | `src/kernels/streaming_memory_access_mpi/` | MPI CPU kernels |
| GPU kernels | `src/kernels/rocm/` | ROCm / HIP |
| Kernel build | `src/kernels/CMakeLists.txt` | GLOB discovery + shared-lib rules |
| Registry | `cmake/TPBenchKernelRegistry.cmake` | CPU + ROCm kernel catalog |
| ROCm build | `cmake/TPBenchGpuKernelsRocm.cmake` | GPU target generation |
| CMake help | `cmake/TPBenchCmakeHelp.cmake` | `tpb_cmake_help` fallback strings |
| Tests | `tests/corelib/`, `tests/integration/`, `tests/tpbcli/` | Unit + integration + CLI |
| User docs | `docs/USAGE.md`, `docs/API_Reference.md`, `docs/design/` | Manuals and design |

Supporting (rarely touched): `src/timers/`, `src/libpfc/`, `src/pmu/`, `src/utils/`, `setup/yaml/`.

## Common Change Recipes

### New `TPB_*` CMake option

- Add `option()` or `set(... CACHE STRING ...)` in root [`CMakeLists.txt`](CMakeLists.txt) or [`src/kernels/CMakeLists.txt`](src/kernels/CMakeLists.txt).
- Append `"VAR|description"` to `_tpb_cmake_help_doc_lines` in [`cmake/TPBenchCmakeHelp.cmake`](cmake/TPBenchCmakeHelp.cmake) (fallback when cache HELPSTRING is overwritten).
- Verify: `cmake --build build --target tpb_cmake_help`.
- Kernel compile overrides (empty → `-O2`): `TPB_KERNEL_CFLAGS`, `TPB_KERNEL_CXXFLAGS`, `TPB_KERNEL_FFLAGS`.

### New CPU kernel

1. Add row to [`src/kernels/kernel_list.cmake.in`](src/kernels/kernel_list.cmake.in): `NAME|TAGS|PATH` (`PATH` relative to `src/kernels/`, contains `tpbk_<NAME>.c`).
2. If the kernel needs extra link libraries or a build condition (MPI), add a row to **`TPB_CPU_KERNEL_LINK_DEFS`** in [`cmake/TPBenchKernelRegistry.cmake`](cmake/TPBenchKernelRegistry.cmake): `NAME|EXTRA_LINK_LIBS|CONDITION`.
3. Add `tpbk_<kern>.c` under `src/kernels/<PATH>/`.
4. Implement `tpbk_pli_register_<kern>`, `tpbk_<kern>_entry`, runner, debug `main()` → `tpbk_<kern>_entry()`.
5. Build rules auto-generated in [`src/kernels/CMakeLists.txt`](src/kernels/CMakeLists.txt); no per-kernel `add_library` in root CMake.

### New MPI CPU kernel

Same as CPU kernel, plus:

- Registry: `CONDITION` = `MPI_C_FOUND`, `EXTRA_LINK_LIBS` = `MPI::MPI_C` (see `stream_mpi` row).
- Source in `streaming_memory_access_mpi/`.
- Root [`CMakeLists.txt`](CMakeLists.txt) runs `find_package(MPI)` when a selected kernel needs it.

### New ROCm kernel

1. Add row to `TPB_ROCM_KERNEL_DEFS`: `NAME|TAGS|PREREQ_TEXT|rocm|<hip path>|<entry source>`.
2. Entry source provides `tpbk_<kern>_entry()` + debug `main()`.
3. Build via [`cmake/TPBenchGpuKernelsRocm.cmake`](cmake/TPBenchGpuKernelsRocm.cmake).

### New CLI subcommand

- Add under `src/tpbcli/`; register in [`src/tpbcli/main/tpbcli.c`](src/tpbcli/main/tpbcli.c) (`tpbcli-argp` tree).
- `database`/`db` nested parsing: [`src/tpbcli/database/tpbcli-database.c`](src/tpbcli/database/tpbcli-database.c).
- Benchmark/YAML helpers: `src/tpbcli/benchmark/` if needed.

### Corelib feature

Extend [`src/corelib/`](src/corelib/) directly; keep CLI on public API only.

### Error handling (layered module + cause)

Return values encode **module** (high byte, `TPB_MOD_*`) and **cause** (low byte, `TPBE_*`):

```c
#define TPBE_MAKE(mod, cause)  /* combine module + cause */
#define TPBE_CAUSE(err)        /* extract cause for comparisons / exit status */
#define TPBE_MODULE(err)       /* extract reporting module (internal) */
```

| Macro / function | When to use |
|------------------|-------------|
| `TPB_FAIL(mod, cause, msg)` | Origin failure: local statement or non-TPBench external call. Logs file, func, msg, reason string, `[errcode=cause]`. |
| `TPB_PROPAGATE(mod, err, msg)` | Callee returned an error and this layer does not handle it. Logs file, func, msg, `[errcode=cause]`; swaps module, keeps cause. |
| `tpb_err_propagate(mod, err)` | Same re-encode as propagate, no log (cleanup / goto paths). |

Module codes: `TPB_MOD_DRIVER`, `TPB_MOD_ARGP`, `TPB_MOD_IMPL`, `TPB_MOD_IO`, `TPB_MOD_MISC`, `TPB_MOD_RAF_L1`, `TPB_MOD_RAF_L2_{KERNEL,TASK,TBATCH}`, `TPB_MOD_RAF_L3_{KERNEL,TASK,TBATCH}`, `TPB_MOD_RAF_MISC`, `TPB_MOD_CLI_{RUN,BENCHMARK,KERNEL,MISC}`.

Rules:
- Corelib **must not** call `exit()`; return encoded errors to the caller.
- `tpbcli` decides termination; `main()` maps returns with `tpb_err_to_exit_status()` (cause only). `TPBE_EXIT_ON_HELP` stays unencoded and exits 0.
- Compare named causes with `TPBE_CAUSE(err) == TPBE_*`. Raw kernel exit codes (0–255) are stored verbatim in the cause byte.

## Kernel Layout (PLI)

**Discovery**: [`src/kernels/CMakeLists.txt`](src/kernels/CMakeLists.txt) GLOBs `/*/tpbk_<kern>.c` under immediate subdirs of `src/kernels/`. Registry name `<kern>` must match filename.

**Single-file CPU kernel** (`tpbk_<kern>.c` linked to `libtpbench.so`):

| Artifact | Location | Role |
|----------|----------|------|
| `libtpbk_<kern>.so` | `build/lib/` | Loaded by corelib via `dlopen` / `dlsym("tpbk_pli_register_<kern>")` — see [`src/corelib/tpb-dynloader.c`](src/corelib/tpb-dynloader.c) |
| `tpbcli-pli-launcher` | `build/bin/` | Fork/exec child; child loads `.so`, calls `tpbk_<kern>_entry()` |

**Registration**: `tpbk_pli_register_<kern>()` declares params and static outputs; dynamic outputs may be added at run time in the runner.

**MPI note**: `stream_mpi` enabled when MPI found; `scale_mpi`, `axpy_mpi`, `rtriad_mpi`, `sum_mpi` share the same layout but registry rows are commented out.

**ROCm**: One `libtpbk_<kern>.so` per GPU kernel (HIP/C++ + entry source); same launcher as CPU.
