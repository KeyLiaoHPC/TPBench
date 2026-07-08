# Changelog

## Unreleased

### Frontend: tpbcli

- **Breaking:** `tpbcli db dump` now mirrors `db list` domain flags: requires `-dT`/`-dt`/`-dk`/`-dr` or `--domain`, plus either `-i`/`--id` (single `.tpbr`) or `-e` (`.tpbe` index with optional `-n`/`-N`). Removed `--id`, `--tbatch-id`, `--kernel-id`, `--task-id`, `--score-id`, `--file`, and `--entry` selectors. RTEnv is supported via `-dr`.
- **Breaking (API):** `tpbcli_database_dump_resolved(workspace, domain, entry_mode, id_value, count, from_oldest)` replaces the previous selector-based signature.
- **Tests:** Pack **B4** extended with B4.17–B4.24 for dump domain/mode validation and rtenv dump paths.

### Build

- **Breaking:** PLI kernels ship as a single shared library per kernel: `lib/libtpbk_<name>.so` (CPU, MPI, and ROCm). Per-kernel `.tpbx` executables and the old dual `.so` + `.tpbx` layout are removed.
- **New:** `bin/tpbcli-pli-launcher` — thin ET_EXEC child process used by `tpb_run_pli()`; `dlopen()`s the kernel `.so` and calls `tpbk_<name>_entry()` (fallback: `main()`).
- **Change:** `tpbench_add_kernel()` (out-of-tree) builds one `.so` only; `MAIN_SOURCE` is no longer required.
- **Change:** `tpbcli` no longer links kernel libraries at build time; kernels are discovered at runtime under `${TPB_HOME}/lib/`.
- **Change:** `tpb_build_kernel` / `tpb_install_kernel` depend on `tpbcli-pli-launcher` and kernel `.so` targets only.

### Documentation

- **Update:** [`docs/USAGE.md`](docs/USAGE.md) / [`docs/USAGE_CN.md`](docs/USAGE_CN.md) — current top-level commands and aliases (`database`/`db`, `kernel`/`k`, etc.), new §2.3 `database`; [`docs/howtos/1.1_howto_build_EN.md`](docs/howtos/1.1_howto_build_EN.md) — runtime kernel listing uses `tpbcli kernel list` (replacing removed `tpbcli list`); [`docs/design/tpbcli_argp_EN.md`](docs/design/tpbcli_argp_EN.md) / [`docs/design/tpbcli_argp_CN.md`](docs/design/tpbcli_argp_CN.md) — help flag wording `--help`/`-h` and `database` caller scope; [`AGENTS.md`](AGENTS.md) — nested `database`/`db` argp in `tpbcli-database.c`.
- **Update:** PLI kernel layout, build outputs, and KernelID semantics in [`AGENTS.md`](AGENTS.md), [`docs/API_Reference.md`](docs/API_Reference.md), [`docs/design/design_record_EN.md`](docs/design/design_record_EN.md) / [`docs/design/design_record_CN.md`](docs/design/design_record_CN.md), and [`docs/howtos/1.1_howto_build_EN.md`](docs/howtos/1.1_howto_build_EN.md) — single `.so` model, `tpbcli-pli-launcher`, and `tpbk_<kern>_entry()`.

### Frontend: tpbcli

- **Change:** `tpbcli run` never falls back to a full `lib/` scan. It loads only `--kernel` / `-k` names via `tpb_register_kernels()`. Missing kernels print `Kernel <name> not found. Use \`tpbcli kernel list\` to show kernel lists.` Help paths (`run -h`, `run --kernel -h`) register `_tpb_common` only (`tpb_register_kernels(0, NULL)`).
- **Change:** When `run` finds an already-recorded KernelID, it logs `Kernel <name> found, KernelID: <id>` at INFO instead of warning about `TPB_K_OVERRIDE`. Metadata updates remain in `tpbcli kernel set` / `kernel list`.
- **Change:** `tpbcli run` registers only the kernel(s) named on the command line via `tpb_register_kernels()` (full `lib/` scan remains for `tpbcli kernel list`).
- **Refactor:** `tpbcli database` uses the `tpbcli-argp` tree parser (same stack-based model as `run`). Top-level `database` has short alias **`db`**. After `database`/`db`, a subcommand is **required**: `list` (alias `ls`) or `dump`. Help flags use **`--help`** as the primary name and **`-h`** as the short name at each level. `tpbcli database --help` prints subcommand descriptions and a brief summary of dump selectors; `tpbcli database dump --help` lists all dump options. Conflicting dump selectors (`--id`, `--file`, etc.) are rejected by the parser.
- **Breaking (API):** `tpbcli_database_dump(int argc, char **argv, …)` is removed from the public header; use **`tpbcli_database_dump_resolved(workspace, selector_name, primary_value, entry_value)`** (`selector_name` is a long option string such as `"--id"`, or NULL for usage).
- **Tests:** Pack **B4** (`tests/tpbcli/test-cli-database.c`) covers missing subcommand, help, list/dump help, dump usage, conflicts, unknown args, and `ls` alias.
- **Refactor:** `tpbcli run` argument parsing uses the `tpbcli-argp` tree parser. Kernel-scoped options (`--kargs`, `--kargs-dim`, etc.) are only valid under `--kernel`; unknown options at the wrong depth report `error: unknown argument '…'`.
- **New:** `-d` / `--dry-run` — expand dimensions, print the same `Exec:` lines as a real run, skip `fork`/`exec` in the driver, and skip task/tbatch auto-record.
- **New:** `run --help` / `run -h` prints run-level help; `--kernel <name> --help` prints that kernel’s registered parameters and outputs; `--kernel -h` (no name) prints a legal-kernel hint, kernel list, and kernel sub-option help.
- **Removed:** Nested `{…}` syntax inside a single `--kargs-dim` / `--kenvs-dim` string. Use multiple `--kargs-dim` (or `--kenvs-dim`) options under one `--kernel` for a flat Cartesian product.
- **Change:** `--kmpiargs-dim` accepts a single quoted list `['…','…']` only (no nested `{…}` list).
- **Breaking:** `tpbcli run` no longer accepts `--kargs`, `--kargs-dim`, `--kenvs`, `--kenvs-dim`, `--kmpiargs`, or `--kmpiargs-dim` before a preceding `--kernel`. Each such option must follow the `--kernel` it applies to. The `_tpb_common` pseudo handle (`handle_list[0]`) and default-parameter inheritance from it are removed. Multiple `--kmpiargs` after the same `--kernel` concatenate with a space.
- **Tests:** Pack B2 — k-options before `--kernel` expect `unknown argument` in output; B2.5+ cover dry-run, help, and Cartesian dim cases. Pack B1 — nested dim string rejected (B1.4).
- **Breaking:** `database dump --entry task` prints only task `.tpbe` rows with `derive_to` all-zero (task entry points); line `(N task entry points / M total rows)` reports filtered vs total row count.
- **Breaking:** `tpbcli list` / `l` removed. Use `tpbcli kernel list`, `kernel ls`, `k list`, or `k ls`.
- `kernel` / `k` refreshes kernel records in the workspace, then runs the subcommand (`list` / `ls` today).
- List output: columns Kernel, KernelID, Description; KernelID is six hex chars + `*`, or `[ERROR]` / `-` when applicable; failures get a tagged line via `tpb_printf` on stdout.
- New sources: `tpbcli-kernel.c`, `tpbcli-kernel-list.c` (replaces `tpbcli-list.c`).

### Corelib

- **New:** `tpb_register_kernels(int n, const char *const *names)` — register `_tpb_common` and scan only the named PLI kernels (strict: any failure aborts registration). Used by `tpbcli run`.
- **New:** `tpb_dl_scan_kernel(const char *kernel_name)` — scan one `${TPB_HOME}/lib/libtpbk_<name>.so`.
- **New:** `tpb_dl_get_pli_launch_path()` — resolve `${TPB_HOME}/bin/tpbcli-pli-launcher`.
- **Change:** `tpb_dl_scan()` walks `${TPB_HOME}/lib/` for `libtpbk_*.so` (was `${TPB_HOME}/lib/` + `${TPB_HOME}/bin/` for `.so` / `.tpbx`). Per-kernel scan failures during a full scan are warnings; `tpbcli kernel list` continues with successfully registered kernels.
- **Change:** `tpb_run_pli()` builds `… tpbcli-pli-launcher <kernel.so> <timer> …` for `.so` modules (process isolation in the launcher child); non-`.so` exec paths (e.g. test mocks) still exec directly.
- **Breaking (RAFDB / KernelID):** `KernelID` is the SHA-1 digest of `libtpbk_<name>.so` (direct copy via `tpb_raf_gen_kernel_id(tpbx_sha1, id_out)`). Removed `src_sha1`, `so_sha1`, and `bin_sha1` from `kernel_attr_t` / `kernel_entry_t`; on-disk reserve padding adjusted (`TPB_RAF_KERNEL_ATTR_RESERVE`, `kernel_entry_t.reserve` +20 bytes). Existing kernel records keyed on the old composite ID are not compatible.
- **New:** `tpb_driver_set_dry_run(int)` — when enabled, `tpb_run_pli` prints `Exec:` then returns without forking (dry-run mode for `tpbcli run -d`).
- **Breaking:** TBatch `.tpbr` auto-record layout: two headers `TPBLINK::TaskID` (append 20-byte TaskRecordIDs) and `TPBLINK::KernelID` (empty data for now), replacing `KernelRecordIDs` / `TaskRecordIDs` / `ScoreRecordIDs`. Skeleton `.tpbr` is written at `tpb_record_begin_batch`; `tpb_record_end_batch` runs an internal scan of `task.tpbe` (rows with `utc_bits` not before batch start, matching `tbatch_id`, `derive_to` all-zero) and appends each via `tpb_raf_record_append_tbatch`, then patches `duration` / `ntask` / `nkernel` with `tpb_raf_record_patch_tbatch_counters`.
- **Breaking:** RAFDB lineage fields renamed: `dup_from` → `inherit_from`, `dup_to` → `derive_to` in `tbatch_attr_t` / `tbatch_entry_t` / `kernel_attr_t` / `kernel_entry_t` / `task_attr_t` / `task_entry_t` and in CLI dump key names. `tpb_k_task_set_derive_to(task_id, derive_to_id)` replaces `tpb_k_task_set_dup_to`. On-disk layout byte positions unchanged; existing workspaces are not read as compatible.
- Auto-record tbatch finalization sets `ntask` and fills `TPBLINK::TaskID` from task `.tpbe` rows that match the batch (including `utc_bits` not before batch start), with `derive_to` all-zero (one logical task per MPI invocation when a capsule is used). Unique-kernel counting (`nkernel`) uses the same entry-point filter so the capsule row is not skipped as a duplicate of per-rank rows.
- `tpb_mpik_corelib_init(void *mpi_comm, const char *)` is always declared in `tpb-public.h` (opaque communicator, e.g. `(void *)MPI_COMM_WORLD`). With MPI: coordinated init after `MPI_Init` as before; stores the communicator for `tpb_mpik_write_task`. Without MPI: `tpb_corelib_mpi_stub.c` returns `TPBE_ILLEGAL_CALL`. New caller types `TPB_CORELIB_CTX_CALLER_KERNEL_MPI_MAIN_RANK` / `TPB_CORELIB_CTX_CALLER_KERNEL_MPI_SUB_RANK`. `tpb_corelib_init` / `tpb_k_corelib_init` print `TPBench is called by tpbcli|kernel|MPI kernel` after version and workspace lines. When the real MPI object is used, `libtpbench` links `MPI::MPI_C` privately.
- `tpb_mpik_write_task(hdl, exit_code, task_id_out, tcap_id_out)` — MPI-coordinated `tpb_k_write_task` + capsule: rank 0 creates capsule and `MPI_Bcast`s ID; each rank patches `derive_to` via `tpb_k_task_set_derive_to`; `MPI_Gather` + rank-0-only `tpb_k_append_capsule_task` for ranks 1..n-1. Stub returns `TPBE_ILLEGAL_CALL` without MPI.
- `tpb_k_task_set_derive_to(task_id, derive_to_id)` — seek-and-patch `derive_to` in task `.tpbr` (validated magic + task ID) and matching `task.tpbe` row (`flock` on entry file).
- `tpb_register_kernel()` is part of the public API (`tpb-public.h` / `tpbench.h`).
- `tpb_list()` removed from `tpb-io`; table printing moved to the CLI.
- `tpb_k_static_info_t` adds `kernel_record_ok`; `tpb_driver_set_kernel_record_ok()` added.
- **At 20260402**: Refactor APIs according to the update `STYLE_GUIDE.md` 

#### RAFDB

- **Public API:** `tpb_raf_record_append_tbatch`, `tpb_raf_record_patch_tbatch_counters` — append TaskRecordID to tbatch `.tpbr` under lock; patch duration and counts after scan.

Design and implement task capsule record to enclose mp/mt task records instead of unstable/dangerous merging. Task capsule record (multi-rank / multi-process grouping)

- **Public API:** `tpb_k_write_task(hdl, exit_code, task_id_out)` — when `task_id_out` is non-NULL, copy the 20-byte TaskRecordID after a successful write. `tpb_raf_gen_taskcapsule_id`; `tpb_raf_record_create_task_capsule` / `tpb_raf_record_append_task_capsule`; `tpb_k_create_capsule_task`, `tpb_k_sync_capsule_task`, `tpb_k_append_capsule_task`, `tpb_k_unlink_capsule_sync_shm`.
- **rafdb:** Capsule `.tpbr` — `inherit_from` all `0xFF`, `ninput`/`noutput` 0, single header `TPBLINK::TaskID` (20-byte TaskIDs, growing 1-D); capsule ID uses SHA-1 prefix `taskcapsule` instead of `task`. Append uses advisory `fcntl` write lock; SPLIT offset derived from serialized first-header size (not stored `metasize` padding); task file `nheader` offset corrected to **172**; unbuffered stdio on append `FILE*` to avoid seek/read issues.
- **rafdb / `.tpbe`:** `entry_append_generic` uses `open(2)` + exclusive `flock(2)` + `fdopen` around read-modify-write so multiple MPI ranks can append entry rows safely; empty-file path writes start/end magic without the old `stat` + `strerror` printf.
- **Workspace:** `tpb_raf_resolve_workspace` may use `TPB_WORKSPACE` when the directory exists and corelib is not initialized (e.g. tests).
- **Autorecord:** `tpb_k_create_capsule_task` sets the workspace `.tpbe` row `inherit_from` to all `0xFF` (matches capsule record layout).
- **Autorecord / task `.tpbr`:** `tpb_record_write_task` writes output headers and payload only for outputs that were allocated (`out->n > 0` and `out->p != NULL`). Skipped slots remain registered via `tpb_k_add_output` for CLI/schema but are omitted from the on-disk task record; `attr.noutput` and `attr.nheader` reflect the serialized subset. **Tests:** `tests/corelib/test_1d_array_write.c` pack A5.7 (`skip_unalloc_output`).
- **Linking:** `tpbench` links `librt` (POSIX shm for capsule sync).
- **Tests:** `tests/corelib/test_capsule.c` (pack A6.1–A6.8), CMake target `test-capsule`.
- **Build:** `stream_mpi` kernel sources taken from `src/kernels/streaming_memory_access_mpi/` (registry name `stream_mpi` no longer assumes `simple/tpbk_stream_mpi.c`).
- **Docs:** `docs/design/design_record_EN.md` / `design_record_CN.md` — `inherit_from` / `derive_to`, task entry point semantics, §2.4.4 Task Capsule Record; §5 recommends capsule over merge for multi-rank grouping. `docs/API_Reference.md` struct excerpts updated.

### Kernel

- **Breaking:** All PLI kernels (CPU, MPI, ROCm) use one source tree → one `libtpbk_<name>.so`. Export `tpbk_pli_register_<name>`, `tpbk_<name>_entry()`, and a debug `main()` that forwards to the entry. Removed `TPB_K_BUILD_MAIN` and separate `*_main.c` / `.tpbx` executables.
- **CPU:** `stream`, `triad`, `scale`, `axpy`, `rtriad`, `sum`, `staxpy`, `striad` — entry + launcher model; registration unchanged at scan time.
- **MPI:** `stream_mpi` uses `tpbk_stream_mpi_entry()`; `scale_mpi`, `axpy_mpi`, `rtriad_mpi`, `sum_mpi` merged from dual-file layout into single `.c` sources (registry rows still commented until re-enabled).
- **ROCm:** `roofline_rocm` builds one `.so` from HIP + entry source; `tpbk_roofline_rocm_entry()` replaces the old standalone `.tpbx` main.
- During `tpb_dl_scan()` / `tpb_dl_scan_kernel()`, workspace kernel record sync sets `kernel_record_ok` per loaded PLI kernel.
- stream_mpi: Success path calls `tpb_mpik_write_task` (corelib MPI collectives + `derive_to` patch + rank-0 capsule appends); rank 0 `tpb_k_unlink_capsule_sync_shm` after barrier. Error path still uses `tpb_k_write_task` only.
- stream_mpi: Static PLI registration (`tpb_k_pli_register_stream_mpi`) registers all outputs on every rank (kernel `.so` scan / handle build): `INPARM::*`, per-iteration `EVENT,TIME::Copy/Scale/Add/Triad`, and sixteen aggregate `FOM,BANDWIDTH::…` / `FOM,TIME::…` summaries. Only MPI rank 0 calls `tpb_k_alloc_output` for the FOM slots; other ranks leave them unallocated so task `.tpbr` records omit FOM payload (corelib skips unallocated outputs).
- Other CPU PLI kernels: call `tpb_k_write_task(..., NULL)` for the optional TaskID argument (backward compatible).
- **Tests:** integration C1 direct invoke uses `tpbcli-pli-launcher` + `lib/libtpbk_stream.so`; `test_scale_axpy.sh` MPI sections updated for launcher + `.so`.

